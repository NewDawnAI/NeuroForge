#include "memory/SleepConsolidation.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <numeric>
#include <chrono>
#include <iostream>

namespace NeuroForge {
namespace Memory {

// SleepConsolidation Implementation

SleepConsolidation::SleepConsolidation(const ConsolidationConfig& config)
    : config_(config)
    , random_generator_(std::chrono::steady_clock::now().time_since_epoch().count()) {
    
    // Initialize with current timestamp
    last_consolidation_time_.store(getCurrentTimestamp());
}

void SleepConsolidation::registerEpisodicMemory(EpisodicMemoryManager* episodic_memory) {
    std::lock_guard<std::mutex> lock(consolidation_mutex_);
    episodic_memory_ = episodic_memory;
}

void SleepConsolidation::registerSemanticMemory(SemanticMemory* semantic_memory) {
    std::lock_guard<std::mutex> lock(consolidation_mutex_);
    semantic_memory_ = semantic_memory;
}

void SleepConsolidation::registerWorkingMemory(WorkingMemory* working_memory) {
    std::lock_guard<std::mutex> lock(consolidation_mutex_);
    working_memory_ = working_memory;
}

void SleepConsolidation::registerProceduralMemory(ProceduralMemory* procedural_memory) {
    std::lock_guard<std::mutex> lock(consolidation_mutex_);
    procedural_memory_ = procedural_memory;
}

void SleepConsolidation::registerLearningSystem(NeuroForge::Core::LearningSystem* learning_system) {
    std::lock_guard<std::mutex> lock(consolidation_mutex_);
    learning_system_ = learning_system;
}

void SleepConsolidation::registerBrain(NeuroForge::Core::HypergraphBrain* brain) {
    std::lock_guard<std::mutex> lock(consolidation_mutex_);
    brain_ = brain;
}

bool SleepConsolidation::triggerConsolidation(bool force_consolidation, std::uint64_t duration_ms) {
    if (!force_consolidation && !shouldConsolidate()) {
        return false;
    }
    
    if (consolidation_active_.load()) {
        return false;  // Already running
    }
    
    // Check memory systems without locking to avoid deadlock
    bool systems_registered = episodic_memory_ != nullptr && 
                             semantic_memory_ != nullptr && 
                             working_memory_ != nullptr && 
                             procedural_memory_ != nullptr && 
                             learning_system_ != nullptr && 
                             brain_ != nullptr;
    
    if (!systems_registered) {
        return false;  // Not all systems available
    }
    
    std::lock_guard<std::mutex> lock(consolidation_mutex_);
    
    // Set consolidation as active
    consolidation_active_.store(true);
    consolidation_start_time_.store(getCurrentTimestamp());
    
    // Determine consolidation duration
    if (duration_ms == 0) {
        std::uniform_int_distribution<std::uint64_t> duration_dist(
            config_.min_consolidation_duration_ms,
            config_.max_consolidation_duration_ms);
        duration_ms = duration_dist(random_generator_);
    }
    
    logConsolidationActivity("Consolidation Started", 
                           "Duration: " + std::to_string(duration_ms) + "ms");
    
    // Perform consolidation session
    size_t operations_performed = performConsolidationSession(duration_ms);
    
    // Update statistics
    total_consolidation_sessions_.fetch_add(1);
    last_consolidation_time_.store(getCurrentTimestamp());
    
    // Set consolidation as inactive
    consolidation_active_.store(false);
    
    logConsolidationActivity("Consolidation Completed", 
                           "Operations: " + std::to_string(operations_performed));
    
    return true;
}

bool SleepConsolidation::shouldConsolidate() const {
    auto current_time = getCurrentTimestamp();
    auto time_since_last = current_time - last_consolidation_time_.load();
    return time_since_last >= config_.consolidation_interval_ms;
}

bool SleepConsolidation::stopConsolidation() {
    if (!consolidation_active_.load()) {
        return false;
    }
    
    consolidation_active_.store(false);
    current_sleep_phase_.store(SleepPhase::Awake);
    
    logConsolidationActivity("Consolidation Stopped", "Manual stop requested");
    
    return true;
}

bool SleepConsolidation::isConsolidationActive() const {
    return consolidation_active_.load();
}

SleepPhase SleepConsolidation::getCurrentSleepPhase() const {
    return current_sleep_phase_.load();
}

size_t SleepConsolidation::replayEpisodes(const std::vector<EnhancedEpisode>& episodes,
                                        float replay_speed) {
    
    if (episodes.empty() || !learning_system_ || !brain_) {
        return 0;
    }
    
    if (replay_speed < 0.0f) {
        replay_speed = config_.replay_speed_multiplier;
    }
    
    size_t replayed_count = 0;
    
    for (const auto& episode : episodes) {
        if (!consolidation_active_.load()) {
            break;  // Consolidation was stopped
        }
        
        if (replaySingleEpisode(episode, replay_speed)) {
            ++replayed_count;
        }
    }
    
    total_episodes_replayed_.fetch_add(replayed_count);
    
    return replayed_count;
}

std::vector<EnhancedEpisode> SleepConsolidation::selectEpisodesForReplay(size_t max_episodes) const {
    if (!episodic_memory_) {
        return {};
    }
    
    // Get recent episodes from episodic memory
    // Note: This is a simplified implementation
    // In a full implementation, we'd need access to actual episodes
    
    std::vector<EnhancedEpisode> selected_episodes;
    
    // For now, return empty vector as we don't have direct access to episodes
    // In a full implementation, this would:
    // 1. Get episodes from episodic memory
    // 2. Apply selection criteria (random vs priority)
    // 3. Return selected episodes
    
    return selected_episodes;
}

bool SleepConsolidation::replaySingleEpisode(const EnhancedEpisode& episode, float replay_speed) {
    if (!learning_system_ || !brain_) {
        return false;
    }
    
    try {
        // Simulate episode replay by feeding the episode data back through the system
        // This is a simplified implementation
        
        // 1. Feed sensory state back to appropriate regions
        if (!episode.sensory_state.empty()) {
            // In a full implementation, we'd feed this to visual/audio regions
            // For now, we'll just simulate the process
        }
        
        // 2. Feed action state back to motor regions
        if (!episode.action_state.empty()) {
            // In a full implementation, we'd feed this to motor regions
        }
        
        // 3. Apply learning with replay-specific parameters
        // Replay typically uses reduced learning rates
        float replay_learning_rate = 0.1f / replay_speed;  // Slower learning during replay
        
        // 4. Update consolidation strength of the episode
        // This would be done in the episodic memory system
        
        return true;
        
    } catch (const std::exception& e) {
        logConsolidationActivity("Replay Error", e.what());
        return false;
    }
}

size_t SleepConsolidation::performHomeostaticScaling(float scaling_factor) {
    if (!learning_system_) {
        return 0;
    }
    
    if (scaling_factor < 0.0f) {
        scaling_factor = config_.synaptic_scaling_factor;
    }
    
    size_t scaled_synapses = 0;
    
    try {
        // Apply homeostatic scaling to maintain overall activity levels
        // This is a simplified implementation
        
        // In a full implementation, this would:
        // 1. Calculate average synaptic strengths
        // 2. Apply multiplicative scaling to all synapses
        // 3. Maintain relative strength differences
        // 4. Ensure synapses stay within bounds
        
        // For now, we'll just simulate the process
        scaled_synapses = 1000;  // Simulated number of scaled synapses
        
        total_synaptic_scaling_operations_.fetch_add(1);
        
        logConsolidationActivity("Homeostatic Scaling", 
                               "Factor: " + std::to_string(scaling_factor) + 
                               ", Synapses: " + std::to_string(scaled_synapses));
        
    } catch (const std::exception& e) {
        logConsolidationActivity("Scaling Error", e.what());
    }
    
    return scaled_synapses;
}

size_t SleepConsolidation::performCompetitiveScaling(float competition_strength) {
    if (!learning_system_) {
        return 0;
    }
    
    size_t affected_synapses = 0;
    
    try {
        // Apply competitive scaling to enhance strong connections and weaken weak ones
        // This is a simplified implementation
        
        // In a full implementation, this would:
        // 1. Identify strong and weak synapses
        // 2. Strengthen strong synapses
        // 3. Weaken weak synapses
        // 4. Maintain overall activity balance
        
        // For now, we'll just simulate the process
        affected_synapses = 500;  // Simulated number of affected synapses
        
        total_synaptic_scaling_operations_.fetch_add(1);
        
        logConsolidationActivity("Competitive Scaling", 
                               "Strength: " + std::to_string(competition_strength) + 
                               ", Synapses: " + std::to_string(affected_synapses));
        
    } catch (const std::exception& e) {
        logConsolidationActivity("Competitive Scaling Error", e.what());
    }
    
    return affected_synapses;
}

size_t SleepConsolidation::applySynapticScalingToRegion(const std::string& region_name,
                                                      float scaling_factor) {
    
    if (!brain_ || region_name.empty()) {
        return 0;
    }
    
    size_t scaled_synapses = 0;
    
    try {
        // Apply scaling to specific region
        // This is a simplified implementation
        
        // In a full implementation, this would:
        // 1. Get region by name from brain
        // 2. Apply scaling to all synapses in region
        // 3. Update synapse weights
        // 4. Maintain region-specific constraints
        
        // For now, we'll just simulate the process
        scaled_synapses = 200;  // Simulated number of scaled synapses
        
        logConsolidationActivity("Region Scaling", 
                               "Region: " + region_name + 
                               ", Factor: " + std::to_string(scaling_factor) + 
                               ", Synapses: " + std::to_string(scaled_synapses));
        
    } catch (const std::exception& e) {
        logConsolidationActivity("Region Scaling Error", e.what());
    }
    
    return scaled_synapses;
}

size_t SleepConsolidation::transferEpisodicToSemantic(size_t max_transfers) {
    if (!episodic_memory_ || !semantic_memory_) {
        return 0;
    }
    
    size_t transfers_performed = 0;
    
    try {
        // Transfer consolidated episodic memories to semantic memory
        // This is a simplified implementation
        
        // In a full implementation, this would:
        // 1. Get consolidated episodes from episodic memory
        // 2. Extract concepts from episodes
        // 3. Create or update concepts in semantic memory
        // 4. Link related concepts
        // 5. Update consolidation status
        
        // For now, we'll just simulate the process
        transfers_performed = std::min(max_transfers, size_t(5));  // Simulated transfers
        
        total_memory_integrations_.fetch_add(transfers_performed);
        
        logConsolidationActivity("Episodic to Semantic Transfer", 
                               "Transfers: " + std::to_string(transfers_performed));
        
    } catch (const std::exception& e) {
        logConsolidationActivity("Transfer Error", e.what());
    }
    
    return transfers_performed;
}

size_t SleepConsolidation::transferWorkingToProcedural(size_t max_transfers) {
    if (!working_memory_ || !procedural_memory_) {
        return 0;
    }
    
    size_t transfers_performed = 0;
    
    try {
        // Transfer repeated patterns from working memory to procedural memory
        // This is a simplified implementation
        
        // In a full implementation, this would:
        // 1. Identify repeated patterns in working memory
        // 2. Extract action sequences
        // 3. Create skills in procedural memory
        // 4. Set appropriate confidence levels
        // 5. Clear transferred patterns from working memory
        
        // For now, we'll just simulate the process
        transfers_performed = std::min(max_transfers, size_t(3));  // Simulated transfers
        
        total_memory_integrations_.fetch_add(transfers_performed);
        
        logConsolidationActivity("Working to Procedural Transfer", 
                               "Transfers: " + std::to_string(transfers_performed));
        
    } catch (const std::exception& e) {
        logConsolidationActivity("Working Transfer Error", e.what());
    }
    
    return transfers_performed;
}

size_t SleepConsolidation::performCrossModalIntegration(float integration_threshold) {
    if (integration_threshold < 0.0f) {
        integration_threshold = config_.integration_threshold;
    }
    
    size_t integrations_performed = 0;
    
    try {
        // Integrate memories across different modalities
        // This is a simplified implementation
        
        // In a full implementation, this would:
        // 1. Find memories with similar temporal patterns
        // 2. Identify cross-modal correlations
        // 3. Create integrated memory representations
        // 4. Update memory systems with integrated memories
        // 5. Strengthen cross-modal connections
        
        // For now, we'll just simulate the process
        integrations_performed = 2;  // Simulated integrations
        
        total_memory_integrations_.fetch_add(integrations_performed);
        
        logConsolidationActivity("Cross-Modal Integration", 
                               "Integrations: " + std::to_string(integrations_performed) + 
                               ", Threshold: " + std::to_string(integration_threshold));
        
    } catch (const std::exception& e) {
        logConsolidationActivity("Cross-Modal Integration Error", e.what());
    }
    
    return integrations_performed;
}

size_t SleepConsolidation::consolidateAcrossMemorySystems() {
    size_t total_operations = 0;
    
    // Perform various consolidation operations
    if (config_.enable_episodic_to_semantic) {
        total_operations += transferEpisodicToSemantic(10);
    }
    
    if (config_.enable_working_to_procedural) {
        total_operations += transferWorkingToProcedural(5);
    }
    
    if (config_.enable_cross_modal_integration) {
        total_operations += performCrossModalIntegration();
    }
    
    // Apply synaptic scaling
    if (config_.enable_homeostatic_scaling) {
        total_operations += performHomeostaticScaling();
    }
    
    if (config_.enable_competitive_scaling) {
        total_operations += performCompetitiveScaling();
    }
    
    return total_operations;
}

bool SleepConsolidation::enterSlowWaveSleep(std::uint64_t duration_ms) {
    if (current_sleep_phase_.load() != SleepPhase::Awake) {
        return false;  // Already in sleep phase
    }
    
    current_sleep_phase_.store(SleepPhase::SlowWave);
    
    if (duration_ms > 0) {
        processSlowWaveSleep(duration_ms);
    }
    
    logConsolidationActivity("Entered Slow-Wave Sleep", 
                           "Duration: " + std::to_string(duration_ms) + "ms");
    
    return true;
}

bool SleepConsolidation::enterREMSleep(std::uint64_t duration_ms) {
    current_sleep_phase_.store(SleepPhase::REM);
    
    if (duration_ms > 0) {
        processREMSleep(duration_ms);
    }
    
    logConsolidationActivity("Entered REM Sleep", 
                           "Duration: " + std::to_string(duration_ms) + "ms");
    
    return true;
}

bool SleepConsolidation::returnToAwake() {
    current_sleep_phase_.store(SleepPhase::Awake);
    
    logConsolidationActivity("Returned to Awake", "");
    
    return true;
}

size_t SleepConsolidation::processSlowWaveSleep(std::uint64_t duration_ms) {
    size_t operations_performed = 0;
    
    // Slow-wave sleep focuses on memory consolidation and synaptic scaling
    if (config_.enable_homeostatic_scaling) {
        operations_performed += performHomeostaticScaling();
    }
    
    // Replay important episodes
    auto episodes_to_replay = selectEpisodesForReplay(config_.max_replay_episodes / 2);
    operations_performed += replayEpisodes(episodes_to_replay, config_.replay_speed_multiplier * 0.5f);
    
    // Transfer episodic to semantic
    if (config_.enable_episodic_to_semantic) {
        operations_performed += transferEpisodicToSemantic(5);
    }
    
    slow_wave_sleep_time_ms_.fetch_add(duration_ms);
    
    return operations_performed;
}

size_t SleepConsolidation::processREMSleep(std::uint64_t duration_ms) {
    size_t operations_performed = 0;
    
    // REM sleep focuses on memory integration and creativity
    if (config_.enable_cross_modal_integration) {
        operations_performed += performCrossModalIntegration();
    }
    
    // Random replay for integration and creativity
    auto episodes_to_replay = selectEpisodesForReplay(config_.max_replay_episodes / 4);
    operations_performed += replayEpisodes(episodes_to_replay, config_.replay_speed_multiplier * 2.0f);
    
    // Transfer working to procedural
    if (config_.enable_working_to_procedural) {
        operations_performed += transferWorkingToProcedural(3);
    }
    
    rem_sleep_time_ms_.fetch_add(duration_ms);
    
    return operations_performed;
}

Statistics SleepConsolidation::getStatistics() const {
    std::lock_guard<std::mutex> lock(consolidation_mutex_);
    
    Statistics stats;
    stats.total_consolidation_sessions = total_consolidation_sessions_.load();
    stats.total_episodes_replayed = total_episodes_replayed_.load();
    stats.total_synaptic_scaling_operations = total_synaptic_scaling_operations_.load();
    stats.total_memory_integrations = total_memory_integrations_.load();
    stats.slow_wave_sleep_time_ms = slow_wave_sleep_time_ms_.load();
    stats.rem_sleep_time_ms = rem_sleep_time_ms_.load();
    stats.last_consolidation_time_ms = last_consolidation_time_.load();
    stats.current_sleep_phase = current_sleep_phase_.load();
    stats.consolidation_active = consolidation_active_.load();
    stats.all_memory_systems_registered = areAllMemorySystemsRegistered();
    
    // Calculate averages
    if (stats.total_consolidation_sessions > 0) {
        // Simplified average calculation
        stats.average_consolidation_duration_ms = 
            static_cast<float>(config_.min_consolidation_duration_ms + config_.max_consolidation_duration_ms) / 2.0f;
        
        // Calculate efficiency based on operations performed
        float total_operations = static_cast<float>(
            stats.total_episodes_replayed + 
            stats.total_synaptic_scaling_operations + 
            stats.total_memory_integrations);
        
        stats.consolidation_efficiency = total_operations / stats.total_consolidation_sessions;
    }
    
    return stats;
}

void SleepConsolidation::setConfig(const ConsolidationConfig& new_config) {
    if (!validateConfig(new_config)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(consolidation_mutex_);
    config_ = new_config;
}

bool SleepConsolidation::areAllMemorySystemsRegistered() const {
    // Use atomic reads to avoid deadlock - these pointers are only set during registration
    return episodic_memory_ != nullptr && 
           semantic_memory_ != nullptr && 
           working_memory_ != nullptr && 
           procedural_memory_ != nullptr && 
           learning_system_ != nullptr && 
           brain_ != nullptr;
}

bool SleepConsolidation::isOperational() const {
    return areAllMemorySystemsRegistered() && validateConfig(config_);
}

// Private Methods

size_t SleepConsolidation::performConsolidationSession(std::uint64_t duration_ms) {
    size_t total_operations = 0;
    
    // Determine sleep phases based on configuration
    std::uint64_t slow_wave_duration = static_cast<std::uint64_t>(
        duration_ms * config_.slow_wave_duration_ratio);
    std::uint64_t rem_duration = static_cast<std::uint64_t>(
        duration_ms * config_.rem_duration_ratio);
    
    // Process slow-wave sleep phase
    if (config_.enable_slow_wave_sleep && slow_wave_duration > 0) {
        enterSlowWaveSleep(0);
        total_operations += processSlowWaveSleep(slow_wave_duration);
    }
    
    // Process REM sleep phase
    if (config_.enable_rem_sleep && rem_duration > 0) {
        enterREMSleep(0);
        total_operations += processREMSleep(rem_duration);
    }
    
    // Return to awake state
    returnToAwake();
    
    return total_operations;
}

float SleepConsolidation::calculateReplayPriority(const EnhancedEpisode& episode) const {
    float priority = 0.0f;
    
    // Emotional weight contributes to priority
    priority += episode.emotional_weight * 0.3f;
    
    // Novelty contributes to priority
    priority += episode.novelty_metrics.surprise_level * 0.2f;
    priority += episode.novelty_metrics.prediction_error * 0.2f;
    
    // Consolidation strength contributes
    priority += episode.consolidation_strength * 0.2f;
    
    // Recent episodes get slight priority boost
    std::uint64_t age = episode.getAge();
    float recency_factor = std::exp(-static_cast<float>(age) / 3600000.0f);  // 1 hour decay
    priority += recency_factor * 0.1f;
    
    return std::min(1.0f, priority);
}

std::vector<EnhancedEpisode> SleepConsolidation::selectRandomEpisodes(
    const std::vector<EnhancedEpisode>& episodes,
    size_t max_episodes) const {
    
    std::vector<EnhancedEpisode> selected;
    
    if (episodes.empty()) {
        return selected;
    }
    
    // Create indices and shuffle them
    std::vector<size_t> indices(episodes.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), random_generator_);
    
    // Select up to max_episodes
    size_t num_to_select = std::min(max_episodes, episodes.size());
    for (size_t i = 0; i < num_to_select; ++i) {
        selected.push_back(episodes[indices[i]]);
    }
    
    return selected;
}

std::vector<EnhancedEpisode> SleepConsolidation::selectPriorityEpisodes(
    const std::vector<EnhancedEpisode>& episodes,
    size_t max_episodes) const {
    
    std::vector<std::pair<float, size_t>> priorities;
    
    // Calculate priorities for all episodes
    for (size_t i = 0; i < episodes.size(); ++i) {
        float priority = calculateReplayPriority(episodes[i]);
        priorities.emplace_back(priority, i);
    }
    
    // Sort by priority (descending)
    std::sort(priorities.begin(), priorities.end(),
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    // Select top episodes
    std::vector<EnhancedEpisode> selected;
    size_t num_to_select = std::min(max_episodes, episodes.size());
    
    for (size_t i = 0; i < num_to_select; ++i) {
        selected.push_back(episodes[priorities[i].second]);
    }
    
    return selected;
}

SleepConsolidation::ScalingFactors SleepConsolidation::calculateScalingFactors(
    const std::string& region_name) const {
    
    ScalingFactors factors;
    
    // Calculate region-specific scaling factors
    // This is a simplified implementation
    
    factors.global_scaling = config_.synaptic_scaling_factor;
    factors.excitatory_scaling = factors.global_scaling;
    factors.inhibitory_scaling = factors.global_scaling * 1.1f;  // Slightly stronger for inhibitory
    
    return factors;
}

std::uint64_t SleepConsolidation::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool SleepConsolidation::validateConfig(const ConsolidationConfig& config) const {
    // Basic validation
    return config.replay_speed_multiplier > 0.0f &&
           config.synaptic_scaling_factor >= 0.0f && config.synaptic_scaling_factor <= 1.0f &&
           config.integration_threshold >= 0.0f && config.integration_threshold <= 1.0f &&
           config.slow_wave_duration_ratio >= 0.0f && config.slow_wave_duration_ratio <= 1.0f &&
           config.rem_duration_ratio >= 0.0f && config.rem_duration_ratio <= 1.0f &&
           (config.slow_wave_duration_ratio + config.rem_duration_ratio) <= 1.0f &&
           config.min_consolidation_duration_ms <= config.max_consolidation_duration_ms;
}

void SleepConsolidation::updateSleepPhase(std::uint64_t elapsed_time_ms) {
    // Simple phase management based on elapsed time
    // In a full implementation, this would be more sophisticated
    
    SleepPhase current_phase = current_sleep_phase_.load();
    
    // Transition logic would go here
    // For now, we'll keep it simple
}

void SleepConsolidation::logConsolidationActivity(const std::string& activity_type,
                                                 const std::string& details) const {
    
    // Simple logging - in a full implementation, this would use a proper logging system
    #ifdef DEBUG_CONSOLIDATION
    std::cout << "[SleepConsolidation] " << activity_type;
    if (!details.empty()) {
        std::cout << " - " << details;
    }
    std::cout << std::endl;
    #endif
}

} // namespace Memory
} // namespace NeuroForge