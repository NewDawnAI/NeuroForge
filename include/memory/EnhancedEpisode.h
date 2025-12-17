#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace NeuroForge {
namespace Memory {

// Unified EnhancedEpisode structure used across Episodic, Semantic, and SleepConsolidation.
struct EnhancedEpisode {
    // Core state snapshots
    std::vector<float> sensory_state;   // Features from sensory inputs
    std::vector<float> action_state;    // Features representing actions taken
    std::vector<float> substrate_state; // Snapshot features from neural substrate
    std::string context_tag;            // Optional context label to prefix concepts

    // Novelty metrics observed during the episode
    struct NoveltyMetrics {
        float prediction_error = 0.0f;
        float information_gain = 0.0f;
        float surprise_level = 0.0f;
        float attention_level = 0.0f;
    } novelty_metrics;

    // Affect and reinforcement
    float emotional_weight = 0.0f; // Contextual affect associated with the episode
    float reward_signal = 0.0f;    // Reinforcement signal associated with the episode

    // Temporal and consolidation tracking
    std::uint64_t timestamp_ms = 0;            // Occurrence time in milliseconds since epoch
    float consolidation_strength = 0.0f;       // Strength used for consolidation decisions
    std::vector<std::uint64_t> related_episodes; // Simple links to other episode IDs

    // Convenience helpers used by various subsystems
    std::uint64_t getAge() const {
        const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return (timestamp_ms <= now_ms) ? (now_ms - timestamp_ms) : 0ULL;
    }

    bool shouldConsolidate(float threshold) const {
        return consolidation_strength >= threshold;
    }
};

} // namespace Memory
} // namespace NeuroForge