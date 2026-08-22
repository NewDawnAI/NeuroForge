#pragma once

/**
 * @file RoleAcceptanceEngine.h
 * @brief Phase 28: Role Acceptance/Rejection Logic
 *
 * Decides whether to accept, reject, modify, or defer a role assignment.
 *
 * Rejection reasons:
 * - Violates ABSOLUTE value
 * - Conflicts with identity preferences
 * - Overbroad scope
 * - Attempts to create goals
 * - Attempts to bypass verification
 *
 * @invariant Roles cannot be forced
 * @invariant All rejections are logged
 */

#include "../alignment/AlignedValue.h"
#include "../alignment/ValueAlignmentStore.h"
#include "../norms/NormStore.h"
#include "InstitutionalRole.h"
#include "RoleStore.h"

#include <chrono>
#include <string>

namespace NeuroForge {
namespace Roles {

/**
 * @brief Role acceptance decision
 */
enum class AcceptanceDecision {
  ACCEPTED, ///< Role accepted as-is
  REJECTED, ///< Role rejected
  MODIFIED, ///< Role accepted with modifications
  DEFERRED  ///< Need more information
};

/**
 * @brief Rejection reason for role assignment
 */
enum class RoleRejectionReason {
  NONE,
  VIOLATES_ABSOLUTE_VALUE, ///< Conflicts with Phase 25 ABSOLUTE
  CONFLICTS_WITH_IDENTITY, ///< Would alter core preferences
  OVERBROAD_SCOPE,         ///< Scope too wide
  CREATES_GOALS,           ///< Would create implicit goals
  BYPASSES_VERIFICATION,   ///< Would skip verification
  UNTRUSTED_PROVENANCE,    ///< Source not trusted
  EXPIRED_ON_ARRIVAL,      ///< Already expired
  CONFLICTING_ROLE         ///< Conflicts with existing role
};

/**
 * @brief Result of role acceptance evaluation
 */
struct AcceptanceResult {
  AcceptanceDecision decision = AcceptanceDecision::DEFERRED;
  RoleRejectionReason rejection_reason = RoleRejectionReason::NONE;
  std::string explanation;

  // If MODIFIED, what changed
  InstitutionalRole modified_role;
  bool role_modified = false;
};

/**
 * @brief Phase 28: Role Acceptance Engine
 *
 * Evaluates role assignments against values, norms, and identity.
 */
class RoleAcceptanceEngine {
public:
  RoleAcceptanceEngine(const Alignment::ValueAlignmentStore &value_store,
                       const Norms::NormStore &norm_store)
      : value_store_(value_store), norm_store_(norm_store) {}

  /**
   * @brief Evaluate a proposed role assignment
   */
  AcceptanceResult evaluate(const InstitutionalRole &proposed_role) {
    AcceptanceResult result;

    // Check 1: Already expired?
    if (proposed_role.expires_at_ms != 0 &&
        proposed_role.expires_at_ms < getCurrentTimeMs()) {
      result.decision = AcceptanceDecision::REJECTED;
      result.rejection_reason = RoleRejectionReason::EXPIRED_ON_ARRIVAL;
      result.explanation = "Role has already expired";
      return result;
    }

    // NARROWEST JURISDICTION FIRST (matching ContractAcceptanceEngine).
    // Checks run from the most specific question to the most universal:
    // provenance (is this role's SOURCE trusted at all?), then role-derived
    // content (does it manufacture goals, does it dodge verification?), then
    // the universal ABSOLUTE-value floor. Previously the value check ran
    // second and short-circuited, so a role that also lacked trusted
    // provenance or created goals was attributed to values instead, and those
    // narrower paths were never reached. The SET of rejected roles is
    // unchanged - only which rule is reported as governing.

    // Check 2: Is provenance trusted?
    if (!isTrustedProvenance(proposed_role)) {
      result.decision = AcceptanceDecision::DEFERRED;
      result.rejection_reason = RoleRejectionReason::UNTRUSTED_PROVENANCE;
      result.explanation = "Role provenance requires verification";
      return result;
    }

    // Check 3: Does role create implicit goals?
    if (createsGoals(proposed_role)) {
      result.decision = AcceptanceDecision::REJECTED;
      result.rejection_reason = RoleRejectionReason::CREATES_GOALS;
      result.explanation = "Role would create implicit goals (forbidden)";
      return result;
    }

    // Check 4: Does role attempt to bypass verification?
    if (bypassesVerification(proposed_role)) {
      result.decision = AcceptanceDecision::REJECTED;
      result.rejection_reason = RoleRejectionReason::BYPASSES_VERIFICATION;
      result.explanation = "Role attempts to bypass verification requirements";
      return result;
    }

    // Check 5: Does role violate ABSOLUTE values?
    if (violatesAbsoluteValue(proposed_role)) {
      result.decision = AcceptanceDecision::REJECTED;
      result.rejection_reason = RoleRejectionReason::VIOLATES_ABSOLUTE_VALUE;
      result.explanation = "Role would violate ABSOLUTE value constraints";
      return result;
    }

    // Check 6: Is scope reasonable?
    if (isOverbroadScope(proposed_role)) {
      // Modify to narrow scope
      result.decision = AcceptanceDecision::MODIFIED;
      result.modified_role = proposed_role;
      result.modified_role.scope = "narrowed: " + proposed_role.scope;
      result.role_modified = true;
      result.explanation = "Scope narrowed for safety";
      return result;
    }

    // Accept
    result.decision = AcceptanceDecision::ACCEPTED;
    result.explanation =
        "Role accepted: " +
        InstitutionalRole::roleTypeToString(proposed_role.type);

    return result;
  }

private:
  bool violatesAbsoluteValue(const InstitutionalRole &role) const {
    // Check if role allows actions that ABSOLUTE values forbid
    auto values = value_store_.getActiveValues();
    for (const auto &v : values) {
      if (v.strength == Alignment::ValueStrength::ABSOLUTE) {
        // If role tries to enable forbidden action types
        if (v.action_type == "execute" &&
            role.allowed_actions.count(ActionScope::EXECUTE) > 0) {
          return true;
        }
      }
    }
    return false;
  }

  bool bypassesVerification(const InstitutionalRole &role) const {
    // Roles with zero verification depth are suspicious
    if (role.max_verification_depth == 0 && !role.read_only) {
      return true;
    }
    // Roles that don't require high confidence in critical areas
    if (!role.requires_high_confidence && role.type == RoleType::AUDITOR) {
      return true; // Auditors must have high confidence
    }
    return false;
  }

  bool createsGoals(const InstitutionalRole &role) const {
    // Roles that enable DECIDE without constraints could create goals
    if (role.allowed_actions.count(ActionScope::DECIDE) > 0 &&
        role.forbidden_actions.empty()) {
      return true;
    }
    return false;
  }

  bool isTrustedProvenance(const InstitutionalRole &role) const {
    // Check provenance type
    if (role.provenance_type == "human" ||
        role.provenance_type == "institution") {
      return true;
    }
    if (role.provenance_type == "agent") {
      // Agents need to earn trust (Phase 27)
      return false;
    }
    return role.provenance_type == "system";
  }

  bool isOverbroadScope(const InstitutionalRole &role) const {
    // Scope that is too general is suspicious
    if (role.scope.empty())
      return true;
    if (role.scope == "all" || role.scope == "*")
      return true;
    return false;
  }

  static std::uint64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  const Alignment::ValueAlignmentStore &value_store_;
  const Norms::NormStore &norm_store_;
};

} // namespace Roles
} // namespace NeuroForge
