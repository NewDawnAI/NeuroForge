#pragma once

#include "core/Types.h"
#include "memory/WorkingMemory.h"
#include "memory/ProceduralMemory.h"
#include "memory/EpisodicMemoryManager.h"
#include "memory/SemanticMemory.h"
#include "memory/DevelopmentalConstraints.h"
#include "memory/SleepConsolidation.h"
#include <chrono>
#include <mutex>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

namespace NeuroForge {
namespace Memory {

/**
 * @brief Memory integration event
 */
struct MemoryEvent {
    std::string event_type;
    std::uint64_t source_memory_id;
    std::uint64_t target_memory_id;
    float integration_strength = 1.0f;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief Cross-memory system query
 */
struct CrossMemoryQuery {
    std::string query_text;
    std::vector<float> query_embedding;
    std::vector<std::string> target_systems; // "episodic", "semantic", "procedural"
    std::size_t max_results = 10;
    float relevance_threshold = 0.1f;
};

/**
 * @brief Integrated memory result
 */
struct IntegratedMemoryResult {
    std::string memory_type;
    std::uint64_t memory_id;
    float relevance_score = 0.0f;
    std::string content_summary;
    std::vector<float> embedding;
};

/**
 * @brief Memory integration configuration
 */
struct IntegrationConfig {
    bool enable_cross_system_links = true;
    bool enable_automatic_consolidation = true;
    float integration_threshold = 0.5f;
    std::size_t max_cross_links = 1000;
    float decay_rate = 0.01f;
    // Phase 2 system toggles
    bool enable_working_memory = true;
    bool enable_procedural_memory = true;
    bool enable_episodic_memory = true;
    bool enable_semantic_memory = true;
    bool enable_developmental_constraints = true;
    bool enable_sleep_consolidation = true;
};

/**
 * @brief Statistics for memory integration
 */
struct IntegrationStatistics {
    std::size_t total_integrations = 0;
    std::size_t episodic_semantic_links = 0;
    std::size_t semantic_procedural_links = 0;
    std::size_t cross_system_queries = 0;
    float average_integration_strength = 0.0f;
};

/**
 * @brief Integrates multiple memory systems for unified memory access
 */
class MemoryIntegrator {
public:
    using Config = IntegrationConfig;
    explicit MemoryIntegrator(const IntegrationConfig& config = IntegrationConfig{});
    ~MemoryIntegrator() = default;

    // Memory system getters
    WorkingMemory& getWorkingMemory();
    ProceduralMemory& getProceduralMemory();
    EpisodicMemoryManager& getEpisodicMemory();
    SemanticMemory& getSemanticMemory();
    DevelopmentalConstraints& getDevelopmentalConstraints();
    SleepConsolidation& getSleepConsolidation();

    // Memory system registration
    void setEpisodicMemory(std::shared_ptr<EpisodicMemoryManager> episodic);
    void setSemanticMemory(std::shared_ptr<SemanticMemory> semantic);
    void setDevelopmentalConstraints(std::shared_ptr<DevelopmentalConstraints> constraints);
    void setSleepConsolidation(std::shared_ptr<SleepConsolidation> sleep);
    
    // Cross-system operations
    std::vector<IntegratedMemoryResult> queryAllSystems(const CrossMemoryQuery& query);
    void createCrossSystemLink(const std::string& source_system, std::uint64_t source_id,
                              const std::string& target_system, std::uint64_t target_id,
                              float strength = 1.0f);
    
    // Memory formation with integration
    std::uint64_t storeIntegratedMemory(const std::string& content,
                                       const std::vector<float>& sensory_data,
                                       const std::vector<float>& emotional_state,
                                       const std::string& context = "");
    
    // Consolidation and maintenance
    void performCrossSystemConsolidation();
    void strengthenFrequentlyAccessedLinks();
    void pruneWeakLinks();
    void updateMemoryRelevance();
    
    // Retrieval with context
    std::vector<IntegratedMemoryResult> retrieveWithContext(const std::string& query,
                                                           const std::string& context = "",
                                                           std::size_t max_results = 10);

    // Retrieval of relevant memories
    std::vector<std::vector<float>> retrieveRelevantMemories(const std::vector<float>& query,
                                                             float relevance_threshold = 0.5f);
    
    // Memory replay and rehearsal
    void performMemoryReplay(std::size_t num_memories = 10);
    void rehearseImportantMemories();
    
    // Developmental integration
    void applyDevelopmentalConstraints();
    bool canAccessMemoryType(const std::string& memory_type) const;

    // Operational status
    bool isOperational() const;
    
    // Statistics and monitoring
    const IntegrationStatistics& getStatistics() const { return statistics_; }
    std::size_t getTotalMemoryCount() const;
    float getSystemIntegrationLevel() const;
    
    // Configuration
    void updateConfig(const IntegrationConfig& config) { config_ = config; }
    const IntegrationConfig& getConfig() const { return config_; }

private:
    // Helper methods
    float calculateCrossSystemSimilarity(const std::string& system1, std::uint64_t id1,
                                        const std::string& system2, std::uint64_t id2) const;
    void updateIntegrationStatistics();
    std::vector<float> extractEmbedding(const std::string& system, std::uint64_t id) const;
    void processMemoryEvent(const MemoryEvent& event);

private:
    IntegrationConfig config_;
    
    // Memory system references
    std::shared_ptr<WorkingMemory> working_memory_;
    std::shared_ptr<ProceduralMemory> procedural_memory_;
    std::shared_ptr<EpisodicMemoryManager> episodic_memory_;
    std::shared_ptr<SemanticMemory> semantic_memory_;
    std::shared_ptr<DevelopmentalConstraints> developmental_constraints_;
    std::shared_ptr<SleepConsolidation> sleep_consolidation_;
    
    // Cross-system links
    std::vector<MemoryEvent> integration_events_;
    std::unordered_map<std::string, std::vector<std::uint64_t>> cross_system_links_;
    
    // Statistics
    IntegrationStatistics statistics_;
    
    // State tracking
    std::chrono::steady_clock::time_point last_consolidation_;
    std::uint64_t next_event_id_ = 1;

    // Thread-safety for integration operations
    mutable std::mutex integration_mutex_;
};

} // namespace Memory
} // namespace NeuroForge