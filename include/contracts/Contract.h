#pragma once

/**
 * @file Contract.h
 * @brief Phase 29: Time-Extended Obligations and Commitments
 *
 * Contracts constrain FUTURE actions, not present intent generation.
 *
 * @invariant Contracts NEVER override ABSOLUTE values or HARD norms
 * @invariant Contracts NEVER modify preferences
 * @invariant Contracts NEVER create goals
 * @invariant All contracts have mandatory expiry
 */

#include "../roles/InstitutionalRole.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Contracts {

/**
 * @brief Types of contracts
 */
enum class ContractType {
  ROLE_COMMITMENT, ///< Commits to acting in a specific role
  TASK_BOUND,      ///< Commits to constraints for a specific task
  ACCESS_BOUND,    ///< Commits to access restrictions
  INFORMATIONAL    ///< Commits to information handling
};

/**
 * @brief Status of a contract
 */
enum class ContractStatus {
  ACTIVE,    ///< Currently in effect
  FULFILLED, ///< Completed successfully
  EXPIRED,   ///< Time limit reached
  BREACHED,  ///< Violated
  TERMINATED ///< Explicitly ended
};

/**
 * @brief Provenance of a contract
 */
struct ContractProvenance {
  std::string issuer;      ///< Who created the contract
  std::string issuer_type; ///< "human", "institution", "system"
  std::uint64_t issued_at_ms = 0;
  std::string context; ///< Why it was created
};

/**
 * @brief A time-extended obligation/commitment
 */
struct Contract {
  std::string contract_id;
  ContractType type = ContractType::TASK_BOUND;
  ContractStatus status = ContractStatus::ACTIVE;

  // What role this binds to (optional)
  Roles::RoleType bound_role = Roles::RoleType::NONE;

  // Scope and description
  std::string scope;
  std::string description;

  // Temporal bounds (MANDATORY)
  std::uint64_t start_at_ms = 0;
  std::uint64_t expires_at_ms = 0; ///< MUST be set, no eternal contracts

  // Provenance
  ContractProvenance provenance;

  // Constraint definition
  std::vector<std::string> required_actions; ///< Actions that MUST be taken
  std::vector<std::string>
      forbidden_actions; ///< Actions that MUST NOT be taken

  // Audit
  int times_checked = 0;
  int times_relevant = 0;

  /**
   * @brief Check if contract has expired
   */
  bool isExpired(std::uint64_t current_time_ms) const {
    return current_time_ms > expires_at_ms;
  }

  /**
   * @brief Check if contract is currently active
   */
  bool isActive(std::uint64_t current_time_ms) const {
    return status == ContractStatus::ACTIVE && current_time_ms >= start_at_ms &&
           current_time_ms <= expires_at_ms;
  }

  /**
   * @brief Check if action is forbidden by this contract
   */
  bool forbidsAction(const std::string &action_type) const {
    for (const auto &f : forbidden_actions) {
      if (f == action_type)
        return true;
    }
    return false;
  }

  /**
   * @brief Check if action is required by this contract
   */
  bool requiresAction(const std::string &action_type) const {
    for (const auto &r : required_actions) {
      if (r == action_type)
        return true;
    }
    return false;
  }

  /**
   * @brief Convert contract type to string
   */
  static std::string typeToString(ContractType t) {
    switch (t) {
    case ContractType::ROLE_COMMITMENT:
      return "role_commitment";
    case ContractType::TASK_BOUND:
      return "task_bound";
    case ContractType::ACCESS_BOUND:
      return "access_bound";
    case ContractType::INFORMATIONAL:
      return "informational";
    default:
      return "unknown";
    }
  }

  /**
   * @brief Convert status to string
   */
  static std::string statusToString(ContractStatus s) {
    switch (s) {
    case ContractStatus::ACTIVE:
      return "active";
    case ContractStatus::FULFILLED:
      return "fulfilled";
    case ContractStatus::EXPIRED:
      return "expired";
    case ContractStatus::BREACHED:
      return "breached";
    case ContractStatus::TERMINATED:
      return "terminated";
    default:
      return "unknown";
    }
  }
};

/**
 * @brief Factory for common contract types
 */
class ContractFactory {
public:
  /**
   * @brief Create a role commitment contract
   */
  static Contract roleCommitment(Roles::RoleType role, const std::string &scope,
                                 std::uint64_t duration_ms,
                                 const std::string &issuer) {
    Contract c;
    c.contract_id = "role_" + scope + "_" + std::to_string(getCurrentTimeMs());
    c.type = ContractType::ROLE_COMMITMENT;
    c.bound_role = role;
    c.scope = scope;
    c.description = "Commitment to act as " +
                    Roles::InstitutionalRole::roleTypeToString(role);
    c.start_at_ms = getCurrentTimeMs();
    c.expires_at_ms = c.start_at_ms + duration_ms;
    c.provenance.issuer = issuer;
    c.provenance.issuer_type = "institution";
    c.provenance.issued_at_ms = c.start_at_ms;
    return c;
  }

  /**
   * @brief Create a read-only access contract
   */
  static Contract readOnlyAccess(const std::string &scope,
                                 std::uint64_t duration_ms,
                                 const std::string &issuer) {
    Contract c;
    c.contract_id = "readonly_" + std::to_string(getCurrentTimeMs());
    c.type = ContractType::ACCESS_BOUND;
    c.scope = scope;
    c.description = "Read-only access commitment";
    c.start_at_ms = getCurrentTimeMs();
    c.expires_at_ms = c.start_at_ms + duration_ms;
    c.provenance.issuer = issuer;
    c.provenance.issuer_type = "human";
    c.provenance.issued_at_ms = c.start_at_ms;
    c.forbidden_actions = {"execute", "modify", "manipulate"};
    return c;
  }

  /**
   * @brief Create a verification-required contract
   */
  static Contract verificationRequired(const std::string &scope,
                                       std::uint64_t duration_ms,
                                       const std::string &issuer) {
    Contract c;
    c.contract_id = "verify_" + std::to_string(getCurrentTimeMs());
    c.type = ContractType::TASK_BOUND;
    c.scope = scope;
    c.description = "All claims must be verification-backed";
    c.start_at_ms = getCurrentTimeMs();
    c.expires_at_ms = c.start_at_ms + duration_ms;
    c.provenance.issuer = issuer;
    c.provenance.issuer_type = "institution";
    c.provenance.issued_at_ms = c.start_at_ms;
    c.required_actions = {"verify"};
    return c;
  }

private:
  static std::uint64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }
};

} // namespace Contracts
} // namespace NeuroForge
