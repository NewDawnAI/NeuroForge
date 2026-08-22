// Phase 24 Validation Test Runner
// Run with: cl /EHsc /std:c++17 /I..\..\include test_phase24_validation.cpp

#include "norms/Phase24ValidationRunner.h"

int main() {
  NeuroForge::Tests::Phase24ValidationRunner::runValidation();
  return 0;
}
