#include "accountability/Phase30StressTests.h"
#include <iostream>

int main() {
  std::cout << "\n=== NeuroForge Phase 30 Adversarial Stress Tests ==="
            << std::endl;
  std::cout << "Testing: Institutional Accountability\n" << std::endl;

  NeuroForge::Tests::Phase30StressTests tests;
  auto results = tests.runAll();

  NeuroForge::Tests::Phase30StressTests::printResults(results);

  // Print details
  std::cout << "\nDetails:\n";
  for (const auto &r : results) {
    std::cout << "\n[" << (r.passed ? "PASS" : "FAIL") << "] " << r.test_name
              << std::endl;
    std::cout << "  Expected:  " << r.expected << std::endl;
    std::cout << "  Actual:    " << r.actual << std::endl;
    std::cout << "  Invariant: " << r.invariant << std::endl;
  }

  // Count results
  int passed = 0, failed = 0;
  for (const auto &r : results) {
    if (r.passed)
      passed++;
    else
      failed++;
  }

  return failed > 0 ? 1 : 0;
}
