#pragma once

/**
 * @file WorldEncoder.h
 * @brief JEPA-Style Multi-Modal Fusion Encoder
 *
 * Fuses outputs from perceptual biases into unified latent WorldState.
 * No gradient learning — uses weighted linear fusion.
 *
 * Integration points:
 * - ContrastEdgeBias::EdgeResponse → visual component
 * - MotionBias::MotionField → motion component
 * - TemporalBias::TemporalContext → temporal component
 * - SocialPerceptionBias::SocialEvent → social component
 * - LanguageSystem::SymbolicToken → linguistic component (weak, lags
 * perception)
 */

#include "WorldState.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <random>

namespace NeuroForge {
namespace Perception {

/**
 * @brief Configuration for WorldEncoder
 */
struct WorldEncoderConfig {
  std::size_t latent_dim = 256;          ///< Output latent dimension
  std::size_t visual_input_dim = 64;     ///< Expected visual feature size
  std::size_t motion_input_dim = 32;     ///< Expected motion feature size
  std::size_t temporal_input_dim = 32;   ///< Expected temporal feature size
  std::size_t social_input_dim = 16;     ///< Expected social feature size
  std::size_t spatial_input_dim = 32;    ///< Expected spatial feature size
  std::size_t auditory_input_dim = 32;   ///< Expected auditory feature size
  std::size_t linguistic_input_dim = 32; ///< Expected linguistic embedding size
  std::size_t semantic_input_dim = 128;  ///< Expected semantic projection size
  std::uint32_t random_seed = 1337;

  // Modality weights (intentionally sum to ~1.0)
  // Language weight is LOW by design: it lags perception in development
  float visual_weight = 0.20f;
  float motion_weight = 0.15f;
  float temporal_weight = 0.12f;
  float social_weight = 0.12f;
  float spatial_weight = 0.10f;
  float auditory_weight = 0.12f;
  float linguistic_weight =
      0.05f; ///< Intentionally weak — language lags perception
  float semantic_weight =
      0.14f; ///< Grounded semantic concepts (stronger than raw linguistic)

  bool normalize_output = true;
  bool track_modality_contributions = true;
};

/**
 * @brief Multi-Modal Fusion Encoder
 *
 * Encodes multi-modal perceptual inputs into a unified latent space.
 * Uses simple linear projections (no deep learning required).
 */
class WorldEncoder {
public:
  explicit WorldEncoder(const WorldEncoderConfig &config = {})
      : config_(config) {
    initializeProjections();
  }

  /**
   * @brief Encode multi-modal inputs into WorldState
   *
   * @param visual Visual features (from VisualCortex/ContrastEdgeBias)
   * @param motion Motion features (from MotionBias)
   * @param temporal Temporal features (from TemporalBias)
   * @param social Social features (from SocialPerceptionBias)
   * @param spatial Spatial features (from SpatialNavigationBias)
   * @param auditory Auditory features (from AuditoryCortex/VoiceBias)
   * @param linguistic Language embeddings (from LanguageSystem tokens)
   * @return Unified WorldState
   */
  WorldState
  encode(const std::vector<float> &visual, const std::vector<float> &motion,
         const std::vector<float> &temporal, const std::vector<float> &social,
         const std::vector<float> &spatial, const std::vector<float> &auditory,
         const std::vector<float> &linguistic = {},
         const std::vector<float> &semantic = {}) {
    WorldState state(config_.latent_dim);
    state.timestamp_ms = getCurrentTimeMs();

    // Store raw sources (all 8 modalities)
    state.sources.visual = visual;
    state.sources.motion = motion;
    state.sources.temporal = temporal;
    state.sources.social = social;
    state.sources.spatial = spatial;
    state.sources.auditory = auditory;
    state.sources.linguistic = linguistic;
    state.sources.semantic = semantic;

    // Project each modality to latent space
    std::vector<float> visual_proj = project(visual, visual_projection_);
    std::vector<float> motion_proj = project(motion, motion_projection_);
    std::vector<float> temporal_proj = project(temporal, temporal_projection_);
    std::vector<float> social_proj = project(social, social_projection_);
    std::vector<float> spatial_proj = project(spatial, spatial_projection_);
    std::vector<float> auditory_proj = project(auditory, auditory_projection_);
    std::vector<float> linguistic_proj =
        project(linguistic, linguistic_projection_);
    std::vector<float> semantic_proj = project(semantic, semantic_projection_);

    // Weighted fusion (8 modalities)
    // Language weight is intentionally LOW — infant-like developmental ordering
    for (std::size_t i = 0; i < config_.latent_dim; ++i) {
      state.latent[i] =
          config_.visual_weight * safeGet(visual_proj, i) +
          config_.motion_weight * safeGet(motion_proj, i) +
          config_.temporal_weight * safeGet(temporal_proj, i) +
          config_.social_weight * safeGet(social_proj, i) +
          config_.spatial_weight * safeGet(spatial_proj, i) +
          config_.auditory_weight * safeGet(auditory_proj, i) +
          config_.linguistic_weight * safeGet(linguistic_proj, i) +
          config_.semantic_weight * safeGet(semantic_proj, i);
    }

    // Track modality contributions
    if (config_.track_modality_contributions) {
      state.modality_weights.visual = computeContribution(visual);
      state.modality_weights.motion = computeContribution(motion);
      state.modality_weights.temporal = computeContribution(temporal);
      state.modality_weights.social = computeContribution(social);
      state.modality_weights.spatial = computeContribution(spatial);
      state.modality_weights.auditory = computeContribution(auditory);
      state.modality_weights.linguistic = computeContribution(linguistic);
      state.modality_weights.semantic = computeContribution(semantic);
    }

    // Compute uncertainty (higher when fewer modalities active)
    float active_modalities = 0.0f;
    if (!visual.empty())
      active_modalities += 1.0f;
    if (!motion.empty())
      active_modalities += 1.0f;
    if (!temporal.empty())
      active_modalities += 1.0f;
    if (!social.empty())
      active_modalities += 1.0f;
    if (!spatial.empty())
      active_modalities += 1.0f;
    if (!auditory.empty())
      active_modalities += 1.0f;
    if (!linguistic.empty())
      active_modalities += 1.0f;
    if (!semantic.empty())
      active_modalities += 1.0f;

    state.uncertainty = 1.0f - (active_modalities / 8.0f);

    // Normalize if configured
    if (config_.normalize_output) {
      state.normalize();
    }

    return state;
  }

  /**
   * @brief Encode from WorldStateComponents directly
   */
  WorldState encode(const WorldStateComponents &components) {
    return encode(components.visual, components.motion, components.temporal,
                  components.social, components.spatial, components.auditory,
                  components.linguistic, components.semantic);
  }

  const WorldEncoderConfig &getConfig() const { return config_; }

private:
  WorldEncoderConfig config_;

  // Linear projection matrices (stored as flat vectors for simplicity)
  std::vector<float> visual_projection_;
  std::vector<float> motion_projection_;
  std::vector<float> temporal_projection_;
  std::vector<float> social_projection_;
  std::vector<float> spatial_projection_;
  std::vector<float> auditory_projection_;
  std::vector<float> linguistic_projection_;
  std::vector<float> semantic_projection_;

  void initializeProjections() {
    // Initialize with small random values
    visual_projection_ = initProjection(config_.visual_input_dim);
    motion_projection_ = initProjection(config_.motion_input_dim);
    temporal_projection_ = initProjection(config_.temporal_input_dim);
    social_projection_ = initProjection(config_.social_input_dim);
    spatial_projection_ = initProjection(config_.spatial_input_dim);
    auditory_projection_ = initProjection(config_.auditory_input_dim);
    linguistic_projection_ = initProjection(config_.linguistic_input_dim);
    semantic_projection_ = initProjection(config_.semantic_input_dim);
  }

  std::vector<float> initProjection(std::size_t input_dim) {
    std::vector<float> proj(input_dim * config_.latent_dim);
    float scale =
        2.0f / std::sqrt(static_cast<float>(input_dim + config_.latent_dim));
    std::mt19937 rng(config_.random_seed ^
                     static_cast<std::uint32_t>(input_dim * 2654435761u));
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
    for (auto &v : proj) {
      v = scale * dist(rng);
    }
    return proj;
  }

  std::vector<float> project(const std::vector<float> &input,
                             const std::vector<float> &projection) {
    std::vector<float> output(config_.latent_dim, 0.0f);
    if (input.empty())
      return output;

    std::size_t input_dim = projection.size() / config_.latent_dim;
    for (std::size_t o = 0; o < config_.latent_dim; ++o) {
      for (std::size_t i = 0; i < std::min(input.size(), input_dim); ++i) {
        output[o] += input[i] * projection[o * input_dim + i];
      }
    }
    return output;
  }

  float safeGet(const std::vector<float> &vec, std::size_t idx) {
    return idx < vec.size() ? vec[idx] : 0.0f;
  }

  float computeContribution(const std::vector<float> &vec) {
    if (vec.empty())
      return 0.0f;
    float sum = 0.0f;
    for (float v : vec)
      sum += std::abs(v);
    return sum / vec.size();
  }

  std::uint64_t getCurrentTimeMs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count());
  }
};

} // namespace Perception
} // namespace NeuroForge
