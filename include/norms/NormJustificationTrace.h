#pragma once

/**
 * @file NormJustificationTrace.h
 * @brief Phase 26: How NeuroForge Explains Its Norms
 *
 * This is how NeuroForge answers:
 * "Why do you think this norm is needed?"
 *
 * @invariant Justifications must be replay-backed
 * @invariant No invented moral reasoning
 */

#include <cstdint>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Norms {

/**
 * @brief Evidence type for norm justification
 */
enum class EvidenceType {
  REPLAY_OBSERVATION, ///< Seen in replay frames
  OUTCOME_PATTERN,    ///< Repeated outcomes
  GATE_ACTIVATION,    ///< Safety gate triggered
  VALUE_ALIGNMENT,    ///< External value enforced
  HUMAN_FEEDBACK      ///< Previous human input
};

/**
 * @brief A piece of evidence supporting a norm
 */
struct NormEvidence {
  EvidenceType type = EvidenceType::REPLAY_OBSERVATION;
  std::string description;
  std::uint64_t replay_frame_id = 0; ///< If applicable
  float weight = 1.0f;               ///< Evidence strength
};

/**
 * @brief Justification trace for a norm
 *
 * When asked "Why?", NeuroForge responds with:
 * - Evidence from behavior
 * - Contradictions found
 * - Empirical support score
 */
struct NormJustificationTrace {
  std::string norm_id;

  // Evidence chain
  std::vector<NormEvidence> supporting_evidence;
  std::vector<NormEvidence> contradicting_evidence;

  // Frame references
  std::vector<std::uint64_t> replay_frame_ids;

  // Summary statistics
  float empirical_support = 0.0f; ///< (supporting - contradicting) / total
  int violation_count = 0;
  int reinforcement_count = 0;

  // Human-readable explanation
  std::string summary;

  /**
   * @brief Calculate empirical support score
   */
  float calculateSupport() const {
    int total = supporting_evidence.size() + contradicting_evidence.size();
    if (total == 0)
      return 0.0f;

    float support = 0.0f;
    for (const auto &e : supporting_evidence)
      support += e.weight;
    for (const auto &e : contradicting_evidence)
      support -= e.weight;

    return support / static_cast<float>(total);
  }

  /**
   * @brief Generate summary explanation
   */
  std::string generateSummary() const {
    std::string result;

    result += "Norm: " + norm_id + "\n";
    result +=
        "Supporting evidence: " + std::to_string(supporting_evidence.size()) +
        "\n";
    result += "Contradicting evidence: " +
              std::to_string(contradicting_evidence.size()) + "\n";
    result += "Empirical support: " + std::to_string(empirical_support) + "\n";
    result += "Violations: " + std::to_string(violation_count) + "\n";
    result += "Reinforcements: " + std::to_string(reinforcement_count);

    return result;
  }
};

} // namespace Norms
} // namespace NeuroForge
