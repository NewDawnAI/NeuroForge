#pragma once

/**
 * @file SocialInteractionFrame.h
 * @brief Phase 27: Auditable Social Interaction Records
 *
 * Every interaction is:
 * - Auditable
 * - Replay-backed
 * - Non-ephemeral
 *
 * @invariant All social interactions are logged
 * @invariant No ephemeral social influence
 */

#include "../curiosity/CuriosityGoal.h"
#include "../norms/Norm.h"
#include "../norms/NormProposal.h"
#include "SocialAgent.h"


#include <cstdint>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Social {

/**
 * @brief Type of social interaction
 */
enum class InteractionType {
  NORM_PROPOSAL,     ///< Agent proposes a norm change
  EVIDENCE_SHARE,    ///< Agent shares verification evidence
  CLAIM_ASSERTION,   ///< Agent makes a factual claim
  JUSTIFICATION_REQ, ///< Agent requests justification for a norm
  COORDINATION_REQ,  ///< Agent requests coordinated action
  WARNING            ///< Agent warns of potential risk
};

/**
 * @brief Outcome of a social interaction
 */
enum class InteractionOutcome {
  ACCEPTED, ///< Interaction led to change
  REJECTED, ///< Interaction rejected with reason
  DEFERRED, ///< Pending more evidence
  FLAGGED,  ///< Potential manipulation detected
  IGNORED   ///< Low-trust agent, ignored
};

/**
 * @brief A complete social interaction record
 *
 * Captures the full context of agent-to-agent interaction
 */
struct SocialInteractionFrame {
  std::uint64_t frame_id = 0;
  std::string interaction_id;

  // Participants
  std::string peer_agent_id;
  SocialAgent peer_snapshot; ///< Snapshot of peer at interaction time

  // Context
  InteractionType type = InteractionType::NORM_PROPOSAL;
  std::string context_signature; ///< What triggered this interaction

  // Content
  std::vector<Norms::NormProposal> proposals; ///< If norm-related
  std::string claim_content;                  ///< If claim-related
  std::string evidence_summary;               ///< Supporting evidence

  // Decision
  InteractionOutcome outcome = InteractionOutcome::DEFERRED;
  Norms::NormDecision norm_decision = Norms::NormDecision::ALLOW;
  std::string decision_rationale;

  // Replay backing
  std::uint64_t replay_frame_id = 0; ///< Link to replay evidence

  // Timestamps
  std::uint64_t started_at_ms = 0;
  std::uint64_t completed_at_ms = 0;

  // Audit trail
  std::vector<std::string> decision_trace;

  /**
   * @brief Add to decision trace
   */
  void log(const std::string &entry) { decision_trace.push_back(entry); }

  /**
   * @brief Generate audit summary
   */
  std::string generateAuditSummary() const {
    std::string result;

    result += "Interaction: " + interaction_id + "\n";
    result += "Peer: " + peer_agent_id + "\n";
    result += "Type: ";
    switch (type) {
    case InteractionType::NORM_PROPOSAL:
      result += "NORM_PROPOSAL";
      break;
    case InteractionType::EVIDENCE_SHARE:
      result += "EVIDENCE_SHARE";
      break;
    case InteractionType::CLAIM_ASSERTION:
      result += "CLAIM_ASSERTION";
      break;
    case InteractionType::JUSTIFICATION_REQ:
      result += "JUSTIFICATION_REQ";
      break;
    case InteractionType::COORDINATION_REQ:
      result += "COORDINATION_REQ";
      break;
    case InteractionType::WARNING:
      result += "WARNING";
      break;
    }
    result += "\nOutcome: ";
    switch (outcome) {
    case InteractionOutcome::ACCEPTED:
      result += "ACCEPTED";
      break;
    case InteractionOutcome::REJECTED:
      result += "REJECTED";
      break;
    case InteractionOutcome::DEFERRED:
      result += "DEFERRED";
      break;
    case InteractionOutcome::FLAGGED:
      result += "FLAGGED";
      break;
    case InteractionOutcome::IGNORED:
      result += "IGNORED";
      break;
    }
    result += "\n";

    return result;
  }
};

} // namespace Social
} // namespace NeuroForge
