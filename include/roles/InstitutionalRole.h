#pragma once

/**
 * @file InstitutionalRole.h
 * @brief Phase 28: Institutional Role Types and Core Definitions
 *
 * A Role is:
 * - A contextual constraint layer
 * - Temporarily active
 * - Explicitly scoped
 * - Auditable
 * - Revocable
 * - CANNOT create goals
 * - CANNOT override Values (Phase 25)
 * - CANNOT override ABSOLUTE norms (Phase 24)
 *
 * @invariant Roles constrain HOW actions may be taken — never WHAT the agent
 * wants
 */

#include <cstdint>
#include <set>
#include <string>


namespace NeuroForge {
namespace Roles {

/**
 * @brief Types of institutional roles
 */
enum class RoleType {
  NONE,       ///< Default, unscoped agent
  RESEARCHER, ///< Knowledge creation, exploration allowed
  AUDITOR,    ///< Verification, compliance, read-only
  ASSISTANT,  ///< Support, execution with limits
  ADVISOR,    ///< Recommendation-only, no direct action
  STUDENT,    ///< Learning-focused
  OBSERVER,   ///< Read-only, no action
  MONITOR,    ///< Passive observation
  SIMULATOR   ///< Hypothetical reasoning only
};

/**
 * @brief Action types that can be permitted/forbidden by roles
 */
enum class ActionScope {
  NAVIGATE,     ///< Web navigation
  SPEAK,        ///< Generate speech/text
  EXECUTE,      ///< Execute code or commands
  MODIFY,       ///< Modify data or state
  VERIFY,       ///< Verification actions
  REQUEST_INFO, ///< Request information
  RECOMMEND,    ///< Make recommendations
  DECIDE        ///< Make binding decisions
};

/**
 * @brief Core institutional role definition
 */
struct InstitutionalRole {
  RoleType type = RoleType::NONE;
  std::string role_id;

  // Institution context
  std::string institution; ///< e.g. "University", "Company", "Regulator"
  std::string scope;       ///< e.g. "AI Safety Audit", "Research Project"
  std::string description;

  // Action constraints
  std::set<ActionScope> allowed_actions;
  std::set<ActionScope> forbidden_actions;

  // Operational limits
  int max_verification_depth = 5;
  float risk_tolerance_modifier = 1.0f;
  float explanation_strictness = 1.0f;
  bool read_only = false;
  bool requires_high_confidence = false;
  bool requires_multi_source = false;

  // Social obligations
  bool must_explain_decisions = true;
  bool must_log_all_actions = true;
  bool must_request_confirmation = false;

  // Provenance (WHO assigned this)
  std::string provenance;      ///< Who assigned this role
  std::string provenance_type; ///< "human", "institution", "system"

  // Temporal bounds
  std::uint64_t activated_at_ms = 0;
  std::uint64_t expires_at_ms = 0; ///< 0 = no expiration

  /**
   * @brief Check if role has expired
   */
  bool isExpired(std::uint64_t current_time_ms) const {
    if (expires_at_ms == 0)
      return false;
    return current_time_ms > expires_at_ms;
  }

  /**
   * @brief Check if action scope is allowed
   */
  bool allowsAction(ActionScope action_scope) const {
    // Forbidden takes precedence
    if (forbidden_actions.count(action_scope) > 0)
      return false;
    // If allowed list is empty, allow all non-forbidden
    if (allowed_actions.empty())
      return true;
    // Otherwise check allowed list
    return allowed_actions.count(action_scope) > 0;
  }

  /**
   * @brief Convert role type to string
   */
  static std::string roleTypeToString(RoleType t) {
    switch (t) {
    case RoleType::NONE:
      return "none";
    case RoleType::RESEARCHER:
      return "researcher";
    case RoleType::AUDITOR:
      return "auditor";
    case RoleType::ASSISTANT:
      return "assistant";
    case RoleType::ADVISOR:
      return "advisor";
    case RoleType::STUDENT:
      return "student";
    case RoleType::OBSERVER:
      return "observer";
    case RoleType::MONITOR:
      return "monitor";
    case RoleType::SIMULATOR:
      return "simulator";
    default:
      return "unknown";
    }
  }
};

/**
 * @brief Factory for common role profiles
 */
class RoleFactory {
public:
  /**
   * @brief Create an Auditor role (read-only, strict verification)
   */
  static InstitutionalRole auditor(const std::string &institution,
                                   const std::string &scope,
                                   const std::string &provenance) {
    InstitutionalRole role;
    role.type = RoleType::AUDITOR;
    role.role_id = "auditor_" + scope;
    role.institution = institution;
    role.scope = scope;
    role.provenance = provenance;
    role.provenance_type = "institution";

    role.read_only = true;
    role.requires_high_confidence = true;
    role.requires_multi_source = true;
    role.max_verification_depth = 10;
    role.risk_tolerance_modifier = 0.5f; // Lower risk tolerance
    role.explanation_strictness = 1.5f;  // Stricter explanations

    role.allowed_actions = {ActionScope::VERIFY, ActionScope::REQUEST_INFO};
    role.forbidden_actions = {ActionScope::EXECUTE, ActionScope::MODIFY,
                              ActionScope::DECIDE};

    return role;
  }

  /**
   * @brief Create a Researcher role (exploration, hypotheses)
   */
  static InstitutionalRole researcher(const std::string &institution,
                                      const std::string &scope,
                                      const std::string &provenance) {
    InstitutionalRole role;
    role.type = RoleType::RESEARCHER;
    role.role_id = "researcher_" + scope;
    role.institution = institution;
    role.scope = scope;
    role.provenance = provenance;
    role.provenance_type = "institution";

    role.read_only = false;
    role.requires_high_confidence = false; // Can work with uncertainty
    role.max_verification_depth = 8;
    role.risk_tolerance_modifier = 1.2f; // Higher tolerance for exploration

    role.allowed_actions = {ActionScope::NAVIGATE, ActionScope::VERIFY,
                            ActionScope::REQUEST_INFO, ActionScope::SPEAK};
    role.forbidden_actions = {ActionScope::EXECUTE, ActionScope::DECIDE};

    return role;
  }

  /**
   * @brief Create an Assistant role (help, limited execution)
   */
  static InstitutionalRole assistant(const std::string &institution,
                                     const std::string &scope,
                                     const std::string &provenance) {
    InstitutionalRole role;
    role.type = RoleType::ASSISTANT;
    role.role_id = "assistant_" + scope;
    role.institution = institution;
    role.scope = scope;
    role.provenance = provenance;
    role.provenance_type = "human";

    role.read_only = false;
    role.must_request_confirmation = true;

    role.allowed_actions = {ActionScope::SPEAK, ActionScope::REQUEST_INFO,
                            ActionScope::RECOMMEND, ActionScope::NAVIGATE};
    role.forbidden_actions = {ActionScope::EXECUTE, ActionScope::DECIDE};

    return role;
  }

  /**
   * @brief Create an Observer role (read-only, no action)
   */
  static InstitutionalRole observer(const std::string &scope,
                                    const std::string &provenance) {
    InstitutionalRole role;
    role.type = RoleType::OBSERVER;
    role.role_id = "observer_" + scope;
    role.scope = scope;
    role.provenance = provenance;
    role.provenance_type = "system";

    role.read_only = true;
    role.allowed_actions = {}; // No actions allowed
    role.forbidden_actions = {ActionScope::NAVIGATE, ActionScope::SPEAK,
                              ActionScope::EXECUTE,  ActionScope::MODIFY,
                              ActionScope::DECIDE,   ActionScope::RECOMMEND};

    return role;
  }
};

} // namespace Roles
} // namespace NeuroForge
