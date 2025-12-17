#pragma once
#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include "core/MemoryDB.h"

namespace NeuroForge {
namespace Core {

class MemoryDB;

// Phase 12: Self-Consistency
// Aggregates recent metacognition signals to produce a stability/consistency score
// capturing trust stability and outcome trend coherence.
class Phase12Consistency {
public:
    Phase12Consistency(MemoryDB* db, std::int64_t run_id)
        : db_(db), run_id_(run_id) {}

    // Compute and persist consistency score for the latest window
    bool runForLatest(const std::string& context = "");

    // Configuration
    void setAnalysisWindow(int n) { analysis_window_ = (n > 1 ? n : analysis_window_); }
    void setWeights(double w_trust_stability, double w_coherence_trend, double w_goal_trend) {
        trust_stability_w_ = w_trust_stability; coherence_trend_w_ = w_coherence_trend; goal_trend_w_ = w_goal_trend;
    }

private:
    double computeConsistencyScore(const std::vector<MemoryDB::MetacognitionEntry>& entries,
                                   std::string& out_window_json,
                                   std::string& out_driver_expl,
                                   const std::string& context);

    MemoryDB* db_ = nullptr;
    std::int64_t run_id_ = 0;
    int analysis_window_ = 8; // default window size
    double trust_stability_w_ = 0.5;
    double coherence_trend_w_ = 0.25;
    double goal_trend_w_ = 0.25;
};

} // namespace Core
} // namespace NeuroForge

