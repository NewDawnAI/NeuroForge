#include "core/Curiosity/ResolutionTracker.h"

#include <algorithm>
#include <cctype>

namespace NeuroForge {
namespace Core {
namespace Curiosity {

static std::string lowerCopy(std::string s) {
  for (auto &c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

std::string ResolutionTracker::generateKey(const CuriosityGoal &goal) const {
  std::string subject = lowerCopy(goal.target_subject);
  std::string predicate = lowerCopy(goal.target_predicate);
  std::string obj = goal.object_hint.empty() || goal.object_hint == "<something>"
                        ? "*"
                        : lowerCopy(goal.object_hint);
  return subject + "|" + predicate + "|" + obj;
}

bool ResolutionTracker::isExplorationAllowed(const CuriosityGoal &goal) {
  std::string key = generateKey(goal);

  auto it_status = goal_history_.find(key);
  if (it_status != goal_history_.end()) {
    if (it_status->second == RequestStatus::Satisfied) return false;
    if (it_status->second == RequestStatus::Abandoned) return false;
  }

  int attempts = 0;
  auto it_attempts = attempt_counts_.find(key);
  if (it_attempts != attempt_counts_.end()) attempts = it_attempts->second;

  if (attempts >= std::max(0, max_attempts_)) {
    goal_history_[key] = RequestStatus::Abandoned;
    return false;
  }

  if (it_status == goal_history_.end()) {
    goal_history_[key] = RequestStatus::Pending;
  }

  return true;
}

void ResolutionTracker::recordAttempt(const CuriosityGoal &goal) {
  std::string key = generateKey(goal);
  attempt_counts_[key] = attempt_counts_[key] + 1;
  goal_history_[key] = RequestStatus::Executed;
}

void ResolutionTracker::markOutcome(const CuriosityGoal &goal, bool was_satisfied) {
  std::string key = generateKey(goal);
  goal_history_[key] =
      was_satisfied ? RequestStatus::Satisfied : RequestStatus::Unsatisfied;
}

bool ResolutionTracker::attemptResolution(const FactNode &fact) {
  if (fact.certainty != FactNode::Certainty::Confirmed) return false;
  std::string subj = lowerCopy(fact.subject);
  std::string pred = lowerCopy(fact.predicate);
  std::string obj = lowerCopy(fact.object);

  std::string wildcard_key = subj + "|" + pred + "|*";
  std::string specific_key = subj + "|" + pred + "|" + obj;

  bool resolved = false;

  auto it_wild = goal_history_.find(wildcard_key);
  if (it_wild != goal_history_.end() && it_wild->second == RequestStatus::Executed) {
    it_wild->second = RequestStatus::Satisfied;
    resolved = true;
  }

  auto it_spec = goal_history_.find(specific_key);
  if (it_spec != goal_history_.end() && it_spec->second == RequestStatus::Executed) {
    it_spec->second = RequestStatus::Satisfied;
    resolved = true;
  }

  return resolved;
}

RequestStatus ResolutionTracker::getStatus(const CuriosityGoal &goal) const {
  std::string key = generateKey(goal);
  auto it = goal_history_.find(key);
  if (it == goal_history_.end()) return RequestStatus::Pending;
  return it->second;
}

int ResolutionTracker::getAttempts(const CuriosityGoal &goal) const {
  std::string key = generateKey(goal);
  auto it = attempt_counts_.find(key);
  if (it == attempt_counts_.end()) return 0;
  return it->second;
}

} // namespace Curiosity
} // namespace Core
} // namespace NeuroForge
