#pragma once

#include "core/Types.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <memory>
#include <mutex>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

namespace NeuroForge {
namespace Memory {

// Forward declaration for cross-system integration
class EpisodicMemoryManager;

// ─────────────────────────────────────────────────────────────────
//  Legacy WorkingMemoryItem (PRESERVED for backward compat)
// ─────────────────────────────────────────────────────────────────
struct WorkingMemoryItem {
  std::uint64_t id;
  std::string content;
  std::vector<float> representation;
  float activation_level = 1.0f;
  std::chrono::steady_clock::time_point creation_time;
  std::chrono::steady_clock::time_point last_access;
  std::uint32_t access_count = 0;
  bool rehearsed = false;
};

// ─────────────────────────────────────────────────────────────────
//  Configuration
// ─────────────────────────────────────────────────────────────────
struct WorkingMemoryConfig {
  std::size_t capacity = 7;                    // Miller's magic number
  std::chrono::milliseconds decay_time{15000}; // 15 seconds
  float decay_rate = 0.1f;
  bool enable_rehearsal = true;
  float rehearsal_boost = 0.2f;
  std::size_t max_rehearsal_items = 3;
  // Phase 2 compatibility thresholds
  float refresh_threshold = 0.3f; // Minimum activation to keep item active
  float push_threshold = 0.1f; // Minimum activation required to accept a push
  float consolidation_threshold =
      0.7f; // Activation level required for long-term transfer

  // Weight-based config (new)
  std::size_t slot_dim = 64; // dimensionality of each slot pattern
  float gating_lr = 0.01f;   // learning rate for gating weights
};

// ─────────────────────────────────────────────────────────────────
//  Statistics
// ─────────────────────────────────────────────────────────────────
struct WorkingMemoryStats {
  std::size_t current_load = 0;
  std::size_t total_items_processed = 0;
  std::size_t items_forgotten = 0;
  std::size_t items_rehearsed = 0;
  float average_retention_time = 0.0f;
  float capacity_utilization = 0.0f;

  // Weight-based stats (new)
  float average_slot_activation = 0.0f;
  float gating_weight_norm = 0.0f;
  std::size_t total_pushes = 0;
  std::size_t total_recalls = 0;
};

// ─────────────────────────────────────────────────────────────────
//  WorkingMemory
//
//  Dual-mode: activation buffer (primary) + legacy API adapters.
//  Internally, each slot holds an activation pattern (vector<float>)
//  with a scalar activation level that decays naturally.
//  Gating weights determine which incoming patterns get stored
//  and which displaced items get forgotten.
// ─────────────────────────────────────────────────────────────────
static constexpr std::size_t MAX_SLOTS = 9; // Miller's 7±2

class WorkingMemory {
public:
  explicit WorkingMemory(
      const WorkingMemoryConfig &config = WorkingMemoryConfig{});
  ~WorkingMemory() = default;

  // Phase 2 compatibility aliases and constants
  using Config = WorkingMemoryConfig;
  static constexpr std::size_t MILLER_CAPACITY = 7;

  // ═══════════════════════════════════════════════════════════
  //  ACTIVATION BUFFER API  (the real working memory)
  // ═══════════════════════════════════════════════════════════

  /// Push a pattern into working memory with given activation
  /// Returns true if accepted, false if gated out
  bool push(const std::vector<float> &representation, float activation,
            const std::string &name = "");

  /// Get activation pattern at slot index
  std::vector<float> getSlotContent(std::size_t slot_index) const;

  /// Get all slot patterns as a combined active content
  std::vector<float> getActiveContent() const;

  /// Get content of most active slot
  std::vector<float> getMostActiveContent() const;

  /// Decay all activations by delta_time
  void decay(float delta_time);

  /// Refresh a slot's activation
  bool refresh(std::size_t slot_index, float new_activation);

  /// Refresh items similar to a query
  std::size_t refreshBySimilarity(const std::vector<float> &query,
                                  float similarity_threshold,
                                  float activation_boost);

  /// Find index of similar slot
  std::size_t findSimilarSlot(const std::vector<float> &query,
                              float similarity_threshold) const;

  /// Get gating weights (for serialization)
  const std::vector<float> &getGatingWeights() const { return gating_weights_; }

  /// Load gating weights (from Cap'n Proto)
  void loadGatingWeights(const std::vector<float> &weights);

  /// Get average activation across slots
  float getAverageActivation() const;

  // ═══════════════════════════════════════════════════════════
  //  LEGACY API  (adapters — preserved for callers)
  // ═══════════════════════════════════════════════════════════

  // Item management
  std::uint64_t addItem(const std::string &content,
                        const std::vector<float> &representation);
  std::shared_ptr<WorkingMemoryItem> getItem(std::uint64_t item_id);
  bool removeItem(std::uint64_t item_id);
  void clear();

  // Capacity management
  bool isFull() const { return active_count_ >= config_.capacity; }
  std::size_t getCurrentLoad() const { return active_count_; }
  std::size_t getCapacity() const { return config_.capacity; }
  float getUtilization() const;

  // Phase 2 compatibility wrappers
  std::size_t getOccupiedSlots() const { return active_count_; }
  float getCapacityUtilization() const { return getUtilization(); }

  // Activation and decay
  void updateActivations(float delta_time);
  void rehearseItems();
  void boostActivation(std::uint64_t item_id, float boost = 0.2f);

  // Retrieval and search
  std::vector<std::shared_ptr<WorkingMemoryItem>> getAllItems() const;
  std::vector<std::shared_ptr<WorkingMemoryItem>>
  getActiveItems(float threshold = 0.1f) const;
  std::shared_ptr<WorkingMemoryItem> findMostActive() const;
  std::shared_ptr<WorkingMemoryItem>
  findByContent(const std::string &content) const;

  // Memory operations
  void consolidateToLongTerm();
  void refreshItem(std::uint64_t item_id);
  void forgetWeakestItem();

  // Chunking and organization
  std::uint64_t createChunk(const std::vector<std::uint64_t> &item_ids,
                            const std::string &chunk_name);
  void expandChunk(std::uint64_t chunk_id);

  // Statistics and monitoring
  const WorkingMemoryStats &getStatistics() const { return statistics_; }
  void updateStatistics();

  // Configuration
  void updateConfig(const WorkingMemoryConfig &config) { config_ = config; }
  const WorkingMemoryConfig &getConfig() const { return config_; }

  // Cross-system integration
  void setEpisodicMemory(EpisodicMemoryManager *episodic) {
    episodic_memory_ = episodic;
  }

private:
  void enforceCapacityLimit();
  void decayItems(float delta_time);
  void removeExpiredItems();
  void selectItemsForRehearsal();
  float calculateRetentionProbability(const WorkingMemoryItem &item) const;
  float cosineSim(const std::vector<float> &a,
                  const std::vector<float> &b) const;

  /// Compute gating score for incoming pattern (learned attention gate)
  float computeGatingScore(const std::vector<float> &pattern) const;

private:
  WorkingMemoryConfig config_;

  // ═══ PRIMARY STORAGE: activation buffer ═══
  std::array<std::vector<float>, MAX_SLOTS>
      slots_; // content = activation patterns
  std::array<float, MAX_SLOTS> activation_levels_{}; // decay naturally
  std::array<bool, MAX_SLOTS> slot_occupied_{};      // occupancy
  std::size_t active_count_ = 0;
  std::size_t slot_dim_;

  // Learned gating weights: what to attend to
  std::vector<float> gating_weights_;

  // ═══ LEGACY STORAGE: adapters only ═══
  std::deque<std::shared_ptr<WorkingMemoryItem>> items_;
  std::unordered_map<std::uint64_t, std::shared_ptr<WorkingMemoryItem>>
      item_lookup_;
  mutable WorkingMemoryStats statistics_;
  std::uint64_t next_item_id_ = 1;
  std::chrono::steady_clock::time_point last_update_;

  // Cross-system integration
  EpisodicMemoryManager *episodic_memory_ = nullptr;

  // Chunk tracking for expandChunk()
  std::unordered_map<std::uint64_t,
                     std::vector<std::pair<std::string, std::vector<float>>>>
      chunk_constituents_;
};

} // namespace Memory
} // namespace NeuroForge