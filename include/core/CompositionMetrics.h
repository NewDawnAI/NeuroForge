#pragma once
/**
 * @file CompositionMetrics.h
 * @brief Stage 3 — Compositional Emergence Metrics
 *
 * Measures whether merged representations are truly compositional
 * (where the whole is efficiently explained by its parts) using a
 * reconstruction-error proxy for conditional Kolmogorov complexity:
 *
 *   K(whole | part_a, part_b) ≈ ||whole − reconstruct(part_a, part_b)||²
 *
 * When this is low, a genuine compositional structure has emerged
 * (proto-symbiogenesis: parts combine into a novel, more efficient whole).
 */

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace NeuroForge {
namespace Core {

/**
 * @brief Score measuring how compositional a merged representation is
 */
struct CompositionScore {
  float conditional_complexity =
      1.0f; ///< K(whole | parts) proxy (lower = more compositional)
  float reconstruction_error = 1.0f; ///< ||whole − reconstruct(a,b)||²
  float assembly_cost =
      0.0f; ///< Information cost of merger: ||a||+||b|| vs ||whole||
  float compression_ratio = 1.0f; ///< dim(whole) / (dim(a) + dim(b))
  bool is_compositional = false; ///< True if conditional_complexity < threshold
};

/**
 * @brief Configuration for composition metrics
 */
struct CompositionConfig {
  float composition_threshold =
      0.3f;                  ///< Below this, merger is deemed compositional
  float noise_sigma = 0.05f; ///< Gaussian noise for creative recombination
  std::size_t history_capacity = 500; ///< Merger history buffer size
  float ema_alpha = 0.1f; ///< EMA rate for tracking running composition score
};

/**
 * @brief Merger history entry
 */
struct MergerRecord {
  uint64_t entity_a = 0;
  uint64_t entity_b = 0;
  CompositionScore score;
  uint64_t timestamp_ms = 0;
};

/**
 * @brief Compositional Emergence Metrics
 *
 * Evaluates whether merged representations exhibit true compositionality
 * (efficient encoding of wholes from parts). Used during dream/consolidation
 * to discover emergent hierarchical structures.
 */
class CompositionMetrics {
public:
  explicit CompositionMetrics(const CompositionConfig &cfg = {});

  /**
   * @brief Evaluate composability of a merged representation
   *
   * Computes reconstruction error as a proxy for conditional complexity.
   * If recombining parts a and b can predict the whole, it's compositional.
   *
   * @param whole  The merged representation vector
   * @param part_a First component representation
   * @param part_b Second component representation
   * @return CompositionScore with compositionality metrics
   */
  CompositionScore evaluate(const std::vector<float> &whole,
                            const std::vector<float> &part_a,
                            const std::vector<float> &part_b) const;

  /**
   * @brief Record a merger event for history tracking
   */
  void recordMerger(uint64_t entity_a, uint64_t entity_b,
                    const CompositionScore &score);

  /**
   * @brief Inject Gaussian noise for creative recombination
   *
   * Adds thermal noise to a representation, then tests whether
   * the perturbed version is still compositional with its parts.
   *
   * @param repr Representation to perturb
   * @param sigma Noise standard deviation (0 = use default config)
   * @return Perturbed representation
   */
  std::vector<float> perturbForCreativity(const std::vector<float> &repr,
                                          float sigma = 0.0f) const;

  /**
   * @brief Get running average composition score
   */
  float getRunningCompositionScore() const noexcept;

  /**
   * @brief Get total merger count
   */
  uint64_t getTotalMergers() const noexcept;

  /**
   * @brief Get count of compositional mergers (is_compositional == true)
   */
  uint64_t getCompositionalMergers() const noexcept;

  /**
   * @brief Get merger history
   */
  std::deque<MergerRecord> getHistory() const;

  /**
   * @brief Update config at runtime
   */
  void setConfig(const CompositionConfig &cfg);

  /**
   * @brief Reset all state
   */
  void reset();

private:
  CompositionConfig config_;

  // Running statistics
  std::atomic<float> running_score_{1.0f};
  std::atomic<uint64_t> total_mergers_{0};
  std::atomic<uint64_t> compositional_mergers_{0};

  // History buffer
  mutable std::mutex history_mutex_;
  std::deque<MergerRecord> history_;
};

} // namespace Core
} // namespace NeuroForge
