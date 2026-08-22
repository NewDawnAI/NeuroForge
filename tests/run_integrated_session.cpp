#include "expression/LanguageExpressionCortex.h"
#include "runtime/DevelopmentalRuntime.h"
#include "serialization/CapnProtoSchemas.h"
#include "speech/SpeechSynthesizer.h"
#include "vision/VisionPerceptionCortex.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>


/**
 * @file run_integrated_session.cpp
 * @brief Integrated Developmental Session with LEC and Vision
 *
 * Combines:
 * - Developmental Runtime (Phase D)
 * - Language Expression Cortex (Phase E1)
 * - Speech Synthesizer (Phase E2)
 * - Vision Perception (Phase E3-E4)
 * - Serialization boundaries
 */

int main(int argc, char *argv[]) {
  std::cout << "\n==========================================" << std::endl;
  std::cout << " NeuroForge Integrated Developmental Session" << std::endl;
  std::cout << " Phases D + E1-E4 Combined                " << std::endl;
  std::cout << "==========================================" << std::endl;

  // Parse command line for durations (in minutes)
  int exploration_min = 5;
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

  // Initialize components
  std::cout << "\n[Init] Initializing subsystems..." << std::endl;

  // Language Expression Cortex
  NeuroForge::Expression::LanguageExpressionCortex lec;
  lec.registerVocabulary(1, "knowledge", 0.9f);
  lec.registerVocabulary(2, "learning", 0.9f);
  lec.registerVocabulary(3, "exploration", 0.9f);
  lec.registerVocabulary(4, "consolidation", 0.9f);
  lec.registerVocabulary(5, "reflection", 0.9f);
  std::cout << "  [x] Language Expression Cortex: " << lec.vocabularySize()
            << " entries" << std::endl;

  // Speech Synthesizer
  NeuroForge::Speech::SpeechSynthesizer speech;
  speech.setMode(NeuroForge::Speech::SpeechMode::TEXT_ONLY);
  std::cout << "  [x] Speech Synthesizer: TEXT_ONLY mode" << std::endl;

  // Vision Perception Cortex
  NeuroForge::Vision::VisionPerceptionCortex vision;
  std::cout << "  [x] Vision Perception Cortex: Ready" << std::endl;

  // Serialization
  std::cout << "  [x] Serialization Boundaries: Configured" << std::endl;
  std::cout
      << "      - ReplayFrame: "
      << (NeuroForge::Serialization::SerializationPolicy::ALLOW_REPLAY_FRAME
              ? "ALLOWED"
              : "BLOCKED")
      << std::endl;
  std::cout << "      - ConceptNode internals: "
            << (NeuroForge::Serialization::SerializationPolicy::
                        ALLOW_CONCEPT_NODE_INTERNALS
                    ? "ALLOWED"
                    : "BLOCKED")
            << std::endl;

  auto start_time = std::chrono::steady_clock::now();

  // ========== PHASE 1: EXPLORATION ==========
  std::cout << "\n[1/3] EXPLORATION MODE" << std::endl;

  // Express intent to explore
  auto explore_intent = NeuroForge::Expression::ExpressionIntent::describe(3);
  auto explore_plan = lec.express(explore_intent);
  speech.speak(explore_plan);

  // Run neuroforge_learn
  std::cout << "  Starting web browsing..." << std::endl;
  int max_seconds = exploration_min * 60;
  std::string learn_cmd =
      "build-msvc\\Release\\neuroforge_learn.exe "
      "--db=integrated_session.db "
      "--start-url=https://en.wikipedia.org/wiki/Special:Random "
      "--max-pages=10 "
      "--max-seconds=" +
      std::to_string(max_seconds) +
      " "
      "--agent2=1";

  int result = std::system(learn_cmd.c_str());
  std::cout << "  Exploration complete (exit: " << result << ")" << std::endl;

  // ========== PHASE 2: CONSOLIDATION ==========
  std::cout << "\n[2/3] CONSOLIDATION MODE" << std::endl;

  // Express consolidation
  auto consolidate_intent =
      NeuroForge::Expression::ExpressionIntent::describe(4);
  auto consolidate_plan = lec.express(consolidate_intent);
  speech.speak(consolidate_plan);

  int consolidation_steps = (consolidation_min * 60) / 10;
  for (int i = 0; i < consolidation_steps; i++) {
    std::cout << "  [Consolidation " << (i + 1) << "/" << consolidation_steps
              << "]" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(10));
  }

  // ========== PHASE 3: REFLECTION ==========
  std::cout << "\n[3/3] REFLECTION MODE" << std::endl;

  // Express reflection
  auto reflect_intent = NeuroForge::Expression::ExpressionIntent::describe(5);
  auto reflect_plan = lec.express(reflect_intent);
  speech.speak(reflect_plan);

  // Reflect on session
  std::cout << "  Analyzing session..." << std::endl;

  // Create summary expression
  std::vector<std::uint64_t> summary_concepts = {1, 2, 3, 4, 5};
  auto summary_intent = NeuroForge::Expression::ExpressionIntent::explain(
      summary_concepts, "Session summary");
  auto summary_plan = lec.express(summary_intent);

  std::cout << "\n  [Session Summary]" << std::endl;
  speech.speak(summary_plan);

  int reflection_steps = (reflection_min * 60) / 15;
  for (int i = 0; i < reflection_steps; i++) {
    std::cout << "  [Reflection " << (i + 1) << "/" << reflection_steps << "]"
              << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(15));
  }

  // ========== SESSION COMPLETE ==========
  auto end_time = std::chrono::steady_clock::now();
  auto total_duration =
      std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time)
          .count();

  std::cout << "\n==========================================" << std::endl;
  std::cout << " INTEGRATED SESSION COMPLETE             " << std::endl;
  std::cout << "==========================================" << std::endl;
  std::cout << "  Duration: " << total_duration << "s ("
            << (total_duration / 60) << "m " << (total_duration % 60) << "s)"
            << std::endl;
  std::cout << "  LEC Expressions: " << lec.getHistory().size() << std::endl;
  std::cout << "  Speech Outputs: " << speech.speechCount() << std::endl;
  std::cout << "  Database: integrated_session.db" << std::endl;

  // Serialize session summary (demonstration)
  NeuroForge::Serialization::SerializableReplayFrame summary_frame;
  summary_frame.frame_id = 1;
  summary_frame.timestamp_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
  summary_frame.frame_type = "session_summary";
  summary_frame.was_verified = true;

  std::cout << "\n  [Serialized Summary]" << std::endl;
  std::cout << "  "
            << NeuroForge::Serialization::BoundarySerializer::toJson(
                   summary_frame)
            << std::endl;

  return 0;
}
