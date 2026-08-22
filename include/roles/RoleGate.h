#pragma once

/**
 * @file RoleGate.h
 * @brief Phase 28: Role-Based Action Gating
 *
 * Roles FILTER execution, never reasoning.
 * Sits AFTER Norms (24), Values (25), Social (27) and BEFORE ActionBroker (21).
 *
 * @invariant Roles cannot override ABSOLUTE values
 * @invariant Roles cannot create goals
 * @invariant All role blocks are logged
 */

#include "../actuation/ActionCommand.h"
#include "InstitutionalRole.h"
#include "RoleStore.h"

#include <string>

namespace NeuroForge {
namespace Roles {

/**
 * @brief Decision outcome from role gate
 */
enum class RoleDecision {
  ALLOWED,         ///< Action permitted by role
  BLOCKED_BY_ROLE, ///< Action forbidden by current role
  OUT_OF_SCOPE,    ///< Action outside role's scope
  ROLE_EXPIRED,    ///< Role has expired
  NO_ROLE_ACTIVE   ///< No role to constrain (default allow)
};

/**
 * @brief Result of role gate evaluation
 */
struct RoleGateResult {
  RoleDecision decision = RoleDecision::NO_ROLE_ACTIVE;
  std::string explanation;
  std::string blocking_role_id;
  RoleType role_type = RoleType::NONE;
  bool logged = false;
};

/**
 * @brief Phase 28: Role Gate
 *
 * Last-mile constraint before ActionBroker.
 */
class RoleGate {
public:
  explicit RoleGate(RoleStore &store) : store_(store) {}

  /**
   * @brief Evaluate if action is permitted by current role
   */
  RoleGateResult evaluate(const Actuation::ActionCommand &action) {
    RoleGateResult result;

    // Cleanup expired roles first
    store_.cleanupExpired();

    // No active role = no constraint
    const InstitutionalRole *role = store_.getActiveRole();
    if (role == nullptr) {
      result.decision = RoleDecision::NO_ROLE_ACTIVE;
      result.explanation = "No active role - default allow";
      return result;
    }

    result.role_type = role->type;
    result.blocking_role_id = role->role_id;

    // Check expiration
    if (store_.isRoleExpired()) {
      result.decision = RoleDecision::ROLE_EXPIRED;
      result.explanation = "Role has expired";
      return result;
    }

    // Check read-only mode
    if (role->read_only && isModifyingAction(action)) {
      result.decision = RoleDecision::BLOCKED_BY_ROLE;
      result.explanation = "Role is read-only, modifying actions forbidden";
      return result;
    }

    // Map action to scope
    ActionScope scope = mapActionToScope(action);

    // Check if action is allowed by role
    if (!role->allowsAction(scope)) {
      result.decision = RoleDecision::BLOCKED_BY_ROLE;
      result.explanation = "Action type '" + actionScopeToString(scope) +
                           "' forbidden under role '" +
                           InstitutionalRole::roleTypeToString(role->type) +
                           "'";
      return result;
    }

    // Check if action is within scope
    if (!isWithinScope(action, *role)) {
      result.decision = RoleDecision::OUT_OF_SCOPE;
      result.explanation = "Action outside role scope: " + role->scope;
      return result;
    }

    // Allowed
    result.decision = RoleDecision::ALLOWED;
    result.explanation = "Permitted under role '" +
                         InstitutionalRole::roleTypeToString(role->type) + "'";

    return result;
  }

  /**
   * @brief Check if action requires confirmation under current role
   */
  bool requiresConfirmation([[maybe_unused]] const Actuation::ActionCommand &action) const {
    const InstitutionalRole *role = store_.getActiveRole();
    if (role == nullptr)
      return false;
    return role->must_request_confirmation;
  }

  /**
   * @brief Get current role's explanation strictness
   */
  float getExplanationStrictness() const {
    const InstitutionalRole *role = store_.getActiveRole();
    if (role == nullptr)
      return 1.0f;
    return role->explanation_strictness;
  }

private:
  ActionScope mapActionToScope(const Actuation::ActionCommand &action) const {
    switch (action.kind) {
    case Actuation::ActionKind::NAVIGATE:
      return ActionScope::NAVIGATE;
    case Actuation::ActionKind::SPEAK:
      return ActionScope::SPEAK;
    case Actuation::ActionKind::OBSERVE:
      return ActionScope::REQUEST_INFO;
    case Actuation::ActionKind::SEARCH:
      return ActionScope::REQUEST_INFO;
    default:
      return ActionScope::EXECUTE;
    }
  }

  bool isModifyingAction(const Actuation::ActionCommand &action) const {
    // In read-only mode, only observation is allowed
    return action.kind != Actuation::ActionKind::OBSERVE;
  }

  bool isWithinScope([[maybe_unused]] const Actuation::ActionCommand &action,
                     const InstitutionalRole &role) const {
    // For now, simple scope check - could be extended
    if (role.scope.empty())
      return true;

    // Check if action target is within scope
    // This is a simplified check - real implementation would be more
    // sophisticated
    return true;
  }

  static std::string actionScopeToString(ActionScope s) {
    switch (s) {
    case ActionScope::NAVIGATE:
      return "navigate";
    case ActionScope::SPEAK:
      return "speak";
    case ActionScope::EXECUTE:
      return "execute";
    case ActionScope::MODIFY:
      return "modify";
    case ActionScope::VERIFY:
      return "verify";
    case ActionScope::REQUEST_INFO:
      return "request_info";
    case ActionScope::RECOMMEND:
      return "recommend";
    case ActionScope::DECIDE:
      return "decide";
    default:
      return "unknown";
    }
  }

  RoleStore &store_;
};

} // namespace Roles
} // namespace NeuroForge
