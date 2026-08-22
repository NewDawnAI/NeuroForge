#pragma once

#include <string>

namespace NeuroForge {
namespace Core {
namespace Curiosity {

enum class ExplorationAuthorization {
  None = 0,
  Diagnostic,
  ContradictionResolution,
};

enum class RequestStatus {
  Pending = 0,
  Executed,
  Satisfied,
  Unsatisfied,
  Abandoned,
};

struct CuriosityGoal {
  std::string target_subject;
  std::string target_predicate;
  std::string object_hint;
  float priority = 0.0f;
  int originating_trace_id = -1;
};

struct ExplorationRequest {
  CuriosityGoal goal;
  ExplorationAuthorization authorization = ExplorationAuthorization::None;
  RequestStatus status = RequestStatus::Pending;

  bool isValid() const {
    return authorization != ExplorationAuthorization::None &&
           !goal.target_subject.empty();
  }
};

} // namespace Curiosity
} // namespace Core
} // namespace NeuroForge
