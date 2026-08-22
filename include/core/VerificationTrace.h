#pragma once

#include <string>
#include <vector>

namespace NeuroForge {
namespace Core {

/**
 * @brief Phase 20a: Status of a verification frame
 */
enum class TraceStatus {
  PENDING,  ///< Queued for verification
  FETCHED,  ///< URL has been fetched
  PROMOTED, ///< Fact was promoted to CONFIRMED
  FAILED,   ///< Verification failed (no match)
  ABANDONED ///< Abandoned (depth limit, no source, etc.)
};

/**
 * @brief Convert TraceStatus to string
 */
inline std::string traceStatusToString(TraceStatus status) {
  switch (status) {
  case TraceStatus::PENDING:
    return "PENDING";
  case TraceStatus::FETCHED:
    return "FETCHED";
  case TraceStatus::PROMOTED:
    return "PROMOTED";
  case TraceStatus::FAILED:
    return "FAILED";
  case TraceStatus::ABANDONED:
    return "ABANDONED";
  default:
    return "UNKNOWN";
  }
}

/**
 * @brief Phase 20a: Single node in the verification trace tree
 *
 * Represents one verification attempt with parent/child lineage.
 */
struct VerificationTraceNode {
  int frame_id = 0;
  int parent_frame_id = -1; ///< -1 for root

  std::string subject;
  std::string predicate;
  std::string object;

  int depth = 0;
  TraceStatus status = TraceStatus::PENDING;

  std::string source_url;
  std::string source_domain_class;

  float epistemic_value = 0.0f;
  float verification_cost = 0.0f;
  float roi_score = 0.0f;

  std::vector<int> children; ///< Child frame_ids
};

} // namespace Core
} // namespace NeuroForge
