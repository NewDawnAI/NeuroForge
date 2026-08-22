#pragma once
/**
 * @file UniversalLearningSignal.h
 * @brief Unified biological learning signal: Δw = η · δ · ∇_w I(gain)
 *
 * Broadcasts a single learning signal that all subsystems consume.
 * Based on the equation:
 *   Δw ∝ η · (r_intrinsic + γ · max(I_future)) · (ŷ − y) · ∂f(w)/∂w
 * Shortened to: Δw = η · δ · ∇_w I(gain)
 *
 * Where:
 *   δ = prediction error from WorldModelCortex
 *   I_gain = information gain / curiosity from IntrinsicMotivationSystem
 *   η = modulated learning rate (scales with |δ|)
 *
 * The signal is computed once per tick and broadcast to:
 *   - LearningSystem (reward shaping)
 *   - Synapses with PlasticityRule::Universal
 *   - Cognitive phases (Phase 7-9 reflection gating)
 */

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>

namespace NeuroForge {
namespace Core {

/**
 * @brief Snapshot of the universal learning signal at a moment in time
 */
struct UniversalSignal {
  float delta = 0.0f;          ///< δ: prediction error (surprise)
  float info_gain = 0.0f;      ///< I_gain: curiosity / information gain
  float reward_total = 0.0f;   ///< r_total = r_ext + γ · I_gain
  float eta_effective = 0.01f; ///< η_eff: modulated learning rate
  float sparsity_cost = 0.0f;  ///< λ‖a‖₁: compression pressure
  float hebbian_corr = 0.0f;   ///< Pre-post correlation (Hebbian term)
  uint64_t timestamp_ms = 0;   ///< Milliseconds since epoch
};

/**
 * @brief Configuration for the universal learning signal computation
 */
struct UniversalSignalConfig {
  float gamma = 0.99f;       ///< Discount for future info gain
  float alpha = 0.5f;        ///< Error-proportional rate scaling
  float beta_sparse = 0.01f; ///< Sparsity penalty coefficient
  float eta_base = 0.01f;    ///< Base learning rate (η₀)
  float eta_min = 0.001f;    ///< Minimum effective learning rate
  float eta_max = 0.1f;      ///< Maximum effective learning rate (safety cap)
  float delta_ema = 0.95f;   ///< EMA smoothing for delta (stability)
  std::size_t history_capacity = 1000; ///< Signal history buffer size
};

/**
 * @brief Universal Learning Signal — unifies δ and I_gain into one broadcast
 *
 * Thread-safe: compute() may be called from any thread; getLatest() is
 * lock-free.
 */
class UniversalLearningSignal {
public:
  explicit UniversalLearningSignal(const UniversalSignalConfig &cfg = {});

  /**
   * @brief Compute the unified signal from subsystem outputs
   *
   * @param surprise         WorldModelCortex::getLastSurpriseLevel()
   * @param intrinsic_composite IntrinsicMotivationSystem composite score
   * @param external_reward  Task/environment reward signal
   * @param sparsity_ratio   Fraction of active neurons (‖a‖₀ / N)
   * @param hebbian_pre_post Pre × post synaptic correlation (optional)
   * @return Computed UniversalSignal
   */
  UniversalSignal compute(float surprise, float intrinsic_composite,
                          float external_reward, float sparsity_ratio = 0.0f,
                          float hebbian_pre_post = 0.0f);

  /**
   * @brief Get latest signal (lock-free atomic read)
   */
  UniversalSignal getLatest() const noexcept;

  /**
   * @brief Get smoothed delta (EMA-filtered prediction error)
   */
  float getSmoothedDelta() const noexcept;

  /**
   * @brief Get cumulative information gain over session
   */
  float getCumulativeInfoGain() const noexcept;

  /**
   * @brief Update config at runtime
   */
  void setConfig(const UniversalSignalConfig &cfg);

  /**
   * @brief Get signal history for analysis
   */
  struct HistoryEntry {
    float delta;
    float info_gain;
    float eta;
    float reward_total;
    uint64_t timestamp_ms;
  };
  std::deque<HistoryEntry> getHistory() const;

  /**
   * @brief Reset all state (for new runs)
   */
  void reset();

  /**
   * @brief Get total number of compute() calls
   */
  uint64_t getComputeCount() const noexcept;

private:
  UniversalSignalConfig config_;

  // Atomic latest signal (lock-free broadcast)
  // We store components individually for atomic access
  std::atomic<float> latest_delta_{0.0f};
  std::atomic<float> latest_info_gain_{0.0f};
  std::atomic<float> latest_reward_total_{0.0f};
  std::atomic<float> latest_eta_{0.01f};
  std::atomic<float> latest_sparsity_{0.0f};
  std::atomic<float> latest_hebbian_{0.0f};
  std::atomic<uint64_t> latest_ts_{0};

  // Smoothed / accumulated state
  std::atomic<float> smoothed_delta_{0.0f};
  std::atomic<float> cumulative_info_gain_{0.0f};
  std::atomic<uint64_t> compute_count_{0};

  // History buffer (guarded by mutex)
  mutable std::mutex history_mutex_;
  std::deque<HistoryEntry> history_;
};

} // namespace Core
} // namespace NeuroForge
