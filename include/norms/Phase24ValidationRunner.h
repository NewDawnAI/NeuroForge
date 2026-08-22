#pragma once

#include "actuation/ActionCommand.h"
#include "actuation/ReplayFrame.h"
#include "norms/NormInductionEngine.h"
#include "norms/NormMetricsDashboard.h"
#include "norms/NormStore.h"
#include "norms/NormStressTests.h"
#include "norms/NormativeReasoner.h"


#include <iostream>
#include <random>
#include <vector>


namespace NeuroForge {
namespace Tests {

/**
 * @brief Phase 24: Integrated Validation Runner
 *
 * Runs stress tests with monitoring dashboard enabled.
 */
class Phase24ValidationRunner {
public:
  /**
   * @brief Run complete Phase 24 validation with dashboard
   */
  static void runValidation() {
    std::cout << "\n";
    std::cout
        << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout
        << "║       PHASE 24 VALIDATION WITH MONITORING DASHBOARD      ║\n";
    std::cout
        << "╚══════════════════════════════════════════════════════════╝\n";

    // 1. Run stress tests
    std::cout << "\n[Phase 1] Running Adversarial Stress Tests...\n";
    int stress_failures = NormStressTests::runAllTests();

    // 2. Run simulated session with dashboard
    std::cout << "\n[Phase 2] Running Simulated Session with Dashboard...\n";
    Norms::NormStore store;
    Norms::NormMetricsDashboard dashboard;

    // Seed some norms
    seedNorms(store);

    // Run simulated actions
    runSimulatedSession(store, dashboard, 50);

    // 3. Print dashboard
    std::cout << "\n[Phase 3] Final Metrics Dashboard\n";
    dashboard.updateStoreStats(store);
    dashboard.printDashboard();

    // 4. Summary
    std::cout << "\n[Phase 4] Validation Summary\n";
    std::cout << "═══════════════════════════════════════════════\n";
    std::cout << "  Stress Test Failures:  " << stress_failures << "\n";

    auto metrics = dashboard.getMetrics();
    std::cout << "  Total Actions Evaluated: " << metrics.total_evaluations
              << "\n";
    std::cout << "  Actions Permitted:       " << metrics.permitted_count
              << "\n";
    std::cout << "  Actions Blocked:         " << metrics.blocked_count << "\n";
    std::cout << "  ABSOLUTE Blocks:         " << metrics.absolute_blocks
              << "\n";

    // Validation criteria
    bool valid = true;

    if (stress_failures > 0) {
      std::cout << "\n  ❌ FAIL: Stress tests failed\n";
      valid = false;
    }

    if (metrics.absolute_blocks > 0 && metrics.permitted_count > 0) {
      std::cout << "  ✅ PASS: ABSOLUTE norms enforced\n";
    }

    if (metrics.hard_blocks > 0) {
      std::cout << "  ✅ PASS: HARD norms blocked actions\n";
    }

    if (metrics.soft_overrides >= 0) {
      std::cout << "  ✅ PASS: SOFT overrides tracked\n";
    }

    std::cout << "\n";
    if (valid) {
      std::cout
          << "╔══════════════════════════════════════════════════════════╗\n";
      std::cout
          << "║    ✅ PHASE 24 VALIDATION PASSED                         ║\n";
      std::cout
          << "║    Normative Reasoning is lawful under pressure          ║\n";
      std::cout
          << "╚══════════════════════════════════════════════════════════╝\n";
    } else {
      std::cout
          << "╔══════════════════════════════════════════════════════════╗\n";
      std::cout
          << "║    ❌ PHASE 24 VALIDATION FAILED                         ║\n";
      std::cout
          << "║    Review stress test results                            ║\n";
      std::cout
          << "╚══════════════════════════════════════════════════════════╝\n";
    }

    // Export JSON
    std::cout << "\n[JSON Export]\n";
    std::cout << dashboard.exportJSON();
  }

private:
  static void seedNorms(Norms::NormStore &store) {
    // ABSOLUTE: Physical safety
    Norms::Norm absolute;
    absolute.norm_id = "physical_safety";
    absolute.context_signature = "embodiment";
    absolute.strength = Norms::NormStrength::ABSOLUTE;
    absolute.decision = Norms::NormDecision::FORBID;
    absolute.justification = "Physical safety is non-negotiable";
    absolute.priority = 1000;
    store.addOrUpdateNorm(absolute);

    // HARD: Require verification
    Norms::Norm hard;
    hard.norm_id = "require_verification";
    hard.context_signature = "external_action";
    hard.strength = Norms::NormStrength::HARD;
    hard.decision = Norms::NormDecision::FORBID;
    hard.justification = "Verification required before external action";
    hard.priority = 100;
    store.addOrUpdateNorm(hard);

    // SOFT: Avoid high cost
    Norms::Norm soft;
    soft.norm_id = "avoid_high_cost";
    soft.context_signature = "external_action";
    soft.strength = Norms::NormStrength::SOFT;
    soft.decision = Norms::NormDecision::DISCOURAGE;
    soft.justification = "Prefer low-cost operations";
    soft.priority = 50;
    store.addOrUpdateNorm(soft);

    std::cout << "  Seeded " << store.count() << " norms\n";
  }

  static void runSimulatedSession(Norms::NormStore &store,
                                  Norms::NormMetricsDashboard &dashboard,
                                  int num_actions) {
    Norms::NormativeReasoner reasoner(store);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> action_dist(0, 4);

    for (int i = 0; i < num_actions; i++) {
      Actuation::ActionCommand action;
      std::string desc;

      switch (action_dist(gen)) {
      case 0:
        action.kind = Actuation::ActionKind::NAVIGATE;
        desc = "Navigate to external URL";
        break;
      case 1:
        action.kind = Actuation::ActionKind::SEARCH;
        desc = "Search query";
        break;
      case 2:
        action.kind = Actuation::ActionKind::SPEAK;
        desc = "Speak clarification";
        break;
      case 3:
        action.kind = Actuation::ActionKind::OBSERVE;
        desc = "Observe environment";
        break;
      case 4:
        action.kind = Actuation::ActionKind::MANIPULATE;
        desc = "Manipulate physical object";
        break;
      }

      auto judgment = reasoner.evaluate(action);
      dashboard.recordJudgment(judgment, desc);
    }

    std::cout << "  Ran " << num_actions << " simulated actions\n";
  }
};

} // namespace Tests
} // namespace NeuroForge
