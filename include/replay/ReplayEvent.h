#pragma once

#include <cstdint>
#include <string>


namespace NeuroForge {
namespace Replay {

/**
 * @brief Phase 21a: Types of replay events
 */
enum class ReplayEventType {
  THINK,   ///< Reasoning / goal formation
  GATE,    ///< EpistemicGates check
  ACT,     ///< Action dispatched
  OBSERVE, ///< Observation received
  PROMOTE, ///< Fact promoted to confirmed
  FAIL     ///< Action or verification failed
};

/**
 * @brief Phase 21a: Single replay event in the timeline
 *
 * A ReplayFrame may produce multiple ReplayEvents.
 */
struct ReplayEvent {
  ReplayEventType type = ReplayEventType::THINK;
  std::string summary;
  std::uint64_t timestamp_ms = 0;
  int frame_id = 0;

  /// Convert event type to string
  static std::string typeToString(ReplayEventType t) {
    switch (t) {
    case ReplayEventType::THINK:
      return "THINK";
    case ReplayEventType::GATE:
      return "GATE";
    case ReplayEventType::ACT:
      return "ACT";
    case ReplayEventType::OBSERVE:
      return "OBSERVE";
    case ReplayEventType::PROMOTE:
      return "PROMOTE";
    case ReplayEventType::FAIL:
      return "FAIL";
    default:
      return "UNKNOWN";
    }
  }
};

} // namespace Replay
} // namespace NeuroForge
