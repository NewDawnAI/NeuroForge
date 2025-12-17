#pragma once
#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Core {

class MemoryDB;

// Phase 14: Meta-Reasoner — evaluates system health from recent metacognition
class Phase14MetaReasoner {
public:
    struct Config {
        int window{10};
        double trust_degraded_threshold{0.35};
        double rmse_degraded_threshold{0.60};
    };

    Phase14MetaReasoner(MemoryDB* db, std::int64_t run_id, const Config& cfg)
        : db_(db), run_id_(run_id), cfg_(cfg) {}

    void setConfig(const Config& cfg) { cfg_ = cfg; }
    const Config& getConfig() const noexcept { return cfg_; }

    // Analyze recent metacognition entries and log a meta-reason verdict
    // Returns the verdict string logged
    std::string runForLatest(const std::string& context = "");

private:
    MemoryDB* db_{nullptr};
    std::int64_t run_id_{0};
    Config cfg_{};
};

} // namespace Core
} // namespace NeuroForge
