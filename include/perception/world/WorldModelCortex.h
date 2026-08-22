#pragma once

/**
 * @file WorldModelCortex.h
 * @brief JEPA-Style World Model Cortex
 *
 * Orchestrates predictive world modeling:
 * 1. Receives multi-modal inputs from biases
 * 2. Encodes to unified latent WorldState
 * 3. Predicts future states
 * 4. Computes prediction errors for curiosity
 *
 * This is the main integration point for JEPA-style learning.
 */

#include "WorldEncoder.h"
#include "WorldPredictor.h"
#include "WorldState.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <memory>

namespace NeuroForge {
namespace Perception {

/**
 * @brief Configuration for WorldModelCortex
 */
struct WorldModelCortexConfig {
  WorldEncoderConfig encoder_config;
  WorldPredictorConfig predictor_config;

  bool enable_prediction = true;
  bool log_predictions = false;
  bool enable_cognitive_regulation = false;
  std::uint64_t min_cycle_interval_ms = 50; ///< Minimum time between updates
};

/**
 * @brief World Model Cortex - JEPA-Style Predictive World Model
 *
 * Integrates with existing NeuroForge components:
 * - Receives inputs from perceptual biases
 * - Computes prediction errors
 * - Feeds IntrinsicMotivationSystem for curiosity
 * - Updates ConceptNode.predictive_power
 */
class WorldModelCortex {
public:
  using SurpriseCallback =
      std::function<void(float surprise_level, float prediction_error)>;
  using StateCallback = std::function<void(const WorldState &)>;

  struct AttentionState {
    float semantic_weight = 1.0f;
    float visual_weight = 1.0f;
    float motion_weight = 1.0f;
    float temporal_weight = 1.0f;
    float social_weight = 1.0f;
    float spatial_weight = 1.0f;
    float auditory_weight = 1.0f;
    float linguistic_weight = 1.0f;
  };

  struct RegulationState {
    float attention_alpha = 0.2f;
    float attention_lambda = 0.2f;
    float learning_rate_multiplier = 1.0f;
  };

  explicit WorldModelCortex(const WorldModelCortexConfig &config = {})
      : config_(config),
        encoder_(std::make_unique<WorldEncoder>(config.encoder_config)),
        predictor_(std::make_unique<WorldPredictor>(config.predictor_config)) {

    // Wire prediction callback to our internal handler
    predictor_->setPredictionCallback(
        [this](const WorldPredictionResult &result) {
          handlePredictionResult(result);
        });
  }

  /**
   * @brief Process one cycle of world modeling
   *
   * @param visual Visual features from VisualCortex/ContrastEdgeBias
   * @param motion Motion features from MotionBias
   * @param temporal Temporal features from TemporalBias
   * @param social Social features from SocialPerceptionBias
   * @param spatial Spatial features from SpatialNavigationBias
   * @param auditory Auditory features from VoiceBias/AuditoryCortex
   * @param linguistic Language embeddings from LanguageSystem (weak, lags
   * perception)
   * @return Current WorldState
   */
  WorldState processCycle(const std::vector<float> &visual,
                          const std::vector<float> &motion,
                          const std::vector<float> &temporal,
                          const std::vector<float> &social,
                          const std::vector<float> &spatial,
                          const std::vector<float> &auditory,
                          const std::vector<float> &linguistic = {},
                          const std::vector<float> &semantic = {}) {
    WorldState observed =
        observeCycle(visual, motion, temporal, social, spatial, auditory,
                     linguistic, semantic);

    if (config_.enable_prediction) {
      pending_prediction_ = predictor_->predict(observed);
      pending_action_.clear();
      has_pending_prediction_ = true;
    }

    return current_state_;
  }

  WorldState observeCycle(const std::vector<float> &visual,
                          const std::vector<float> &motion,
                          const std::vector<float> &temporal,
                          const std::vector<float> &social,
                          const std::vector<float> &spatial,
                          const std::vector<float> &auditory,
                          const std::vector<float> &linguistic = {},
                          const std::vector<float> &semantic = {}) {
    // Rate limiting
    auto now = getCurrentTimeMs();
    if (now - last_cycle_time_ms_ < config_.min_cycle_interval_ms) {
      return current_state_;
    }
    last_cycle_time_ms_ = now;

    auto scale_vec = [](const std::vector<float> &in, float gain) {
      if (in.empty() || gain == 1.0f) {
        return in;
      }
      std::vector<float> out = in;
      for (float &v : out) {
        v *= gain;
      }
      return out;
    };

    std::vector<float> visual_in = scale_vec(visual, attention_.visual_weight);
    std::vector<float> motion_in = scale_vec(motion, attention_.motion_weight);
    std::vector<float> temporal_in =
        scale_vec(temporal, attention_.temporal_weight);
    std::vector<float> social_in = scale_vec(social, attention_.social_weight);
    std::vector<float> spatial_in =
        scale_vec(spatial, attention_.spatial_weight);
    std::vector<float> auditory_in =
        scale_vec(auditory, attention_.auditory_weight);
    std::vector<float> linguistic_in =
        scale_vec(linguistic, attention_.linguistic_weight);
    std::vector<float> semantic_in =
        scale_vec(semantic, attention_.semantic_weight);

    // Encode current observation (7 modalities)
    WorldState observed =
        encoder_->encode(visual_in, motion_in, temporal_in, social_in,
                         spatial_in, auditory_in, linguistic_in, semantic_in);

    // If we have a previous prediction, compute error
    if (config_.enable_prediction && has_pending_prediction_) {
      if (!pending_action_.empty()) {
        last_observed_prediction_ = pending_prediction_;
        last_observed_action_ = pending_action_;
        has_last_observed_ = true;
        predictor_->observe(pending_prediction_, observed, pending_action_);
      } else {
        last_observed_prediction_ = pending_prediction_;
        last_observed_action_.clear();
        has_last_observed_ = true;
        predictor_->observe(pending_prediction_, observed);
      }
      has_pending_prediction_ = false;
      pending_action_.clear();
    }

    // Update current state
    current_state_ = observed;
    cycle_count_++;

    last_latent_entropy_ = computeLatentEntropy(current_state_.latent);
    if (config_.enable_cognitive_regulation) {
      updateRegulation();
    } else {
      regulation_.attention_alpha = 0.2f;
      regulation_.attention_lambda = 0.2f;
      regulation_.learning_rate_multiplier = 1.0f;
    }

    updateAttention(observed.sources);

    // Fire state callback
    if (state_callback_) {
      state_callback_(current_state_);
    }

    return current_state_;
  }

  void predictNext(const std::vector<float> &action) {
    if (!config_.enable_prediction) {
      return;
    }
    pending_prediction_ = predictor_->predict(current_state_, action);
    pending_action_ = action;
    has_pending_prediction_ = true;
  }

  /**
   * @brief Process cycle from WorldStateComponents (uses all 7 modalities)
   */
  WorldState processCycle(const WorldStateComponents &components) {
    return processCycle(components.visual, components.motion, components.temporal,
                        components.social, components.spatial,
                        components.auditory, components.linguistic,
                        components.semantic);
  }

  /**
   * @brief Set callback for surprise events (for NoveltyBias integration)
   */
  void setSurpriseCallback(SurpriseCallback callback) {
    surprise_callback_ = callback;
  }

  /**
   * @brief Set callback for new states
   */
  void setStateCallback(StateCallback callback) { state_callback_ = callback; }

  // Accessors
  const WorldState &getCurrentState() const { return current_state_; }
  const WorldState &getPendingPrediction() const { return pending_prediction_; }
  bool hasPendingPrediction() const { return has_pending_prediction_; }

  float getAveragePredictionError() const {
    return predictor_->getAveragePredictionError();
  }

  float getTotalSurprise() const { return total_surprise_; }
  std::uint64_t getCycleCount() const { return cycle_count_; }

  AttentionState getAttentionState() const { return attention_; }
  RegulationState getRegulationState() const { return regulation_; }
  float getLastPredictionError() const { return last_prediction_error_; }
  float getLastSurpriseLevel() const { return last_surprise_level_; }
  float getLastLatentEntropy() const { return last_latent_entropy_; }
  bool hasLastObserved() const { return has_last_observed_; }
  const WorldState &getLastObservedPrediction() const {
    return last_observed_prediction_;
  }
  const std::vector<float> &getLastObservedAction() const {
    return last_observed_action_;
  }

  WorldEncoder &getEncoder() { return *encoder_; }
  WorldPredictor &getPredictor() { return *predictor_; }

  const WorldModelCortexConfig &getConfig() const { return config_; }

private:
  WorldModelCortexConfig config_;
  std::unique_ptr<WorldEncoder> encoder_;
  std::unique_ptr<WorldPredictor> predictor_;

  WorldState current_state_;
  WorldState pending_prediction_;
  bool has_pending_prediction_ = false;
  std::vector<float> pending_action_;
  WorldState last_observed_prediction_;
  std::vector<float> last_observed_action_;
  bool has_last_observed_ = false;

  SurpriseCallback surprise_callback_;
  StateCallback state_callback_;

  std::uint64_t last_cycle_time_ms_ = 0;
  std::uint64_t cycle_count_ = 0;
  float total_surprise_ = 0.0f;
  float last_prediction_error_ = 0.0f;
  float last_surprise_level_ = 0.0f;
  float last_latent_entropy_ = 0.0f;
  AttentionState attention_;
  RegulationState regulation_;

  void handlePredictionResult(const WorldPredictionResult &result) {
    total_surprise_ += result.surprise_level;
    last_surprise_level_ = result.surprise_level;
    last_prediction_error_ = result.prediction_error;

    // Fire surprise callback for NoveltyBias integration
    if (surprise_callback_) {
      surprise_callback_(result.surprise_level, result.prediction_error);
    }
  }

  void updateAttention(const WorldStateComponents &sources) {
    const float A_min = 0.5f;
    const float A_max = 1.5f;
    const float eps = 1e-8f;

    auto mag2 = [](const std::vector<float> &v) {
      float s = 0.0f;
      for (float x : v) {
        s += x * x;
      }
      return s;
    };

    bool semantic_active = mag2(sources.semantic) > eps;
    bool visual_active = !sources.visual.empty();
    bool auditory_active = !sources.auditory.empty();
    bool temporal_active = !sources.temporal.empty();
    bool motion_active = !sources.motion.empty();
    bool social_active = !sources.social.empty();
    bool spatial_active = !sources.spatial.empty();
    bool linguistic_active = !sources.linguistic.empty();

    float pe_norm = std::clamp(std::tanh(last_prediction_error_), 0.0f, 1.0f);

    auto decay_to_baseline = [&](float &w) {
      w = 1.0f + (w - 1.0f) * std::exp(-regulation_.attention_lambda);
      w = std::clamp(w, A_min, A_max);
    };

    decay_to_baseline(attention_.semantic_weight);
    decay_to_baseline(attention_.visual_weight);
    decay_to_baseline(attention_.auditory_weight);
    decay_to_baseline(attention_.temporal_weight);
    decay_to_baseline(attention_.motion_weight);
    decay_to_baseline(attention_.social_weight);
    decay_to_baseline(attention_.spatial_weight);
    decay_to_baseline(attention_.linguistic_weight);

    if (pe_norm < 0.05f) {
      return;
    }

    float alpha = regulation_.attention_alpha;
    float delta = alpha * pe_norm;

    auto bump = [&](float &w) { w = std::clamp(w + delta, A_min, A_max); };

    if (semantic_active) {
      bump(attention_.semantic_weight);
    } else if (visual_active) {
      bump(attention_.visual_weight);
    } else if (auditory_active) {
      bump(attention_.auditory_weight);
    } else if (temporal_active) {
      bump(attention_.temporal_weight);
    } else if (motion_active) {
      bump(attention_.motion_weight);
    } else if (social_active) {
      bump(attention_.social_weight);
    } else if (spatial_active) {
      bump(attention_.spatial_weight);
    } else if (linguistic_active) {
      bump(attention_.linguistic_weight);
    }
  }

  void updateRegulation() {
    float pe_norm = std::clamp(std::tanh(last_prediction_error_), 0.0f, 1.0f);
    regulation_.attention_alpha =
        std::clamp(0.1f + 0.2f * pe_norm, 0.1f, 0.3f);
    regulation_.attention_lambda =
        std::clamp(0.3f - 0.15f * pe_norm, 0.15f, 0.3f);
    regulation_.learning_rate_multiplier =
        std::clamp(0.5f + pe_norm, 0.5f, 1.5f);
  }

  float computeLatentEntropy(const std::vector<float> &latent) {
    float abs_sum = 0.0f;
    for (float v : latent) {
      abs_sum += std::fabs(v);
    }
    if (abs_sum <= 1e-12f) {
      return 0.0f;
    }
    float entropy = 0.0f;
    for (float v : latent) {
      float p = std::fabs(v) / abs_sum;
      if (p > 1e-12f) {
        entropy -= p * std::log(p);
      }
    }
    return entropy;
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
