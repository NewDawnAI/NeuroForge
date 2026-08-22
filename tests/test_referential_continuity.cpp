
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

TEST(ReferentialContinuity, NormalizationAliasAndCoref) {
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

  Perception::PageSnapshot captured;
  bool got_page = false;
  perception.setPageCallback([&](const Perception::PageSnapshot &snap) {
    captured = snap;
    got_page = true;
  });

  const std::string text =
      "Machine learning (ML) is a field. "
      "It is used for prediction. "
      "This field includes supervised learning.";

  EXPECT_TRUE(perception.submitText(text, "https://example.com/test"));
  for (int i = 0; i < 200 && !got_page; ++i) {
    perception.update(0.016f);
  }

  EXPECT_TRUE(got_page);

  bool has_machine_learning = false;
  for (const auto &e : captured.entities) {
    if (e.text == "machine learning") {
      has_machine_learning = true;
      break;
    }
  }
  EXPECT_TRUE(has_machine_learning);

  auto findRel = [&](const std::string &s, const std::string &p,
                     const std::string &o) {
    for (const auto &r : captured.candidate_relations) {
      if (r.subject.text == s && r.predicate.text == p && r.object.text == o) {
        return true;
      }
    }
    return false;
  };

  EXPECT_TRUE(findRel("machine learning", "is_a", "field"));
  EXPECT_TRUE(findRel("machine learning", "used_for", "prediction"));
  EXPECT_TRUE(findRel("field", "includes", "supervised learning"));
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
