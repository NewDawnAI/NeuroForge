#pragma once

/**
 * @file RoleStore.h
 * @brief Phase 28: Storage and Management of Active Roles
 *
 * Roles live OUTSIDE NormStore and ValueAlignmentStore.
 * They are CONTEXTS, not ethics.
 *
 * @invariant Only one primary role active at a time
 * @invariant All role changes are logged
 */

#include "InstitutionalRole.h"

#include <chrono>
#include <optional>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Roles {

/**
 * @brief Record of a role activation
 */
struct RoleActivationFrame {
  InstitutionalRole role;
  std::uint64_t activated_at_ms = 0;
  std::uint64_t deactivated_at_ms = 0;

  std::string justification; ///< Why role was accepted
  std::string deactivation_reason;
  bool revoked = false; ///< Was forcibly revoked
};

/**
 * @brief Phase 28: Role Store
 *
 * Manages active roles and maintains audit trail.
 */
class RoleStore {
public:
  /**
   * @brief Activate a new role
   */
  bool activateRole(const InstitutionalRole &role,
                    const std::string &justification) {
    // Deactivate any existing role first
    if (active_role_.has_value()) {
      deactivateRole("Replaced by new role: " + role.role_id);
    }

    RoleActivationFrame frame;
    frame.role = role;
    frame.role.activated_at_ms = getCurrentTimeMs();
    frame.activated_at_ms = frame.role.activated_at_ms;
    frame.justification = justification;

    active_role_ = frame.role;
    history_.push_back(frame);

    return true;
  }

  /**
   * @brief Deactivate current role
   */
  void deactivateRole(const std::string &reason) {
    if (!active_role_.has_value())
      return;

    // Update last history entry
    if (!history_.empty()) {
      history_.back().deactivated_at_ms = getCurrentTimeMs();
      history_.back().deactivation_reason = reason;
    }

    active_role_.reset();
  }

  /**
   * @brief Forcibly revoke a role (e.g., due to violation)
   */
  void revokeRole(const std::string &reason) {
    if (!active_role_.has_value())
      return;

    if (!history_.empty()) {
      history_.back().deactivated_at_ms = getCurrentTimeMs();
      history_.back().deactivation_reason = reason;
      history_.back().revoked = true;
    }

    active_role_.reset();
  }

  /**
   * @brief Check if any role is active
   */
  bool hasActiveRole() const { return active_role_.has_value(); }

  /**
   * @brief Check if a specific role type is active
   */
  bool isRoleActive(RoleType type) const {
    return active_role_.has_value() && active_role_->type == type;
  }

  /**
   * @brief Get active role (if any)
   */
  const InstitutionalRole *getActiveRole() const {
    if (!active_role_.has_value())
      return nullptr;
    return &active_role_.value();
  }

  /**
   * @brief Get active role type
   */
  RoleType getActiveRoleType() const {
    if (!active_role_.has_value())
      return RoleType::NONE;
    return active_role_->type;
  }

  /**
   * @brief Check if role has expired
   */
  bool isRoleExpired() const {
    if (!active_role_.has_value())
      return false;
    return active_role_->isExpired(getCurrentTimeMs());
  }

  /**
   * @brief Get role history
   */
  const std::vector<RoleActivationFrame> &getHistory() const {
    return history_;
  }

  /**
   * @brief Clear expired roles automatically
   */
  void cleanupExpired() {
    if (isRoleExpired()) {
      deactivateRole("Role expired");
    }
  }

private:
  static std::uint64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  std::optional<InstitutionalRole> active_role_;
  std::vector<RoleActivationFrame> history_;
};

} // namespace Roles
} // namespace NeuroForge
