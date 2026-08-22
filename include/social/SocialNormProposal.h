#pragma once

/**
 * @file SocialNormProposal.h
 * @brief Phase 27: How Agents Propose Norms to Each Other
 *
 * Other agents CANNOT say "Do X."
 * They can ONLY say "Given evidence E, constraint C may be justified."
 *
 * @invariant Proposals require evidence
 * @invariant No agent can command action
 * @invariant All proposals are auditable
 */

#include "../norms/Norm.h"
#include "SocialAgent.h"


#include <cstdint>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Social {

/**
 * @brief Type of social norm proposal
 */
enum class SocialProposalType {
  SUGGEST_CONSTRAINT,    ///< "This should be constrained"
  WARN_RISK,             ///< "This behavior has risk"
  REQUEST_JUSTIFICATION, ///< "Why did you allow this?"
  SHARE_EVIDENCE,        ///< "Here is evidence about X"
  PROPOSE_COORDINATION   ///< "We should coordinate on X"
};

/**
 * @brief Evidence provided with a social proposal
 */
struct SocialEvidence {
  std::string evidence_id;
  std::string source_description;
  std::uint64_t observed_at_ms = 0;
  float confidence = 0.0f;
  bool independently_verifiable = false; ///< Can we verify this ourselves?
};

/**
 * @brief A norm proposal from another agent
 *
 * Other agents can only propose, never command.
 */
struct SocialNormProposal {
  std::string proposal_id;
  SocialProposalType type = SocialProposalType::SUGGEST_CONSTRAINT;

  // Who proposed this
  std::string originating_agent_id;
  AgentRole claimed_role = AgentRole::PEER;

  // The proposed norm (if applicable)
  Norms::Norm proposed_norm;
  Norms::NormStrength proposed_strength = Norms::NormStrength::SOFT;

  // Justification (REQUIRED)
  std::string justification;            ///< Why this norm is needed
  std::vector<SocialEvidence> evidence; ///< Supporting evidence

  // Context
  std::string applies_to_context; ///< What context this applies to
  std::string domain_hint;        ///< Optional domain filter

  // Metadata
  std::uint64_t proposed_at_ms = 0;
  float proposer_confidence = 0.0f; ///< How confident is the proposer?

  /**
   * @brief Check if proposal has sufficient justification
   */
  bool hasJustification() const {
    return !justification.empty() && proposer_confidence > 0.0f;
  }

  /**
   * @brief Check if proposal has verifiable evidence
   */
  bool hasVerifiableEvidence() const {
    for (const auto &e : evidence) {
      if (e.independently_verifiable)
        return true;
    }
    return false;
  }

  /**
   * @brief Calculate evidence strength
   */
  float evidenceStrength() const {
    if (evidence.empty())
      return 0.0f;
    float total = 0.0f;
    for (const auto &e : evidence) {
      total += e.confidence * (e.independently_verifiable ? 1.5f : 1.0f);
    }
    return total / static_cast<float>(evidence.size());
  }
};

} // namespace Social
} // namespace NeuroForge
