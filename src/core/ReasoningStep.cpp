#include "core/ReasoningStep.h"

#include <algorithm>

namespace NeuroForge {
namespace Core {

static bool equalsFold(std::string a, std::string b) {
  auto fold = [](unsigned char c) -> char {
    if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
    return static_cast<char>(c);
  };
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (fold(static_cast<unsigned char>(a[i])) !=
        fold(static_cast<unsigned char>(b[i]))) {
      return false;
    }
  }
  return true;
}

std::optional<ReasoningStep>
ReasoningEngine::inferUsedForViaIsA(const std::string &focus,
                                   const std::vector<ObservedRelation> &obs) {
  if (focus.empty()) return std::nullopt;

  const ObservedRelation *best_is_a = nullptr;
  for (const auto &r : obs) {
    if (!equalsFold(r.subject, focus)) continue;
    if (r.predicate != "is_a") continue;
    if (!best_is_a || r.confidence > best_is_a->confidence) {
      best_is_a = &r;
    }
  }
  if (!best_is_a) return std::nullopt;

  const ObservedRelation *best_used_for = nullptr;
  for (const auto &r : obs) {
    if (!equalsFold(r.subject, best_is_a->object)) continue;
    if (r.predicate != "used_for") continue;
    if (!best_used_for || r.confidence > best_used_for->confidence) {
      best_used_for = &r;
    }
  }
  if (!best_used_for) return std::nullopt;

  ReasoningStep step;
  step.rule_id = "is_a_then_used_for";
  step.premises.push_back(*best_is_a);
  step.premises.push_back(*best_used_for);

  step.conclusion.subject = focus;
  step.conclusion.predicate = "used_for";
  step.conclusion.object = best_used_for->object;
  step.conclusion.confidence = 0.0f;
  step.conclusion.source = "inference";
  step.conclusion.window_index = std::max(best_is_a->window_index, best_used_for->window_index);

  float base = std::min(best_is_a->confidence, best_used_for->confidence);
  step.derived_confidence = std::clamp(base * 0.65f, 0.0f, 1.0f);
  return step;
}

} // namespace Core
} // namespace NeuroForge

