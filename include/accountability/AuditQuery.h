#pragma once

/**
 * @file AuditQuery.h
 * @brief Phase 30: Read-Only External Inspection
 *
 * This is the ONLY interface external systems get.
 * AuditQuery CANNOT modify state.
 *
 * @invariant Read-only
 * @invariant No side effects
 * @invariant Cannot issue commands
 */

#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Accountability {

/**
 * @brief Query parameters for audit inspection
 */
struct AuditQuery {
  // What to include
  bool include_actions = true;
  bool include_blocks = true;
  bool include_violations = true;

  // Filters
  std::string filter_role;
  std::string filter_contract;
  std::string filter_value;
  std::string filter_action_id;

  // Time range
  std::uint64_t start_time_ms = 0;
  std::uint64_t end_time_ms = 0; ///< 0 = no end limit

  // Pagination
  std::size_t max_results = 100;
  std::size_t offset = 0;
};

/**
 * @brief Result of audit query
 */
struct AuditQueryResult {
  std::size_t total_matching = 0;
  std::size_t returned = 0;
  bool has_more = false;

  // Summary statistics
  std::size_t actions_executed = 0;
  std::size_t actions_blocked = 0;
  std::size_t violations_detected = 0;
};

} // namespace Accountability
} // namespace NeuroForge
