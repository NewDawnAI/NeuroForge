#pragma once

/**
 * @file LiabilityMarker.h
 * @brief Phase 30: Attribution Without Punishment
 *
 * Liability ≠ punishment
 * Liability = traceable responsibility
 *
 * @invariant Liability is never auto-resolved
 * @invariant Attribution is always explicit
 */

#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Accountability {

/**
 * @brief Scope of liability
 */
enum class LiabilityScope {
  INTERNAL,   ///< System self-accountability
  OPERATOR,   ///< Human-configured constraint
  REGULATORY, ///< Legal boundary
  CONTRACTUAL ///< Voluntary agreement
};

/**
 * @brief Liability marker - tracks responsibility
 */
struct LiabilityMarker {
  std::uint64_t marker_id = 0;
  std::string event_id;

  LiabilityScope scope = LiabilityScope::INTERNAL;

  // Attribution
  std::string attributed_to; ///< "system", "operator", "regulator", "contract"
  std::string rationale;

  // State
  bool acknowledged = false; ///< Never auto-resolved
  std::uint64_t created_at_ms = 0;

  /**
   * @brief Convert scope to string
   */
  static std::string scopeToString(LiabilityScope s) {
    switch (s) {
    case LiabilityScope::INTERNAL:
      return "internal";
    case LiabilityScope::OPERATOR:
      return "operator";
    case LiabilityScope::REGULATORY:
      return "regulatory";
    case LiabilityScope::CONTRACTUAL:
      return "contractual";
    default:
      return "unknown";
    }
  }
};

} // namespace Accountability
} // namespace NeuroForge
