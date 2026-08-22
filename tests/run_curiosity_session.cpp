#include "expression/LanguageExpressionCortex.h"
#include "navigation/CuriosityFirstConfig.h"
#include "navigation/PreferenceEvolutionLogger.h"
#include "runtime/DevelopmentalRuntime.h"
#include "speech/SpeechSynthesizer.h"


#include <chrono>
#include <cstdlib>
#include <iostream>
#include <regex>
#include <thread>


/**
 * @file run_curiosity_session.cpp
 * @brief Curiosity-First Autonomous Browsing Session
 *
 * Key differences from seed-first:
 * 1. Seeds = fallback only (1-3 max, Special:Random)
 * 2. CuriosityNavigator = primary driver
 * 3. Guardrails enabled (domain diversity, topic penalty)
 * 4. Preference evolution logged
 */

// Extract domain from URL
std::string extractDomain(const std::string &url) {
  std::regex domain_regex(R"(https?://([^/]+))");
  std::smatch match;
  if (std::regex_search(url, match, domain_regex)) {
    return match[1].str();
  }
  return "unknown";
}

// Extract topic from title
std::string extractTopic(const std::string &title) {
  // Simple: use first 3 words or title up to " - "
  size_t dash = title.find(" - ");
  std::string topic = dash != std::string::npos ? title.substr(0, dash) : title;
  if (topic.length() > 50)
    topic = topic.substr(0, 50);
  return topic;
}

int main(int argc, char *argv[]) {
  std::cout << "\n==========================================" << std::endl;
  std::cout << " NeuroForge CURIOSITY-FIRST Session       " << std::endl;
  std::cout << " Fully Autonomous Browsing                " << std::endl;
  std::cout << "==========================================" << std::endl;

  // Parse command line
  int max_pages = 20;
  int max_minutes = 10;
  std::string mode = "default";

  if (argc > 1)
    max_pages = std::atoi(argv[1]);
  if (argc > 2)
    max_minutes = std::atoi(argv[2]);
  if (argc > 3)
    mode = argv[3];

  // Load config
  NeuroForge::Navigation::CuriosityFirstConfig config;
  if (mode == "aggressive") {
    config = NeuroForge::Navigation::CuriosityFirstConfig::aggressive();
  } else if (mode == "conservative") {
    config = NeuroForge::Navigation::CuriosityFirstConfig::conservative();
  } else {
    config = NeuroForge::Navigation::CuriosityFirstConfig::defaultConfig();
  }

  config.max_pages = max_pages;
  config.max_seconds = max_minutes * 60;

  std::cout << "\nConfiguration:" << std::endl;
  std::cout << "  Mode: " << mode << std::endl;
  std::cout << "  Max Pages: " << config.max_pages << std::endl;
  std::cout << "  Max Time: " << max_minutes << " minutes" << std::endl;
  std::cout << "  Seeds as Fallback: "
            << (config.seeds_as_fallback ? "YES" : "NO") << std::endl;
  std::cout << "  Novelty Weight: " << config.novelty_weight << std::endl;
  std::cout << "  Uncertainty Weight: " << config.uncertainty_weight
            << std::endl;
  std::cout << "  Knowledge Gain Weight: " << config.knowledge_gain_weight
            << std::endl;
  std::cout << "  Domain Diversity: "
            << (config.enable_domain_diversity ? "ON" : "OFF") << std::endl;
  std::cout << "  Topic Penalty: "
            << (config.enable_topic_penalty ? "ON" : "OFF") << std::endl;

  // Initialize preference logger
  NeuroForge::Navigation::PreferenceEvolutionLogger pref_logger;

  // Initialize LEC
  NeuroForge::Expression::LanguageExpressionCortex lec;
  lec.registerVocabulary(1, "curiosity", 0.9f);
  lec.registerVocabulary(2, "exploration", 0.9f);
  lec.registerVocabulary(3, "discovery", 0.9f);

  // Initialize Speech
  NeuroForge::Speech::SpeechSynthesizer speech;
  speech.setMode(NeuroForge::Speech::SpeechMode::TEXT_ONLY);

  auto start_time = std::chrono::steady_clock::now();

  std::cout << "\n[Init] Starting curiosity-first exploration..." << std::endl;

  // Express exploration intent
  auto explore_intent = NeuroForge::Expression::ExpressionIntent::describe(1);
  auto explore_plan = lec.express(explore_intent);
  speech.speak(explore_plan);

  // Build command with curiosity-first settings
  // Key: Start with just 1 seed (Special:Random) and let curiosity take over
  std::string learn_cmd =
      "build-msvc\\Release\\neuroforge_learn.exe "
      "--db=curiosity_session.db "
      "--start-url=https://en.wikipedia.org/wiki/Special:Random "
      "--max-pages=" +
      std::to_string(config.max_pages) +
      " "
      "--max-seconds=" +
      std::to_string(config.max_seconds) +
      " "
      "--agent2=1 "
      "--curiosity-first=1"; // Enable curiosity-first mode

  std::cout << "\n[Exploration] Command: " << learn_cmd << std::endl;
  std::cout << "[Exploration] Curiosity-First Mode ENABLED" << std::endl;
  std::cout << "[Exploration] Starting from: Special:Random" << std::endl;

  // Run exploration
  int result = std::system(learn_cmd.c_str());

  if (result == 0) {
    std::cout << "\n[Exploration] Session completed successfully" << std::endl;
  } else {
    std::cout << "\n[Exploration] Session ended with code: " << result
              << std::endl;
  }

  // Session complete
  auto end_time = std::chrono::steady_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time)
          .count();

  std::cout << "\n==========================================" << std::endl;
  std::cout << " CURIOSITY-FIRST SESSION COMPLETE        " << std::endl;
  std::cout << "==========================================" << std::endl;
  std::cout << "  Duration: " << duration << "s (" << (duration / 60) << "m "
            << (duration % 60) << "s)" << std::endl;
  std::cout << "  Mode: " << mode << std::endl;
  std::cout << "  Database: curiosity_session.db" << std::endl;

  // Print preference summary (would be populated by actual browsing)
  std::cout << "\n[Preference Evolution]" << std::endl;
  std::cout << "  Check curiosity_session.db for topic/domain distribution"
            << std::endl;
  std::cout << "  Run: sqlite3 curiosity_session.db \"SELECT * FROM "
               "language_grounding_map LIMIT 10;\""
            << std::endl;

  return 0;
}
