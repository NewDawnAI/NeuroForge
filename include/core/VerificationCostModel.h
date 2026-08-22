#pragma once

#include "core/RelationGate.h"
#include "core/SourceClassifier.h"

namespace NeuroForge {
namespace Core {

/**
 * @brief Phase 20b: Cost estimation for verification attempts
 *
 * Higher cost = more expensive to verify (time, uncertainty, difficulty)
 */
struct CostEstimate {
  float cost;       ///< Higher = more expensive
  float confidence; ///< How confident we are in this estimate
};

class VerificationCostModel {
public:
  /**
   * @brief Estimate cost of verifying a given provisional fact
   */
  static CostEstimate estimateCost(const RelationTriple &gate) {
    float base_cost = 0.5f;

    // Already seen many domains → harder to find new ones
    base_cost += 0.2f * static_cast<float>(gate.domain_classes.size());

    // Penalize if already attempted verification
    base_cost += 0.15f * static_cast<float>(gate.support_count);

    return {base_cost, 0.8f};
  }

private:
  static float domainClassCost(DomainClass cls) {
    switch (cls) {
    case DomainClass::Academic:
      return 0.8f; // PDFs, slow parsing
    case DomainClass::Government:
      return 0.7f;
    case DomainClass::Encyclopedia:
      return 0.4f;
    case DomainClass::Educational:
      return 0.5f;
    case DomainClass::News:
      return 0.5f;
    case DomainClass::Blog:
      return 0.3f;
    case DomainClass::Forum:
      return 0.2f;
    default:
      return 0.6f;
    }
  }
};

} // namespace Core
} // namespace NeuroForge
