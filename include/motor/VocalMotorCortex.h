#pragma once

/**
 * @file VocalMotorCortex.h
 * @brief Motor Cortex for Vocal Output (Phase-21 Compatible)
 *
 * Voice is a MOTOR modality, not language generation.
 * This cortex produces raw acoustic frames that:
 *   1. Learn from auditory feedback (perception-action loop)
 *   2. Start with babbling, develop to prosody, then syllables
 *   3. Are eventually biased (but not controlled) by language intent
 *
 * Key invariant:
 *   "Language never directly emits sound. Only motor actions do."
 */

#include "motor/VocalAction.h"
#include <algorithm>
#include <cmath>
#include <deque>
#include <functional>
#include <mutex>
#include <numeric>
#include <random>

namespace NeuroForge {
namespace Motor {

/**
 * @brief Configuration for VocalMotorCortex
 */
struct VocalMotorCortexConfig {
  VocalDevelopmentConfig development;

  // Output configuration
  std::size_t spectral_dim = 13; ///< MFCC-like dimensions
  float action_rate_hz = 20.0f;  ///< Actions per second (50ms window)

  // Feedback integration
  float feedback_weight = 0.3f; ///< How much feedback affects next action
  float momentum = 0.8f;        ///< Temporal smoothing of motor output

  // Reward shaping
  float novelty_bonus = 0.1f; ///< Reward for exploring new sounds
  float prediction_error_penalty = 0.5f;

  // Language bias (only in late stage)
  float language_bias_weight = 0.0f; ///< Starts at 0, increases developmentally
  float max_language_bias = 0.3f;    ///< Never exceeds this (voice ≠ language)
};

/**
 * @brief Vocal Motor Cortex
 *
 * Implements developmental vocal learning:
 *   Babbling → Prosody → Syllables → Proto-words
 *
 * Perception-Action Loop:
 *   VocalMotorCortex → Sound → AuditoryCortex → WorldState → Prediction Error
 *   ↑___________________________________________________________|
 */
class VocalMotorCortex {
public:
  using FeedbackCallback = std::function<void(const VocalFeedback &)>;
  using ActionCallback = std::function<void(const VocalAction &)>;

  explicit VocalMotorCortex(const VocalMotorCortexConfig &config = {})
      : config_(config), stage_(config.development.initial_stage),
        rng_(std::random_device{}()) {
    initializeMotorState();
  }

  /**
   * @brief Generate next vocal action
   *
   * Called at action_rate_hz to produce continuous motor output.
   * Early stages are pure babbling; later stages integrate feedback.
   */
  VocalAction generateAction() {
    std::lock_guard<std::mutex> lock(mutex_);

    VocalAction action;
    action.timestamp_ms = getCurrentTimeMs();
    action.spectral_envelope.resize(config_.spectral_dim);

    switch (stage_) {
    case VocalStage::BABBLING:
      generateBabblingAction(action);
      break;
    case VocalStage::PROSODY:
      generateProsodyAction(action);
      break;
    case VocalStage::SYLLABIC:
      generateSyllabicAction(action);
      break;
    case VocalStage::PROTO_WORD:
    case VocalStage::LANGUAGE_BIASED:
      generateControlledAction(action);
      break;
    }

    // Apply temporal smoothing
    applyMomentum(action);

    // Store for feedback comparison
    pending_action_ = action;
    action_count_++;

    // Fire callback
    if (action_callback_) {
      action_callback_(action);
    }

    return action;
  }

  /**
   * @brief Receive auditory feedback from AuditoryCortex
   *
   * This closes the perception-action loop:
   *   Action → Sound → Perception → Feedback → Motor Update
   */
  void receiveFeedback(const VocalFeedback &feedback) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Compute motor prediction error
    float error = computePredictionError(pending_action_, feedback);
    total_prediction_error_ += error;

    // Update motor model from feedback
    updateMotorModel(feedback, error);

    // Track for stage transitions
    recent_errors_.push_back(error);
    if (recent_errors_.size() > 100) {
      recent_errors_.pop_front();
    }

    // Check for stage advancement
    checkStageTransition();

    // Fire callback
    if (feedback_callback_) {
      feedback_callback_(feedback);
    }
  }

  /**
   * @brief Receive language bias (only in late stages)
   *
   * Language can BIAS motor intent, but never directly produce sound.
   * This is a weak input, not a command.
   *
   * @param intent Vector from LanguageExpressionCortex
   */
  void receiveLinguisticBias(const std::vector<float> &intent) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (stage_ != VocalStage::LANGUAGE_BIASED) {
      return; // Ignore until late development
    }

    linguistic_intent_ = intent;
  }

  // Callbacks
  void setFeedbackCallback(FeedbackCallback cb) { feedback_callback_ = cb; }
  void setActionCallback(ActionCallback cb) { action_callback_ = cb; }

  // Accessors
  VocalStage getStage() const { return stage_; }
  float getAveragePredictionError() const {
    return recent_errors_.empty()
               ? 1.0f
               : std::accumulate(recent_errors_.begin(), recent_errors_.end(),
                                 0.0f) /
                     recent_errors_.size();
  }
  std::uint64_t getActionCount() const { return action_count_; }

  const VocalMotorCortexConfig &getConfig() const { return config_; }

private:
  VocalMotorCortexConfig config_;
  VocalStage stage_;
  std::mt19937 rng_;
  mutable std::mutex mutex_;

  // Motor state
  VocalAction last_action_;
  VocalAction pending_action_;
  std::vector<float> motor_mean_; ///< Running average of motor output
  std::vector<float> motor_variance_;

  // Feedback tracking
  std::deque<float> recent_errors_;
  float total_prediction_error_ = 0.0f;

  // Language bias (late stage only)
  std::vector<float> linguistic_intent_;

  // Callbacks
  FeedbackCallback feedback_callback_;
  ActionCallback action_callback_;

  std::uint64_t action_count_ = 0;

  void initializeMotorState() {
    motor_mean_.resize(config_.spectral_dim, 0.0f);
    motor_variance_.resize(config_.spectral_dim, 1.0f);
  }

  void generateBabblingAction(VocalAction &action) {
    action.is_babbling = true;
    action.exploration_level = config_.development.babbling_exploration;

    // Random pitch exploration
    std::normal_distribution<float> pitch_dist(
        220.0f, config_.development.babbling_pitch_range);
    action.pitch = std::clamp(pitch_dist(rng_), 50.0f, 800.0f);

    // Random amplitude (with bursts)
    std::uniform_real_distribution<float> amp_dist(0.1f, 0.7f);
    action.amplitude = amp_dist(rng_);

    // Random spectral exploration
    std::normal_distribution<float> spec_dist(0.0f, 0.5f);
    for (auto &s : action.spectral_envelope) {
      s = std::clamp(spec_dist(rng_), -1.0f, 1.0f);
    }

    // Random duration
    std::normal_distribution<float> dur_dist(
        config_.development.babbling_duration_mean_ms, 30.0f);
    action.duration_ms = std::clamp(dur_dist(rng_), 20.0f, 300.0f);
  }

  void generateProsodyAction(VocalAction &action) {
    action.is_babbling = false;
    action.exploration_level = 0.4f;

    // Prosody: focus on pitch contour, not phonemes
    // Use feedback-adjusted pitch
    float base_pitch = motor_mean_.empty() ? 220.0f : 220.0f;
    std::normal_distribution<float> pitch_dist(base_pitch, 50.0f);
    action.pitch = std::clamp(pitch_dist(rng_), 80.0f, 500.0f);

    // More stable amplitude
    action.amplitude = 0.5f + 0.1f * std::sin(action_count_ * 0.1f);

    // Spectral follows learned patterns
    for (std::size_t i = 0; i < action.spectral_envelope.size(); ++i) {
      std::normal_distribution<float> d(motor_mean_[i], 0.3f);
      action.spectral_envelope[i] = std::clamp(d(rng_), -1.0f, 1.0f);
    }
  }

  void generateSyllabicAction(VocalAction &action) {
    action.is_babbling = false;
    action.exploration_level = 0.2f;

    // Syllabic: rhythmic patterns
    float phase = std::fmod(action_count_ * 0.2f, 6.28f);
    action.pitch = 180.0f + 40.0f * std::sin(phase);
    action.amplitude = 0.4f + 0.2f * std::abs(std::sin(phase * 2.0f));

    for (std::size_t i = 0; i < action.spectral_envelope.size(); ++i) {
      action.spectral_envelope[i] = motor_mean_[i];
    }
  }

  void generateControlledAction(VocalAction &action) {
    action.is_babbling = false;
    action.exploration_level = 0.1f;

    // Use learned motor model
    action.pitch = 200.0f;
    action.amplitude = 0.5f;

    for (std::size_t i = 0; i < action.spectral_envelope.size(); ++i) {
      action.spectral_envelope[i] = motor_mean_[i];
    }

    // Apply linguistic bias if in LANGUAGE_BIASED stage
    if (stage_ == VocalStage::LANGUAGE_BIASED && !linguistic_intent_.empty()) {
      float bias = config_.language_bias_weight;
      for (std::size_t i = 0; i < std::min(action.spectral_envelope.size(),
                                           linguistic_intent_.size());
           ++i) {
        action.spectral_envelope[i] += bias * linguistic_intent_[i];
      }
    }
  }

  void applyMomentum(VocalAction &action) {
    float m = config_.momentum;
    action.pitch = m * last_action_.pitch + (1.0f - m) * action.pitch;
    action.amplitude =
        m * last_action_.amplitude + (1.0f - m) * action.amplitude;
    last_action_ = action;
  }

  float computePredictionError(const VocalAction &action,
                               const VocalFeedback &feedback) {
    float pitch_error =
        std::abs(action.pitch - feedback.observed_pitch) / 400.0f;
    float amp_error = std::abs(action.amplitude - feedback.observed_amplitude);

    float spectral_error = 0.0f;
    std::size_t n = std::min(action.spectral_envelope.size(),
                             feedback.observed_spectrum.size());
    for (std::size_t i = 0; i < n; ++i) {
      spectral_error +=
          std::abs(action.spectral_envelope[i] - feedback.observed_spectrum[i]);
    }
    spectral_error /= std::max(n, std::size_t(1));

    return (pitch_error + amp_error + spectral_error) / 3.0f;
  }

  void updateMotorModel(const VocalFeedback &feedback, float error) {
    float lr = config_.development.feedback_learning_rate *
               (1.0f - error); // Learn more from good feedback

    // Update spectral mean towards observed
    for (std::size_t i = 0;
         i < std::min(motor_mean_.size(), feedback.observed_spectrum.size());
         ++i) {
      motor_mean_[i] += lr * (feedback.observed_spectrum[i] - motor_mean_[i]);
    }
  }

  void checkStageTransition() {
    float avg_error = getAveragePredictionError();

    switch (stage_) {
    case VocalStage::BABBLING:
      if (avg_error < config_.development.prosody_transition_error) {
        stage_ = VocalStage::PROSODY;
      }
      break;
    case VocalStage::PROSODY:
      if (avg_error < config_.development.syllabic_transition_error) {
        stage_ = VocalStage::SYLLABIC;
      }
      break;
    case VocalStage::SYLLABIC:
      if (avg_error < config_.development.proto_word_transition_error) {
        stage_ = VocalStage::PROTO_WORD;
      }
      break;
    case VocalStage::PROTO_WORD:
      // LANGUAGE_BIASED stage is triggered externally
      break;
    case VocalStage::LANGUAGE_BIASED:
      // Final stage
      break;
    }
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
