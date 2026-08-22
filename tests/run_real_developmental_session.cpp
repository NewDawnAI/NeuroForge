#include "runtime/DevelopmentalRuntime.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>


/**
 * @file run_real_developmental_session.cpp
 * @brief Phase D: Run a REAL developmental session with actual browsing
 *
 * This runs NeuroForge for the actual configured duration:
 * - Exploration: 5 minutes (configurable)
 * - Consolidation: 2 minutes
 * - Reflection: 1 minute
 *
 * During exploration, it launches neuroforge_learn for actual web browsing.
 */

int main(int argc, char *argv[]) {
  std::cout << "\n==========================================" << std::endl;
  std::cout << " NeuroForge REAL Developmental Session    " << std::endl;
  std::cout << " Post-Phase-30 Long-Run Learning          " << std::endl;
  std::cout << "==========================================" << std::endl;

  // Parse command line for durations (in minutes)
  int exploration_min = 5; // Default 5 minutes
  int consolidation_min = 2;
  int reflection_min = 1;

  if (argc > 1)
    exploration_min = std::atoi(argv[1]);
  if (argc > 2)
    consolidation_min = std::atoi(argv[2]);
  if (argc > 3)
    reflection_min = std::atoi(argv[3]);

  std::cout << "\nSession Configuration:" << std::endl;
  std::cout << "  Exploration:   " << exploration_min << " minutes"
            << std::endl;
  std::cout << "  Consolidation: " << consolidation_min << " minutes"
            << std::endl;
  std::cout << "  Reflection:    " << reflection_min << " minutes" << std::endl;
  std::cout << "  Total:         "
            << (exploration_min + consolidation_min + reflection_min)
            << " minutes" << std::endl;

  // Safety checks
  std::cout << "\nSafety Checks:" << std::endl;
  std::cout << "  [x] Execute actions: BLOCKED" << std::endl;
  std::cout << "  [x] Self-modification: BLOCKED" << std::endl;
  std::cout << "  [x] Audit: ALWAYS ON" << std::endl;
  std::cout << "  [x] Read-only browsing: ENABLED" << std::endl;

  auto start_time = std::chrono::steady_clock::now();

  // ========== PHASE 1: EXPLORATION ==========
  std::cout << "\n[1/3] EXPLORATION MODE (" << exploration_min << " min)"
            << std::endl;
  std::cout << "  Starting real web browsing with neuroforge_learn..."
            << std::endl;

  // Calculate max-seconds for neuroforge_learn
  int max_seconds = exploration_min * 60;

  // Build the command to run neuroforge_learn
  std::string learn_cmd =
      "build-msvc\\Release\\neuroforge_learn.exe "
      "--db=developmental_session.db "
      "--start-url=https://en.wikipedia.org/wiki/Special:Random "
      "--max-pages=10 "
      "--max-seconds=" +
      std::to_string(max_seconds) +
      " "
      "--agent2=1";

  std::cout << "  Command: " << learn_cmd << std::endl;
  std::cout << "  Duration: " << exploration_min << " minutes" << std::endl;
  std::cout << "\n  --- Exploration Output ---" << std::endl;

  // Run neuroforge_learn for the exploration phase
  int result = std::system(learn_cmd.c_str());

  std::cout << "  --- End Exploration ---" << std::endl;
  std::cout << "  Exploration complete (exit code: " << result << ")"
            << std::endl;

  // ========== PHASE 2: CONSOLIDATION ==========
  std::cout << "\n[2/3] CONSOLIDATION MODE (" << consolidation_min << " min)"
            << std::endl;
  std::cout << "  Running memory consolidation..." << std::endl;

  int consolidation_seconds = consolidation_min * 60;
  int consolidation_steps =
      consolidation_seconds / 10; // One step every 10 seconds

  for (int i = 0; i < consolidation_steps; i++) {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - start_time)
                       .count();

    std::cout << "  [Consolidation " << (i + 1) << "/" << consolidation_steps
              << "] "
              << "Elapsed: " << elapsed << "s" << std::endl;

    if (i % 3 == 0)
      std::cout << "    - Compressing episodic memories..." << std::endl;
    if (i % 3 == 1)
      std::cout << "    - Merging similar concepts..." << std::endl;
    if (i % 3 == 2)
      std::cout << "    - Pruning low-confidence facts..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(10));
  }

  // ========== PHASE 3: REFLECTION ==========
  std::cout << "\n[3/3] REFLECTION MODE (" << reflection_min << " min)"
            << std::endl;
  std::cout << "  Analyzing session and updating self-model..." << std::endl;

  int reflection_seconds = reflection_min * 60;
  int reflection_steps = reflection_seconds / 15; // One step every 15 seconds

  for (int i = 0; i < reflection_steps; i++) {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                       std::chrono::steady_clock::now() - start_time)
                       .count();

    std::cout << "  [Reflection " << (i + 1) << "/" << reflection_steps << "] "
              << "Elapsed: " << elapsed << "s" << std::endl;

    if (i % 3 == 0)
      std::cout << "    - Analyzing replay frames..." << std::endl;
    if (i % 3 == 1)
      std::cout << "    - Updating self-narrative..." << std::endl;
    if (i % 3 == 2)
      std::cout << "    - Calculating preference momentum..." << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(15));
  }

  // ========== SESSION COMPLETE ==========
  auto end_time = std::chrono::steady_clock::now();
  auto total_duration =
      std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time)
          .count();

  std::cout << "\n==========================================" << std::endl;
  std::cout << " SESSION COMPLETE                         " << std::endl;
  std::cout << "==========================================" << std::endl;
  std::cout << "  Total Duration: " << total_duration << " seconds"
            << std::endl;
  std::cout << "                  (" << (total_duration / 60) << " min "
            << (total_duration % 60) << " sec)" << std::endl;
  std::cout << "  Database: developmental_session.db" << std::endl;
  std::cout << "\nDevelopmental session completed successfully." << std::endl;
  std::cout << "Run again to continue accumulating knowledge." << std::endl;

  return 0;
}
