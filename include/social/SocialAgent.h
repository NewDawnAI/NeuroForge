#pragma once

/**
 * @file SocialAgent.h
 * @brief Phase 27: Multi-Agent Social Norms - Agent Identity & Trust
 *
 * Represents another agent in the social environment.
 * Trust is EARNED via replay consistency, never declared.
 *
 * @invariant Agent identity ≠ authority
 * @invariant Trust is replay-backed, not claimed
 */

#include <cstdint>
#include <string>


namespace NeuroForge {
namespace Social {

/**
 * @brief Declared role of a social agent
 * Role is self-declared but does NOT confer authority
 */
enum class AgentRole {
  UNKNOWN,    ///< Role not specified
  RESEARCHER, ///< Claims to do research
  AUDITOR,    ///< Claims to audit
  ASSISTANT,  ///< Claims to assist
  PEER,       ///< Equal standing
  REGULATOR   ///< Claims regulatory authority (verified via values)
};

/**
 * @brief Trust level for a social agent
 */
enum class TrustLevel {
  UNTRUSTED, ///< No interactions yet
  LOW,       ///< Few or inconsistent interactions
  MEDIUM,    ///< Some verified interactions
  HIGH,      ///< Many consistent, verified interactions
  VERIFIED   ///< Trust backed by external value source
};

/**
 * @brief A social agent in the multi-agent environment
 *
 * Trust is EARNED only via replay consistency.
 * Declared role does NOT confer authority.
 */
struct SocialAgent {
  std::string agent_id;          ///< Unique identifier
  std::string declared_role_str; ///< Self-declared role (unverified)
  AgentRole role = AgentRole::UNKNOWN;

  // Learned metrics (NOT declared)
  float epistemic_reliability = 0.0f; ///< Accuracy of past claims
  float norm_alignment_score = 0.0f;  ///< Compatibility with our norms
  float interaction_trust = 0.0f;     ///< Overall trust score [0,1]

  // Interaction history
  int total_interactions = 0;
  int verified_claims = 0;     ///< Claims later confirmed
  int contradicted_claims = 0; ///< Claims later refuted
  int norm_proposals_accepted = 0;
  int norm_proposals_rejected = 0;

  // Timestamps
  std::uint64_t first_seen_ms = 0;
  std::uint64_t last_seen_ms = 0;

  /**
   * @brief Calculate trust level from interaction history
   */
  TrustLevel calculateTrustLevel() const {
    if (total_interactions == 0)
      return TrustLevel::UNTRUSTED;

    float trust = interaction_trust;
    if (trust < 0.2f)
      return TrustLevel::UNTRUSTED;
    if (trust < 0.4f)
      return TrustLevel::LOW;
    if (trust < 0.6f)
      return TrustLevel::MEDIUM;
    if (trust < 0.8f)
      return TrustLevel::HIGH;
    return TrustLevel::VERIFIED;
  }

  /**
   * @brief Calculate reliability from verified/contradicted claims
   */
  float calculateReliability() const {
    int total = verified_claims + contradicted_claims;
    if (total == 0)
      return 0.0f;
    return static_cast<float>(verified_claims) / static_cast<float>(total);
  }

  /**
   * @brief Convert role to string
   */
  static std::string roleToString(AgentRole r) {
    switch (r) {
    case AgentRole::UNKNOWN:
      return "unknown";
    case AgentRole::RESEARCHER:
      return "researcher";
    case AgentRole::AUDITOR:
      return "auditor";
    case AgentRole::ASSISTANT:
      return "assistant";
    case AgentRole::PEER:
      return "peer";
    case AgentRole::REGULATOR:
      return "regulator";
    default:
      return "unknown";
    }
  }
};

} // namespace Social
} // namespace NeuroForge
