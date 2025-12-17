#pragma once

#include "core/Types.h"
#include "memory/EnhancedEpisode.h"
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace NeuroForge {
namespace Memory {

/**
 * @brief Episode representation for episodic memory
 */
struct Episode {
    std::uint64_t id;
    std::chrono::steady_clock::time_point timestamp;
    std::string context;
    std::vector<float> sensory_data;
    std::vector<float> emotional_state;
    std::string narrative;
    float salience = 0.0f;
    bool consolidated = false;
};

/**
 * @brief Memory trace for episodic recall
 */
struct MemoryTrace {
    std::uint64_t episode_id;
    float activation_strength = 0.0f;
    std::chrono::steady_clock::time_point last_accessed;
    std::uint32_t access_count = 0;
};

/**
 * @brief Configuration for episodic memory manager
 */
struct EpisodicConfig {
    std::size_t max_episodes = 10000;
    float consolidation_threshold = 0.7f;
    float decay_rate = 0.01f;
    std::size_t context_window = 5;
    bool enable_forgetting = true;
};

/**
 * @brief Manages episodic memory formation, storage, and retrieval
 */
class EpisodicMemoryManager {
public:
    explicit EpisodicMemoryManager(const EpisodicConfig& config = EpisodicConfig{});
    ~EpisodicMemoryManager() = default;

    // Episode management
    std::uint64_t storeEpisode(const std::string& context, 
                              const std::vector<float>& sensory_data,
                              const std::vector<float>& emotional_state,
                              const std::string& narrative = "");
    
    std::shared_ptr<Episode> retrieveEpisode(std::uint64_t episode_id);
    std::vector<std::shared_ptr<Episode>> searchEpisodes(const std::string& query, 
                                                         std::size_t max_results = 10);
    
    // Memory operations
    void consolidateMemories();
    void updateSalience(std::uint64_t episode_id, float salience);
    void forgetOldMemories();
    
    // Statistics and monitoring
    std::size_t getEpisodeCount() const { return episodes_.size(); }
    std::size_t getConsolidatedCount() const;
    float getAverageActivation() const;

    // Extended statistics (advanced API)
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
    };
    Statistics getStatistics() const;
    
    // Configuration
    void updateConfig(const EpisodicConfig& config) { config_ = config; }
    const EpisodicConfig& getConfig() const { return config_; }

private:
    void decayMemoryTraces();
    float calculateSimilarity(const Episode& a, const Episode& b) const;
    void pruneWeakMemories();

private:
    EpisodicConfig config_;
    std::unordered_map<std::uint64_t, std::shared_ptr<Episode>> episodes_;
    std::unordered_map<std::uint64_t, MemoryTrace> memory_traces_;
    std::uint64_t next_episode_id_ = 1;

    // Fields used by extended API present in cpp
    mutable std::mutex episodes_mutex_;
    std::vector<EnhancedEpisode> recent_episodes_;
    std::vector<EnhancedEpisode> consolidated_episodes_;
    std::unordered_map<std::string, std::vector<size_t>> context_index_;
    std::unordered_map<std::uint64_t, size_t> episode_id_index_;
    std::atomic<std::size_t> total_episodes_recorded_{0};
    std::atomic<std::size_t> total_consolidations_{0};
    std::atomic<std::size_t> total_retrievals_{0};
    std::atomic<std::size_t> successful_retrievals_{0};
};

} // namespace Memory
} // namespace NeuroForge