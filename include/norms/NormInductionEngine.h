#pragma once

#include "actuation/ReplayFrame.h"
#include "norms/Norm.h"
#include "norms/NormStore.h"


#include <iostream>
#include <string>
#include <unordered_map>


namespace NeuroForge {
namespace Norms {

/**
 * @brief Phase 24: Norm Induction Engine
 *
 * Passive, conservative norm learning from ReplayFrames.
 * Norms emerge from behavior, not from text.
 *
 * Norms decay without evidence.
 * Nothing is permanent by default.
 */
class NormInductionEngine {
public:
  static constexpr int BLOCK_THRESHOLD = 3; ///< Blocks → propose HARD norm
  static constexpr int DISCOURAGE_THRESHOLD =
      5; ///< Discourages → propose SOFT norm

  explicit NormInductionEngine(NormStore &store) : store_(store) {}

  /**
   * @brief Observe a replay frame and potentially induce norms
   */
  void observeReplay(const Actuation::ReplayFrame &frame) {
    std::string context = getContextFromFrame(frame);

    // Track action attempts and outcomes
    auto &stats = action_stats_[context];
    stats.total_attempts++;

    if (!frame.success) {
      stats.failure_count++;
    }

    if (frame.notes.find("Gate blocked") != std::string::npos ||
        frame.notes.find("BLOCKED") != std::string::npos) {
      stats.block_count++;
    }

    // Induce norms when thresholds are met
    if (stats.block_count >= BLOCK_THRESHOLD && !stats.hard_norm_proposed) {
      proposeNorm(context, NormStrength::HARD, NormDecision::FORBID,
                  "Repeated gate blocks indicate unsafe pattern");
      stats.hard_norm_proposed = true;
      std::cout << "  [NormInduction] Proposed HARD norm for: " << context
                << "\n";
    } else if (stats.failure_count >= DISCOURAGE_THRESHOLD &&
               !stats.soft_norm_proposed) {
      proposeNorm(context, NormStrength::SOFT, NormDecision::DISCOURAGE,
                  "Repeated failures suggest caution");
      stats.soft_norm_proposed = true;
      std::cout << "  [NormInduction] Proposed SOFT norm for: " << context
                << "\n";
    }
  }

  /**
   * @brief Decay norms that haven't been reinforced recently
   */
  void decayUnreinforcedNorms() {
    // This would be called periodically to remove stale norms
    // For now, a placeholder for the decay mechanism
  }

private:
  struct ActionStats {
    int total_attempts = 0;
    int failure_count = 0;
    int block_count = 0;
    bool hard_norm_proposed = false;
    bool soft_norm_proposed = false;
  };

  void proposeNorm(const std::string &context, NormStrength strength,
                   NormDecision decision, const std::string &justification) {
    Norm norm;
    norm.norm_id = "learned_" + context + "_" + std::to_string(next_norm_id_++);
    norm.context_signature = context;
    norm.strength = strength;
    norm.decision = decision;
    norm.justification = justification;
    norm.source = "learned";
    norm.priority = (strength == NormStrength::HARD) ? 100 : 50;

    store_.addOrUpdateNorm(norm);
  }

  std::string getContextFromFrame(const Actuation::ReplayFrame &frame) const {
    switch (frame.action.kind) {
    case Actuation::ActionKind::SPEAK:
      return "speech";
    case Actuation::ActionKind::NAVIGATE:
    case Actuation::ActionKind::SEARCH:
      return "external_action";
    case Actuation::ActionKind::MANIPULATE:
      return "embodiment";
    default:
      return "general";
    }
  }

  NormStore &store_;
  std::unordered_map<std::string, ActionStats> action_stats_;
  int next_norm_id_ = 1;
};

} // namespace Norms
} // namespace NeuroForge
