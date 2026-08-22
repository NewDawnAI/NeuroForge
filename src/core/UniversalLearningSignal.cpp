#include "core/UniversalLearningSignal.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace NeuroForge {
namespace Core {

UniversalLearningSignal::UniversalLearningSignal(
    const UniversalSignalConfig &cfg)
    : config_(cfg) {}

UniversalSignal UniversalLearningSignal::compute(float surprise,
                                                 float intrinsic_composite,
                                                 float external_reward,
                                                 float sparsity_ratio,
                                                 float hebbian_pre_post) {
  // ──────────────────────────────────────────────────────────────
  // The Universal Learning Equation:
  //   Δw = η · δ · ∇_w I(gain)
  //
  // Expanded form:
  //   Δw ∝ η · (r_intrinsic + γ · max(I_future)) · (ŷ − y) · ∂f(w)/∂w
  //
  // We compute the *signal* here; the actual weight update happens
  // in Synapse::updateWeight() when PlasticityRule == Universal.
  // ──────────────────────────────────────────────────────────────

  // 1. Prediction error δ (surprise from WorldModelCortex)
  float delta = surprise;

  // 2. EMA-smooth δ for stability (prevents instability from spikes)
  float prev_smooth = smoothed_delta_.load(std::memory_order_relaxed);
  float smoothed =
      config_.delta_ema * prev_smooth + (1.0f - config_.delta_ema) * delta;
  smoothed_delta_.store(smoothed, std::memory_order_relaxed);

  // 3. Information gain I_gain (curiosity from IntrinsicMotivationSystem)
  float info_gain = intrinsic_composite;

  // 4. Blended reward: r_total = r_ext + γ · I_gain
  //    This merges external task reward with intrinsic curiosity
  float reward_total = external_reward + config_.gamma * info_gain;

  // 5. Modulated learning rate: η_eff = η₀ · (1 + α · |δ|)
  //    Higher surprise → faster learning (biological: dopamine surge)
  float eta_raw =
      config_.eta_base * (1.0f + config_.alpha * std::fabs(smoothed));
  float eta_effective = std::clamp(eta_raw, config_.eta_min, config_.eta_max);

  // 6. Sparsity cost: λ · (active_ratio)
  //    Encourages compression — only a small fraction of neurons active
  //    This is the "low-poly memory" pressure from biological encoding
  float sparsity_cost = config_.beta_sparse * sparsity_ratio;

  // 7. Timestamp
  auto now = std::chrono::steady_clock::now();
  uint64_t ts = static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch())
          .count());

  // 8. Build the signal
  UniversalSignal sig;
  sig.delta = smoothed; // Use smoothed δ for stability
  sig.info_gain = info_gain;
  sig.reward_total = reward_total;
  sig.eta_effective = eta_effective;
  sig.sparsity_cost = sparsity_cost;
  sig.hebbian_corr = hebbian_pre_post;
  sig.timestamp_ms = ts;

  // 9. Atomic broadcast (lock-free for readers)
  latest_delta_.store(sig.delta, std::memory_order_relaxed);
  latest_info_gain_.store(sig.info_gain, std::memory_order_relaxed);
  latest_reward_total_.store(sig.reward_total, std::memory_order_relaxed);
  latest_eta_.store(sig.eta_effective, std::memory_order_relaxed);
  latest_sparsity_.store(sig.sparsity_cost, std::memory_order_relaxed);
  latest_hebbian_.store(sig.hebbian_corr, std::memory_order_relaxed);
  latest_ts_.store(sig.timestamp_ms, std::memory_order_relaxed);

  // 10. Accumulate telemetry
  cumulative_info_gain_.store(
      cumulative_info_gain_.load(std::memory_order_relaxed) + info_gain,
      std::memory_order_relaxed);
  compute_count_.fetch_add(1, std::memory_order_relaxed);

  // 11. Record history (guarded)
  {
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.push_back({sig.delta, sig.info_gain, sig.eta_effective,
                        sig.reward_total, sig.timestamp_ms});
    while (history_.size() > config_.history_capacity) {
      history_.pop_front();
    }
  }

  return sig;
}

UniversalSignal UniversalLearningSignal::getLatest() const noexcept {
  UniversalSignal sig;
  sig.delta = latest_delta_.load(std::memory_order_relaxed);
  sig.info_gain = latest_info_gain_.load(std::memory_order_relaxed);
  sig.reward_total = latest_reward_total_.load(std::memory_order_relaxed);
  sig.eta_effective = latest_eta_.load(std::memory_order_relaxed);
  sig.sparsity_cost = latest_sparsity_.load(std::memory_order_relaxed);
  sig.hebbian_corr = latest_hebbian_.load(std::memory_order_relaxed);
  sig.timestamp_ms = latest_ts_.load(std::memory_order_relaxed);
  return sig;
}

float UniversalLearningSignal::getSmoothedDelta() const noexcept {
  return smoothed_delta_.load(std::memory_order_relaxed);
}

float UniversalLearningSignal::getCumulativeInfoGain() const noexcept {
  return cumulative_info_gain_.load(std::memory_order_relaxed);
}

void UniversalLearningSignal::setConfig(const UniversalSignalConfig &cfg) {
  std::lock_guard<std::mutex> lock(history_mutex_);
  config_ = cfg;
}

std::deque<UniversalLearningSignal::HistoryEntry>
UniversalLearningSignal::getHistory() const {
  std::lock_guard<std::mutex> lock(history_mutex_);
  return history_;
}

void UniversalLearningSignal::reset() {
  latest_delta_.store(0.0f, std::memory_order_relaxed);
  latest_info_gain_.store(0.0f, std::memory_order_relaxed);
  latest_reward_total_.store(0.0f, std::memory_order_relaxed);
  latest_eta_.store(config_.eta_base, std::memory_order_relaxed);
  latest_sparsity_.store(0.0f, std::memory_order_relaxed);
  latest_hebbian_.store(0.0f, std::memory_order_relaxed);
  latest_ts_.store(0, std::memory_order_relaxed);
  smoothed_delta_.store(0.0f, std::memory_order_relaxed);
  cumulative_info_gain_.store(0.0f, std::memory_order_relaxed);
  compute_count_.store(0, std::memory_order_relaxed);
  {
    std::lock_guard<std::mutex> lock(history_mutex_);
    history_.clear();
  }
}

uint64_t UniversalLearningSignal::getComputeCount() const noexcept {
  return compute_count_.load(std::memory_order_relaxed);
}

} // namespace Core
} // namespace NeuroForge
