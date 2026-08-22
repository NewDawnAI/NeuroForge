#pragma once

#include <cstdint>
#include <string>


namespace NeuroForge {
namespace Norms {

/**
 * @brief Phase 24: Norm strength levels
 */
enum class NormStrength {
  SOFT,    ///< Preference-aligned, can be overridden by higher norms
  HARD,    ///< Safety / law / integrity - rarely override
  ABSOLUTE ///< Cannot be overridden (e.g., physical safety)
};

/**
 * @brief Phase 24: Norm decision types
 */
enum class NormDecision {
  ALLOW,      ///< Explicitly permissible
  DISCOURAGE, ///< Should avoid unless necessary
  FORBID      ///< Must not proceed
};

/**
 * @brief Convert NormStrength to string
 */
inline std::string strengthToString(NormStrength s) {
  switch (s) {
  case NormStrength::SOFT:
    return "SOFT";
  case NormStrength::HARD:
    return "HARD";
  case NormStrength::ABSOLUTE:
    return "ABSOLUTE";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief Convert NormDecision to string
 */
inline std::string decisionToString(NormDecision d) {
  switch (d) {
  case NormDecision::ALLOW:
    return "ALLOW";
  case NormDecision::DISCOURAGE:
    return "DISCOURAGE";
  case NormDecision::FORBID:
    return "FORBID";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief Phase 24: A Norm
 *
 * A norm is a rule of the form:
 *   IF context THEN action is {allowed | discouraged | forbidden}
 *   WITH justification trace
 *
 * Norms are evaluated, not optimized.
 * Norms filter, never select.
 * Norms can be overridden only by higher-priority norms.
 */
struct Norm {
  std::string norm_id;

  /// Context signature this norm applies to
  /// e.g., "external_action", "speech", "verification", "embodiment"
  std::string context_signature;

  NormStrength strength = NormStrength::SOFT;
  NormDecision decision = NormDecision::ALLOW;

  /// Why this norm exists (traceable to ReplayFrames)
  std::string justification;

  /// Source of this norm
  std::string source; ///< "learned", "gate_derived", "human_injected"

  /// Learning metadata
  int violation_count = 0;
  int reinforcement_count = 0;

  /// Priority (higher = more authoritative)
  int priority = 0;
};

} // namespace Norms
} // namespace NeuroForge
