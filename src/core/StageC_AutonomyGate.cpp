#include "core/StageC_AutonomyGate.h"
#include "core/AutonomyEnvelope.h"
#include "core/MemoryDB.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <unordered_map>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <sstream>

namespace NeuroForge {
namespace Core {

static double clamp01d(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

static double safe_log1p(double x) {
    if (x <= -1.0) return 0.0;
    return std::log1p(x);
}

static double softplus01(double x) {
    return clamp01d(1.0 - std::exp(-std::max(0.0, x)));
}

static constexpr double kGoalMinPreferenceStrength01 = 0.05;

static float wilson_upper_bound_ub95(std::size_t k, std::size_t n) {
    if (n == 0) return 0.0f;
    const double z = 1.96;
    const double nd = static_cast<double>(n);
    const double phat = static_cast<double>(k) / nd;
    const double z2 = z * z;
    const double denom = 1.0 + z2 / nd;
    const double center = phat + z2 / (2.0 * nd);
    const double rad = z * std::sqrt((phat * (1.0 - phat) + z2 / (4.0 * nd)) / nd);
    const double upper = (center + rad) / denom;
    return static_cast<float>(std::clamp(upper, 0.0, 1.0));
}

static std::vector<MemoryDB::PreferenceMemoryEntry> load_preferences_with_fallback(MemoryDB* db,
                                                                                  std::int64_t run_id,
                                                                                  std::size_t n,
                                                                                  bool copy_forward) {
    std::vector<MemoryDB::PreferenceMemoryEntry> prefs;
    if (!db || run_id <= 0 || n == 0) return prefs;

    prefs = db->getPreferenceMemory(run_id, n);
    if (!prefs.empty()) return prefs;

    const auto prev = db->getLatestRunIdWithPreferenceMemory(run_id);
    if (!prev.has_value()) return prefs;

    prefs = db->getPreferenceMemory(*prev, n);
    if (!copy_forward || prefs.empty()) return prefs;

    for (const auto& p : prefs) {
        if (p.key.empty()) continue;
        std::int64_t pref_id = 0;
        (void)db->upsertPreferenceMemory(run_id,
                                         p.key,
                                         p.preferred_value,
                                         std::max(0.0, p.strength01),
                                         p.evidence_n,
                                         p.beneficial_n,
                                         p.harmful_n,
                                         p.updated_ts_ms,
                                         pref_id);
    }

    return db->getPreferenceMemory(run_id, n);
}

static std::string format_goal_description_from_preference(const MemoryDB::PreferenceMemoryEntry& p) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss << "Preserve internal coherence by maintaining preference '" << p.key << "' near "
        << std::setprecision(3) << p.preferred_value;
    return oss.str();
}

StageC_AutonomyGate::Result StageC_AutonomyGate::evaluateV1(std::int64_t run_id, std::size_t window_size) const {
    Result out{};
    out.version = 1;
    out.revision_reputation = computeRevisionReputation(run_id, window_size, out.window_n);
    out.autonomy_cap_multiplier = (out.window_n == 0) ? 1.0f : mapReputationToCap(out.revision_reputation);
#ifndef NDEBUG
    assert(out.autonomy_cap_multiplier <= 1.0f);
    assert(out.autonomy_cap_multiplier >= 0.5f);
#endif
    return out;
}

StageC_AutonomyGate::Result StageC_AutonomyGate::evaluateV2(std::int64_t run_id, std::size_t window_size) const {
    Result out{};
    out.version = 2;
    out.window_n = 0;
    if (!db_ || run_id <= 0 || window_size == 0) {
        out.autonomy_cap_multiplier = 1.0f;
        return out;
    }

    const auto outcomes = db_->getRecentSelfRevisionOutcomes(run_id, window_size);
    out.window_n = outcomes.size();
    for (const auto& o : outcomes) {
        if (o.outcome_class == "Beneficial") {
            ++out.beneficial_n;
        } else if (o.outcome_class == "Harmful") {
            ++out.harmful_n;
        } else {
            ++out.neutral_n;
        }
    }

    const std::size_t decisive_n = out.beneficial_n + out.harmful_n;
    if (decisive_n == 0) {
        out.p_harm_mean = 0.0f;
        out.p_harm_ub95 = 0.0f;
        out.harm_risk_cap_multiplier = 1.0f;
        out.autonomy_cap_multiplier = 1.0f;
        return out;
    }

    const std::size_t alpha0 = 1;
    const std::size_t beta0 = 1;
    const std::size_t harm_adj = out.harmful_n + beta0;
    const std::size_t n_adj = decisive_n + alpha0 + beta0;
    out.p_harm_mean = static_cast<float>(static_cast<double>(harm_adj) / static_cast<double>(n_adj));
    out.p_harm_ub95 = wilson_upper_bound_ub95(harm_adj, n_adj);
    out.harm_risk_cap_multiplier = mapRiskUpperBoundToCap(out.p_harm_ub95);

    const auto credit = db_->getLatestAutonomyCredit(run_id);
    if (credit.has_value()) {
        out.autonomy_credit = static_cast<float>(clamp01d(credit->credit_value));
        out.autonomy_credit_cap_multiplier = mapCreditToCap(out.autonomy_credit);
    } else {
        out.autonomy_credit = 0.0f;
        out.autonomy_credit_cap_multiplier = 1.0f;
    }

    out.autonomy_cap_multiplier = std::min(out.harm_risk_cap_multiplier, out.autonomy_credit_cap_multiplier);
#ifndef NDEBUG
    assert(out.autonomy_cap_multiplier <= 1.0f);
    assert(out.autonomy_cap_multiplier >= 0.5f);
#endif
    return out;
}

StageC_AutonomyGate::Result StageC_AutonomyGate::evaluateV3(std::int64_t run_id, std::size_t window_size) const {
    Result out = evaluateV2(run_id, window_size);
    out.version = 3;
    if (!db_ || run_id <= 0) return out;

    auto prefs = db_->getPreferenceMemory(run_id, 1000);
    if (prefs.empty()) {
        const auto prev = db_->getLatestRunIdWithPreferenceMemory(run_id);
        if (prev.has_value()) {
            prefs = db_->getPreferenceMemory(*prev, 1000);
        }
    }
    double sum_strength = 0.0;
    std::size_t active_n = 0;
    for (const auto& p : prefs) {
        if (p.key.empty()) continue;
        if (p.strength01 < 0.05) continue;
        sum_strength += std::max(0.0, p.strength01);
        ++active_n;
    }

    out.preference_active_n = active_n;
    out.preference_rigidity01 = static_cast<float>(softplus01(sum_strength));
    out.preference_destabilization01 = 0.0f;
    return out;
}

StageC_AutonomyGate::Result StageC_AutonomyGate::evaluateV4(std::int64_t run_id, std::size_t window_size) const {
    Result out = evaluateV3(run_id, window_size);
    out.version = 4;
    out.goal_ttl_ms = 7LL * 24LL * 60LL * 60LL * 1000LL;

    if (!db_ || run_id <= 0) return out;

    bool veto = false;
    const auto ethics = db_->getRecentEthicsRegulator(run_id, 1);
    if (!ethics.empty() && ethics.front().decision == "deny") {
        veto = true;
    }

    const auto consist = db_->getRecentConsistency(run_id, 1);
    if (!consist.empty() && consist.front().consistency_score < 0.75) {
        veto = true;
    }

    out.goal_governance_veto = veto;

    const auto prefs = load_preferences_with_fallback(db_, run_id, 1000, false);
    std::size_t candidate_n = 0;
    for (const auto& p : prefs) {
        if (p.key.empty()) continue;
        if (p.strength01 < kGoalMinPreferenceStrength01) continue;
        ++candidate_n;
        if (candidate_n >= 3) break;
    }
    out.goal_candidate_n = candidate_n;
    return out;
}

StageC_AutonomyGate::Result StageC_AutonomyGate::evaluateAndApply(AutonomyEnvelope& envelope, std::int64_t run_id, std::size_t window_size) {
    Result out = evaluateV1(run_id, window_size);
    if (out.window_n == 0) {
        out.applied = false;
        return out;
    }
    out.applied = envelope.applyAutonomyCap(static_cast<double>(out.autonomy_cap_multiplier));
    return out;
}

StageC_AutonomyGate::Result StageC_AutonomyGate::evaluateAndApplyV2(AutonomyEnvelope& envelope, std::int64_t run_id, std::size_t window_size) {
    Result out = evaluateV2(run_id, window_size);
    if (out.window_n == 0) {
        out.applied = false;
        return out;
    }
    out.applied = envelope.applyAutonomyCap(static_cast<double>(out.autonomy_cap_multiplier));
    return out;
}

StageC_AutonomyGate::Result StageC_AutonomyGate::evaluateAndApplyV3(AutonomyEnvelope& envelope, std::int64_t run_id, std::size_t window_size) {
    Result out = evaluateV3(run_id, window_size);
    if (out.window_n == 0) {
        out.applied = false;
        return out;
    }
    out.applied = envelope.applyAutonomyCap(static_cast<double>(out.autonomy_cap_multiplier));
    return out;
}

StageC_AutonomyGate::Result StageC_AutonomyGate::evaluateAndApplyV4(AutonomyEnvelope& envelope, std::int64_t run_id, std::size_t window_size) {
    Result out = evaluateV4(run_id, window_size);
    if (out.window_n == 0) {
        out.applied = false;
    } else {
        out.applied = envelope.applyAutonomyCap(static_cast<double>(out.autonomy_cap_multiplier));
    }

    if (!db_ || run_id <= 0) return out;
    if (out.goal_governance_veto) return out;
    if (out.goal_candidate_n == 0 || out.goal_ttl_ms <= 0) return out;

    const auto now_ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    const std::int64_t expires_ts_ms = now_ts_ms + out.goal_ttl_ms;

    const auto prefs = load_preferences_with_fallback(db_, run_id, 1000, true);
    std::size_t processed = 0;
    for (const auto& p : prefs) {
        if (p.key.empty()) continue;
        if (p.strength01 < kGoalMinPreferenceStrength01) continue;
        if (processed >= 3) break;

        const std::string desc = format_goal_description_from_preference(p);
        const double pr = clamp01d(0.5 + 0.5 * clamp01d(p.strength01));
        const double st = clamp01d(0.5 + 0.5 * clamp01d(p.strength01));

        std::int64_t goal_id = 0;
        bool inserted = false;
        bool reaffirmed = false;
        const bool ok = db_->upsertBoundedGoalNode(run_id,
                                                  desc,
                                                  pr,
                                                  st,
                                                  now_ts_ms,
                                                  expires_ts_ms,
                                                  false,
                                                  std::string(),
                                                  goal_id,
                                                  inserted,
                                                  reaffirmed);
        if (ok) {
            if (inserted) ++out.goal_created_n;
            if (reaffirmed) ++out.goal_reaffirmed_n;
        }
        ++processed;
    }
    return out;
}

StageC_AutonomyGate::PreferenceStabilization StageC_AutonomyGate::stabilizePreferenceDeltasV3(
    std::int64_t run_id,
    const std::map<std::string, double>& current_params,
    std::vector<std::pair<std::string, double>>& deltas_io,
    std::size_t outcome_window_size,
    std::size_t param_window_size) const {

    PreferenceStabilization out{};
    if (!db_ || run_id <= 0 || deltas_io.empty() || outcome_window_size == 0 || param_window_size == 0) {
        return out;
    }

    struct ParamStats {
        double sum{0.0};
        double sum_sq{0.0};
        std::size_t n{0};
        double sum_beneficial{0.0};
        std::size_t n_beneficial{0};
        double sum_harmful{0.0};
        std::size_t n_harmful{0};
        std::int64_t latest_ts_ms{0};
    };

    std::unordered_map<std::int64_t, int> outcome_class_by_revision;
    outcome_class_by_revision.reserve(outcome_window_size * 2);
    const auto outcomes = db_->getRecentSelfRevisionOutcomes(run_id, outcome_window_size);
    for (const auto& o : outcomes) {
        int cls = 0;
        if (o.outcome_class == "Beneficial") cls = 1;
        else if (o.outcome_class == "Harmful") cls = -1;
        outcome_class_by_revision[o.revision_id] = cls;
    }

    std::unordered_map<std::string, ParamStats> stats_by_param;
    stats_by_param.reserve(static_cast<std::size_t>(param_window_size / 2 + 8));
    const auto params = db_->getRecentParamHistory(run_id, param_window_size);
    for (const auto& rec : params) {
        if (rec.parameter.empty()) continue;
        auto& s = stats_by_param[rec.parameter];
        s.sum += rec.value;
        s.sum_sq += rec.value * rec.value;
        ++s.n;
        if (rec.ts_ms > s.latest_ts_ms) s.latest_ts_ms = rec.ts_ms;
        const auto it = outcome_class_by_revision.find(rec.revision_id);
        if (it != outcome_class_by_revision.end()) {
            if (it->second > 0) {
                s.sum_beneficial += rec.value;
                ++s.n_beneficial;
            } else if (it->second < 0) {
                s.sum_harmful += rec.value;
                ++s.n_harmful;
            }
        }
    }

    struct Pref {
        double preferred{0.0};
        double strength01{0.0};
        int evidence_n{0};
        int beneficial_n{0};
        int harmful_n{0};
        std::int64_t updated_ts_ms{0};
    };

    std::unordered_map<std::string, Pref> derived_by_param;
    derived_by_param.reserve(stats_by_param.size() + 8);
    for (const auto& kv : stats_by_param) {
        const auto& name = kv.first;
        const auto& s = kv.second;
        if (s.n == 0) continue;

        const double n_d = static_cast<double>(s.n);
        const double mean = s.sum / n_d;
        const double ex2 = s.sum_sq / n_d;
        const double var = std::max(0.0, ex2 - mean * mean);
        const double var01 = var / (var + 0.01);
        const double stability01 = clamp01d(1.0 - var01);

        const double evidence01 = clamp01d(safe_log1p(n_d) / safe_log1p(50.0));
        const double decisive_n = static_cast<double>(s.n_beneficial + s.n_harmful);
        const double benefit_bias01 = (decisive_n <= 0.0) ? 0.5 : clamp01d(static_cast<double>(s.n_beneficial) / (decisive_n + 1.0));

        const double strength01 = clamp01d(stability01 * evidence01 * benefit_bias01);
        if (strength01 < 0.02) continue;

        double preferred = mean;
        if (s.n_beneficial >= 3) {
            preferred = s.sum_beneficial / static_cast<double>(s.n_beneficial);
        }

        derived_by_param.emplace(name,
                                 Pref{preferred,
                                      strength01,
                                      static_cast<int>(s.n),
                                      static_cast<int>(s.n_beneficial),
                                      static_cast<int>(s.n_harmful),
                                      s.latest_ts_ms});
    }

    std::unordered_map<std::string, Pref> pref_by_param;
    pref_by_param.reserve(derived_by_param.size() + 64);
    {
        auto stored = db_->getPreferenceMemory(run_id, 2000);
        if (stored.empty()) {
            const auto prev = db_->getLatestRunIdWithPreferenceMemory(run_id);
            if (prev.has_value()) {
                stored = db_->getPreferenceMemory(*prev, 2000);
                if (!stored.empty()) {
                    for (const auto& p : stored) {
                        if (p.key.empty()) continue;
                        std::int64_t pref_id = 0;
                        (void)db_->upsertPreferenceMemory(run_id,
                                                         p.key,
                                                         p.preferred_value,
                                                         std::max(0.0, p.strength01),
                                                         p.evidence_n,
                                                         p.beneficial_n,
                                                         p.harmful_n,
                                                         p.updated_ts_ms,
                                                         pref_id);
                    }
                }
            }
        }

        for (const auto& p : stored) {
            if (p.key.empty()) continue;
            pref_by_param.emplace(p.key,
                                  Pref{p.preferred_value,
                                       std::max(0.0, p.strength01),
                                       p.evidence_n,
                                       p.beneficial_n,
                                       p.harmful_n,
                                       p.updated_ts_ms});
        }
    }

    for (const auto& kv : derived_by_param) {
        const auto& name = kv.first;
        const auto& d = kv.second;

        auto it = pref_by_param.find(name);
        if (it == pref_by_param.end()) {
            pref_by_param.emplace(name, d);
            std::int64_t pref_id = 0;
            (void)db_->upsertPreferenceMemory(run_id,
                                             name,
                                             d.preferred,
                                             d.strength01,
                                             d.evidence_n,
                                             d.beneficial_n,
                                             d.harmful_n,
                                             d.updated_ts_ms,
                                             pref_id);
            continue;
        }

        const double prev_strength = std::max(0.0, it->second.strength01);
        const double next_strength = (d.strength01 > prev_strength)
            ? clamp01d(prev_strength + 0.25 * (d.strength01 - prev_strength))
            : clamp01d(prev_strength + 0.02 * (d.strength01 - prev_strength));
        const double alpha = std::clamp(0.05 + 0.35 * clamp01d(d.strength01), 0.05, 0.4);
        const double next_preferred = it->second.preferred + alpha * (d.preferred - it->second.preferred);

        it->second.preferred = next_preferred;
        it->second.strength01 = next_strength;
        it->second.evidence_n = d.evidence_n;
        it->second.beneficial_n = d.beneficial_n;
        it->second.harmful_n = d.harmful_n;
        it->second.updated_ts_ms = d.updated_ts_ms;

        std::int64_t pref_id = 0;
        (void)db_->upsertPreferenceMemory(run_id,
                                         name,
                                         next_preferred,
                                         next_strength,
                                         d.evidence_n,
                                         d.beneficial_n,
                                         d.harmful_n,
                                         d.updated_ts_ms,
                                         pref_id);
    }

    double sum_strength = 0.0;
    std::size_t active_n = 0;
    for (const auto& kv : pref_by_param) {
        const double s = kv.second.strength01;
        if (s < 0.05) continue;
        sum_strength += s;
        ++active_n;
    }

    out.active_n = active_n;
    out.rigidity01 = static_cast<float>(softplus01(sum_strength));
    if (active_n == 0 || sum_strength <= 0.0) return out;

    const double budget = 0.75;
    double destabilization_mass = 0.0;

    for (auto& d : deltas_io) {
        const std::string& pname = d.first;
        const auto pit = pref_by_param.find(pname);
        if (pit == pref_by_param.end()) continue;

        const double weight = (pit->second.strength01 / sum_strength) * budget;
        const double preferred = pit->second.preferred;
        const auto cit = current_params.find(pname);
        const double current = (cit != current_params.end()) ? cit->second : 0.0;

        const double before = std::fabs(current - preferred);
        const double after = std::fabs((current + d.second) - preferred);
        if (after <= before) continue;

        const double inc = after - before;
        destabilization_mass += weight * inc;

        double scale = 1.0 / (1.0 + weight * inc * 10.0);
        scale = std::clamp(scale, 0.2, 1.0);
        d.second *= scale;
    }

    out.destabilization01 = static_cast<float>(softplus01(destabilization_mass));
    return out;
}

float StageC_AutonomyGate::computeRevisionReputation(std::int64_t run_id, std::size_t window_size, std::size_t& out_window_n) const {
    out_window_n = 0;
    if (!db_ || run_id <= 0 || window_size == 0) return 0.5f;

    const auto outcomes = db_->getRecentSelfRevisionOutcomes(run_id, window_size);
    out_window_n = outcomes.size();
    if (outcomes.empty()) return 0.5f;

    float sum = 0.0f;
    for (const auto& o : outcomes) {
        if (o.outcome_class == "Beneficial") sum += 1.0f;
        else if (o.outcome_class == "Harmful") sum -= 1.0f;
        else sum += 0.0f;
    }

    const float mean = sum / static_cast<float>(outcomes.size());
    return std::clamp(0.5f + 0.5f * mean, 0.0f, 1.0f);
}

float StageC_AutonomyGate::mapReputationToCap(float reputation) const {
    if (reputation < 0.4f) return 0.5f;
    if (reputation < 0.6f) return 0.75f;
    return 1.0f;
}

float StageC_AutonomyGate::mapRiskUpperBoundToCap(float p_harm_ub95) const {
    if (p_harm_ub95 >= 0.55f) return 0.5f;
    if (p_harm_ub95 >= 0.35f) return 0.75f;
    return 1.0f;
}

float StageC_AutonomyGate::mapCreditToCap(float credit) const {
    if (credit < 0.4f) return 0.5f;
    if (credit < 0.6f) return 0.75f;
    return 1.0f;
}

std::optional<double> StageC_AutonomyGate::updateAutonomyCreditV2(std::int64_t run_id,
                                                                  std::int64_t ts_ms,
                                                                  std::size_t window_size,
                                                                  double decay_rate) const {
    if (!db_ || run_id <= 0 || ts_ms <= 0 || window_size == 0) return std::nullopt;

    const auto prev = db_->getLatestAutonomyCredit(run_id);
    double prev_credit = prev.has_value() ? prev->credit_value : 0.5;
    std::int64_t prev_ts = prev.has_value() ? prev->ts_ms : ts_ms;

    if (decay_rate < 0.0) decay_rate = 0.0;
    if (decay_rate > 1.0) decay_rate = 1.0;

    const std::int64_t dt_ms = (ts_ms >= prev_ts) ? (ts_ms - prev_ts) : 0;
    const double dt_days = static_cast<double>(dt_ms) / (24.0 * 60.0 * 60.0 * 1000.0);
    const double decay_factor = (dt_days > 0.0) ? std::pow(decay_rate, dt_days) : 1.0;
    const double decayed = clamp01d(prev_credit * decay_factor);

    double consistency = 0.5;
    const auto consist = db_->getRecentConsistency(run_id, 1);
    if (!consist.empty()) {
        consistency = clamp01d(consist.front().consistency_score);
    }

    bool ethics_hard_block = false;
    const auto ethics = db_->getRecentEthicsRegulator(run_id, 1);
    if (!ethics.empty()) {
        ethics_hard_block = (ethics.front().decision == "deny");
    }

    const auto outcomes = db_->getRecentSelfRevisionOutcomes(run_id, window_size);
    std::size_t beneficial_n = 0;
    std::size_t harmful_n = 0;
    for (const auto& o : outcomes) {
        if (o.outcome_class == "Beneficial") ++beneficial_n;
        else if (o.outcome_class == "Harmful") ++harmful_n;
    }
    const std::size_t window_n = outcomes.size();
    const std::size_t decisive_n = beneficial_n + harmful_n;
    const double quality01 = (decisive_n == 0)
        ? 0.5
        : clamp01d(0.5 + 0.5 * (static_cast<double>(beneficial_n) - static_cast<double>(harmful_n)) / static_cast<double>(decisive_n));

    const bool increase_ok = (!ethics_hard_block) &&
                             (window_n >= 5) &&
                             (beneficial_n >= 3) &&
                             (harmful_n == 0) &&
                             (quality01 >= 0.65) &&
                             (consistency >= 0.80);

    double updated = decayed;
    std::string reason = "decay_only";
    if (ethics_hard_block) {
        updated = clamp01d(decayed - 0.25);
        reason = "ethics_hard_block_penalty";
    } else if (increase_ok) {
        const double gain = 0.02 * (quality01 - 0.65) + 0.02 * (consistency - 0.80);
        updated = clamp01d(decayed + std::max(0.0, gain));
        reason = "earned_trust_gain";
    } else if (harmful_n > beneficial_n && harmful_n >= 2) {
        updated = clamp01d(decayed - 0.05);
        reason = "harmful_outcomes_penalty";
    }

    const bool should_log = (!prev.has_value()) || (dt_ms >= 60'000) || (std::fabs(updated - prev_credit) >= 1e-4);
    if (!should_log) return updated;

    std::ostringstream drv;
    drv.setf(std::ios::fixed);
    drv << "{"
        << "\"reason\":\"" << reason << "\""
        << ",\"prev_credit\":" << prev_credit
        << ",\"decayed_credit\":" << decayed
        << ",\"updated_credit\":" << updated
        << ",\"dt_ms\":" << dt_ms
        << ",\"decay_rate\":" << decay_rate
        << ",\"consistency\":" << consistency
        << ",\"ethics_hard_block\":" << (ethics_hard_block ? "true" : "false")
        << ",\"window_n\":" << window_n
        << ",\"beneficial_n\":" << beneficial_n
        << ",\"harmful_n\":" << harmful_n
        << ",\"quality01\":" << quality01
        << "}";

    std::int64_t out_id = 0;
    (void)db_->insertAutonomyCredit(run_id, ts_ms, updated, decay_rate, drv.str(), out_id);
    return updated;
}

} // namespace Core
} // namespace NeuroForge
