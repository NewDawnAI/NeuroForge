#include "core/Phase13AutonomyEnvelope.h"
#include "core/MemoryDB.h"
#include <chrono>
#include <sstream>
#include <algorithm>

namespace NeuroForge {
namespace Core {

static inline std::int64_t now_ms_p13() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::string Phase13AutonomyEnvelope::maybeAdjustEnvelope(const std::string& context) {
    if (!db_) return {};

    // Pull recent metacognition and consistency windows
    const int N = std::max(1, cfg_.analysis_window);
    auto metacog = db_->getRecentMetacognition(run_id_, N);
    auto consist = db_->getRecentConsistency(run_id_, N);

    if (metacog.empty()) {
        // Without trust signal, do not change envelope; but allow periodic logging of "normal"
        const std::int64_t now = now_ms_p13();
        if (now - last_log_ms_ < cfg_.min_log_interval_ms) return {};
        std::int64_t out_id = 0;
        std::ostringstream drv;
        drv << "{\"context\":\"" << context << "\",\"reason\":\"no_metacognition\"}";
        db_->insertAutonomyDecision(run_id_, now, "normal", drv.str(), out_id);
        last_log_ms_ = now;
        last_decision_ = "normal";
        return last_decision_;
    }

    const double trust = metacog.front().self_trust;
    double trust_prev = trust;
    if (metacog.size() >= 2) trust_prev = metacog[1].self_trust;
    const double trust_slope = trust - trust_prev;

    double consistency_score = 1.0; // default optimistic if missing
    if (!consist.empty()) {
        consistency_score = consist.front().consistency_score;
    }

    // Decide desired envelope state
    std::string decision = "normal";
    const bool tighten_cond = (trust < cfg_.trust_tighten_threshold) || (consistency_score < cfg_.consistency_tighten_threshold);
    const bool expand_cond = (trust > cfg_.trust_expand_threshold) && (consistency_score > cfg_.consistency_expand_threshold);

    const std::int64_t now = now_ms_p13();
    const bool can_contract = (now - last_change_ms_) >= cfg_.contraction_hysteresis_ms;
    const bool can_expand = (now - last_change_ms_) >= cfg_.expansion_hysteresis_ms;

    if (tighten_cond && can_contract) {
        decision = "tighten";
    } else if (expand_cond && can_expand) {
        decision = "expand";
    } else {
        decision = "normal";
    }

    // Only log when decision changes or min interval elapsed
    if (decision == last_decision_ && (now - last_log_ms_) < cfg_.min_log_interval_ms) {
        return {};
    }

    std::int64_t out_id = 0;
    std::ostringstream drv;
    drv << "{\"trust\":" << trust
        << ",\"trust_slope\":" << trust_slope
        << ",\"consistency\":" << consistency_score
        << ",\"tighten_thresholds\":{\"trust\":" << cfg_.trust_tighten_threshold
        << ",\"consistency\":" << cfg_.consistency_tighten_threshold << "}"
        << ",\"expand_thresholds\":{\"trust\":" << cfg_.trust_expand_threshold
        << ",\"consistency\":" << cfg_.consistency_expand_threshold << "}"
        << ",\"context\":\"" << context << "\"}";

    db_->insertAutonomyDecision(run_id_, now, decision, drv.str(), out_id);
    last_log_ms_ = now;
    if (decision != last_decision_) {
        last_change_ms_ = now;
        last_decision_ = decision;
    }
    return decision;
}

} // namespace Core
} // namespace NeuroForge

