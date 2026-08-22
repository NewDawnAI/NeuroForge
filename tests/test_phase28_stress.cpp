#include "roles/Phase28StressTests.h"
#include <iostream>

int main() {
  std::cout << "\n=== NeuroForge Phase 28 Adversarial Stress Tests ===\n"
            << std::endl;

  NeuroForge::Tests::Phase28StressTests tests;
  auto results = tests.runAll();

  NeuroForge::Tests::Phase28StressTests::printResults(results);

  // Count results
  int passed = 0, failed = 0;
  for (const auto &r : results) {
    if (r.passed)
      passed++;
    else
      failed++;
  }

  std::cout << "\nDetails:\n";
  for (const auto &r : results) {
    std::cout << "\n[" << (r.passed ? "PASS" : "FAIL") << "] " << r.test_name
              << std::endl;
    std::cout << "  Expected: " << r.expected << std::endl;
    std::cout << "  Actual:   " << r.actual << std::endl;
    std::cout << "  Detail:   " << r.details << std::endl;
  }

  return failed > 0 ? 1 : 0;
}
