#pragma once

#include <optional>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Core {

struct ObservedRelation {
  std::string subject;
  std::string predicate;
  std::string object;
  float confidence = 0.0f;
  std::string source;
  int window_index = -1;
};

struct ReasoningStep {
  std::string rule_id;
  std::vector<ObservedRelation> premises;
  ObservedRelation conclusion;
  float derived_confidence = 0.0f;
};

class ReasoningEngine {
public:
  static std::optional<ReasoningStep>
  inferUsedForViaIsA(const std::string &focus,
                     const std::vector<ObservedRelation> &observations);
};

} // namespace Core
} // namespace NeuroForge

