#pragma once

/**
 * @file VocalAction.h
 * @brief Raw Acoustic Motor Action Schema
 *
 * Voice is ACTION, not language. This schema represents raw acoustic
 * motor commands—not phonemes, not text, not TTS.
 *
 * Design principle:
 *   "Language describes the world. Voice acts on the world."
 *
 * Developmental ordering:
 *   1. Babbling (random motor exploration)
 *   2. Prosody (pitch/timing patterns)
 *   3. Syllables (motor sequences)
 *   4. Words (stable attractors in sound space)
 */

#include <cstdint>
#include <vector>

namespace NeuroForge {
namespace Motor {

/**
 * @brief Raw acoustic motor command
 *
 * This is NOT a phoneme or word—it's a continuous control signal
 * for vocal tract parameters, analogous to muscle actuation.
 */
struct VocalAction {
  std::uint64_t timestamp_ms = 0;

  // Vocal tract control parameters (continuous motor output)
  float pitch = 220.0f;     ///< Fundamental frequency (Hz) [50-800]
  float amplitude = 0.5f;   ///< Volume/loudness [0-1]
  float breathiness = 0.2f; ///< Breath noise component [0-1]
  float tension = 0.5f;     ///< Vocal fold tension [0-1]

  // Spectral shaping (formant-like)
  std::vector<float> spectral_envelope; ///< MFCC-like control (13-dim typical)

  // Temporal control
  float duration_ms = 50.0f; ///< Duration of this action
  float attack_ms = 5.0f;    ///< Attack time
  float release_ms = 5.0f;   ///< Release time

  // Motor intent (not linguistic!)
  float exploration_level = 0.5f; ///< How much variation to inject [0-1]
  bool is_babbling = true;        ///< True during early development
};

/**
 * @brief Result of vocal action execution (auditory feedback)
 *
 * This comes from AuditoryCortex perceiving self-generated sound.
 */
struct VocalFeedback {
  std::uint64_t timestamp_ms = 0;

  // What was actually produced (from auditory perception)
  float observed_pitch = 0.0f;
  float observed_amplitude = 0.0f;
  std::vector<float> observed_spectrum;

  // Self-generated detection
  bool is_self_generated = false; ///< Distinguishes own voice from external
  float self_match_confidence = 0.0f;

  // Motor-sensory prediction error
  float prediction_error = 0.0f; ///< Action → Sound prediction error
};

/**
 * @brief Developmental stage for vocal motor learning
 */
enum class VocalStage {
  BABBLING,       ///< Random/exploratory motor patterns
  PROSODY,        ///< Pitch contour learning (before phonemes)
  SYLLABIC,       ///< Motor sequence patterns
  PROTO_WORD,     ///< Stable sound attractors
  LANGUAGE_BIASED ///< Language modulates motor intent (late stage)
};

/**
 * @brief Configuration for vocal motor development
 */
struct VocalDevelopmentConfig {
  VocalStage initial_stage = VocalStage::BABBLING;

  // Babbling parameters
  float babbling_exploration = 0.8f;
  float babbling_pitch_range = 200.0f; ///< Hz variance
  float babbling_duration_mean_ms = 100.0f;
  float babbling_burst_probability = 0.1f;

  // Learning rates
  float motor_learning_rate = 0.01f;
  float feedback_learning_rate = 0.05f;

  // Stage transition thresholds
  float prosody_transition_error = 0.3f; ///< Move to PROSODY when error < this
  float syllabic_transition_error = 0.2f;
  float proto_word_transition_error = 0.1f;
};

} // namespace Motor
} // namespace NeuroForge
