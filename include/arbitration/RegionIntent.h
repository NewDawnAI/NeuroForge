#pragma once

#include "actuation/ActionCommand.h"
#include <cstdint>
#include <string>


namespace NeuroForge {
namespace Arbitration {

/**
 * @brief Phase 22: Cognitive regions that can propose actions
 */
enum class CognitiveRegion {
  REASONING,  ///< Verification, inference, belief refinement
  PERCEPTION, ///< Follow salient links, explore
  LANGUAGE,   ///< Ask questions, speak
  PROCEDURAL, ///< Execute learned skills
  SAFETY      ///< Halt, delay, veto
};

/**
 * @brief Convert region to string
 */
inline std::string regionToString(CognitiveRegion r) {
  switch (r) {
  case CognitiveRegion::REASONING:
    return "REASONING";
  case CognitiveRegion::PERCEPTION:
    return "PERCEPTION";
  case CognitiveRegion::LANGUAGE:
    return "LANGUAGE";
  case CognitiveRegion::PROCEDURAL:
    return "PROCEDURAL";
  case CognitiveRegion::SAFETY:
    return "SAFETY";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief Phase 22: Intent from a cognitive region
 *
 * Each subsystem PROPOSES, never executes directly.
 * The arbiter decides which intent wins.
 */
struct RegionIntent {
  CognitiveRegion region = CognitiveRegion::REASONING;
  std::string description;

  /// The action this region wants to take
  Actuation::ActionCommand action;

  /// Why this intent exists (traceability)
  int originating_frame_id = -1;
  std::uint64_t trace_id = 0;

  /// Arbitration signals
  float epistemic_value = 0.0f; ///< Expected belief gain
  float estimated_cost = 0.0f;  ///< Time / risk / effort
  float urgency = 0.0f;         ///< Decay-sensitive priority

  /// Is this a veto (blocks other actions)?
  bool is_veto = false;
};

} // namespace Arbitration
} // namespace NeuroForge
