#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Sandbox {

class LinearActionModel {
public:
  LinearActionModel(std::size_t obs_dim, std::size_t action_dim)
      : obs_dim_(obs_dim), action_dim_(action_dim),
        in_dim_(obs_dim + action_dim + 1),
        W_(obs_dim * in_dim_, 0.0f) {
    for (std::size_t i = 0; i < obs_dim_ && i < in_dim_; ++i) {
      W_[i * in_dim_ + i] = 1.0f;
    }
  }

  std::size_t obsDim() const { return obs_dim_; }
  std::size_t actionDim() const { return action_dim_; }

  std::vector<float> predict(const std::vector<float> &obs,
                             std::size_t action_index) const {
    std::vector<float> x = makeInput(obs, action_index);
    std::vector<float> y(obs_dim_, 0.0f);
    for (std::size_t i = 0; i < obs_dim_; ++i) {
      float s = 0.0f;
      const std::size_t row = i * in_dim_;
      for (std::size_t j = 0; j < in_dim_; ++j) {
        s += W_[row + j] * x[j];
      }
      y[i] = s;
    }
    return y;
  }

  float computeErrorNorm(const std::vector<float> &pred,
                         const std::vector<float> &target) const {
    float s = 0.0f;
    for (std::size_t i = 0; i < obs_dim_; ++i) {
      float d = target[i] - pred[i];
      s += d * d;
    }
    return std::sqrt(s);
  }

  float update(const std::vector<float> &obs, std::size_t action_index,
               const std::vector<float> &next_obs, float learning_rate) {
    if (learning_rate <= 0.0f) {
      return 0.0f;
    }
    std::vector<float> x = makeInput(obs, action_index);
    std::vector<float> pred = predict(obs, action_index);
    std::vector<float> err(obs_dim_, 0.0f);
    for (std::size_t i = 0; i < obs_dim_; ++i) {
      err[i] = next_obs[i] - pred[i];
    }

    float delta_sq_sum = 0.0f;
    for (std::size_t i = 0; i < obs_dim_; ++i) {
      const std::size_t row = i * in_dim_;
      for (std::size_t j = 0; j < in_dim_; ++j) {
        float delta = learning_rate * err[i] * x[j];
        W_[row + j] += delta;
        delta_sq_sum += delta * delta;
      }
    }
    return std::sqrt(delta_sq_sum);
  }

private:
  std::size_t obs_dim_{0};
  std::size_t action_dim_{0};
  std::size_t in_dim_{0};
  std::vector<float> W_;

  std::vector<float> makeInput(const std::vector<float> &obs,
                               std::size_t action_index) const {
    std::vector<float> x(in_dim_, 0.0f);
    for (std::size_t i = 0; i < obs_dim_ && i < obs.size(); ++i) {
      x[i] = obs[i];
    }
    if (action_index < action_dim_) {
      x[obs_dim_ + action_index] = 1.0f;
    }
    x[in_dim_ - 1] = 1.0f;
    return x;
  }
};

} // namespace Sandbox
} // namespace NeuroForge
