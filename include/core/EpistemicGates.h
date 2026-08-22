#pragma once

#include <string>

namespace NeuroForge {
namespace Core {

/**
 * @brief Phase 17: Epistemic Readiness Report
 *
 * Evaluates whether the system is healthy enough to perform
 * verification actions. Prevents the "Drunk Librarian" problem.
 */
struct ReadinessReport {
  bool is_ready = false;
  std::string denial_reason;

  // Metrics
  int provisional_count = 0;
  float signal_to_noise = 0.0f;
  bool has_contradictions = false;
};

/**
 * @brief Phase 17: Epistemic Readiness Gates
 *
 * Gatekeeper that evaluates the system's internal state before
 * allowing any transition to active verification.
 *
 * The Three Gates:
 * 1. Signal Gate: Enough provisional facts to form a hypothesis?
 * 2. Clarity Gate: Healthy acceptance ratio (not drowning in noise)?
 * 3. Stability Gate: No unresolved contradictions?
 */
class EpistemicGates {
public:
  // Configuration thresholds
  struct Config {
    int min_provisional_count = 5;    ///< Signal Gate threshold
    float min_signal_to_noise = 0.1f; ///< Clarity Gate threshold (10%)

    Config() = default;
  };

  EpistemicGates() : config_() {}
  explicit EpistemicGates(const Config &config) : config_(config) {}

  /**
   * @brief The Master Switch - check if system is ready for verification
   *
   * @param provisional_count Number of provisional facts in the system
   * @param accepted_count Number of accepted relations
   * @param rejected_count Number of rejected relations
   * @param has_contradictions Whether there are unresolved contradictions
   * @return ReadinessReport with decision and metrics
   */
  ReadinessReport checkReadiness(int provisional_count, int accepted_count,
                                 int rejected_count,
                                 bool has_contradictions) const;

private:
  Config config_;
};

} // namespace Core
} // namespace NeuroForge
