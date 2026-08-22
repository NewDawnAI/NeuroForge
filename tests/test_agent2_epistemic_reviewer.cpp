
#include "core/Agent2EpistemicReviewer.h"
#include "core/ReasoningStep.h"
#include "core/ReasoningTrace.h"
#include <iostream>
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

using namespace NeuroForge::Core;

static bool hasCode(const Agent2ReviewResult &r, const std::string &code) {
  for (const auto &f : r.findings) {
    if (f.code == code) return true;
  }
  return false;
}

TEST(Agent2, ParsesProvenance) {
  const std::string ans =
      "X. (source: Machine_learning (en.wikipedia.org) | window=1 | evidence=is_a) "
      "(seen_in_pages=2)";
  auto p = Agent2EpistemicReviewer::parseProvenanceFromAnswer(ans);
  EXPECT_TRUE(p.has_value());
  if (p.has_value()) {
    EXPECT_EQ(std::string("Machine_learning (en.wikipedia.org)"), p->source);
    EXPECT_EQ(1, p->window_index);
    EXPECT_EQ(std::string("is_a"), p->evidence);
    EXPECT_EQ(2, p->seen_in_pages);
  }
}

TEST(Agent2, FlagsIntentMismatchUsedFor) {
  Agent2EpistemicReviewer a2;
  Agent2ReviewInput in;
  in.question = "what is it used for?";
  in.answer =
      "Y. (source: Machine_learning (en.wikipedia.org) | window=1 | evidence=is_a)";
  auto r = a2.review(in);
  EXPECT_TRUE(hasCode(r, "intent_mismatch"));
  bool has_suggestion = false;
  for (const auto &f : r.findings) {
    if (f.code == "intent_mismatch") {
      EXPECT_EQ(std::string("Answer may not address usage; consider qualifying uncertainty."),
                f.suggestion);
      has_suggestion = true;
    }
  }
  EXPECT_TRUE(has_suggestion);
}

TEST(Agent2, AcceptsWhatIsWithIsA) {
  Agent2EpistemicReviewer a2;
  Agent2ReviewInput in;
  in.question = "what is machine learning?";
  in.answer =
      "Z. (source: Machine_learning (en.wikipedia.org) | window=0 | evidence=is_a)";
  auto r = a2.review(in);
  EXPECT_TRUE(!hasCode(r, "intent_mismatch"));
  EXPECT_TRUE(!hasCode(r, "no_provenance"));
}

TEST(ReasoningEngine, InfersUsedForViaIsA) {
  std::vector<ObservedRelation> obs;
  obs.push_back({"machine learning", "is_a", "field", 0.8f, "u1", 0});
  obs.push_back({"field", "used_for", "prediction", 0.6f, "u2", 1});

  auto step = ReasoningEngine::inferUsedForViaIsA("machine learning", obs);
  EXPECT_TRUE(step.has_value());
  if (step.has_value()) {
    EXPECT_EQ(std::string("is_a_then_used_for"), step->rule_id);
    EXPECT_EQ(std::string("machine learning"), step->conclusion.subject);
    EXPECT_EQ(std::string("used_for"), step->conclusion.predicate);
    EXPECT_EQ(std::string("prediction"), step->conclusion.object);
  }
}

TEST(Agent2, FlagsWeakInferenceFromSteps) {
  Agent2EpistemicReviewer a2;
  Agent2ReviewInput in;
  in.question = "what is it used for?";
  in.answer =
      "Y. (source: inference | window=1 | evidence=inferred_used_for_via_is_a)";

  ReasoningStep step;
  step.rule_id = "is_a_then_used_for";
  step.derived_confidence = 0.2f;
  step.conclusion.subject = "machine learning";
  step.conclusion.predicate = "used_for";
  step.conclusion.object = "prediction";
  std::vector<ReasoningStep> steps{step};

  auto r = a2.review(in, steps);
  EXPECT_TRUE(hasCode(r, "weak_inference"));
  bool has_suggestion = false;
  for (const auto &f : r.findings) {
    if (f.code == "weak_inference") {
      EXPECT_EQ(std::string("Inference confidence is low; qualify uncertainty."),
                f.suggestion);
      has_suggestion = true;
    }
  }
  EXPECT_TRUE(has_suggestion);
}

TEST(ReasoningFirst, DerivesIsUsefulFromPremises) {
  std::vector<FactNode> facts;
  FactNode f1;
  f1.subject = "machine learning";
  f1.predicate = "used_for";
  f1.object = "automation";
  f1.confidence = 0.8f;
  f1.trust = 0.9f;
  f1.source = "https://en.wikipedia.org/wiki/Machine_learning";
  f1.window_index = 0;
  facts.push_back(f1);

  FactNode f2;
  f2.subject = "automation";
  f2.predicate = "is";
  f2.object = "useful";
  f2.confidence = 0.7f;
  f2.trust = 0.9f;
  f2.source = "https://en.wikipedia.org/wiki/Automation";
  f2.window_index = 1;
  facts.push_back(f2);

  auto trace = ReasoningFirstEngine::reasonIsUseful("machine learning", facts);
  EXPECT_EQ(static_cast<int>(ReasoningFailure::None),
            static_cast<int>(trace.failure_code));
  EXPECT_TRUE(trace.conclusion.has_value());
  if (trace.conclusion.has_value()) {
    EXPECT_EQ(std::string("machine learning"), trace.conclusion->subject);
    EXPECT_EQ(std::string("is"), trace.conclusion->predicate);
    EXPECT_EQ(std::string("useful"), trace.conclusion->object);
  }
  EXPECT_TRUE(trace.aggregate_confidence > 0.0f);
}

TEST(ReasoningFirst, DetectsAntonymCounterEvidence) {
  std::vector<FactNode> facts;
  FactNode f1;
  f1.subject = "machine learning";
  f1.predicate = "used_for";
  f1.object = "automation";
  f1.confidence = 0.8f;
  f1.trust = 0.9f;
  facts.push_back(f1);

  FactNode f2;
  f2.subject = "automation";
  f2.predicate = "is";
  f2.object = "useful";
  f2.confidence = 0.7f;
  f2.trust = 0.9f;
  facts.push_back(f2);

  FactNode c;
  c.subject = "machine learning";
  c.predicate = "is";
  c.object = "harmful";
  c.confidence = 0.95f;
  c.trust = 0.9f;
  facts.push_back(c);

  auto trace = ReasoningFirstEngine::reasonIsUseful("machine learning", facts);
  EXPECT_EQ(static_cast<int>(ReasoningFailure::LogicalContradiction),
            static_cast<int>(trace.failure_code));
  EXPECT_TRUE(!trace.conclusion.has_value());
  EXPECT_TRUE(!trace.counter_evidence.empty());
}

TEST(ReasoningFirst, DetectsDirectNegationCounterEvidence) {
  std::vector<FactNode> facts;
  FactNode c;
  c.subject = "machine learning";
  c.predicate = "not_useful";
  c.object = "";
  c.polarity = -1;
  c.confidence = 0.9f;
  c.trust = 0.9f;
  facts.push_back(c);

  auto trace = ReasoningFirstEngine::reasonIsUseful("machine learning", facts);
  EXPECT_EQ(static_cast<int>(ReasoningFailure::LogicalContradiction),
            static_cast<int>(trace.failure_code));
  EXPECT_TRUE(!trace.counter_evidence.empty());
}

static bool hasMissing(const ReasoningTrace &t, const std::string &subject,
                       const std::string &predicate,
                       const std::string &object_hint) {
  for (const auto &m : t.missing_premises) {
    if (m.subject == subject && m.predicate == predicate &&
        m.object_hint == object_hint) {
      return true;
    }
  }
  return false;
}

TEST(ReasoningFirst, EmitsMissingPremisesWhenNoSupport) {
  std::vector<FactNode> facts;
  auto trace = ReasoningFirstEngine::reasonIsUseful("machine learning", facts);
  EXPECT_EQ(static_cast<int>(ReasoningFailure::EpistemicVoid),
            static_cast<int>(trace.failure_code));

  facts.push_back({"x", "is_a", "y", 1, "is_a", "u", 0, 0.9f, 1.0f});
  trace = ReasoningFirstEngine::reasonIsUseful("machine learning", facts);
  EXPECT_EQ(static_cast<int>(ReasoningFailure::PremiseGap),
            static_cast<int>(trace.failure_code));
  EXPECT_TRUE(!trace.missing_premises.empty());
  EXPECT_TRUE(hasMissing(trace, "machine learning", "used_for", "<something>"));
  EXPECT_TRUE(hasMissing(trace, "machine learning", "is", "useful"));
}

TEST(ReasoningFirst, EmitsMissingPremiseForIntermediateUsefulness) {
  std::vector<FactNode> facts;
  FactNode f1;
  f1.subject = "machine learning";
  f1.predicate = "used_for";
  f1.object = "automation";
  f1.confidence = 0.8f;
  f1.trust = 0.9f;
  facts.push_back(f1);

  auto trace = ReasoningFirstEngine::reasonIsUseful("machine learning", facts);
  EXPECT_EQ(static_cast<int>(ReasoningFailure::PremiseGap),
            static_cast<int>(trace.failure_code));
  EXPECT_TRUE(!trace.missing_premises.empty());
  EXPECT_TRUE(hasMissing(trace, "automation", "is", "useful"));
}

TEST(Agent2, FlagsMissingPremiseOnTrace) {
  Agent2EpistemicReviewer a2;
  Agent2ReviewInput in;
  in.question = "is machine learning useful?";
  in.answer = "no derivation";

  ReasoningTrace t;
  t.failure_code = ReasoningFailure::PremiseGap;
  t.missing_premises.push_back({"machine learning", "used_for", "<something>"});
  auto r = a2.review(in, t);
  EXPECT_TRUE(hasCode(r, "missing_premise"));
}

TEST(Agent2, FlagsPremiseGapOnTrace) {
  Agent2EpistemicReviewer a2;
  Agent2ReviewInput in;
  in.question = "is machine learning useful?";
  in.answer = "no derivation";

  ReasoningTrace t;
  t.failure_code = ReasoningFailure::PremiseGap;
  auto r = a2.review(in, t);
  EXPECT_TRUE(hasCode(r, "premise_gap"));
}

TEST(HypothesisBuffer, DetectsContradiction) {
  HypothesisBuffer b;
  FactNode a;
  a.subject = "machine learning";
  a.predicate = "requires";
  a.object = "big data";
  a.polarity = 1;
  a.confidence = 0.8f;
  a.trust = 0.9f;
  b.addPremise(a);

  FactNode n;
  n.subject = "machine learning";
  n.predicate = "does_not_require";
  n.object = "big data";
  n.polarity = -1;
  n.confidence = 0.8f;
  n.trust = 0.9f;
  b.addPremise(n);

  EXPECT_TRUE(b.hasContradiction());
}

TEST(HypothesisBuffer, AggregatesSupportVsConflict) {
  HypothesisBuffer b;
  FactNode s;
  s.subject = "x";
  s.predicate = "p";
  s.object = "o";
  s.confidence = 0.9f;
  s.trust = 1.0f;
  b.addPremise(s);

  FactNode c;
  c.subject = "x";
  c.predicate = "not_p";
  c.object = "o";
  c.confidence = 0.9f;
  c.trust = 1.0f;
  b.addCounter(c);

  float agg = b.aggregateSupportConfidence(0.05f);
  EXPECT_TRUE(agg > 0.0f);
  EXPECT_TRUE(agg < 1.0f);
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
