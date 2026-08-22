#pragma once

/**
 * @file VocalFeedbackLoop.h
 * @brief Perception-Action Loop for Vocal Motor Learning
 *
 * Closes the loop:
 *   VocalMotorCortex → Sound → AuditoryCortex → VocalFeedbackLoop
 *   ↑_________________________________________________________________|
 *
 * This integrates with WorldModelCortex for prediction-based learning.
 */

#include "motor/VocalMotorCortex.h"
#include <functional>
#include <memory>

namespace NeuroForge {
namespace Motor {

/**
 * @brief Configuration for VocalFeedbackLoop
 */
struct VocalFeedbackLoopConfig {
  float self_voice_delay_ms = 20.0f; ///< Expected delay to hear own voice
  float self_detection_threshold =
      0.7f;                          ///< Confidence to mark as self-generated
  float feedback_latency_ms = 30.0f; ///< Processing latency tolerance
};

/**
 * @brief Vocal Feedback Loop
 *
 * Connects VocalMotorCortex to auditory perception for motor learning.
 * Distinguishes self-generated sounds from external sounds.
 */
class VocalFeedbackLoop {
public:
  using AudioProvider =
      std::function<std::vector<float>(std::uint64_t timestamp_ms)>;

  explicit VocalFeedbackLoop(VocalMotorCortex &motor,
                             const VocalFeedbackLoopConfig &config = {})
      : motor_(motor), config_(config) {

    // Wire motor output callback
    motor_.setActionCallback(
        [this](const VocalAction &action) { onMotorAction(action); });
  }

  /**
   * @brief Set audio provider (from AuditoryCortex or mic input)
   */
  void setAudioProvider(AudioProvider provider) {
    audio_provider_ = std::move(provider);
  }

  /**
   * @brief Process audio feedback at regular intervals
   *
   * Call this at ~20Hz to close the perception-action loop.
   */
  void processFeedback() {
    if (!audio_provider_ || !pending_action_)
      return;

    std::uint64_t now = getCurrentTimeMs();

    // Get observed audio
    std::vector<float> observed = audio_provider_(now);
    if (observed.empty())
      return;

    // Create feedback
    VocalFeedback feedback;
    feedback.timestamp_ms = now;
    feedback.observed_spectrum = observed;

    // Extract pitch/amplitude from spectrum
    extractAudioFeatures(observed, feedback);

    // Self-generated detection
    feedback.is_self_generated =
        detectSelfGenerated(*pending_action_, feedback, now);
    feedback.self_match_confidence =
        computeSelfMatchConfidence(*pending_action_, feedback);

    // Compute prediction error
    feedback.prediction_error =
        computePredictionError(*pending_action_, feedback);

    // Send to motor cortex
    motor_.receiveFeedback(feedback);

    // Track for WorldModelCortex integration
    last_feedback_ = feedback;
    pending_action_.reset();
    feedback_count_++;
  }

  /**
   * @brief Get last feedback for WorldModelCortex integration
   */
  const VocalFeedback &getLastFeedback() const { return last_feedback_; }
  std::uint64_t getFeedbackCount() const { return feedback_count_; }

private:
  VocalMotorCortex &motor_;
  VocalFeedbackLoopConfig config_;
  AudioProvider audio_provider_;

  std::optional<VocalAction> pending_action_;
  std::uint64_t pending_action_time_ = 0;
  VocalFeedback last_feedback_;
  std::uint64_t feedback_count_ = 0;

  void onMotorAction(const VocalAction &action) {
    pending_action_ = action;
    pending_action_time_ = action.timestamp_ms;
  }

  void extractAudioFeatures(const std::vector<float> &spectrum,
                            VocalFeedback &feedback) {
    // Simple feature extraction (would be more sophisticated in practice)
    if (spectrum.size() >= 2) {
      feedback.observed_pitch =
          100.0f + 300.0f * std::abs(spectrum[0]); // Crude pitch estimate
      feedback.observed_amplitude = std::abs(spectrum[1]);
    }
  }

  bool detectSelfGenerated(const VocalAction &action,
                           const VocalFeedback &feedback,
                           std::uint64_t now_ms) {
    // Self-voice detection based on timing and prediction
    float delay = static_cast<float>(now_ms - pending_action_time_);
    bool timing_ok = std::abs(delay - config_.self_voice_delay_ms) <
                     config_.feedback_latency_ms;

    float match = computeSelfMatchConfidence(action, feedback);
    return timing_ok && match > config_.self_detection_threshold;
  }

  float computeSelfMatchConfidence(const VocalAction &action,
                                   const VocalFeedback &feedback) {
    // Compare action parameters to observed feedback
    float pitch_match =
        1.0f -
        std::min(std::abs(action.pitch - feedback.observed_pitch) / 200.0f,
                 1.0f);
    float amp_match =
        1.0f - std::abs(action.amplitude - feedback.observed_amplitude);

    return (pitch_match + amp_match) / 2.0f;
  }

  float computePredictionError(const VocalAction &action,
                               const VocalFeedback &feedback) {
    float pitch_err = std::abs(action.pitch - feedback.observed_pitch) / 400.0f;
    float amp_err = std::abs(action.amplitude - feedback.observed_amplitude);

    float spectral_err = 0.0f;
    std::size_t n = std::min(action.spectral_envelope.size(),
                             feedback.observed_spectrum.size());
    for (std::size_t i = 0; i < n; ++i) {
      spectral_err +=
          std::abs(action.spectral_envelope[i] - feedback.observed_spectrum[i]);
    }
    spectral_err /= std::max(n, std::size_t(1));

    return (pitch_err + amp_err + spectral_err) / 3.0f;
  }

  std::uint64_t getCurrentTimeMs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
  }
};

} // namespace Motor
} // namespace NeuroForge
