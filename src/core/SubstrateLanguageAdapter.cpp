#include "core/SubstrateLanguageAdapter.h"
#include "core/Region.h"
#include "core/Neuron.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace NeuroForge {
namespace Core {

SubstrateLanguageAdapter::SubstrateLanguageAdapter(std::shared_ptr<HypergraphBrain> brain,
                                                 std::shared_ptr<LanguageSystem> language_system,
                                                 const Config& config)
    : config_(config)
    , brain_(brain)
    , language_system_(language_system) {
    
    activation_history_.reserve(history_window_size_);
}

bool SubstrateLanguageAdapter::initialize() {
    if (!brain_ || !language_system_) {
        return false;
    }
    
    // Initialize statistics
    stats_ = Statistics{};
    
    return true;
}

void SubstrateLanguageAdapter::shutdown() {
    discovered_assemblies_.clear();
    assembly_lookup_.clear();
    activation_history_.clear();
}

void SubstrateLanguageAdapter::processSubstrateActivations(float delta_time) {
    // Get current neural activations from substrate
    auto current_activations = getCurrentActivations();
    
    // Add to activation history
    activation_history_.push_back(current_activations);
    if (activation_history_.size() > history_window_size_) {
        activation_history_.erase(activation_history_.begin());
    }
    
    // Update stability of existing assemblies
    updateAssemblyStabilities(delta_time);
    
    // Detect new stable assemblies
    auto new_assemblies = detectStableAssemblies();
    
    // Process novel assemblies and create tokens
    std::size_t tokens_created_this_cycle = 0;
    for (const auto& assembly : new_assemblies) {
        if (tokens_created_this_cycle >= config_.max_tokens_per_cycle) {
            break;
        }
        
        if (isNovelAssembly(assembly)) {
            // Generate token for this assembly
            std::string token_symbol = generateTokenForAssembly(assembly);
            
            // Create token in language system
            std::vector<float> assembly_embedding = assembly.activation_pattern;
            std::size_t token_id = language_system_->createToken(
                token_symbol, 
                LanguageSystem::TokenType::Word, 
                assembly_embedding
            );
            
            // Associate token with neurons in assembly
            for (NeuroForge::NeuronID neuron_id : assembly.neurons) {
                language_system_->associateTokenWithNeuron(token_id, neuron_id, 1.0f);
            }
            
            // Add to discovered assemblies
            NeuralAssembly stored_assembly = assembly;
            stored_assembly.generated_token = token_symbol;
            discovered_assemblies_.push_back(stored_assembly);
            assembly_lookup_[token_symbol] = discovered_assemblies_.size() - 1;
            
            tokens_created_this_cycle++;
            stats_.tokens_created++;
        }
    }
    
    // Prune stale assemblies
    pruneStaleAssemblies();
    
    // Update statistics
    updateStatistics();
    stats_.processing_cycles++;
}

std::vector<SubstrateLanguageAdapter::NeuralAssembly> SubstrateLanguageAdapter::detectStableAssemblies() {
    std::vector<NeuralAssembly> assemblies;
    
    if (activation_history_.size() < 3) {
        return assemblies; // Need sufficient history
    }
    
    // Enhanced assembly detection with improved pattern recognition
    std::unordered_map<std::string, std::vector<std::size_t>> coactivation_patterns;
    std::unordered_map<std::string, std::vector<std::vector<float>>> pattern_activations;
    
    for (std::size_t t = 0; t < activation_history_.size(); ++t) {
        const auto& activations = activation_history_[t];
        
        // Find highly active neurons in this time step
        std::vector<NeuroForge::NeuronID> active_neurons;
        std::vector<float> active_values;
        for (const auto& [neuron_id, activation] : activations) {
            if (activation > 0.5f) { // Threshold for "active"
                active_neurons.push_back(neuron_id);
                active_values.push_back(activation);
            }
        }
        
        // Generate patterns for groups of co-active neurons
        if (active_neurons.size() >= config_.min_assembly_size) {
            // Sort neurons and corresponding values together
            std::vector<std::pair<NeuroForge::NeuronID, float>> neuron_pairs;
            for (std::size_t i = 0; i < active_neurons.size(); ++i) {
                neuron_pairs.emplace_back(active_neurons[i], active_values[i]);
            }
            std::sort(neuron_pairs.begin(), neuron_pairs.end());
            
            // Create pattern key
            std::ostringstream pattern_key;
            std::vector<float> step_activations;
            for (std::size_t i = 0; i < neuron_pairs.size(); ++i) {
                if (i > 0) pattern_key << "_";
                pattern_key << neuron_pairs[i].first;
                step_activations.push_back(neuron_pairs[i].second);
            }
            
            std::string key = pattern_key.str();
            coactivation_patterns[key].push_back(t);
            
            // Store activation values for variance analysis
            if (pattern_activations.find(key) == pattern_activations.end()) {
                pattern_activations[key] = std::vector<std::vector<float>>();
            }
            pattern_activations[key].push_back(step_activations);
        }
    }
    
    // Identify stable patterns with enhanced validation
    for (const auto& [pattern_key, time_steps] : coactivation_patterns) {
        if (time_steps.size() >= 2) { // Appeared at least twice
            NeuralAssembly assembly;
            
            // Parse neuron IDs from pattern key
            std::istringstream iss(pattern_key);
            std::string neuron_str;
            while (std::getline(iss, neuron_str, '_')) {
                assembly.neurons.push_back(std::stoull(neuron_str));
            }
            
            // Enhanced activation pattern calculation with variance analysis
            assembly.activation_pattern.resize(assembly.neurons.size(), 0.0f);
            std::vector<float> activation_variance(assembly.neurons.size(), 0.0f);
            
            const auto& activations = pattern_activations[pattern_key];
            if (!activations.empty()) {
                // Calculate mean activations
                for (const auto& step_activations : activations) {
                    for (std::size_t i = 0; i < std::min(assembly.activation_pattern.size(), step_activations.size()); ++i) {
                        assembly.activation_pattern[i] += step_activations[i];
                    }
                }
                
                // Normalize to get mean
                for (float& activation : assembly.activation_pattern) {
                    activation /= static_cast<float>(activations.size());
                }
                
                // Calculate variance for stability assessment
                for (const auto& step_activations : activations) {
                    for (std::size_t i = 0; i < std::min(activation_variance.size(), step_activations.size()); ++i) {
                        float diff = step_activations[i] - assembly.activation_pattern[i];
                        activation_variance[i] += diff * diff;
                    }
                }
                
                // Normalize variance
                for (float& variance : activation_variance) {
                    variance /= static_cast<float>(activations.size());
                }
                
                // Enhanced stability score incorporating pattern consistency
                float pattern_consistency = 0.0f;
                for (float variance : activation_variance) {
                    pattern_consistency += 1.0f / (1.0f + variance); // Lower variance = higher consistency
                }
                pattern_consistency /= static_cast<float>(activation_variance.size());
                
                float temporal_stability = static_cast<float>(time_steps.size()) / 
                                         static_cast<float>(activation_history_.size());
                
                // Combined stability score
                assembly.stability_score = 0.6f * temporal_stability + 0.4f * pattern_consistency;
                assembly.occurrence_count = time_steps.size();
                assembly.last_seen = std::chrono::steady_clock::now();
                
                // Enhanced threshold with pattern quality consideration
                float quality_threshold = config_.stability_threshold * 
                                        (0.8f + 0.2f * pattern_consistency);
                
                if (assembly.stability_score >= quality_threshold) {
                    assemblies.push_back(assembly);
                }
            }
        }
    }
    
    // Sort assemblies by stability score (most stable first)
    std::sort(assemblies.begin(), assemblies.end(),
        [](const NeuralAssembly& a, const NeuralAssembly& b) {
            return a.stability_score > b.stability_score;
        });
    
    // Limit number of assemblies to prevent overflow
    if (assemblies.size() > 50) { // Reasonable default limit
        assemblies.resize(50);
    }
    
    stats_.assemblies_discovered += assemblies.size();
    return assemblies;
}

std::string SubstrateLanguageAdapter::generateTokenForAssembly(const NeuralAssembly& assembly) {
    return generateUniqueTokenSymbol();
}

bool SubstrateLanguageAdapter::isNovelAssembly(const NeuralAssembly& assembly) const {
    // Check similarity against existing assemblies
    for (const auto& existing : discovered_assemblies_) {
        float similarity = calculateAssemblySimilarity(assembly, existing);
        if (similarity > (1.0f - config_.novelty_threshold)) {
            return false; // Too similar to existing assembly
        }
    }
    return true;
}

void SubstrateLanguageAdapter::updateAssemblyStabilities(float delta_time) {
    auto now = std::chrono::steady_clock::now();
    
    for (auto& assembly : discovered_assemblies_) {
        // Decay stability over time
        assembly.stability_score *= config_.decay_rate;
        
        // Check if assembly is currently active
        auto current_activations = getCurrentActivations();
        bool is_active = true;
        
        for (NeuroForge::NeuronID neuron_id : assembly.neurons) {
            auto it = std::find_if(current_activations.begin(), current_activations.end(),
                [neuron_id](const auto& pair) { return pair.first == neuron_id; });
            if (it == current_activations.end() || it->second < 0.3f) {
                is_active = false;
                break;
            }
        }
        
        if (is_active) {
            // Boost stability if currently active
            assembly.stability_score = std::min(1.0f, assembly.stability_score + 0.1f);
            assembly.last_seen = now;
            assembly.occurrence_count++;
        }
    }
}

std::vector<std::pair<NeuroForge::NeuronID, float>> SubstrateLanguageAdapter::getCurrentActivations() {
    std::vector<std::pair<NeuroForge::NeuronID, float>> activations;
    
    if (!brain_) return activations;
    
    const auto& regions_map = brain_->getRegionsMap();
    for (const auto& [region_id, region] : regions_map) {
        if (!region) continue;
        
        const auto& neurons = region->getNeurons();
        for (const auto& neuron : neurons) {
            if (!neuron) continue;
            
            float activation = neuron->getActivation();
            if (activation > 0.1f) { // Only include significantly active neurons
                activations.emplace_back(neuron->getId(), activation);
            }
        }
    }
    
    return activations;
}

float SubstrateLanguageAdapter::calculateAssemblyStability(const NeuralAssembly& assembly) const {
    return assembly.stability_score;
}

float SubstrateLanguageAdapter::calculateAssemblySimilarity(const NeuralAssembly& a, const NeuralAssembly& b) const {
    // Calculate Jaccard similarity of neuron sets
    std::unordered_set<NeuroForge::NeuronID> set_a(a.neurons.begin(), a.neurons.end());
    std::unordered_set<NeuroForge::NeuronID> set_b(b.neurons.begin(), b.neurons.end());
    
    std::size_t intersection = 0;
    for (NeuroForge::NeuronID neuron : set_a) {
        if (set_b.count(neuron)) {
            intersection++;
        }
    }
    
    std::size_t union_size = set_a.size() + set_b.size() - intersection;
    if (union_size == 0) return 0.0f;
    
    return static_cast<float>(intersection) / static_cast<float>(union_size);
}

std::string SubstrateLanguageAdapter::generateUniqueTokenSymbol() {
    std::size_t counter = token_counter_.fetch_add(1);
    std::ostringstream oss;
    oss << "sub_" << std::setfill('0') << std::setw(4) << counter;
    return oss.str();
}

void SubstrateLanguageAdapter::pruneStaleAssemblies() {
    auto now = std::chrono::steady_clock::now();
    auto threshold = std::chrono::seconds(30); // Remove assemblies not seen for 30 seconds
    
    discovered_assemblies_.erase(
        std::remove_if(discovered_assemblies_.begin(), discovered_assemblies_.end(),
            [now, threshold](const NeuralAssembly& assembly) {
                return (now - assembly.last_seen) > threshold || 
                       assembly.stability_score < 0.1f;
            }),
        discovered_assemblies_.end()
    );
    
    // Rebuild lookup map
    assembly_lookup_.clear();
    for (std::size_t i = 0; i < discovered_assemblies_.size(); ++i) {
        assembly_lookup_[discovered_assemblies_[i].generated_token] = i;
    }
}

void SubstrateLanguageAdapter::updateStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.stable_assemblies = 0;
    float total_stability = 0.0f;
    
    for (const auto& assembly : discovered_assemblies_) {
        if (assembly.stability_score >= config_.stability_threshold) {
            stats_.stable_assemblies++;
        }
        total_stability += assembly.stability_score;
    }
    
    if (!discovered_assemblies_.empty()) {
        stats_.average_stability = total_stability / static_cast<float>(discovered_assemblies_.size());
    }
}

SubstrateLanguageAdapter::Statistics SubstrateLanguageAdapter::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

} // namespace Core
} // namespace NeuroForge