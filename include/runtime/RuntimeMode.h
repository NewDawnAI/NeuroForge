#pragma once

/**
 * @file RuntimeMode.h
 * @brief Phase D: Developmental Runtime Modes
 *
 * NeuroForge must never be "just running."
 * It always runs in one of these explicit modes.
 *
 * @invariant Architecture defines what is possible.
 * @invariant Runtime defines what is allowed to persist.
 */

#include <string>

namespace NeuroForge {
namespace Runtime {

/**
 * @brief Explicit runtime modes
 *
 * NeuroForge always runs in one of these modes.
 */
enum class RuntimeMode {
  EXPLORATION,   ///< Acquire descriptive knowledge only
  CONSOLIDATION, ///< Prevent cognitive inflation (sleep)
  REFLECTION,    ///< Update self-model, not beliefs
  AUDIT_ONLY,    ///< Inspection without influence
  EXPERIMENT     ///< Stress-test specific mechanisms
};

/**
 * @brief What is allowed in each mode
 */
struct ModePermissions {
  bool allow_web_browsing = false;
  bool allow_perception = false;
  bool allow_verification = false;
  bool allow_memory_write = false;
  bool allow_concept_formation = false;
  bool allow_language_exposure = false;

  bool allow_norm_induction = false;
  bool allow_preference_updates = false;
  bool allow_execute_actions = false;
  bool allow_decide_actions = false;

  bool allow_consolidation = false;
  bool allow_dream_processing = false;
  bool allow_concept_merging = false;
  bool allow_pruning = false;

  bool allow_replay_analysis = false;
  bool allow_self_narrator = false;

  bool allow_external_io = false;
  bool allow_action_broker = false;
  bool allow_norm_negotiation = false;

  bool allow_audit_query = false;
  bool allow_compliance_export = false;
};

/**
 * @brief Get permissions for a runtime mode
 */
inline ModePermissions getModePermissions(RuntimeMode mode) {
  ModePermissions p;

  switch (mode) {
  case RuntimeMode::EXPLORATION:
    // Acquire descriptive knowledge only
    p.allow_web_browsing = true;
    p.allow_perception = true;
    p.allow_verification = true;
    p.allow_memory_write = true;
    p.allow_concept_formation = true;
    p.allow_language_exposure = true;
    p.allow_external_io = true;
    // Blocked: norm induction, preference updates, execute/decide
    break;

  case RuntimeMode::CONSOLIDATION:
    // Prevent cognitive inflation (sleep)
    p.allow_consolidation = true;
    p.allow_dream_processing = true;
    p.allow_concept_merging = true;
    p.allow_pruning = true;
    // Blocked: external I/O, ActionBroker, norm negotiation
    break;

  case RuntimeMode::REFLECTION:
    // Update self-model, not beliefs
    p.allow_replay_analysis = true;
    p.allow_preference_updates = true; // Momentum only
    p.allow_self_narrator = true;
    // Blocked: new exploration, norm/value changes
    break;

  case RuntimeMode::AUDIT_ONLY:
    // Inspection without influence
    p.allow_audit_query = true;
    p.allow_compliance_export = true;
    // Everything else blocked
    break;

  case RuntimeMode::EXPERIMENT:
    // Stress-test specific mechanisms (configured per-experiment)
    p.allow_web_browsing = true;
    p.allow_perception = true;
    p.allow_verification = true;
    // Other permissions configured per-experiment
    break;
  }

  return p;
}

/**
 * @brief Convert mode to string
 */
inline std::string modeToString(RuntimeMode mode) {
  switch (mode) {
  case RuntimeMode::EXPLORATION:
    return "exploration";
  case RuntimeMode::CONSOLIDATION:
    return "consolidation";
  case RuntimeMode::REFLECTION:
    return "reflection";
  case RuntimeMode::AUDIT_ONLY:
    return "audit_only";
  case RuntimeMode::EXPERIMENT:
    return "experiment";
  default:
    return "unknown";
  }
}

} // namespace Runtime
} // namespace NeuroForge
