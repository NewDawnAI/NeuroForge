#pragma once

/**
 * @file ExpressionReplayFrame.h
 * @brief Phase E2: Auditable expression events
 *
 * Every speech/text output is logged for accountability.
 */

#include "../expression/UtterancePlan.h"
#include <cstdint>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Expression {

/**
 * @brief Auditable record of an expression event
 */
struct ExpressionAuditRecord {
  std::uint64_t record_id = 0;
  std::uint64_t timestamp_ms = 0;

  // What was expressed
  std::string output_text;
  ExpressionType expression_type = ExpressionType::DESCRIBE;

  // Evidence chain
  std::vector<std::uint64_t> concept_ids;
  std::vector<std::uint64_t> evidence_frame_ids;

  // Normative context
  std::vector<std::string> norms_applied;
  std::vector<std::string> values_checked;
  std::vector<std::string> roles_active;

  // Status
  NormativeStatus status = NormativeStatus::PERMITTED;
  std::string blocked_reason;

  // Confidence metrics
  float overall_confidence = 0.0f;
  int grounded_tokens = 0;
  int total_tokens = 0;
};

} // namespace Expression
} // namespace NeuroForge
