#include "core/Phase12Consistency.h"
#include <algorithm>
#include <numeric>
#include <sstream>
#include <chrono>

namespace NeuroForge {
namespace Core {

static inline std::int64_t now_ms_p12() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

static inline double clamp01(double x) { return std::max(0.0, std::min(1.0, x)); }

bool Phase12Consistency::runForLatest(const std::string& context) {
    if (!db_) return false;
    auto window = db_->getRecentMetacognition(run_id_, analysis_window_);
    if (window.empty()) return false;

    std::string window_json, driver_expl;
    double score = computeConsistencyScore(window, window_json, driver_expl, context);
    std::int64_t out_id = 0;
    std::ostringstream notes;
    notes << "window=" << window.size();
    return db_->insertSelfConsistency(run_id_, now_ms_p12(), score, notes.str(), window_json, driver_expl, out_id);
}

double Phase12Consistency::computeConsistencyScore(const std::vector<MemoryDB::MetacognitionEntry>& entries,
                                                   std::string& out_window_json,
                                                   std::string& out_driver_expl,
                                                   const std::string& context) {
    // Collect deltas where present
    std::vector<double> trust_deltas; trust_deltas.reserve(entries.size());
    std::vector<double> coh_deltas; coh_deltas.reserve(entries.size());
    std::vector<double> goal_deltas; goal_deltas.reserve(entries.size());
    for (const auto& e : entries) {
        if (e.trust_delta.has_value()) trust_deltas.push_back(*e.trust_delta);
        if (e.coherence_delta.has_value()) coh_deltas.push_back(*e.coherence_delta);
        if (e.goal_accuracy_delta.has_value()) goal_deltas.push_back(*e.goal_accuracy_delta);
    }

    auto mean = [](const std::vector<double>& v) {
        if (v.empty()) return 0.0; return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
    };
    auto stddev = [&](const std::vector<double>& v) {
        if (v.size() <= 1) return 0.0; double m = mean(v); double acc = 0.0; for (double x : v) { double d = x - m; acc += d*d; } return std::sqrt(acc / static_cast<double>(v.size() - 1));
    };

    double trust_std = stddev(trust_deltas);
    double trust_stability = clamp01(1.0 - (trust_std / 0.10)); // std <= 0.10 -> ~1.0

    double coh_mean = mean(coh_deltas); // positive = error decreasing
    double goal_mean = mean(goal_deltas); // positive = MAE decreasing
    double coh_improve = clamp01(0.5 + coh_mean); // shift to [0,1] assuming mean in ~[-0.5,0.5]
    double goal_improve = clamp01(0.5 + goal_mean);

    double score = trust_stability_w_ * trust_stability +
                   coherence_trend_w_ * coh_improve +
                   goal_trend_w_ * goal_improve;
    score = clamp01(score);

    // Serialize window summary
    std::ostringstream w;
    w << "{";
    w << "\"size\":" << entries.size() << ",";
    w << "\"trust_std\":" << trust_std << ",";
    w << "\"coh_mean_delta\":" << coh_mean << ",";
    w << "\"goal_mean_delta\":" << goal_mean;
    w << "}";
    out_window_json = w.str();

    std::ostringstream expl;
    expl << "Phase12Consistency: score=" << score
         << ", trust_stability=" << trust_stability
         << ", coh_improve=" << coh_improve
         << ", goal_improve=" << goal_improve;
    if (!context.empty()) expl << ", context=\"" << context << "\"";
    out_driver_expl = expl.str();

    return score;
}

} // namespace Core
} // namespace NeuroForge

