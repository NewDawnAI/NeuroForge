#include "core/Phase15EthicsRegulator.h"
#include "core/MemoryDB.h"
#include "core/ContextHooks.h"
#include "core/AutonomyEnvelope.h"
#include <chrono>
#include <sstream>
#include <algorithm>

namespace NeuroForge {
namespace Core {

static inline std::int64_t now_ms_phase15() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Minimal initial policy:
// - Use a simple risk proxy from config; if >= 0.85 -> "deny"
// - If in [0.60, 0.85) -> "review"
// - Else -> "allow"
// Real implementations may aggregate Phase 9/13 signals; this skeleton focuses on
// structured logging and safe defaults.
std::string Phase15EthicsRegulator::runForLatest(const std::string& context) {
    if (!db_) return std::string("allow");

    // Sample the current context signal using provided label
    double sample = NeuroForge::Core::NF_SampleContext(context);
    auto cfg_ctx = NeuroForge::Core::NF_GetContextConfig();

    // Compute decision relative to configured risk threshold with a small margin
    const double margin = 0.05;
    double risk = std::clamp(sample, 0.0, 1.0);
    std::string decision = "allow";
    if (risk >= (cfg_.risk_threshold + margin)) decision = "deny";
    else if (risk >= (cfg_.risk_threshold - margin)) decision = "review";

    if (autonomy_env_ && autonomy_env_->valid) {
        last_decision_context_["autonomy_tier"] = static_cast<int>(autonomy_env_->tier);
    } else {
        last_decision_context_.erase("autonomy_tier");
    }

    std::ostringstream driver;
    driver << "{\"version\":1,\"phase\":15,\"window\":" << cfg_ctx.window
           << ",\"risk_threshold\":" << cfg_.risk_threshold
           << ",\"context\":{\"label\":\"" << context << "\",\"sample\":" << sample
           << ",\"config\":{\"gain\":" << cfg_ctx.gain << ",\"update_ms\":" << cfg_ctx.update_ms << ",\"window\":" << cfg_ctx.window << "}}";
    if (autonomy_env_ && autonomy_env_->valid) {
        driver << ",\"autonomy\":{\"tier\":" << static_cast<int>(autonomy_env_->tier)
               << ",\"score\":" << autonomy_env_->autonomy_score << "}";
    }
    driver << "}";

    std::int64_t ts = now_ms_phase15();
    // Insert a context log entry
    std::int64_t out_context_id = 0;
    (void)db_->insertContextLog(run_id_, ts, sample, cfg_ctx.gain, cfg_ctx.update_ms, cfg_ctx.window, context, out_context_id);
    // Insert ethics regulator decision
    std::int64_t out_id = 0;
    (void)db_->insertEthicsRegulator(run_id_, ts, decision, driver.str(), out_id);
    return decision;
}

Phase15EthicsRegulator::PersonalityApprovalResult Phase15EthicsRegulator::reviewPersonalityProposal(
    std::int64_t personality_id,
    const std::string& context,
    const std::string& rationale) {
    PersonalityApprovalResult result;
    result.decision = runForLatest(context);
    result.approved = false;

    if (!db_) {
        return result;
    }

    if (result.decision != "allow") {
        return result;
    }

    std::ostringstream approval_notes;
    approval_notes << "Phase15 decision=" << result.decision << " context=" << context;
    if (!rationale.empty()) {
        approval_notes << " rationale=" << rationale;
    }

    bool ok = db_->approvePersonalityProposal(personality_id, "phase15", approval_notes.str());
    if (ok) {
        result.approved = true;
    }
    return result;
}

} // namespace Core
} // namespace NeuroForge
