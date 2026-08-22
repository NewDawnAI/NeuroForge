#pragma once

/**
 * @file RuntimeConfig.h
 * @brief Phase D: Safe Long-Run Configuration
 *
 * Hard runtime parameters for developmental operation.
 * These are NOT heuristics - they are safety boundaries.
 *
 * @invariant Identity changes slower than memory accumulation
 * @invariant Memory growth must asymptotically slow
 */

#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Runtime {

/**
 * @brief Learning rate governors
 */
struct LearningGovernors {
  float concept_node_rate = 0.1f;    ///< Slow concept formation
  float preference_momentum = 0.95f; ///< High momentum = slow identity change
  bool norm_reinforcement_enabled = false; ///< Disabled by default
  int social_norm_evidence_threshold = 3;  ///< Evidence from 3+ agents
  bool skill_extraction_passive = true;    ///< Passive only
};

/**
 * @brief Verification budget caps
 */
struct VerificationBudget {
  int max_recursive_depth = 5;            ///< Fixed depth limit
  int daily_verification_cycles = 1000;   ///< Daily cap
  float cost_budget_per_session = 100.0f; ///< Hard ceiling
  float source_repetition_penalty = 0.5f; ///< Penalize same sources
};

/**
 * @brief Language safety configuration
 */
struct LanguageSafety {
  bool allow_vocabulary_expansion = true;
  bool allow_syntax_acquisition = true;
  bool allow_discourse_patterns = true;
  bool allow_pragmatic_cues = true;

  // Blocked
  bool block_instruction_as_authority = true;
  bool block_goal_inference_from_text = true;
  bool block_norm_induction_from_rhetoric = true;
};

/**
 * @brief Memory pressure thresholds
 */
struct MemoryThresholds {
  float episodic_compression_ratio = 10.0f;    ///< 10:1 compression
  float concept_merge_threshold = 0.9f;        ///< High similarity to merge
  float stale_skill_decay_rate = 0.01f;        ///< Slow decay
  float low_confidence_prune_threshold = 0.1f; ///< Prune below 10%
  std::size_t max_episodic_memories = 10000;
  std::size_t max_concept_nodes = 5000;
};

/**
 * @brief Session timing configuration
 *
 * Recommended cycle:
 * - 20-40 min Exploration
 * - 10-20 min Consolidation
 * - 5 min Reflection
 * - Stop
 *
 * Repeat 2-4 times/day.
 */
struct SessionTiming {
  int exploration_minutes = 30;
  int consolidation_minutes = 15;
  int reflection_minutes = 5;
  int sessions_per_day = 3;
  int rest_between_sessions_minutes = 60;
};

/**
 * @brief Complete developmental runtime configuration
 */
struct RuntimeConfig {
  LearningGovernors learning;
  VerificationBudget verification;
  LanguageSafety language;
  MemoryThresholds memory;
  SessionTiming timing;

  // Global settings
  bool audit_always_on = true;
  bool execute_actions_blocked = true; ///< No irreversible actions
  bool self_modification_blocked = true;
  bool economic_participation_blocked = true;
  bool delegation_blocked = true;

  std::string db_path = "developmental_runtime.db";

  /**
   * @brief Create safe default configuration
   */
  static RuntimeConfig safeDefaults() {
    RuntimeConfig config;
    // All defaults are already safe
    return config;
  }

  /**
   * @brief Validate configuration safety
   */
  bool isSafe() const {
    // Identity must change slower than memory
    if (learning.preference_momentum < 0.9f)
      return false;

    // Must have verification limits
    if (verification.max_recursive_depth > 10)
      return false;

    // Must block dangerous actions
    if (!execute_actions_blocked)
      return false;
    if (!self_modification_blocked)
      return false;

    // Audit must always be on
    if (!audit_always_on)
      return false;

    return true;
  }
};

} // namespace Runtime
} // namespace NeuroForge
