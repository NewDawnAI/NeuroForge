#pragma once

#include "core/Types.h"
#include "memory/EnhancedEpisode.h"
#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <mutex>
#include <thread>
#include <random>

namespace NeuroForge {
namespace Memory {

// Forward declarations for memory systems
class EpisodicMemoryManager;
class SemanticMemory;
class WorkingMemory;
class ProceduralMemory;

} // namespace Memory

namespace Core {
class LearningSystem;
class HypergraphBrain;
} // namespace Core

namespace Memory {

// EnhancedEpisode now provided by include/memory/EnhancedEpisode.h

// Sleep consolidation configuration
struct ConsolidationConfig {
    // Replay
    float replay_speed_multiplier = 2.0f;
    std::size_t max_replay_episodes = 100;
    
    // Synaptic scaling
    float synaptic_scaling_factor = 0.95f;
    bool enable_homeostatic_scaling = true;
    bool enable_competitive_scaling = true;
    
    // Transfers and integration
    bool enable_episodic_to_semantic = true;
    bool enable_working_to_procedural = true;
    bool enable_cross_modal_integration = true;
    float integration_threshold = 0.7f;
    
    // Sleep phases
    bool enable_slow_wave_sleep = true;
    bool enable_rem_sleep = true;
    float slow_wave_duration_ratio = 0.7f;
    float rem_duration_ratio = 0.3f;
    
    // Timing
    std::uint64_t min_consolidation_duration_ms = 5000;   // 5s
    std::uint64_t max_consolidation_duration_ms = 30000;  // 30s
    std::uint64_t consolidation_interval_ms = 60000;      // 60s between sessions
};

// Backward-compatible alias used by tests and integrator
using SleepConfig = ConsolidationConfig;

// Sleep phases available across the namespace
enum class SleepPhase { Awake, SlowWave, REM };

// Statistics structure
struct Statistics {
    std::uint64_t total_consolidation_sessions = 0;
    std::uint64_t total_episodes_replayed = 0;
    std::uint64_t total_synaptic_scaling_operations = 0;
    std::uint64_t total_memory_integrations = 0;
    std::uint64_t slow_wave_sleep_time_ms = 0;
    std::uint64_t rem_sleep_time_ms = 0;
    std::uint64_t last_consolidation_time_ms = 0;
    SleepPhase current_sleep_phase = SleepPhase::Awake;
    bool consolidation_active = false;
    bool all_memory_systems_registered = false;
    float average_consolidation_duration_ms = 0.0f;
    float consolidation_efficiency = 0.0f;
    std::uint64_t total_cycles = 0; // compat with tests
};

class SleepConsolidation {
public:
    explicit SleepConsolidation(const ConsolidationConfig& config = ConsolidationConfig{});
    ~SleepConsolidation() = default;

    // Backward-compatible nested alias so tests using SleepConsolidation::SleepPhase still compile
    using SleepPhase = ::NeuroForge::Memory::SleepPhase;

   // Registration of subsystems
   void registerEpisodicMemory(EpisodicMemoryManager* episodic_memory);
    void setEpisodicMemory(EpisodicMemoryManager* episodic_memory) { registerEpisodicMemory(episodic_memory); }
   void registerSemanticMemory(SemanticMemory* semantic_memory);
    void setSemanticMemory(SemanticMemory* semantic_memory) { registerSemanticMemory(semantic_memory); }
   void registerWorkingMemory(WorkingMemory* working_memory);
   void registerProceduralMemory(ProceduralMemory* procedural_memory);
   void registerLearningSystem(NeuroForge::Core::LearningSystem* learning_system);
   void registerBrain(NeuroForge::Core::HypergraphBrain* brain);

   // Control
   bool triggerConsolidation(bool force_consolidation = false, std::uint64_t duration_ms = 0);
    bool startSleepCycle(std::uint64_t duration_ms = 0) { return triggerConsolidation(false, duration_ms); }
   bool shouldConsolidate() const;
   bool stopConsolidation();
   bool isConsolidationActive() const;
   SleepPhase getCurrentSleepPhase() const;
    SleepPhase getCurrentPhase() const { return getCurrentSleepPhase(); }

    // Replay
    std::size_t replayEpisodes(const std::vector<EnhancedEpisode>& episodes, float replay_speed = -1.0f);
    std::vector<EnhancedEpisode> selectEpisodesForReplay(std::size_t max_episodes) const;

    // Synaptic scaling
    std::size_t performHomeostaticScaling(float scaling_factor = -1.0f);
    std::size_t performCompetitiveScaling(float competition_strength = 0.8f);
    std::size_t applySynapticScalingToRegion(const std::string& region_name, float scaling_factor);

    // Transfers & integration
    std::size_t transferEpisodicToSemantic(std::size_t max_transfers = 10);
    std::size_t transferWorkingToProcedural(std::size_t max_transfers = 5);
    std::size_t performCrossModalIntegration(float integration_threshold = -1.0f);
    std::size_t consolidateAcrossMemorySystems();

    // Sleep phase management
    bool enterSlowWaveSleep(std::uint64_t duration_ms = 0);
    bool enterREMSleep(std::uint64_t duration_ms = 0);
    bool returnToAwake();

    // Stats & config
    Statistics getStatistics() const;
    void setConfig(const ConsolidationConfig& new_config);

private:
    struct ScalingFactors {
        float global_scaling = 1.0f;
        float excitatory_scaling = 1.0f;
        float inhibitory_scaling = 1.0f;
    };

    // Internals
    bool replaySingleEpisode(const EnhancedEpisode& episode, float replay_speed);
    std::size_t processSlowWaveSleep(std::uint64_t duration_ms);
    std::size_t processREMSleep(std::uint64_t duration_ms);
    std::size_t performConsolidationSession(std::uint64_t duration_ms);
    float calculateReplayPriority(const EnhancedEpisode& episode) const;
    std::vector<EnhancedEpisode> selectRandomEpisodes(const std::vector<EnhancedEpisode>& episodes, std::size_t max_episodes) const;
    std::vector<EnhancedEpisode> selectPriorityEpisodes(const std::vector<EnhancedEpisode>& episodes, std::size_t max_episodes) const;
    ScalingFactors calculateScalingFactors(const std::string& region_name) const;
    bool areAllMemorySystemsRegistered() const;
    bool isOperational() const;
    bool validateConfig(const ConsolidationConfig& config) const;
    void updateSleepPhase(std::uint64_t elapsed_time_ms);
    void logConsolidationActivity(const std::string& activity_type, const std::string& details) const;
    std::uint64_t getCurrentTimestamp() const;

private:
    // Config
    ConsolidationConfig config_{};

    // Subsystems
    EpisodicMemoryManager* episodic_memory_ = nullptr;
    SemanticMemory* semantic_memory_ = nullptr;
    WorkingMemory* working_memory_ = nullptr;
    ProceduralMemory* procedural_memory_ = nullptr;
    NeuroForge::Core::LearningSystem* learning_system_ = nullptr;
    NeuroForge::Core::HypergraphBrain* brain_ = nullptr;

    // State
    std::atomic<bool> consolidation_active_{false};
    std::atomic<SleepPhase> current_sleep_phase_{SleepPhase::Awake};
    mutable std::mutex consolidation_mutex_;
    std::thread consolidation_thread_;

    // Statistics
    std::atomic<std::uint64_t> total_consolidation_sessions_{0};
    std::atomic<std::uint64_t> total_episodes_replayed_{0};
    std::atomic<std::uint64_t> total_synaptic_scaling_operations_{0};
    std::atomic<std::uint64_t> total_memory_integrations_{0};
    std::atomic<std::uint64_t> slow_wave_sleep_time_ms_{0};
    std::atomic<std::uint64_t> rem_sleep_time_ms_{0};
    std::atomic<std::uint64_t> last_consolidation_time_{0};
    std::atomic<std::uint64_t> consolidation_start_time_{0};

    // RNG
    mutable std::mt19937 random_generator_;
};

} // namespace Memory
} // namespace NeuroForge