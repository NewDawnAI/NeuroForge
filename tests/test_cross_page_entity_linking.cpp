
#include "connectivity/ConnectivityManager.h"
#include "core/LanguageSystem.h"
#include "core/RelationGate.h"
#include "perception/LivePerceptionLoop.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

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

static bool test_failed = false;
static std::vector<std::pair<std::string, void (*)()>> test_registry;

void register_test(const std::string &name, void (*func)()) {
  test_registry.push_back({name, func});
}

using namespace NeuroForge;

static const Perception::ExtractedEntity *
findEntity(const Perception::PageSnapshot &snap, const std::string &text) {
  for (const auto &e : snap.entities) {
    if (e.text == text) return &e;
  }
  return nullptr;
}

TEST(CrossPageEntityLinking, CanonicalAndEvidenceCarryAcrossSnapshots) {
  auto connectivity_manager =
      std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
  auto brain = std::make_shared<Core::HypergraphBrain>(connectivity_manager);

  Core::LanguageSystem::Config lang_config;
  Core::LanguageSystem language_system(lang_config);
  EXPECT_TRUE(language_system.initialize());

  Core::RelationGateManager relation_gates(brain, &language_system);

  Perception::LivePerceptionLoop perception(&language_system, &relation_gates);
  EXPECT_TRUE(perception.initialize());
  perception.start();

  std::vector<Perception::PageSnapshot> pages;
  perception.setPageCallback([&](const Perception::PageSnapshot &snap) {
    pages.push_back(snap);
  });

  EXPECT_TRUE(perception.submitText(
      "Machine learning is a field.",
      "https://example.com/page1"));
  EXPECT_TRUE(perception.submitText(
      "Machine learning is used for prediction.",
      "https://example.com/page2"));

  for (int i = 0; i < 500 && pages.size() < 2; ++i) {
    perception.update(0.016f);
  }

  EXPECT_EQ(static_cast<std::size_t>(2), pages.size());

  const auto *e1 = findEntity(pages[0], "machine learning");
  const auto *e2 = findEntity(pages[1], "machine learning");
  EXPECT_TRUE(e1 != nullptr);
  EXPECT_TRUE(e2 != nullptr);

  if (e1 && e2) {
    EXPECT_TRUE(e1->token_id > 0);
    EXPECT_EQ(e1->token_id, e2->token_id);
    EXPECT_TRUE(e2->salience > e1->salience);
  }
}

int main() {
  for (const auto &[name, func] : test_registry) {
    test_failed = false;
    func();
    if (test_failed) {
      std::cerr << "[FAIL] " << name << std::endl;
      return 1;
    }
    std::cout << "[PASS] " << name << std::endl;
  }
  return 0;
}
