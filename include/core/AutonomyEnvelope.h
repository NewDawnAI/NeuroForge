#pragma once

#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Core {

class MemoryDB;

enum class AutonomyTier : std::uint8_t {
    NONE = 0,
    SHADOW = 1,
    CONDITIONAL = 2,
    FULL = 3
};

struct AutonomyInputs {
    double identity_confidence = 0.5;
    double self_trust = 0.5;
    double ethics_score = 0.5;
    bool ethics_hard_block = false;
    double social_alignment = 0.5;
    double reputation = 0.5;
};

struct AutonomyEnvelope {
    double autonomy_score = 0.0;
    AutonomyTier tier = AutonomyTier::NONE;
    double self_component = 0.0;
    double ethics_component = 0.0;
    double social_component = 0.0;
    double autonomy_cap_multiplier = 1.0;
    bool allow_action = false;
    bool allow_goal_commit = false;
    bool allow_self_revision = false;
    std::int64_t ts_ms = 0;
    std::uint64_t step = 0;
    std::string rationale;
    bool valid = false;

    double getBaseAutonomy() const { return autonomy_score; }
    bool applyAutonomyCap(double multiplier);
    double getEffectiveAutonomy() const;
};

AutonomyEnvelope ComputeAutonomyEnvelope(const AutonomyInputs& inputs,
                                         std::int64_t ts_ms,
                                         std::uint64_t step,
                                         const std::string& context = "");

bool LogAutonomyEnvelope(MemoryDB* db,
                         std::int64_t run_id,
                         const AutonomyEnvelope& envelope,
                         const AutonomyInputs& inputs,
                         const std::string& context = "");

} // namespace Core
} // namespace NeuroForge

