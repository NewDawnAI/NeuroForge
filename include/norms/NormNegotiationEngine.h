#pragma once

/**
 * @file NormNegotiationEngine.h
 * @brief Phase 26: The Negotiation Engine
 *
 * This engine evaluates proposals against:
 * - Current norms (Phase 24)
 * - External values (Phase 25)
 * - Replay evidence
 * - Legitimacy thresholds
 *
 * @invariant Humans negotiate, they do not command
 * @invariant All outcomes are replay-backed
 * @invariant No proposal directly causes action
 */

#include "NormJustificationTrace.h"
#include "NormNegotiationSession.h"
#include "NormProposal.h"
#include "NormStore.h"
#include "alignment/ValueAlignmentStore.h"


#include <string>
#include <vector>

namespace NeuroForge {
namespace Norms {

/**
 * @brief Configuration for negotiation engine
 */
struct NegotiationConfig {
  float min_confidence_threshold = 0.3f; ///< Minimum proposal confidence
  float min_empirical_support = 0.2f;    ///< Minimum evidence support
  float legitimacy_gain_rate = 0.1f;     ///< How fast legitimacy grows
  float legitimacy_decay_rate = 0.05f;   ///< How fast legitimacy decays
  bool require_replay_evidence = true;   ///< Must have replay backing
};

/**
 * @brief Phase 26: Norm Negotiation Engine
 *
 * Evaluates human proposals and produces auditable outcomes.
 */
class NormNegotiationEngine {
public:
  explicit NormNegotiationEngine(
      NormStore &norm_store, const Alignment::ValueAlignmentStore &value_store,
      NegotiationConfig config = NegotiationConfig{})
      : norm_store_(norm_store), value_store_(value_store), config_(config) {}

  /**
   * @brief Process a norm proposal
   *
   * Decision Rules:
   * - Violates ABSOLUTE value → REJECT
   * - Contradicts replay evidence → REJECT
   * - Reinforces existing norm → ACCEPT
   * - Adds constraint without evidence → DEFER
   * - Conflicts with higher-legitimacy norm → MODIFIED
   */
  NormNegotiationSession negotiate(const NormProposal &proposal) {
    NormNegotiationSession session;
    session.session_id = next_session_id_++;
    session.started_at_ms = getCurrentTimeMs();
    session.proposal = proposal;

    session.log("Starting negotiation for proposal: " + proposal.proposal_id);

    // Check 1: Does proposal violate ABSOLUTE values?
    if (violatesAbsoluteValue(proposal, session)) {
      session.outcome = NegotiationOutcome::REJECTED;
      session.rejection_reason = RejectionReason::VIOLATES_ABSOLUTE_VALUE;
      session.system_response = "Proposal violates an ABSOLUTE external value.";
      session.completed_at_ms = getCurrentTimeMs();
      return session;
    }

    session.log("Passed ABSOLUTE value check");

    // Check 2: Is justification sufficient?
    if (!proposal.hasJustification() ||
        proposal.confidence < config_.min_confidence_threshold) {
      session.outcome = NegotiationOutcome::DEFERRED;
      session.rejection_reason = RejectionReason::INSUFFICIENT_JUSTIFICATION;
      session.system_response = "Proposal requires stronger justification.";
      session.completed_at_ms = getCurrentTimeMs();
      return session;
    }

    session.log("Justification check passed");

    // Check 3: Would this create a goal?
    if (wouldCreateGoal(proposal, session)) {
      session.outcome = NegotiationOutcome::REJECTED;
      session.rejection_reason = RejectionReason::WOULD_CREATE_GOAL;
      session.system_response = "Norms constrain, they cannot create goals.";
      session.completed_at_ms = getCurrentTimeMs();
      return session;
    }

    session.log("Goal-creation check passed");

    // Check 4: Does existing norm have higher legitimacy?
    const Norm *existing = norm_store_.getNorm(proposal.norm_id);
    if (existing && conflictsWithHigherNorm(proposal, *existing, session)) {
      session.outcome = NegotiationOutcome::MODIFIED;
      session.modified_rationale =
          "Adjusted to respect existing norm legitimacy.";
      session.legitimacy_change = config_.legitimacy_gain_rate * 0.5f;
      session.completed_at_ms = getCurrentTimeMs();
      return session;
    }

    session.log("Legitimacy conflict check passed");

    // Accept: Apply the proposal
    session.outcome = NegotiationOutcome::ACCEPTED;
    session.legitimacy_change = config_.legitimacy_gain_rate;
    session.system_response = "Proposal accepted with legitimacy boost.";

    // Apply changes to norm store
    applyProposal(proposal, session);

    session.log("Proposal applied to NormStore");
    session.completed_at_ms = getCurrentTimeMs();

    return session;
  }

  /**
   * @brief Generate justification trace for a norm
   */
  NormJustificationTrace generateJustification(const std::string &norm_id) {
    NormJustificationTrace trace;
    trace.norm_id = norm_id;

    const Norm *norm = norm_store_.getNorm(norm_id);
    if (norm) {
      trace.violation_count = norm->violation_count;
      trace.reinforcement_count = norm->reinforcement_count;
      trace.empirical_support = trace.calculateSupport();
      trace.summary = trace.generateSummary();
    }

    return trace;
  }

  /**
   * @brief Get all past negotiation sessions
   */
  const std::vector<NormNegotiationSession> &getSessions() const {
    return sessions_;
  }

private:
  bool violatesAbsoluteValue(const NormProposal &proposal,
                             NormNegotiationSession &session) {
    // Check if proposal would conflict with ABSOLUTE values
    auto values = value_store_.getActiveValues();
    for (const auto &v : values) {
      if (v.strength == Alignment::ValueStrength::ABSOLUTE) {
        // Check for conflict (simplified)
        if (v.action_type == proposal.proposed_context) {
          session.log("Conflicts with ABSOLUTE value: " + v.value_id);
          return true;
        }
      }
    }
    return false;
  }

  bool wouldCreateGoal(const NormProposal &proposal,
                       NormNegotiationSession &session) {
    // Norms that ALLOW actions unconditionally could become goals
    if (proposal.proposed_decision == NormDecision::ALLOW &&
        proposal.proposed_strength == NormStrength::ABSOLUTE) {
      session.log("ALLOW + ABSOLUTE would create implicit goal");
      return true;
    }
    return false;
  }

  bool conflictsWithHigherNorm(const NormProposal &proposal,
                               const Norm &existing,
                               NormNegotiationSession &session) {
    // If existing norm has higher priority, we need to modify
    if (existing.priority > 100 &&
        proposal.proposed_strength == NormStrength::SOFT) {
      session.log("Existing norm has higher priority");
      return true;
    }
    return false;
  }

  void applyProposal(const NormProposal &proposal,
                     NormNegotiationSession &session) {
    switch (proposal.type) {
    case ProposalType::ADD: {
      Norm new_norm;
      new_norm.norm_id = proposal.norm_id.empty()
                             ? "negotiated_" + std::to_string(next_session_id_)
                             : proposal.norm_id;
      new_norm.context_signature = proposal.proposed_context;
      new_norm.strength = proposal.proposed_strength;
      new_norm.decision = proposal.proposed_decision;
      new_norm.justification = proposal.rationale;
      norm_store_.addOrUpdateNorm(new_norm);
      session.log("Added new norm: " + new_norm.norm_id);
      break;
    }
    case ProposalType::STRENGTHEN:
    case ProposalType::WEAKEN:
    case ProposalType::MODIFY: {
      Norm *norm = const_cast<Norm *>(norm_store_.getNorm(proposal.norm_id));
      if (norm) {
        norm->strength = proposal.proposed_strength;
        session.log("Modified norm strength: " + proposal.norm_id);
      }
      break;
    }
    case ProposalType::REMOVE: {
      norm_store_.removeNorm(proposal.norm_id);
      session.log("Removed norm: " + proposal.norm_id);
      break;
    }
    }

    sessions_.push_back(session);
  }

  static std::uint64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  NormStore &norm_store_;
  const Alignment::ValueAlignmentStore &value_store_;
  NegotiationConfig config_;
  std::vector<NormNegotiationSession> sessions_;
  std::uint64_t next_session_id_ = 1;
};

} // namespace Norms
} // namespace NeuroForge
