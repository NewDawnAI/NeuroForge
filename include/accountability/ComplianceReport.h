#pragma once

/**
 * @file ComplianceReport.h
 * @brief Phase 30: Formal Export for Regulators/Institutions
 *
 * Reports are DERIVED, never authoritative.
 * Reports OBSERVE, they never DECIDE.
 *
 * @invariant Reports cannot change behavior
 * @invariant Reports cannot issue commands
 */

#include <cstdint>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Accountability {

/**
 * @brief Type of compliance report
 */
enum class ReportType {
  GENERAL,    ///< General audit report
  INCIDENT,   ///< Specific incident analysis
  REGULATORY, ///< For regulatory submission
  CONTRACTUAL ///< For contract compliance
};

/**
 * @brief Formal compliance report
 */
struct ComplianceReport {
  std::string report_id;
  ReportType type = ReportType::GENERAL;

  // Scope
  std::string scope;       ///< "GDPR", "Institutional Audit", etc.
  std::string time_period; ///< "2026-01-01 to 2026-01-17"

  // Statistics
  std::size_t actions_executed = 0;
  std::size_t actions_blocked = 0;
  std::size_t violations_detected = 0;
  std::size_t liabilities_attached = 0;

  // Breakdown
  std::vector<std::string> roles_active;
  std::vector<std::string> contracts_active;
  std::vector<std::string> unresolved_liabilities;

  // Summary
  std::string summary;
  std::string recommendation;

  // Metadata
  std::uint64_t generated_at_ms = 0;
  std::string generated_by; ///< "system", not a user

  /**
   * @brief Convert report type to string
   */
  static std::string typeToString(ReportType t) {
    switch (t) {
    case ReportType::GENERAL:
      return "general";
    case ReportType::INCIDENT:
      return "incident";
    case ReportType::REGULATORY:
      return "regulatory";
    case ReportType::CONTRACTUAL:
      return "contractual";
    default:
      return "unknown";
    }
  }
};

} // namespace Accountability
} // namespace NeuroForge
