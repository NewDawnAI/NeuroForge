#pragma once

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <map>

namespace NeuroForge {
namespace Core {

class MemoryDB;
struct AutonomyEnvelope;

class StageC_AutonomyGate {
public:
    struct Result {
        int version{1};
        float revision_reputation{0.5f};
        float autonomy_credit{0.0f};
        float autonomy_credit_cap_multiplier{1.0f};
        float harm_risk_cap_multiplier{1.0f};
        float autonomy_cap_multiplier{1.0f};
        std::size_t window_n{0};
        bool applied{false};
        float p_harm_mean{0.0f};
        float p_harm_ub95{0.0f};
        std::size_t beneficial_n{0};
        std::size_t harmful_n{0};
        std::size_t neutral_n{0};
        float preference_rigidity01{0.0f};
        float preference_destabilization01{0.0f};
        std::size_t preference_active_n{0};
        std::size_t goal_candidate_n{0};
        std::size_t goal_created_n{0};
        std::size_t goal_reaffirmed_n{0};
        bool goal_governance_veto{false};
        std::int64_t goal_ttl_ms{0};
    };

    struct PreferenceStabilization {
        float rigidity01{0.0f};
        float destabilization01{0.0f};
        std::size_t active_n{0};
    };

    explicit StageC_AutonomyGate(MemoryDB* db) : db_(db) {}

    Result evaluateV1(std::int64_t run_id, std::size_t window_size = 20) const;
    Result evaluateV2(std::int64_t run_id, std::size_t window_size = 200) const;
    Result evaluateV3(std::int64_t run_id, std::size_t window_size = 200) const;
    Result evaluateV4(std::int64_t run_id, std::size_t window_size = 200) const;

    Result evaluateAndApply(AutonomyEnvelope& envelope, std::int64_t run_id, std::size_t window_size = 20);
    Result evaluateAndApplyV2(AutonomyEnvelope& envelope, std::int64_t run_id, std::size_t window_size = 200);
    Result evaluateAndApplyV3(AutonomyEnvelope& envelope, std::int64_t run_id, std::size_t window_size = 200);
    Result evaluateAndApplyV4(AutonomyEnvelope& envelope, std::int64_t run_id, std::size_t window_size = 200);

    std::optional<double> updateAutonomyCreditV2(std::int64_t run_id,
                                                 std::int64_t ts_ms,
                                                 std::size_t window_size = 200,
                                                 double decay_rate = 0.99) const;

    PreferenceStabilization stabilizePreferenceDeltasV3(std::int64_t run_id,
                                                        const std::map<std::string, double>& current_params,
                                                        std::vector<std::pair<std::string, double>>& deltas_io,
                                                        std::size_t outcome_window_size = 200,
                                                        std::size_t param_window_size = 1000) const;

private:
    float computeRevisionReputation(std::int64_t run_id, std::size_t window_size, std::size_t& out_window_n) const;
    float mapReputationToCap(float reputation) const;
    float mapRiskUpperBoundToCap(float p_harm_ub95) const;
    float mapCreditToCap(float credit) const;

    MemoryDB* db_{nullptr};
};

} // namespace Core
} // namespace NeuroForge
