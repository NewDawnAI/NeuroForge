#include "core/CompositionMetrics.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>
#include <random>
#include "core/DeterministicRng.h"

namespace NeuroForge {
namespace Core {

CompositionMetrics::CompositionMetrics(const CompositionConfig &cfg)
    : config_(cfg) {}

CompositionScore
CompositionMetrics::evaluate(const std::vector<float> &whole,
                             const std::vector<float> &part_a,
                             const std::vector<float> &part_b) const {

  CompositionScore result;

  if (whole.empty() || part_a.empty() || part_b.empty()) {
    return result; // Defaults to non-compositional
  }

  // ──────────────────────────────────────────────────────────
  // Reconstruction: reconstruct(a, b) = (a + b) / ||a + b||
  // This is a simple linear composition; a learned decoder
  // would replace this in a production system.
  // ──────────────────────────────────────────────────────────
  std::size_t dim = whole.size();
  std::vector<float> reconstructed(dim, 0.0f);

  // Pad shorter vectors with zeros
  std::size_t dim_a = part_a.size();
  std::size_t dim_b = part_b.size();

  for (std::size_t i = 0; i < dim; ++i) {
    float a_val = (i < dim_a) ? part_a[i] : 0.0f;
    float b_val = (i < dim_b) ? part_b[i] : 0.0f;
    reconstructed[i] = a_val + b_val;
  }

  // Normalize reconstructed vector
  float recon_norm = 0.0f;
  for (float v : reconstructed)
    recon_norm += v * v;
  recon_norm = std::sqrt(recon_norm);
  if (recon_norm > 1e-8f) {
    for (float &v : reconstructed)
      v /= recon_norm;
  }

  // Normalize whole vector for comparison
  std::vector<float> whole_normed(dim);
  float whole_norm = 0.0f;
  for (float v : whole)
    whole_norm += v * v;
  whole_norm = std::sqrt(whole_norm);
  if (whole_norm > 1e-8f) {
    for (std::size_t i = 0; i < dim; ++i)
      whole_normed[i] = whole[i] / whole_norm;
  } else {
    whole_normed = whole;
  }

  // ──────────────────────────────────────────────────────────
  // Reconstruction error: ||whole_norm − reconstructed||²
  // This proxies K(whole | part_a, part_b)
  // ──────────────────────────────────────────────────────────
  float recon_error = 0.0f;
  for (std::size_t i = 0; i < dim; ++i) {
    float diff = whole_normed[i] - reconstructed[i];
    recon_error += diff * diff;
  }
  result.reconstruction_error = recon_error;

  // ──────────────────────────────────────────────────────────
  // Conditional complexity: sigmoid-mapped reconstruction error
  // Maps error to [0, 1] where 0 = perfectly compositional
  // ──────────────────────────────────────────────────────────
  result.conditional_complexity = std::tanh(recon_error * 2.0f);

  // ──────────────────────────────────────────────────────────
  // Assembly cost:  (||a|| + ||b||) / ||whole||
  // If > 1: parts are "bigger" than whole → compression achieved
  // If < 1: whole is bigger → expansion, not compression
  // ──────────────────────────────────────────────────────────
  float norm_a = 0.0f, norm_b = 0.0f;
  for (float v : part_a)
    norm_a += v * v;
  for (float v : part_b)
    norm_b += v * v;
  norm_a = std::sqrt(norm_a);
  norm_b = std::sqrt(norm_b);
  float parts_total = norm_a + norm_b;
  result.assembly_cost = (whole_norm > 1e-8f) ? parts_total / whole_norm : 0.0f;

  // ──────────────────────────────────────────────────────────
  // Compression ratio: dim_whole / (dim_a + dim_b)
  // ──────────────────────────────────────────────────────────
  result.compression_ratio =
      static_cast<float>(dim) / static_cast<float>(dim_a + dim_b);

  // ──────────────────────────────────────────────────────────
  // Decision: is this merger compositional?
  // ──────────────────────────────────────────────────────────
  result.is_compositional =
      result.conditional_complexity < config_.composition_threshold;

  return result;
}

void CompositionMetrics::recordMerger(uint64_t entity_a, uint64_t entity_b,
                                      const CompositionScore &score) {
  // Update running EMA
  float prev = running_score_.load(std::memory_order_relaxed);
  float updated = config_.ema_alpha * score.conditional_complexity +
                  (1.0f - config_.ema_alpha) * prev;
  running_score_.store(updated, std::memory_order_relaxed);

  total_mergers_.fetch_add(1, std::memory_order_relaxed);
  if (score.is_compositional) {
    compositional_mergers_.fetch_add(1, std::memory_order_relaxed);
  }

  // Record to history
  auto now = std::chrono::steady_clock::now();
  uint64_t ts = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch())
          .count());

  std::lock_guard<std::mutex> lock(history_mutex_);
  history_.push_back({entity_a, entity_b, score, ts});
  while (history_.size() > config_.history_capacity) {
    history_.pop_front();
  }
}

std::vector<float>
CompositionMetrics::perturbForCreativity(const std::vector<float> &repr,
                                         float sigma) const {

  float noise_sigma = (sigma > 0.0f) ? sigma : config_.noise_sigma;

  // Thread-local RNG for efficiency
  thread_local std::mt19937 rng(NeuroForge::Core::DeterministicRng::seedFor("CompositionMetrics"));
  std::normal_distribution<float> dist(0.0f, noise_sigma);

  std::vector<float> perturbed(repr.size());
  for (std::size_t i = 0; i < repr.size(); ++i) {
    perturbed[i] = repr[i] + dist(rng);
  }
  return perturbed;
}

float CompositionMetrics::getRunningCompositionScore() const noexcept {
  return running_score_.load(std::memory_order_relaxed);
}

uint64_t CompositionMetrics::getTotalMergers() const noexcept {
  return total_mergers_.load(std::memory_order_relaxed);
}

uint64_t CompositionMetrics::getCompositionalMergers() const noexcept {
  return compositional_mergers_.load(std::memory_order_relaxed);
}

std::deque<MergerRecord> CompositionMetrics::getHistory() const {
  std::lock_guard<std::mutex> lock(history_mutex_);
  return history_;
}

void CompositionMetrics::setConfig(const CompositionConfig &cfg) {
  std::lock_guard<std::mutex> lock(history_mutex_);
  config_ = cfg;
}

void CompositionMetrics::reset() {
  running_score_.store(1.0f, std::memory_order_relaxed);
  total_mergers_.store(0, std::memory_order_relaxed);
  compositional_mergers_.store(0, std::memory_order_relaxed);
  std::lock_guard<std::mutex> lock(history_mutex_);
  history_.clear();
}

} // namespace Core
} // namespace NeuroForge
