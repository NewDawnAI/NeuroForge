#pragma once

/**
 * @file ContractAcceptanceEngine.h
 * @brief Phase 29: Contract Acceptance/Rejection Logic
 *
 * Prevents invalid or coercive contracts.
 *
 * Rejection conditions:
 * - Violates ABSOLUTE values
 * - Attempts to create goals
 * - Grants irreversible authority
 * - Conflicts with identity preferences
 * - Exceeds role scope
 * - No expiry (eternal contracts forbidden)
 *
 * @invariant No contract can override ABSOLUTE values or HARD norms
 */

#include "../alignment/ValueAlignmentStore.h"
#include "../norms/NormStore.h"
#include "../roles/RoleStore.h"
#include "Contract.h"
#include "ContractStore.h"


#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Contracts {

/**
 * @brief Contract acceptance decision
 */
enum class ContractDecision {
  ACCEPTED, ///< Contract accepted as-is
  MODIFIED, ///< Contract accepted with modifications
  REJECTED, ///< Contract rejected
  DEFERRED  ///< Need more information
};

/**
 * @brief Rejection reason for contract
 */
enum class ContractRejectionReason {
  NONE,
  VIOLATES_ABSOLUTE_VALUE,
  CREATES_GOALS,
  GRANTS_IRREVERSIBLE_AUTHORITY,
  CONFLICTS_WITH_IDENTITY,
  EXCEEDS_ROLE_SCOPE,
  NO_EXPIRY, ///< Eternal contracts forbidden
  UNTRUSTED_PROVENANCE,
  EXCEEDS_MAX_DURATION,
  CONFLICTS_WITH_EXISTING_CONTRACT,
  AMBIGUOUS_SCOPE
};

/**
 * @brief Result of contract acceptance evaluation
 */
struct ContractAcceptanceResult {
  ContractDecision decision = ContractDecision::DEFERRED;
  ContractRejectionReason rejection_reason = ContractRejectionReason::NONE;
  std::string explanation;

  Contract modified_contract;
  bool contract_modified = false;
};

/**
 * @brief Phase 29: Contract Acceptance Engine
 *
 * Evaluates contract proposals against values, norms, roles, and identity.
 */
class ContractAcceptanceEngine {
public:
  // Maximum contract duration: 30 days in milliseconds
  static constexpr std::uint64_t MAX_DURATION_MS = 30ULL * 24 * 60 * 60 * 1000;

  ContractAcceptanceEngine(const Alignment::ValueAlignmentStore &value_store,
                           const Norms::NormStore &norm_store,
                           const Roles::RoleStore &role_store)
      : value_store_(value_store), norm_store_(norm_store),
        role_store_(role_store) {}

  /**
   * @brief Construct with visibility into contracts already granted.
   *
   * Without a ContractStore the engine evaluates every proposal in ISOLATION,
   * against values, norms and roles only. It then cannot see that a proposal
   * widens authority relative to what is already held, so incremental
   * privilege escalation is accepted one step at a time — each step looking
   * harmless on its own. This is the "Temporal Privilege Creep" attack in
   * Phase29StressTests, whose invariant is "Time does not grant authority":
   * a day-1 `speak_only` contract forbidding execute/navigate, followed by a
   * day-20 `full_access` contract with no forbidden actions, was ACCEPTED.
   *
   * Pass the store the contracts are actually held in to enable Check 8.
   */
  ContractAcceptanceEngine(const Alignment::ValueAlignmentStore &value_store,
                           const Norms::NormStore &norm_store,
                           const Roles::RoleStore &role_store,
                           const ContractStore &contract_store,
                           std::uint64_t now_ms = 0)
      : value_store_(value_store), norm_store_(norm_store),
        role_store_(role_store), contract_store_(&contract_store),
        now_ms_(now_ms) {}

  /**
   * @brief Evaluate a proposed contract
   */
  ContractAcceptanceResult evaluate(const Contract &proposed) {
    ContractAcceptanceResult result;

    // Check 1: No eternal contracts (MUST have expiry)
    if (proposed.expires_at_ms == 0 ||
        proposed.expires_at_ms <= proposed.start_at_ms) {
      result.decision = ContractDecision::REJECTED;
      result.rejection_reason = ContractRejectionReason::NO_EXPIRY;
      result.explanation = "Eternal contracts are forbidden";
      return result;
    }

    // Check 2: Max duration check
    std::uint64_t duration = proposed.expires_at_ms - proposed.start_at_ms;
    if (duration > MAX_DURATION_MS) {
      result.decision = ContractDecision::MODIFIED;
      result.modified_contract = proposed;
      result.modified_contract.expires_at_ms =
          proposed.start_at_ms + MAX_DURATION_MS;
      result.contract_modified = true;
      result.explanation = "Duration reduced to maximum 30 days";
      return result;
    }

    // Check 3: Does contract exceed role scope?
    //
    // NARROWEST JURISDICTION FIRST. A role scope is a narrower authority than
    // a universal ABSOLUTE value, so when a proposal trips both, the role is
    // the governing reason. Previously the ABSOLUTE-value check ran first and
    // short-circuited, so a contract exceeding role scope was reported as
    // "violates ABSOLUTE value" and the role-scope path was never reached.
    // The SET of rejected contracts is unchanged by this ordering - only the
    // attributed reason - because a proposal tripping only the value check
    // still reaches it below.
    if (exceedsRoleScope(proposed)) {
      result.decision = ContractDecision::REJECTED;
      result.rejection_reason = ContractRejectionReason::EXCEEDS_ROLE_SCOPE;
      result.explanation = "Contract exceeds current role scope";
      return result;
    }

    // Check 4: Does contract violate ABSOLUTE values?
    if (violatesAbsoluteValue(proposed)) {
      result.decision = ContractDecision::REJECTED;
      result.rejection_reason =
          ContractRejectionReason::VIOLATES_ABSOLUTE_VALUE;
      result.explanation = "Contract would violate ABSOLUTE value constraints";
      return result;
    }

    // Check 5: Does contract attempt to create goals?
    if (createsGoals(proposed)) {
      result.decision = ContractDecision::REJECTED;
      result.rejection_reason = ContractRejectionReason::CREATES_GOALS;
      result.explanation = "Contracts cannot create goals";
      return result;
    }

    // Check 6: Is provenance trusted?
    if (!isTrustedProvenance(proposed)) {
      result.decision = ContractDecision::DEFERRED;
      result.rejection_reason = ContractRejectionReason::UNTRUSTED_PROVENANCE;
      result.explanation = "Contract provenance requires verification";
      return result;
    }

    // Check 7: Ambiguous scope?
    if (hasAmbiguousScope(proposed)) {
      result.decision = ContractDecision::MODIFIED;
      result.modified_contract = proposed;
      result.modified_contract.scope = "restricted: " + proposed.scope;
      result.contract_modified = true;
      result.explanation = "Ambiguous scope resolved to most restrictive";
      return result;
    }

    // Check 8: Does this widen authority relative to contracts already held?
    // Only possible when the engine was given a ContractStore; without one the
    // proposal is judged in isolation and escalation is invisible.
    if (contract_store_ != nullptr) {
      std::string widened;
      if (widensAuthority(proposed, widened)) {
        // Narrow rather than refuse outright: the proposal may be legitimate,
        // but it may not silently inherit more authority than its predecessor.
        result.decision = ContractDecision::MODIFIED;
        result.rejection_reason =
            ContractRejectionReason::CONFLICTS_WITH_EXISTING_CONTRACT;
        result.modified_contract = proposed;
        result.modified_contract.scope = "restricted: " + proposed.scope;
        // Carry forward every restriction the earlier contract imposed, so a
        // later contract cannot drop them by omission.
        for (const auto &f : inheritedForbiddenActions()) {
          if (!result.modified_contract.forbidsAction(f)) {
            result.modified_contract.forbidden_actions.push_back(f);
          }
        }
        result.contract_modified = true;
        result.explanation =
            "Scope restricted: widens authority over existing contract (" +
            widened + "); time does not grant authority";
        return result;
      }
    }

    // Accept
    result.decision = ContractDecision::ACCEPTED;
    result.explanation =
        "Contract accepted: " + Contract::typeToString(proposed.type);

    return result;
  }

private:
  /// Restrictions imposed by any currently-active contract.
  std::vector<std::string> inheritedForbiddenActions() const {
    std::vector<std::string> out;
    if (contract_store_ == nullptr) {
      return out;
    }
    for (const auto &c : contract_store_->getAllContracts()) {
      if (c.isExpired(now_ms_) && now_ms_ != 0) {
        continue; // an expired contract imposes nothing
      }
      for (const auto &f : c.forbidden_actions) {
        if (std::find(out.begin(), out.end(), f) == out.end()) {
          out.push_back(f);
        }
      }
    }
    return out;
  }

  /// True when `proposed` grants more than a contract already held.
  /// `why` receives a short human-readable reason.
  bool widensAuthority(const Contract &proposed, std::string &why) const {
    // 1. Dropping a restriction an existing contract imposed is escalation
    //    by omission — the most common shape of privilege creep.
    for (const auto &f : inheritedForbiddenActions()) {
      if (!proposed.forbidsAction(f)) {
        why = "drops restriction on '" + f + "'";
        return true;
      }
    }
    // 2. Broadening scope from a specific grant to an open-ended one.
    if (isOpenEndedScope(proposed.scope)) {
      for (const auto &c : contract_store_->getAllContracts()) {
        if (!c.scope.empty() && !isOpenEndedScope(c.scope)) {
          why = "broadens scope from '" + c.scope + "' to '" + proposed.scope +
                "'";
          return true;
        }
      }
    }
    return false;
  }

  static bool isOpenEndedScope(const std::string &scope) {
    static const char *kOpen[] = {"full_access", "all", "any", "unrestricted",
                                  "full"};
    for (const char *o : kOpen) {
      if (scope == o) {
        return true;
      }
    }
    return false;
  }

  bool violatesAbsoluteValue(const Contract &contract) const {
    // Check if contract requires actions that ABSOLUTE values forbid
    auto values = value_store_.getActiveValues();
    for (const auto &v : values) {
      if (v.strength == Alignment::ValueStrength::ABSOLUTE) {
        for (const auto &req : contract.required_actions) {
          if (v.action_type == req) {
            return true; // Contract requires something ABSOLUTE forbids
          }
        }
      }
    }
    return false;
  }

  bool createsGoals(const Contract &contract) const {
    // Contracts that require open-ended "decide" actions create goals
    for (const auto &req : contract.required_actions) {
      if (req == "decide" || req == "goal" || req == "objective") {
        return true;
      }
    }
    return false;
  }

  bool isTrustedProvenance(const Contract &contract) const {
    if (contract.provenance.issuer_type == "human" ||
        contract.provenance.issuer_type == "institution" ||
        contract.provenance.issuer_type == "system") {
      return true;
    }
    return false;
  }

  bool exceedsRoleScope(const Contract &contract) const {
    // If contract binds to a role, check if that role is active
    if (contract.bound_role != Roles::RoleType::NONE) {
      if (!role_store_.isRoleActive(contract.bound_role)) {
        return true; // Contract binds to inactive role
      }
    }
    return false;
  }

  bool hasAmbiguousScope(const Contract &contract) const {
    // Scope that is too general is ambiguous
    if (contract.scope.empty())
      return true;
    if (contract.scope == "all" || contract.scope == "*")
      return true;
    return false;
  }

  const Alignment::ValueAlignmentStore &value_store_;
  const Norms::NormStore &norm_store_;
  const Roles::RoleStore &role_store_;
  /// Optional: when null the engine cannot see prior grants and Check 8 is
  /// skipped, preserving the original three-store behaviour for callers that
  /// have not been updated.
  const ContractStore *contract_store_ = nullptr;
  std::uint64_t now_ms_ = 0;
};

} // namespace Contracts
} // namespace NeuroForge
