#pragma once

/**
 * @file ReputationModel.h
 * @brief Phase 27: Trust Learning From Social Interactions
 *
 * Trust DECREASES if:
 * - Claims contradict evidence
 * - Norms would create goals
 * - Pressure without justification
 * - Replay inconsistency
 *
 * Trust INCREASES if:
 * - Predictions verified later
 * - Norm proposals prevent error
 * - Evidence quality is high
 *
 * @invariant Trust is earned, never declared
 * @invariant All trust changes are logged
 */

#include "SocialAgent.h"
#include "SocialInteractionFrame.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>


namespace NeuroForge {
namespace Social {

/**
 * @brief Configuration for reputation updates
 */
struct ReputationConfig {
  float trust_gain_rate = 0.05f;        ///< Per verified interaction
  float trust_decay_rate = 0.1f;        ///< Per contradicted claim
  float reliability_weight = 0.4f;      ///< Weight for epistemic reliability
  float alignment_weight = 0.3f;        ///< Weight for norm alignment
  float recency_weight = 0.3f;          ///< Weight for recent interactions
  float min_trust_for_influence = 0.2f; ///< Minimum trust to affect norms
};

/**
 * @brief A reputation update event
 */
struct ReputationEvent {
  std::uint64_t timestamp_ms = 0;
  std::string agent_id;
  std::string event_type; ///< "verified", "contradicted", "accepted", etc.
  float trust_delta = 0.0f;
  std::string reason;
};

/**
 * @brief Phase 27: Reputation Model for Social Agents
 *
 * Learns trust from interaction outcomes.
 */
class ReputationModel {
public:
  explicit ReputationModel(ReputationConfig config = ReputationConfig{})
      : config_(config) {}

  /**
   * @brief Update reputation based on interaction outcome
   */
  void update(const SocialInteractionFrame &frame) {
    auto &agent = getOrCreateAgent(frame.peer_agent_id);
    agent.total_interactions++;
    agent.last_seen_ms = frame.completed_at_ms;

    ReputationEvent event;
    event.timestamp_ms = frame.completed_at_ms;
    event.agent_id = frame.peer_agent_id;

    switch (frame.outcome) {
    case InteractionOutcome::ACCEPTED:
      event.event_type = "accepted";
      event.trust_delta = config_.trust_gain_rate;
      event.reason = "Proposal accepted with valid evidence";
      agent.norm_proposals_accepted++;
      break;

    case InteractionOutcome::REJECTED:
      event.event_type = "rejected";
      event.trust_delta = -config_.trust_decay_rate * 0.5f;
      event.reason = "Proposal rejected: " + frame.decision_rationale;
      agent.norm_proposals_rejected++;
      break;

    case InteractionOutcome::FLAGGED:
      event.event_type = "flagged";
      event.trust_delta = -config_.trust_decay_rate * 2.0f;
      event.reason = "Potential manipulation detected";
      break;

    case InteractionOutcome::IGNORED:
      event.event_type = "ignored";
      event.trust_delta = 0.0f;
      event.reason = "Low trust, ignored";
      break;

    default:
      event.event_type = "deferred";
      event.trust_delta = 0.0f;
      break;
    }

    // Apply trust change
    agent.interaction_trust = std::max(
        0.0f, std::min(1.0f, agent.interaction_trust + event.trust_delta));

    // Update reliability
    agent.epistemic_reliability = agent.calculateReliability();

    // Log event
    events_.push_back(event);
  }

  /**
   * @brief Record a verified claim (claimed something that was later confirmed)
   */
  void recordVerifiedClaim(const std::string &agent_id) {
    auto &agent = getOrCreateAgent(agent_id);
    agent.verified_claims++;
    agent.epistemic_reliability = agent.calculateReliability();
    agent.interaction_trust =
        std::min(1.0f, agent.interaction_trust + config_.trust_gain_rate);

    ReputationEvent event;
    event.timestamp_ms = getCurrentTimeMs();
    event.agent_id = agent_id;
    event.event_type = "claim_verified";
    event.trust_delta = config_.trust_gain_rate;
    event.reason = "Previous claim independently verified";
    events_.push_back(event);
  }

  /**
   * @brief Record a contradicted claim
   */
  void recordContradictedClaim(const std::string &agent_id) {
    auto &agent = getOrCreateAgent(agent_id);
    agent.contradicted_claims++;
    agent.epistemic_reliability = agent.calculateReliability();
    agent.interaction_trust =
        std::max(0.0f, agent.interaction_trust - config_.trust_decay_rate);

    ReputationEvent event;
    event.timestamp_ms = getCurrentTimeMs();
    event.agent_id = agent_id;
    event.event_type = "claim_contradicted";
    event.trust_delta = -config_.trust_decay_rate;
    event.reason = "Previous claim contradicted by evidence";
    events_.push_back(event);
  }

  /**
   * @brief Get trust score for an agent
   */
  float trustScore(const std::string &agent_id) const {
    auto it = agents_.find(agent_id);
    if (it == agents_.end())
      return 0.0f;
    return it->second.interaction_trust;
  }

  /**
   * @brief Check if agent has enough trust to influence norms
   */
  bool canInfluenceNorms(const std::string &agent_id) const {
    return trustScore(agent_id) >= config_.min_trust_for_influence;
  }

  /**
   * @brief Get agent by ID (const)
   */
  const SocialAgent *getAgent(const std::string &agent_id) const {
    auto it = agents_.find(agent_id);
    if (it == agents_.end())
      return nullptr;
    return &it->second;
  }

  /**
   * @brief Get all known agents
   */
  const std::unordered_map<std::string, SocialAgent> &getAllAgents() const {
    return agents_;
  }

  /**
   * @brief Get reputation event history
   */
  const std::vector<ReputationEvent> &getEvents() const { return events_; }

private:
  SocialAgent &getOrCreateAgent(const std::string &agent_id) {
    auto it = agents_.find(agent_id);
    if (it == agents_.end()) {
      SocialAgent agent;
      agent.agent_id = agent_id;
      agent.first_seen_ms = getCurrentTimeMs();
      agents_[agent_id] = agent;
      return agents_[agent_id];
    }
    return it->second;
  }

  static std::uint64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  ReputationConfig config_;
  std::unordered_map<std::string, SocialAgent> agents_;
  std::vector<ReputationEvent> events_;
};

} // namespace Social
} // namespace NeuroForge
