#pragma once

#include "core/Types.h"
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <deque>
#include <unordered_map>

namespace NeuroForge {
namespace Memory {

/**
 * @brief Working memory item
 */
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

/**
 * @brief Working memory buffer configuration
 */
struct WorkingMemoryConfig {
    std::size_t capacity = 7; // Miller's magic number
    std::chrono::milliseconds decay_time{15000}; // 15 seconds
    float decay_rate = 0.1f;
    bool enable_rehearsal = true;
    float rehearsal_boost = 0.2f;
    std::size_t max_rehearsal_items = 3;
    // Phase 2 compatibility thresholds
    float refresh_threshold = 0.3f;   // Minimum activation to keep item active
    float push_threshold = 0.1f;      // Minimum activation required to accept a push
};

/**
 * @brief Working memory statistics
 */
struct WorkingMemoryStats {
    std::size_t current_load = 0;
    std::size_t total_items_processed = 0;
    std::size_t items_forgotten = 0;
    std::size_t items_rehearsed = 0;
    float average_retention_time = 0.0f;
    float capacity_utilization = 0.0f;
};

/**
 * @brief Manages working memory for temporary information storage and manipulation
 */
class WorkingMemory {
public:
    explicit WorkingMemory(const WorkingMemoryConfig& config = WorkingMemoryConfig{});
    ~WorkingMemory() = default;

    // Phase 2 compatibility aliases and constants
    using Config = WorkingMemoryConfig;
    static constexpr std::size_t MILLER_CAPACITY = 7;

    // Item management
    std::uint64_t addItem(const std::string& content, const std::vector<float>& representation);
    std::shared_ptr<WorkingMemoryItem> getItem(std::uint64_t item_id);
    bool removeItem(std::uint64_t item_id);
    void clear();
    
    // Capacity management
    bool isFull() const { return items_.size() >= config_.capacity; }
    std::size_t getCurrentLoad() const { return items_.size(); }
    std::size_t getCapacity() const { return config_.capacity; }
    float getUtilization() const;

    // Phase 2 compatibility wrappers
    // Push an item representation with activation and optional name
    bool push(const std::vector<float>& representation, float activation, const std::string& name = "");
    // Get number of occupied slots
    std::size_t getOccupiedSlots() const { return items_.size(); }
    // Capacity utilization convenience
    float getCapacityUtilization() const { return getUtilization(); }
    // Average activation across items
    float getAverageActivation() const;
    // Get slot content by index
    std::vector<float> getSlotContent(std::size_t slot_index) const;
    // Decay wrapper
    void decay(float delta_time);
    // Refresh a slot's activation
    bool refresh(std::size_t slot_index, float new_activation);
    // Refresh items similar to a query
    std::size_t refreshBySimilarity(const std::vector<float>& query,
                                    float similarity_threshold,
                                    float activation_boost);
    // Get combined active content (simple aggregate)
    std::vector<float> getActiveContent() const;
    // Get content of most active item
    std::vector<float> getMostActiveContent() const;
    // Find index of similar slot
    std::size_t findSimilarSlot(const std::vector<float>& query, float similarity_threshold) const;
    
    // Activation and decay
    void updateActivations(float delta_time);
    void rehearseItems();
    void boostActivation(std::uint64_t item_id, float boost = 0.2f);
    
    // Retrieval and search
    std::vector<std::shared_ptr<WorkingMemoryItem>> getAllItems() const;
    std::vector<std::shared_ptr<WorkingMemoryItem>> getActiveItems(float threshold = 0.1f) const;
    std::shared_ptr<WorkingMemoryItem> findMostActive() const;
    std::shared_ptr<WorkingMemoryItem> findByContent(const std::string& content) const;
    
    // Memory operations
    void consolidateToLongTerm(); // Interface for long-term memory transfer
    void refreshItem(std::uint64_t item_id);
    void forgetWeakestItem();
    
    // Chunking and organization
    std::uint64_t createChunk(const std::vector<std::uint64_t>& item_ids, const std::string& chunk_name);
    void expandChunk(std::uint64_t chunk_id);
    
    // Statistics and monitoring
    const WorkingMemoryStats& getStatistics() const { return statistics_; }
    void updateStatistics();
    
    // Configuration
    void updateConfig(const WorkingMemoryConfig& config) { config_ = config; }
    const WorkingMemoryConfig& getConfig() const { return config_; }

private:
    void enforceCapacityLimit();
    void decayItems(float delta_time);
    void removeExpiredItems();
    void selectItemsForRehearsal();
    float calculateRetentionProbability(const WorkingMemoryItem& item) const;

private:
    WorkingMemoryConfig config_;
    std::deque<std::shared_ptr<WorkingMemoryItem>> items_;
    std::unordered_map<std::uint64_t, std::shared_ptr<WorkingMemoryItem>> item_lookup_;
    WorkingMemoryStats statistics_;
    std::uint64_t next_item_id_ = 1;
    std::chrono::steady_clock::time_point last_update_;
};

} // namespace Memory
} // namespace NeuroForge