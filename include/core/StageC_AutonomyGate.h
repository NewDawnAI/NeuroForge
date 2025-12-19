#pragma once

#include <cstdint>
#include <cstddef>

namespace NeuroForge {
namespace Core {

class MemoryDB;
struct AutonomyEnvelope;

class StageC_AutonomyGate {
public:
    struct Result {
        float revision_reputation{0.5f};
        float autonomy_cap_multiplier{1.0f};
        std::size_t window_n{0};
        bool applied{false};
    };

    explicit StageC_AutonomyGate(MemoryDB* db) : db_(db) {}

    Result evaluateAndApply(AutonomyEnvelope& envelope, std::int64_t run_id, std::size_t window_size = 20);

private:
    float computeRevisionReputation(std::int64_t run_id, std::size_t window_size, std::size_t& out_window_n) const;
    float mapReputationToCap(float reputation) const;

    MemoryDB* db_{nullptr};
};

} // namespace Core
} // namespace NeuroForge
