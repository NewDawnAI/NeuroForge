#include "core/SubstratePhaseC.h"
#include "biases/SurvivalBias.h"
#include <algorithm>
#include <random>
#include <chrono>
#include <cmath>
#include <sstream>
#include <iomanip>

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
            brain_->connectRegions(binding_regions_[i], binding_regions_[j], 
                                 config_.recurrent_strength, {0.05f, 0.15f});
        }
    }
    
    // Connect sequence regions with temporal adjacency connections
    for (std::size_t i = 0; i < sequence_regions_.size() - 1; ++i) {
        brain_->connectRegions(sequence_regions_[i], sequence_regions_[i + 1],
                             config_.recurrent_strength, {0.05f, 0.15f});
    }
    
    // Connect competition region to all other regions for global competition
    for (auto region_id : binding_regions_) {
        brain_->connectRegions(competition_region_, region_id,
                             config_.competition_strength, {0.1f, 0.3f});
    }
    for (auto region_id : sequence_regions_) {
        brain_->connectRegions(competition_region_, region_id,
                             config_.competition_strength, {0.1f, 0.3f});
    }
    
    // Connect goal region to all task regions for top-down control
    for (auto region_id : binding_regions_) {
        brain_->connectRegions(goal_region_, region_id,
                             config_.goal_setting_strength, {0.05f, 0.15f});
    }
    for (auto region_id : sequence_regions_) {
        brain_->connectRegions(goal_region_, region_id,
                             config_.goal_setting_strength, {0.05f, 0.15f});
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
        // Set up binding goal - activate appropriate binding regions
        goal.target_regions = binding_regions_;
        goal.target_pattern.resize(config_.neurons_per_region, 0.0f);
        
        // Create target pattern based on color/shape parameters
        if (parameters.find("color") != parameters.end()) {
            auto color = parameters.at("color");
            auto color_it = std::find(colors_.begin(), colors_.end(), color);
            if (color_it != colors_.end()) {
                std::size_t color_idx = std::distance(colors_.begin(), color_it);
                std::size_t region_idx = color_idx % binding_regions_.size();
                if (region_idx < goal.target_pattern.size()) {
                    goal.target_pattern[region_idx] = 0.8f;
                }
            }
        }
        
        if (parameters.find("shape") != parameters.end()) {
            auto shape = parameters.at("shape");
            auto shape_it = std::find(shapes_.begin(), shapes_.end(), shape);
            if (shape_it != shapes_.end()) {
                std::size_t shape_idx = std::distance(shapes_.begin(), shape_it);
                std::size_t region_idx = (shape_idx + colors_.size()) % binding_regions_.size();
                if (region_idx < goal.target_pattern.size()) {
                    goal.target_pattern[region_idx] = 0.8f;
                }
            }
        }
    } else if (task_type == "sequence") {
        // Set up sequence goal - activate sequence regions
        goal.target_regions = sequence_regions_;
        goal.target_pattern.resize(config_.neurons_per_region, 0.0f);
        
        // Create target pattern for sequence prediction
        if (parameters.find("target") != parameters.end()) {
            auto target = parameters.at("target");
            auto token_it = std::find(seq_tokens_.begin(), seq_tokens_.end(), target);
            if (token_it != seq_tokens_.end()) {
                std::size_t token_idx = std::distance(seq_tokens_.begin(), token_it);
                std::size_t region_idx = token_idx % sequence_regions_.size();
                if (region_idx < goal.target_pattern.size()) {
                    goal.target_pattern[region_idx] = 0.9f;
                }
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
        // Process goal-setting signals
        processGoalSetting(delta_time);
        
        // Update assembly dynamics
        updateAssemblyDynamics(delta_time);
        
        // Update competitive dynamics
        updateCompetitiveDynamics();
        
        // Detect bindings and sequences from substrate activity
        detectBindings(step);
        predictSequences(step);
        
        // Update statistics
        updateStatistics();

        // Integrate metabolic hazard from LearningSystem into SurvivalBias
        if (brain_ && survival_bias_) {
            auto* ls = brain_->getLearningSystem();
            if (ls) {
                auto stats = ls->getStatistics();
                survival_bias_->setMetabolicHazard(stats.metabolic_hazard);
            }
        }

        // Emit shaped reward based on SurvivalBias metrics once per step
        emitSurvivalReward();
        
    } catch (const std::exception&) {
        // Handle errors gracefully
    }
    
    processing_.store(false);
}

void SubstratePhaseC::processGoalSetting(float delta_time) {
    std::lock_guard<std::mutex> lock(goals_mutex_);
    
    if (current_goal_.active) {
        activateGoalRegions(current_goal_);
        injectGoalSignals(current_goal_);
        
        // Check if goal is achieved
        if (isGoalAchieved(current_goal_)) {
            current_goal_.active = false;
            stats_.goals_achieved++;
        }
    }
}

void SubstratePhaseC::activateGoalRegions(const SubstrateGoal& goal) {
    // Activate goal region to provide top-down signals
    auto goal_region = brain_->getRegion(goal_region_);
    if (goal_region) {
        auto neurons = goal_region->getNeurons();
        for (std::size_t i = 0; i < neurons.size() && i < 10; ++i) {
            if (neurons[i]) {
                // Inject goal signal by increasing activation
                float current_activation = neurons[i]->getActivation();
                neurons[i]->setActivation(current_activation + goal.priority * 0.5f);
            }
        }
    }
}

void SubstratePhaseC::injectGoalSignals(const SubstrateGoal& goal) {
    // Inject goal-specific activation patterns into target regions
    for (std::size_t i = 0; i < goal.target_regions.size(); ++i) {
        auto region = brain_->getRegion(goal.target_regions[i]);
        if (region) {
            auto neurons = region->getNeurons();
            for (std::size_t j = 0; j < neurons.size() && j < goal.target_pattern.size(); ++j) {
                if (neurons[j] && goal.target_pattern[j] > 0.1f) {
                    // Inject goal signal by increasing activation
                    float current_activation = neurons[j]->getActivation();
                    neurons[j]->setActivation(current_activation + goal.target_pattern[j] * goal.priority * 0.3f);
                }
            }
        }
    }
}

bool SubstratePhaseC::isGoalAchieved(const SubstrateGoal& goal) const {
    // Check if target regions show desired activation patterns
    float total_match = 0.0f;
    std::size_t total_neurons = 0;
    
    for (std::size_t i = 0; i < goal.target_regions.size(); ++i) {
        auto region = brain_->getRegion(goal.target_regions[i]);
        if (region) {
            auto neurons = region->getNeurons();
            for (std::size_t j = 0; j < neurons.size() && j < goal.target_pattern.size(); ++j) {
                if (neurons[j]) {
                    float target = goal.target_pattern[j];
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
    return average_match > 0.7f; // Goal achieved if 70% match
}

void SubstratePhaseC::updateAssemblyDynamics(float delta_time) {
    // Detect currently active assemblies
    auto active_assemblies = detectActiveAssemblies();
    
    std::lock_guard<std::mutex> lock(assemblies_mutex_);
    
    // Update existing assemblies and add new ones
    current_assemblies_ = active_assemblies;
    
    // Update coherence scores
    updateAssemblyCoherence();
    
    // Prune stale assemblies
    pruneStaleAssemblies();
    
    // Rebuild lookup map
    assembly_lookup_.clear();
    for (std::size_t i = 0; i < current_assemblies_.size(); ++i) {
        assembly_lookup_[current_assemblies_[i].symbol] = i;
    }
}

std::vector<SubstratePhaseC::SubstrateAssembly> SubstratePhaseC::detectActiveAssemblies() const {
    std::vector<SubstrateAssembly> assemblies;
    
    // Check binding regions for assemblies
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
            
            if (active_neurons.size() >= 3) { // Minimum assembly size
                SubstrateAssembly assembly;
                assembly.neurons = active_neurons;
                assembly.activation_pattern = activations;
                assembly.coherence_score = calculateCoherence(active_neurons);
                assembly.symbol = "binding_assembly_" + std::to_string(assemblies.size());
                assembly.last_active = std::chrono::steady_clock::now();
                
                if (assembly.coherence_score > config_.binding_coherence_min) {
                    assemblies.push_back(assembly);
                }
            }
        }
    }
    
    // Check sequence regions for assemblies
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
            
            if (active_neurons.size() >= 2) { // Minimum sequence assembly size
                SubstrateAssembly assembly;
                assembly.neurons = active_neurons;
                assembly.activation_pattern = activations;
                assembly.coherence_score = calculateCoherence(active_neurons);
                assembly.symbol = "sequence_assembly_" + std::to_string(assemblies.size());
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
    
    // Generate bindings from substrate assembly activity
    for (const auto& assembly : current_assemblies_) {
        if (assembly.symbol.find("binding_assembly") != std::string::npos) {
            // Determine role and filler from assembly activity
            std::string role, filler;
            float strength = assembly.coherence_score;
            
            // Map assembly activity to semantic bindings
            if (assembly.activation_pattern.size() > 0) {
                float max_activation = *std::max_element(assembly.activation_pattern.begin(), 
                                                       assembly.activation_pattern.end());
                
                // Simple mapping based on activation patterns
                if (max_activation > 0.8f) {
                    role = "color";
                    filler = colors_[step % colors_.size()];
                } else if (max_activation > 0.6f) {
                    role = "shape";
                    filler = shapes_[step % shapes_.size()];
                } else {
                    continue; // Skip weak assemblies
                }
                
                BindingRow binding;
                binding.step = step;
                binding.role = role;
                binding.filler = filler;
                binding.strength = strength;
                
                recent_bindings_.push_back(binding);
                stats_.bindings_created++;
            }
        }
    }
}

void SubstratePhaseC::predictSequences(int step) {
    std::lock_guard<std::mutex> lock(assemblies_mutex_);
    
    // Generate sequence predictions from substrate assembly activity
    std::string predicted_token;
    float max_coherence = 0.0f;
    
    for (const auto& assembly : current_assemblies_) {
        if (assembly.symbol.find("sequence_assembly") != std::string::npos && 
            assembly.coherence_score > max_coherence) {
            max_coherence = assembly.coherence_score;
            
            // Map assembly to sequence token
            if (assembly.activation_pattern.size() > 0) {
                std::size_t max_idx = std::distance(assembly.activation_pattern.begin(),
                    std::max_element(assembly.activation_pattern.begin(), assembly.activation_pattern.end()));
                predicted_token = seq_tokens_[max_idx % seq_tokens_.size()];
            }
        }
    }
    
    if (!predicted_token.empty()) {
        SequenceRow sequence;
        sequence.step = step;
        sequence.target = seq_tokens_[step % seq_tokens_.size()]; // Expected token
        sequence.predicted = predicted_token;
        sequence.correct = (sequence.target == sequence.predicted) ? 1 : 0;
        
        recent_sequences_.push_back(sequence);
        if (recent_sequences_.size() > max_history_size_) {
            recent_sequences_.erase(recent_sequences_.begin());
        }
        
        stats_.sequences_predicted++;
        if (sequence.correct) {
            // Update accuracy statistics
        }
    }
}

void SubstratePhaseC::updateCompetitiveDynamics() {
    // Apply competitive dynamics through the competition region
    auto comp_region = brain_->getRegion(competition_region_);
    if (comp_region) {
        auto neurons = comp_region->getNeurons();
        
        // Find most active neuron
        float max_activation = 0.0f;
        NeuroForge::NeuronPtr winner = nullptr;
        
        for (auto neuron : neurons) {
            if (neuron && neuron->getActivation() > max_activation) {
                max_activation = neuron->getActivation();
                winner = neuron;
            }
        }
        
        // Suppress non-winners
        if (winner) {
            for (auto neuron : neurons) {
                if (neuron && neuron != winner) {
                    // Apply lateral inhibition by reducing activation
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
    return SequenceRow{}; // Return empty if no sequences
}

std::vector<SubstratePhaseC::SubstrateAssembly> SubstratePhaseC::getCurrentAssemblies() const {
    std::lock_guard<std::mutex> lock(assemblies_mutex_);
    return current_assemblies_;
}

float SubstratePhaseC::calculateCoherence(const std::vector<NeuroForge::NeuronID>& neurons) const {
    if (neurons.size() < 2) return 0.0f;
    
    // Calculate coherence as correlation between neuron activations
    std::vector<float> activations;
    for (auto neuron_id : neurons) {
        // Find the neuron in the regions
        NeuroForge::NeuronPtr neuron = nullptr;
        for (auto region_id : binding_regions_) {
            auto region = brain_->getRegion(region_id);
            if (region) {
                neuron = region->getNeuron(neuron_id);
                if (neuron) break;
            }
        }
        if (neuron) {
            activations.push_back(neuron->getActivation());
        }
    }
    
    if (activations.size() < 2) return 0.0f;
    
    // Simple coherence measure: variance of activations (lower = more coherent)
    float mean = 0.0f;
    for (float activation : activations) {
        mean += activation;
    }
    mean /= static_cast<float>(activations.size());
    
    float variance = 0.0f;
    for (float activation : activations) {
        variance += (activation - mean) * (activation - mean);
    }
    variance /= static_cast<float>(activations.size());
    
    // Convert variance to coherence (0-1 scale)
    return std::max(0.0f, 1.0f - variance);
}

void SubstratePhaseC::updateAssemblyCoherence() {
    for (auto& assembly : current_assemblies_) {
        // Base coherence from substrate dynamics
        float base = calculateCoherence(assembly.neurons);
        assembly.coherence_score = base;

        // Modulate with SurvivalBias if attached
        if (survival_bias_) {
            float modulated = survival_bias_->applyCoherenceBias(
                base, assembly.activation_pattern, config_.hazard_coherence_weight);
            assembly.coherence_score = modulated;

            // Emit telemetry if sink is set
            if (json_sink_) {
                auto m = survival_bias_->getLastMetrics();
                std::ostringstream js;
                js.setf(std::ios::fixed);
                js << "{\"version\":1,\"phase\":\"C\",\"event\":\"survival_mod\",\"time\":\"";
                // Simple ISO8601 timestamp
                {
                    using namespace std::chrono;
                    auto now = system_clock::now();
                    std::time_t t = system_clock::to_time_t(now);
                    std::tm tm{};
#if defined(_WIN32)
                    gmtime_s(&tm, &t);
#else
                    gmtime_r(&t, &tm);
#endif
                    std::ostringstream ts;
                    ts << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
                    js << ts.str();
                }
                js << "\",\"step\":" << current_step_
                   << ",\"symbol\":\"" << assembly.symbol << "\""
                   << ",\"base\":" << std::setprecision(4) << base
                   << ",\"modulated\":" << std::setprecision(4) << modulated
                   << ",\"delta\":" << std::setprecision(4) << (modulated - base)
                   << ",\"hazard_probability\":" << std::setprecision(4) << m.hazard_probability
                   << ",\"risk_score\":" << std::setprecision(4) << m.risk_score
                   << ",\"arousal_level\":" << std::setprecision(4) << m.arousal_level
                   << ",\"avoidance_drive\":" << std::setprecision(4) << m.avoidance_drive
                   << ",\"approach_drive\":" << std::setprecision(4) << m.approach_drive
                   << ",\"weight\":" << std::setprecision(4) << config_.hazard_coherence_weight
                   << ",\"effective_weight\":" << std::setprecision(4) << (survival_bias_ ? survival_bias_->getLastAppliedWeight() : config_.hazard_coherence_weight)
                   << "}";
                json_sink_(js.str());
            }
        }
    }
}

void SubstratePhaseC::pruneStaleAssemblies() {
    auto now = std::chrono::steady_clock::now();
    auto threshold = std::chrono::seconds(5); // Remove assemblies not active for 5 seconds
    
    current_assemblies_.erase(
        std::remove_if(current_assemblies_.begin(), current_assemblies_.end(),
            [this, now, threshold](const SubstrateAssembly& assembly) {
                return (now - assembly.last_active) > threshold || 
                       assembly.coherence_score < config_.prune_coherence_threshold;
            }),
        current_assemblies_.end()
    );
}

void SubstratePhaseC::updateStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.assemblies_formed = current_assemblies_.size();
    
    // Calculate average coherence
    if (!current_assemblies_.empty()) {
        float total_coherence = 0.0f;
        for (const auto& assembly : current_assemblies_) {
            total_coherence += assembly.coherence_score;
        }
        stats_.average_coherence = total_coherence / static_cast<float>(current_assemblies_.size());
    }
    
    // Calculate binding accuracy
    if (!recent_bindings_.empty()) {
        // Simple accuracy measure based on binding strength
        float total_strength = 0.0f;
        for (const auto& binding : recent_bindings_) {
            total_strength += binding.strength;
        }
        stats_.binding_accuracy = total_strength / static_cast<float>(recent_bindings_.size());
    }
    
    // Calculate sequence accuracy
    if (!recent_sequences_.empty()) {
        std::size_t correct_predictions = 0;
        for (const auto& sequence : recent_sequences_) {
            if (sequence.correct) {
                correct_predictions++;
            }
        }
        stats_.sequence_accuracy = static_cast<float>(correct_predictions) / 
                                 static_cast<float>(recent_sequences_.size());
    }
}

void SubstratePhaseC::emitSurvivalReward() {
    if (!config_.emit_survival_rewards) return;
    if (!survival_bias_ || !brain_) return;

    // Use latest metrics from SurvivalBias which reflect current step dynamics
    auto m = survival_bias_->getLastMetrics();

    // Simple shaped reward: encourage approach, discourage avoidance
    double reward = static_cast<double>(config_.survival_reward_scale) *
                    static_cast<double>(m.approach_drive - m.avoidance_drive);
    // Clamp to [-1, 1] for stability
    if (reward > 1.0) reward = 1.0; else if (reward < -1.0) reward = -1.0;

    // Context for logging/telemetry
    std::ostringstream ctx;
    ctx.setf(std::ios::fixed);
    ctx << "{"
        << "\"phase\":\"C\","
        << "\"event\":\"survival_reward\","
        << "\"step\":" << current_step_
        << ",\"hazard_probability\":" << std::setprecision(4) << m.hazard_probability
        << ",\"risk_score\":" << std::setprecision(4) << m.risk_score
        << ",\"arousal_level\":" << std::setprecision(4) << m.arousal_level
        << ",\"avoidance_drive\":" << std::setprecision(4) << m.avoidance_drive
        << ",\"approach_drive\":" << std::setprecision(4) << m.approach_drive
        << ",\"weight\":" << std::setprecision(4) << config_.hazard_coherence_weight
        << ",\"effective_weight\":" << std::setprecision(4) << (survival_bias_ ? survival_bias_->getLastAppliedWeight() : config_.hazard_coherence_weight)
        << "}";

    brain_->deliverReward(reward, std::string("phase_c_survival"), ctx.str());
}

} // namespace Core
} // namespace NeuroForge
