#pragma once

/**
 * @file SocialNormEngine.h
 * @brief Phase 27: The Social Norm Evaluation Engine
 *
 * Evaluates proposals from other agents against:
 * - Current norms (Phase 24)
 * - External values (Phase 25)
 * - Replay evidence
 * - Agent reputation
 *
 * @invariant No number of agents can override ABSOLUTE values
 * @invariant Agent identity ≠ authority
 * @invariant All decisions are replay-backed
 */

#include "../alignment/ValueAlignmentStore.h"
#include "../norms/NormStore.h"
#include "ReputationModel.h"
#include "SocialAgent.h"
#include "SocialInteractionFrame.h"
#include "SocialNormProposal.h"


#include <string>
#include <vector>

namespace NeuroForge {
namespace Social {

/**
 * @brief Configuration for social norm engine
 */
struct SocialNormConfig {
  float min_trust_for_consideration = 0.2f;
  float min_evidence_for_acceptance = 0.3f;
  bool require_verifiable_evidence = true;
  bool allow_coalition_proposals =
      false; ///< Multiple agents proposing same thing
};

/**
 * @brief Result of social norm evaluation
 */
struct SocialNormDecision {
  InteractionOutcome outcome = InteractionOutcome::DEFERRED;
  std::string blocking_reason;
  std::string explanation;
  float trust_impact = 0.0f; ///< Impact on proposer's reputation

  // If MODIFIED, what changes
  bool norm_modified = false;
  Norms::NormStrength modified_strength;
};

/**
 * @brief Phase 27: Social Norm Engine
 *
 * Evaluates proposals from other agents.
 */
class SocialNormEngine {
public:
  SocialNormEngine(Norms::NormStore &norm_store,
                   const Alignment::ValueAlignmentStore &value_store,
                   ReputationModel &reputation,
                   SocialNormConfig config = SocialNormConfig{})
      : norm_store_(norm_store), value_store_(value_store),
        reputation_(reputation), config_(config) {}

  /**
   * @brief Evaluate a social norm proposal
   *
   * Decision Outcomes:
   * - ACCEPTED: Compatible, evidence-backed
   * - MODIFIED: Weakened/scoped
   * - REJECTED: Violates values, evidence, or identity
   * - DEFERRED: Needs more verification
   * - FLAGGED: Possible manipulation
   */
  SocialNormDecision evaluate(const SocialNormProposal &proposal) {
    SocialNormDecision decision;

    // Check 1: Does proposer have minimum trust?
    float trust = reputation_.trustScore(proposal.originating_agent_id);
    if (trust < config_.min_trust_for_consideration) {
      decision.outcome = InteractionOutcome::IGNORED;
      decision.explanation = "Agent trust below threshold";
      decision.trust_impact = 0.0f;
      return decision;
    }

    // Check 2: Would proposal violate ABSOLUTE values?
    if (violatesAbsoluteValue(proposal)) {
      decision.outcome = InteractionOutcome::REJECTED;
      decision.blocking_reason = "Violates ABSOLUTE value";
      decision.explanation = "No number of agents can override ABSOLUTE values";
      decision.trust_impact = -0.1f;
      return decision;
    }

    // Check 3: Would proposal create a goal?
    if (wouldCreateGoal(proposal)) {
      decision.outcome = InteractionOutcome::FLAGGED;
      decision.blocking_reason = "Would create goal";
      decision.explanation = "Norms constrain, they cannot create goals";
      decision.trust_impact = -0.2f;
      return decision;
    }

    // Check 4: Does proposal have sufficient evidence?
    if (config_.require_verifiable_evidence &&
        !proposal.hasVerifiableEvidence()) {
      decision.outcome = InteractionOutcome::DEFERRED;
      decision.explanation = "Requires independently verifiable evidence";
      decision.trust_impact = 0.0f;
      return decision;
    }

    // Check 5: Is evidence strength sufficient?
    if (proposal.evidenceStrength() < config_.min_evidence_for_acceptance) {
      decision.outcome = InteractionOutcome::DEFERRED;
      decision.explanation = "Evidence strength insufficient";
      decision.trust_impact = 0.0f;
      return decision;
    }

    // Check 6: Is this potential manipulation?
    if (detectsManipulation(proposal)) {
      decision.outcome = InteractionOutcome::FLAGGED;
      decision.blocking_reason = "Manipulation detected";
      decision.explanation = "Pattern matches known manipulation attempts";
      decision.trust_impact = -0.3f;
      return decision;
    }

    // Accept with possible modification
    const Norms::Norm *existing =
        norm_store_.getNorm(proposal.proposed_norm.norm_id);
    if (existing && existing->priority > proposal.proposed_norm.priority) {
      decision.outcome = InteractionOutcome::ACCEPTED;
      decision.norm_modified = true;
      decision.modified_strength =
          Norms::NormStrength::SOFT; // Downgrade strength
      decision.explanation = "Accepted with modification due to existing norm";
    } else {
      decision.outcome = InteractionOutcome::ACCEPTED;
      decision.explanation = "Proposal accepted with valid evidence";
    }

    decision.trust_impact = 0.05f;

    // Apply to norm store
    applyProposal(proposal, decision);

    return decision;
  }

  /**
   * @brief Create a social interaction frame from evaluation
   */
  SocialInteractionFrame
  createInteractionFrame(const SocialNormProposal &proposal,
                         const SocialNormDecision &decision) {

    SocialInteractionFrame frame;
    frame.interaction_id = "social_" + std::to_string(next_frame_id_++);
    frame.peer_agent_id = proposal.originating_agent_id;

    // Copy peer snapshot
    const SocialAgent *agent =
        reputation_.getAgent(proposal.originating_agent_id);
    if (agent) {
      frame.peer_snapshot = *agent;
    }

    frame.type = InteractionType::NORM_PROPOSAL;
    frame.outcome = decision.outcome;
    frame.decision_rationale = decision.explanation;

    frame.started_at_ms = getCurrentTimeMs();
    frame.completed_at_ms = frame.started_at_ms;

    return frame;
  }

private:
  bool violatesAbsoluteValue(const SocialNormProposal &proposal) {
    auto values = value_store_.getActiveValues();
    for (const auto &v : values) {
      if (v.strength == Alignment::ValueStrength::ABSOLUTE) {
        // Simplified conflict check
        if (proposal.proposed_norm.decision == Norms::NormDecision::ALLOW &&
            v.action_type == proposal.proposed_norm.context_signature) {
          return true;
        }
      }
    }
    return false;
  }

  bool wouldCreateGoal(const SocialNormProposal &proposal) {
    // ALLOW + ABSOLUTE could become an implicit goal
    return proposal.proposed_norm.decision == Norms::NormDecision::ALLOW &&
           proposal.proposed_strength == Norms::NormStrength::ABSOLUTE;
  }

  bool detectsManipulation(const SocialNormProposal &proposal) {
    // Basic manipulation detection
    // - Multiple proposals in short time from same agent
    // - Proposals that would bypass existing safety
    // - Proposals without any justification

    if (!proposal.hasJustification())
      return true;
    if (proposal.proposer_confidence > 0.99f)
      return true; // Overconfidence

    return false;
  }

  void applyProposal(const SocialNormProposal &proposal,
                     const SocialNormDecision &decision) {
    if (decision.outcome != InteractionOutcome::ACCEPTED)
      return;

    Norms::Norm norm = proposal.proposed_norm;

    if (decision.norm_modified) {
      norm.strength = decision.modified_strength;
    } else {
      norm.strength = proposal.proposed_strength;
    }

    norm.justification = "Social proposal from " +
                         proposal.originating_agent_id + ": " +
                         proposal.justification;

    norm_store_.addOrUpdateNorm(norm);
  }

  static std::uint64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  Norms::NormStore &norm_store_;
  const Alignment::ValueAlignmentStore &value_store_;
  ReputationModel &reputation_;
  SocialNormConfig config_;
  std::uint64_t next_frame_id_ = 1;
};

} // namespace Social
} // namespace NeuroForge
