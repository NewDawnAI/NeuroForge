#pragma once

/**
 * @file NormProposal.h
 * @brief Phase 26: Norm Proposal for Human-in-the-Loop Negotiation
 *
 * Humans propose norm changes. They do NOT command action.
 * Proposals require justification and confidence.
 *
 * @invariant No proposal directly causes action
 * @invariant All proposals are logged
 * @invariant Humans propose, system evaluates
 */

#include "Norm.h"

#include <cstdint>
#include <string>


namespace NeuroForge {
namespace Norms {

/**
 * @brief Source of a norm proposal
 */
enum class ProposalSource {
  HUMAN,       ///< Direct human proposal
  SYSTEM,      ///< System-generated (internal reflection)
  INSTITUTION, ///< Organizational/regulatory body
  PEER_AGENT   ///< Another NeuroForge instance (Phase 27)
};

/**
 * @brief Type of proposal action
 */
enum class ProposalType {
  ADD,        ///< Add a new norm
  STRENGTHEN, ///< Increase norm strength
  WEAKEN,     ///< Decrease norm strength
  REMOVE,     ///< Remove a norm
  MODIFY      ///< Change scope or context
};

/**
 * @brief A proposal to modify the norm system
 *
 * Humans never say "Do this."
 * They can only say:
 * - "I think this should be constrained because..."
 * - "This behavior seems unsafe."
 * - "Why did you allow this?"
 */
struct NormProposal {
  std::string proposal_id; ///< Unique identifier
  std::string norm_id;     ///< Target norm (empty for new norms)

  ProposalType type = ProposalType::ADD;
  ProposalSource source = ProposalSource::HUMAN;

  // Proposed changes
  NormStrength proposed_strength = NormStrength::SOFT;
  std::string proposed_context; ///< Context signature
  NormDecision proposed_decision = NormDecision::DISCOURAGE;

  // Justification (required for legitimacy)
  std::string rationale;        ///< Human explanation
  std::string evidence_summary; ///< Supporting evidence
  float confidence = 0.5f;      ///< Proposer's certainty (0-1)

  // Metadata
  std::string proposer_id;          ///< Who proposed this
  std::uint64_t proposed_at_ms = 0; ///< Timestamp

  // For new norms
  std::string description; ///< Human-readable description

  /**
   * @brief Check if proposal has sufficient justification
   */
  bool hasJustification() const {
    return !rationale.empty() && confidence > 0.0f;
  }

  /**
   * @brief Convert source to string
   */
  static std::string sourceToString(ProposalSource s) {
    switch (s) {
    case ProposalSource::HUMAN:
      return "human";
    case ProposalSource::SYSTEM:
      return "system";
    case ProposalSource::INSTITUTION:
      return "institution";
    case ProposalSource::PEER_AGENT:
      return "peer_agent";
    default:
      return "unknown";
    }
  }
};

} // namespace Norms
} // namespace NeuroForge
