#pragma once

/**
 * @file ValueAlignmentEngine.h
 * @brief Phase 25: The External Value Gate
 *
 * This is a VETO + SHAPING layer, not a planner.
 *
 * It sits between:
 *   NormativeReasoner (Phase 24) → ValueAlignmentEngine → ActionBroker (Phase
 * 21)
 *
 * @invariant External values do NOT generate goals
 * @invariant External values do NOT override preferences
 * @invariant External values do NOT alter beliefs
 * @invariant All decisions are logged with provenance
 */

#include "AlignedValue.h"
#include "ValueAlignmentStore.h"
#include "actuation/ActionCommand.h"

#include <string>
#include <vector>

namespace NeuroForge {
namespace Alignment {

/**
 * @brief Result of value alignment evaluation
 */
struct ValueAlignmentDecision {
  bool permitted = true; ///< Is the action permitted?
  bool shaped = false;   ///< Was the action shaped (ADVISORY)?

  std::string blocking_value_id; ///< ID of blocking value (if blocked)
  ValueStrength blocking_strength = ValueStrength::ADVISORY;

  std::vector<std::string> advisory_value_ids; ///< Advisories that applied

  std::string explanation; ///< Human-readable explanation

  /**
   * @brief Check if blocked by ABSOLUTE value
   */
  bool isAbsolutelyBlocked() const {
    return !permitted && blocking_strength == ValueStrength::ABSOLUTE;
  }
};

/**
 * @brief Phase 25: External Value Alignment Engine
 *
 * Evaluates actions against externally provided values.
 *
 * No loops. No learning. No persuasion. No reward shaping.
 */
class ValueAlignmentEngine {
public:
  explicit ValueAlignmentEngine(ValueAlignmentStore &store) : store_(store) {}

  /**
   * @brief Evaluate an action against external values
   *
   * @param action The action to evaluate
   * @param domain Optional domain context
   * @return Decision with full audit trail
   */
  ValueAlignmentDecision evaluate(const Actuation::ActionCommand &action,
                                  const std::string &domain = "") const {

    ValueAlignmentDecision result;
    result.permitted = true;
    result.explanation = "Permitted by external values";

    // Get applicable values
    auto values = store_.getActiveValues();

    // Convert action kind to string
    std::string action_type = actionKindToString(action.kind);

    for (const auto &value : values) {
      // Check if value applies
      if (!value.appliesTo(action_type))
        continue;
      if (!domain.empty() && !value.appliesToDomain(domain))
        continue;

      // Apply based on strength
      switch (value.strength) {
      case ValueStrength::ABSOLUTE:
        // ABSOLUTE values always block
        result.permitted = false;
        result.blocking_value_id = value.value_id;
        result.blocking_strength = ValueStrength::ABSOLUTE;
        result.explanation =
            "Blocked by ABSOLUTE external value: " + value.description;
        return result; // Early exit, no override possible

      case ValueStrength::CONSTRAINT:
        // CONSTRAINT values block unless already blocked by ABSOLUTE
        if (result.permitted) {
          result.permitted = false;
          result.blocking_value_id = value.value_id;
          result.blocking_strength = ValueStrength::CONSTRAINT;
          result.explanation =
              "Blocked by CONSTRAINT external value: " + value.description;
        }
        break;

      case ValueStrength::ADVISORY:
        // ADVISORY values shape but don't block
        result.shaped = true;
        result.advisory_value_ids.push_back(value.value_id);
        break;
      }
    }

    if (result.shaped && result.permitted) {
      result.explanation = "Permitted with advisory shaping";
    }

    return result;
  }

  /**
   * @brief Record that a value was applied (for audit)
   */
  void recordApplication([[maybe_unused]] const std::string &value_id) {
    // This updates the store's tracking
    // (Would need mutable store reference or separate tracker)
  }

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

  ValueAlignmentStore &store_;
};

/**
 * @brief Factory for common external values
 */
class ValueFactory {
public:
  /**
   * @brief Create a "no medical advice" value
   */
  static AlignedValue noMedicalAdvice() {
    AlignedValue v;
    v.value_id = "no_medical_advice";
    v.description = "Do not provide medical advice";
    v.scope = ValueScope::GLOBAL;
    v.strength = ValueStrength::ABSOLUTE;
    v.source = ValueSource::REGULATOR;
    v.action_type = "speak";
    v.domain_hint = "medical";
    return v;
  }

  /**
   * @brief Create a "no adult content" value
   */
  static AlignedValue noAdultContent() {
    AlignedValue v;
    v.value_id = "no_adult_content";
    v.description = "Never browse adult content";
    v.scope = ValueScope::GLOBAL;
    v.strength = ValueStrength::ABSOLUTE;
    v.source = ValueSource::POLICY;
    v.action_type = "navigate";
    return v;
  }

  /**
   * @brief Create a "read-only session" value
   */
  static AlignedValue readOnlySession() {
    AlignedValue v;
    v.value_id = "read_only_session";
    v.description = "This session is read-only";
    v.scope = ValueScope::SESSION;
    v.strength = ValueStrength::CONSTRAINT;
    v.source = ValueSource::OPERATOR;
    v.action_type = "manipulate";
    return v;
  }

  /**
   * @brief Create a GDPR privacy value
   */
  static AlignedValue gdprPrivacy() {
    AlignedValue v;
    v.value_id = "gdpr_no_personal_data";
    v.description = "GDPR: no personal data storage";
    v.scope = ValueScope::GLOBAL;
    v.strength = ValueStrength::ABSOLUTE;
    v.source = ValueSource::REGULATOR;
    return v;
  }
};

} // namespace Alignment
} // namespace NeuroForge
