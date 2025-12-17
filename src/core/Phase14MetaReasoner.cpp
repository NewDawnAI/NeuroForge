#include "core/Phase14MetaReasoner.h"
#include "core/MemoryDB.h"
#include <chrono>
#include <sstream>
#include <algorithm>

namespace NeuroForge {
namespace Core {

static inline std::int64_t now_ms_phase14() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::string Phase14MetaReasoner::runForLatest(const std::string& context) {
    if (!db_ || run_id_ <= 0) return std::string();

    // Pull recent metacognition entries
    auto recent = db_->getRecentMetacognition(run_id_, cfg_.window);
    if (recent.empty()) {
        std::int64_t out_id = 0;
        std::ostringstream js;
        js << "{\"context\":\"" << context << "\",\"samples\":0}";
        db_->insertMetaReason(run_id_, now_ms_phase14(), "ok", js.str(), out_id);
        return "ok";
    }

    // Aggregate simple metrics
    double avg_trust = 0.0;
    double avg_rmse = 0.0;
    int rmse_count = 0;
    for (const auto& m : recent) {
        avg_trust += m.self_trust;
        if (m.narrative_rmse.has_value()) { avg_rmse += *m.narrative_rmse; rmse_count++; }
    }
    avg_trust /= static_cast<double>(recent.size());
    if (rmse_count > 0) avg_rmse /= static_cast<double>(rmse_count);

    // Verdict logic: simple thresholds on trust and RMSE
    std::string verdict = "ok";
    if (avg_trust < cfg_.trust_degraded_threshold || (rmse_count > 0 && avg_rmse > cfg_.rmse_degraded_threshold)) {
        verdict = "degraded";
    }

    // Reasoning JSON
    std::ostringstream js;
    js << "{\"context\":\"" << context << "\"";
    js << ",\"samples\":" << recent.size();
    js << ",\"avg_trust\":" << avg_trust;
    if (rmse_count > 0) {
        js << ",\"avg_rmse\":" << avg_rmse;
        js << ",\"rmse_samples\":" << rmse_count;
    }
    js << ",\"thresholds\":{\"trust\":" << cfg_.trust_degraded_threshold
       << ",\"rmse\":" << cfg_.rmse_degraded_threshold << "}}";

    std::int64_t out_id = 0;
    db_->insertMetaReason(run_id_, now_ms_phase14(), verdict, js.str(), out_id);
    return verdict;
}

} // namespace Core
} // namespace NeuroForge

