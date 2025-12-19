#include "core/Phase11SelfRevision.h"
#include "core/MemoryDB.h"
#include "core/AutonomyEnvelope.h"
#include "core/StageC_AutonomyGate.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <sstream>
#include <regex>
#include <limits>

namespace {

std::string escape_json_string(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
        case '\"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                out += '\\';
                out += 'u';
                const char* hex = "0123456789abcdef";
                unsigned char uc = static_cast<unsigned char>(c);
                out += '0';
                out += '0';
                out += hex[(uc >> 4) & 0xF];
                out += hex[uc & 0xF];
            } else {
                out += c;
            }
        }
    }
    return out;
}

}

namespace NeuroForge {
namespace Core {

static inline double mean_or_nan(const std::vector<double>& values) {
    if (values.empty()) return std::numeric_limits<double>::quiet_NaN();
    double s = 0.0;
    for (double v : values) s += v;
    return s / static_cast<double>(values.size());
}

static inline double compute_reward_slope_per_s(const std::vector<MemoryDB::RewardEntry>& rewards) {
    if (rewards.size() < 2) return std::numeric_limits<double>::quiet_NaN();
    const double t0 = static_cast<double>(rewards.front().ts_ms);
    double sum_t = 0.0;
    double sum_r = 0.0;
    std::size_t n = 0;
    for (const auto& r : rewards) {
        const double t = (static_cast<double>(r.ts_ms) - t0) / 1000.0;
        sum_t += t;
        sum_r += r.reward;
        ++n;
    }
    if (n < 2) return std::numeric_limits<double>::quiet_NaN();
    const double mean_t = sum_t / static_cast<double>(n);
    const double mean_r = sum_r / static_cast<double>(n);
    double num = 0.0;
    double den = 0.0;
    for (const auto& r : rewards) {
        const double t = (static_cast<double>(r.ts_ms) - t0) / 1000.0;
        const double dt = t - mean_t;
        const double dr = r.reward - mean_r;
        num += dt * dr;
        den += dt * dt;
    }
    if (den <= 1e-12) return 0.0;
    return num / den;
}

static inline double compute_prediction_error_mean(const std::vector<MemoryDB::MetacognitionEntry>& entries) {
    double sum = 0.0;
    std::size_t n = 0;
    for (const auto& e : entries) {
        double s = 0.0;
        std::size_t k = 0;
        if (e.narrative_rmse.has_value()) { s += *e.narrative_rmse; ++k; }
        if (e.goal_mae.has_value()) { s += *e.goal_mae; ++k; }
        if (k == 0) continue;
        sum += (s / static_cast<double>(k));
        ++n;
    }
    if (n == 0) return std::numeric_limits<double>::quiet_NaN();
    return sum / static_cast<double>(n);
}

static inline std::string classify_outcome(double trust_pre,
                                           double trust_post,
                                           double pred_err_pre,
                                           double pred_err_post,
                                           double coherence_pre,
                                           double coherence_post,
                                           double reward_slope_pre,
                                           double reward_slope_post) {
    struct MetricVote {
        bool has{false};
        bool improved{false};
        bool worsened{false};
    };

    MetricVote trust{};
    if (!std::isnan(trust_pre) && !std::isnan(trust_post)) {
        trust.has = true;
        const double d = trust_post - trust_pre;
        trust.improved = d > 0.02;
        trust.worsened = d < -0.02;
    }

    MetricVote pred{};
    if (!std::isnan(pred_err_pre) && !std::isnan(pred_err_post)) {
        pred.has = true;
        const double d = pred_err_pre - pred_err_post;
        pred.improved = d > 0.02;
        pred.worsened = d < -0.02;
    }

    MetricVote coh{};
    if (!std::isnan(coherence_pre) && !std::isnan(coherence_post)) {
        coh.has = true;
        const double d = coherence_post - coherence_pre;
        coh.improved = d > 0.02;
        coh.worsened = d < -0.02;
    }

    MetricVote slope{};
    if (!std::isnan(reward_slope_pre) && !std::isnan(reward_slope_post)) {
        slope.has = true;
        const double d = reward_slope_post - reward_slope_pre;
        slope.improved = d > 1e-4;
        slope.worsened = d < -1e-4;
    }

    int improvements = 0;
    int worsens = 0;
    int available = 0;
    for (const auto& v : {trust, pred, coh, slope}) {
        if (!v.has) continue;
        ++available;
        if (v.improved) ++improvements;
        if (v.worsened) ++worsens;
    }

    if (available == 0) return "Neutral";
    if (available == 1) {
        if (improvements == 1) return "Beneficial";
        if (worsens == 1) return "Harmful";
        return "Neutral";
    }

    if (improvements >= 2 && worsens == 0) return "Beneficial";
    if (worsens >= 2 && improvements == 0) return "Harmful";
    return "Neutral";
}

bool Phase11SelfRevision::maybeRevise(const std::string& context) {
    if (!shouldTriggerRevision()) {
        return false;
    }
    return runForLatest(context);
}

bool Phase11SelfRevision::runForLatest(const std::string& context) {
    if (!db_) return false;

    std::int64_t now_ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (outcome_eval_window_ms_ > 0) {
        const std::int64_t max_revision_ts = now_ts_ms - outcome_eval_window_ms_;
        auto pending_revision_id = db_->getLatestUnevaluatedSelfRevisionId(run_id_, max_revision_ts);
        if (pending_revision_id.has_value() && *pending_revision_id > 0) {
            auto revision_ts = db_->getSelfRevisionTimestamp(*pending_revision_id);
            if (revision_ts.has_value()) {
                const std::int64_t pre_start = *revision_ts - outcome_eval_window_ms_;
                const std::int64_t pre_end = *revision_ts;
                const std::int64_t post_start = *revision_ts;
                const std::int64_t post_end = *revision_ts + outcome_eval_window_ms_;

                const auto pre_metacog = db_->getMetacognitionBetween(run_id_, pre_start, pre_end, 200);
                const auto post_metacog = db_->getMetacognitionBetween(run_id_, post_start, post_end, 200);

                std::vector<double> trust_pre_vals;
                trust_pre_vals.reserve(pre_metacog.size());
                for (const auto& e : pre_metacog) trust_pre_vals.push_back(e.self_trust);
                std::vector<double> trust_post_vals;
                trust_post_vals.reserve(post_metacog.size());
                for (const auto& e : post_metacog) trust_post_vals.push_back(e.self_trust);

                const double trust_pre = mean_or_nan(trust_pre_vals);
                const double trust_post = mean_or_nan(trust_post_vals);
                const double pred_pre = compute_prediction_error_mean(pre_metacog);
                const double pred_post = compute_prediction_error_mean(post_metacog);

                const auto pre_mot = db_->getMotivationStatesBetween(run_id_, pre_start, pre_end, 200);
                const auto post_mot = db_->getMotivationStatesBetween(run_id_, post_start, post_end, 200);
                std::vector<double> coh_pre_vals;
                coh_pre_vals.reserve(pre_mot.size());
                for (const auto& e : pre_mot) coh_pre_vals.push_back(e.coherence);
                std::vector<double> coh_post_vals;
                coh_post_vals.reserve(post_mot.size());
                for (const auto& e : post_mot) coh_post_vals.push_back(e.coherence);
                const double coherence_pre = mean_or_nan(coh_pre_vals);
                const double coherence_post = mean_or_nan(coh_post_vals);

                const auto pre_rewards = db_->getRewardsBetween(run_id_, pre_start, pre_end, 1000);
                const auto post_rewards = db_->getRewardsBetween(run_id_, post_start, post_end, 1000);
                const double reward_slope_pre = compute_reward_slope_per_s(pre_rewards);
                const double reward_slope_post = compute_reward_slope_per_s(post_rewards);

                const std::string outcome_class = classify_outcome(trust_pre,
                                                                   trust_post,
                                                                   pred_pre,
                                                                   pred_post,
                                                                   coherence_pre,
                                                                   coherence_post,
                                                                   reward_slope_pre,
                                                                   reward_slope_post);

                (void)db_->insertSelfRevisionOutcome(*pending_revision_id,
                                                     now_ts_ms,
                                                     outcome_class,
                                                     trust_pre,
                                                     trust_post,
                                                     pred_pre,
                                                     pred_post,
                                                     coherence_pre,
                                                     coherence_post,
                                                     reward_slope_pre,
                                                     reward_slope_post);
            }
        }
    }

    StageC_AutonomyGate::Result stage_c_result{};
    bool stage_c_ran = false;
    if (run_id_ > 0) {
        AutonomyEnvelope env{};
        if (autonomy_env_) {
            env = *autonomy_env_;
        }
        StageC_AutonomyGate gate(db_);
        stage_c_result = gate.evaluateAndApply(env, run_id_, 20);
        stage_c_ran = true;
    }
    
    // Check if we should trigger a revision
    auto trigger = checkRevisionTriggers();
    if (!trigger.has_value()) {
        return false;
    }
    
    // Get recent explanations and metacognition data
    auto explanations = db_->getRecentExplanations(run_id_, analysis_window_);
    auto metacog_entries = db_->getRecentMetacognition(run_id_, analysis_window_);
    
    if (explanations.empty() && metacog_entries.empty()) {
        return false; // Not enough data for analysis
    }
    
    // Analyze patterns and generate parameter deltas
    std::vector<ParameterDelta> deltas;
    
    // Analyze failure patterns from explanations
    if (!explanations.empty()) {
        auto pattern_deltas = analyzeFailurePatterns(explanations);
        deltas.insert(deltas.end(), pattern_deltas.begin(), pattern_deltas.end());
    }
    
    // Analyze trust trends from metacognition
    if (!metacog_entries.empty()) {
        auto trend_deltas = analyzeTrustTrends(metacog_entries);
        deltas.insert(deltas.end(), trend_deltas.begin(), trend_deltas.end());
    }
    
    if (deltas.empty()) {
        if (trigger->type == RevisionTrigger::PERIODIC && revision_interval_ms_ <= 5000) {
            deltas.push_back({"phase11.noop", 0.0, 0.9,
                             "No parameter deltas; emit no-op revision for validation"});
        } else {
            return false; // No revisions needed
        }
    }
    
    // Safety validation and clamping
    if (!validateRevisionSafety(deltas)) {
        std::cerr << "[Phase11] Revision failed safety validation" << std::endl;
        return false;
    }
    clampParameterDeltas(deltas);
    
    std::string revision_json = generateRevisionJson(deltas);
    std::string driver_explanation = synthesizeDriverExplanation(trigger.value(), deltas);
    if (autonomy_env_ && autonomy_env_->valid) {
        const double base_autonomy = autonomy_env_->autonomy_score;
        const double cap = stage_c_ran ? static_cast<double>(stage_c_result.autonomy_cap_multiplier) : autonomy_env_->autonomy_cap_multiplier;
        driver_explanation += " base_autonomy=" + std::to_string(base_autonomy);
        driver_explanation += " autonomy_cap_multiplier=" + std::to_string(cap);
        driver_explanation += " effective_autonomy=" + std::to_string(base_autonomy * cap);
        if (stage_c_ran) {
            driver_explanation += " stage_c_rr=" + std::to_string(stage_c_result.revision_reputation);
        }
    }
    
    // Get current trust for before/after tracking
    double trust_before = 0.5;
    if (!metacog_entries.empty()) {
        trust_before = metacog_entries[0].self_trust; // Most recent
    }
    
    std::int64_t revision_id = 0;
    std::int64_t ts_ms = now_ts_ms;
    
    bool success = db_->insertSelfRevision(run_id_, ts_ms, revision_json, 
                                          driver_explanation, trust_before, 
                                          trust_before, revision_id);
    
    if (success) {
        for (const auto& d : deltas) {
            current_revision_params_[d.parameter_name] += d.delta_value;
            double new_val = current_revision_params_[d.parameter_name];
            int phase_num = 0;
            std::smatch m;
            std::regex re("^phase(\\d+)\\.");
            if (std::regex_search(d.parameter_name, m, re) && m.size() >= 2) {
                try { phase_num = std::stoi(m[1].str()); } catch (...) { phase_num = 0; }
            }
            db_->insertParameterHistory(run_id_, revision_id, phase_num, d.parameter_name, new_val, ts_ms);
        }

        std::uint64_t proposal_step = 0;
        if (!context.empty()) {
            std::smatch step_match;
            std::regex step_re("step=([0-9]+)");
            if (std::regex_search(context, step_match, step_re) && step_match.size() >= 2) {
                try {
                    proposal_step = static_cast<std::uint64_t>(std::stoll(step_match[1].str()));
                } catch (...) {
                    proposal_step = 0;
                }
            }
        }

        std::string trigger_type_str;
        switch (trigger->type) {
        case RevisionTrigger::TRUST_DRIFT: trigger_type_str = "TRUST_DRIFT"; break;
        case RevisionTrigger::PERIODIC: trigger_type_str = "PERIODIC"; break;
        case RevisionTrigger::PATTERN_DETECTED: trigger_type_str = "PATTERN_DETECTED"; break;
        case RevisionTrigger::MANUAL: trigger_type_str = "MANUAL"; break;
        default: trigger_type_str = "UNKNOWN"; break;
        }

        std::ostringstream trait;
        trait << "{";
        trait << "\"source\":\"phase11_self_revision\",";
        trait << "\"trigger_type\":\"" << trigger_type_str << "\",";
        trait << "\"trigger_confidence\":" << trigger->confidence << ",";
        trait << "\"trigger_description\":\"" << escape_json_string(trigger->description) << "\",";
        trait << "\"deltas\":[";
        for (std::size_t i = 0; i < deltas.size(); ++i) {
            const auto& d = deltas[i];
            if (i > 0) trait << ",";
            trait << "{";
            trait << "\"parameter\":\"" << escape_json_string(d.parameter_name) << "\",";
            trait << "\"delta\":" << d.delta_value << ",";
            trait << "\"confidence\":" << d.confidence;
            trait << "}";
        }
        trait << "]";
        trait << "}";
        std::string trait_json = trait.str();

        std::optional<int> source_phase = 11;
        std::optional<std::int64_t> revision_ref;
        if (revision_id > 0) {
            revision_ref = revision_id;
        }
        std::int64_t personality_id = 0;
        int proposal_flag = 1;
        int approved_flag = 0;
        db_->insertPersonalityHistory(run_id_,
                                      ts_ms,
                                      proposal_step,
                                      trait_json,
                                      proposal_flag,
                                      approved_flag,
                                      source_phase,
                                      revision_ref,
                                      driver_explanation,
                                      personality_id);

        last_revision_ts_ = ts_ms;
        std::cerr << "[Phase11] Generated revision " << revision_id 
                  << " with " << deltas.size() << " parameter changes" << std::endl;
    }
    
    return success;
}

std::optional<Phase11SelfRevision::RevisionTrigger> Phase11SelfRevision::checkRevisionTriggers() {
    // Check time-based trigger
    std::int64_t current_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::int64_t last_revision = getLastRevisionTimestamp();
    if (current_ts - last_revision > revision_interval_ms_) {
        return RevisionTrigger{RevisionTrigger::PERIODIC, 0.8, "Periodic revision interval reached"};
    }
    
    // Check trust drift trigger
    auto recent_metacog = db_->getRecentMetacognition(run_id_, 5);
    if (recent_metacog.size() >= 3) {
        double recent_trust = recent_metacog[0].self_trust;
        double older_trust = recent_metacog[recent_metacog.size()-1].self_trust;
        double drift = older_trust - recent_trust; // Positive = trust decreased
        
        if (drift > trust_drift_threshold_) {
            return RevisionTrigger{RevisionTrigger::TRUST_DRIFT, 0.9, 
                                 "Significant trust drift detected: -" + std::to_string(drift)};
        }
    }
    
    // Check pattern-based triggers
    auto explanations = db_->getRecentExplanations(run_id_, analysis_window_);
    if (!explanations.empty()) {
        if (detectOverconfidencePattern(explanations)) {
            return RevisionTrigger{RevisionTrigger::PATTERN_DETECTED, 0.85, 
                                 "Overconfidence pattern detected"};
        }
        if (detectUnderexplorationPattern(explanations)) {
            return RevisionTrigger{RevisionTrigger::PATTERN_DETECTED, 0.8, 
                                 "Under-exploration pattern detected"};
        }
        if (detectReflectionCadenceIssues(explanations)) {
            return RevisionTrigger{RevisionTrigger::PATTERN_DETECTED, 0.75, 
                                 "Reflection cadence issues detected"};
        }
    }
    
    return std::nullopt;
}

std::vector<Phase11SelfRevision::ParameterDelta> Phase11SelfRevision::analyzeFailurePatterns(
    const std::vector<std::string>& explanations) {
    
    std::vector<ParameterDelta> deltas;
    
    // Count pattern occurrences
    int overconfidence_count = 0;
    int underexploration_count = 0;
    int coherence_issues = 0;
    
    for (const auto& explanation : explanations) {
        // Simple pattern matching on explanation text
        if (explanation.find("overconfident") != std::string::npos ||
            explanation.find("too confident") != std::string::npos) {
            overconfidence_count++;
        }
        if (explanation.find("exploration") != std::string::npos ||
            explanation.find("not enough variety") != std::string::npos) {
            underexploration_count++;
        }
        if (explanation.find("coherence") != std::string::npos ||
            explanation.find("inconsistent") != std::string::npos) {
            coherence_issues++;
        }
    }
    
    double total_explanations = static_cast<double>(explanations.size());
    
    // Generate deltas based on pattern frequency
    if (overconfidence_count / total_explanations > 0.4) {
        deltas.push_back({"phase6.learning_rate", -0.02, 0.8, 
                         "Reduce learning rate due to overconfidence pattern"});
        deltas.push_back({"phase9.trust_adjustment_rate", 0.01, 0.7,
                         "Increase trust sensitivity to overconfidence"});
    }
    
    if (underexploration_count / total_explanations > 0.3) {
        deltas.push_back({"phase6.exploration_bonus", 0.05, 0.75,
                         "Increase exploration due to under-exploration pattern"});
    }
    
    if (coherence_issues / total_explanations > 0.5) {
        deltas.push_back({"phase7.reflection_interval", -1000, 0.7,
                         "Increase reflection frequency for coherence issues"});
        deltas.push_back({"phase8.decay_scale", 0.02, 0.6,
                         "Adjust goal decay for better coherence"});
    }
    
    return deltas;
}

std::vector<Phase11SelfRevision::ParameterDelta> Phase11SelfRevision::analyzeTrustTrends(
    const std::vector<MemoryDB::MetacognitionEntry>& entries) {
    
    std::vector<ParameterDelta> deltas;
    
    if (entries.size() < 3) return deltas;
    
    // Calculate trust trend
    double trust_sum = 0.0;
    double trust_trend = 0.0;
    for (size_t i = 0; i < entries.size(); ++i) {
        trust_sum += entries[i].self_trust;
        if (i > 0) {
            trust_trend += (entries[i-1].self_trust - entries[i].self_trust); // Recent - older
        }
    }
    
    double avg_trust = trust_sum / entries.size();
    trust_trend /= (entries.size() - 1);
    
    // Analyze error patterns
    double avg_narrative_rmse = 0.0;
    double avg_goal_mae = 0.0;
    int rmse_count = 0, mae_count = 0;
    
    for (const auto& entry : entries) {
        if (entry.narrative_rmse.has_value()) {
            avg_narrative_rmse += entry.narrative_rmse.value();
            rmse_count++;
        }
        if (entry.goal_mae.has_value()) {
            avg_goal_mae += entry.goal_mae.value();
            mae_count++;
        }
    }
    
    if (rmse_count > 0) avg_narrative_rmse /= rmse_count;
    if (mae_count > 0) avg_goal_mae /= mae_count;
    
    // Generate deltas based on trends
    if (trust_trend < -0.05) { // Trust declining
        if (avg_narrative_rmse > 0.3) {
            deltas.push_back({"phase7.narrative_weight", 0.1, 0.8,
                             "Increase narrative weight due to declining trust and high RMSE"});
        }
        if (avg_goal_mae > 0.25) {
            deltas.push_back({"phase8.goal_stability_threshold", 0.05, 0.75,
                             "Adjust goal stability due to declining trust and high MAE"});
        }
    }
    
    if (avg_trust < 0.3) { // Very low trust
        deltas.push_back({"phase6.exploration_bonus", 0.08, 0.9,
                         "Increase exploration due to very low trust"});
        deltas.push_back({"phase7.reflection_interval", -2000, 0.85,
                         "Increase reflection frequency due to low trust"});
    }
    
    return deltas;
}

bool Phase11SelfRevision::detectOverconfidencePattern(const std::vector<std::string>& explanations) {
    int overconfidence_indicators = 0;
    for (const auto& explanation : explanations) {
        if (explanation.find("overconfident") != std::string::npos ||
            explanation.find("too certain") != std::string::npos ||
            explanation.find("high confidence") != std::string::npos) {
            overconfidence_indicators++;
        }
    }
    return overconfidence_indicators >= static_cast<int>(explanations.size() * 0.4);
}

bool Phase11SelfRevision::detectUnderexplorationPattern(const std::vector<std::string>& explanations) {
    int exploration_indicators = 0;
    for (const auto& explanation : explanations) {
        if (explanation.find("exploration") != std::string::npos ||
            explanation.find("variety") != std::string::npos ||
            explanation.find("same pattern") != std::string::npos) {
            exploration_indicators++;
        }
    }
    return exploration_indicators >= static_cast<int>(explanations.size() * 0.3);
}

bool Phase11SelfRevision::detectReflectionCadenceIssues(const std::vector<std::string>& explanations) {
    int cadence_indicators = 0;
    for (const auto& explanation : explanations) {
        if (explanation.find("reflection") != std::string::npos ||
            explanation.find("timing") != std::string::npos ||
            explanation.find("frequency") != std::string::npos) {
            cadence_indicators++;
        }
    }
    return cadence_indicators >= static_cast<int>(explanations.size() * 0.25);
}

std::string Phase11SelfRevision::generateRevisionJson(const std::vector<ParameterDelta>& deltas) {
    std::ostringstream json;
    json << "{\n";
    
    for (size_t i = 0; i < deltas.size(); ++i) {
        json << "  \"" << deltas[i].parameter_name << "\": " << deltas[i].delta_value;
        if (i < deltas.size() - 1) json << ",";
        json << "\n";
    }
    
    json << "}";
    return json.str();
}

std::string Phase11SelfRevision::synthesizeDriverExplanation(
    const RevisionTrigger& trigger, const std::vector<ParameterDelta>& deltas) {
    
    std::ostringstream explanation;
    explanation << "Revision triggered by: " << trigger.description 
                << " (confidence: " << trigger.confidence << "). ";
    
    explanation << "Applied " << deltas.size() << " parameter adjustments: ";
    
    for (size_t i = 0; i < deltas.size(); ++i) {
        explanation << deltas[i].parameter_name << " " 
                   << (deltas[i].delta_value > 0 ? "+" : "") << deltas[i].delta_value
                   << " (" << deltas[i].rationale << ")";
        if (i < deltas.size() - 1) explanation << "; ";
    }
    
    return explanation.str();
}

bool Phase11SelfRevision::validateRevisionSafety(const std::vector<ParameterDelta>& deltas) {
    // Check for reasonable delta magnitudes
    for (const auto& delta : deltas) {
        if (delta.confidence < 0.5) { // Require minimum confidence
            return false;
        }
        const bool looks_like_time_ms =
            (delta.parameter_name.find("interval") != std::string::npos) ||
            (delta.parameter_name.find("_ms") != std::string::npos) ||
            (delta.parameter_name.find("hysteresis") != std::string::npos);
        if (looks_like_time_ms) {
            if (std::abs(delta.delta_value) > 10000.0) {
                return false;
            }
        } else {
            if (std::abs(delta.delta_value) > 0.5) {
                return false;
            }
        }
    }
    
    // Check for conflicting changes
    std::map<std::string, double> param_changes;
    for (const auto& delta : deltas) {
        param_changes[delta.parameter_name] += delta.delta_value;
    }
    
    for (const auto& [param, total_change] : param_changes) {
        const bool looks_like_time_ms =
            (param.find("interval") != std::string::npos) ||
            (param.find("_ms") != std::string::npos) ||
            (param.find("hysteresis") != std::string::npos);
        if (looks_like_time_ms) {
            if (std::abs(total_change) > 20000.0) {
                return false;
            }
        } else {
            if (std::abs(total_change) > 0.3) {
                return false;
            }
        }
    }
    
    return true;
}

void Phase11SelfRevision::clampParameterDeltas(std::vector<ParameterDelta>& deltas) {
    for (auto& delta : deltas) {
        const bool looks_like_time_ms =
            (delta.parameter_name.find("interval") != std::string::npos) ||
            (delta.parameter_name.find("_ms") != std::string::npos) ||
            (delta.parameter_name.find("hysteresis") != std::string::npos);
        if (looks_like_time_ms) {
            delta.delta_value = std::max(-5000.0, std::min(5000.0, delta.delta_value));
        } else {
            delta.delta_value = std::max(-0.2, std::min(0.2, delta.delta_value));
        }
        delta.delta_value *= delta.confidence;
    }
}

bool Phase11SelfRevision::shouldTriggerRevision() {
    std::int64_t current_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::int64_t last_revision = getLastRevisionTimestamp();
    
    // Enforce minimum gap between revisions
    return (current_ts - last_revision) >= min_revision_gap_ms_;
}

std::int64_t Phase11SelfRevision::getLastRevisionTimestamp() {
    if (last_revision_ts_.has_value()) {
        return last_revision_ts_.value();
    }
    
    // Query database for last revision
    // For now, return 0 to allow first revision
    return 0;
}

} // namespace Core
} // namespace NeuroForge
