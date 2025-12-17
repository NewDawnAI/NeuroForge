#pragma once

#include "core/Types.h"
#include "core/LanguageSystem.h"
#include "core/HypergraphBrain.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>

namespace NeuroForge {
namespace Core {

/**
 * @brief Neural bindings for direct language-substrate interaction
 * 
 * Provides low-level neural substrate bindings for language processing,
 * enabling direct neural representation of linguistic concepts and patterns.
 */
class NeuralLanguageBindings {
public:
    /**
     * @brief Neural assembly for language token representation
     */
    struct TokenNeuralAssembly {
        std::string token_symbol;                          ///< Token symbol
        NeuroForge::NeuronID primary_neuron;               ///< Primary neuron for token
        std::vector<NeuroForge::NeuronID> assembly_neurons; ///< Full neural assembly
        std::vector<NeuroForge::SynapseID> internal_synapses; ///< Internal assembly connections
        float assembly_coherence = 0.0f;                  ///< Assembly coherence measure
        float activation_threshold = 0.5f;                ///< Activation threshold
        std::uint64_t firing_count = 0;                   ///< Number of times assembly fired
        std::chrono::steady_clock::time_point last_firing; ///< Last firing time
    };

    /**
     * @brief Neural pattern for proto-word crystallization
     */
    struct ProtoWordNeuralPattern {
        std::string proto_word_pattern;                    ///< Proto-word pattern string
        std::vector<std::string> phoneme_sequence;         ///< Phoneme sequence
        std::vector<NeuroForge::NeuronID> sequence_neurons; ///< Neurons for phoneme sequence
        std::vector<NeuroForge::SynapseID> sequence_synapses; ///< Sequential connections
        NeuroForge::NeuronID pattern_neuron;               ///< Pattern recognition neuron
        float crystallization_strength = 0.0f;            ///< Crystallization strength
        float neural_stability = 0.0f;                    ///< Neural pattern stability
        bool is_crystallized = false;                     ///< Crystallization status
        std::uint64_t reinforcement_count = 0;            ///< Number of reinforcements
    };

    /**
     * @brief Neural circuit for prosodic pattern processing
     */
    struct ProsodicNeuralCircuit {
        std::string pattern_name;                          ///< Prosodic pattern name
        NeuroForge::NeuronID pitch_neuron;                 ///< Pitch processing neuron
        NeuroForge::NeuronID energy_neuron;                ///< Energy processing neuron
        NeuroForge::NeuronID rhythm_neuron;                ///< Rhythm processing neuron
        NeuroForge::NeuronID integration_neuron;           ///< Pattern integration neuron
        std::vector<NeuroForge::SynapseID> circuit_synapses; ///< Circuit connections
        float pattern_sensitivity = 0.5f;                 ///< Pattern detection sensitivity
        float motherese_bias = 0.0f;                      ///< Motherese detection bias
        bool is_motherese_detector = false;               ///< Whether circuit detects motherese
    };

    /**
     * @brief Cross-modal neural binding for grounding
     */
    struct CrossModalNeuralBinding {
        std::size_t grounding_id;                          ///< Grounding association ID
        std::string object_category;                       ///< Object category
        NeuroForge::NeuronID language_neuron;              ///< Language representation neuron
        NeuroForge::NeuronID visual_neuron;                ///< Visual representation neuron
        NeuroForge::NeuronID auditory_neuron;              ///< Auditory representation neuron
        NeuroForge::NeuronID tactile_neuron;               ///< Tactile representation neuron
        NeuroForge::NeuronID binding_neuron;               ///< Cross-modal binding neuron
        std::vector<NeuroForge::SynapseID> binding_synapses; ///< Cross-modal synapses
        float binding_strength = 0.0f;                    ///< Overall binding strength
        std::unordered_map<std::string, float> modality_strengths; ///< Per-modality strengths
        bool is_stable_binding = false;                   ///< Binding stability status
    };

    /**
     * @brief Neural attention circuit for language learning
     */
    struct AttentionNeuralCircuit {
        NeuroForge::NeuronID attention_controller;         ///< Attention control neuron
        std::vector<NeuroForge::NeuronID> attention_targets; ///< Attention target neurons
        std::vector<NeuroForge::SynapseID> attention_synapses; ///< Attention modulation synapses
        float attention_strength = 0.0f;                  ///< Current attention strength
        float attention_focus = 0.0f;                     ///< Attention focus measure
        std::string attention_context;                     ///< Current attention context
        bool is_joint_attention = false;                  ///< Joint attention status
    };

    /**
     * @brief Configuration for neural language bindings
     */
    struct Config {
        // Assembly parameters
        std::size_t token_assembly_size = 8;              ///< Size of token neural assemblies
        float assembly_coherence_threshold = 0.2f;        ///< Coherence threshold for assemblies
        float assembly_activation_decay = 0.95f;          ///< Activation decay rate
        
        // Pattern parameters
        std::size_t max_phoneme_sequence_length = 10;     ///< Maximum phoneme sequence length
        float crystallization_threshold = 0.8f;           ///< Crystallization threshold
        float pattern_stability_threshold = 0.75f;        ///< Pattern stability threshold
        
        // Circuit parameters
        float prosodic_sensitivity = 0.6f;                ///< Prosodic pattern sensitivity
        float motherese_detection_threshold = 0.8f;       ///< Motherese detection threshold
        float cross_modal_binding_threshold = 0.7f;       ///< Cross-modal binding threshold
        
        // Learning parameters
        float neural_learning_rate = 0.01f;               ///< Neural learning rate
        float stdp_learning_rate = 0.005f;                ///< STDP learning rate
        float hebbian_learning_rate = 0.008f;             ///< Hebbian learning rate
        
        // Performance parameters
        bool enable_sparse_activation = true;             ///< Enable sparse neural activation
        bool enable_dynamic_thresholds = true;            ///< Enable dynamic thresholds
        std::size_t max_concurrent_bindings = 100;        ///< Maximum concurrent bindings
    };

    /**
     * @brief Binding statistics
     */
    struct Statistics {
        std::size_t total_token_assemblies = 0;           ///< Total token assemblies
        std::size_t active_token_assemblies = 0;          ///< Active token assemblies
        std::size_t total_proto_word_patterns = 0;        ///< Total proto-word patterns
        std::size_t crystallized_patterns = 0;            ///< Crystallized patterns
        std::size_t total_prosodic_circuits = 0;          ///< Total prosodic circuits
        std::size_t active_prosodic_circuits = 0;         ///< Active prosodic circuits
        std::size_t total_cross_modal_bindings = 0;       ///< Total cross-modal bindings
        std::size_t stable_cross_modal_bindings = 0;      ///< Stable cross-modal bindings
        float average_assembly_coherence = 0.0f;          ///< Average assembly coherence
        float average_pattern_stability = 0.0f;           ///< Average pattern stability
        float average_binding_strength = 0.0f;            ///< Average binding strength
        std::uint64_t neural_language_operations = 0;     ///< Total neural language operations
    };

private:
    // System references
    std::shared_ptr<HypergraphBrain> hypergraph_brain_;
    std::shared_ptr<LearningSystem> learning_system_;
    
    // Configuration
    Config config_;
    std::atomic<bool> is_initialized_{false};
    
    // Neural bindings storage
    std::unordered_map<std::string, TokenNeuralAssembly> token_assemblies_;
    std::unordered_map<std::string, ProtoWordNeuralPattern> proto_word_patterns_;
    std::unordered_map<std::string, ProsodicNeuralCircuit> prosodic_circuits_;
    std::unordered_map<std::size_t, CrossModalNeuralBinding> cross_modal_bindings_;
    std::unordered_map<std::string, AttentionNeuralCircuit> attention_circuits_;
    
    // Thread safety
    mutable std::recursive_mutex bindings_mutex_;
    mutable std::mutex assemblies_mutex_;
    mutable std::mutex patterns_mutex_;
    mutable std::mutex circuits_mutex_;
    mutable std::mutex attention_mutex_;
    
    // Statistics
    Statistics statistics_;
    mutable std::mutex statistics_mutex_;

public:
    /**
     * @brief Constructor
     * @param hypergraph_brain Shared pointer to hypergraph brain
     * @param config Binding configuration
     */
    explicit NeuralLanguageBindings(std::shared_ptr<HypergraphBrain> hypergraph_brain,
                                   const Config& config);

    /**
     * @brief Destructor
     */
    ~NeuralLanguageBindings();

    // Core lifecycle
    bool initialize();
    void shutdown();
    void reset();

    // Configuration management
    void updateConfig(const Config& new_config);
    const Config& getConfig() const { return config_; }

    // Token neural assembly operations
    bool createTokenNeuralAssembly(const std::string& token_symbol,
                                  const std::vector<float>& token_embedding,
                                  NeuroForge::RegionID target_region);
    bool activateTokenAssembly(const std::string& token_symbol, float activation_strength);
    bool reinforceTokenAssembly(const std::string& token_symbol, float reinforcement_strength);
    TokenNeuralAssembly* getTokenAssembly(const std::string& token_symbol);
    std::vector<std::string> getActiveTokens(float coherence_threshold = 0.5f) const;
    std::vector<std::string> getActiveTokenAssemblies(float coherence_threshold = 0.5f) const;
    float calculateAssemblyCoherence(const TokenNeuralAssembly& assembly) const;

    // Proto-word neural pattern operations
    bool createProtoWordNeuralPattern(const std::string& pattern,
                                     const std::vector<std::string>& phoneme_sequence,
                                     NeuroForge::RegionID target_region);
    bool reinforceProtoWordPattern(const std::string& pattern, float reinforcement_strength);
    bool crystallizeProtoWordPattern(const std::string& pattern);
    ProtoWordNeuralPattern* getProtoWordPattern(const std::string& pattern);
    std::vector<std::string> getCrystallizedProtoWords() const;
    std::vector<std::string> getStableProtoWordPatterns(float stability_threshold = 0.7f) const;
    float calculatePatternStability(const ProtoWordNeuralPattern& pattern) const;

    // Prosodic neural circuit operations
    bool createProsodicNeuralCircuit(const std::string& pattern_name,
                                    const LanguageSystem::AcousticFeatures& template_features,
                                    NeuroForge::RegionID target_region);
    bool activateProsodicCircuit(const std::string& pattern_name,
                                const LanguageSystem::AcousticFeatures& features);
    bool configureMothereseBias(const std::string& pattern_name, float bias_strength);
    ProsodicNeuralCircuit* getProsodicCircuit(const std::string& pattern_name);
    std::vector<std::string> detectActiveProsodicPatterns(float sensitivity_threshold = 0.6f) const;

    // Cross-modal neural binding operations
    bool createCrossModalNeuralBinding(std::size_t grounding_id,
                                      const std::string& object_category,
                                      const std::vector<float>& visual_features,
                                      const std::vector<float>& auditory_features,
                                      const std::vector<float>& tactile_features,
                                      const std::vector<float>& language_features);
    bool strengthenCrossModalBinding(std::size_t grounding_id, float strength_boost);
    bool stabilizeCrossModalBinding(std::size_t grounding_id);
    CrossModalNeuralBinding* getCrossModalBinding(std::size_t grounding_id);
    std::vector<std::size_t> getStableCrossModalBindings() const;
    float calculateBindingStrength(const CrossModalNeuralBinding& binding) const;

    // Attention neural circuit operations
    bool createAttentionNeuralCircuit(const std::string& context,
                                     const std::vector<NeuroForge::NeuronID>& target_neurons,
                                     NeuroForge::RegionID control_region);
    bool modulateAttention(const std::string& context, float attention_strength);
    bool enableJointAttention(const std::string& context, bool enable);
    AttentionNeuralCircuit* getAttentionCircuit(const std::string& context);
    float calculateAttentionFocus(const AttentionNeuralCircuit& circuit) const;

    // Neural learning integration
    void applyNeuralLanguageLearning(float delta_time);
    void applySTDPToLanguageBindings(const std::unordered_map<NeuroForge::NeuronID, NeuroForge::TimePoint>& spike_times);
    void applyHebbianToLanguageBindings(float learning_rate);
    void modulateLanguageLearning(const std::unordered_map<NeuroForge::NeuronID, float>& attention_weights);

    // Neural substrate operations
    void propagateLanguageActivations();
    void updateNeuralLanguageRepresentations();
    void consolidateLanguageBindings();
    void pruneInactiveBindings(float inactivity_threshold = 0.1f);

    // Performance optimization
    void optimizeNeuralBindings();
    void balanceNeuralLoad();
    void adaptiveThresholdAdjustment();

    // Statistics and monitoring
    Statistics getStatistics() const;
    void resetStatistics();
    std::string generateBindingReport() const;

    // State queries
    bool isInitialized() const { return is_initialized_.load(); }
    std::size_t getTotalBindings() const;
    float getOverallBindingHealth() const;

private:
    // Internal helper methods
    NeuroForge::NeuronID allocateNeuronForBinding(NeuroForge::RegionID region_id);
    std::vector<NeuroForge::NeuronID> allocateNeuralAssembly(NeuroForge::RegionID region_id, std::size_t size);
    NeuroForge::SynapseID createBindingSynapse(NeuroForge::NeuronID source, NeuroForge::NeuronID target,
                                              float weight, NeuroForge::SynapseType type);
    
    void setupTokenAssemblyConnections(TokenNeuralAssembly& assembly);
    void setupProtoWordPatternConnections(ProtoWordNeuralPattern& pattern);
    void setupProsodicCircuitConnections(ProsodicNeuralCircuit& circuit);
    void setupCrossModalBindingConnections(CrossModalNeuralBinding& binding);
    void setupAttentionCircuitConnections(AttentionNeuralCircuit& circuit);
    
    void updateBindingStatistics();
    float measureNeuralActivity(const std::vector<NeuroForge::NeuronID>& neurons) const;
    float measureSynapticStrength(const std::vector<NeuroForge::SynapseID>& synapses) const;
    
    // Neural substrate utility methods
    void activateNeurons(const std::vector<NeuroForge::NeuronID>& neurons, float activation);
    void strengthenSynapses(const std::vector<NeuroForge::SynapseID>& synapses, float factor);
    void modulateSynapses(const std::vector<NeuroForge::SynapseID>& synapses, float modulation);
    
    // Learning system integration
    void integrateWithLearningSystem();
    void configureLanguageSpecificLearning();
    void applyLanguageSpecificPlasticity();
};

} // namespace Core
} // namespace NeuroForge