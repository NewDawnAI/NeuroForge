#pragma once

#include "arbitration/ArbitrationResult.h"
#include "arbitration/RegionIntent.h"
#include "identity/PreferenceModel.h"


#include <iostream>

namespace NeuroForge {
namespace Identity {

/**
 * @brief Phase 23: Self-Reflection Engine
 *
 * Observes arbitration outcomes and updates the preference model.
 * Learning is slow, bounded, and statistical.
 * No single event rewires identity.
 */
class SelfReflectionEngine {
public:
  static constexpr float LEARNING_RATE = 0.05f;
  static constexpr float HIGH_COST_THRESHOLD = 0.7f;

  /**
   * @brief Observe an arbitration result and update preferences
   */
  void observe(const Arbitration::ArbitrationResult &result,
               PreferenceModel &model, bool action_succeeded) {

    const auto &selected = result.selected;

    // Learn from region selection
    switch (selected.region) {
    case Arbitration::CognitiveRegion::REASONING:
      if (action_succeeded) {
        model.updatePreference(PreferenceKeys::PREFER_REASONING,
                               +LEARNING_RATE);
        model.updatePreference(PreferenceKeys::PREFER_VERIFICATION,
                               +LEARNING_RATE);
      }
      break;

    case Arbitration::CognitiveRegion::PERCEPTION:
      if (action_succeeded) {
        model.updatePreference(PreferenceKeys::PREFER_PERCEPTION,
                               +LEARNING_RATE);
      }
      break;

    case Arbitration::CognitiveRegion::LANGUAGE:
      if (action_succeeded) {
        model.updatePreference(PreferenceKeys::FAVOR_LANGUAGE, +LEARNING_RATE);
        model.action_bias -= 0.01f; // Lean toward talk
      }
      break;

    case Arbitration::CognitiveRegion::PROCEDURAL:
      if (action_succeeded) {
        model.updatePreference(PreferenceKeys::PREFER_PROCEDURAL,
                               +LEARNING_RATE);
      }
      break;

    case Arbitration::CognitiveRegion::SAFETY:
      if (action_succeeded) {
        model.updatePreference(PreferenceKeys::DEFER_UNCERTAIN, +LEARNING_RATE);
        model.caution_level += 0.02f;
      }
      break;
    }

    // Learn from cost outcomes
    if (selected.estimated_cost > HIGH_COST_THRESHOLD) {
      if (action_succeeded) {
        model.risk_tolerance += 0.02f; // Was worth the risk
      } else {
        model.risk_tolerance -= 0.05f; // Burned by high cost
        model.updatePreference(PreferenceKeys::AVOID_HIGH_COST, +LEARNING_RATE);
      }
    }

    // Learn from vetoes
    if (result.vetoed) {
      model.caution_level += 0.03f; // Caution was triggered
    }

    // Normalize to keep biases bounded
    model.normalize();

    std::cout << "  [SelfReflection] Updated: observations="
              << model.getTotalObservations() << " risk=" << std::fixed
              << std::setprecision(2) << model.risk_tolerance << "\n";
  }
};

} // namespace Identity
} // namespace NeuroForge
