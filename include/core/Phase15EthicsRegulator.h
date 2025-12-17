#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <unordered_map>
#include "core/MemoryDB.h"

namespace NeuroForge {
namespace Core {

class AutonomyEnvelope;

// Phase 15: Ethics Regulator
// Monitors recent metacognition and autonomy signals to emit ethics decisions
// (e.g., allow, review, deny) with rationale for auditability.
class Phase15EthicsRegulator {
public:
    struct Config {
        int window = 10;              // analysis window (recent entries)
        double risk_threshold = 0.50; // simple risk threshold (0..1)
    };

    struct PersonalityApprovalResult {
        std::string decision;
        bool approved{false};
    };

    Phase15EthicsRegulator(MemoryDB* db, std::int64_t run_id, const Config& cfg)
        : db_(db), run_id_(run_id), cfg_(cfg) {}

    // Runs an ethics check for the latest context and logs a decision.
    // Returns the decision string ("allow", "review", or "deny").
    std::string runForLatest(const std::string& context);

    PersonalityApprovalResult reviewPersonalityProposal(std::int64_t personality_id,
                                                        const std::string& context,
                                                        const std::string& rationale);

    void setAutonomyEnvelope(const AutonomyEnvelope* env) { autonomy_env_ = env; }

private:
    MemoryDB* db_{nullptr};
    std::int64_t run_id_{0};
    Config cfg_{};
    const AutonomyEnvelope* autonomy_env_{nullptr};
    std::unordered_map<std::string, int> last_decision_context_;
};

} // namespace Core
} // namespace NeuroForge
