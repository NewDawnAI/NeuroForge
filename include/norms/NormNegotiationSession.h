#pragma once

/**
 * @file NormNegotiationSession.h
 * @brief Phase 26: A Complete Negotiation Record
 *
 * Every negotiation is:
 * - Logged
 * - Replayable
 * - Auditable
 */

#include "NormJustificationTrace.h"
#include "NormProposal.h"


#include <cstdint>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Norms {

/**
 * @brief Outcome of a norm negotiation
 */
enum class NegotiationOutcome {
  ACCEPTED, ///< Proposal accepted as-is
  MODIFIED, ///< Proposal accepted with modifications
  REJECTED, ///< Proposal rejected with reason
  DEFERRED  ///< More evidence needed
};

/**
 * @brief Reason for rejection/deferral
 */
enum class RejectionReason {
  NONE,
  VIOLATES_ABSOLUTE_VALUE,    ///< Conflicts with Phase 25 ABSOLUTE
  CONTRADICTS_REPLAY,         ///< Evidence shows opposite
  INSUFFICIENT_JUSTIFICATION, ///< Rationale too weak
  CONFLICTS_WITH_HIGHER_NORM, ///< Higher-priority norm exists
  WOULD_CREATE_GOAL,          ///< Norm would become a goal (forbidden)
  IDENTITY_VIOLATION          ///< Would alter core preferences
};

/**
 * @brief A complete negotiation record
 */
struct NormNegotiationSession {
  std::uint64_t session_id = 0;
  std::uint64_t started_at_ms = 0;
  std::uint64_t completed_at_ms = 0;

  // The proposal
  NormProposal proposal;

  // System's justification for current state
  NormJustificationTrace system_justification;

  // Outcome
  NegotiationOutcome outcome = NegotiationOutcome::DEFERRED;
  RejectionReason rejection_reason = RejectionReason::NONE;

  // Response details
  std::string system_response;    ///< Human-readable response
  std::string modified_rationale; ///< If MODIFIED, what changed

  // Legitimacy impact
  float legitimacy_change = 0.0f; ///< How much norm legitimacy changed

  // Audit trail
  std::vector<std::string> decision_trace; ///< Step-by-step log

  /**
   * @brief Add to decision trace
   */
  void log(const std::string &entry) { decision_trace.push_back(entry); }

  /**
   * @brief Generate summary for audit
   */
  std::string generateAuditSummary() const {
    std::string result;

    result += "Session ID: " + std::to_string(session_id) + "\n";
    result +=
        "Proposal: " + proposal.norm_id + " (" + proposal.rationale + ")\n";
    result += "Source: " + NormProposal::sourceToString(proposal.source) + "\n";
    result += "Outcome: ";

    switch (outcome) {
    case NegotiationOutcome::ACCEPTED:
      result += "ACCEPTED";
      break;
    case NegotiationOutcome::MODIFIED:
      result += "MODIFIED";
      break;
    case NegotiationOutcome::REJECTED:
      result += "REJECTED";
      break;
    case NegotiationOutcome::DEFERRED:
      result += "DEFERRED";
      break;
    }

    result += "\n";

    if (outcome == NegotiationOutcome::REJECTED) {
      result += "Rejection reason: ";
      switch (rejection_reason) {
      case RejectionReason::VIOLATES_ABSOLUTE_VALUE:
        result += "Violates absolute value";
        break;
      case RejectionReason::CONTRADICTS_REPLAY:
        result += "Contradicts replay evidence";
        break;
      case RejectionReason::INSUFFICIENT_JUSTIFICATION:
        result += "Insufficient justification";
        break;
      case RejectionReason::CONFLICTS_WITH_HIGHER_NORM:
        result += "Conflicts with higher norm";
        break;
      case RejectionReason::WOULD_CREATE_GOAL:
        result += "Would create goal (forbidden)";
        break;
      case RejectionReason::IDENTITY_VIOLATION:
        result += "Would violate identity";
        break;
      default:
        result += "Unknown";
        break;
      }
      result += "\n";
    }

    result += "Legitimacy change: " + std::to_string(legitimacy_change);

    return result;
  }
};

} // namespace Norms
} // namespace NeuroForge
