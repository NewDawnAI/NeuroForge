#pragma once

/**
 * @file ContractGate.h
 * @brief Phase 29: Final Temporal Gate Before Action Execution
 *
 * Checks if ANY active contract forbids the action.
 * Authority is checked at ACTION time, not intention time.
 *
 * @invariant Execution time check, not intent time
 * @invariant Stricter contract wins in conflicts
 */

#include "../actuation/ActionCommand.h"
#include "Contract.h"
#include "ContractStore.h"


#include <string>

namespace NeuroForge {
namespace Contracts {

/**
 * @brief Decision outcome from contract gate
 */
enum class ContractGateDecision {
  ALLOWED,             ///< No contract forbids this action
  BLOCKED_BY_CONTRACT, ///< Active contract forbids this action
  CONTRACT_EXPIRED,    ///< Contract has expired
  NO_CONTRACTS         ///< No active contracts
};

/**
 * @brief Result of contract gate evaluation
 */
struct ContractGateResult {
  ContractGateDecision decision = ContractGateDecision::NO_CONTRACTS;
  std::string explanation;
  std::string blocking_contract_id;
  ContractType blocking_type = ContractType::TASK_BOUND;
};

/**
 * @brief Phase 29: Contract Gate
 *
 * Final temporal gate before ActionBroker.
 */
class ContractGate {
public:
  explicit ContractGate(ContractStore &store) : store_(store) {}

  /**
   * @brief Evaluate if action is permitted by active contracts
   */
  ContractGateResult evaluate(const Actuation::ActionCommand &action) {
    ContractGateResult result;

    // Cleanup expired contracts first
    store_.expireContracts();

    auto active = store_.getActiveContracts();

    // No active contracts = no constraint
    if (active.empty()) {
      result.decision = ContractGateDecision::NO_CONTRACTS;
      result.explanation = "No active contracts - default allow";
      return result;
    }

    // Convert action kind to string
    std::string action_type = actionKindToString(action.kind);

    // Check each contract
    for (const auto &contract : active) {
      if (contract.forbidsAction(action_type)) {
        result.decision = ContractGateDecision::BLOCKED_BY_CONTRACT;
        result.blocking_contract_id = contract.contract_id;
        result.blocking_type = contract.type;
        result.explanation = "Action '" + action_type +
                             "' forbidden by contract: " + contract.description;
        return result;
      }
    }

    // Not blocked by any contract
    result.decision = ContractGateDecision::ALLOWED;
    result.explanation = "Action permitted by all active contracts";

    return result;
  }

  /**
   * @brief Check if there are any active contracts
   */
  bool hasActiveContracts() const { return store_.activeCount() > 0; }

private:
  static std::string actionKindToString(Actuation::ActionKind kind) {
    switch (kind) {
    case Actuation::ActionKind::SEARCH:
      return "search";
    case Actuation::ActionKind::NAVIGATE:
      return "navigate";
    case Actuation::ActionKind::SPEAK:
      return "speak";
    case Actuation::ActionKind::MANIPULATE:
      return "manipulate";
    case Actuation::ActionKind::OBSERVE:
      return "observe";
    default:
      return "unknown";
    }
  }

  ContractStore &store_;
};

} // namespace Contracts
} // namespace NeuroForge
