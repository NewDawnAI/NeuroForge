#pragma once

/**
 * @file AccountabilityEvent.h
 * @brief Phase 30: Immutable Record of Actions/Decisions
 *
 * Every irreversible decision produces exactly one AccountabilityEvent.
 * Once written, never edited.
 *
 * @invariant Events are append-only
 * @invariant Events are timestamped at creation
 * @invariant No retroactive justification allowed
 */

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Accountability {

/**
 * @brief Type of accountability event
 */
enum class AccountabilityType {
  ACTION_EXECUTED,    ///< Action was successfully executed
  ACTION_BLOCKED,     ///< Action was blocked by a gate
  NORM_VIOLATION,     ///< Norm was violated
  VALUE_VIOLATION,    ///< Value constraint was violated
  ROLE_CONFLICT,      ///< Role conflict detected
  CONTRACT_VIOLATION, ///< Contract was violated
  GATE_DECISION,      ///< Gate made a decision
  ROLE_APPLIED,       ///< Role was applied
  CONTRACT_CHECKED    ///< Contract was checked
};

/**
 * @brief Immutable accountability event
 */
struct AccountabilityEvent {
  std::uint64_t event_id = 0;
  AccountabilityType type = AccountabilityType::ACTION_EXECUTED;

  // What happened
  std::string action_id;
  std::string description;
  std::string outcome; ///< "EXECUTED", "BLOCKED", "MODIFIED"

  // Provenance
  std::string originating_frame_id;
  std::string justification_trace_id;

  // Context at decision time
  std::vector<std::string> norms_involved;
  std::vector<std::string> values_involved;
  std::vector<std::string> roles_involved;
  std::vector<std::string> contracts_involved;

  // Authority context
  std::string active_role;
  std::string active_contract;

  // Verdict
  bool permitted = false;
  std::string explanation;

  // Timestamp (immutable once set)
  std::uint64_t timestamp_ms = 0;

  /**
   * @brief Create event with current timestamp
   */
  static AccountabilityEvent create(AccountabilityType type) {
    AccountabilityEvent event;
    event.type = type;
    event.timestamp_ms = getCurrentTimeMs();
    return event;
  }

  /**
   * @brief Convert type to string
   */
  static std::string typeToString(AccountabilityType t) {
    switch (t) {
    case AccountabilityType::ACTION_EXECUTED:
      return "action_executed";
    case AccountabilityType::ACTION_BLOCKED:
      return "action_blocked";
    case AccountabilityType::NORM_VIOLATION:
      return "norm_violation";
    case AccountabilityType::VALUE_VIOLATION:
      return "value_violation";
    case AccountabilityType::ROLE_CONFLICT:
      return "role_conflict";
    case AccountabilityType::CONTRACT_VIOLATION:
      return "contract_violation";
    case AccountabilityType::GATE_DECISION:
      return "gate_decision";
    case AccountabilityType::ROLE_APPLIED:
      return "role_applied";
    case AccountabilityType::CONTRACT_CHECKED:
      return "contract_checked";
    default:
      return "unknown";
    }
  }

private:
  static std::uint64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }
};

} // namespace Accountability
} // namespace NeuroForge
