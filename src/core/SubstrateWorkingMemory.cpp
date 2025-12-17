#include "core/SubstrateWorkingMemory.h"
#include "core/Region.h"
#include "core/Neuron.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>

namespace NeuroForge {
namespace Core {

SubstrateWorkingMemory::SubstrateWorkingMemory(std::shared_ptr<HypergraphBrain> brain, const Config& config)
    : config_(config)
    , brain_(brain) {
}

bool SubstrateWorkingMemory::initialize() {
    if (!brain_) {
        return false;
    }

    // Create working memory regions
    for (std::size_t i = 0; i < config_.working_memory_regions; ++i) {
        std::string region_name = "WM_Region_" + std::to_string(i);
        auto region = brain_->createRegion(region_name, Region::Type::Custom, Region::ActivationPattern::Synchronous);
        
        if (!region) {
            std::cerr << "[SubstrateWorkingMemory] Failed to create region: " << region_name << std::endl;
            return false;
        }

        // Add neurons to the region using factory-based creation to ensure global ID uniqueness
        region->createNeurons(config_.neurons_per_region);

        wm_regions_.push_back(region->getId());
    }

    // Create specialized binding regions
    for (std::size_t i = 0; i < 2; ++i) { // Role and Filler regions
        std::string region_name = (i == 0) ? "Binding_Roles" : "Binding_Fillers";
        auto region = brain_->createRegion(region_name, Region::Type::Custom, Region::ActivationPattern::Synchronous);
        
        if (!region) {
            std::cerr << "[SubstrateWorkingMemory] Failed to create binding region: " << region_name << std::endl;
            return false;
        }

        // Add neurons for binding representations using factory-based creation
        region->createNeurons(config_.neurons_per_region);

        binding_regions_.push_back(region->getId());
    }

    // Create sequence processing regions
    for (std::size_t i = 0; i < 3; ++i) { // Current, Previous, Predicted
        std::string region_name = "Sequence_" + std::to_string(i);
        auto region = brain_->createRegion(region_name, Region::Type::Custom, Region::ActivationPattern::Synchronous);
        
        if (!region) {
            std::cerr << "[SubstrateWorkingMemory] Failed to create sequence region: " << region_name << std::endl;
            return false;
        }

        // Add neurons for sequence representations using factory-based creation
        region->createNeurons(config_.neurons_per_region);

        sequence_regions_.push_back(region->getId());
    }

    // Initialize statistics
    stats_ = Statistics{};
    stats_.total_regions = wm_regions_.size() + binding_regions_.size() + sequence_regions_.size();

    std::cout << "[SubstrateWorkingMemory] Initialized with " << stats_.total_regions << " regions" << std::endl;
    return true;
}

void SubstrateWorkingMemory::shutdown() {
    active_bindings_.clear();
    wm_regions_.clear();
    binding_regions_.clear();
    sequence_regions_.clear();
}

void SubstrateWorkingMemory::processStep(float delta_time) {
    // Apply maintenance current to keep working memory active
    applyMaintenance();

    // Apply decay to working memory representations
    applyDecay(delta_time);

    // Update binding strengths based on neural activity
    updateBindingStrengths();

    // Prune weak bindings
    pruneWeakBindings();

    // Update statistics
    updateStatistics();
    stats_.maintenance_cycles++;
}

bool SubstrateWorkingMemory::createBinding(const std::string& role, const std::string& filler, float strength) {
    if (binding_regions_.size() < 2) {
        return false;
    }

    // Check if we're at capacity
    if (active_bindings_.size() >= config_.max_binding_capacity) {
        // Remove weakest binding
        auto weakest = std::min_element(active_bindings_.begin(), active_bindings_.end(),
            [](const SubstrateBinding& a, const SubstrateBinding& b) {
                return a.strength < b.strength;
            });
        if (weakest != active_bindings_.end()) {
            active_bindings_.erase(weakest);
        }
    }

    // Create new substrate binding
    SubstrateBinding binding;
    binding.role_region = binding_regions_[0];
    binding.filler_region = binding_regions_[1];
    binding.strength = std::clamp(strength, 0.0f, 1.0f);
    binding.role_label = role;
    binding.filler_label = filler;

    // Create activation patterns for role and filler
    std::vector<float> role_pattern(config_.neurons_per_region, 0.0f);
    std::vector<float> filler_pattern(config_.neurons_per_region, 0.0f);

    // Simple hash-based encoding for role and filler
    std::hash<std::string> hasher;
    std::size_t role_hash = hasher(role);
    std::size_t filler_hash = hasher(filler);

    // Activate specific neurons based on hash
    for (std::size_t i = 0; i < 5; ++i) { // Activate 5 neurons per concept
        std::size_t role_idx = (role_hash + i) % config_.neurons_per_region;
        std::size_t filler_idx = (filler_hash + i) % config_.neurons_per_region;
        
        role_pattern[role_idx] = binding.strength;
        filler_pattern[filler_idx] = binding.strength;
    }

    // Inject patterns into substrate
    injectRegionActivation(binding.role_region, role_pattern);
    injectRegionActivation(binding.filler_region, filler_pattern);

    // Establish binding connections
    if (establishBinding(binding.role_region, binding.filler_region, binding.strength)) {
        active_bindings_.push_back(binding);
        return true;
    }

    return false;
}

void SubstrateWorkingMemory::updateSequence(const std::string& token) {
    if (sequence_regions_.empty()) {
        return;
    }

    current_sequence_.current_token = token;

    // Create activation pattern for current token
    std::vector<float> token_pattern(config_.neurons_per_region, 0.0f);
    std::hash<std::string> hasher;
    std::size_t token_hash = hasher(token);

    // Activate neurons for current token
    for (std::size_t i = 0; i < 8; ++i) { // Activate 8 neurons per token
        std::size_t idx = (token_hash + i) % config_.neurons_per_region;
        token_pattern[idx] = 0.8f;
    }

    // Inject into current sequence region
    injectRegionActivation(sequence_regions_[0], token_pattern);

    // Update prediction based on sequence region activity
    if (sequence_regions_.size() >= 3) {
        auto prediction_activations = extractRegionActivation(sequence_regions_[2]);
        current_sequence_.prediction_activations = prediction_activations;
        
        // Simple prediction confidence calculation
        float max_activation = *std::max_element(prediction_activations.begin(), prediction_activations.end());
        current_sequence_.prediction_confidence = max_activation;
        
        // Mock prediction for demonstration
        if (max_activation > config_.sequence_threshold) {
            current_sequence_.predicted_token = "PREDICTED_" + token;
        } else {
            current_sequence_.predicted_token = "UNKNOWN";
        }
    }
}

std::vector<SubstrateWorkingMemory::SubstrateBinding> SubstrateWorkingMemory::getCurrentBindings() const {
    return active_bindings_;
}

SubstrateWorkingMemory::SubstrateSequence SubstrateWorkingMemory::getSequencePrediction() const {
    return current_sequence_;
}

void SubstrateWorkingMemory::applyMaintenance() {
    // Apply maintenance current to all working memory regions
    for (NeuroForge::RegionID region_id : wm_regions_) {
        auto region = brain_->getRegion(region_id);
        if (region) {
            // Apply maintenance current to keep neurons active
            const auto& neurons = region->getNeurons();
            for (const auto& neuron : neurons) {
                if (neuron && neuron->getActivation() > 0.1f) {
                    // Apply maintenance current by increasing activation
                    float current_activation = neuron->getActivation();
                    neuron->setActivation(std::min(1.0f, current_activation + config_.maintenance_current));
                }
            }
        }
    }

    // Apply maintenance to binding regions
    for (NeuroForge::RegionID region_id : binding_regions_) {
        auto region = brain_->getRegion(region_id);
        if (region) {
            const auto& neurons = region->getNeurons();
            for (const auto& neuron : neurons) {
                if (neuron && neuron->getActivation() > 0.1f) {
                    float current_activation = neuron->getActivation();
                    neuron->setActivation(std::min(1.0f, current_activation + config_.maintenance_current * 0.8f));
                }
            }
        }
    }
}

void SubstrateWorkingMemory::applyDecay(float delta_time) {
    // Apply decay to working memory regions
    for (NeuroForge::RegionID region_id : wm_regions_) {
        auto region = brain_->getRegion(region_id);
        if (region) {
            const auto& neurons = region->getNeurons();
            for (const auto& neuron : neurons) {
                if (neuron) {
                    float current_activation = neuron->getActivation();
                    float decayed_activation = current_activation * std::pow(config_.decay_rate, delta_time);
                    // Note: This is a simplified decay - in practice, you'd need proper neuron state management
                }
            }
        }
    }

    // Apply decay to binding strengths
    for (auto& binding : active_bindings_) {
        binding.strength *= std::pow(config_.decay_rate, delta_time);
    }
}

NeuroForge::RegionID SubstrateWorkingMemory::createWorkingMemoryRegion(const std::string& name) {
    auto region = brain_->createRegion(name, Region::Type::Custom, Region::ActivationPattern::Synchronous);
    if (region) {
        return region->getId();
    }
    return 0; // Invalid region ID
}

bool SubstrateWorkingMemory::establishBinding(NeuroForge::RegionID role_region, NeuroForge::RegionID filler_region, float strength) {
    // Create connections between role and filler regions to establish binding
    std::size_t connections_created = brain_->connectRegions(role_region, filler_region, 0.1f, {strength * 0.5f, strength});
    return connections_created > 0;
}

void SubstrateWorkingMemory::updateBindingStrengths() {
    for (auto& binding : active_bindings_) {
        // Extract current activation from role and filler regions
        auto role_activations = extractRegionActivation(binding.role_region);
        auto filler_activations = extractRegionActivation(binding.filler_region);

        // Calculate binding strength based on co-activation
        float role_activity = std::accumulate(role_activations.begin(), role_activations.end(), 0.0f) / role_activations.size();
        float filler_activity = std::accumulate(filler_activations.begin(), filler_activations.end(), 0.0f) / filler_activations.size();

        // Update binding strength based on co-activation
        float co_activation = std::min(role_activity, filler_activity);
        binding.strength = std::max(binding.strength * 0.9f, co_activation);
    }
}

void SubstrateWorkingMemory::pruneWeakBindings() {
    // Remove bindings below threshold
    active_bindings_.erase(
        std::remove_if(active_bindings_.begin(), active_bindings_.end(),
            [this](const SubstrateBinding& binding) {
                return binding.strength < 0.1f;
            }),
        active_bindings_.end()
    );
}

std::vector<float> SubstrateWorkingMemory::extractRegionActivation(NeuroForge::RegionID region_id) const {
    std::vector<float> activations;
    
    auto region = brain_->getRegion(region_id);
    if (region) {
        const auto& neurons = region->getNeurons();
        activations.reserve(neurons.size());
        
        for (const auto& neuron : neurons) {
            if (neuron) {
                activations.push_back(neuron->getActivation());
            } else {
                activations.push_back(0.0f);
            }
        }
    }
    
    return activations;
}

void SubstrateWorkingMemory::injectRegionActivation(NeuroForge::RegionID region_id, const std::vector<float>& pattern) {
    auto region = brain_->getRegion(region_id);
    if (region) {
        const auto& neurons = region->getNeurons();
        std::size_t min_size = std::min(neurons.size(), pattern.size());
        
        for (std::size_t i = 0; i < min_size; ++i) {
            if (neurons[i] && pattern[i] > 0.0f) {
                float current_activation = neurons[i]->getActivation();
                neurons[i]->setActivation(std::min(1.0f, current_activation + pattern[i]));
            }
        }
    }
}

void SubstrateWorkingMemory::updateStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.active_bindings = active_bindings_.size();
    
    if (!active_bindings_.empty()) {
        float total_strength = 0.0f;
        for (const auto& binding : active_bindings_) {
            total_strength += binding.strength;
        }
        stats_.average_binding_strength = total_strength / static_cast<float>(active_bindings_.size());
    } else {
        stats_.average_binding_strength = 0.0f;
    }
    
    // Mock sequence prediction accuracy
    stats_.sequence_prediction_accuracy = current_sequence_.prediction_confidence;
}

SubstrateWorkingMemory::Statistics SubstrateWorkingMemory::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

} // namespace Core
} // namespace NeuroForge
