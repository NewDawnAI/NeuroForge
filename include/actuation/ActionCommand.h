#pragma once

#include <cstdint>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Actuation {

/**
 * @brief Phase 21: Action kind categories
 */
enum class ActionKind {
  SEARCH,     ///< Web search query
  NAVIGATE,   ///< Navigate to URL
  SPEAK,      ///< Speech output (verification probe)
  MANIPULATE, ///< Physical manipulation (future)
  OBSERVE     ///< Passive observation request
};

/**
 * @brief Phase 21: Safety level for actions
 */
enum class SafetyLevel {
  PASSIVE,     ///< Read-only, no side effects
  INTERACTIVE, ///< Speech, UI interaction
  PHYSICAL     ///< Embodied (future, requires extra gates)
};

/**
 * @brief Phase 21: Action command with epistemic provenance
 *
 * Every action traces back to a belief that caused it.
 */
struct ActionCommand {
  std::uint64_t id = 0;
  std::uint64_t timestamp_ms = 0;

  ActionKind kind = ActionKind::OBSERVE;
  std::string parameters; ///< JSON or structured params

  // Epistemic provenance
  int originating_frame_id = -1; ///< VerificationFrame that caused this
  std::uint64_t trace_id = 0;    ///< Reasoning trace ID
  std::string expected_outcome;  ///< What we expect to observe

  SafetyLevel safety_level = SafetyLevel::PASSIVE;

  /// Convert ActionKind to string for logging
  static std::string kindToString(ActionKind kind) {
    switch (kind) {
    case ActionKind::SEARCH:
      return "SEARCH";
    case ActionKind::NAVIGATE:
      return "NAVIGATE";
    case ActionKind::SPEAK:
      return "SPEAK";
    case ActionKind::MANIPULATE:
      return "MANIPULATE";
    case ActionKind::OBSERVE:
      return "OBSERVE";
    default:
      return "UNKNOWN";
    }
  }
};

} // namespace Actuation
} // namespace NeuroForge
