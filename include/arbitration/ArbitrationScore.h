#pragma once

namespace NeuroForge {
namespace Arbitration {

/**
 * @brief Phase 22: Scoring formula for arbitration
 *
 * score = (value * urgency) / (cost + epsilon)
 */
struct ArbitrationScore {
  float value = 0.0f;
  float cost = 0.0f;
  float urgency = 0.0f;

  /**
   * @brief Calculate total score for arbitration
   */
  float total() const {
    constexpr float epsilon = 1e-3f;
    return (value * urgency) / (cost + epsilon);
  }

  /**
   * @brief Compare scores
   */
  bool operator>(const ArbitrationScore &other) const {
    return total() > other.total();
  }
};

} // namespace Arbitration
} // namespace NeuroForge
