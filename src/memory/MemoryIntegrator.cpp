#include "memory/MemoryIntegrator.h"
#include <algorithm>
#include <stdexcept>
#include <cmath>

namespace NeuroForge {
namespace Memory {

MemoryIntegrator::MemoryIntegrator(const IntegrationConfig& config)
    : config_(config) {
    last_consolidation_ = std::chrono::steady_clock::now();

    // Initialize enabled core systems
    if (config_.enable_working_memory) {
        WorkingMemoryConfig wm_cfg{};
        working_memory_ = std::make_shared<WorkingMemory>(wm_cfg);
    }
    if (config_.enable_procedural_memory) {
        ProceduralConfig pm_cfg{};
        procedural_memory_ = std::make_shared<ProceduralMemory>(pm_cfg);
    }
    if (config_.enable_episodic_memory) {
        EpisodicConfig ep_cfg{};
        episodic_memory_ = std::make_shared<EpisodicMemoryManager>(ep_cfg);
    }
    if (config_.enable_semantic_memory) {
        SemanticMemory::Config sm_cfg{};
        semantic_memory_ = std::make_shared<SemanticMemory>(sm_cfg);
    }
    if (config_.enable_developmental_constraints) {
        DevelopmentalConstraints::Config dc_cfg{};
        developmental_constraints_ = std::make_shared<DevelopmentalConstraints>(dc_cfg);
    }
    if (config_.enable_sleep_consolidation) {
        SleepConfig sc_cfg{};
        sleep_consolidation_ = std::make_shared<SleepConsolidation>(sc_cfg);
        // If other systems are already registered, wire them
        if (episodic_memory_) sleep_consolidation_->setEpisodicMemory(episodic_memory_.get());
        if (semantic_memory_) sleep_consolidation_->setSemanticMemory(semantic_memory_.get());
    }
}

WorkingMemory& MemoryIntegrator::getWorkingMemory() {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    if (!working_memory_) throw std::runtime_error("Working memory not initialized");
    return *working_memory_;
}

ProceduralMemory& MemoryIntegrator::getProceduralMemory() {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    if (!procedural_memory_) throw std::runtime_error("Procedural memory not initialized");
    return *procedural_memory_;
}

EpisodicMemoryManager& MemoryIntegrator::getEpisodicMemory() {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    if (!episodic_memory_) throw std::runtime_error("Episodic memory not initialized");
    return *episodic_memory_;
}

SemanticMemory& MemoryIntegrator::getSemanticMemory() {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    if (!semantic_memory_) throw std::runtime_error("Semantic memory not initialized");
    return *semantic_memory_;
}

DevelopmentalConstraints& MemoryIntegrator::getDevelopmentalConstraints() {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    if (!developmental_constraints_) throw std::runtime_error("Developmental constraints not initialized");
    return *developmental_constraints_;
}

SleepConsolidation& MemoryIntegrator::getSleepConsolidation() {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    if (!sleep_consolidation_) throw std::runtime_error("Sleep consolidation not initialized");
    return *sleep_consolidation_;
}

void MemoryIntegrator::setEpisodicMemory(std::shared_ptr<EpisodicMemoryManager> episodic) {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    episodic_memory_ = std::move(episodic);
    if (sleep_consolidation_ && episodic_memory_) {
        sleep_consolidation_->setEpisodicMemory(episodic_memory_.get());
    }
}

void MemoryIntegrator::setSemanticMemory(std::shared_ptr<SemanticMemory> semantic) {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    semantic_memory_ = std::move(semantic);
    if (sleep_consolidation_ && semantic_memory_) {
        sleep_consolidation_->setSemanticMemory(semantic_memory_.get());
    }
}

void MemoryIntegrator::setDevelopmentalConstraints(std::shared_ptr<DevelopmentalConstraints> constraints) {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    developmental_constraints_ = std::move(constraints);
}

void MemoryIntegrator::setSleepConsolidation(std::shared_ptr<SleepConsolidation> sleep) {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    sleep_consolidation_ = std::move(sleep);
    if (sleep_consolidation_) {
        if (episodic_memory_) sleep_consolidation_->setEpisodicMemory(episodic_memory_.get());
        if (semantic_memory_) sleep_consolidation_->setSemanticMemory(semantic_memory_.get());
    }
}

std::vector<IntegratedMemoryResult> MemoryIntegrator::queryAllSystems(const CrossMemoryQuery& query) {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    statistics_.cross_system_queries++;
    std::vector<IntegratedMemoryResult> results;
    // Minimal placeholder: returns empty or basic stubs to satisfy API
    return results;
}

void MemoryIntegrator::createCrossSystemLink(const std::string& source_system, std::uint64_t source_id,
                                             const std::string& target_system, std::uint64_t target_id,
                                             float strength) {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    if (!config_.enable_cross_system_links) return;
    cross_system_links_[source_system].push_back(target_id);
    MemoryEvent evt{ "link", source_id, target_id, strength, std::chrono::steady_clock::now() };
    integration_events_.push_back(evt);
    statistics_.total_integrations++;
    updateIntegrationStatistics();
}

std::uint64_t MemoryIntegrator::storeIntegratedMemory(const std::string& content,
                                                     const std::vector<float>& sensory_data,
                                                     const std::vector<float>& emotional_state,
                                                     const std::string& context) {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    statistics_.total_integrations++;
    // Prefer semantic memory when available
    if (semantic_memory_) {
        auto id = semantic_memory_->addConcept(content, sensory_data);
        return static_cast<std::uint64_t>(id);
    }
    // Fallback to episodic memory
    if (episodic_memory_) {
        return episodic_memory_->storeEpisode(context, sensory_data, emotional_state, content);
    }
    return 0ULL;
}

void MemoryIntegrator::performCrossSystemConsolidation() {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    if (sleep_consolidation_) {
        sleep_consolidation_->startSleepCycle();
    }
}

void MemoryIntegrator::strengthenFrequentlyAccessedLinks() {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    // Placeholder: update stats only
    updateIntegrationStatistics();
}

void MemoryIntegrator::pruneWeakLinks() {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    // Placeholder: limit stored links
    for (auto& kv : cross_system_links_) {
        if (kv.second.size() > config_.max_cross_links) {
            kv.second.resize(config_.max_cross_links);
        }
    }
}

void MemoryIntegrator::updateMemoryRelevance() {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    updateIntegrationStatistics();
}

std::vector<IntegratedMemoryResult> MemoryIntegrator::retrieveWithContext(const std::string& query,
                                                                          const std::string& context,
                                                                          std::size_t max_results) {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    std::vector<IntegratedMemoryResult> results;
    // Minimal stub; real implementation would leverage embeddings and context
    return results;
}

std::vector<std::vector<float>> MemoryIntegrator::retrieveRelevantMemories(const std::vector<float>& query,
                                                                           float relevance_threshold) {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    std::vector<std::vector<float>> results;
    if (!working_memory_) return results;

    // Try to find a similar slot
    auto slot_index = working_memory_->findSimilarSlot(query, relevance_threshold);
    if (slot_index < WorkingMemory::MILLER_CAPACITY) {
        auto content = working_memory_->getSlotContent(slot_index);
        if (!content.empty()) results.push_back(content);
    }
    // Fallback to most active content
    if (results.empty()) {
        auto active = working_memory_->getMostActiveContent();
        if (!active.empty()) results.push_back(active);
    }
    return results;
}

void MemoryIntegrator::performMemoryReplay(std::size_t num_memories) {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    // Placeholder: no-op for Phase 2 tests
}

void MemoryIntegrator::rehearseImportantMemories() {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    // Placeholder: no-op
}

void MemoryIntegrator::applyDevelopmentalConstraints() {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    // Placeholder: integration point for constraints
}

bool MemoryIntegrator::canAccessMemoryType(const std::string& memory_type) const {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    if (memory_type == "working") return config_.enable_working_memory && static_cast<bool>(working_memory_);
    if (memory_type == "procedural") return config_.enable_procedural_memory && static_cast<bool>(procedural_memory_);
    if (memory_type == "episodic") return config_.enable_episodic_memory && static_cast<bool>(episodic_memory_);
    if (memory_type == "semantic") return config_.enable_semantic_memory && static_cast<bool>(semantic_memory_);
    if (memory_type == "developmental") return config_.enable_developmental_constraints && static_cast<bool>(developmental_constraints_);
    if (memory_type == "sleep") return config_.enable_sleep_consolidation && static_cast<bool>(sleep_consolidation_);
    return false;
}

bool MemoryIntegrator::isOperational() const {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    bool ok = true;
    if (config_.enable_working_memory) ok &= static_cast<bool>(working_memory_);
    if (config_.enable_procedural_memory) ok &= static_cast<bool>(procedural_memory_);
    if (config_.enable_episodic_memory) ok &= static_cast<bool>(episodic_memory_);
    if (config_.enable_semantic_memory) ok &= static_cast<bool>(semantic_memory_);
    if (config_.enable_developmental_constraints) ok &= static_cast<bool>(developmental_constraints_);
    if (config_.enable_sleep_consolidation) ok &= static_cast<bool>(sleep_consolidation_);
    return ok;
}

std::size_t MemoryIntegrator::getTotalMemoryCount() const {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    std::size_t total = 0;
    // WorkingMemory uses getCurrentLoad() in its public API
    if (working_memory_) total += working_memory_->getCurrentLoad();
    if (procedural_memory_) total += procedural_memory_->getStatistics().total_skills;
    if (episodic_memory_) total += static_cast<std::size_t>(episodic_memory_->getEpisodeCount());
    // SemanticMemory exposes getTotalConceptCount() in its public API
    if (semantic_memory_) total += static_cast<std::size_t>(semantic_memory_->getTotalConceptCount());
    return total;
}

float MemoryIntegrator::getSystemIntegrationLevel() const {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    int enabled = 0, initialized = 0;
    if (config_.enable_working_memory) { enabled++; if (working_memory_) initialized++; }
    if (config_.enable_procedural_memory) { enabled++; if (procedural_memory_) initialized++; }
    if (config_.enable_episodic_memory) { enabled++; if (episodic_memory_) initialized++; }
    if (config_.enable_semantic_memory) { enabled++; if (semantic_memory_) initialized++; }
    if (config_.enable_developmental_constraints) { enabled++; if (developmental_constraints_) initialized++; }
    if (config_.enable_sleep_consolidation) { enabled++; if (sleep_consolidation_) initialized++; }
    if (enabled == 0) return 0.0f;
    return static_cast<float>(initialized) / static_cast<float>(enabled);
}

float MemoryIntegrator::calculateCrossSystemSimilarity(const std::string& system1, std::uint64_t id1,
                                                      const std::string& system2, std::uint64_t id2) const {
    // Placeholder similarity
    (void)system1; (void)id1; (void)system2; (void)id2;
    return 0.0f;
}

void MemoryIntegrator::updateIntegrationStatistics() {
    // Placeholder: compute averages if needed
}

std::vector<float> MemoryIntegrator::extractEmbedding(const std::string& system, std::uint64_t id) const {
    (void)system; (void)id;
    return {};
}

void MemoryIntegrator::processMemoryEvent(const MemoryEvent& event) {
    std::lock_guard<std::mutex> lock(integration_mutex_);
    integration_events_.push_back(event);
}

} // namespace Memory
} // namespace NeuroForge