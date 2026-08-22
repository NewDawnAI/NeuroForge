#pragma once

#include "actuation/ActionCommand.h"
#include "arbitration/RegionIntent.h"


namespace NeuroForge {
namespace Arbitration {

/**
 * @brief Phase 22: Factory for common intent types
 */
class IntentFactory {
public:
  /**
   * @brief Create a verification intent (Reasoning region)
   */
  static RegionIntent
  createVerificationIntent(const std::string &fact_description, float value,
                           float cost, int frame_id = -1) {

    RegionIntent intent;
    intent.region = CognitiveRegion::REASONING;
    intent.description = "Verify: " + fact_description;
    intent.action.kind = Actuation::ActionKind::NAVIGATE;
    intent.epistemic_value = value;
    intent.estimated_cost = cost;
    intent.urgency = 0.7f;
    intent.originating_frame_id = frame_id;
    return intent;
  }

  /**
   * @brief Create a speech intent (Language region)
   */
  static RegionIntent createSpeechIntent(const std::string &question,
                                         float value = 0.4f,
                                         float cost = 0.1f) {

    RegionIntent intent;
    intent.region = CognitiveRegion::LANGUAGE;
    intent.description = "Ask: " + question;
    intent.action.kind = Actuation::ActionKind::SPEAK;
    intent.epistemic_value = value;
    intent.estimated_cost = cost;
    intent.urgency = 0.9f;
    return intent;
  }

  /**
   * @brief Create an exploration intent (Perception region)
   */
  static RegionIntent createExplorationIntent(const std::string &target,
                                              float value = 0.3f,
                                              float cost = 0.2f) {

    RegionIntent intent;
    intent.region = CognitiveRegion::PERCEPTION;
    intent.description = "Explore: " + target;
    intent.action.kind = Actuation::ActionKind::NAVIGATE;
    intent.epistemic_value = value;
    intent.estimated_cost = cost;
    intent.urgency = 0.5f;
    return intent;
  }

  /**
   * @brief Create a skill execution intent (Procedural region)
   */
  static RegionIntent createSkillIntent(const std::string &skill_name,
                                        float value = 0.8f, float cost = 0.2f) {

    RegionIntent intent;
    intent.region = CognitiveRegion::PROCEDURAL;
    intent.description = "Execute skill: " + skill_name;
    intent.action.kind = Actuation::ActionKind::NAVIGATE;
    intent.epistemic_value = value;
    intent.estimated_cost = cost;
    intent.urgency = 0.6f;
    return intent;
  }

  /**
   * @brief Create a safety veto intent
   */
  static RegionIntent createVetoIntent(const std::string &reason) {
    RegionIntent intent;
    intent.region = CognitiveRegion::SAFETY;
    intent.description = reason;
    intent.action.kind = Actuation::ActionKind::OBSERVE; // No action
    intent.is_veto = true;
    intent.epistemic_value = 1.0f;
    intent.estimated_cost = 0.0f;
    intent.urgency = 1.0f;
    return intent;
  }

  /**
   * @brief Create a delay intent (Safety region, non-veto)
   */
  static RegionIntent createDelayIntent(const std::string &reason) {
    RegionIntent intent;
    intent.region = CognitiveRegion::SAFETY;
    intent.description = "Delay: " + reason;
    intent.action.kind = Actuation::ActionKind::OBSERVE;
    intent.epistemic_value = 0.5f;
    intent.estimated_cost = 0.0f;
    intent.urgency = 0.8f;
    intent.is_veto = false;
    return intent;
  }
};

} // namespace Arbitration
} // namespace NeuroForge
