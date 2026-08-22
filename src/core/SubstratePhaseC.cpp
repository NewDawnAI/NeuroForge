#include "core/SubstratePhaseC.h"
#include "biases/SurvivalBias.h"
#include <algorithm>
#include <functional>
#include <queue>
#include <optional>
#include <random>
#include <chrono>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace NeuroForge {
namespace Core {

SubstratePhaseC::SubstratePhaseC(std::shared_ptr<HypergraphBrain> brain, 
                                 std::shared_ptr<SubstrateWorkingMemory> working_memory,
                                 const Config& config)
    : config_(config), brain_(brain), working_memory_(working_memory) {
}

bool SubstratePhaseC::initialize() {
    if (!brain_ || !working_memory_) {
        return false;
    }
    
    try {
        initializeRegions();
        setupRecurrentConnections();
        initialized_.store(true);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

void SubstratePhaseC::shutdown() {
    processing_.store(false);
    initialized_.store(false);
    
    std::lock_guard<std::mutex> assemblies_lock(assemblies_mutex_);
    std::lock_guard<std::mutex> goals_lock(goals_mutex_);
    
    current_assemblies_.clear();
    assembly_lookup_.clear();
    active_goals_.clear();
}

void SubstratePhaseC::initializeRegions() {
    // Create specialized binding regions (role-filler pairs)
    binding_regions_.clear();
    for (std::size_t i = 0; i < config_.binding_regions; ++i) {
        std::string region_name = "SubstratePhaseC_Binding_" + std::to_string(i);
        auto region = brain_->createRegion(region_name, 
                                         Region::Type::Cortical,
                                         Region::ActivationPattern::Competitive);
        if (region) {
            region->createNeurons(config_.neurons_per_region);
            binding_regions_.push_back(region->getId());
        }
    }
    
    // Create sequence memory regions
    sequence_regions_.clear();
    for (std::size_t i = 0; i < config_.sequence_regions; ++i) {
        std::string region_name = "SubstratePhaseC_Sequence_" + std::to_string(i);
        auto region = brain_->createRegion(region_name,
                                         Region::Type::Cortical,
                                         Region::ActivationPattern::Asynchronous);
        if (region) {
            region->createNeurons(config_.neurons_per_region);
            sequence_regions_.push_back(region->getId());
        }
    }
    
    // Create competition region for winner-take-all dynamics
    auto comp_region = brain_->createRegion("SubstratePhaseC_Competition",
                                          Region::Type::Subcortical,
                                          Region::ActivationPattern::Competitive);
    if (comp_region) {
        comp_region->createNeurons(config_.neurons_per_region / 2);
        competition_region_ = comp_region->getId();
    }
    
    // Create goal-setting region
    auto goal_region = brain_->createRegion("SubstratePhaseC_Goals",
                                          Region::Type::Cortical,
                                          Region::ActivationPattern::Asynchronous);
    if (goal_region) {
        goal_region->createNeurons(config_.neurons_per_region / 4);
        goal_region_ = goal_region->getId();
    }
}

void SubstratePhaseC::setupRecurrentConnections() {
    // Connect binding regions with recurrent connections for binding maintenance
    for (std::size_t i = 0; i < binding_regions_.size(); ++i) {
        for (std::size_t j = i + 1; j < binding_regions_.size(); ++j) {
            // Use fixed density (20%) and use recurrent_strength for weights
            float base_weight = config_.recurrent_strength;
            brain_->connectRegions(binding_regions_[i], binding_regions_[j], 
                                 0.2f, {base_weight * 0.8f, base_weight * 1.2f});
        }
    }
    
    // Connect sequence regions with temporal adjacency connections
    for (std::size_t i = 0; i < sequence_regions_.size() - 1; ++i) {
        float base_weight = config_.recurrent_strength;
        brain_->connectRegions(sequence_regions_[i], sequence_regions_[i + 1],
                             0.2f, {base_weight * 0.8f, base_weight * 1.2f});
    }
    
    // Connect competition region to all other regions for global competition
    for (auto region_id : binding_regions_) {
        float base_weight = config_.competition_strength;
        brain_->connectRegions(competition_region_, region_id,
                             0.2f, {base_weight * 0.8f, base_weight * 1.2f});
    }
    for (auto region_id : sequence_regions_) {
        float base_weight = config_.competition_strength;
        brain_->connectRegions(competition_region_, region_id,
                             0.2f, {base_weight * 0.8f, base_weight * 1.2f});
    }
    
    // Connect goal region to all task regions for top-down control
    for (auto region_id : binding_regions_) {
        float base_weight = config_.goal_setting_strength;
        brain_->connectRegions(goal_region_, region_id,
                             0.2f, {base_weight * 0.8f, base_weight * 1.2f});
    }
    for (auto region_id : sequence_regions_) {
        float base_weight = config_.goal_setting_strength;
        brain_->connectRegions(goal_region_, region_id,
                             0.2f, {base_weight * 0.8f, base_weight * 1.2f});
    }
}

void SubstratePhaseC::setGoal(const std::string& task_type, 
                              const std::map<std::string, std::string>& parameters) {
    std::lock_guard<std::mutex> lock(goals_mutex_);
    
    SubstrateGoal goal;
    goal.task_type = task_type;
    goal.priority = 1.0f;
    goal.active = true;
    
    if (task_type == "binding") {
        goal.target_regions = binding_regions_;
        goal.target_pattern.resize(config_.neurons_per_region, 0.0f);
        
        std::size_t num_role_regions = binding_regions_.size() / 2;
        if (num_role_regions == 0) num_role_regions = 1;
        
        // Determine pair index based on role to distribute load
        std::size_t pair_idx = 0;
        
        if (parameters.find("role") != parameters.end()) {
            std::string role = parameters.at("role");
            std::size_t idx = getTokenIndex(role);
            
            // Assume tokens come in pairs (Role, Filler), so divide by 2 to pack densely
            pair_idx = (idx / 2) % num_role_regions;
            
            std::size_t region_idx = pair_idx * 2; // Even regions for Roles
            std::size_t neuron_idx = idx % config_.neurons_per_region;
            
            if (region_idx < binding_regions_.size()) {
                std::vector<float> pattern(config_.neurons_per_region, 0.0f);
                pattern[neuron_idx] = 1.0f; // Strong activation
                goal.region_patterns[binding_regions_[region_idx]] = pattern;
            }
        }
        
        if (parameters.find("filler") != parameters.end()) {
            std::string filler = parameters.at("filler");
            std::size_t idx = getTokenIndex(filler);
            
            // Filler goes to the same pair as Role
            std::size_t region_idx = pair_idx * 2 + 1; // Odd regions for Fillers
            std::size_t neuron_idx = idx % config_.neurons_per_region;
            
            if (region_idx < binding_regions_.size()) {
                std::vector<float> pattern(config_.neurons_per_region, 0.0f);
                pattern[neuron_idx] = 1.0f; // Strong activation
                goal.region_patterns[binding_regions_[region_idx]] = pattern;
            }
        }
        
        // Backward compatibility for color/shape if needed
        if (parameters.find("color") != parameters.end()) {
             // ...
        }
        
    } else if (task_type == "sequence") {
        goal.target_regions = sequence_regions_;
        goal.target_pattern.resize(config_.neurons_per_region, 0.0f);
        
        std::string token;
        if (parameters.find("target") != parameters.end()) {
            token = parameters.at("target");
        } else if (parameters.find("sequence_input") != parameters.end()) {
            token = parameters.at("sequence_input");
        }
        
        if (!token.empty()) {
            std::size_t idx = getTokenIndex(token);
            
            std::size_t region_idx = idx % sequence_regions_.size();
            std::size_t neuron_idx = idx % config_.neurons_per_region;
            
            if (region_idx < sequence_regions_.size()) {
                std::vector<float> pattern(config_.neurons_per_region, 0.0f);
                pattern[neuron_idx] = 1.0f;
                goal.region_patterns[sequence_regions_[region_idx]] = pattern;
            }
        }
    }
    
    current_goal_ = goal;
    active_goals_.push_back(goal);
}

void SubstratePhaseC::processStep(int step, float delta_time) {
    if (!initialized_.load()) {
        return;
    }
    
    processing_.store(true);
    current_step_ = step;
    
    try {
        processGoalSetting(delta_time);
        if (brain_) brain_->processStep(delta_time);
        updateAssemblyDynamics(delta_time);
        updateCompetitiveDynamics();
        detectBindings(step);
        predictSequences(step);
        updateStatistics();

        // Integrate metabolic hazard from LearningSystem into SurvivalBias
        if (brain_ && survival_bias_) {
            auto* ls = brain_->getLearningSystem();
            if (ls) {
                auto stats = ls->getStatistics();
                survival_bias_->setExternalHazard(stats.metabolic_hazard);
            }
        }

        // Emit shaped reward based on SurvivalBias metrics once per step
        emitSurvivalReward();
        
    } catch (const std::exception&) {
    }
    
    processing_.store(false);
}

void SubstratePhaseC::processGoalSetting(float delta_time) {
    std::lock_guard<std::mutex> lock(goals_mutex_);
    
    if (current_goal_.active) {
        activateGoalRegions(current_goal_);
        injectGoalSignals(current_goal_);
        
        if (isGoalAchieved(current_goal_)) {
            current_goal_.active = false;
            stats_.goals_achieved++;
        }
    }
}

void SubstratePhaseC::activateGoalRegions(const SubstrateGoal& goal) {
    auto goal_region = brain_->getRegion(goal_region_);
    if (goal_region) {
        auto neurons = goal_region->getNeurons();
        for (std::size_t i = 0; i < neurons.size() && i < 10; ++i) {
            if (neurons[i]) {
                float current_activation = neurons[i]->getActivation();
                float new_act = std::min(1.0f, current_activation + goal.priority * 1.0f);
                neurons[i]->setActivation(new_act);
            }
        }
    }
}

void SubstratePhaseC::injectGoalSignals(const SubstrateGoal& goal) {
    for (std::size_t i = 0; i < goal.target_regions.size(); ++i) {
        auto region_id = goal.target_regions[i];
        auto region = brain_->getRegion(region_id);
        if (region) {
            auto neurons = region->getNeurons();
            
            // Check if we have a specific pattern for this region
            const std::vector<float>* pattern_ptr = &goal.target_pattern;
            auto it = goal.region_patterns.find(region_id);
            if (it != goal.region_patterns.end()) {
                pattern_ptr = &it->second;
            }
            
            const auto& pattern = *pattern_ptr;
            for (std::size_t j = 0; j < neurons.size() && j < pattern.size(); ++j) {
                if (neurons[j] && pattern[j] > 0.1f) {
                    float current_activation = neurons[j]->getActivation();
                    // Strong injection for benchmark validation
                    float new_act = std::min(1.0f, current_activation + pattern[j] * goal.priority * 1.0f);
                    neurons[j]->setActivation(new_act);
                }
            }
        }
    }
}

bool SubstratePhaseC::isGoalAchieved(const SubstrateGoal& goal) const {
    float total_match = 0.0f;
    std::size_t total_neurons = 0;
    
    for (std::size_t i = 0; i < goal.target_regions.size(); ++i) {
        auto region_id = goal.target_regions[i];
        auto region = brain_->getRegion(region_id);
        if (region) {
            auto neurons = region->getNeurons();
            
            const std::vector<float>* pattern_ptr = &goal.target_pattern;
            auto it = goal.region_patterns.find(region_id);
            if (it != goal.region_patterns.end()) {
                pattern_ptr = &it->second;
            }
            const auto& pattern = *pattern_ptr;

            for (std::size_t j = 0; j < neurons.size() && j < pattern.size(); ++j) {
                if (neurons[j]) {
                    float target = pattern[j];
                    float actual = neurons[j]->getActivation();
                    float match = 1.0f - std::abs(target - actual);
                    total_match += match;
                    total_neurons++;
                }
            }
        }
    }
    
    if (total_neurons == 0) return false;
    
    float average_match = total_match / static_cast<float>(total_neurons);
    return average_match > 0.7f;
}

void SubstratePhaseC::updateAssemblyDynamics(float delta_time) {
    auto active_assemblies = detectActiveAssemblies();
    
    std::lock_guard<std::mutex> lock(assemblies_mutex_);
    
    current_assemblies_ = active_assemblies;
    
    updateAssemblyCoherence();
    pruneStaleAssemblies();
    
    assembly_lookup_.clear();
    for (std::size_t i = 0; i < current_assemblies_.size(); ++i) {
        assembly_lookup_[current_assemblies_[i].symbol] = i;
    }
}

std::vector<SubstratePhaseC::SubstrateAssembly> SubstratePhaseC::detectActiveAssemblies() const {
    std::vector<SubstrateAssembly> assemblies;
    
    // Check binding regions
    for (auto region_id : binding_regions_) {
        auto region = brain_->getRegion(region_id);
        if (region) {
            auto neurons = region->getNeurons();
            std::vector<NeuroForge::NeuronID> active_neurons;
            std::vector<float> activations;
            
            for (auto neuron : neurons) {
                if (neuron && neuron->getActivation() > config_.binding_threshold) {
                    active_neurons.push_back(neuron->getId());
                    activations.push_back(neuron->getActivation());
                }
            }
            
            if (active_neurons.size() >= 1) { // Reduced requirement for sparse tokens
                SubstrateAssembly assembly;
                assembly.neurons = active_neurons;
                assembly.activation_pattern = activations;
                assembly.coherence_score = calculateCoherence(activations);
                assembly.symbol = "binding_assembly_" + std::to_string(region_id);
                assembly.last_active = std::chrono::steady_clock::now();
                
                if (assembly.coherence_score > config_.binding_coherence_min) {
                    assemblies.push_back(assembly);
                }
            }
        }
    }
    
    // Check sequence regions
    for (auto region_id : sequence_regions_) {
        auto region = brain_->getRegion(region_id);
        if (region) {
            auto neurons = region->getNeurons();
            std::vector<NeuroForge::NeuronID> active_neurons;
            std::vector<float> activations;
            
            for (auto neuron : neurons) {
                if (neuron && neuron->getActivation() > config_.sequence_threshold) {
                    active_neurons.push_back(neuron->getId());
                    activations.push_back(neuron->getActivation());
                }
            }
            
            if (active_neurons.size() >= 1) {
                SubstrateAssembly assembly;
                assembly.neurons = active_neurons;
                assembly.activation_pattern = activations;
                assembly.coherence_score = calculateCoherence(activations);
                assembly.symbol = "sequence_assembly_" + std::to_string(region_id);
                assembly.last_active = std::chrono::steady_clock::now();
                
                if (assembly.coherence_score > config_.sequence_coherence_min) {
                    assemblies.push_back(assembly);
                }
            }
        }
    }
    
    return assemblies;
}

void SubstratePhaseC::detectBindings(int step) {
    std::lock_guard<std::mutex> lock(assemblies_mutex_);
    
    recent_bindings_.clear();
    
    // std::cout << "DEBUG: active_assemblies count: " << current_assemblies_.size() << std::endl;

    std::size_t num_role_regions = binding_regions_.size() / 2;
    if (num_role_regions == 0) num_role_regions = 1;

    std::vector<std::vector<std::string>> pair_roles(num_role_regions);
    std::vector<std::vector<std::string>> pair_fillers(num_role_regions);

    for (const auto& assembly : current_assemblies_) {
        long long region_id = -1;
        try {
            if (assembly.symbol.find("binding_assembly_") == 0) {
                region_id = std::stoll(assembly.symbol.substr(17));
            }
        } catch (...) { continue; }
        
        if (region_id == -1) continue;
        
        auto it = std::find(binding_regions_.begin(), binding_regions_.end(), region_id);
        if (it == binding_regions_.end()) continue;
        std::size_t region_idx = std::distance(binding_regions_.begin(), it);
        
        if (assembly.neurons.empty()) continue;
        
        auto region = brain_->getRegion(region_id);
        if (!region) continue;
        auto region_neurons = region->getNeurons();
        
        float max_act = -1.0f;
        NeuroForge::NeuronID max_neuron_id = 0;
        
        for (size_t i = 0; i < assembly.neurons.size(); ++i) {
            if (assembly.activation_pattern[i] > max_act) {
                max_act = assembly.activation_pattern[i];
                max_neuron_id = assembly.neurons[i];
            }
        }
        
        std::size_t neuron_idx = 0;
        bool found = false;
        for (size_t i = 0; i < region_neurons.size(); ++i) {
            if (region_neurons[i] && region_neurons[i]->getId() == max_neuron_id) {
                neuron_idx = i;
                found = true;
                break;
            }
        }
        
        if (!found) continue;
        
        std::size_t token_idx = neuron_idx; // Simplified mapping
        std::string token = getTokenString(token_idx);
        
        std::size_t pair_idx = region_idx / 2;
        bool is_role = (region_idx % 2 == 0);
        
        if (is_role) {
            if (pair_idx < pair_roles.size()) pair_roles[pair_idx].push_back(token);
        } else {
            if (pair_idx < pair_fillers.size()) pair_fillers[pair_idx].push_back(token);
        }
    }
    
    for (size_t i = 0; i < num_role_regions; ++i) {
        for (const auto& role : pair_roles[i]) {
            for (const auto& filler : pair_fillers[i]) {
                BindingRow binding;
                binding.step = step;
                binding.role = role;
                binding.filler = filler;
                binding.strength = 1.0f;
                recent_bindings_.push_back(binding);
                stats_.bindings_created++;
            }
        }
    }
}

void SubstratePhaseC::predictSequences(int step) {
    std::lock_guard<std::mutex> lock(assemblies_mutex_);
    
    std::string predicted_token;
    float max_coherence = 0.0f;
    
    for (const auto& assembly : current_assemblies_) {
        long long region_id = -1;
        try {
             if (assembly.symbol.find("sequence_assembly_") == 0) {
                region_id = std::stoll(assembly.symbol.substr(18));
            }
        } catch (...) { continue; }
        
        if (region_id == -1) continue;
        
        auto it = std::find(sequence_regions_.begin(), sequence_regions_.end(), region_id);
        if (it == sequence_regions_.end()) continue;
        
        if (assembly.coherence_score > max_coherence) {
            auto region = brain_->getRegion(region_id);
            if (!region) continue;
            auto region_neurons = region->getNeurons();
            
            float max_act = -1.0f;
            NeuroForge::NeuronID max_neuron_id = 0;
            for (size_t i = 0; i < assembly.neurons.size(); ++i) {
                if (assembly.activation_pattern[i] > max_act) {
                    max_act = assembly.activation_pattern[i];
                    max_neuron_id = assembly.neurons[i];
                }
            }
            
            std::size_t neuron_idx = 0;
            for (size_t i = 0; i < region_neurons.size(); ++i) {
                if (region_neurons[i] && region_neurons[i]->getId() == max_neuron_id) {
                    neuron_idx = i;
                    break;
                }
            }
            
            std::size_t token_idx = neuron_idx; // Simplified mapping
            predicted_token = getTokenString(token_idx);
            max_coherence = assembly.coherence_score;
            
            // Debug print for sequence prediction
            // std::cout << "DEBUG: Predict seq assembly " << assembly.symbol 
            //           << " coherence=" << assembly.coherence_score 
            //           << " neuron=" << max_neuron_id 
            //           << " token=" << predicted_token << std::endl;
        }
    }
    
    if (!predicted_token.empty()) {
        SequenceRow seq;
        seq.step = step;
        seq.predicted = predicted_token;
        seq.target = ""; 
        seq.correct = 0;
        
        recent_sequences_.push_back(seq);
        stats_.sequences_predicted++;
    }
}

void SubstratePhaseC::updateCompetitiveDynamics() {
    auto comp_region = brain_->getRegion(competition_region_);
    if (comp_region) {
        auto neurons = comp_region->getNeurons();
        float max_activation = 0.0f;
        NeuroForge::NeuronPtr winner = nullptr;
        
        for (auto neuron : neurons) {
            if (neuron && neuron->getActivation() > max_activation) {
                max_activation = neuron->getActivation();
                winner = neuron;
            }
        }
        
        if (winner) {
            for (auto neuron : neurons) {
                if (neuron && neuron != winner) {
                    float current_activation = neuron->getActivation();
                    neuron->setActivation(current_activation - 0.2f * max_activation);
                }
            }
        }
    }
}

std::vector<BindingRow> SubstratePhaseC::getBindingResults(int step) const {
    std::lock_guard<std::mutex> lock(assemblies_mutex_);
    return recent_bindings_;
}

SequenceRow SubstratePhaseC::getSequenceResult(int step) const {
    std::lock_guard<std::mutex> lock(assemblies_mutex_);
    if (!recent_sequences_.empty()) {
        return recent_sequences_.back();
    }
    return SequenceRow{};
}

std::vector<SubstratePhaseC::SubstrateAssembly> SubstratePhaseC::getCurrentAssemblies() const {
    std::lock_guard<std::mutex> lock(assemblies_mutex_);
    return current_assemblies_;
}

float SubstratePhaseC::calculateCoherence(const std::vector<float>& activations) const {
    if (activations.empty()) return 0.0f;
    
    // Coherence = Mean * (1.0 - StdDev)
    // This rewards high average activation while penalizing inconsistency (noise)
    
    float sum = 0.0f;
    for (float a : activations) {
        sum += a;
    }
    float mean = sum / activations.size();
    
    float variance_sum = 0.0f;
    for (float a : activations) {
        variance_sum += (a - mean) * (a - mean);
    }
    float variance = variance_sum / activations.size();
    float std_dev = std::sqrt(variance);
    
    // Clamp std_dev to [0, 1] effectively (activations are 0-1)
    // If std_dev is high (e.g. > 0.3), penalize heavily.
    // Let's use a softer penalty: Mean / (1.0 + StdDev)
    // Or Mean * exp(-StdDev)
    
    // User requested: "favors consistent high activation over noisy activation"
    // Let's try: Mean * (1.0 - 6.0 * StdDev) clamped to 0
    // Increased penalty to 6.0 to further reduce crosstalk at higher loads.
    
    float coherence = mean * (1.0f - 6.0f * std_dev);
    // std::cout << "DEBUG: calculateCoherence mean=" << mean << " std_dev=" << std_dev << " raw_coherence=" << coherence << std::endl;
    return std::max(0.0f, coherence);
}

std::size_t SubstratePhaseC::getTokenIndex(const std::string& token) {
    auto it = token_to_index_.find(token);
    if (it != token_to_index_.end()) {
        return it->second;
    }
    std::size_t idx = next_token_index_++;
    token_to_index_[token] = idx;
    {
        index_to_token_[idx] = token;
    }
    return idx;
}

std::string SubstratePhaseC::getTokenString(std::size_t index) const {
    auto it = index_to_token_.find(index);
    if (it != index_to_token_.end()) {
        return it->second;
    }
    return "unknown_" + std::to_string(index);
}

void SubstratePhaseC::updateStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    std::lock_guard<std::mutex> lock_assemblies(assemblies_mutex_);
    stats_.active_assemblies = current_assemblies_.size();
    
    float total_coherence = 0.0f;
    if (!current_assemblies_.empty()) {
        for (const auto& a : current_assemblies_) {
            total_coherence += a.coherence_score;
        }
        stats_.average_coherence = total_coherence / current_assemblies_.size();
    } else {
        stats_.average_coherence = 0.0f;
    }
}

void SubstratePhaseC::updateAssemblyCoherence() {
    // Coherence is calculated during detection
}

void SubstratePhaseC::pruneStaleAssemblies() {
    // Stale assemblies are filtered during detection
}

void SubstratePhaseC::emitSurvivalReward() {
    if (!config_.emit_survival_rewards || !survival_bias_ || !brain_) return;
    
    float threat = survival_bias_->getLastAppliedWeight();
    float safety = 1.0f - threat;
    float reward = safety * config_.survival_reward_scale;
    
    // In a real system, we'd emit this to the LearningSystem
    if (auto ls = brain_->getLearningSystem()) {
        ls->applyExternalReward(reward);
    }
}

} // namespace Core
} // namespace NeuroForge
