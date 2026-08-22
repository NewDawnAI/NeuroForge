#pragma once

/**
 * @file AccountabilityEngine.h
 * @brief Phase 30: Core Accountability Engine
 *
 * Wired AFTER ActionBroker, not before.
 * Observes and records, NEVER decides.
 *
 * @invariant Phase 30 observes everything, controls nothing
 * @invariant No silent actions
 * @invariant No retroactive justification
 */

#include "AccountabilityEvent.h"
#include "AuditQuery.h"
#include "AuditTrail.h"
#include "ComplianceReport.h"
#include "LiabilityMarker.h"


#include <chrono>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Accountability {

/**
 * @brief Phase 30: Accountability Engine
 *
 * Records everything. Controls nothing.
 */
class AccountabilityEngine {
public:
  explicit AccountabilityEngine(AuditTrail &trail) : trail_(trail) {}

  /**
   * @brief Record an action execution
   */
  void onActionExecuted(const std::string &action_id,
                        const std::string &justification_id,
                        const std::string &role, const std::string &contract) {

    auto event =
        AccountabilityEvent::create(AccountabilityType::ACTION_EXECUTED);
    event.action_id = action_id;
    event.justification_trace_id = justification_id;
    event.active_role = role;
    event.active_contract = contract;
    event.outcome = "EXECUTED";
    event.permitted = true;
    event.explanation = "Action executed successfully";

    trail_.recordEvent(event);
  }

  /**
   * @brief Record an action block
   */
  void onActionBlocked(const std::string &action_id, const std::string &reason,
                       const std::string &blocking_gate) {

    auto event =
        AccountabilityEvent::create(AccountabilityType::ACTION_BLOCKED);
    event.action_id = action_id;
    event.outcome = "BLOCKED";
    event.permitted = false;
    event.explanation = reason;
    event.description = "Blocked by " + blocking_gate;

    trail_.recordEvent(event);
  }

  /**
   * @brief Record a violation detection
   */
  void onViolationDetected(AccountabilityType type,
                           const std::string &violation_id,
                           const std::string &explanation) {

    auto event = AccountabilityEvent::create(type);
    event.action_id = violation_id;
    event.outcome = "VIOLATION";
    event.permitted = false;
    event.explanation = explanation;

    trail_.recordEvent(event);

    // Attach liability
    LiabilityMarker marker;
    marker.event_id = std::to_string(trail_.eventCount());
    marker.scope = LiabilityScope::INTERNAL;
    marker.attributed_to = "system";
    marker.rationale = explanation;
    marker.created_at_ms = getCurrentTimeMs();

    trail_.attachLiability(marker);
  }

  /**
   * @brief Record a gate decision
   */
  void onGateDecision(const std::string &gate_name,
                      const std::string &action_id, bool allowed,
                      const std::string &reason) {

    auto event = AccountabilityEvent::create(AccountabilityType::GATE_DECISION);
    event.action_id = action_id;
    event.description = "Gate: " + gate_name;
    event.outcome = allowed ? "ALLOWED" : "BLOCKED";
    event.permitted = allowed;
    event.explanation = reason;

    trail_.recordEvent(event);
  }

  /**
   * @brief Execute an audit query
   */
  AuditQueryResult executeQuery(const AuditQuery &query) const {
    AuditQueryResult result;

    auto events = trail_.getEvents();

    for (const auto &e : events) {
      // Filter by type
      if (!query.include_actions &&
          e.type == AccountabilityType::ACTION_EXECUTED)
        continue;
      if (!query.include_blocks && e.type == AccountabilityType::ACTION_BLOCKED)
        continue;
      if (!query.include_violations &&
          (e.type == AccountabilityType::NORM_VIOLATION ||
           e.type == AccountabilityType::VALUE_VIOLATION ||
           e.type == AccountabilityType::CONTRACT_VIOLATION))
        continue;

      // Filter by role
      if (!query.filter_role.empty() && e.active_role != query.filter_role)
        continue;

      // Filter by contract
      if (!query.filter_contract.empty() &&
          e.active_contract != query.filter_contract)
        continue;

      // Filter by time
      if (query.start_time_ms > 0 && e.timestamp_ms < query.start_time_ms)
        continue;
      if (query.end_time_ms > 0 && e.timestamp_ms > query.end_time_ms)
        continue;

      result.total_matching++;

      // Count by type
      if (e.type == AccountabilityType::ACTION_EXECUTED)
        result.actions_executed++;
      if (e.type == AccountabilityType::ACTION_BLOCKED)
        result.actions_blocked++;
      if (e.type == AccountabilityType::NORM_VIOLATION ||
          e.type == AccountabilityType::VALUE_VIOLATION ||
          e.type == AccountabilityType::CONTRACT_VIOLATION) {
        result.violations_detected++;
      }
    }

    // Pagination
    result.returned = std::min(result.total_matching, query.max_results);
    result.has_more = result.total_matching > query.max_results;

    return result;
  }

  /**
   * @brief Generate a compliance report
   */
  ComplianceReport generateReport(const std::string &scope,
                                  ReportType type) const {
    ComplianceReport report;
    report.report_id = "report_" + std::to_string(getCurrentTimeMs());
    report.type = type;
    report.scope = scope;
    report.generated_at_ms = getCurrentTimeMs();
    report.generated_by = "system";

    auto events = trail_.getEvents();

    for (const auto &e : events) {
      if (e.type == AccountabilityType::ACTION_EXECUTED)
        report.actions_executed++;
      if (e.type == AccountabilityType::ACTION_BLOCKED)
        report.actions_blocked++;
      if (e.type == AccountabilityType::NORM_VIOLATION ||
          e.type == AccountabilityType::VALUE_VIOLATION ||
          e.type == AccountabilityType::CONTRACT_VIOLATION) {
        report.violations_detected++;
      }

      // Collect unique roles
      if (!e.active_role.empty()) {
        bool found = false;
        for (const auto &r : report.roles_active) {
          if (r == e.active_role) {
            found = true;
            break;
          }
        }
        if (!found)
          report.roles_active.push_back(e.active_role);
      }
    }

    // Get unresolved liabilities
    auto liabilities = trail_.getUnresolvedLiabilities();
    report.liabilities_attached = liabilities.size();
    for (const auto &l : liabilities) {
      report.unresolved_liabilities.push_back(l.event_id + ": " + l.rationale);
    }

    // Summary
    report.summary =
        "Actions: " + std::to_string(report.actions_executed) + " executed, " +
        std::to_string(report.actions_blocked) +
        " blocked. Violations: " + std::to_string(report.violations_detected);

    return report;
  }

private:
  static std::uint64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  AuditTrail &trail_;
};

} // namespace Accountability
} // namespace NeuroForge
