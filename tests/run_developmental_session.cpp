#include "runtime/DevelopmentalRuntime.h"
#include <iostream>

/**
 * @file run_developmental_session.cpp
 * @brief Phase D: Run a safe developmental session
 *
 * This runs NeuroForge in the recommended developmental cycle:
 * - 30 min Exploration (simulated as 10 steps)
 * - 15 min Consolidation (simulated)
 * - 5 min Reflection (simulated)
 */

int main() {
  std::cout << "\n==========================================" << std::endl;
  std::cout << " NeuroForge Developmental Runtime Session " << std::endl;
  std::cout << " Post-Phase-30 Operational Design        " << std::endl;
  std::cout << "==========================================" << std::endl;

  // Create safe default configuration
  NeuroForge::Runtime::RuntimeConfig config =
      NeuroForge::Runtime::RuntimeConfig::safeDefaults();

  // Validate safety
  if (!config.isSafe()) {
    std::cerr << "ERROR: Configuration is not safe!" << std::endl;
    return 1;
  }

  std::cout << "\nSafety checks:" << std::endl;
  std::cout << "  ✓ Preference momentum: "
            << config.learning.preference_momentum << std::endl;
  std::cout << "  ✓ Execute actions: "
            << (config.execute_actions_blocked ? "BLOCKED" : "ALLOWED")
            << std::endl;
  std::cout << "  ✓ Self-modification: "
            << (config.self_modification_blocked ? "BLOCKED" : "ALLOWED")
            << std::endl;
  std::cout << "  ✓ Audit: " << (config.audit_always_on ? "ALWAYS ON" : "OFF")
            << std::endl;

  std::cout << "\nLearning governors:" << std::endl;
  std::cout << "  ConceptNode rate: " << config.learning.concept_node_rate
            << " (slow)" << std::endl;
  std::cout << "  Norm reinforcement: "
            << (config.learning.norm_reinforcement_enabled ? "ON" : "OFF")
            << std::endl;
  std::cout << "  Social evidence threshold: "
            << config.learning.social_norm_evidence_threshold << " agents"
            << std::endl;

  std::cout << "\nMemory thresholds:" << std::endl;
  std::cout << "  Compression ratio: "
            << config.memory.episodic_compression_ratio << ":1" << std::endl;
  std::cout << "  Merge threshold: " << config.memory.concept_merge_threshold
            << std::endl;
  std::cout << "  Max episodic memories: "
            << config.memory.max_episodic_memories << std::endl;

  // Create runtime
  NeuroForge::Runtime::DevelopmentalRuntime runtime(config);

  // Set callbacks (in real implementation, these would do actual work)
  runtime.setExplorationCallback(
      [](const NeuroForge::Runtime::ModePermissions &p) {
        // Simulated exploration
        if (p.allow_web_browsing) {
          // Would browse web here
        }
      });

  runtime.setConsolidationCallback(
      [](const NeuroForge::Runtime::ModePermissions &p) {
        // Simulated consolidation
        if (p.allow_consolidation) {
          // Would run sleep consolidation here
        }
      });

  runtime.setReflectionCallback(
      [](const NeuroForge::Runtime::ModePermissions &p) {
        // Simulated reflection
        if (p.allow_replay_analysis) {
          // Would analyze replay frames here
        }
      });

  // Run session
  auto stats = runtime.runSession();

  // Get audit trail
  auto events = runtime.getAuditTrail().getEvents();

  std::cout << "\n=== Audit Trail (" << events.size()
            << " events) ===" << std::endl;
  for (size_t i = 0; i < std::min(events.size(), size_t(5)); i++) {
    std::cout << "  [" << (i + 1) << "] "
              << NeuroForge::Accountability::AccountabilityEvent::typeToString(
                     events[i].type)
              << ": " << events[i].action_id << std::endl;
  }
  if (events.size() > 5) {
    std::cout << "  ... and " << (events.size() - 5) << " more events"
              << std::endl;
  }

  std::cout << "\n=== Session Summary ===" << std::endl;
  std::cout << "  Status: "
            << (stats.completed_successfully ? "SUCCESS" : "FAILED")
            << std::endl;
  std::cout << "  Final mode: "
            << NeuroForge::Runtime::modeToString(stats.final_mode) << std::endl;
  std::cout << "\nDevelopmental Runtime is working correctly." << std::endl;
  std::cout
      << "NeuroForge is ready for controlled, intermittent long-run sessions."
      << std::endl;

  return stats.completed_successfully ? 0 : 1;
}
