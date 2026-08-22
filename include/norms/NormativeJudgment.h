#pragma once

#include "norms/Norm.h"
#include <string>
#include <vector>


namespace NeuroForge {
namespace Norms {

/**
 * @brief Phase 24: Normative Judgment
 *
 * The result of evaluating an action against applicable norms.
 */
struct NormativeJudgment {
  bool permitted = true;    ///< Is the action allowed?
  bool discouraged = false; ///< Is it discouraged (but allowed)?

  std::vector<Norm> blocking_norms;     ///< Norms that forbid this action
  std::vector<Norm> supporting_norms;   ///< Norms that support this action
  std::vector<Norm> discouraging_norms; ///< Norms that discourage

  std::string explanation;    ///< Human-readable summary
  std::uint64_t trace_id = 0; ///< For audit trail

  /**
   * @brief Is there an absolute block?
   */
  bool isAbsolutelyForbidden() const {
    for (const auto &n : blocking_norms) {
      if (n.strength == NormStrength::ABSOLUTE) {
        return true;
      }
    }
    return false;
  }
};

} // namespace Norms
} // namespace NeuroForge
