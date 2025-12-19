#include "core/Phase6Reasoner.h"
#include "core/Phase7AffectiveState.h"
#include "core/Phase7Reflection.h"
#include "core/Phase9Metacognition.h"
#include "core/Phase8GoalSystem.h"
#include "core/SelfModel.h"
#include "core/AutonomyEnvelope.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <cmath>

namespace NeuroForge {
namespace Core {

static std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::vector<std::int64_t> Phase6Reasoner::registerOptions(const std::vector<ReasonOption>& options,
                                                          std::uint64_t step,
                                                          std::int64_t ts_ms,
                                                          std::optional<std::size_t> selected_index) {
    std::vector<std::int64_t> ids;
    ids.reserve(options.size());
    for (std::size_t i = 0; i < options.size(); ++i) {
        const auto& opt = options[i];
        std::int64_t oid = 0;
        bool ok = memdb_ ? memdb_->insertOption(ts_ms,
                                                step,
                                                opt.source,
                                                opt.payload_json,
                                                opt.confidence,
                                                (selected_index.has_value() && selected_index.value() == i),
                                                run_id_,
                                                oid) : false;
        if (ok) {
            ids.push_back(oid);
            // Ensure we have a posterior entry for this key
            auto it = posteriors_.find(opt.key);
            if (it == posteriors_.end()) {
                posteriors_[opt.key] = Posterior{};
            }
        } else {
            ids.push_back(0);
        }
    }
    return ids;
}

ReasonScore Phase6Reasoner::scoreOptions(const std::vector<ReasonOption>& options) {
    ReasonScore rs;
    rs.scores.resize(options.size(), 0.0);
    if (options.empty()) {
        rs.best_index = 0;
        rs.best_score = 0.0;
        return rs;
    }
    double best = -1e9;
    std::size_t best_idx = 0;
    double alignment = 0.5;
    if (phase8_goals_) {
        alignment = std::min(1.0, std::max(0.0, phase8_goals_->getLastCoherence()));
    }
    const double alpha_eff = alpha_ * (0.5 + (1.0 - alignment));
    if (autonomy_env_ && autonomy_env_->valid) {
        if (debug_) {
            std::cerr << "[Phase6] autonomy tier=" << static_cast<int>(autonomy_env_->tier)
                      << " score=" << autonomy_env_->getEffectiveAutonomy() << std::endl;
        }
    }

    double trust = 0.5;
    if (metacog_) {
        trust = std::min(1.0, std::max(0.0, metacog_->getSelfTrust()));
    }
    double personality_risk = 0.0;
    double identity_conf = 0.5;
    if (self_model_ && self_model_->isLoaded()) {
        const auto& p = self_model_->personality();
        if (!p.trait_json.empty()) {
            std::size_t pos = p.trait_json.find("\"risk_tolerance\"");
            if (pos != std::string::npos) {
                pos = p.trait_json.find(':', pos);
                if (pos != std::string::npos) {
                    try {
                        double v = std::stod(p.trait_json.substr(pos + 1));
                        if (v >= -1.0 && v <= 1.0) {
                            personality_risk = v;
                        }
                    } catch (...) {
                    }
                }
            }
        }
        const auto& id = self_model_->identity();
        if (id.confidence.has_value()) {
            identity_conf = std::min(1.0, std::max(0.0, id.confidence.value()));
        }
    }
    const double trust_factor = (0.9 + 0.4 * (1.0 - trust));
    const double risk_factor = (1.0 + 0.1 * personality_risk);
    const double identity_factor = (0.95 + 0.1 * (identity_conf - 0.5));
    double alpha_eff_trust = alpha_eff * trust_factor * risk_factor * identity_factor;
    alpha_eff_trust = std::min(alpha_eff_trust, alpha_ * 2.0);
    alpha_eff_trust = std::max(alpha_eff_trust, alpha_ * 0.25);

    std::vector<double> pre_scores(options.size(), 0.0);
    for (std::size_t i = 0; i < options.size(); ++i) {
        const auto& opt = options[i];
        double prior = 0.0;
        auto it = posteriors_.find(opt.key);
        if (it != posteriors_.end()) prior = it->second.mean;
        double s = prior - alpha_eff_trust * opt.complexity;
        pre_scores[i] = s;
        rs.scores[i] = s;
        if (s > best) {
            best = s;
            best_idx = i;
        }
    }

    double pre_max = pre_scores[0];
    for (double v : pre_scores) {
        if (v > pre_max) pre_max = v;
    }
    double pre_z = 0.0;
    for (double v : pre_scores) {
        pre_z += std::exp(v - pre_max);
    }
    double pre_rank_entropy = 0.0;
    if (pre_z > 0.0) {
        for (double v : pre_scores) {
            double p = std::exp(v - pre_max) / pre_z;
            if (p > 0.0) {
                pre_rank_entropy -= p * std::log(p);
            }
        }
    }

    bool ethics_hard_block = false;
    double ethics_soft_risk = 0.0;
    if (memdb_ && run_id_ > 0) {
        auto ethics_entries = memdb_->getRecentEthicsRegulator(run_id_, 1);
        if (!ethics_entries.empty()) {
            const auto& e = ethics_entries.front();
            if (e.decision == "deny") {
                ethics_hard_block = true;
            }
        }
    }

    double autonomy_score = 0.0;
    AutonomyTier autonomy_tier = AutonomyTier::NONE;
    if (autonomy_env_ && autonomy_env_->valid) {
        autonomy_score = autonomy_env_->getEffectiveAutonomy();
        autonomy_tier = autonomy_env_->tier;
    }

    double autonomy_gain = 0.0;
    double exploration_bias = 0.0;
    int autonomy_applied = 0;
    std::string autonomy_tier_str = "NONE";
    if (autonomy_tier == AutonomyTier::SHADOW) autonomy_tier_str = "SHADOW";
    else if (autonomy_tier == AutonomyTier::CONDITIONAL) autonomy_tier_str = "CONDITIONAL";
    else if (autonomy_tier == AutonomyTier::FULL) autonomy_tier_str = "FULL";

    const double AUTONOMY_GAIN_MAX = 0.3;
    if (!ethics_hard_block && autonomy_score > 0.0) {
        double gate_trust = std::min(1.0, std::max(0.0, trust));
        autonomy_gain = AUTONOMY_GAIN_MAX * autonomy_score * gate_trust;
        if (autonomy_gain < 0.0) autonomy_gain = 0.0;
        if (autonomy_gain > AUTONOMY_GAIN_MAX) autonomy_gain = AUTONOMY_GAIN_MAX;
        exploration_bias = autonomy_gain;
        if (exploration_bias < 0.0) exploration_bias = 0.0;
        if (exploration_bias > 1.0) exploration_bias = 1.0;
        if (autonomy_gain > 0.0) autonomy_applied = 1;
    }

    std::vector<double> post_scores = pre_scores;
    if (autonomy_applied == 1) {
        double mean_pre = 0.0;
        for (double v : pre_scores) {
            mean_pre += v;
        }
        mean_pre /= static_cast<double>(pre_scores.size());
        for (std::size_t i = 0; i < post_scores.size(); ++i) {
            post_scores[i] = pre_scores[i] * (1.0 - exploration_bias) + mean_pre * exploration_bias;
        }
    }

    double post_max = post_scores[0];
    for (double v : post_scores) {
        if (v > post_max) post_max = v;
    }
    double post_z = 0.0;
    for (double v : post_scores) {
        post_z += std::exp(v - post_max);
    }
    double post_rank_entropy = 0.0;
    if (post_z > 0.0) {
        for (double v : post_scores) {
            double p = std::exp(v - post_max) / post_z;
            if (p > 0.0) {
                post_rank_entropy -= p * std::log(p);
            }
        }
    }

    std::vector<std::pair<std::size_t, double>> pre_rank_vec;
    std::vector<std::pair<std::size_t, double>> post_rank_vec;
    pre_rank_vec.reserve(options.size());
    post_rank_vec.reserve(options.size());
    for (std::size_t i = 0; i < options.size(); ++i) {
        pre_rank_vec.emplace_back(i, pre_scores[i]);
        post_rank_vec.emplace_back(i, post_scores[i]);
    }
    std::sort(pre_rank_vec.begin(), pre_rank_vec.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::sort(post_rank_vec.begin(), post_rank_vec.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::vector<int> pre_rank(options.size(), 0);
    std::vector<int> post_rank(options.size(), 0);
    for (std::size_t i = 0; i < pre_rank_vec.size(); ++i) {
        pre_rank[pre_rank_vec[i].first] = static_cast<int>(i);
    }
    for (std::size_t i = 0; i < post_rank_vec.size(); ++i) {
        post_rank[post_rank_vec[i].first] = static_cast<int>(i);
    }
    double shift_sum = 0.0;
    double shift_max = 0.0;
    for (std::size_t i = 0; i < options.size(); ++i) {
        double delta = std::fabs(static_cast<double>(pre_rank[i] - post_rank[i]));
        shift_sum += delta;
        if (delta > shift_max) shift_max = delta;
    }
    double option_rank_shift_mean = shift_sum / static_cast<double>(options.size());
    double option_rank_shift_max = shift_max;

    best = -1e9;
    best_idx = 0;
    for (std::size_t i = 0; i < post_scores.size(); ++i) {
        double s = post_scores[i];
        rs.scores[i] = s;
        if (s > best) {
            best = s;
            best_idx = i;
        }
    }

    rs.best_index = best_idx;
    rs.best_score = best;

    if (memdb_ && run_id_ > 0) {
        std::int64_t ts_ms = now_ms();
        int options_considered = static_cast<int>(options.size());
        std::int64_t selected_option_id = 0;
        double decision_confidence = options[best_idx].confidence;
        int ethics_flag = ethics_hard_block ? 1 : 0;
        int applied_flag = autonomy_applied;
        std::string veto_reason;
        if (ethics_flag == 1) {
            veto_reason = "ethics_hard_block";
        } else if (autonomy_score <= 0.0 || autonomy_gain <= 0.0) {
            veto_reason = "no_autonomy_influence";
        }
        std::int64_t modulation_id = 0;
        (void)memdb_->insertAutonomyModulation(run_id_,
                                               ts_ms,
                                               autonomy_score,
                                               autonomy_tier_str,
                                               autonomy_gain,
                                               ethics_flag,
                                               ethics_soft_risk,
                                               pre_rank_entropy,
                                               post_rank_entropy,
                                               exploration_bias,
                                               options_considered,
                                               option_rank_shift_mean,
                                               option_rank_shift_max,
                                               selected_option_id,
                                               decision_confidence,
                                               applied_flag,
                                               veto_reason,
                                               modulation_id);
    }

    return rs;
}

void Phase6Reasoner::applyOptionResult(std::int64_t option_id,
                                       const std::string& option_key,
                                       double observed_reward,
                                       std::int64_t ts_ms,
                                       bool emit_verification) {
    auto& post = posteriors_[option_key];
    // Online mean update
    post.n += 1;
    double n = static_cast<double>(post.n);
    double old_mean = post.mean;
    post.mean += (observed_reward - post.mean) / (n > 0.0 ? n : 1.0);
    post.last_ms = ts_ms;

    // Phase 7 affective feedback
    if (phase7_affect_) {
        double drift = std::abs(observed_reward - old_mean);
        phase7_affect_->updateFromReward(observed_reward, drift);
    }

    if (memdb_) {
        (void)memdb_->upsertOptionStats(option_id, post.n, post.mean, ts_ms);
        if (emit_verification) {
            // Simple mismatch criterion: strong negative reward vs positive expectation
            bool contradiction = (observed_reward < -0.25 && post.mean > 0.15);
            std::ostringstream dj;
            dj << "{\"key\":\"" << option_key << "\","
               << "\"observed_reward\":" << std::fixed << std::setprecision(6) << observed_reward << ","
               << "\"posterior_mean\":" << std::fixed << std::setprecision(6) << post.mean << "}";
            std::int64_t vid = 0;
            // No fact_id on minimal path; use 0 and encode context in details_json
            (void)memdb_->insertVerification(ts_ms, 0, contradiction ? "contradiction" : "observation",
                                             contradiction, dj.str(), run_id_, vid);
            // Phase 7 bridge: intent formation/resolution
            maybeEmitIntentFormation(option_key, ts_ms, contradiction, observed_reward, post.mean);
        }
    }
}

double Phase6Reasoner::getPosteriorMean(const std::string& key) const {
    auto it = posteriors_.find(key);
    return it == posteriors_.end() ? 0.0 : it->second.mean;
}

void Phase6Reasoner::onEpisodeEnd(std::int64_t episode_index, double contradiction_rate, double avg_reward) {
    if (phase7_reflect_ && phase7_affect_) {
        auto state = phase7_affect_->getState();
        phase7_reflect_->maybeReflect(episode_index, contradiction_rate, avg_reward,
                                      state.valence, state.arousal);
    }
}

void Phase6Reasoner::maybeEmitIntentFormation(const std::string& key,
                                              std::int64_t ts_ms,
                                              bool contradiction,
                                              double observed_reward,
                                              double posterior_mean) {
    if (!memdb_) return;
    bool last = false;
    auto it = last_contradiction_.find(key);
    if (it != last_contradiction_.end()) last = it->second;

    const double mismatch = std::fabs(observed_reward - posterior_mean);
    const double confidence = std::min(1.0, std::max(0.0, mismatch));

    if (contradiction && !last) {
        // Spawn intent correction node
        std::ostringstream sj;
        sj << "{\"key\":\"" << key << "\","
           << "\"posterior_mean\":" << std::fixed << std::setprecision(6) << posterior_mean << ","
           << "\"observed_reward\":" << std::fixed << std::setprecision(6) << observed_reward << ","
           << "\"event\":\"contradiction_detected\"}";
        std::int64_t node_id = 0;
        if (memdb_->insertIntentNode(ts_ms, "correction", sj.str(), confidence, "reasoner", run_id_, node_id)) {
            last_intent_node_[key] = node_id;
            last_contradiction_[key] = true;
        }
    } else if (!contradiction && last) {
        // Resolution event: connect prior correction node to resolution node
        auto nit = last_intent_node_.find(key);
        if (nit != last_intent_node_.end()) {
            std::int64_t corr_node = nit->second;
            std::ostringstream rj;
            rj << "{\"key\":\"" << key << "\","
               << "\"posterior_mean\":" << std::fixed << std::setprecision(6) << posterior_mean << ","
               << "\"observed_reward\":" << std::fixed << std::setprecision(6) << observed_reward << ","
               << "\"event\":\"contradiction_resolved\"}";
            std::int64_t res_node = 0;
            if (memdb_->insertIntentNode(ts_ms, "resolution", rj.str(), confidence, "reasoner", run_id_, res_node)) {
                std::int64_t edge_id = 0;
                std::ostringstream dj;
                dj << "{\"key\":\"" << key << "\"," << "\"type\":\"contradiction_resolved\"}";
                (void)memdb_->insertIntentEdge(ts_ms, corr_node, res_node, "contradiction_resolved", mismatch, dj.str(), run_id_, edge_id);
            }
        }
        last_contradiction_[key] = false;
    }
}

} // namespace Core
} // namespace NeuroForge
