#pragma once

#include "arbitration/ArbitrationResult.h"
#include "arbitration/RegionIntent.h"
#include "identity/PreferenceModel.h"


#include <sstream>
#include <string>


namespace NeuroForge {
namespace Identity {

/**
 * @brief Phase 23: Self-Narrator
 *
 * Generates human-readable explanations of decisions
 * based on preferences and arbitration outcomes.
 */
class SelfNarrator {
public:
  /**
   * @brief Explain why a decision was made
   */
  static std::string
  explainDecision(const Arbitration::ArbitrationResult &result,
                  const PreferenceModel &model) {

    std::ostringstream ss;

    if (result.vetoed) {
      ss << "I chose to pause because safety concerns arose: "
         << result.veto_reason;
      return ss.str();
    }

    ss << "I chose to " << describeAction(result.selected);

    // Add preference-based explanation
    std::string dominant = getDominantTendency(model);
    if (!dominant.empty()) {
      ss << " because I tend to " << dominant;
    }

    // Add score context
    ss << " (score=" << std::fixed << std::setprecision(2)
       << result.selected_score << ")";

    return ss.str();
  }

  /**
   * @brief Describe the agent's current self-understanding
   */
  static std::string describeSelf(const PreferenceModel &model) {
    std::ostringstream ss;
    ss << "After " << model.getTotalObservations() << " observations, I am:\n";

    if (model.risk_tolerance > 0.6f) {
      ss << "  - willing to take risks\n";
    } else if (model.risk_tolerance < 0.4f) {
      ss << "  - cautious about high-cost actions\n";
    }

    if (model.action_bias > 0.6f) {
      ss << "  - inclined to act rather than speak\n";
    } else if (model.action_bias < 0.4f) {
      ss << "  - inclined to ask questions before acting\n";
    }

    if (model.verification_bias > 0.6f) {
      ss << "  - thorough in verification\n";
    } else if (model.verification_bias < 0.4f) {
      ss << "  - exploratory rather than confirmatory\n";
    }

    if (model.caution_level > 0.6f) {
      ss << "  - deliberate and careful\n";
    } else if (model.caution_level < 0.4f) {
      ss << "  - quick to decide\n";
    }

    return ss.str();
  }

private:
  static std::string describeAction(const Arbitration::RegionIntent &intent) {
    switch (intent.region) {
    case Arbitration::CognitiveRegion::REASONING:
      return "verify a belief";
    case Arbitration::CognitiveRegion::PERCEPTION:
      return "explore further";
    case Arbitration::CognitiveRegion::LANGUAGE:
      return "ask a clarifying question";
    case Arbitration::CognitiveRegion::PROCEDURAL:
      return "use a learned skill";
    case Arbitration::CognitiveRegion::SAFETY:
      return "pause and reconsider";
    default:
      return "act";
    }
  }

  static std::string getDominantTendency(const PreferenceModel &model) {
    float max_pref = 0.0f;
    std::string dominant;

    for (const auto &[key, pref] : model.preferences) {
      if (pref.weight > max_pref && pref.observations > 5) {
        max_pref = pref.weight;
        dominant = key;
      }
    }

    if (dominant == PreferenceKeys::PREFER_VERIFICATION) {
      return "verify before believing";
    } else if (dominant == PreferenceKeys::AVOID_HIGH_COST) {
      return "avoid expensive operations";
    } else if (dominant == PreferenceKeys::FAVOR_LANGUAGE) {
      return "ask before acting";
    } else if (dominant == PreferenceKeys::PREFER_REASONING) {
      return "reason carefully";
    }

    return "";
  }
};

} // namespace Identity
} // namespace NeuroForge
