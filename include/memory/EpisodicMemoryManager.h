#pragma once

#include "core/Types.h"
#include "memory/EnhancedEpisode.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <numeric>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace NeuroForge {
namespace Memory {

// ─────────────────────────────────────────────────────────────────
//  Named constants (replacing scattered magic numbers)
// ─────────────────────────────────────────────────────────────────
constexpr float kDefaultSalience = 0.5f;
constexpr float kMinSearchSimilarity = 0.1f;
constexpr float kActivationBoost = 0.1f;
constexpr float kConsolidationStep = 0.1f;
constexpr float kPruneActivationThreshold = 0.01f;
constexpr float kDefaultMinForgetStrength = 0.1f;
constexpr std::size_t kMaxRecentEpisodes = 200;

// ─────────────────────────────────────────────────────────────────
//  EpisodicPattern: hippocampal-style reactivation pattern
//    NO strings. Context and narrative ARE vectors.
// ─────────────────────────────────────────────────────────────────
struct EpisodicPattern {
  std::vector<float> sensory_state; // what happened
  std::vector<float>
      context_state; // where/when encoding (replaces string context)
  std::vector<float> emotional_state;  // emotional/affect state
  float salience = 0.0f;               // importance weighting
  float consolidation_strength = 0.0f; // how consolidated this pattern is
  uint64_t timestamp_ms = 0;           // when it was encoded
  bool consolidated = false;
};

// ─────────────────────────────────────────────────────────────────
//  Legacy Episode — DEPRECATED. Use EpisodicPattern instead.
//  Kept only so downstream code that references the type still
//  compiles with a deprecation warning.
// ─────────────────────────────────────────────────────────────────
struct [[deprecated(
    "Use EpisodicPattern (vector-only) instead of Episode (string-based)")]]
Episode {
  std::uint64_t id;
  std::chrono::steady_clock::time_point timestamp;
  std::string context;
  std::vector<float> sensory_data;
  std::vector<float> emotional_state;
  std::string narrative;
  float salience = 0.0f;
  bool consolidated = false;
};

// ─────────────────────────────────────────────────────────────────
//  Memory trace for episodic recall
// ─────────────────────────────────────────────────────────────────
struct MemoryTrace {
  std::uint64_t episode_id;
  float activation_strength = 0.0f;
  std::chrono::steady_clock::time_point last_accessed;
  std::uint32_t access_count = 0;
};

// ─────────────────────────────────────────────────────────────────
//  Configuration
// ─────────────────────────────────────────────────────────────────
struct EpisodicConfig {
  std::size_t max_episodes = 10000;
  float consolidation_threshold = 0.7f;
  float decay_rate = 0.01f;
  std::size_t context_window = 5;
  bool enable_forgetting = true;

  // Pattern-based config (new)
  std::size_t pattern_dim = 64; // dimensionality of context/pattern vectors
  float pattern_completion_threshold = 0.6f; // min similarity for recall
};

// ─────────────────────────────────────────────────────────────────
//  EpisodicMemoryManager
//
//  Dual-mode: pattern-based hippocampal index (primary) + legacy
//  string-based Episode API (backward compat).
// ─────────────────────────────────────────────────────────────────
class EpisodicMemoryManager {
public:
  explicit EpisodicMemoryManager(
      const EpisodicConfig &config = EpisodicConfig{});
  ~EpisodicMemoryManager() = default;

  // ═══════════════════════════════════════════════════════════
  //  PATTERN-BASED API (hippocampal index)
  // ═══════════════════════════════════════════════════════════

  /// Encode a new episodic pattern (no strings)
  void encodePattern(const std::vector<float> &sensory,
                     const std::vector<float> &context,
                     const std::vector<float> &emotional,
                     float salience = 0.5f);

  /// Pattern-completion recall: given a cue, find the closest stored pattern
  std::vector<EpisodicPattern> recallByCue(const std::vector<float> &cue,
                                           std::size_t max_results = 5,
                                           float threshold = -1.0f) const;

  /// Get all stored patterns (thread-safe)
  std::vector<EpisodicPattern> getPatterns() const {
    std::shared_lock<std::shared_mutex> lock(episodes_mutex_);
    return patterns_;
  }

  /// Load patterns from Cap'n Proto
  void loadPatterns(const std::vector<EpisodicPattern> &patterns);

  /// Consolidate: strengthen high-salience patterns, weaken low
  void consolidatePatterns();

  /// Pattern-based forgetting
  void forgetWeakPatterns(float min_strength = 0.1f);

  // ═══════════════════════════════════════════════════════════
  //  LEGACY API — DEPRECATED
  //  Use encodePattern() / recallByCue() instead.
  // ═══════════════════════════════════════════════════════════

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

  // Episode management (deprecated — use encodePattern)
  [[deprecated("Use encodePattern() instead")]]
  std::uint64_t storeEpisode(const std::string &context,
                             const std::vector<float> &sensory_data,
                             const std::vector<float> &emotional_state,
                             const std::string &narrative = "");

  [[deprecated("Use recallByCue() instead")]]
  std::shared_ptr<Episode> retrieveEpisode(std::uint64_t episode_id);

  [[deprecated("Use recallByCue() instead")]]
  std::vector<std::shared_ptr<Episode>>
  searchEpisodes(const std::string &query, std::size_t max_results = 10);

  [[deprecated("Use recallByCue() instead")]]
  std::vector<std::shared_ptr<Episode>>
  searchByVector(const std::vector<float> &query_vector,
                 std::size_t max_results = 10);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

  // Get recent episodes for DreamProcessor/SleepConsolidation
  std::vector<EnhancedEpisode>
  getRecentEpisodes(std::uint64_t time_window_ms,
                    std::size_t max_count = 100) const;

  // Memory operations
  void consolidateMemories();
  void updateSalience(std::uint64_t episode_id, float salience);
  void forgetOldMemories();

  // Statistics and monitoring
  std::size_t getEpisodeCount() const { return patterns_.size(); }
  std::size_t getConsolidatedCount() const;
  float getAverageActivation() const;

  // Extended statistics
  struct Statistics {
    std::size_t total_episodes_recorded = 0;
    std::size_t recent_episodes_count = 0;
    std::size_t consolidated_episodes_count = 0;
    std::size_t total_consolidations = 0;
    std::size_t total_retrievals = 0;
    std::size_t successful_retrievals = 0;
    std::size_t context_categories_count = 0;
    float average_episode_age_ms = 0.0f;
    float average_consolidation_strength = 0.0f;
    float retrieval_success_rate = 0.0f;
    bool consolidation_active = false;

    // Pattern-based stats (new)
    std::size_t total_patterns = 0;
    float average_pattern_salience = 0.0f;
    std::size_t pattern_recalls = 0;
  };
  Statistics getStatistics() const;

  // Configuration
  void updateConfig(const EpisodicConfig &config) { config_ = config; }
  const EpisodicConfig &getConfig() const { return config_; }

private:
  void decayMemoryTraces();
  void pruneWeakMemories();
  float cosineSim(const std::vector<float> &a,
                  const std::vector<float> &b) const;
  std::vector<float> encodeString(const std::string &s) const;

  // Lock-free versions (caller must hold episodes_mutex_)
  void encodePatternLocked(const std::vector<float> &sensory,
                           const std::vector<float> &context,
                           const std::vector<float> &emotional, float salience);
  void consolidatePatternsLocked();
  void forgetWeakPatternsLocked(float min_strength);
  void decayMemoryTracesLocked();
  void pruneWeakMemoriesLocked();

private:
  EpisodicConfig config_;

  // ═══ PRIMARY STORAGE: hippocampal pattern index ═══
  std::vector<EpisodicPattern> patterns_;
  std::size_t pattern_dim_;

  // Legacy storage removed — all data now lives in patterns_ + recent_episodes_
  std::uint64_t next_episode_id_ = 1; // kept for ID generation only

  // Fields used by extended API
  mutable std::shared_mutex episodes_mutex_;
  std::vector<EnhancedEpisode> recent_episodes_;
  std::unordered_map<std::string, std::vector<size_t>> context_index_;
  std::unordered_map<std::uint64_t, size_t> episode_id_index_;
  std::atomic<std::size_t> total_episodes_recorded_{0};
  std::atomic<std::size_t> total_consolidations_{0};
  std::atomic<std::size_t> total_retrievals_{0};
  std::atomic<std::size_t> successful_retrievals_{0};
  mutable std::atomic<std::size_t> pattern_recalls_{0};
};

} // namespace Memory
} // namespace NeuroForge