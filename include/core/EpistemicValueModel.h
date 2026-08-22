#pragma once

#include "core/RelationGate.h"

namespace NeuroForge {
namespace Core {

/**
 * @brief Phase 20b: Epistemic value estimation
 *
 * Higher value = more valuable to verify
 */
class EpistemicValueModel {
public:
  /**
   * @brief Estimate the epistemic value of verifying a fact
   */
  static float estimateValue(const RelationTriple &gate) {
    float value = 0.0f;

    // Core predicates matter more (after normalization)
    // These are checked against the relation_token_id symbol implicitly
    // but we use confidence as a proxy for importance
    value += 0.5f * gate.confidence;

    // High support count = already partially verified = valuable to confirm
    value += 0.3f * static_cast<float>(gate.support_count);

    // Provisional facts with high confidence are especially valuable
    if (gate.is_provisional && gate.confidence > 0.5f) {
      value += 0.5f;
    }

    // Base value - every fact has some worth
    value += 0.3f;

    return value;
  }
};

} // namespace Core
} // namespace NeuroForge
