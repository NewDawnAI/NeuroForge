
#include "connectivity/ConnectivityManager.h"
#include "core/LanguageSystem.h"
#include "core/RelationGate.h"
#include "navigation/CuriosityNavigator.h"
#include "perception/LivePerceptionLoop.h"
#include "verification/GroundingVerifier.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// Minimal Test Framework Macros (adapted from test_autonomous_scheduler.cpp)
#define TEST(test_case, test_name)                                             \
  void test_case##_##test_name();                                              \
  static bool test_case##_##test_name##_registered = []() {                    \
    register_test(#test_case "." #test_name, test_case##_##test_name);         \
    return true;                                                               \
  }();                                                                         \
  void test_case##_##test_name()

#define EXPECT_TRUE(condition)                                                 \
  if (!(condition)) {                                                          \
    std::cerr << "EXPECT_TRUE failed: " << #condition << " at " << __FILE__    \
              << ":" << __LINE__ << std::endl;                                 \
    test_failed = true;                                                        \
  }

#define EXPECT_EQ(expected, actual)                                            \
  if ((expected) != (actual)) {                                                \
    std::cerr << "EXPECT_EQ failed: expected " << (expected) << ", got "       \
              << (actual) << " at " << __FILE__ << ":" << __LINE__             \
              << std::endl;                                                    \
    test_failed = true;                                                        \
  }

#define EXPECT_GT(val1, val2)                                                  \
  if (!((val1) > (val2))) {                                                    \
    std::cerr << "EXPECT_GT failed: " << (val1) << " > " << (val2) << " at "   \
              << __FILE__ << ":" << __LINE__ << std::endl;                     \
    test_failed = true;                                                        \
  }

#define ASSERT_TRUE(condition)                                                 \
  if (!(condition)) {                                                          \
    std::cerr << "ASSERT_TRUE failed: " << #condition << " at " << __FILE__    \
              << ":" << __LINE__ << std::endl;                                 \
    test_failed = true;                                                        \
    return;                                                                    \
  }

#define ASSERT_GT(val1, val2)                                                  \
  if (!((val1) > (val2))) {                                                    \
    std::cerr << "ASSERT_GT failed: " << (val1) << " > " << (val2) << " at "   \
              << __FILE__ << ":" << __LINE__ << std::endl;                     \
    test_failed = true;                                                        \
    return;                                                                    \
  }

static bool test_failed = false;
static std::vector<std::pair<std::string, void (*)()>> test_registry;

void register_test(const std::string &name, void (*func)()) {
  test_registry.push_back({name, func});
}

using namespace NeuroForge;
using namespace NeuroForge::Perception;
using namespace NeuroForge::Navigation;
using namespace NeuroForge::Verification;

// Global instances for the test
static std::shared_ptr<Core::HypergraphBrain> brain;
static std::unique_ptr<Core::LanguageSystem> language_system;
static std::unique_ptr<Core::RelationGateManager> relation_gates;
static std::unique_ptr<LivePerceptionLoop> perception;
static std::unique_ptr<CuriosityNavigator> navigator;
static std::unique_ptr<GroundingVerifier> verifier;

void setup() {
  // Initialize Core Systems
  auto connectivity_manager =
      std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
  brain = std::make_shared<Core::HypergraphBrain>(connectivity_manager);

  Core::LanguageSystem::Config lang_config;
  language_system = std::make_unique<Core::LanguageSystem>(lang_config);
  language_system->initialize();

  // Create basic tokens
  language_system->createToken("cat", Core::LanguageSystem::TokenType::Word);
  language_system->createToken("mammal", Core::LanguageSystem::TokenType::Word);
  language_system->createToken("is_a",
                               Core::LanguageSystem::TokenType::Relation);
  language_system->createToken("animal", Core::LanguageSystem::TokenType::Word);
  language_system->createToken("dog", Core::LanguageSystem::TokenType::Word);
  language_system->createToken("has",
                               Core::LanguageSystem::TokenType::Relation);
  language_system->createToken("fur", Core::LanguageSystem::TokenType::Word);

  relation_gates =
      std::make_unique<Core::RelationGateManager>(brain, language_system.get());

  // Initialize Components
  perception = std::make_unique<LivePerceptionLoop>(language_system.get(),
                                                    relation_gates.get());
  perception->initialize();

  navigator = std::make_unique<CuriosityNavigator>(language_system.get(),
                                                   relation_gates.get());
  navigator->initialize();

  verifier = std::make_unique<GroundingVerifier>(language_system.get(),
                                                 relation_gates.get());
  verifier->initialize();
}

TEST(InternetGrounding, FullLoopSimulation) {
  setup();

  // 1. Setup Phase: Start systems and add a goal
  perception->start();
  navigator->start();

  EXPECT_TRUE(perception->isRunning());
  EXPECT_TRUE(navigator->isRunning());

  navigator->addGoal("mammals", 1.0f);
  auto current_goal = navigator->getCurrentGoal();
  ASSERT_TRUE(current_goal.has_value());
  EXPECT_EQ(current_goal->topic, "mammals");

  // 2. Perception Phase
  std::string page_url = "https://simple.wikipedia.org/wiki/Cat";
  std::string page_content =
      "<html><body>"
      "<p>A cat is a mammal. Cats have fur.</p>"
      "<a href='https://simple.wikipedia.org/wiki/Mammal'>Mammal</a>"
      "</body></html>";

  bool relation_detected = false;
  perception->setRelationCallback([&](const CandidateRelation &rel,
                                      [[maybe_unused]] bool created) {
    if (rel.subject.text == "cat" && rel.predicate.text == "is_a" &&
        rel.object.text == "mammal") {
      relation_detected = true;
      std::cout << "Perception: Detected relation " << rel.subject.text << " "
                << rel.predicate.text << " " << rel.object.text << "\n";
    }
  });

  perception->submitHtml(page_content, page_url);
  for (int i = 0; i < 5; ++i)
    perception->update(0.1f);

  // 3. Verification Phase: Create Hypothesis
  auto hypothesis_id = verifier->createHypothesis("cat", "is_a", "mammal");
  ASSERT_GT(hypothesis_id, 0);

  RelationEvidence ev1;
  ev1.source_url = page_url;
  ev1.source_text = "A cat is a mammal";
  ev1.confidence = 0.9f;
  ev1.supports = true;
  verifier->addEvidence(hypothesis_id, ev1);

  auto h_after_ev1 = verifier->getHypothesis(hypothesis_id);
  ASSERT_TRUE(h_after_ev1.has_value());
  EXPECT_EQ((int)h_after_ev1->state, (int)RelationHypothesis::State::Pending);

  // 4. Navigation Phase
  std::vector<std::pair<std::string, std::string>> links = {
      {"https://simple.wikipedia.org/wiki/Mammal", "Mammal"}};
  navigator->processLinks(links, page_url);

  auto action = navigator->getNextAction();
  if (action.type == NavigationAction::Type::Navigate) {
    EXPECT_EQ(action.target, "https://simple.wikipedia.org/wiki/Mammal");
  }

  // 5. Verification Phase: Second Source
  std::string page2_url = "https://www.britannica.com/animal/cat";
  RelationEvidence ev2;
  ev2.source_url = page2_url;
  ev2.source_text = "The cat is a domestic mammal.";
  ev2.confidence = 0.95f;
  ev2.supports = true;
  verifier->addEvidence(hypothesis_id, ev2);

  auto state = verifier->verifyHypothesis(hypothesis_id);
  EXPECT_EQ((int)state, (int)RelationHypothesis::State::Verified);

  // 6. Grounding: Sync
  verifier->syncToRelationGates();

  std::size_t s_id = 0, r_id = 0, o_id = 0;
  language_system->getTokenId("cat", s_id);
  language_system->getTokenId("is_a", r_id);
  language_system->getTokenId("mammal", o_id);

  auto gate = relation_gates->getRelation(s_id, r_id, o_id);
  ASSERT_TRUE(gate.has_value());
  EXPECT_GT(gate->confidence, 0.0f);

  std::cout << "Test Complete: Full integration loop verified.\n";
}

int main() {
  int passed = 0;
  int failed = 0;

  for (const auto &test : test_registry) {
    test_failed = false;
    std::cout << "Running " << test.first << "..." << std::endl;
    try {
      test.second();
      if (!test_failed) {
        std::cout << "  PASSED" << std::endl;
        passed++;
      } else {
        std::cout << "  FAILED" << std::endl;
        failed++;
      }
    } catch (const std::exception &e) {
      std::cout << "  FAILED with exception: " << e.what() << std::endl;
      failed++;
    }
  }
  std::cout << "\nTest Results: " << passed << " passed, " << failed
            << " failed" << std::endl;
  return failed > 0 ? 1 : 0;
}
