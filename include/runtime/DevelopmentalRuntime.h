#pragma once

/**
 * @file DevelopmentalRuntime.h
 * @brief Phase D: Main Developmental Runtime Controller
 *
 * The operating regime under which the completed architecture lives.
 *
 * Runtime optimizes for:
 * 1. Epistemic stability
 * 2. Identity continuity
 * 3. Bounded growth
 * 4. Auditability
 * 5. Reversibility
 *
 * @invariant Long-run operation is developmental, not goal-seeking
 */

#include "../accountability/AccountabilityEngine.h"
#include "../accountability/AuditTrail.h"
#include "RuntimeConfig.h"
#include "RuntimeMode.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

namespace NeuroForge {
namespace Runtime {

/**
 * @brief Session statistics
 */
struct SessionStats {
  std::uint64_t start_time_ms = 0;
  std::uint64_t end_time_ms = 0;

  int pages_browsed = 0;
  int concepts_formed = 0;
  int verifications_performed = 0;
  int memories_consolidated = 0;
  int reflections_completed = 0;

  RuntimeMode final_mode = RuntimeMode::EXPLORATION;
  bool completed_successfully = false;
};

/**
 * @brief Phase D: Developmental Runtime Controller
 */
class DevelopmentalRuntime {
public:
  using ExplorationCallback = std::function<void(const ModePermissions &)>;
  using ConsolidationCallback = std::function<void(const ModePermissions &)>;
  using ReflectionCallback = std::function<void(const ModePermissions &)>;

  explicit DevelopmentalRuntime(const RuntimeConfig &config)
      : config_(config), current_mode_(RuntimeMode::EXPLORATION), trail_(),
        engine_(trail_) {

    // Validate configuration safety
    if (!config_.isSafe()) {
      std::cerr << "[DevelopmentalRuntime] WARNING: Configuration is not safe!"
                << std::endl;
    }
  }

  /**
   * @brief Run a complete developmental session
   *
   * Cycle:
   * - Exploration (30 min default)
   * - Consolidation (15 min default)
   * - Reflection (5 min default)
   */
  SessionStats runSession() {
    SessionStats stats;
    stats.start_time_ms = getCurrentTimeMs();

    std::cout << "\n=== NeuroForge Developmental Runtime Session ==="
              << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Exploration:   " << config_.timing.exploration_minutes
              << " min" << std::endl;
    std::cout << "  Consolidation: " << config_.timing.consolidation_minutes
              << " min" << std::endl;
    std::cout << "  Reflection:    " << config_.timing.reflection_minutes
              << " min" << std::endl;
    std::cout << "  Audit:         " << (config_.audit_always_on ? "ON" : "OFF")
              << std::endl;
    std::cout << "  Execute:       "
              << (config_.execute_actions_blocked ? "BLOCKED" : "ALLOWED")
              << std::endl;
    std::cout << std::endl;

    // Phase 1: Exploration
    std::cout << "[1/3] EXPLORATION MODE" << std::endl;
    switchMode(RuntimeMode::EXPLORATION);
    runExplorationPhase(stats);

    // Phase 2: Consolidation
    std::cout << "\n[2/3] CONSOLIDATION MODE (Sleep)" << std::endl;
    switchMode(RuntimeMode::CONSOLIDATION);
    runConsolidationPhase(stats);

    // Phase 3: Reflection
    std::cout << "\n[3/3] REFLECTION MODE" << std::endl;
    switchMode(RuntimeMode::REFLECTION);
    runReflectionPhase(stats);

    stats.end_time_ms = getCurrentTimeMs();
    stats.final_mode = current_mode_;
    stats.completed_successfully = true;

    // Print summary
    std::cout << "\n=== Session Complete ===" << std::endl;
    std::cout << "Duration:       "
              << (stats.end_time_ms - stats.start_time_ms) / 1000 << " seconds"
              << std::endl;
    std::cout << "Pages browsed:  " << stats.pages_browsed << std::endl;
    std::cout << "Concepts:       " << stats.concepts_formed << std::endl;
    std::cout << "Verifications:  " << stats.verifications_performed
              << std::endl;
    std::cout << "Consolidations: " << stats.memories_consolidated << std::endl;
    std::cout << "Reflections:    " << stats.reflections_completed << std::endl;

    return stats;
  }

  /**
   * @brief Switch to a specific runtime mode
   */
  void switchMode(RuntimeMode mode) {
    auto permissions = getModePermissions(mode);

    // Log mode transition
    engine_.onGateDecision("ModeSwitch", "mode_" + modeToString(mode), true,
                           "Switching to " + modeToString(mode) + " mode");

    current_mode_ = mode;
    current_permissions_ = permissions;

    std::cout << "  Mode: " << modeToString(mode) << std::endl;
  }

  /**
   * @brief Get current mode
   */
  RuntimeMode getCurrentMode() const { return current_mode_; }

  /**
   * @brief Get current permissions
   */
  const ModePermissions &getPermissions() const { return current_permissions_; }

  /**
   * @brief Check if an action is permitted
   */
  bool isActionPermitted(const std::string &action_type) const {
    if (action_type == "browse" || action_type == "navigate") {
      return current_permissions_.allow_web_browsing;
    }
    if (action_type == "perceive" || action_type == "observe") {
      return current_permissions_.allow_perception;
    }
    if (action_type == "verify") {
      return current_permissions_.allow_verification;
    }
    if (action_type == "execute") {
      return current_permissions_.allow_execute_actions &&
             !config_.execute_actions_blocked;
    }
    if (action_type == "decide") {
      return current_permissions_.allow_decide_actions;
    }
    return false;
  }

  /**
   * @brief Set exploration callback
   */
  void setExplorationCallback(ExplorationCallback cb) {
    exploration_callback_ = cb;
  }

  /**
   * @brief Set consolidation callback
   */
  void setConsolidationCallback(ConsolidationCallback cb) {
    consolidation_callback_ = cb;
  }

  /**
   * @brief Set reflection callback
   */
  void setReflectionCallback(ReflectionCallback cb) {
    reflection_callback_ = cb;
  }

  /**
   * @brief Get audit trail
   */
  const Accountability::AuditTrail &getAuditTrail() const { return trail_; }

private:
  void runExplorationPhase(SessionStats &stats) {
    auto permissions = getModePermissions(RuntimeMode::EXPLORATION);

    // Simulate exploration (in real implementation, this would call actual
    // browsing)
    int duration_ms = config_.timing.exploration_minutes * 60 * 1000;
    int steps = 10; // Simulated steps

    for (int i = 0; i < steps; i++) {
      std::cout << "  [Exploration] Step " << (i + 1) << "/" << steps
                << std::endl;

      // Call user callback if set
      if (exploration_callback_) {
        exploration_callback_(permissions);
      }

      stats.pages_browsed++;
      stats.concepts_formed++;
      stats.verifications_performed++;

      // Record accountability event
      engine_.onActionExecuted("explore_" + std::to_string(i),
                               "exploration_phase", "EXPLORER", "");

      // Sleep briefly (in real implementation, this would be actual work)
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  void runConsolidationPhase(SessionStats &stats) {
    auto permissions = getModePermissions(RuntimeMode::CONSOLIDATION);

    std::cout << "  [Consolidation] Memory compression..." << std::endl;

    if (consolidation_callback_) {
      consolidation_callback_(permissions);
    }

    stats.memories_consolidated++;

    engine_.onActionExecuted("consolidate_1", "consolidation_phase",
                             "CONSOLIDATOR", "");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "  [Consolidation] Concept merging..." << std::endl;
    stats.memories_consolidated++;

    std::cout << "  [Consolidation] Pruning low-confidence facts..."
              << std::endl;
    stats.memories_consolidated++;
  }

  void runReflectionPhase(SessionStats &stats) {
    auto permissions = getModePermissions(RuntimeMode::REFLECTION);

    std::cout << "  [Reflection] Analyzing replay frames..." << std::endl;

    if (reflection_callback_) {
      reflection_callback_(permissions);
    }

    stats.reflections_completed++;

    engine_.onActionExecuted("reflect_1", "reflection_phase", "REFLECTOR", "");

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "  [Reflection] Updating self-narrative..." << std::endl;
    stats.reflections_completed++;

    std::cout << "  [Reflection] Preference momentum update..." << std::endl;
    stats.reflections_completed++;
  }

  static std::uint64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  RuntimeConfig config_;
  RuntimeMode current_mode_;
  ModePermissions current_permissions_;

  Accountability::AuditTrail trail_;
  Accountability::AccountabilityEngine engine_;

  ExplorationCallback exploration_callback_;
  ConsolidationCallback consolidation_callback_;
  ReflectionCallback reflection_callback_;
};

} // namespace Runtime
} // namespace NeuroForge
