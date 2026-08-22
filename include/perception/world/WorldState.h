#pragma once

/**
 * @file WorldState.h
 * @brief JEPA-Style Latent World State Representation
 *
 * Unified latent representation fusing multi-modal perceptual biases.
 * This is the core data structure for predictive world modeling.
 *
 * @invariant WorldState is a passive observation, not a controller
 * @invariant Latent vectors are normalized to prevent runaway
 */

#include <cstdint>
#include <cmath>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Perception {

/**
 * @brief Multi-modal source components for world state
 *
 * Language is treated as a PERCEPTUAL modality (like sound with structure),
 * NOT as a reasoning substrate. This enables infant-like co-binding:
 *   [See object] + [Hear word] → same latent region → concept grounds
 */
struct WorldStateComponents {
  std::vector<float> visual;     ///< From VisualCortex + ContrastEdgeBias
  std::vector<float> motion;     ///< From MotionBias (flow vectors)
  std::vector<float> temporal;   ///< From TemporalBias (rhythms, sequences)
  std::vector<float> social;     ///< From SocialPerceptionBias (gaze, faces)
  std::vector<float> spatial;    ///< From SpatialNavigationBias
  std::vector<float> auditory;   ///< From VoiceBias + AuditoryCortex
  std::vector<float> linguistic; ///< From LanguageSystem token embeddings
                                 ///< (weak, slow, noisy — lags perception)
  std::vector<float> semantic; ///< From SemanticProjection (grounded concepts)
                               ///< - Rate-limited, confidence-weighted
                               ///< - No raw tokens, no grammar
                               ///< - Derived from ConceptNode activations
                               ///< This allows language to READ world state
                               ///< without DEFINING it (JEPA-style safety)
};

/**
 * @brief Unified Latent World State
 *
 * Represents the world at a single moment in time as a compressed
 * latent vector, following JEPA principles:
 * - No pixel reconstruction
 * - No labels
 * - Prediction in latent space only
 */
struct WorldState {
  /// Unified latent representation (fused from all modalities)
  std::vector<float> latent;

  /// Epistemic uncertainty of this state
  float uncertainty = 0.0f;

  /// Timestamp when this state was observed
  std::uint64_t timestamp_ms = 0;

  /// Source modality strengths (how much each contributed)
  struct ModalityWeights {
    float visual = 0.0f;
    float motion = 0.0f;
    float temporal = 0.0f;
    float social = 0.0f;
    float spatial = 0.0f;
    float auditory = 0.0f;
    float linguistic = 0.0f; ///< Language weight (intentionally low early)
    float semantic =
        0.0f; ///< Grounded concept weight (from SemanticProjection)
  } modality_weights;

  /// Raw source components (before fusion)
  WorldStateComponents sources;

  /// Is this a prediction or an observation?
  bool is_predicted = false;

  /// If predicted, what was the prediction horizon (ms)?
  std::uint64_t prediction_horizon_ms = 0;

  /// Dimensionality of the latent space
  static constexpr std::size_t DEFAULT_LATENT_DIM = 256;

  /**
   * @brief Create an empty world state
   */
  WorldState() : latent(DEFAULT_LATENT_DIM, 0.0f) {}

  /**
   * @brief Create a world state with specific latent dimension
   */
  explicit WorldState(std::size_t latent_dim) : latent(latent_dim, 0.0f) {}

  /**
   * @brief Compute L2 distance to another state in latent space
   */
  float distanceTo(const WorldState &other) const {
    if (latent.size() != other.latent.size()) {
      return -1.0f; // Invalid comparison
    }
    float sum_sq = 0.0f;
    for (std::size_t i = 0; i < latent.size(); ++i) {
      float diff = latent[i] - other.latent[i];
      sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq);
  }

  /**
   * @brief Normalize the latent vector to unit length
   */
  void normalize() {
    float norm = 0.0f;
    for (float v : latent) {
      norm += v * v;
    }
    norm = std::sqrt(norm);
    if (norm > 1e-8f) {
      for (float &v : latent) {
        v /= norm;
      }
    }
  }
};

/**
 * @brief Prediction result with error metrics
 */
struct WorldPredictionResult {
  WorldState predicted_state;
  WorldState observed_state;
  float prediction_error = 0.0f; ///< L2 distance in latent space
  float surprise_level = 0.0f;   ///< Normalized surprise [0,1]
  std::uint64_t prediction_time_ms = 0;
  std::uint64_t observation_time_ms = 0;
};

} // namespace Perception
} // namespace NeuroForge
