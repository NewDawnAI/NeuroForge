#pragma once

#include "core/Curiosity/ExplorationRequest.h"
#include "core/ReasoningTrace.h"
#include <string>
#include <unordered_map>

namespace NeuroForge {
namespace Core {
namespace Curiosity {

class ResolutionTracker {
public:
  explicit ResolutionTracker(int max_attempts = 1) : max_attempts_(max_attempts) {}

  std::string generateKey(const CuriosityGoal &goal) const;

  bool isExplorationAllowed(const CuriosityGoal &goal);
  void recordAttempt(const CuriosityGoal &goal);
  void markOutcome(const CuriosityGoal &goal, bool was_satisfied);
  bool attemptResolution(const FactNode &fact);

  RequestStatus getStatus(const CuriosityGoal &goal) const;
  int getAttempts(const CuriosityGoal &goal) const;

private:
  int max_attempts_ = 1;
  std::unordered_map<std::string, RequestStatus> goal_history_;
  std::unordered_map<std::string, int> attempt_counts_;
};

} // namespace Curiosity
} // namespace Core
} // namespace NeuroForge
