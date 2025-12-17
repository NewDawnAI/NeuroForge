#include "core/NeuralLanguageBindings.h"
#include "core/Neuron.h"
#include "core/Synapse.h"
#include "core/Region.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <iostream>
#include <sstream>

namespace NeuroForge {
namespace Core {

NeuralLanguageBindings::NeuralLanguageBindings(std::shared_ptr<HypergraphBrain> hypergraph_brain,
                                             const Config& config)
    : hypergraph_brain_(hypergraph_brain)
    , config_(config) {
}

NeuralLanguageBindings::~NeuralLanguageBindings() {
    shutdown();
}

bool NeuralLanguageBindings::initialize() {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    if (is_initialized_.load()) {
        return true;
    }
    
    if (!hypergraph_brain_) {
        std::cerr << "NeuralLanguageBindings: Missing hypergraph brain reference" << std::endl;
        return false;
    }
    
    // Integrate with learning system
    integrateWithLearningSystem();
    
    // Configure language-specific learning parameters
    configureLanguageSpecificLearning();
    
    is_initialized_.store(true);
    
    std::cout << "NeuralLanguageBindings: Successfully initialized" << std::endl;
    return true;
}

void NeuralLanguageBindings::shutdown() {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    is_initialized_.store(false);
    
    // Clear all bindings
    token_assemblies_.clear();
    proto_word_patterns_.clear();
    prosodic_circuits_.clear();
    cross_modal_bindings_.clear();
    attention_circuits_.clear();
    
    // Reset statistics
    statistics_ = Statistics{};
}

void NeuralLanguageBindings::reset() {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    // Reset all binding states without clearing structures
    for (auto& [symbol, assembly] : token_assemblies_) {
        assembly.assembly_coherence = 0.0f;
        assembly.firing_count = 0;
    }
    
    for (auto& [pattern, neural_pattern] : proto_word_patterns_) {
        neural_pattern.crystallization_strength = 0.0f;
        neural_pattern.neural_stability = 0.0f;
        neural_pattern.is_crystallized = false;
        neural_pattern.reinforcement_count = 0;
    }
    
    for (auto& [name, circuit] : prosodic_circuits_) {
        circuit.pattern_sensitivity = config_.prosodic_sensitivity;
        circuit.motherese_bias = 0.0f;
        circuit.is_motherese_detector = false;
    }
    
    for (auto& [id, binding] : cross_modal_bindings_) {
        binding.binding_strength = 0.0f;
        binding.is_stable_binding = false;
        binding.modality_strengths.clear();
    }
    
    // Reset statistics
    statistics_ = Statistics{};
}

bool NeuralLanguageBindings::createTokenNeuralAssembly(const std::string& token_symbol,
                                                      const std::vector<float>& token_embedding,
                                                      NeuroForge::RegionID target_region) {
    std::lock_guard<std::mutex> lock(assemblies_mutex_);
    
    if (token_assemblies_.find(token_symbol) != token_assemblies_.end()) {
        return false; // Assembly already exists
    }
    
    // Allocate neural assembly
    std::vector<NeuroForge::NeuronID> assembly_neurons = allocateNeuralAssembly(target_region, config_.token_assembly_size);
    if (assembly_neurons.empty()) {
        return false;
    }
    
    // Create token neural assembly
    TokenNeuralAssembly assembly;
    assembly.token_symbol = token_symbol;
    assembly.primary_neuron = assembly_neurons[0];
    assembly.assembly_neurons = assembly_neurons;
    assembly.assembly_coherence = 0.0f;
    assembly.activation_threshold = 0.5f;
    assembly.firing_count = 0;
    assembly.last_firing = std::chrono::steady_clock::now();
    
    // Setup internal connections
    setupTokenAssemblyConnections(assembly);
    
    // Calculate and set initial coherence
    assembly.assembly_coherence = calculateAssemblyCoherence(assembly);
    
    token_assemblies_[token_symbol] = assembly;
    
    // Update statistics after creating the assembly
    updateBindingStatistics();
    
    std::cout << "Created token neural assembly for: " << token_symbol << std::endl;
    return true;
}

bool NeuralLanguageBindings::activateTokenAssembly(const std::string& token_symbol, float activation_strength) {
    std::lock_guard<std::mutex> lock(assemblies_mutex_);
    
    auto it = token_assemblies_.find(token_symbol);
    if (it == token_assemblies_.end()) {
        return false;
    }
    
    TokenNeuralAssembly& assembly = it->second;
    
    // Activate all neurons in the assembly
    activateNeurons(assembly.assembly_neurons, activation_strength);
    
    // Update assembly state
    assembly.firing_count++;
    assembly.last_firing = std::chrono::steady_clock::now();
    
    // Calculate coherence and ensure it reflects the activation
    float calculated_coherence = calculateAssemblyCoherence(assembly);
    
    // Ensure coherence is at least as high as the activation strength
    // This prevents recently activated assemblies from having artificially low coherence
    assembly.assembly_coherence = std::max(calculated_coherence, activation_strength * 0.8f);
    
    return true;
}

bool NeuralLanguageBindings::createProtoWordNeuralPattern(const std::string& pattern,
                                                         const std::vector<std::string>& phoneme_sequence,
                                                         NeuroForge::RegionID target_region) {
    std::lock_guard<std::mutex> lock(patterns_mutex_);
    
    if (proto_word_patterns_.find(pattern) != proto_word_patterns_.end()) {
        return false; // Pattern already exists
    }
    
    if (phoneme_sequence.size() > config_.max_phoneme_sequence_length) {
        return false; // Sequence too long
    }
    
    // Allocate neurons for phoneme sequence + pattern neuron
    std::size_t total_neurons = phoneme_sequence.size() + 1;
    std::vector<NeuroForge::NeuronID> pattern_neurons = allocateNeuralAssembly(target_region, total_neurons);
    if (pattern_neurons.empty()) {
        return false;
    }
    
    // Create proto-word neural pattern
    ProtoWordNeuralPattern neural_pattern;
    neural_pattern.proto_word_pattern = pattern;
    neural_pattern.phoneme_sequence = phoneme_sequence;
    neural_pattern.sequence_neurons = std::vector<NeuroForge::NeuronID>(pattern_neurons.begin(), pattern_neurons.end() - 1);
    neural_pattern.pattern_neuron = pattern_neurons.back();
    neural_pattern.crystallization_strength = 0.0f;
    neural_pattern.neural_stability = 0.0f;
    neural_pattern.is_crystallized = false;
    neural_pattern.reinforcement_count = 0;
    
    // Setup sequential connections
    setupProtoWordPatternConnections(neural_pattern);
    
    proto_word_patterns_[pattern] = neural_pattern;
    
    std::cout << "Created proto-word neural pattern for: " << pattern << std::endl;
    return true;
}

bool NeuralLanguageBindings::reinforceProtoWordPattern(const std::string& pattern, float reinforcement_strength) {
    std::lock_guard<std::mutex> lock(patterns_mutex_);
    
    auto it = proto_word_patterns_.find(pattern);
    if (it == proto_word_patterns_.end()) {
        return false;
    }
    
    ProtoWordNeuralPattern& neural_pattern = it->second;
    
    // Increase crystallization strength
    neural_pattern.crystallization_strength += reinforcement_strength;
    neural_pattern.crystallization_strength = std::min(1.0f, neural_pattern.crystallization_strength);
    neural_pattern.reinforcement_count++;
    
    // Strengthen synaptic connections
    strengthenSynapses(neural_pattern.sequence_synapses, reinforcement_strength);
    
    // Update neural stability
    neural_pattern.neural_stability = calculatePatternStability(neural_pattern);
    
    // Check for crystallization
    if (neural_pattern.crystallization_strength >= config_.crystallization_threshold &&
        neural_pattern.neural_stability >= config_.pattern_stability_threshold &&
        !neural_pattern.is_crystallized) {
        
        crystallizeProtoWordPattern(pattern);
    }
    
    return true;
}

bool NeuralLanguageBindings::crystallizeProtoWordPattern(const std::string& pattern) {
    std::lock_guard<std::mutex> lock(patterns_mutex_);
    
    auto it = proto_word_patterns_.find(pattern);
    if (it == proto_word_patterns_.end()) {
        return false;
    }
    
    ProtoWordNeuralPattern& neural_pattern = it->second;
    
    if (neural_pattern.is_crystallized) {
        return true; // Already crystallized
    }
    
    // Mark as crystallized
    neural_pattern.is_crystallized = true;
    
    // Strengthen all connections significantly
    strengthenSynapses(neural_pattern.sequence_synapses, 2.0f);
    
    // Activate pattern neuron strongly
    std::vector<NeuroForge::NeuronID> pattern_neuron_vec = {neural_pattern.pattern_neuron};
    activateNeurons(pattern_neuron_vec, 1.0f);
    
    std::cout << "Crystallized proto-word pattern: " << pattern << std::endl;
    return true;
}

bool NeuralLanguageBindings::createProsodicNeuralCircuit(const std::string& pattern_name,
                                                        const LanguageSystem::AcousticFeatures& template_features,
                                                        NeuroForge::RegionID target_region) {
    std::lock_guard<std::mutex> lock(circuits_mutex_);
    
    if (prosodic_circuits_.find(pattern_name) != prosodic_circuits_.end()) {
        return false; // Circuit already exists
    }
    
    // Allocate neurons for prosodic circuit (4 neurons: pitch, energy, rhythm, integration)
    std::vector<NeuroForge::NeuronID> circuit_neurons = allocateNeuralAssembly(target_region, 4);
    if (circuit_neurons.size() < 4) {
        return false;
    }
    
    // Create prosodic neural circuit
    ProsodicNeuralCircuit circuit;
    circuit.pattern_name = pattern_name;
    circuit.pitch_neuron = circuit_neurons[0];
    circuit.energy_neuron = circuit_neurons[1];
    circuit.rhythm_neuron = circuit_neurons[2];
    circuit.integration_neuron = circuit_neurons[3];
    circuit.pattern_sensitivity = config_.prosodic_sensitivity;
    circuit.motherese_bias = 0.0f;
    circuit.is_motherese_detector = (template_features.motherese_score > config_.motherese_detection_threshold);
    
    // Setup circuit connections
    setupProsodicCircuitConnections(circuit);
    
    prosodic_circuits_[pattern_name] = circuit;
    
    std::cout << "Created prosodic neural circuit for: " << pattern_name << std::endl;
    return true;
}

bool NeuralLanguageBindings::configureMothereseBias(const std::string& pattern_name, float bias_strength) {
    std::lock_guard<std::mutex> lock(circuits_mutex_);
    
    auto it = prosodic_circuits_.find(pattern_name);
    if (it != prosodic_circuits_.end()) {
        it->second.motherese_bias = bias_strength;
        it->second.is_motherese_detector = (bias_strength > 0.0f);
        return true;
    }
    return false;
}

bool NeuralLanguageBindings::activateProsodicCircuit(const std::string& pattern_name,
                                                    const LanguageSystem::AcousticFeatures& features) {
    std::lock_guard<std::mutex> lock(circuits_mutex_);
    
    auto it = prosodic_circuits_.find(pattern_name);
    if (it != prosodic_circuits_.end()) {
        // Activate the circuit based on acoustic features
        // This would involve setting neuron activations based on the features
        return true;
    }
    return false;
}

bool NeuralLanguageBindings::strengthenCrossModalBinding(std::size_t grounding_id, float strength_boost) {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    auto it = cross_modal_bindings_.find(grounding_id);
    if (it == cross_modal_bindings_.end()) {
        return false;
    }
    
    CrossModalNeuralBinding& binding = it->second;
    
    // Increase binding strength
    binding.binding_strength += strength_boost;
    binding.binding_strength = std::min(1.0f, binding.binding_strength);
    
    // Strengthen synaptic connections
    this->strengthenSynapses(binding.binding_synapses, strength_boost);
    
    // Check for stabilization
    if (binding.binding_strength >= config_.cross_modal_binding_threshold && !binding.is_stable_binding) {
        stabilizeCrossModalBinding(grounding_id);
    }
    
    return true;
}

bool NeuralLanguageBindings::stabilizeCrossModalBinding(std::size_t grounding_id) {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    auto it = cross_modal_bindings_.find(grounding_id);
    if (it != cross_modal_bindings_.end()) {
        if (it->second.binding_strength >= 0.7f) {
            it->second.is_stable_binding = true;
            return true;
        }
    }
    return false;
}

NeuralLanguageBindings::CrossModalNeuralBinding* NeuralLanguageBindings::getCrossModalBinding(std::size_t grounding_id) {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    auto it = cross_modal_bindings_.find(grounding_id);
    if (it != cross_modal_bindings_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::size_t> NeuralLanguageBindings::getStableCrossModalBindings() const {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    std::vector<std::size_t> stable_bindings;
    for (const auto& [grounding_id, binding] : cross_modal_bindings_) {
        if (binding.is_stable_binding) {
            stable_bindings.push_back(grounding_id);
        }
    }
    return stable_bindings;
}

float NeuralLanguageBindings::calculateBindingStrength(const CrossModalNeuralBinding& binding) const {
    // Calculate overall binding strength based on modality strengths
    float total_strength = 0.0f;
    for (const auto& [modality, strength] : binding.modality_strengths) {
        total_strength += strength;
    }
    return total_strength / std::max(1.0f, static_cast<float>(binding.modality_strengths.size()));
}

bool NeuralLanguageBindings::createCrossModalNeuralBinding(std::size_t grounding_id,
                                                          const std::string& object_category,
                                                          const std::vector<float>& visual_features,
                                                          const std::vector<float>& auditory_features,
                                                          const std::vector<float>& tactile_features,
                                                          const std::vector<float>& language_features) {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    // Create cross-modal binding
    CrossModalNeuralBinding binding;
    binding.grounding_id = grounding_id;
    binding.object_category = object_category;
    binding.binding_strength = 0.5f; // Initial strength
    binding.is_stable_binding = false;
    
    // Allocate neurons for each modality
    auto visual_region = hypergraph_brain_->getModalityRegion(NeuroForge::Modality::Visual);
    auto auditory_region = hypergraph_brain_->getModalityRegion(NeuroForge::Modality::Audio);
    auto language_region = hypergraph_brain_->getModalityRegion(NeuroForge::Modality::Text);
    
    if (visual_region && !visual_features.empty()) {
        auto visual_neurons = allocateNeuralAssembly(visual_region->getId(), 1);
        if (!visual_neurons.empty()) {
            binding.visual_neuron = visual_neurons[0];
        }
    }
    
    if (auditory_region && !auditory_features.empty()) {
        auto auditory_neurons = allocateNeuralAssembly(auditory_region->getId(), 1);
        if (!auditory_neurons.empty()) {
            binding.auditory_neuron = auditory_neurons[0];
        }
    }
    
    if (language_region && !language_features.empty()) {
        auto language_neurons = allocateNeuralAssembly(language_region->getId(), 1);
        if (!language_neurons.empty()) {
            binding.language_neuron = language_neurons[0];
        }
    }
    
    // Allocate tactile and binding neurons from visual region for now
    if (visual_region && !tactile_features.empty()) {
        auto tactile_neurons = allocateNeuralAssembly(visual_region->getId(), 1);
        if (!tactile_neurons.empty()) {
            binding.tactile_neuron = tactile_neurons[0];
        }
    }
    
    if (visual_region) {
        auto binding_neurons = allocateNeuralAssembly(visual_region->getId(), 1);
        if (!binding_neurons.empty()) {
            binding.binding_neuron = binding_neurons[0];
        }
    }
    
    // Calculate modality strengths based on feature vectors
    if (!visual_features.empty()) {
        float visual_strength = std::accumulate(visual_features.begin(), visual_features.end(), 0.0f) / visual_features.size();
        binding.modality_strengths["visual"] = visual_strength;
    }
    
    if (!auditory_features.empty()) {
        float auditory_strength = std::accumulate(auditory_features.begin(), auditory_features.end(), 0.0f) / auditory_features.size();
        binding.modality_strengths["auditory"] = auditory_strength;
    }
    
    if (!tactile_features.empty()) {
        float tactile_strength = std::accumulate(tactile_features.begin(), tactile_features.end(), 0.0f) / tactile_features.size();
        binding.modality_strengths["tactile"] = tactile_strength;
    }
    
    if (!language_features.empty()) {
        float language_strength = std::accumulate(language_features.begin(), language_features.end(), 0.0f) / language_features.size();
        binding.modality_strengths["language"] = language_strength;
    }

    // Setup connections
    setupCrossModalBindingConnections(binding);
    
    // Store binding
    cross_modal_bindings_[grounding_id] = binding;
    
    return true;
}

// Additional missing methods
void NeuralLanguageBindings::applyNeuralLanguageLearning(float delta_time) {
    if (!learning_system_) {
        return;
    }
    
    // Apply language-specific plasticity
    applyLanguageSpecificPlasticity();
    
    // Update neural representations
    updateNeuralLanguageRepresentations();
    
    // Update statistics
    updateBindingStatistics();
}

void NeuralLanguageBindings::propagateLanguageActivations() {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    // Propagate activations through token assemblies
    for (auto& [symbol, assembly] : token_assemblies_) {
        if (assembly.assembly_coherence > config_.assembly_coherence_threshold) {
            // Activate connected assemblies and patterns
            activateNeurons(assembly.assembly_neurons, assembly.assembly_coherence);
        }
    }
    
    // Propagate activations through proto-word patterns
    for (auto& [pattern, neural_pattern] : proto_word_patterns_) {
        if (neural_pattern.is_crystallized) {
            // Activate pattern sequence
            activateNeurons(neural_pattern.sequence_neurons, neural_pattern.neural_stability);
            
            // Strongly activate pattern neuron
            std::vector<NeuroForge::NeuronID> pattern_neuron_vec = {neural_pattern.pattern_neuron};
            activateNeurons(pattern_neuron_vec, 1.0f);
        }
    }
    
    // Propagate activations through cross-modal bindings
    for (auto& [id, binding] : cross_modal_bindings_) {
        if (binding.is_stable_binding) {
            // Only activate neurons that have valid IDs (not 0)
            if (binding.visual_neuron != 0) {
                std::vector<NeuroForge::NeuronID> visual_vec = {binding.visual_neuron};
                activateNeurons(visual_vec, binding.binding_strength);
            }
            if (binding.auditory_neuron != 0) {
                std::vector<NeuroForge::NeuronID> auditory_vec = {binding.auditory_neuron};
                activateNeurons(auditory_vec, binding.binding_strength);
            }
            if (binding.language_neuron != 0) {
                std::vector<NeuroForge::NeuronID> language_vec = {binding.language_neuron};
                activateNeurons(language_vec, binding.binding_strength);
            }
        }
    }
}

void NeuralLanguageBindings::updateNeuralLanguageRepresentations() {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    // Update token assembly coherence
    for (auto& [symbol, assembly] : token_assemblies_) {
        assembly.assembly_coherence = calculateAssemblyCoherence(assembly);
    }
    
    // Update proto-word pattern stability
    for (auto& [pattern, neural_pattern] : proto_word_patterns_) {
        neural_pattern.neural_stability = calculatePatternStability(neural_pattern);
    }
    
    // Update cross-modal binding strength
    for (auto& [id, binding] : cross_modal_bindings_) {
        binding.binding_strength = calculateBindingStrength(binding);
    }
}

NeuroForge::NeuronID NeuralLanguageBindings::allocateNeuronForBinding(NeuroForge::RegionID region_id) {
    auto region = hypergraph_brain_->getRegion(region_id);
    if (!region) {
        return 0;
    }
    
    auto neurons = region->getNeurons();
    if (neurons.empty()) {
        return 0;
    }
    
    // Simple allocation: return first available neuron
    return neurons[0]->getId();
}

std::vector<NeuroForge::NeuronID> NeuralLanguageBindings::allocateNeuralAssembly(NeuroForge::RegionID region_id, std::size_t size) {
    auto region = hypergraph_brain_->getRegion(region_id);
    if (!region) {
        return {};
    }
    
    auto neurons = region->getNeurons();
    if (neurons.size() < size) {
        return {};
    }
    
    std::vector<NeuroForge::NeuronID> assembly;
    for (std::size_t i = 0; i < size && i < neurons.size(); ++i) {
        assembly.push_back(neurons[i]->getId());
    }
    
    return assembly;
}

NeuroForge::SynapseID NeuralLanguageBindings::createBindingSynapse(NeuroForge::NeuronID source, NeuroForge::NeuronID target,
                                                                  float weight, NeuroForge::SynapseType type) {
    // This would need to be implemented with proper region lookup
    // For now, return a placeholder
    return 0;
}

void NeuralLanguageBindings::setupTokenAssemblyConnections(TokenNeuralAssembly& assembly) {
    // Create internal connections within the assembly
    for (std::size_t i = 0; i < assembly.assembly_neurons.size(); ++i) {
        for (std::size_t j = i + 1; j < assembly.assembly_neurons.size(); ++j) {
            NeuroForge::SynapseID synapse_id = createBindingSynapse(
                assembly.assembly_neurons[i], assembly.assembly_neurons[j],
                0.5f, NeuroForge::SynapseType::Excitatory);
            
            if (synapse_id != 0) {
                assembly.internal_synapses.push_back(synapse_id);
            }
        }
    }
}

void NeuralLanguageBindings::setupProtoWordPatternConnections(ProtoWordNeuralPattern& pattern) {
    // Create sequential connections between phoneme neurons
    for (std::size_t i = 0; i < pattern.sequence_neurons.size() - 1; ++i) {
        NeuroForge::SynapseID synapse_id = createBindingSynapse(
            pattern.sequence_neurons[i], pattern.sequence_neurons[i + 1],
            0.7f, NeuroForge::SynapseType::Excitatory);
        
        if (synapse_id != 0) {
            pattern.sequence_synapses.push_back(synapse_id);
        }
    }
    
    // Connect all sequence neurons to pattern neuron
    for (NeuroForge::NeuronID sequence_neuron : pattern.sequence_neurons) {
        NeuroForge::SynapseID synapse_id = createBindingSynapse(
            sequence_neuron, pattern.pattern_neuron,
            0.6f, NeuroForge::SynapseType::Excitatory);
        
        if (synapse_id != 0) {
            pattern.sequence_synapses.push_back(synapse_id);
        }
    }
}

void NeuralLanguageBindings::setupProsodicCircuitConnections(ProsodicNeuralCircuit& circuit) {
    // Connect feature neurons to integration neuron
    std::vector<NeuroForge::NeuronID> feature_neurons = {
        circuit.pitch_neuron, circuit.energy_neuron, circuit.rhythm_neuron
    };
    
    for (NeuroForge::NeuronID feature_neuron : feature_neurons) {
        NeuroForge::SynapseID synapse_id = createBindingSynapse(
            feature_neuron, circuit.integration_neuron,
            0.8f, NeuroForge::SynapseType::Excitatory);
        
        if (synapse_id != 0) {
            circuit.circuit_synapses.push_back(synapse_id);
        }
    }
}

void NeuralLanguageBindings::setupCrossModalBindingConnections(CrossModalNeuralBinding& binding) {
    // Connect all modality neurons to each other
    std::vector<NeuroForge::NeuronID> all_neurons = {
        binding.visual_neuron,
        binding.auditory_neuron,
        binding.language_neuron,
        binding.tactile_neuron,
        binding.binding_neuron
    };
    
    for (std::size_t i = 0; i < all_neurons.size(); ++i) {
        for (std::size_t j = i + 1; j < all_neurons.size(); ++j) {
            // Skip connections involving invalid neuron IDs
            if (all_neurons[i] == 0 || all_neurons[j] == 0) {
                continue;
            }
            
            NeuroForge::SynapseID synapse_id = createBindingSynapse(
                all_neurons[i], all_neurons[j],
                0.6f, NeuroForge::SynapseType::Excitatory);
            
            if (synapse_id != 0) {
                binding.binding_synapses.push_back(synapse_id);
            }
        }
    }
}

float NeuralLanguageBindings::calculateAssemblyCoherence(const TokenNeuralAssembly& assembly) const {
    // Measure coherence based on neural activity correlation
    float total_activity = this->measureNeuralActivity(assembly.assembly_neurons);
    float synaptic_strength = this->measureSynapticStrength(assembly.internal_synapses);
    
    std::cout << "Debug: Assembly coherence calculation for '" << assembly.token_symbol << "'" << std::endl;
    std::cout << "  Neural activity: " << total_activity << std::endl;
    std::cout << "  Synaptic strength: " << synaptic_strength << std::endl;
    std::cout << "  Calculated coherence: " << (total_activity + synaptic_strength) / 2.0f << std::endl;
    
    return (total_activity + synaptic_strength) / 2.0f;
}

float NeuralLanguageBindings::calculatePatternStability(const ProtoWordNeuralPattern& pattern) const {
    // Measure stability based on sequential activation and synaptic strength
    float sequence_activity = this->measureNeuralActivity(pattern.sequence_neurons);
    float synaptic_strength = this->measureSynapticStrength(pattern.sequence_synapses);
    
    return (sequence_activity + synaptic_strength) / 2.0f;
}

void NeuralLanguageBindings::integrateWithLearningSystem() {
    if (!hypergraph_brain_) {
        return;
    }
    
    auto learning_system = hypergraph_brain_->getLearningSystem();
    if (learning_system) {
        learning_system_ = std::shared_ptr<LearningSystem>(learning_system, [](LearningSystem*){});
    }
}

void NeuralLanguageBindings::configureLanguageSpecificLearning() {
    if (!learning_system_) {
        return;
    }
    
    // Configure learning parameters for language processing
    LearningSystem::Config learning_config = learning_system_->getConfig();
    learning_config.global_learning_rate = config_.neural_learning_rate;
    learning_config.stdp_rate = config_.stdp_learning_rate;
    learning_config.hebbian_rate = config_.hebbian_learning_rate;
    learning_config.enable_attention_modulation = true;
    
    learning_system_->updateConfig(learning_config);
}

void NeuralLanguageBindings::applyLanguageSpecificPlasticity() {
    // Apply STDP to sequential patterns
    for (auto& [pattern, neural_pattern] : proto_word_patterns_) {
        if (!neural_pattern.sequence_synapses.empty()) {
            // Simulate spike times for STDP
            std::unordered_map<NeuroForge::NeuronID, NeuroForge::TimePoint> spike_times;
            auto current_time = std::chrono::steady_clock::now();
            
            for (std::size_t i = 0; i < neural_pattern.sequence_neurons.size(); ++i) {
                spike_times[neural_pattern.sequence_neurons[i]] = current_time + std::chrono::microseconds(i * 1000); // 1ms intervals
            }
            
            // Apply STDP learning (placeholder - would need actual synapse objects)
            // learning_system_->applySTDPLearning(synapses, spike_times);
        }
    }
    
    // Apply Hebbian learning to assemblies
    for (auto& [symbol, assembly] : token_assemblies_) {
        if (assembly.assembly_coherence > config_.assembly_coherence_threshold) {
            // Apply Hebbian learning (placeholder - would need region ID)
            // learning_system_->applyHebbianLearning(region_id, config_.hebbian_learning_rate);
        }
    }
}

void NeuralLanguageBindings::updateBindingStatistics() {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    
    statistics_.total_token_assemblies = token_assemblies_.size();
    statistics_.total_proto_word_patterns = proto_word_patterns_.size();
    statistics_.total_prosodic_circuits = prosodic_circuits_.size();
    statistics_.total_cross_modal_bindings = cross_modal_bindings_.size();
    
    // Count active/stable bindings
    statistics_.active_token_assemblies = 0;
    statistics_.crystallized_patterns = 0;
    statistics_.active_prosodic_circuits = 0;
    statistics_.stable_cross_modal_bindings = 0;
    
    for (const auto& [symbol, assembly] : token_assemblies_) {
        if (assembly.assembly_coherence >= config_.assembly_coherence_threshold) {
            statistics_.active_token_assemblies++;
        }
    }
    
    for (const auto& [pattern, neural_pattern] : proto_word_patterns_) {
        if (neural_pattern.is_crystallized) {
            statistics_.crystallized_patterns++;
        }
    }
    
    for (const auto& [name, circuit] : prosodic_circuits_) {
        if (circuit.pattern_sensitivity > config_.prosodic_sensitivity) {
            statistics_.active_prosodic_circuits++;
        }
    }
    
    for (const auto& [id, binding] : cross_modal_bindings_) {
        if (binding.is_stable_binding) {
            statistics_.stable_cross_modal_bindings++;
        }
    }
    
    // Calculate averages
    if (!token_assemblies_.empty()) {
        float total_coherence = 0.0f;
        std::cout << "Debug: Total token assemblies: " << token_assemblies_.size() << std::endl;
        for (const auto& [symbol, assembly] : token_assemblies_) {
            std::cout << "Debug: Assembly '" << symbol << "' coherence: " << assembly.assembly_coherence << std::endl;
            total_coherence += assembly.assembly_coherence;
        }
        std::cout << "Debug: Total coherence sum: " << total_coherence << std::endl;
        statistics_.average_assembly_coherence = total_coherence / token_assemblies_.size();
        std::cout << "Debug: Calculated average: " << statistics_.average_assembly_coherence << std::endl;
    }
    
    if (!proto_word_patterns_.empty()) {
        float total_stability = 0.0f;
        for (const auto& [pattern, neural_pattern] : proto_word_patterns_) {
            total_stability += neural_pattern.neural_stability;
        }
        statistics_.average_pattern_stability = total_stability / proto_word_patterns_.size();
    }
    
    if (!cross_modal_bindings_.empty()) {
        float total_strength = 0.0f;
        for (const auto& [id, binding] : cross_modal_bindings_) {
            total_strength += binding.binding_strength;
        }
        statistics_.average_binding_strength = total_strength / cross_modal_bindings_.size();
    }
    
    statistics_.neural_language_operations++;
}

NeuralLanguageBindings::Statistics NeuralLanguageBindings::getStatistics() const {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    return statistics_;
}

void NeuralLanguageBindings::resetStatistics() {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    statistics_ = Statistics{};
}

std::string NeuralLanguageBindings::generateBindingReport() const {
    auto stats = getStatistics();
    
    std::ostringstream report;
    report << "=== Neural Language Bindings Report ===" << std::endl;
    report << "Token Assemblies: " << stats.active_token_assemblies << "/" << stats.total_token_assemblies << std::endl;
    report << "Proto-word Patterns: " << stats.crystallized_patterns << "/" << stats.total_proto_word_patterns << std::endl;
    report << "Prosodic Circuits: " << stats.active_prosodic_circuits << "/" << stats.total_prosodic_circuits << std::endl;
    report << "Cross-modal Bindings: " << stats.stable_cross_modal_bindings << "/" << stats.total_cross_modal_bindings << std::endl;
    report << "Average Assembly Coherence: " << stats.average_assembly_coherence << std::endl;
    report << "Average Pattern Stability: " << stats.average_pattern_stability << std::endl;
    report << "Average Binding Strength: " << stats.average_binding_strength << std::endl;
    report << "Neural Language Operations: " << stats.neural_language_operations << std::endl;
    
    return report.str();
}

std::size_t NeuralLanguageBindings::getTotalBindings() const {
    return token_assemblies_.size() + proto_word_patterns_.size() + 
           prosodic_circuits_.size() + cross_modal_bindings_.size();
}

float NeuralLanguageBindings::getOverallBindingHealth() const {
    auto stats = getStatistics();
    
    if (stats.total_token_assemblies == 0) {
        return 0.0f;
    }
    
    float assembly_health = static_cast<float>(stats.active_token_assemblies) / stats.total_token_assemblies;
    float pattern_health = stats.total_proto_word_patterns > 0 ? 
        static_cast<float>(stats.crystallized_patterns) / stats.total_proto_word_patterns : 1.0f;
    float binding_health = stats.total_cross_modal_bindings > 0 ? 
        static_cast<float>(stats.stable_cross_modal_bindings) / stats.total_cross_modal_bindings : 1.0f;
    
    return (assembly_health + pattern_health + binding_health) / 3.0f;
}

void NeuralLanguageBindings::applySTDPToLanguageBindings(const std::unordered_map<NeuroForge::NeuronID, NeuroForge::TimePoint>& spike_times) {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    // Apply STDP to token assemblies
    for (auto& [symbol, assembly] : token_assemblies_) {
        for (std::size_t i = 0; i < assembly.internal_synapses.size(); ++i) {
            // Find pre and post synaptic neurons and apply STDP based on spike timing
            // This is a simplified implementation - in practice would use the learning system
            if (assembly.assembly_coherence > config_.assembly_coherence_threshold) {
                assembly.assembly_coherence = std::min(1.0f, assembly.assembly_coherence + 0.01f);
            }
        }
    }
    
    // Apply STDP to proto-word patterns
    for (auto& [pattern, neural_pattern] : proto_word_patterns_) {
        for (std::size_t i = 0; i < neural_pattern.sequence_synapses.size(); ++i) {
            if (neural_pattern.crystallization_strength > 0.5f) {
                neural_pattern.neural_stability = std::min(1.0f, neural_pattern.neural_stability + 0.005f);
            }
        }
    }
    
    // Apply STDP to cross-modal bindings
    for (auto& [id, binding] : cross_modal_bindings_) {
        for (std::size_t i = 0; i < binding.binding_synapses.size(); ++i) {
            if (binding.binding_strength > 0.6f) {
                binding.binding_strength = std::min(1.0f, binding.binding_strength + 0.002f);
            }
        }
    }
}

void NeuralLanguageBindings::applyHebbianToLanguageBindings(float learning_rate) {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    // Apply Hebbian learning to token assemblies
    for (auto& [symbol, assembly] : token_assemblies_) {
        if (assembly.firing_count > 0) {
            float hebbian_factor = learning_rate * (assembly.firing_count / 100.0f);
            assembly.assembly_coherence = std::min(1.0f, assembly.assembly_coherence + hebbian_factor);
        }
    }
    
    // Apply Hebbian learning to proto-word patterns
    for (auto& [pattern, neural_pattern] : proto_word_patterns_) {
        if (neural_pattern.reinforcement_count > 0) {
            float hebbian_factor = learning_rate * (neural_pattern.reinforcement_count / 50.0f);
            neural_pattern.crystallization_strength = std::min(1.0f, neural_pattern.crystallization_strength + hebbian_factor);
        }
    }
    
    // Apply Hebbian learning to cross-modal bindings
    for (auto& [id, binding] : cross_modal_bindings_) {
        float total_modality_strength = 0.0f;
        for (const auto& [modality, strength] : binding.modality_strengths) {
            total_modality_strength += strength;
        }
        if (total_modality_strength > 0.0f) {
            float hebbian_factor = learning_rate * (total_modality_strength / binding.modality_strengths.size());
            binding.binding_strength = std::min(1.0f, binding.binding_strength + hebbian_factor);
        }
    }
}

void NeuralLanguageBindings::consolidateLanguageBindings() {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    // Consolidate token assemblies
    for (auto& [symbol, assembly] : token_assemblies_) {
        // Always consolidate assemblies, regardless of coherence threshold
        assembly.activation_threshold *= 0.95f; // Make more sensitive
        assembly.firing_count = std::min(assembly.firing_count, static_cast<std::uint64_t>(1000)); // Cap firing count
        
        // Preserve activation state by re-activating neurons with current coherence
        if (assembly.assembly_coherence > 0.0f) {
            activateNeurons(assembly.assembly_neurons, assembly.assembly_coherence);
        }
        
        // Ensure coherence is maintained during consolidation
        float recalculated_coherence = calculateAssemblyCoherence(assembly);
        assembly.assembly_coherence = std::max(assembly.assembly_coherence, recalculated_coherence);
    }
    
    // Consolidate proto-word patterns
    for (auto& [pattern, neural_pattern] : proto_word_patterns_) {
        if (neural_pattern.crystallization_strength > 0.8f && !neural_pattern.is_crystallized) {
            neural_pattern.is_crystallized = true;
            neural_pattern.neural_stability = std::max(neural_pattern.neural_stability, 0.9f);
            
            // Preserve activation for crystallized patterns
            if (neural_pattern.neural_stability > 0.0f) {
                activateNeurons(neural_pattern.sequence_neurons, neural_pattern.neural_stability);
                std::vector<NeuroForge::NeuronID> pattern_neuron_vec = {neural_pattern.pattern_neuron};
                activateNeurons(pattern_neuron_vec, 1.0f);
            }
        }
    }
    
    // Consolidate cross-modal bindings
    for (auto& [id, binding] : cross_modal_bindings_) {
        if (binding.binding_strength > 0.7f && !binding.is_stable_binding) {
            binding.is_stable_binding = true;
            // Strengthen all modality connections
            for (auto& [modality, strength] : binding.modality_strengths) {
                strength = std::max(strength, 0.8f);
            }
        }
    }
    
    updateBindingStatistics();
}

void NeuralLanguageBindings::pruneInactiveBindings(float inactivity_threshold) {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    auto current_time = std::chrono::steady_clock::now();
    
    // Prune inactive token assemblies
    auto token_it = token_assemblies_.begin();
    while (token_it != token_assemblies_.end()) {
        auto& assembly = token_it->second;
        auto time_since_firing = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - assembly.last_firing).count();
        
        // Before pruning, check if assembly has been recently activated or has good coherence
        bool should_preserve = (assembly.assembly_coherence >= inactivity_threshold && 
                               time_since_firing <= 3600) || // 1 hour
                              assembly.firing_count > 0; // Has been activated at least once
        
        // Additional protection for recently created assemblies (within last 10 seconds)
        bool is_recently_created = time_since_firing <= 10;
        
        if (!should_preserve && !is_recently_created) {
            token_it = token_assemblies_.erase(token_it);
        } else {
            // Preserve activation state for kept assemblies
            if (assembly.assembly_coherence > 0.0f) {
                activateNeurons(assembly.assembly_neurons, assembly.assembly_coherence);
                
                // Recalculate coherence after reactivation to ensure it's accurate
                float recalculated_coherence = calculateAssemblyCoherence(assembly);
                assembly.assembly_coherence = std::max(assembly.assembly_coherence, recalculated_coherence);
            }
            ++token_it;
        }
    }
    
    // Prune inactive proto-word patterns
    auto pattern_it = proto_word_patterns_.begin();
    while (pattern_it != proto_word_patterns_.end()) {
        auto& pattern = pattern_it->second;
        
        // Keep crystallized patterns and patterns with good stability
        bool should_preserve = pattern.is_crystallized || 
                              pattern.crystallization_strength >= inactivity_threshold ||
                              pattern.neural_stability >= inactivity_threshold;
        
        if (!should_preserve) {
            pattern_it = proto_word_patterns_.erase(pattern_it);
        } else {
            // Preserve activation state for kept patterns
            if (pattern.neural_stability > 0.0f) {
                activateNeurons(pattern.sequence_neurons, pattern.neural_stability);
                if (pattern.is_crystallized) {
                    std::vector<NeuroForge::NeuronID> pattern_neuron_vec = {pattern.pattern_neuron};
                    activateNeurons(pattern_neuron_vec, 1.0f);
                }
            }
            ++pattern_it;
        }
    }
    
    // Prune inactive cross-modal bindings
    auto binding_it = cross_modal_bindings_.begin();
    while (binding_it != cross_modal_bindings_.end()) {
        auto& binding = binding_it->second;
        if (binding.binding_strength < inactivity_threshold && !binding.is_stable_binding) {
            binding_it = cross_modal_bindings_.erase(binding_it);
        } else {
            ++binding_it;
        }
    }
    
    updateBindingStatistics();
}

void NeuralLanguageBindings::modulateLanguageLearning(const std::unordered_map<NeuroForge::NeuronID, float>& attention_weights) {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    if (!learning_system_) {
        return;
    }
    
    // Apply attention-modulated learning to token assemblies
    for (auto& [symbol, assembly] : token_assemblies_) {
        float total_attention = 0.0f;
        int attended_neurons = 0;
        
        // Calculate average attention for assembly neurons
        for (NeuroForge::NeuronID neuron_id : assembly.assembly_neurons) {
            auto it = attention_weights.find(neuron_id);
            if (it != attention_weights.end()) {
                total_attention += it->second;
                attended_neurons++;
            }
        }
        
        if (attended_neurons > 0) {
            float avg_attention = total_attention / attended_neurons;
            // Modulate assembly coherence based on attention
            assembly.assembly_coherence = std::min(1.0f, assembly.assembly_coherence * (1.0f + avg_attention));
        }
    }
    
    // Apply attention modulation to proto-word patterns
    for (auto& [pattern, neural_pattern] : proto_word_patterns_) {
        float pattern_attention = 0.0f;
        int attended_neurons = 0;
        
        for (NeuroForge::NeuronID neuron_id : neural_pattern.sequence_neurons) {
            auto it = attention_weights.find(neuron_id);
            if (it != attention_weights.end()) {
                pattern_attention += it->second;
                attended_neurons++;
            }
        }
        
        if (attended_neurons > 0) {
            float avg_attention = pattern_attention / attended_neurons;
            neural_pattern.crystallization_strength = std::min(1.0f, 
                neural_pattern.crystallization_strength * (1.0f + avg_attention * 0.5f));
        }
    }
}

void NeuralLanguageBindings::optimizeNeuralBindings() {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    auto current_time = std::chrono::steady_clock::now();
    
    // Optimize token assemblies
    for (auto& [symbol, assembly] : token_assemblies_) {
        // Check if assembly was recently activated (within last 30 seconds)
        auto time_since_firing = std::chrono::duration_cast<std::chrono::seconds>(
            current_time - assembly.last_firing).count();
        bool is_recently_active = time_since_firing <= 30;
        
        // Only prune weak connections if assembly hasn't been recently activated
        if (assembly.assembly_coherence < config_.assembly_coherence_threshold * 0.5f && !is_recently_active) {
            // Reduce assembly size by removing weakest neurons, but preserve minimum size
            if (assembly.assembly_neurons.size() > 5) {  // Increased minimum size
                assembly.assembly_neurons.resize(assembly.assembly_neurons.size() - 1);
            }
        }
        
        // Strengthen coherent assemblies
        if (assembly.assembly_coherence > config_.assembly_coherence_threshold * 1.5f) {
            assembly.assembly_coherence = std::min(1.0f, assembly.assembly_coherence * 1.1f);
        }
        
        // For recently activated assemblies, ensure they maintain good coherence
        if (is_recently_active && assembly.assembly_coherence < config_.assembly_coherence_threshold) {
            assembly.assembly_coherence = std::max(assembly.assembly_coherence, config_.assembly_coherence_threshold);
        }
    }
    
    // Optimize proto-word patterns
    for (auto& [pattern, neural_pattern] : proto_word_patterns_) {
        // Crystallize stable patterns
        if (neural_pattern.crystallization_strength > 0.8f && !neural_pattern.is_crystallized) {
            neural_pattern.is_crystallized = true;
            neural_pattern.neural_stability = std::min(1.0f, neural_pattern.neural_stability * 1.2f);
        }
        
        // Prune unstable patterns
        if (neural_pattern.crystallization_strength < 0.2f) {
            neural_pattern.sequence_neurons.clear();
            neural_pattern.sequence_synapses.clear();
        }
    }
    
    // Optimize cross-modal bindings
    for (auto& [id, binding] : cross_modal_bindings_) {
        // Strengthen stable bindings
        if (binding.binding_strength > 0.7f) {
            binding.is_stable_binding = true;
            for (auto& [modality, strength] : binding.modality_strengths) {
                strength = std::min(1.0f, strength * 1.1f);
            }
        }
        
        // Mark weak bindings for potential pruning
        if (binding.binding_strength < 0.3f) {
            binding.is_stable_binding = false;
        }
    }
    
    // Update statistics after optimization
    updateBindingStatistics();
}

// Prosodic circuit operations

void NeuralLanguageBindings::activateNeurons(const std::vector<NeuroForge::NeuronID>& neurons, float activation) {
    if (!hypergraph_brain_) {
        return;
    }
    
    // Get all regions to find neurons
    auto regions = hypergraph_brain_->getRegions();
    
    for (NeuroForge::NeuronID neuron_id : neurons) {
        // Skip invalid neuron IDs (0 is typically invalid)
        if (neuron_id == 0) {
            continue;
        }
        
        // Find the neuron in any region
        for (const auto& [region_id, region] : regions) {
            if (region) {
                auto neuron = region->getNeuron(neuron_id);
                if (neuron) {
                    // Set the neuron's activation level
                    neuron->setActivation(activation);
                    break; // Found the neuron, move to next
                }
            }
        }
    }
}

void NeuralLanguageBindings::strengthenSynapses(const std::vector<NeuroForge::SynapseID>& synapses, float factor) {
    if (!hypergraph_brain_) {
        return;
    }
    
    // Get all regions to find synapses
    auto regions = hypergraph_brain_->getRegions();
    
    for (NeuroForge::SynapseID synapse_id : synapses) {
        // Find the synapse in any region
        for (const auto& [region_id, region] : regions) {
            if (region) {
                auto synapse = region->getSynapse(synapse_id);
                if (synapse) {
                    // Strengthen the synapse by multiplying weight by factor
                    float current_weight = synapse->getWeight();
                    float new_weight = std::min(1.0f, current_weight * factor); // Cap at 1.0
                    synapse->setWeight(new_weight);
                    break; // Found the synapse, move to next
                }
            }
        }
    }
}

float NeuralLanguageBindings::measureNeuralActivity(const std::vector<NeuroForge::NeuronID>& neurons) const {
    if (!hypergraph_brain_ || neurons.empty()) {
        return 0.0f;
    }
    
    // Get all regions to find neurons
    auto regions = hypergraph_brain_->getRegions();
    
    float total_activation = 0.0f;
    std::size_t found_neurons = 0;
    
    for (NeuroForge::NeuronID neuron_id : neurons) {
        // Skip invalid neuron IDs
        if (neuron_id == 0) {
            continue;
        }
        
        // Find the neuron in any region
        for (const auto& [region_id, region] : regions) {
            if (region) {
                auto neuron = region->getNeuron(neuron_id);
                if (neuron) {
                    total_activation += neuron->getActivation();
                    found_neurons++;
                    break; // Found the neuron, move to next
                }
            }
        }
    }
    
    // Return average activation level
    return found_neurons > 0 ? (total_activation / found_neurons) : 0.0f;
}

float NeuralLanguageBindings::measureSynapticStrength(const std::vector<NeuroForge::SynapseID>& synapses) const {
    if (!hypergraph_brain_ || synapses.empty()) {
        return 0.0f;
    }
    
    // Get all regions to find synapses
    auto regions = hypergraph_brain_->getRegions();
    
    float total_weight = 0.0f;
    std::size_t found_synapses = 0;
    
    for (NeuroForge::SynapseID synapse_id : synapses) {
        // Find the synapse in any region
        for (const auto& [region_id, region] : regions) {
            if (region) {
                auto synapse = region->getSynapse(synapse_id);
                if (synapse) {
                    total_weight += synapse->getWeight();
                    found_synapses++;
                    break; // Found the synapse, move to next
                }
            }
        }
    }
    
    // Return average synaptic weight
    return found_synapses > 0 ? (total_weight / found_synapses) : 0.0f;
}

NeuralLanguageBindings::ProsodicNeuralCircuit* NeuralLanguageBindings::getProsodicCircuit(const std::string& pattern_name) {
    std::lock_guard<std::mutex> lock(circuits_mutex_);
    
    auto it = prosodic_circuits_.find(pattern_name);
    if (it != prosodic_circuits_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> NeuralLanguageBindings::detectActiveProsodicPatterns(float sensitivity_threshold) const {
    std::lock_guard<std::mutex> lock(circuits_mutex_);
    
    std::vector<std::string> active_patterns;
    
    for (const auto& [pattern_name, circuit] : prosodic_circuits_) {
        // Check if the circuit's integration neuron is active above threshold
        std::vector<NeuroForge::NeuronID> integration_neuron_vec = {circuit.integration_neuron};
        float activity = measureNeuralActivity(integration_neuron_vec);
        
        if (activity >= sensitivity_threshold) {
            active_patterns.push_back(pattern_name);
        }
    }
    
    return active_patterns;
}

std::vector<std::string> NeuralLanguageBindings::getCrystallizedProtoWords() const {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    std::vector<std::string> crystallized_patterns;
    
    for (const auto& [pattern, neural_pattern] : proto_word_patterns_) {
        if (neural_pattern.crystallization_strength >= config_.crystallization_threshold) {
            crystallized_patterns.push_back(pattern);
        }
    }
    
    return crystallized_patterns;
}

std::vector<std::string> NeuralLanguageBindings::getActiveTokenAssemblies(float coherence_threshold) const {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    std::vector<std::string> active_assemblies;
    
    for (const auto& [symbol, assembly] : token_assemblies_) {
        if (assembly.assembly_coherence >= coherence_threshold) {
            active_assemblies.push_back(symbol);
        }
    }
    
    return active_assemblies;
}

std::vector<std::string> NeuralLanguageBindings::getStableProtoWordPatterns(float stability_threshold) const {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    std::vector<std::string> stable_patterns;
    
    for (const auto& [pattern, neural_pattern] : proto_word_patterns_) {
        float stability = calculatePatternStability(neural_pattern);
        if (stability >= stability_threshold) {
            stable_patterns.push_back(pattern);
        }
    }
    
    return stable_patterns;
}

NeuralLanguageBindings::ProtoWordNeuralPattern* NeuralLanguageBindings::getProtoWordPattern(const std::string& pattern) {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    auto it = proto_word_patterns_.find(pattern);
    if (it != proto_word_patterns_.end()) {
        return &(it->second);
    }
    
    return nullptr;
}

NeuralLanguageBindings::TokenNeuralAssembly* NeuralLanguageBindings::getTokenAssembly(const std::string& token_symbol) {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    auto it = token_assemblies_.find(token_symbol);
    if (it != token_assemblies_.end()) {
        return &(it->second);
    }
    
    return nullptr;
}

bool NeuralLanguageBindings::reinforceTokenAssembly(const std::string& token_symbol, float reinforcement_strength) {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    auto it = token_assemblies_.find(token_symbol);
    if (it != token_assemblies_.end()) {
        TokenNeuralAssembly& assembly = it->second;
        
        // Increase assembly coherence based on reinforcement
        assembly.assembly_coherence += reinforcement_strength * 0.1f;
        if (assembly.assembly_coherence > 1.0f) {
            assembly.assembly_coherence = 1.0f;
        }
        
        // Increment firing count as a form of reinforcement
        assembly.firing_count++;
        
        return true;
    }
    
    return false;
}

std::vector<std::string> NeuralLanguageBindings::getActiveTokens(float coherence_threshold) const {
    std::lock_guard<std::recursive_mutex> lock(bindings_mutex_);
    
    std::vector<std::string> active_tokens;
    
    for (const auto& [symbol, assembly] : token_assemblies_) {
        if (assembly.assembly_coherence >= coherence_threshold) {
            active_tokens.push_back(symbol);
        }
    }
    
    return active_tokens;
}

} // namespace Core
} // namespace NeuroForge