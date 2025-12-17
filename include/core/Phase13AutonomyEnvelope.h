#pragma once
#include <cstdint>
#include <string>
#include <optional>

namespace NeuroForge {
namespace Core {

class MemoryDB;

// Phase 13: Autonomy Envelope controller
class Phase13AutonomyEnvelope {
public:
    struct Config {
        double trust_tighten_threshold{0.35};
        double trust_expand_threshold{0.70};
        double consistency_tighten_threshold{0.50};
        double consistency_expand_threshold{0.80};
        std::int64_t contraction_hysteresis_ms{60'000};
        std::int64_t expansion_hysteresis_ms{60'000};
        std::int64_t min_log_interval_ms{30'000};
        int analysis_window{10};
    };

    Phase13AutonomyEnvelope(MemoryDB* db, std::int64_t run_id, const Config& cfg)
        : db_(db), run_id_(run_id), cfg_(cfg) {}

    void setConfig(const Config& cfg) { cfg_ = cfg; }
    const Config& getConfig() const noexcept { return cfg_; }

    // Main entry: analyze recent metrics and decide envelope adjustment
    // Returns the decision string logged (or empty if no change logged)
    std::string maybeAdjustEnvelope(const std::string& context = "");

private:
    MemoryDB* db_{nullptr};
    std::int64_t run_id_{0};
    Config cfg_{};

    std::string last_decision_{}; // "tighten", "normal", "expand", "freeze"
    std::int64_t last_change_ms_{0};
    std::int64_t last_log_ms_{0};
};

} // namespace Core
} // namespace NeuroForge
