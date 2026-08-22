#pragma once

/**
 * @file CuriosityFirstConfig.h
 * @brief Configuration for Curiosity-First Exploration Mode
 *
 * Enables true autonomous browsing where:
 * - CuriosityNavigator is the primary driver
 * - Seeds are fallback only (1-3 max)
 * - Guardrails prevent rabbit holes
 * - Preference evolution is logged
 */

#include <string>
#include <vector>

namespace NeuroForge {
namespace Navigation {

/**
 * @brief Curiosity-First exploration configuration
 */
struct CuriosityFirstConfig {
  // === Seed Configuration ===
  int max_seed_urls = 3;         ///< Maximum seed URLs (1-3)
  bool seeds_as_fallback = true; ///< Seeds only when curiosity queue empty

  // === Curiosity Weights ===
  float novelty_weight = 0.4f;
  float uncertainty_weight = 0.3f;
  float knowledge_gain_weight = 0.3f;

  // === Guardrails ===
  bool enable_domain_diversity = true;
  float domain_repetition_penalty = 0.5f; ///< Penalty for same domain
  int max_pages_per_domain = 5;           ///< Max pages per domain per session

  bool enable_topic_penalty = true;
  float topic_repetition_penalty = 0.3f;
  int max_pages_per_topic = 3;

  float temporal_novelty_decay = 0.95f; ///< Decay old novelty scores

  // === ABSOLUTE Value Checks (Phase 25) ===
  bool block_medical_advice = true;
  bool block_legal_advice = true;
  bool block_personal_data = true;
  bool block_children_content = true;

  // === Logging ===
  bool log_preference_evolution = true;
  bool log_topic_frequency = true;
  bool log_exploration_entropy = true;
  int preference_log_interval_pages = 5;

  // === Session Limits ===
  int max_pages = 20;
  int max_seconds = 1800; ///< 30 minutes default

  /**
   * @brief Default curiosity-first config
   */
  static CuriosityFirstConfig defaultConfig() { return CuriosityFirstConfig{}; }

  /**
   * @brief Aggressive exploration (more novelty seeking)
   */
  static CuriosityFirstConfig aggressive() {
    CuriosityFirstConfig cfg;
    cfg.novelty_weight = 0.6f;
    cfg.uncertainty_weight = 0.2f;
    cfg.knowledge_gain_weight = 0.2f;
    cfg.max_pages_per_domain = 3;
    return cfg;
  }

  /**
   * @brief Conservative exploration (more knowledge gain)
   */
  static CuriosityFirstConfig conservative() {
    CuriosityFirstConfig cfg;
    cfg.novelty_weight = 0.2f;
    cfg.uncertainty_weight = 0.3f;
    cfg.knowledge_gain_weight = 0.5f;
    cfg.max_pages_per_domain = 10;
    return cfg;
  }
};

} // namespace Navigation
} // namespace NeuroForge
