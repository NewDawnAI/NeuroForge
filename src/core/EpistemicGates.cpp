#include "core/EpistemicGates.h"

namespace NeuroForge {
namespace Core {

ReadinessReport EpistemicGates::checkReadiness(int provisional_count,
                                               int accepted_count,
                                               int rejected_count,
                                               bool has_contradictions) const {

  ReadinessReport report;
  report.provisional_count = provisional_count;
  report.has_contradictions = has_contradictions;

  // 1. THE SIGNAL GATE (Minimum Clues)
  // We need at least N provisional facts to believe we understand
  // the topic enough to verify it. If we only found 1 clue, it might
  // be a parsing error.
  if (provisional_count < config_.min_provisional_count) {
    report.is_ready = false;
    report.denial_reason = "INSUFFICIENT_SIGNAL (Provisional Count < " +
                           std::to_string(config_.min_provisional_count) + ")";
    return report;
  }

  // 2. THE CLARITY GATE (Signal-to-Noise Ratio)
  // If we rejected 500 sentences and accepted 5, we are likely reading garbage.
  // Avoid verifying garbage.
  float total_attempts = static_cast<float>(accepted_count + rejected_count);
  if (total_attempts > 0) {
    report.signal_to_noise =
        static_cast<float>(accepted_count) / total_attempts;

    if (report.signal_to_noise < config_.min_signal_to_noise) {
      report.is_ready = false;
      report.denial_reason = "POOR_PERCEPTION_HEALTH (Signal/Noise < " +
                             std::to_string(config_.min_signal_to_noise) + ")";
      return report;
    }
  } else {
    // No attempts yet - not ready
    report.is_ready = false;
    report.denial_reason = "NO_PERCEPTION_DATA";
    return report;
  }

  // 3. THE STABILITY GATE (Contradictions)
  // If the brain is fighting itself, resolve that first.
  if (has_contradictions) {
    report.is_ready = false;
    report.denial_reason = "EPISTEMIC_INSTABILITY (Active Contradictions)";
    return report;
  }

  // If we pass all gates:
  report.is_ready = true;
  report.denial_reason = "READY";
  return report;
}

} // namespace Core
} // namespace NeuroForge
