/**
 * @file language_learning_demo.cpp
 * @brief Demo: NeuroForge learns language from sample text and persists
 * vocabulary
 *
 * Usage: ./language_learning_demo [db_path] [text_file]
 *
 * This demo:
 * 1. Initializes NeuroForge's language learning system
 * 2. Feeds sample text for vocabulary acquisition
 * 3. Persists learned vocabulary to SQLite database
 * 4. Shows learning statistics
 */

#include "core/LanguageSystem.h"
#include "core/MemoryDB.h"
#include "language/LanguageAcquisitionLoop.h"
#include "perception/LivePerceptionLoop.h"


#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace NeuroForge;

// Sample educational content for language learning
const std::vector<std::pair<std::string, std::string>> SAMPLE_CONTENT = {
    {"https://en.wikipedia.org/wiki/Mammal",
     "A mammal is a warm-blooded vertebrate animal. Mammals have fur or hair. "
     "They feed their young with milk. Dogs and cats are mammals. Elephants "
     "are "
     "the largest land mammals. Whales are marine mammals that live in the "
     "ocean. "
     "Humans are also mammals. Mammals evolved from reptiles millions of years "
     "ago."},

    {"https://en.wikipedia.org/wiki/Science",
     "Science is the systematic study of nature and the universe. Scientists "
     "use "
     "observation and experiments to understand the world. Physics studies "
     "matter "
     "and energy. Chemistry studies atoms and molecules. Biology studies "
     "living "
     "organisms. Astronomy studies stars and galaxies. Science helps us solve "
     "problems and create new technologies."},

    {"https://en.wikipedia.org/wiki/Animal",
     "An animal is a living organism that can move and respond to its "
     "environment. "
     "Animals breathe oxygen and need food for energy. Vertebrates have "
     "backbones "
     "while invertebrates do not. Fish live in water and breathe through "
     "gills. "
     "Birds have feathers and most can fly. Reptiles are cold-blooded and have "
     "scales. "
     "Insects have six legs and are the most diverse group of animals."},

    {"https://simple.wikipedia.org/wiki/Planet",
     "A planet is a large object that orbits a star. Earth is the third planet "
     "from "
     "the Sun. Mars is called the Red Planet because of its color. Jupiter is "
     "the "
     "largest planet in our solar system. Saturn has beautiful rings made of "
     "ice. "
     "Venus is the hottest planet. Mercury is closest to the Sun."},

    {"https://en.wikipedia.org/wiki/Water",
     "Water is essential for all life on Earth. Water molecules contain "
     "hydrogen and "
     "oxygen atoms. Water can exist as liquid, solid ice, or water vapor. The "
     "ocean "
     "covers most of Earth's surface. Fresh water is found in rivers, lakes, "
     "and "
     "underground. Plants and animals need water to survive. The water cycle "
     "moves "
     "water between the ocean, atmosphere, and land."}};

std::int64_t getCurrentTimestamp() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

void persistVocabulary(Core::MemoryDB &db, std::int64_t run_id,
                       Language::LanguageAcquisitionLoop &acquisition) {
  auto vocab = acquisition.getLearnedVocabulary();
  auto ts = getCurrentTimestamp();

  std::cout << "\n[Persistence] Saving " << vocab.size()
            << " tokens to database...\n";

  int saved = 0;
  std::int64_t out_id;
  for (const auto &token : vocab) {
    float confidence = acquisition.getTokenConfidence(token);

    // Use insertLanguageGroundingMap to store each token
    std::string state_json = "{\"type\": \"vocabulary\", \"confidence\": " +
                             std::to_string(confidence) + "}";

    if (db.insertLanguageGroundingMap(run_id, ts, 0, 5, token, state_json,
                                      confidence, std::nullopt, "web_learning",
                                      out_id)) {
      saved++;
    }
  }

  // Log learning stats as a run event
  auto stats = acquisition.getStatistics();
  std::string event_msg =
      "Learned " + std::to_string(stats.tokens_learned) +
      " tokens, vocabulary size: " + std::to_string(stats.vocabulary_size);
  db.insertRunEvent(run_id, ts, 0, "language_learning", event_msg, 0, 0.0, 0.0,
                    out_id);

  std::cout << "[Persistence] Saved " << saved << " tokens successfully!\n";
}

void printLearningStats(Language::LanguageAcquisitionLoop &acquisition) {
  auto stats = acquisition.getStatistics();

  std::cout << "\n";
  std::cout << "+------------------------------------------------------+\n";
  std::cout << "|           LANGUAGE LEARNING STATISTICS               |\n";
  std::cout << "+------------------------------------------------------+\n";
  std::cout << "| Pages Processed:       " << std::setw(10)
            << stats.pages_processed << "                 |\n";
  std::cout << "| Tokens Learned:        " << std::setw(10)
            << stats.tokens_learned << "                 |\n";
  std::cout << "| Vocabulary Size:       " << std::setw(10)
            << stats.vocabulary_size << "                 |\n";
  std::cout << "| Relations Formed:      " << std::setw(10)
            << stats.relations_formed << "                 |\n";
  std::cout << "| Avg Token Confidence:  " << std::setw(10) << std::fixed
            << std::setprecision(2) << stats.avg_token_confidence
            << "                 |\n";
  std::cout << "+------------------------------------------------------+\n";
}

void printLearnedVocabulary(Language::LanguageAcquisitionLoop &acquisition,
                            int max_show = 30) {
  auto vocab = acquisition.getLearnedVocabulary();

  std::cout << "\n=== LEARNED VOCABULARY (top " << max_show
            << " tokens) ===\n\n";

  // Collect tokens with confidence
  std::vector<std::pair<std::string, float>> scored_vocab;
  for (const auto &token : vocab) {
    scored_vocab.emplace_back(token, acquisition.getTokenConfidence(token));
  }

  // Sort by confidence descending
  std::sort(scored_vocab.begin(), scored_vocab.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });

  int count = 0;
  for (const auto &[token, confidence] : scored_vocab) {
    if (count >= max_show)
      break;
    std::cout << "  [" << std::fixed << std::setprecision(2) << confidence
              << "] " << token << "\n";
    count++;
  }

  if (vocab.size() > static_cast<size_t>(max_show)) {
    std::cout << "  ... and " << (vocab.size() - max_show) << " more tokens\n";
  }
}

int main(int argc, char *argv[]) {
  std::string db_path = "neuroforge_vocabulary.db";

  if (argc > 1) {
    db_path = argv[1];
  }

  std::cout << "\n";
  std::cout
      << "+==============================================================+\n";
  std::cout
      << "|     NEUROFORGE LANGUAGE LEARNING DEMO                        |\n";
  std::cout
      << "|     Autonomous Language Acquisition from Web Content         |\n";
  std::cout
      << "+==============================================================+\n\n";

  // Initialize persistence
  std::cout << "[Init] Opening database: " << db_path << "\n";
  Core::MemoryDB db(db_path);
  if (!db.open()) {
    std::cerr << "[Error] Failed to open database\n";
    return 1;
  }

  // Create a new run
  std::int64_t run_id = 0;
  if (!db.beginRun("{\"demo\": \"language_learning\", \"version\": \"1.0\"}",
                   run_id)) {
    std::cerr << "[Error] Failed to create run\n";
    return 1;
  }
  std::cout << "[Init] Created run ID: " << run_id << "\n";

  // Initialize LanguageSystem
  std::cout << "[Init] Initializing Language System...\n";
  Core::LanguageSystem::Config lang_config;
  Core::LanguageSystem language_system(lang_config);
  if (!language_system.initialize()) {
    std::cerr << "[Error] Failed to initialize LanguageSystem\n";
    return 1;
  }

  // Initialize Perception (minimal, without browser)
  std::cout << "[Init] Initializing Perception Pipeline...\n";
  Perception::PerceptionConfig perc_config;
  Perception::LivePerceptionLoop perception(&language_system, nullptr,
                                            perc_config);
  if (!perception.initialize()) {
    std::cerr << "[Error] Failed to initialize Perception\n";
    return 1;
  }

  // Initialize LanguageAcquisitionLoop
  std::cout << "[Init] Initializing Language Acquisition Loop...\n";
  Language::AcquisitionConfig acq_config;
  acq_config.min_occurrences_to_learn = 1; // Learn immediately for demo
  acq_config.min_token_confidence = 0.3f;
  Language::LanguageAcquisitionLoop acquisition(&language_system, &perception,
                                                nullptr, nullptr, acq_config);
  if (!acquisition.initialize()) {
    std::cerr << "[Error] Failed to initialize LanguageAcquisitionLoop\n";
    return 1;
  }

  std::cout << "\n[Learning] Starting language acquisition...\n\n";
  acquisition.start();

  // Process sample content
  for (const auto &[url, text] : SAMPLE_CONTENT) {
    std::cout << "[Processing] " << url << "\n";
    acquisition.processWebContent(text, url);
  }

  // If user provided a text file, also process it
  if (argc > 2) {
    std::string text_file = argv[2];
    std::cout << "[Processing] Loading custom text from: " << text_file << "\n";

    std::ifstream file(text_file);
    if (file.is_open()) {
      std::stringstream buffer;
      buffer << file.rdbuf();
      acquisition.processWebContent(buffer.str(), "file://" + text_file);
    } else {
      std::cerr << "[Warning] Could not open text file: " << text_file << "\n";
    }
  }

  acquisition.stop();

  // Print results
  printLearningStats(acquisition);
  printLearnedVocabulary(acquisition);

  // Persist vocabulary to database
  persistVocabulary(db, run_id, acquisition);

  db.close();

  std::cout << "\n=== DEMO COMPLETE ===\n";
  std::cout << "Vocabulary persisted to: " << db_path << "\n";
  std::cout << "You can run again to add more vocabulary!\n\n";

  return 0;
}
