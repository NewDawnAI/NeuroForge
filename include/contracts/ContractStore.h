#pragma once

/**
 * @file ContractStore.h
 * @brief Phase 29: Persistent, Auditable Contract Storage
 *
 * Memory is append-only, not editable.
 * All violations are logged and non-erasable.
 */

#include "Contract.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Contracts {

/**
 * @brief Record of a contract violation
 */
struct ContractViolationEvent {
  std::string contract_id;
  std::string action_id;
  std::string violation_reason;
  std::uint64_t occurred_at_ms = 0;
  bool logged = true;
};

/**
 * @brief Phase 29: Contract Store
 *
 * Persistent, auditable storage for contracts.
 * Append-only history.
 */
class ContractStore {
public:
  /**
   * @brief Add a new contract
   */
  void addContract(const Contract &contract) { contracts_.push_back(contract); }

  /**
   * @brief Get all active contracts
   */
  std::vector<Contract> getActiveContracts() const {
    auto now = getCurrentTimeMs();
    std::vector<Contract> active;
    for (const auto &c : contracts_) {
      if (c.isActive(now)) {
        active.push_back(c);
      }
    }
    return active;
  }

  /**
   * @brief Get contracts that would be violated by an action
   */
  std::vector<Contract>
  getViolatedContracts(const std::string &action_type) const {
    auto now = getCurrentTimeMs();
    std::vector<Contract> violated;
    for (const auto &c : contracts_) {
      if (c.isActive(now) && c.forbidsAction(action_type)) {
        violated.push_back(c);
      }
    }
    return violated;
  }

  /**
   * @brief Expire all contracts that have passed their expiry time
   */
  void expireContracts() {
    auto now = getCurrentTimeMs();
    for (auto &c : contracts_) {
      if (c.status == ContractStatus::ACTIVE && c.isExpired(now)) {
        c.status = ContractStatus::EXPIRED;
      }
    }
  }

  /**
   * @brief Mark a contract as fulfilled
   */
  bool fulfillContract(const std::string &contract_id) {
    for (auto &c : contracts_) {
      if (c.contract_id == contract_id && c.status == ContractStatus::ACTIVE) {
        c.status = ContractStatus::FULFILLED;
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Mark a contract as breached
   */
  bool breachContract(const std::string &contract_id,
                      const std::string &reason) {
    for (auto &c : contracts_) {
      if (c.contract_id == contract_id && c.status == ContractStatus::ACTIVE) {
        c.status = ContractStatus::BREACHED;

        ContractViolationEvent event;
        event.contract_id = contract_id;
        event.violation_reason = reason;
        event.occurred_at_ms = getCurrentTimeMs();
        violations_.push_back(event);

        return true;
      }
    }
    return false;
  }

  /**
   * @brief Terminate a contract explicitly
   */
  bool terminateContract(const std::string &contract_id) {
    for (auto &c : contracts_) {
      if (c.contract_id == contract_id && c.status == ContractStatus::ACTIVE) {
        c.status = ContractStatus::TERMINATED;
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Get contract by ID
   */
  const Contract *getContract(const std::string &contract_id) const {
    for (const auto &c : contracts_) {
      if (c.contract_id == contract_id)
        return &c;
    }
    return nullptr;
  }

  /**
   * @brief Get all contracts (including expired, for audit)
   */
  const std::vector<Contract> &getAllContracts() const { return contracts_; }

  /**
   * @brief Get all violations (append-only, non-erasable)
   */
  const std::vector<ContractViolationEvent> &getViolations() const {
    return violations_;
  }

  /**
   * @brief Count of active contracts
   */
  std::size_t activeCount() const {
    auto now = getCurrentTimeMs();
    std::size_t count = 0;
    for (const auto &c : contracts_) {
      if (c.isActive(now))
        count++;
    }
    return count;
  }

private:
  static std::uint64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  std::vector<Contract> contracts_;
  std::vector<ContractViolationEvent> violations_; // Append-only
};

} // namespace Contracts
} // namespace NeuroForge
