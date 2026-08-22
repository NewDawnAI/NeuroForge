#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace NeuroForge {
namespace Sandbox {

class LinearLatentDecoder {
public:
  LinearLatentDecoder(std::size_t latent_dim, std::size_t out_dim)
      : latent_dim_(latent_dim), out_dim_(out_dim), in_dim_(latent_dim + 1),
        W_(out_dim * in_dim_, 0.0f) {}

  std::vector<float> predict(const std::vector<float> &latent) const {
    std::vector<float> y(out_dim_, 0.0f);
    if (latent.empty()) {
      return y;
    }
    for (std::size_t o = 0; o < out_dim_; ++o) {
      float s = W_[o * in_dim_ + (in_dim_ - 1)];
      const std::size_t ld = std::min(latent.size(), latent_dim_);
      const std::size_t row = o * in_dim_;
      for (std::size_t i = 0; i < ld; ++i) {
        s += W_[row + i] * latent[i];
      }
      y[o] = s;
    }
    return y;
  }

  float update(const std::vector<float> &latent, const std::vector<float> &target,
               float learning_rate) {
    if (learning_rate <= 0.0f || latent.empty() || target.empty()) {
      return 0.0f;
    }
    std::vector<float> pred = predict(latent);
    float delta_sq_sum = 0.0f;
    const std::size_t ld = std::min(latent.size(), latent_dim_);
    const std::size_t od = std::min(target.size(), out_dim_);
    for (std::size_t o = 0; o < od; ++o) {
      float err = target[o] - pred[o];
      const std::size_t row = o * in_dim_;
      for (std::size_t i = 0; i < ld; ++i) {
        float delta = learning_rate * err * latent[i];
        W_[row + i] += delta;
        delta_sq_sum += delta * delta;
      }
      float delta_b = learning_rate * err;
      W_[row + (in_dim_ - 1)] += delta_b;
      delta_sq_sum += delta_b * delta_b;
    }
    return std::sqrt(delta_sq_sum);
  }

private:
  std::size_t latent_dim_{0};
  std::size_t out_dim_{0};
  std::size_t in_dim_{0};
  std::vector<float> W_;
};

} // namespace Sandbox
} // namespace NeuroForge

