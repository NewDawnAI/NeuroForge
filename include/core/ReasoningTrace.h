#pragma once

#include <optional>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Core {

enum class ReasoningFailure {
  None,
  PremiseGap,
  LogicalContradiction,
  EpistemicVoid,
  IntentMismatch,
};

struct FactNode {
  enum class Certainty {
    Provisional = 0,
    Confirmed,
  };

  std::string subject;
  std::string predicate;
  std::string object;
  int polarity = 1;
  std::string evidence;
  std::string source;
  int window_index = -1;
  float confidence = 0.0f;
  float trust = 0.5f;
  float transe_score = 0.0f;
  Certainty certainty = Certainty::Confirmed;
};

struct MissingPremise {
  std::string subject;
  std::string predicate;
  std::string object_hint;
};

struct ReasoningTrace {
  std::vector<FactNode> premises;
  std::vector<FactNode> counter_evidence;
  std::vector<MissingPremise> missing_premises;
  std::optional<FactNode> conclusion;
  float aggregate_confidence = 0.0f;
  ReasoningFailure failure_code = ReasoningFailure::None;
  int trace_id = -1;
};

class HypothesisBuffer {
public:
  void clear();
  void addPremise(FactNode f);
  void addCounter(FactNode f);

  const std::vector<FactNode> &premises() const { return premises_; }
  const std::vector<FactNode> &counterEvidence() const { return counter_; }

  bool hasContradiction() const;
  float aggregateSupportConfidence(float epsilon = 0.05f) const;

private:
  std::vector<FactNode> premises_;
  std::vector<FactNode> counter_;
};

class ReasoningFirstEngine {
public:
  static ReasoningTrace reasonIsUseful(const std::string &focus,
                                      const std::vector<FactNode> &facts);

  static void scanForCounterEvidence(const std::string &subject,
                                    const std::string &predicate,
                                    const std::string &object,
                                    const std::vector<FactNode> &facts,
                                    ReasoningTrace &trace);

  static float sourceTrustFromUrl(const std::string &url);
  static int predicatePolarity(const std::string &predicate);
  static std::string predicateBase(const std::string &predicate);
};

} // namespace Core
} // namespace NeuroForge
