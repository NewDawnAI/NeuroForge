#pragma once

#include "arbitration/ArbitrationResult.h"
#include "arbitration/ArbitrationScore.h"
#include "arbitration/RegionIntent.h"


#include <chrono>
#include <iostream>
#include <vector>


namespace NeuroForge {
namespace Arbitration {

/**
 * @brief Phase 22: The Arbiter ("Prefrontal Cortex")
 *
 * Decides which subsystem's intent wins each cycle.
 * All intents compete lawfully - even Safety must win by score
 * (unless it's a hard veto).
 */
class ArbitrationEngine {
public:
  ArbitrationEngine() : next_cycle_id_(1) {}

  /**
   * @brief Select the winning action from competing intents
   */
  ArbitrationResult selectAction(const std::vector<RegionIntent> &intents) {
    ArbitrationResult result;
    result.cycle_id = next_cycle_id_++;
    result.timestamp_ms = getCurrentTimestamp();
    result.proposals = intents;

    // Check for hard veto first
    for (const auto &intent : intents) {
      if (intent.is_veto) {
        result.vetoed = true;
        result.veto_reason = intent.description;
        result.selected = intent;
        result.selected_score = 999.0f; // Veto wins absolutely

        std::cout << "  [Arbiter] VETOED by " << regionToString(intent.region)
                  << ": " << intent.description << "\n";

        history_.push_back(result);
        return result;
      }
    }

    // Score-based competition
    float best_score = -1.0f;
    const RegionIntent *winner = nullptr;

    for (const auto &intent : intents) {
      ArbitrationScore score{intent.epistemic_value, intent.estimated_cost,
                             intent.urgency};

      float total = score.total();
      if (total > best_score) {
        best_score = total;
        winner = &intent;
      }
    }

    if (winner) {
      result.selected = *winner;
      result.selected_score = best_score;

      std::cout << "  [Arbiter] Selected: " << regionToString(winner->region)
                << " (score=" << std::fixed << std::setprecision(2)
                << best_score << ") - " << winner->description << "\n";
    } else {
      std::cout << "  [Arbiter] No valid intents\n";
    }

    history_.push_back(result);
    return result;
  }

  /**
   * @brief Get arbitration history for replay
   */
  const std::vector<ArbitrationResult> &getHistory() const { return history_; }

  /**
   * @brief Clear history
   */
  void clearHistory() { history_.clear(); }

private:
  static std::uint64_t getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  std::vector<ArbitrationResult> history_;
  std::uint64_t next_cycle_id_;
};

} // namespace Arbitration
} // namespace NeuroForge
