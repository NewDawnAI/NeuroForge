#pragma once

/**
 * @file WorldPredictor.h
 * @brief JEPA-Style Predictive World Model
 *
 * Predicts future WorldState from current state.
 * Core of the JEPA paradigm: learn by prediction in latent space.
 *
 * Integration points:
 * - NoveltyBias: receives prediction errors
 * - IntrinsicMotivationSystem: drives curiosity from surprise
 * - TemporalBias: uses temporal patterns for prediction
 */

#include "WorldEncoder.h"
#include "WorldState.h"
#include <cmath>
#include <deque>
#include <functional>


namespace NeuroForge {
namespace Perception {

/**
 * @brief Configuration for WorldPredictor
 */
struct WorldPredictorConfig {
  std::size_t history_size = 10;             ///< States to remember
  float learning_rate = 0.01f;               ///< Prediction model update rate
  float momentum = 0.9f;                     ///< Momentum for updates
  std::uint64_t prediction_horizon_ms = 100; ///< How far ahead to predict
  float min_prediction_error = 0.001f;       ///< Below this, no learning
  float max_prediction_error = 10.0f;        ///< Clamp for stability
  bool enable_learning = true;               ///< Update prediction model
  std::size_t action_dim = 0;                ///< Optional action conditioning dimension (0 disables)
};

/**
 * @brief Predictive World Model
 *
 * Learns to predict future latent states from past observations.
 * Uses simple linear dynamics + learned transition matrix.
 */
class WorldPredictor {
public:
  using PredictionCallback = std::function<void(const WorldPredictionResult &)>;

  explicit WorldPredictor(
      const WorldPredictorConfig &config = {},
      std::size_t latent_dim = WorldState::DEFAULT_LATENT_DIM)
      : config_(config), latent_dim_(latent_dim) {
    initializeTransitionMatrix();
    initializeActionMatrix();
  }

  /**
   * @brief Predict next WorldState from current
   *
   * @param current Current observed WorldState
   * @return Predicted future WorldState
   */
  WorldState predict(const WorldState &current) {
    WorldState predicted(latent_dim_);
    predicted.is_predicted = true;
    predicted.prediction_horizon_ms = config_.prediction_horizon_ms;
    predicted.timestamp_ms =
        current.timestamp_ms + config_.prediction_horizon_ms;
    predicted.uncertainty =
        current.uncertainty + 0.1f; // Predictions are more uncertain

    // Apply transition matrix: z_{t+1} = W * z_t
    for (std::size_t i = 0; i < latent_dim_; ++i) {
      for (std::size_t j = 0; j < latent_dim_; ++j) {
        predicted.latent[i] +=
            transition_matrix_[i * latent_dim_ + j] * current.latent[j];
      }
    }

    // Add velocity term if we have history
    if (state_history_.size() >= 2) {
      const WorldState &prev = state_history_[state_history_.size() - 2];
      for (std::size_t i = 0; i < latent_dim_; ++i) {
        float velocity = current.latent[i] - prev.latent[i];
        predicted.latent[i] += velocity * 0.5f; // Simple momentum
      }
    }

    predicted.normalize();
    return predicted;
  }

  WorldState predict(const WorldState &current, const std::vector<float> &action) {
    if (config_.action_dim == 0 || action.empty()) {
      return predict(current);
    }

    WorldState predicted = predict(current);
    std::size_t ad = std::min<std::size_t>(config_.action_dim, action.size());
    if (ad == 0 || action_matrix_.empty()) {
      return predicted;
    }
    for (std::size_t i = 0; i < latent_dim_; ++i) {
      float add = 0.0f;
      const std::size_t row = i * config_.action_dim;
      for (std::size_t j = 0; j < ad; ++j) {
        add += action_matrix_[row + j] * action[j];
      }
      predicted.latent[i] += add;
    }
    predicted.normalize();
    return predicted;
  }

  /**
   * @brief Observe actual state and compute prediction error
   *
   * @param predicted Previous prediction
   * @param observed Actual observed state
   * @return Prediction result with error metrics
   */
  WorldPredictionResult observe(const WorldState &predicted,
                                const WorldState &observed) {
    WorldPredictionResult result;
    result.predicted_state = predicted;
    result.observed_state = observed;
    result.prediction_time_ms = predicted.timestamp_ms;
    result.observation_time_ms = observed.timestamp_ms;

    // Compute L2 prediction error
    result.prediction_error = predicted.distanceTo(observed);
    result.prediction_error =
        std::clamp(result.prediction_error, config_.min_prediction_error,
                   config_.max_prediction_error);

    // Compute surprise level (normalized)
    result.surprise_level = std::tanh(result.prediction_error);

    // Update model if learning enabled
    if (config_.enable_learning &&
        result.prediction_error > config_.min_prediction_error) {
      (void)updateTransitionMatrix(predicted, observed, config_.learning_rate);
    }

    // Update history
    state_history_.push_back(observed);
    if (state_history_.size() > config_.history_size) {
      state_history_.pop_front();
    }

    // Fire callback
    if (prediction_callback_) {
      prediction_callback_(result);
    }

    // Track statistics
    total_predictions_++;
    total_error_ += result.prediction_error;

    return result;
  }

  WorldPredictionResult observe(const WorldState &predicted,
                                const WorldState &observed,
                                const std::vector<float> &action) {
    if (config_.action_dim == 0 || action.empty()) {
      return observe(predicted, observed);
    }

    WorldPredictionResult result;
    result.predicted_state = predicted;
    result.observed_state = observed;
    result.prediction_time_ms = predicted.timestamp_ms;
    result.observation_time_ms = observed.timestamp_ms;

    result.prediction_error = predicted.distanceTo(observed);
    result.prediction_error =
        std::clamp(result.prediction_error, config_.min_prediction_error,
                   config_.max_prediction_error);
    result.surprise_level = std::tanh(result.prediction_error);

    if (config_.enable_learning &&
        result.prediction_error > config_.min_prediction_error) {
      (void)updateTransitionAndAction(predicted, observed, action,
                                     config_.learning_rate);
    }

    state_history_.push_back(observed);
    if (state_history_.size() > config_.history_size) {
      state_history_.pop_front();
    }

    if (prediction_callback_) {
      prediction_callback_(result);
    }

    total_predictions_++;
    total_error_ += result.prediction_error;

    return result;
  }

  /**
   * @brief Apply a learning update with an explicit learning rate (externally gated)
   *
   * This does NOT touch history or counters; it only updates internal predictor parameters.
   *
   * @return L2 norm of the parameter delta applied (for audit/instrumentation)
   */
  float updateModel(const WorldState &predicted, const WorldState &observed,
                    float learning_rate) {
    if (learning_rate <= 0.0f) {
      return 0.0f;
    }
    return updateTransitionMatrix(predicted, observed, learning_rate);
  }

  float updateModel(const WorldState &predicted, const WorldState &observed,
                    const std::vector<float> &action, float learning_rate) {
    if (learning_rate <= 0.0f) {
      return 0.0f;
    }
    if (config_.action_dim == 0 || action.empty()) {
      return updateTransitionMatrix(predicted, observed, learning_rate);
    }
    return updateTransitionAndAction(predicted, observed, action, learning_rate);
  }

  /**
   * @brief Set callback for prediction results
   */
  void setPredictionCallback(PredictionCallback callback) {
    prediction_callback_ = callback;
  }

  /**
   * @brief Get average prediction error
   */
  float getAveragePredictionError() const {
    return total_predictions_ > 0 ? total_error_ / total_predictions_ : 0.0f;
  }

  /**
   * @brief Get number of predictions made
   */
  std::uint64_t getTotalPredictions() const { return total_predictions_; }

  /**
   * @brief Get state history
   */
  const std::deque<WorldState> &getStateHistory() const {
    return state_history_;
  }

  const WorldPredictorConfig &getConfig() const { return config_; }

private:
  WorldPredictorConfig config_;
  std::size_t latent_dim_;
  std::vector<float> transition_matrix_;
  std::vector<float> action_matrix_;
  std::deque<WorldState> state_history_;
  PredictionCallback prediction_callback_;

  std::uint64_t total_predictions_ = 0;
  float total_error_ = 0.0f;

  void initializeTransitionMatrix() {
    // Initialize as identity matrix + small noise
    transition_matrix_.resize(latent_dim_ * latent_dim_, 0.0f);
    for (std::size_t i = 0; i < latent_dim_; ++i) {
      transition_matrix_[i * latent_dim_ + i] = 0.95f; // Slight decay
    }
  }

  void initializeActionMatrix() {
    if (config_.action_dim == 0) {
      return;
    }
    action_matrix_.assign(latent_dim_ * config_.action_dim, 0.0f);
  }

  float updateTransitionMatrix(const WorldState &predicted,
                               const WorldState &observed,
                               float learning_rate) {
    float delta_sq_sum = 0.0f;
    // Simple gradient descent update
    // Error = observed - predicted
    // Gradient: dW = learning_rate * error * input^T
    for (std::size_t i = 0; i < latent_dim_; ++i) {
      float error_i = observed.latent[i] - predicted.latent[i];
      for (std::size_t j = 0; j < latent_dim_; ++j) {
        float gradient = error_i * observed.latent[j];
        float delta = learning_rate * gradient;
        transition_matrix_[i * latent_dim_ + j] += delta;
        delta_sq_sum += delta * delta;
      }
    }

    // Regularization: push towards identity
    for (std::size_t i = 0; i < latent_dim_; ++i) {
      for (std::size_t j = 0; j < latent_dim_; ++j) {
        float target = (i == j) ? 0.95f : 0.0f;
        transition_matrix_[i * latent_dim_ + j] =
            config_.momentum * transition_matrix_[i * latent_dim_ + j] +
            (1.0f - config_.momentum) * target;
      }
    }
    return std::sqrt(delta_sq_sum);
  }

  float updateTransitionAndAction(const WorldState &predicted,
                                  const WorldState &observed,
                                  const std::vector<float> &action,
                                  float learning_rate) {
    float delta_sq_sum = 0.0f;

    std::size_t ad = std::min<std::size_t>(config_.action_dim, action.size());
    for (std::size_t i = 0; i < latent_dim_; ++i) {
      float error_i = observed.latent[i] - predicted.latent[i];

      for (std::size_t j = 0; j < latent_dim_; ++j) {
        float gradient = error_i * observed.latent[j];
        float delta = learning_rate * gradient;
        transition_matrix_[i * latent_dim_ + j] += delta;
        delta_sq_sum += delta * delta;
      }

      const std::size_t row = i * config_.action_dim;
      for (std::size_t j = 0; j < ad; ++j) {
        float gradient_a = error_i * action[j];
        float delta_a = learning_rate * gradient_a;
        action_matrix_[row + j] += delta_a;
        delta_sq_sum += delta_a * delta_a;
      }
    }

    for (std::size_t i = 0; i < latent_dim_; ++i) {
      for (std::size_t j = 0; j < latent_dim_; ++j) {
        float target = (i == j) ? 0.95f : 0.0f;
        transition_matrix_[i * latent_dim_ + j] =
            config_.momentum * transition_matrix_[i * latent_dim_ + j] +
            (1.0f - config_.momentum) * target;
      }
    }

    for (float &w : action_matrix_) {
      w *= config_.momentum;
    }

    return std::sqrt(delta_sq_sum);
  }
};

} // namespace Perception
} // namespace NeuroForge
