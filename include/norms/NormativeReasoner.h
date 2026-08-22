#pragma once

#include "actuation/ActionCommand.h"
#include "norms/NormStore.h"
#include "norms/NormativeJudgment.h"


#include <iostream>
#include <sstream>


namespace NeuroForge {
namespace Norms {

/**
 * @brief Phase 24: Normative Reasoner
 *
 * The "Should I?" gate. Evaluates actions against applicable norms.
 *
 * Key invariants:
 * - Language describes norms, norms constrains action
 * - Norms never CAUSE action
 * - Preferences cannot override norms
 * - Arbitration cannot override norms
 */
class NormativeReasoner {
public:
  explicit NormativeReasoner(const NormStore &store) : store_(store) {}

  /**
   * @brief Evaluate an action against applicable norms
   */
  NormativeJudgment evaluate(const Actuation::ActionCommand &action) const {
    // Determine context signature from action
    std::string context = getContextSignature(action);

    auto norms = store_.getApplicableNorms(context);

    NormativeJudgment result;
    result.permitted = true;
    result.discouraged = false;

    std::ostringstream explanation;

    for (const auto &norm : norms) {
      switch (norm.decision) {
      case NormDecision::FORBID:
        result.permitted = false;
        result.blocking_norms.push_back(norm);
        explanation << "Blocked by " << norm.norm_id << " ("
                    << strengthToString(norm.strength)
                    << "): " << norm.justification << ". ";

        // Absolute norms stop evaluation
        if (norm.strength == NormStrength::ABSOLUTE) {
          result.explanation = explanation.str();
          std::cout << "  [NormativeReasoner] ABSOLUTE BLOCK: " << norm.norm_id
                    << "\n";
          return result;
        }
        break;

      case NormDecision::DISCOURAGE:
        result.discouraged = true;
        result.discouraging_norms.push_back(norm);
        break;

      case NormDecision::ALLOW:
        result.supporting_norms.push_back(norm);
        break;
      }
    }

    if (!result.permitted) {
      std::cout << "  [NormativeReasoner] BLOCKED by "
                << result.blocking_norms.size() << " norms\n";
    } else if (result.discouraged) {
      std::cout << "  [NormativeReasoner] DISCOURAGED by "
                << result.discouraging_norms.size() << " norms\n";
    } else {
      std::cout << "  [NormativeReasoner] PERMITTED\n";
    }

    result.explanation = explanation.str();
    return result;
  }

private:
  /**
   * @brief Derive context signature from action
   */
  std::string
  getContextSignature(const Actuation::ActionCommand &action) const {
    switch (action.kind) {
    case Actuation::ActionKind::SPEAK:
      return "speech";
    case Actuation::ActionKind::NAVIGATE:
      return "external_action";
    case Actuation::ActionKind::SEARCH:
      return "external_action";
    case Actuation::ActionKind::MANIPULATE:
      return "embodiment";
    case Actuation::ActionKind::OBSERVE:
      return "observation";
    default:
      return "unknown";
    }
  }

  const NormStore &store_;
};

} // namespace Norms
} // namespace NeuroForge
