#pragma once

#include "norms/Norm.h"
#include "norms/NormStore.h"
#include "norms/NormativeJudgment.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Norms {

/**
 * @brief Phase 24: Norm Metrics Dashboard
 *
 * Tracks and displays norm enforcement metrics for monitoring.
 */
struct NormMetrics {
  // Basic counts
  int total_evaluations = 0;
  int permitted_count = 0;
  int blocked_count = 0;
  int discouraged_count = 0;

  // By strength
  int soft_blocks = 0;
  int hard_blocks = 0;
  int absolute_blocks = 0;

  // Override tracking
  int soft_overrides = 0;
  int hard_conflicts = 0;

  // Norm store stats
  int active_norms = 0;
  int total_reinforcements = 0;
  int total_violations = 0;
};

/**
 * @brief Phase 24: Norm Decision Log Entry
 */
struct NormDecisionLogEntry {
  std::uint64_t timestamp_ms = 0;
  std::string action_description;
  std::string context;
  NormDecision decision = NormDecision::ALLOW;
  std::vector<std::string> blocking_norm_ids;
  std::string justification;
};

/**
 * @brief Phase 24: Norm Metrics Dashboard
 */
class NormMetricsDashboard {
public:
  /**
   * @brief Record a normative judgment
   */
  void recordJudgment(const NormativeJudgment &judgment,
                      const std::string &action_desc) {
    metrics_.total_evaluations++;

    NormDecisionLogEntry entry;
    entry.timestamp_ms = getCurrentTimestamp();
    entry.action_description = action_desc;

    if (!judgment.permitted) {
      metrics_.blocked_count++;
      entry.decision = NormDecision::FORBID;

      for (const auto &norm : judgment.blocking_norms) {
        entry.blocking_norm_ids.push_back(norm.norm_id);

        switch (norm.strength) {
        case NormStrength::SOFT:
          metrics_.soft_blocks++;
          break;
        case NormStrength::HARD:
          metrics_.hard_blocks++;
          break;
        case NormStrength::ABSOLUTE:
          metrics_.absolute_blocks++;
          break;
        }
      }
    } else if (judgment.discouraged) {
      metrics_.discouraged_count++;
      entry.decision = NormDecision::DISCOURAGE;
      metrics_.soft_overrides++;
    } else {
      metrics_.permitted_count++;
      entry.decision = NormDecision::ALLOW;
    }

    if (judgment.blocking_norms.size() >= 2) {
      // Check for HARD/HARD conflict
      int hard_count = 0;
      for (const auto &n : judgment.blocking_norms) {
        if (n.strength == NormStrength::HARD)
          hard_count++;
      }
      if (hard_count >= 2)
        metrics_.hard_conflicts++;
    }

    decision_log_.push_back(entry);
  }

  /**
   * @brief Update store statistics
   */
  void updateStoreStats(const NormStore &store) {
    metrics_.active_norms = static_cast<int>(store.count());
    metrics_.total_reinforcements = 0;
    metrics_.total_violations = 0;

    for (const auto &norm : store.getAllNorms()) {
      metrics_.total_reinforcements += norm.reinforcement_count;
      metrics_.total_violations += norm.violation_count;
    }
  }

  /**
   * @brief Print dashboard to console
   */
  void printDashboard() const {
    std::cout << "\n";
    std::cout
        << "╔══════════════════════════════════════════════════════════╗\n";
    std::cout
        << "║          PHASE 24 NORMATIVE METRICS DASHBOARD            ║\n";
    std::cout
        << "╠══════════════════════════════════════════════════════════╣\n";

    std::cout << "║  Total Evaluations: " << std::setw(6)
              << metrics_.total_evaluations;
    std::cout << "                              ║\n";

    std::cout
        << "╠══════════════════════════════════════════════════════════╣\n";
    std::cout
        << "║  DECISIONS                                               ║\n";
    std::cout << "║    Permitted:   " << std::setw(6)
              << metrics_.permitted_count;
    float permit_rate =
        metrics_.total_evaluations > 0
            ? 100.0f * metrics_.permitted_count / metrics_.total_evaluations
            : 0;
    std::cout << "  (" << std::fixed << std::setprecision(1) << permit_rate
              << "%)";
    std::cout << "                        ║\n";

    std::cout << "║    Blocked:     " << std::setw(6) << metrics_.blocked_count;
    float block_rate =
        metrics_.total_evaluations > 0
            ? 100.0f * metrics_.blocked_count / metrics_.total_evaluations
            : 0;
    std::cout << "  (" << std::fixed << std::setprecision(1) << block_rate
              << "%)";
    std::cout << "                        ║\n";

    std::cout << "║    Discouraged: " << std::setw(6)
              << metrics_.discouraged_count;
    std::cout << "                                    ║\n";

    std::cout
        << "╠══════════════════════════════════════════════════════════╣\n";
    std::cout
        << "║  BLOCKS BY STRENGTH                                      ║\n";
    std::cout << "║    SOFT:     " << std::setw(6) << metrics_.soft_blocks;
    std::cout << "                                       ║\n";
    std::cout << "║    HARD:     " << std::setw(6) << metrics_.hard_blocks;
    std::cout << "                                       ║\n";
    std::cout << "║    ABSOLUTE: " << std::setw(6) << metrics_.absolute_blocks;
    std::cout << "                                       ║\n";

    std::cout
        << "╠══════════════════════════════════════════════════════════╣\n";
    std::cout
        << "║  SPECIAL EVENTS                                          ║\n";
    std::cout << "║    SOFT Overrides:   " << std::setw(6)
              << metrics_.soft_overrides;
    std::cout << "                            ║\n";
    std::cout << "║    HARD Conflicts:   " << std::setw(6)
              << metrics_.hard_conflicts;
    std::cout << "                            ║\n";

    std::cout
        << "╠══════════════════════════════════════════════════════════╣\n";
    std::cout
        << "║  NORM STORE                                              ║\n";
    std::cout << "║    Active Norms:     " << std::setw(6)
              << metrics_.active_norms;
    std::cout << "                            ║\n";
    std::cout << "║    Reinforcements:   " << std::setw(6)
              << metrics_.total_reinforcements;
    std::cout << "                            ║\n";
    std::cout << "║    Violations:       " << std::setw(6)
              << metrics_.total_violations;
    std::cout << "                            ║\n";

    std::cout
        << "╚══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
  }

  /**
   * @brief Export metrics as JSON
   */
  std::string exportJSON() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"total_evaluations\": " << metrics_.total_evaluations << ",\n";
    ss << "  \"permitted\": " << metrics_.permitted_count << ",\n";
    ss << "  \"blocked\": " << metrics_.blocked_count << ",\n";
    ss << "  \"discouraged\": " << metrics_.discouraged_count << ",\n";
    ss << "  \"soft_blocks\": " << metrics_.soft_blocks << ",\n";
    ss << "  \"hard_blocks\": " << metrics_.hard_blocks << ",\n";
    ss << "  \"absolute_blocks\": " << metrics_.absolute_blocks << ",\n";
    ss << "  \"soft_overrides\": " << metrics_.soft_overrides << ",\n";
    ss << "  \"hard_conflicts\": " << metrics_.hard_conflicts << ",\n";
    ss << "  \"active_norms\": " << metrics_.active_norms << "\n";
    ss << "}\n";
    return ss.str();
  }

  /**
   * @brief Get metrics
   */
  const NormMetrics &getMetrics() const { return metrics_; }

  /**
   * @brief Clear all metrics
   */
  void reset() {
    metrics_ = NormMetrics{};
    decision_log_.clear();
  }

private:
  static std::uint64_t getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  NormMetrics metrics_;
  std::vector<NormDecisionLogEntry> decision_log_;
};

} // namespace Norms
} // namespace NeuroForge
