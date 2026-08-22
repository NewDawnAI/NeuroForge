#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/ReasoningStep.h"
#include "core/ReasoningTrace.h"

namespace NeuroForge {
namespace Core {

struct Agent2Provenance {
  std::string source;
  int window_index = -1;
  std::string evidence;
  int seen_in_pages = 0;
};

enum class Agent2ReviewSeverity {
  Ok,
  Notice,
  Warning,
};

struct Agent2ReviewFinding {
  Agent2ReviewSeverity severity = Agent2ReviewSeverity::Ok;
  std::string code;
  std::string message;
  std::string suggestion;
};

struct Agent2ReviewInput {
  std::string question;
  std::string answer;
};

struct Agent2ReviewResult {
  Agent2ReviewSeverity overall = Agent2ReviewSeverity::Ok;
  std::vector<Agent2ReviewFinding> findings;
  std::optional<Agent2Provenance> provenance;
};

class Agent2EpistemicReviewer {
public:
  Agent2ReviewResult review(const Agent2ReviewInput &in) const;
  Agent2ReviewResult review(const Agent2ReviewInput &in,
                            const std::vector<ReasoningStep> &steps) const;
  Agent2ReviewResult review(const Agent2ReviewInput &in,
                            const ReasoningTrace &trace) const;

  static std::optional<Agent2Provenance>
  parseProvenanceFromAnswer(const std::string &answer);
};

} // namespace Core
} // namespace NeuroForge
