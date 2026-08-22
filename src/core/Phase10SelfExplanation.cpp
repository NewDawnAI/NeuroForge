#include "core/Phase10SelfExplanation.h"
#include "core/MemoryDB.h"
#include "core/StageC_AutonomyGate.h"
#include <algorithm>
#include <sstream>
#include <chrono>

namespace NeuroForge {
namespace Core {

static inline std::int64_t now_ms_phase10() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

static inline float compute_revision_reputation(const std::vector<MemoryDB::SelfRevisionOutcomeEntry>& outcomes) {
    if (outcomes.empty()) return 0.5f;
    float sum = 0.0f;
    for (const auto& o : outcomes) {
        if (o.outcome_class == "Beneficial") sum += 1.0f;
        else if (o.outcome_class == "Harmful") sum -= 1.0f;
    }
    const float mean = sum / static_cast<float>(outcomes.size());
    return std::clamp(0.5f + 0.5f * mean, 0.0f, 1.0f);
}

static inline float map_reputation_to_cap(float reputation) {
    if (reputation < 0.4f) return 0.5f;
    if (reputation < 0.6f) return 0.75f;
    return 1.0f;
}

std::string Phase10SelfExplanation::synthesizeNarrative(double trust_delta,
                                                         double mean_abs_error,
                                                         double confidence_bias,
                                                         const std::string& context) {
    std::ostringstream oss;
    oss << "{\"summary\":\"";
    if (trust_delta > 0.0) oss << "Trust increased"; else if (trust_delta < 0.0) oss << "Trust decreased"; else oss << "Trust unchanged";
    oss << " due to recent prediction outcomes.\"";
    oss << ",\"drivers\":{"
        << "\"avg_abs_error\":" << mean_abs_error << ","
        << "\"confidence_bias\":" << confidence_bias
        << "},\"context\":\"" << context << "\"";
    oss << "}";
    return oss.str();
}

bool Phase10SelfExplanation::runForMetacogId(std::int64_t metacog_id, const std::string& context) {
    if (!db_ || metacog_id <= 0) return false;

    // Placeholder heuristics: use neutral deltas until richer inputs are wired.
    const double trust_delta = 0.0;
    const double mean_abs_error = 0.0;
    const double confidence_bias = 0.0;

    std::string narrative = synthesizeNarrative(trust_delta, mean_abs_error, confidence_bias, context);

    if (stage_c_enabled_) {
        if (stage_c_version_ == 5) {
            StageC_AutonomyGate gate(db_);
            const auto r = gate.evaluateV4(run_id_, 200);
            std::string note;
            if (r.window_n == 0) {
                note = "No outcome history; autonomy cap unchanged";
            } else {
                const bool risk_constrained = (r.harm_risk_cap_multiplier < 1.0f);
                const bool credit_constrained = (r.autonomy_credit_cap_multiplier < 1.0f);
                if (risk_constrained && credit_constrained) {
                    note = "Autonomy constrained by both harm-risk and autonomy credit";
                } else if (risk_constrained) {
                    note = "Autonomy constrained by harm-risk upper bound from self-revision outcomes";
                } else if (credit_constrained) {
                    note = "Autonomy constrained by autonomy credit";
                } else {
                    note = "Autonomy unconstrained by harm-risk and autonomy credit";
                }
            }

            if (r.goal_governance_veto) {
                note += "; goals vetoed by governance";
            } else if (r.goal_candidate_n > 0) {
                note += "; goals eligible for formation";
            } else {
                note += "; no goal candidates";
            }

            std::ostringstream oss;
            oss.setf(std::ios::fixed);
            oss << ",\"stage_c_v5\":{";
            oss << "\"p_harm_mean\":" << r.p_harm_mean << ",";
            oss << "\"p_harm_ub95\":" << r.p_harm_ub95 << ",";
            oss << "\"harm_risk_cap_multiplier\":" << r.harm_risk_cap_multiplier << ",";
            oss << "\"autonomy_credit\":" << r.autonomy_credit << ",";
            oss << "\"autonomy_credit_cap_multiplier\":" << r.autonomy_credit_cap_multiplier << ",";
            oss << "\"autonomy_cap_multiplier\":" << r.autonomy_cap_multiplier << ",";
            oss << "\"window_n\":" << r.window_n << ",";
            oss << "\"beneficial_n\":" << r.beneficial_n << ",";
            oss << "\"harmful_n\":" << r.harmful_n << ",";
            oss << "\"neutral_n\":" << r.neutral_n << ",";
            oss << "\"preference_rigidity01\":" << r.preference_rigidity01 << ",";
            oss << "\"preference_destabilization01\":" << r.preference_destabilization01 << ",";
            oss << "\"preference_active_n\":" << r.preference_active_n << ",";
            oss << "\"goal_candidate_n\":" << r.goal_candidate_n << ",";
            oss << "\"goal_created_n\":" << r.goal_created_n << ",";
            oss << "\"goal_reaffirmed_n\":" << r.goal_reaffirmed_n << ",";
            oss << "\"goal_governance_veto\":" << (r.goal_governance_veto ? "true" : "false") << ",";
            oss << "\"goal_ttl_ms\":" << r.goal_ttl_ms << ",";
            oss << "\"note\":\"" << note << "\"";
            oss << "}";
            const std::string injection = oss.str();

            const std::size_t last_brace = narrative.rfind('}');
            if (last_brace != std::string::npos) {
                narrative.insert(last_brace, injection);
            }
        } else if (stage_c_version_ == 4) {
            StageC_AutonomyGate gate(db_);
            const auto r = gate.evaluateV4(run_id_, 200);
            std::string note;
            if (r.window_n == 0) {
                note = "No outcome history; autonomy cap unchanged";
            } else {
                const bool risk_constrained = (r.harm_risk_cap_multiplier < 1.0f);
                const bool credit_constrained = (r.autonomy_credit_cap_multiplier < 1.0f);
                if (risk_constrained && credit_constrained) {
                    note = "Autonomy constrained by both harm-risk and autonomy credit";
                } else if (risk_constrained) {
                    note = "Autonomy constrained by harm-risk upper bound from self-revision outcomes";
                } else if (credit_constrained) {
                    note = "Autonomy constrained by autonomy credit";
                } else {
                    note = "Autonomy unconstrained by harm-risk and autonomy credit";
                }
            }

            if (r.goal_governance_veto) {
                note += "; goals vetoed by governance";
            } else if (r.goal_candidate_n > 0) {
                note += "; goals eligible for formation";
            } else {
                note += "; no goal candidates";
            }

            std::ostringstream oss;
            oss.setf(std::ios::fixed);
            oss << ",\"stage_c_v4\":{";
            oss << "\"p_harm_mean\":" << r.p_harm_mean << ",";
            oss << "\"p_harm_ub95\":" << r.p_harm_ub95 << ",";
            oss << "\"harm_risk_cap_multiplier\":" << r.harm_risk_cap_multiplier << ",";
            oss << "\"autonomy_credit\":" << r.autonomy_credit << ",";
            oss << "\"autonomy_credit_cap_multiplier\":" << r.autonomy_credit_cap_multiplier << ",";
            oss << "\"autonomy_cap_multiplier\":" << r.autonomy_cap_multiplier << ",";
            oss << "\"window_n\":" << r.window_n << ",";
            oss << "\"beneficial_n\":" << r.beneficial_n << ",";
            oss << "\"harmful_n\":" << r.harmful_n << ",";
            oss << "\"neutral_n\":" << r.neutral_n << ",";
            oss << "\"preference_rigidity01\":" << r.preference_rigidity01 << ",";
            oss << "\"preference_destabilization01\":" << r.preference_destabilization01 << ",";
            oss << "\"preference_active_n\":" << r.preference_active_n << ",";
            oss << "\"goal_candidate_n\":" << r.goal_candidate_n << ",";
            oss << "\"goal_created_n\":" << r.goal_created_n << ",";
            oss << "\"goal_reaffirmed_n\":" << r.goal_reaffirmed_n << ",";
            oss << "\"goal_governance_veto\":" << (r.goal_governance_veto ? "true" : "false") << ",";
            oss << "\"goal_ttl_ms\":" << r.goal_ttl_ms << ",";
            oss << "\"note\":\"" << note << "\"";
            oss << "}";
            const std::string injection = oss.str();

            const std::size_t last_brace = narrative.rfind('}');
            if (last_brace != std::string::npos) {
                narrative.insert(last_brace, injection);
            }
        } else if (stage_c_version_ == 3) {
            StageC_AutonomyGate gate(db_);
            const auto r = gate.evaluateV3(run_id_, 200);
            std::string note;
            if (r.window_n == 0) {
                note = "No outcome history; autonomy cap unchanged";
            } else {
                const bool risk_constrained = (r.harm_risk_cap_multiplier < 1.0f);
                const bool credit_constrained = (r.autonomy_credit_cap_multiplier < 1.0f);
                if (risk_constrained && credit_constrained) {
                    note = "Autonomy constrained by both harm-risk and autonomy credit";
                } else if (risk_constrained) {
                    note = "Autonomy constrained by harm-risk upper bound from self-revision outcomes";
                } else if (credit_constrained) {
                    note = "Autonomy constrained by autonomy credit";
                } else {
                    note = "Autonomy unconstrained by harm-risk and autonomy credit";
                }
            }

            std::ostringstream oss;
            oss.setf(std::ios::fixed);
            oss << ",\"stage_c_v3\":{";
            oss << "\"p_harm_mean\":" << r.p_harm_mean << ",";
            oss << "\"p_harm_ub95\":" << r.p_harm_ub95 << ",";
            oss << "\"harm_risk_cap_multiplier\":" << r.harm_risk_cap_multiplier << ",";
            oss << "\"autonomy_credit\":" << r.autonomy_credit << ",";
            oss << "\"autonomy_credit_cap_multiplier\":" << r.autonomy_credit_cap_multiplier << ",";
            oss << "\"autonomy_cap_multiplier\":" << r.autonomy_cap_multiplier << ",";
            oss << "\"window_n\":" << r.window_n << ",";
            oss << "\"beneficial_n\":" << r.beneficial_n << ",";
            oss << "\"harmful_n\":" << r.harmful_n << ",";
            oss << "\"neutral_n\":" << r.neutral_n << ",";
            oss << "\"preference_rigidity01\":" << r.preference_rigidity01 << ",";
            oss << "\"preference_destabilization01\":" << r.preference_destabilization01 << ",";
            oss << "\"preference_active_n\":" << r.preference_active_n << ",";
            oss << "\"note\":\"" << note << "\"";
            oss << "}";
            const std::string injection = oss.str();

            const std::size_t last_brace = narrative.rfind('}');
            if (last_brace != std::string::npos) {
                narrative.insert(last_brace, injection);
            }
        } else if (stage_c_version_ == 2) {
            StageC_AutonomyGate gate(db_);
            const auto r = gate.evaluateV2(run_id_, 200);
            std::string note;
            if (r.window_n == 0) {
                note = "No outcome history; autonomy cap unchanged";
            } else {
                const bool risk_constrained = (r.harm_risk_cap_multiplier < 1.0f);
                const bool credit_constrained = (r.autonomy_credit_cap_multiplier < 1.0f);
                if (risk_constrained && credit_constrained) {
                    note = "Autonomy constrained by both harm-risk and autonomy credit";
                } else if (risk_constrained) {
                    note = "Autonomy constrained by harm-risk upper bound from self-revision outcomes";
                } else if (credit_constrained) {
                    note = "Autonomy constrained by autonomy credit";
                } else {
                    note = "Autonomy unconstrained by harm-risk and autonomy credit";
                }
            }

            std::ostringstream oss;
            oss.setf(std::ios::fixed);
            oss << ",\"stage_c_v2\":{";
            oss << "\"p_harm_mean\":" << r.p_harm_mean << ",";
            oss << "\"p_harm_ub95\":" << r.p_harm_ub95 << ",";
            oss << "\"harm_risk_cap_multiplier\":" << r.harm_risk_cap_multiplier << ",";
            oss << "\"autonomy_credit\":" << r.autonomy_credit << ",";
            oss << "\"autonomy_credit_cap_multiplier\":" << r.autonomy_credit_cap_multiplier << ",";
            oss << "\"autonomy_cap_multiplier\":" << r.autonomy_cap_multiplier << ",";
            oss << "\"window_n\":" << r.window_n << ",";
            oss << "\"beneficial_n\":" << r.beneficial_n << ",";
            oss << "\"harmful_n\":" << r.harmful_n << ",";
            oss << "\"neutral_n\":" << r.neutral_n << ",";
            oss << "\"note\":\"" << note << "\"";
            oss << "}";
            const std::string injection = oss.str();

            const std::size_t last_brace = narrative.rfind('}');
            if (last_brace != std::string::npos) {
                narrative.insert(last_brace, injection);
            }
        } else {
            const auto outcomes = db_->getRecentSelfRevisionOutcomes(run_id_, 20);
            const float rr = compute_revision_reputation(outcomes);
            const float cap = outcomes.empty() ? 1.0f : map_reputation_to_cap(rr);
            const char* note = outcomes.empty()
                ? "No outcome history; autonomy cap unchanged"
                : ((cap < 1.0f)
                    ? "Autonomy constrained due to recent neutral/harmful self-revision outcomes"
                    : "Autonomy unconstrained by recent self-revision outcomes");

            std::ostringstream oss;
            oss.setf(std::ios::fixed);
            oss << ",\"stage_c_v1\":{";
            oss << "\"revision_reputation\":" << rr << ",";
            oss << "\"autonomy_cap_multiplier\":" << cap << ",";
            oss << "\"window_n\":" << outcomes.size() << ",";
            oss << "\"note\":\"" << note << "\"";
            oss << "}";
            const std::string injection = oss.str();

            const std::size_t last_brace = narrative.rfind('}');
            if (last_brace != std::string::npos) {
                narrative.insert(last_brace, injection);
            }
        }
    }

    auto outcome = db_->getLatestSelfRevisionOutcome(run_id_);
    if (outcome.has_value()) {
        auto to_json_num = [](const std::optional<double>& v) -> std::string {
            if (!v.has_value()) return "null";
            std::ostringstream oss;
            oss << *v;
            return oss.str();
        };

        std::ostringstream oss;
        oss << ",\"stage7_5\":{";
        oss << "\"revision_id\":" << outcome->revision_id << ",";
        oss << "\"eval_ts_ms\":" << outcome->eval_ts_ms << ",";
        oss << "\"outcome_class\":\"" << outcome->outcome_class << "\",";
        oss << "\"trust_pre\":" << to_json_num(outcome->trust_pre) << ",";
        oss << "\"trust_post\":" << to_json_num(outcome->trust_post) << ",";
        oss << "\"prediction_error_pre\":" << to_json_num(outcome->prediction_error_pre) << ",";
        oss << "\"prediction_error_post\":" << to_json_num(outcome->prediction_error_post) << ",";
        oss << "\"coherence_pre\":" << to_json_num(outcome->coherence_pre) << ",";
        oss << "\"coherence_post\":" << to_json_num(outcome->coherence_post) << ",";
        oss << "\"reward_slope_pre\":" << to_json_num(outcome->reward_slope_pre) << ",";
        oss << "\"reward_slope_post\":" << to_json_num(outcome->reward_slope_post);
        oss << "}";
        const std::string injection = oss.str();

        const std::size_t last_brace = narrative.rfind('}');
        if (last_brace != std::string::npos) {
            narrative.insert(last_brace, injection);
        }
    }

    if (stage_c_enabled_ && stage_c_version_ == 5) {
        std::int64_t audit_id = 0;
        (void)db_->insertLanguageAuditLog(run_id_,
                                          now_ms_phase10(),
                                          0,
                                          2,
                                          "self_explanation",
                                          std::string(),
                                          narrative,
                                          false,
                                          false,
                                          false,
                                          false,
                                          true,
                                          audit_id);
    }

    return db_->updateMetacognitionExplanation(metacog_id, narrative);
}

bool Phase10SelfExplanation::runForLatest(const std::string& context) {
    if (!db_) return false;
    auto latest_id = db_->getLatestMetacognitionId(run_id_);
    if (!latest_id) return false;
    return runForMetacogId(*latest_id, context);
}

} // namespace Core
} // namespace NeuroForge
