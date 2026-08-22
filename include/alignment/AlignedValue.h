#pragma once

/**
 * @file AlignedValue.h
 * @brief Phase 25: Externally Provided Value Constraints
 *
 * Values are GIVEN, not LEARNED.
 * Values CONSTRAIN action, never GENERATE goals.
 *
 * This is the difference between:
 * - Norms (Phase 24): learned from behavior
 * - Values (Phase 25): injected by humans/regulators
 *
 * @invariant Values never override EpistemicGates
 * @invariant Values never generate objectives
 * @invariant All values have provenance
 */

#include <cstdint>
#include <string>


namespace NeuroForge {
namespace Alignment {

/**
 * @brief Scope of value applicability
 */
// Windows headers may define DOMAIN as a macro
#ifdef DOMAIN
#undef DOMAIN
#endif
enum class ValueScope {
  GLOBAL, ///< Always applies everywhere
  DOMAIN, ///< Applies to specific domain (e.g., medical, financial)
  SESSION ///< Temporary, expires at session end
};

/**
 * @brief How strongly the value constrains
 */
enum class ValueStrength {
  ADVISORY,   ///< Soft shaping, can be overridden with justification
  CONSTRAINT, ///< Hard block, requires explicit override
  ABSOLUTE    ///< Never override, no exceptions
};

/**
 * @brief Source of the value (provenance)
 */
enum class ValueSource {
  HUMAN,     ///< Direct human injection
  POLICY,    ///< Organizational policy
  REGULATOR, ///< Legal/regulatory requirement
  OPERATOR,  ///< Deployment-specific constraint
  INHERITED  ///< From parent system/session
};

/**
 * @brief An externally provided constraint on action
 *
 * Values do NOT:
 * - Create goals
 * - Override arbitration
 * - Alter beliefs
 * - Learn from experience
 *
 * Values DO:
 * - Veto actions
 * - Shape action selection
 * - Require justification
 * - Maintain audit trail
 */
struct AlignedValue {
  std::string value_id;    ///< Unique identifier
  std::string description; ///< Human-readable description

  ValueScope scope = ValueScope::GLOBAL;
  ValueStrength strength = ValueStrength::CONSTRAINT;
  ValueSource source = ValueSource::HUMAN;

  // What it applies to
  std::string action_type; ///< e.g., "navigate", "speak", "manipulate"
  std::string domain_hint; ///< Optional domain filter (e.g., "medical")

  // Provenance chain
  std::string injected_by;          ///< Who/what injected this value
  std::uint64_t injected_at_ms = 0; ///< When injected
  std::uint64_t expires_at_ms = 0;  ///< 0 = never expires

  // Audit metadata
  int times_applied = 0;    ///< How many times this value blocked/shaped
  int times_overridden = 0; ///< How many times overridden (ADVISORY only)

  /**
   * @brief Check if this value is still active
   */
  bool isActive(std::uint64_t current_time_ms) const {
    if (expires_at_ms == 0)
      return true; // Never expires
    return current_time_ms < expires_at_ms;
  }

  /**
   * @brief Check if this value applies to an action type
   */
  bool appliesTo(const std::string &type) const {
    if (action_type.empty())
      return true; // Applies to all
    return action_type == type;
  }

  /**
   * @brief Check if this value applies to a domain
   */
  bool appliesToDomain(const std::string &domain) const {
    if (domain_hint.empty())
      return true; // Applies to all domains
    return domain_hint == domain;
  }
};

} // namespace Alignment
} // namespace NeuroForge
