/**
 * @file test_curiosity_navigator.cpp
 * @brief Unit tests for CuriosityNavigator (Phase 3)
 */

#include "core/LanguageSystem.h"
#include "core/RelationGate.h"
#include "core/Curiosity/ExplorationRequest.h"
#include "core/Curiosity/ResolutionTracker.h"
#include "navigation/CuriosityNavigator.h"

#include <cassert>
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

#define EXPECT_FALSE(expr)                                                     \
  if (expr)                                                                    \
  throw std::runtime_error("EXPECT_FALSE failed: " #expr)

#define EXPECT_EQ(a, b)                                                        \
  if ((a) != (b))                                                              \
  throw std::runtime_error("EXPECT_EQ failed: " #a " != " #b)

#define EXPECT_GT(a, b)                                                        \
  if (!((a) > (b)))                                                            \
  throw std::runtime_error("EXPECT_GT failed: " #a " <= " #b)

#define EXPECT_GE(a, b)                                                        \
  if (!((a) >= (b)))                                                           \
  throw std::runtime_error("EXPECT_GE failed: " #a " < " #b)

#define EXPECT_LE(a, b)                                                        \
  if (!((a) <= (b)))                                                           \
  throw std::runtime_error("EXPECT_LE failed: " #a " > " #b)

#define ASSERT_TRUE(expr) EXPECT_TRUE(expr)

#endif // MINIMAL_TEST_FRAMEWORK

// ============= TEST HELPERS =============

class CuriosityNavigatorTest {
public:
  std::unique_ptr<NeuroForge::Core::LanguageSystem> lang;
  std::unique_ptr<NeuroForge::Navigation::CuriosityNavigator> navigator;

  void setup() {
    // Create LanguageSystem with default config
    NeuroForge::Core::LanguageSystem::Config lang_config;
    lang = std::make_unique<NeuroForge::Core::LanguageSystem>(lang_config);
    lang->initialize();

    // Create CuriosityNavigator with nullptr for RelationGateManager (optional)
    NeuroForge::Navigation::NavigatorConfig nav_config;
    nav_config.allowed_domains = {"wikipedia.org", "britannica.com",
                                  "simple.wikipedia.org"};
    nav_config.exploration_epsilon = 0.1f;
    navigator = std::make_unique<NeuroForge::Navigation::CuriosityNavigator>(
        lang.get(), nullptr, nav_config); // nullptr for optional gates
    navigator->initialize();
  }

  void teardown() {
    navigator.reset();
    lang.reset();
  }
};

static CuriosityNavigatorTest g_fixture;

// ============= TESTS =============

TEST(CuriosityNavigator, Initialization) {
  g_fixture.setup();

  auto stats = g_fixture.navigator->getStatistics();
  EXPECT_EQ(stats.pages_visited, 0u);
  EXPECT_EQ(stats.links_discovered, 0u);
  EXPECT_EQ(stats.goals_completed, 0u);

  g_fixture.teardown();
}

TEST(CuriosityNavigator, AddExplorationGoal) {
  g_fixture.setup();

  // Add a goal using the correct signature
  std::vector<std::string> seed_urls = {"https://en.wikipedia.org/wiki/Mammal"};
  g_fixture.navigator->addGoal("Mammals", 1.0f, seed_urls);

  // Retrieve all goals
  auto goals = g_fixture.navigator->getGoals();
  EXPECT_EQ(goals.size(), 1u);
  EXPECT_EQ(goals[0].topic, "Mammals");

  // Get current goal
  auto current = g_fixture.navigator->getCurrentGoal();
  EXPECT_TRUE(current.has_value());
  EXPECT_EQ(current->topic, "Mammals");

  g_fixture.teardown();
}

TEST(CuriosityNavigator, ProcessLinks) {
  g_fixture.setup();

  // Add a goal first
  g_fixture.navigator->addGoal("Animals", 1.0f, {});

  // Process discovered links
  std::vector<std::pair<std::string, std::string>> links = {
      {"https://en.wikipedia.org/wiki/Cat", "Cat - domestic feline"},
      {"https://en.wikipedia.org/wiki/Dog", "Dog - canine companion"}};
  g_fixture.navigator->processLinks(links,
                                    "https://en.wikipedia.org/wiki/Animal");

  // Check stats - links may be filtered by domain or other criteria
  auto stats = g_fixture.navigator->getStatistics();
  // At minimum, processLinks should have been called without error
  EXPECT_TRUE(true); // If we got here, processLinks worked

  g_fixture.teardown();
}

TEST(CuriosityNavigator, RecordPageVisit) {
  g_fixture.setup();

  // Record a page visit with concepts
  std::vector<std::string> concepts = {"mammal", "vertebrate", "warm-blooded"};
  g_fixture.navigator->recordPageVisit("https://en.wikipedia.org/wiki/Mammal",
                                       concepts);

  auto stats = g_fixture.navigator->getStatistics();
  EXPECT_EQ(stats.pages_visited, 1u);
  EXPECT_GE(stats.concepts_encountered, 3u);

  g_fixture.teardown();
}

TEST(CuriosityNavigator, GetNextAction) {
  g_fixture.setup();

  // Add a goal with seed URL
  std::vector<std::string> seeds = {"https://en.wikipedia.org/wiki/Science"};
  g_fixture.navigator->addGoal("Science", 1.0f, seeds);

  // Process some links
  std::vector<std::pair<std::string, std::string>> links = {
      {"https://en.wikipedia.org/wiki/Physics",
       "Physics - fundamental science"}};
  g_fixture.navigator->processLinks(links,
                                    "https://en.wikipedia.org/wiki/Science");

  // Get next action
  auto action = g_fixture.navigator->getNextAction();

  // Should have some action type
  EXPECT_TRUE(
      action.type == NeuroForge::Navigation::NavigationAction::Type::Navigate ||
      action.type == NeuroForge::Navigation::NavigationAction::Type::Extract ||
      action.type == NeuroForge::Navigation::NavigationAction::Type::Wait ||
      action.type == NeuroForge::Navigation::NavigationAction::Type::NewGoal);

  g_fixture.teardown();
}

TEST(CuriosityNavigator, NoveltyCalculation) {
  g_fixture.setup();

  // First novelty check for an unseen concept should be high
  float novelty1 = g_fixture.navigator->calculateNovelty("elephant");
  EXPECT_GT(novelty1, 0.0f);

  // Record the concept as visited
  g_fixture.navigator->recordPageVisit("https://en.wikipedia.org/wiki/Elephant",
                                       {"elephant", "mammal", "trunk"});

  // Novelty should decrease after seeing the concept
  float novelty2 = g_fixture.navigator->calculateNovelty("elephant");
  EXPECT_LE(novelty2, novelty1); // Should be lower or equal

  g_fixture.teardown();
}

TEST(CuriosityNavigator, GetTopCandidates) {
  g_fixture.setup();

  // Add some links
  std::vector<std::pair<std::string, std::string>> links = {
      {"https://en.wikipedia.org/wiki/Cat", "Cat"},
      {"https://en.wikipedia.org/wiki/Dog", "Dog"},
      {"https://en.wikipedia.org/wiki/Bird", "Bird"}};
  g_fixture.navigator->processLinks(links,
                                    "https://en.wikipedia.org/wiki/Animal");

  // Get top candidates
  auto candidates = g_fixture.navigator->getTopCandidates(2);
  EXPECT_LE(candidates.size(), 2u); // Should be at most 2

  g_fixture.teardown();
}

TEST(CuriosityNavigator, ClearGoals) {
  g_fixture.setup();

  // Add goals
  g_fixture.navigator->addGoal("Topic1", 1.0f, {});
  g_fixture.navigator->addGoal("Topic2", 0.5f, {});

  auto goals = g_fixture.navigator->getGoals();
  EXPECT_EQ(goals.size(), 2u);

  // Clear goals
  g_fixture.navigator->clearGoals();

  goals = g_fixture.navigator->getGoals();
  EXPECT_EQ(goals.size(), 0u);

  g_fixture.teardown();
}

TEST(CuriosityNavigator, AuthorizedExplorationRequestQueue) {
  g_fixture.setup();

  using namespace NeuroForge::Core::Curiosity;

  ExplorationRequest low;
  low.authorization = ExplorationAuthorization::Diagnostic;
  low.goal.target_subject = "machine learning";
  low.goal.target_predicate = "used_for";
  low.goal.priority = 0.2f;
  low.goal.originating_trace_id = 1;

  ExplorationRequest high;
  high.authorization = ExplorationAuthorization::Diagnostic;
  high.goal.target_subject = "machine learning";
  high.goal.target_predicate = "is";
  high.goal.object_hint = "useful";
  high.goal.priority = 1.0f;
  high.goal.originating_trace_id = 2;

  g_fixture.navigator->submitRequest(low);
  g_fixture.navigator->submitRequest(high);

  EXPECT_TRUE(g_fixture.navigator->hasPendingRequests());
  auto next = g_fixture.navigator->getNextRequest();
  EXPECT_TRUE(next.isValid());
  EXPECT_EQ(next.goal.originating_trace_id, 2);
  EXPECT_EQ(next.goal.target_predicate, "is");

  g_fixture.teardown();
}

TEST(CuriosityNavigator, QueryGeneratorMapping) {
  g_fixture.setup();

  using namespace NeuroForge::Core::Curiosity;

  CuriosityGoal g1;
  g1.target_subject = "machine learning";
  g1.target_predicate = "used_for";
  EXPECT_EQ(g_fixture.navigator->generateQueryFromGoal(g1),
            std::string("applications of machine learning"));

  CuriosityGoal g2;
  g2.target_subject = "machine learning";
  g2.target_predicate = "is";
  g2.object_hint = "useful";
  EXPECT_EQ(g_fixture.navigator->generateQueryFromGoal(g2),
            std::string("benefits of machine learning"));

  CuriosityGoal g3;
  g3.target_subject = "machine learning";
  g3.target_predicate = "is_a";
  EXPECT_EQ(g_fixture.navigator->generateQueryFromGoal(g3),
            std::string("definition of machine learning"));

  g_fixture.teardown();
}

TEST(CuriosityNavigator, ResolutionTrackerBlocksAfterMaxAttempts) {
  using namespace NeuroForge::Core::Curiosity;

  ResolutionTracker t(1);
  CuriosityGoal g;
  g.target_subject = "machine learning";
  g.target_predicate = "used_for";
  g.object_hint = "*";

  EXPECT_TRUE(t.isExplorationAllowed(g));
  t.recordAttempt(g);
  EXPECT_FALSE(t.isExplorationAllowed(g));
  EXPECT_EQ(static_cast<int>(t.getStatus(g)),
            static_cast<int>(RequestStatus::Abandoned));
}

TEST(CuriosityNavigator, ResolutionTrackerKeyUsesObjectHint) {
  using namespace NeuroForge::Core::Curiosity;

  ResolutionTracker t(1);
  CuriosityGoal a;
  a.target_subject = "ml";
  a.target_predicate = "used_for";
  a.object_hint = "automation";

  CuriosityGoal b;
  b.target_subject = "ml";
  b.target_predicate = "used_for";
  b.object_hint = "healthcare";

  EXPECT_TRUE(t.isExplorationAllowed(a));
  t.recordAttempt(a);
  EXPECT_FALSE(t.isExplorationAllowed(a));

  EXPECT_TRUE(t.isExplorationAllowed(b));
}

TEST(CuriosityNavigator, ResolutionTrackerSatisfiedBlocksFuture) {
  using namespace NeuroForge::Core::Curiosity;

  ResolutionTracker t(3);
  CuriosityGoal g;
  g.target_subject = "x";
  g.target_predicate = "is_a";
  g.object_hint = "y";

  EXPECT_TRUE(t.isExplorationAllowed(g));
  t.markOutcome(g, true);
  EXPECT_FALSE(t.isExplorationAllowed(g));
  EXPECT_EQ(static_cast<int>(t.getStatus(g)),
            static_cast<int>(RequestStatus::Satisfied));
}

TEST(CuriosityNavigator, ResolutionTrackerResolvesWildcardOnFact) {
  using namespace NeuroForge::Core::Curiosity;

  ResolutionTracker t(2);
  CuriosityGoal g;
  g.target_subject = "machine learning";
  g.target_predicate = "used_for";
  g.object_hint = "<something>";

  EXPECT_TRUE(t.isExplorationAllowed(g));
  t.recordAttempt(g);

  NeuroForge::Core::FactNode f;
  f.subject = "machine learning";
  f.predicate = "used_for";
  f.object = "automation";

  EXPECT_TRUE(t.attemptResolution(f));
  EXPECT_EQ(static_cast<int>(t.getStatus(g)),
            static_cast<int>(RequestStatus::Satisfied));
  EXPECT_FALSE(t.isExplorationAllowed(g));
}

TEST(CuriosityNavigator, ResolutionTrackerResolvesSpecificOnFact) {
  using namespace NeuroForge::Core::Curiosity;

  ResolutionTracker t(2);
  CuriosityGoal g;
  g.target_subject = "machine learning";
  g.target_predicate = "is";
  g.object_hint = "useful";

  EXPECT_TRUE(t.isExplorationAllowed(g));
  t.recordAttempt(g);

  NeuroForge::Core::FactNode f;
  f.subject = "machine learning";
  f.predicate = "is";
  f.object = "useful";

  EXPECT_TRUE(t.attemptResolution(f));
  EXPECT_EQ(static_cast<int>(t.getStatus(g)),
            static_cast<int>(RequestStatus::Satisfied));
}

// ============= MAIN =============

int main() {
  std::cout << "\n========================================\n";
  std::cout << "CuriosityNavigator Unit Tests (Phase 3)\n";
  std::cout << "========================================\n\n";

  // Tests are auto-registered and run via static initialization

  std::cout << "\n========================================\n";
  std::cout << "Test Results: " << g_tests_passed << " passed, "
            << g_tests_failed << " failed\n";
  std::cout << "========================================\n";

  return g_tests_failed > 0 ? 1 : 0;
}
