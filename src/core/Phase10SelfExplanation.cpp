#include "core/Phase10SelfExplanation.h"
#include "core/MemoryDB.h"
#include <sstream>
#include <chrono>

namespace NeuroForge {
namespace Core {

static inline std::int64_t now_ms_phase10() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

std::string Phase10SelfExplanation::synthesizeNarrative(double trust_delta,
                                                         double mean_abs_error,
                                                         double confidence_bias,
                                                         const std::string& context) {
    std::ostringstream oss;
    oss << "{\"summary\":\"";
    if (trust_delta > 0.0) oss << "Trust increased"; else if (trust_delta < 0.0) oss << "Trust decreased"; else oss << "Trust unchanged";
    oss << " due to recent prediction outcomes.\"";
    oss << ",\"drivers\":{"
        << "\"avg_abs_error\":" << mean_abs_error << ","
        << "\"confidence_bias\":" << confidence_bias
        << "},\"context\":\"" << context << "\"";
    oss << "}";
    return oss.str();
}

bool Phase10SelfExplanation::runForMetacogId(std::int64_t metacog_id, const std::string& context) {
    if (!db_ || metacog_id <= 0) return false;

    // Placeholder heuristics: use neutral deltas until richer inputs are wired.
    const double trust_delta = 0.0;
    const double mean_abs_error = 0.0;
    const double confidence_bias = 0.0;

    const std::string narrative = synthesizeNarrative(trust_delta, mean_abs_error, confidence_bias, context);
    return db_->updateMetacognitionExplanation(metacog_id, narrative);
}

bool Phase10SelfExplanation::runForLatest(const std::string& context) {
    if (!db_) return false;
    auto latest_id = db_->getLatestMetacognitionId(run_id_);
    if (!latest_id) return false;
    return runForMetacogId(*latest_id, context);
}

} // namespace Core
} // namespace NeuroForge
