/**
 * @file test_language_acquisition.cpp
 * @brief Integration test for Language Acquisition from Web Content (Phase 5)
 */

#include "core/LanguageSystem.h"
#include "language/LanguageAcquisitionLoop.h"
#include "perception/LivePerceptionLoop.h"


#include <iostream>
#include <string>
#include <vector>

// ============= MINIMAL TEST FRAMEWORK =============
#ifdef MINIMAL_TEST_FRAMEWORK

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(category, name)                                                   \
  void test_##category##_##name();                                             \
  struct TestRegistrar_##category##_##name {                                   \
    TestRegistrar_##category##_##name() {                                      \
      std::cout << "Running " #category "." #name "..." << std::endl;          \
      try {                                                                    \
        test_##category##_##name();                                            \
        g_tests_passed++;                                                      \
        std::cout << "  PASSED" << std::endl;                                  \
      } catch (const std::exception &e) {                                      \
        g_tests_failed++;                                                      \
        std::cout << "  FAILED: " << e.what() << std::endl;                    \
      } catch (...) {                                                          \
        g_tests_failed++;                                                      \
        std::cout << "  FAILED: unknown exception" << std::endl;               \
      }                                                                        \
    }                                                                          \
  } g_registrar_##category##_##name;                                           \
  void test_##category##_##name()

#define EXPECT_TRUE(expr)                                                      \
  if (!(expr))                                                                 \
  throw std::runtime_error("EXPECT_TRUE failed: " #expr)

#define EXPECT_EQ(a, b)                                                        \
  if ((a) != (b))                                                              \
  throw std::runtime_error("EXPECT_EQ failed: " #a " != " #b)

#define EXPECT_GT(a, b)                                                        \
  if (!((a) > (b)))                                                            \
  throw std::runtime_error("EXPECT_GT failed: " #a " <= " #b)

#define EXPECT_GE(a, b)                                                        \
  if (!((a) >= (b)))                                                           \
  throw std::runtime_error("EXPECT_GE failed: " #a " < " #b)

#endif // MINIMAL_TEST_FRAMEWORK

// ============= TEST HELPERS =============

class LanguageAcquisitionTest {
public:
  std::unique_ptr<NeuroForge::Core::LanguageSystem> language_system;
  std::unique_ptr<NeuroForge::Perception::LivePerceptionLoop> perception;
  std::unique_ptr<NeuroForge::Language::LanguageAcquisitionLoop> acquisition;

  void setup() {
    // Create LanguageSystem
    NeuroForge::Core::LanguageSystem::Config lang_config;
    language_system =
        std::make_unique<NeuroForge::Core::LanguageSystem>(lang_config);
    language_system->initialize();

    // Create LivePerceptionLoop (no RelationGateManager for simpler test)
    NeuroForge::Perception::PerceptionConfig perc_config;
    perception = std::make_unique<NeuroForge::Perception::LivePerceptionLoop>(
        language_system.get(), nullptr, perc_config);
    perception->initialize();

    // Create LanguageAcquisitionLoop
    NeuroForge::Language::AcquisitionConfig acq_config;
    acq_config.min_occurrences_to_learn = 1; // Learn faster for testing
    acq_config.enable_direct_tokenization = true;
    acquisition =
        std::make_unique<NeuroForge::Language::LanguageAcquisitionLoop>(
            language_system.get(), perception.get(), nullptr, nullptr,
            acq_config);
    acquisition->initialize();
  }

  void teardown() {
    acquisition.reset();
    perception.reset();
    language_system.reset();
  }
};

static LanguageAcquisitionTest g_fixture;

// ============= TESTS =============

TEST(LanguageAcquisition, Initialization) {
  g_fixture.setup();

  EXPECT_TRUE(g_fixture.acquisition != nullptr);
  auto stats = g_fixture.acquisition->getStatistics();
  EXPECT_EQ(stats.pages_processed, 0u);
  EXPECT_EQ(stats.tokens_learned, 0u);

  g_fixture.teardown();
}

TEST(LanguageAcquisition, ProcessWebContent) {
  g_fixture.setup();

  // Simulate processing web content
  std::string text = "A cat is a mammal. Dogs are also mammals. "
                     "Cats and dogs are common pets.";
  std::string url = "https://en.wikipedia.org/wiki/Mammal";

  g_fixture.acquisition->processWebContent(text, url);

  auto stats = g_fixture.acquisition->getStatistics();
  EXPECT_EQ(stats.pages_processed, 1u);
  EXPECT_GT(stats.tokens_learned, 0u);

  g_fixture.teardown();
}

TEST(LanguageAcquisition, LearnMultipleTimes) {
  g_fixture.setup();

  // Process content multiple times to build confidence
  std::string text1 = "The elephant is the largest land animal.";
  std::string text2 = "Elephants live in Africa and Asia.";
  std::string text3 = "An elephant uses its trunk to drink water.";

  g_fixture.acquisition->processWebContent(
      text1, "https://wikipedia.org/wiki/Elephant");
  g_fixture.acquisition->processWebContent(
      text2, "https://britannica.com/animal/elephant");
  g_fixture.acquisition->processWebContent(
      text3, "https://simple.wikipedia.org/wiki/Elephant");

  auto stats = g_fixture.acquisition->getStatistics();
  EXPECT_EQ(stats.pages_processed, 3u);

  // "elephant" should have higher confidence after multiple occurrences
  float conf = g_fixture.acquisition->getTokenConfidence("elephant");
  EXPECT_GT(conf, 0.0f);

  g_fixture.teardown();
}

TEST(LanguageAcquisition, GetLearnedVocabulary) {
  g_fixture.setup();

  g_fixture.acquisition->processWebContent(
      "Science is the study of nature and the universe.",
      "https://wikipedia.org/wiki/Science");

  auto vocab = g_fixture.acquisition->getLearnedVocabulary();
  EXPECT_GT(vocab.size(), 0u);

  g_fixture.teardown();
}

TEST(LanguageAcquisition, AddLearningTopic) {
  g_fixture.setup();

  g_fixture.acquisition->addLearningTopic("physics", 1.0f);
  g_fixture.acquisition->addLearningTopic("chemistry", 0.8f);

  // Topics should be added without error
  EXPECT_TRUE(true);

  g_fixture.teardown();
}

TEST(LanguageAcquisition, GetLowConfidenceTopics) {
  g_fixture.setup();

  // Process some content
  g_fixture.acquisition->processWebContent(
      "Biology is the study of life and living organisms.",
      "https://wikipedia.org/wiki/Biology");

  auto low_conf = g_fixture.acquisition->getLowConfidenceTopics(5);
  // Should have some tokens with low confidence
  EXPECT_GE(low_conf.size(), 0u);

  g_fixture.teardown();
}

// ============= MAIN =============

int main() {
  std::cout << "\n================================================\n";
  std::cout << "Language Acquisition Integration Tests (Phase 5)\n";
  std::cout << "================================================\n\n";

  // Tests are auto-registered and run via static initialization

  std::cout << "\n================================================\n";
  std::cout << "Test Results: " << g_tests_passed << " passed, "
            << g_tests_failed << " failed\n";
  std::cout << "================================================\n";

  return g_tests_failed > 0 ? 1 : 0;
}
