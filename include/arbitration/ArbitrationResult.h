#pragma once

#include "arbitration/RegionIntent.h"
#include <cstdint>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Arbitration {

/**
 * @brief Phase 22: Record of an arbitration decision
 *
 * Stores what was proposed, what was chosen, and why.
 * This enables the replay viewer to show:
 * "Reasoning wanted X, Language wanted Y, Safety blocked Z — chose A"
 */
struct ArbitrationResult {
  std::uint64_t cycle_id = 0;
  std::uint64_t timestamp_ms = 0;

  /// All proposed intents
  std::vector<RegionIntent> proposals;

  /// Which intent won
  RegionIntent selected;

  /// Why it won
  float selected_score = 0.0f;

  /// Was action vetoed by safety?
  bool vetoed = false;
  std::string veto_reason;
};

} // namespace Arbitration
} // namespace NeuroForge
