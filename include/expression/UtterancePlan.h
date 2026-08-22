#pragma once

/**
 * @file UtterancePlan.h
 * @brief Phase E1: Planned utterance with evidence
 *
 * A complete utterance plan with traceability to replay frames.
 */

#include "LexicalUnit.h"
#include <cstdint>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Expression {

/**
 * @brief Normative status of an utterance
 */
enum class NormativeStatus {
  PERMITTED,  ///< Passes all norm/value checks
  RESTRICTED, ///< Some content filtered
  BLOCKED     ///< Should not be expressed
};

/**
 * @brief A planned utterance with evidence chain
 */
struct UtterancePlan {
  std::vector<LexicalUnit> tokens;         ///< Ordered lexical units
  std::vector<std::uint64_t> evidence_ids; ///< ReplayFrame IDs as evidence
  std::vector<std::uint64_t> concept_ids;  ///< Concepts referenced
  NormativeStatus status = NormativeStatus::PERMITTED;
  std::string blocked_reason; ///< If blocked, why
  float overall_confidence = 0.0f;

  /**
   * @brief Render to plain text
   */
  std::string render() const {
    std::string result;
    for (size_t i = 0; i < tokens.size(); i++) {
      if (i > 0)
        result += " ";
      result += tokens[i].surface_form;
    }
    return result;
  }

  /**
   * @brief Check if utterance is safe to express
   */
  bool canExpress() const {
    return status == NormativeStatus::PERMITTED ||
           status == NormativeStatus::RESTRICTED;
  }

  /**
   * @brief Calculate average confidence
   */
  void calculateConfidence() {
    if (tokens.empty()) {
      overall_confidence = 0.0f;
      return;
    }
    float sum = 0.0f;
    int grounded = 0;
    for (const auto &t : tokens) {
      if (t.is_grounded) {
        sum += t.confidence;
        grounded++;
      }
    }
    overall_confidence = grounded > 0 ? sum / grounded : 0.5f;
  }

  /**
   * @brief Status to string
   */
  static std::string statusToString(NormativeStatus s) {
    switch (s) {
    case NormativeStatus::PERMITTED:
      return "PERMITTED";
    case NormativeStatus::RESTRICTED:
      return "RESTRICTED";
    case NormativeStatus::BLOCKED:
      return "BLOCKED";
    default:
      return "UNKNOWN";
    }
  }
};

} // namespace Expression
} // namespace NeuroForge
