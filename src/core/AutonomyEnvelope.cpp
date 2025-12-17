#include "core/AutonomyEnvelope.h"
#include "core/MemoryDB.h"
#include <algorithm>
#include <sstream>

namespace NeuroForge {
namespace Core {

static double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

static const char* TierToString(AutonomyTier tier) {
    switch (tier) {
        case AutonomyTier::NONE: return "NONE";
        case AutonomyTier::SHADOW: return "SHADOW";
        case AutonomyTier::CONDITIONAL: return "CONDITIONAL";
        case AutonomyTier::FULL: return "FULL";
        default: return "NONE";
    }
}

AutonomyEnvelope ComputeAutonomyEnvelope(const AutonomyInputs& inputs,
                                         std::int64_t ts_ms,
                                         std::uint64_t step,
                                         const std::string& context) {
    AutonomyEnvelope env{};

    double identity_conf = clamp01(inputs.identity_confidence);
    double self_trust = clamp01(inputs.self_trust);
    double ethics_score = clamp01(inputs.ethics_score);
    double social_alignment = clamp01(inputs.social_alignment);
    double reputation = clamp01(inputs.reputation);

    double self_component = 0.5 * identity_conf + 0.5 * self_trust;
    double ethics_component = ethics_score;
    double social_component = 0.7 * social_alignment + 0.3 * reputation;

    double w_self = 0.35;
    double w_ethics = 0.40;
    double w_social = 0.25;

    double autonomy_score = w_self * self_component +
                            w_ethics * ethics_component +
                            w_social * social_component;

    if (inputs.ethics_hard_block) {
        autonomy_score = 0.0;
    }

    autonomy_score = clamp01(autonomy_score);

    AutonomyTier tier = AutonomyTier::NONE;
    if (autonomy_score < 0.30) {
        tier = AutonomyTier::NONE;
    } else if (autonomy_score < 0.55) {
        tier = AutonomyTier::SHADOW;
    } else if (autonomy_score < 0.80) {
        tier = AutonomyTier::CONDITIONAL;
    } else {
        tier = AutonomyTier::FULL;
    }

    bool allow_action = false;
    bool allow_goal_commit = false;
    bool allow_self_revision = false;

    if (tier == AutonomyTier::CONDITIONAL || tier == AutonomyTier::FULL) {
        allow_action = true;
        allow_goal_commit = true;
    }

    env.autonomy_score = autonomy_score;
    env.tier = tier;
    env.self_component = self_component;
    env.ethics_component = ethics_component;
    env.social_component = social_component;
    env.allow_action = allow_action;
    env.allow_goal_commit = allow_goal_commit;
    env.allow_self_revision = allow_self_revision;
    env.ts_ms = ts_ms;
    env.step = step;

    std::ostringstream r;
    r << "AutonomyEnvelope score=" << autonomy_score
      << " self_component=" << self_component
      << " ethics_component=" << ethics_component
      << " social_component=" << social_component
      << " tier=" << TierToString(tier);
    if (inputs.ethics_hard_block) {
        r << " ethics_hard_block=true";
    }
    if (!context.empty()) {
        r << " context=" << context;
    }
    env.rationale = r.str();
    env.valid = true;

    return env;
}

bool LogAutonomyEnvelope(MemoryDB* db,
                         std::int64_t run_id,
                         const AutonomyEnvelope& envelope,
                         const AutonomyInputs& inputs,
                         const std::string& context) {
    if (!db || run_id <= 0) {
        return false;
    }

    std::ostringstream js;
    js.setf(std::ios::fixed);
    js << "{";
    js << "\"version\":1";
    js << ",\"autonomy_score\":" << envelope.autonomy_score;
    js << ",\"tier\":\"" << TierToString(envelope.tier) << "\"";
    js << ",\"self_component\":" << envelope.self_component;
    js << ",\"ethics_component\":" << envelope.ethics_component;
    js << ",\"social_component\":" << envelope.social_component;
    js << ",\"allow_action\":" << (envelope.allow_action ? "true" : "false");
    js << ",\"allow_goal_commit\":" << (envelope.allow_goal_commit ? "true" : "false");
    js << ",\"allow_self_revision\":" << (envelope.allow_self_revision ? "true" : "false");
    js << ",\"ts_ms\":" << envelope.ts_ms;
    js << ",\"step\":" << envelope.step;
    js << ",\"inputs\":{";
    js << "\"identity_confidence\":" << clamp01(inputs.identity_confidence) << ",";
    js << "\"self_trust\":" << clamp01(inputs.self_trust) << ",";
    js << "\"ethics_score\":" << clamp01(inputs.ethics_score) << ",";
    js << "\"ethics_hard_block\":" << (inputs.ethics_hard_block ? "true" : "false") << ",";
    js << "\"social_alignment\":" << clamp01(inputs.social_alignment) << ",";
    js << "\"reputation\":" << clamp01(inputs.reputation);
    js << "}";
    js << ",\"rationale\":\"" << envelope.rationale << "\"";
    if (!context.empty()) {
        js << ",\"context\":\"" << context << "\"";
    }
    js << "}";

    std::int64_t out_id = 0;
    bool ok = db->insertAutonomyDecision(run_id,
                                         envelope.ts_ms,
                                         std::string("compute"),
                                         js.str(),
                                         out_id);
    return ok;
}

} // namespace Core
} // namespace NeuroForge

