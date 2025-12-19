#pragma once
#include <cstdint>
#include <string>
#include <optional>

namespace NeuroForge {
namespace Core {

class MemoryDB;

// Phase 10: Self-Explanation
// Generates short, structured narratives explaining changes in self-trust
// based on recent prediction/resolution outcomes.
class Phase10SelfExplanation {
public:
    Phase10SelfExplanation(MemoryDB* db, std::int64_t run_id)
        : db_(db), run_id_(run_id) {}

    void setStageCEnabled(bool enabled) { stage_c_enabled_ = enabled; }

    // Generate and persist explanation for the latest metacognition row
    bool runForLatest(const std::string& context = "");

    // Optional: generate explanation for a given metacognition row id
    bool runForMetacogId(std::int64_t metacog_id, const std::string& context = "");

private:
    // Future: detect trust windows and compute attribution drivers
    struct TrustWindow { std::int64_t start_ms{0}; std::int64_t end_ms{0}; double delta{0.0}; };

    std::string synthesizeNarrative(double trust_delta,
                                    double mean_abs_error,
                                    double confidence_bias,
                                    const std::string& context);

    MemoryDB* db_ = nullptr;
    std::int64_t run_id_ = 0;
    bool stage_c_enabled_ = true;
};

} // namespace Core
} // namespace NeuroForge
