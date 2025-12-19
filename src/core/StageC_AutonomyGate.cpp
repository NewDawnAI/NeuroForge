#include "core/StageC_AutonomyGate.h"
#include "core/AutonomyEnvelope.h"
#include "core/MemoryDB.h"
#include <algorithm>
#include <cassert>

namespace NeuroForge {
namespace Core {

StageC_AutonomyGate::Result StageC_AutonomyGate::evaluateAndApply(AutonomyEnvelope& envelope, std::int64_t run_id, std::size_t window_size) {
    Result out{};
    out.revision_reputation = computeRevisionReputation(run_id, window_size, out.window_n);
    out.autonomy_cap_multiplier = (out.window_n == 0) ? 1.0f : mapReputationToCap(out.revision_reputation);
#ifndef NDEBUG
    assert(out.autonomy_cap_multiplier <= 1.0f);
    assert(out.autonomy_cap_multiplier >= 0.5f);
#endif
    if (out.window_n == 0) {
        out.applied = false;
        return out;
    }
    out.applied = envelope.applyAutonomyCap(static_cast<double>(out.autonomy_cap_multiplier));
    return out;
}

float StageC_AutonomyGate::computeRevisionReputation(std::int64_t run_id, std::size_t window_size, std::size_t& out_window_n) const {
    out_window_n = 0;
    if (!db_ || run_id <= 0 || window_size == 0) return 0.5f;

    const auto outcomes = db_->getRecentSelfRevisionOutcomes(run_id, window_size);
    out_window_n = outcomes.size();
    if (outcomes.empty()) return 0.5f;

    float sum = 0.0f;
    for (const auto& o : outcomes) {
        if (o.outcome_class == "Beneficial") sum += 1.0f;
        else if (o.outcome_class == "Harmful") sum -= 1.0f;
        else sum += 0.0f;
    }

    const float mean = sum / static_cast<float>(outcomes.size());
    return std::clamp(0.5f + 0.5f * mean, 0.0f, 1.0f);
}

float StageC_AutonomyGate::mapReputationToCap(float reputation) const {
    if (reputation < 0.4f) return 0.5f;
    if (reputation < 0.6f) return 0.75f;
    return 1.0f;
}

} // namespace Core
} // namespace NeuroForge
