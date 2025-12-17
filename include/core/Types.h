#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <chrono>
#include <cstdint>
#include <functional>

namespace NeuroForge {
    namespace Core {
        // Forward declarations
        class Neuron;
        class Synapse;
        class Region;
        class HypergraphBrain;
    }

    // Use the Core namespace classes
    using Neuron = Core::Neuron;
    using Synapse = Core::Synapse;
    using Region = Core::Region;
    using HypergraphBrain = Core::HypergraphBrain;

// Enums that need to be accessible without full class definitions
enum class SynapseType {
    Excitatory = 0,     ///< Increases target neuron activation
    Inhibitory = 1,     ///< Decreases target neuron activation
    Modulatory = 2      ///< Modifies learning rate or other properties
};

// Modality types for multimodal alignment
enum class Modality {
    Visual,         ///< Image/video content
    Audio,          ///< Audio/speech content
    Text,           ///< Text/language content
    Proprioceptive, ///< Internal state/position
    Social,         ///< Social perception and events
    Multimodal      ///< Cross-modal associations
};


// Basic type aliases
using NeuronID = uint64_t;
using SynapseID = uint64_t;
using RegionID = uint32_t;
using Weight = float;
using Activation = float;
using TimePoint = std::chrono::steady_clock::time_point;
using Duration = std::chrono::milliseconds;

// Smart pointer type aliases
using NeuronPtr = std::shared_ptr<Neuron>;
using NeuronWeakPtr = std::weak_ptr<Neuron>;
using SynapsePtr = std::shared_ptr<Synapse>;
using SynapseWeakPtr = std::weak_ptr<Synapse>;
using RegionPtr = std::shared_ptr<Region>;
using RegionWeakPtr = std::weak_ptr<Region>;
using BrainPtr = std::shared_ptr<HypergraphBrain>;
// using BrainWeakPtr = std::weak_ptr<HypergraphBrain>; // removed: LearningSystem uses raw pointer to avoid lock and lifetime issues

// Container type aliases
using NeuronContainer = std::vector<NeuronPtr>;
using SynapseContainer = std::vector<SynapsePtr>;
using RegionContainer = std::vector<RegionPtr>;

// Map type aliases
using NeuronMap = std::unordered_map<NeuronID, NeuronPtr>;
using SynapseMap = std::unordered_map<SynapseID, SynapsePtr>;
using RegionMap = std::unordered_map<RegionID, RegionPtr>;

// Set type aliases
using NeuronSet = std::unordered_set<NeuronID>;
using SynapseSet = std::unordered_set<SynapseID>;
using RegionSet = std::unordered_set<RegionID>;

// Connection type aliases
using ConnectionMap = std::unordered_map<NeuronID, std::vector<SynapsePtr>>;
using RegionConnectionMap = std::unordered_map<RegionID, std::vector<RegionPtr>>;

} // namespace NeuroForge

// Provide hashing for NeuroForge::Modality to allow use in unordered_map
namespace std {
    template<>
    struct hash<NeuroForge::Modality> {
        size_t operator()(NeuroForge::Modality m) const noexcept {
            return static_cast<size_t>(m);
        }
    };
}