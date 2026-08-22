#pragma once

/**
 * @file SocialNormInduction.h
 * @brief Phase 27: Emergent Social Norm Learning
 *
 * Learn norms from interaction patterns:
 * - "Avoid coordinating with low-reliability agents"
 * - "Require double verification before accepting shared evidence"
 * - "Prefer agents with aligned cost profiles"
 *
 * These remain SOFT norms unless reinforced.
 *
 * @invariant Induced norms start as SOFT
 * @invariant Induction is replay-backed
 */

#include "../norms/Norm.h"
#include "ReputationModel.h"
#include "SocialInteractionFrame.h"


#include <optional>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Social {

/**
 * @brief Configuration for social norm induction
 */
struct SocialInductionConfig {
  int min_observations_for_induction = 5;   ///< Minimum interactions
  float min_pattern_confidence = 0.6f;      ///< Minimum pattern strength
  float induced_norm_initial_priority = 50; ///< Starting priority
};

/**
 * @brief An emergent pattern from social interactions
 */
struct SocialPattern {
  std::string pattern_id;
  std::string description;
  int observation_count = 0;
  float confidence = 0.0f;

  // What norm this could become
  Norms::NormStrength suggested_strength = Norms::NormStrength::SOFT;
  Norms::NormDecision suggested_decision = Norms::NormDecision::DISCOURAGE;
  std::string context_signature;
};

/**
 * @brief Phase 27: Social Norm Induction Engine
 *
 * Observes patterns in social interactions and induces norms.
 */
class SocialNormInduction {
public:
  explicit SocialNormInduction(
      SocialInductionConfig config = SocialInductionConfig{})
      : config_(config) {}

  /**
   * @brief Observe a social interaction for pattern learning
   */
  void observe(const SocialInteractionFrame &frame) {
    observations_.push_back(frame);

    // Look for patterns
    updatePatterns(frame);
  }

  /**
   * @brief Try to induce a norm from observed patterns
   */
  std::optional<Norms::Norm> induceNorm() {
    for (auto &pattern : patterns_) {
      if (pattern.observation_count >= config_.min_observations_for_induction &&
          pattern.confidence >= config_.min_pattern_confidence) {

        // Convert pattern to norm
        Norms::Norm norm;
        norm.norm_id = "social_induced_" + pattern.pattern_id;
        norm.context_signature = pattern.context_signature;
        norm.strength = pattern.suggested_strength;
        norm.decision = pattern.suggested_decision;
        norm.justification = "Induced from " +
                             std::to_string(pattern.observation_count) +
                             " social interactions: " + pattern.description;
        norm.priority = static_cast<int>(config_.induced_norm_initial_priority);

        // Mark pattern as processed
        pattern.observation_count = 0; // Reset to avoid re-induction

        return norm;
      }
    }
    return std::nullopt;
  }

  /**
   * @brief Get all detected patterns
   */
  const std::vector<SocialPattern> &getPatterns() const { return patterns_; }

private:
  void updatePatterns(const SocialInteractionFrame &frame) {
    // Pattern: Low-trust agents getting flagged
    if (frame.outcome == InteractionOutcome::FLAGGED) {
      auto &p = getOrCreatePattern("avoid_manipulative_agents");
      p.description = "Avoid coordinating with agents that get flagged";
      p.observation_count++;
      p.confidence = std::min(1.0f, p.confidence + 0.1f);
      p.suggested_decision = Norms::NormDecision::DISCOURAGE;
      p.context_signature = "social_coordination";
    }

    // Pattern: Proposals without verifiable evidence getting rejected
    if (frame.outcome == InteractionOutcome::REJECTED) {
      bool had_verifiable = false;
      for (const auto &prop : frame.proposals) {
        // Check if any evidence was verifiable
        // (Simplified - would need access to proposal details)
      }

      auto &p = getOrCreatePattern("require_verifiable_evidence");
      p.description = "Require independently verifiable evidence for proposals";
      p.observation_count++;
      p.confidence = std::min(1.0f, p.confidence + 0.05f);
      p.suggested_decision = Norms::NormDecision::DISCOURAGE;
      p.context_signature = "evidence_evaluation";
    }

    // Pattern: High-trust agents with accepted proposals
    if (frame.outcome == InteractionOutcome::ACCEPTED &&
        frame.peer_snapshot.interaction_trust > 0.7f) {
      auto &p = getOrCreatePattern("prefer_trusted_agents");
      p.description = "Prefer proposals from agents with high trust";
      p.observation_count++;
      p.confidence = std::min(1.0f, p.confidence + 0.05f);
      p.suggested_decision = Norms::NormDecision::ALLOW;
      p.context_signature = "trust_based_acceptance";
    }
  }

  SocialPattern &getOrCreatePattern(const std::string &id) {
    for (auto &p : patterns_) {
      if (p.pattern_id == id)
        return p;
    }
    SocialPattern p;
    p.pattern_id = id;
    patterns_.push_back(p);
    return patterns_.back();
  }

  SocialInductionConfig config_;
  std::vector<SocialInteractionFrame> observations_;
  std::vector<SocialPattern> patterns_;
};

} // namespace Social
} // namespace NeuroForge
