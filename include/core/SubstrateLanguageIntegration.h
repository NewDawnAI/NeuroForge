#pragma once

#include "core/LanguageSystem.h"
#include "core/HypergraphBrain.h"
#include "core/Region.h"
#include "core/LearningSystem.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>

namespace NeuroForge {
namespace Core {

/**
 * @brief Integration layer between Babbling Stage Language System and Neural Substrate
 * 
 * This class provides the bridge between the high-level language processing capabilities
 * of the Babbling Stage and the low-level neural substrate architecture, enabling
 * biologically-inspired language learning through direct neural mechanisms.
 */
class SubstrateLanguageIntegration {
public:
    /**
     * @brief Configuration for substrate-language integration
     */
    struct Config {
        // Neural substrate mapping
        std::size_t language_region_neurons = 1024;        ///< Neurons dedicated to language processing
        std::size_t proto_word_region_neurons = 512;       ///< Neurons for proto-word crystallization
        std::size_t prosodic_region_neurons = 256;         ///< Neurons for prosodic pattern processing
        std::size_t grounding_region_neurons = 768;        ///< Neurons for multimodal grounding
        
        // Learning integration parameters
        float language_learning_rate = 0.008f;             ///< Learning rate for language-specific updates
        float proto_word_stdp_weight = 0.25f;              ///< STDP weight for proto-word formation
        float prosodic_hebbian_weight = 0.75f;             ///< Hebbian weight for prosodic learning
        float grounding_association_strength = 0.6f;       ///< Strength for cross-modal associations
        
        // Substrate binding parameters
        float neural_token_threshold = 0.7f;               ///< Threshold for neural token activation
        float pattern_recognition_threshold = 0.8f;        ///< Threshold for pattern recognition
        float crystallization_neural_boost = 1.5f;         ///< Neural boost for crystallizing patterns
        
        // Performance optimization
        bool enable_sparse_updates = true;                 ///< Enable sparse neural updates
        bool enable_attention_modulation = true;           ///< Enable attention-based learning modulation
        std::size_t max_concurrent_patterns = 50;          ///< Maximum concurrent pattern processing
        
        // Integration modes
        enum class IntegrationMode {
            Passive,        ///< Language system observes substrate
            Active,         ///< Language system influences substrate
            Bidirectional   ///< Full bidirectional integration
        };
        IntegrationMode integration_mode = IntegrationMode::Bidirectional;
    };

    /**
     * @brief Neural substrate binding for language tokens
     */
    struct NeuralTokenBinding {
        std::size_t token_id;                              ///< Language system token ID
        NeuroForge::NeuronID primary_neuron;               ///< Primary neural representation
        std::vector<NeuroForge::NeuronID> assembly_neurons; ///< Neural assembly for token
        float binding_strength = 0.0f;                     ///< Strength of neural binding
        std::chrono::steady_clock::time_point last_activation; ///< Last activation time
        std::uint64_t activation_count = 0;                ///< Number of activations
    };

    /**
     * @brief Neural pattern for proto-word crystallization
     */
    struct NeuralProtoWordPattern {
        std::string pattern_signature;                     ///< Proto-word pattern signature
        std::vector<NeuroForge::NeuronID> pattern_neurons; ///< Neurons encoding the pattern
        std::vector<NeuroForge::SynapseID> pattern_synapses; ///< Synapses forming the pattern
        float crystallization_strength = 0.0f;            ///< Current crystallization strength
        float neural_stability = 0.0f;                    ///< Neural pattern stability
        bool is_crystallized = false;                     ///< Whether pattern has crystallized
    };

    /**
     * @brief Cross-modal grounding neural representation
     */
    struct NeuralGroundingAssociation {
        std::size_t grounding_id;                          ///< Grounding association ID
        NeuroForge::RegionID visual_region;                ///< Visual processing region
        NeuroForge::RegionID auditory_region;              ///< Auditory processing region
        NeuroForge::RegionID language_region;              ///< Language processing region
        std::vector<NeuroForge::SynapseID> cross_modal_synapses; ///< Cross-modal connections
        float association_strength = 0.0f;                ///< Overall association strength
        std::unordered_map<std::string, float> modality_weights; ///< Per-modality weights
    };

    /**
     * @brief Neural mapping for phoneme processing
     */
    struct PhonemeNeuralMapping {
        std::string phoneme;                               ///< Phoneme string
        std::vector<NeuroForge::NeuronID> assembly_neurons; ///< Neural assembly for phoneme
        float activation_strength = 0.0f;                 ///< Current activation strength
        float motor_coordination = 0.0f;                  ///< Motor coordination strength
        float lipsync_coordination = 0.0f;                ///< Lip-sync coordination strength
        std::chrono::steady_clock::time_point creation_time; ///< Creation timestamp
        std::chrono::steady_clock::time_point last_activation; ///< Last activation time
        std::uint64_t activation_count = 0;               ///< Number of activations
    };

    /**
     * @brief Neural pattern for prosody processing
     */
    struct ProsodyNeuralPattern {
        std::string pattern_name;                          ///< Prosody pattern name
        std::vector<float> pitch_contour;                  ///< Pitch contour pattern
        std::vector<float> energy_contour;                 ///< Energy contour pattern
        std::vector<float> rhythm_pattern;                 ///< Rhythm pattern
        std::vector<NeuroForge::NeuronID> pattern_neurons; ///< Neurons encoding the pattern
        std::vector<NeuroForge::SynapseID> pattern_synapses; ///< Pattern synapses
        float pattern_strength = 0.0f;                    ///< Pattern strength
        float stability = 0.0f;                           ///< Pattern stability
        bool is_stable = false;                           ///< Whether pattern is stable
        std::chrono::steady_clock::time_point creation_time; ///< Creation timestamp
        std::chrono::steady_clock::time_point last_reinforcement; ///< Last reinforcement time
    };

    /**
     * @brief Integration statistics
     */
    struct Statistics {
        std::size_t total_neural_tokens = 0;              ///< Total neural token bindings
        std::size_t active_neural_patterns = 0;           ///< Active neural patterns
        std::size_t crystallized_patterns = 0;            ///< Crystallized proto-word patterns
        std::size_t cross_modal_associations = 0;         ///< Cross-modal grounding associations
        float average_binding_strength = 0.0f;            ///< Average neural binding strength
        float substrate_language_coherence = 0.0f;        ///< Coherence between substrate and language
        std::uint64_t neural_language_updates = 0;        ///< Number of neural-language updates
        float integration_efficiency = 0.0f;              ///< Overall integration efficiency
    };

private:
    // Core system references
    std::shared_ptr<LanguageSystem> language_system_;
    std::shared_ptr<HypergraphBrain> hypergraph_brain_;
    std::shared_ptr<LearningSystem> learning_system_;
    
    // Configuration and state
    Config config_;
    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> is_active_{false};
    
    // Neural substrate regions for language processing
    NeuroForge::RegionPtr language_region_;
    NeuroForge::RegionPtr proto_word_region_;
    NeuroForge::RegionPtr prosodic_region_;
    NeuroForge::RegionPtr grounding_region_;
    
    // Speech production regions
    NeuroForge::RegionPtr phoneme_region_;
    NeuroForge::RegionPtr motor_region_;
    NeuroForge::RegionPtr lipsync_region_;
    NeuroForge::RegionPtr prosody_control_region_;
    
    // Integration mappings
    std::unordered_map<std::size_t, NeuralTokenBinding> token_bindings_;
    std::unordered_map<std::string, NeuralProtoWordPattern> proto_word_patterns_;
    std::unordered_map<std::size_t, NeuralGroundingAssociation> grounding_associations_;
    
    // Speech production mappings
    std::unordered_map<std::string, PhonemeNeuralMapping> phoneme_mappings_;
    std::unordered_map<std::string, ProsodyNeuralPattern> prosody_patterns_;
    
    // Multimodal stream data structures
    struct MultimodalStreamState {
        std::vector<NeuroForge::NeuronID> audio_stream_neurons;
        std::vector<NeuroForge::NeuronID> visual_stream_neurons;
        std::vector<NeuroForge::NeuronID> gaze_stream_neurons;
        std::vector<NeuroForge::NeuronID> integration_neurons;
        float audio_activation_strength = 0.0f;
        float visual_activation_strength = 0.0f;
        float gaze_activation_strength = 0.0f;
        bool streams_synchronized = false;
        std::chrono::steady_clock::time_point last_sync_time;
    };

    struct AudioStreamState {
        std::vector<NeuroForge::NeuronID> assembly_neurons;
        std::vector<float> features;
        float activation_strength = 0.0f;
        std::chrono::steady_clock::time_point last_updated;
    };

    struct VisualStreamState {
        std::vector<NeuroForge::NeuronID> assembly_neurons;
        std::vector<float> features;
        float activation_strength = 0.0f;
        std::chrono::steady_clock::time_point last_updated;
    };

    struct GazeStreamState {
        std::vector<NeuroForge::NeuronID> assembly_neurons;
        std::vector<float> gaze_targets;
        float activation_strength = 0.0f;
        std::chrono::steady_clock::time_point last_updated;
    };
    
    // Neural substrate regions for multimodal processing
    NeuroForge::RegionPtr audio_stream_region_;
    NeuroForge::RegionPtr visual_stream_region_;
    NeuroForge::RegionPtr gaze_coordination_region_;
    NeuroForge::RegionPtr multimodal_integration_region_;
    
    // Multimodal stream state
    MultimodalStreamState multimodal_state_;
    
    // Thread safety
    mutable std::recursive_mutex integration_mutex_;
    mutable std::mutex token_binding_mutex_;
    mutable std::mutex pattern_mutex_;
    mutable std::mutex grounding_mutex_;
    
    // Speech production thread safety
    mutable std::mutex phoneme_mapping_mutex_;
    mutable std::mutex prosody_pattern_mutex_;
    
    // Statistics and monitoring
    Statistics statistics_;
    mutable std::mutex statistics_mutex_;

public:
    /**
     * @brief Constructor
     * @param language_system Shared pointer to language system
     * @param hypergraph_brain Shared pointer to hypergraph brain
     * @param config Integration configuration
     */
    explicit SubstrateLanguageIntegration(
        std::shared_ptr<LanguageSystem> language_system,
        std::shared_ptr<HypergraphBrain> hypergraph_brain,
        const Config& config);

    /**
     * @brief Destructor
     */
    ~SubstrateLanguageIntegration();

    // Core lifecycle
    bool initialize();
    void shutdown();
    void reset();

    // Configuration management
    void updateConfig(const Config& new_config);
    const Config& getConfig() const { return config_; }

    // Neural substrate region management
    bool createLanguageRegions();
    bool connectLanguageRegions();
    NeuroForge::RegionPtr getLanguageRegion() const { return language_region_; }
    NeuroForge::RegionPtr getProtoWordRegion() const { return proto_word_region_; }
    NeuroForge::RegionPtr getProsodicRegion() const { return prosodic_region_; }
    NeuroForge::RegionPtr getGroundingRegion() const { return grounding_region_; }

    // Token-neural binding operations
    bool bindTokenToNeuralAssembly(std::size_t token_id, const std::vector<float>& token_embedding);
    bool updateTokenBinding(std::size_t token_id, float activation_strength);
    NeuralTokenBinding* getTokenBinding(std::size_t token_id);
    std::vector<std::size_t> getActiveTokens(float threshold = 0.5f) const;

    // Proto-word crystallization integration
    bool createNeuralProtoWordPattern(const std::string& pattern, 
                                     const std::vector<std::string>& phonemes);
    bool reinforceNeuralPattern(const std::string& pattern, float reinforcement_strength);
    bool crystallizeNeuralPattern(const std::string& pattern);
    NeuralProtoWordPattern* getNeuralPattern(const std::string& pattern);
    std::vector<std::string> getCrystallizedPatterns() const;

    // Cross-modal grounding integration
    bool createNeuralGroundingAssociation(std::size_t grounding_id,
                                         const std::vector<float>& visual_features,
                                         const std::vector<float>& auditory_features,
                                         const std::vector<float>& language_features);
    bool strengthenGroundingAssociation(std::size_t grounding_id, float strength_boost);
    NeuralGroundingAssociation* getGroundingAssociation(std::size_t grounding_id);
    std::vector<std::size_t> getStableGroundingAssociations(float threshold = 0.7f) const;

    // Speech production neural integration
    bool initializeSpeechProductionRegions();
    bool mapPhonemeToNeuralAssembly(const std::string& phoneme,
                                   const std::vector<NeuroForge::NeuronID>& assembly_neurons);
    void activateSpeechProductionNeurons(const std::string& phoneme, float activation_strength);
    void integrateLipSyncWithNeuralSubstrate(const std::string& phoneme,
                                            const std::vector<float>& lip_motion_features);
    bool mapProsodyToNeuralPattern(const std::string& pattern_name,
                                  const std::vector<float>& pitch_contour,
                                  const std::vector<float>& energy_contour,
                                  const std::vector<float>& rhythm_pattern);
    void connectSpeechProductionRegions();
    
    // Helper methods for motor coordination and lip-sync
    void activateMotorCoordination(const std::string& phoneme, float coordination_strength);
    void activateLipSyncCoordination(const std::string& phoneme, float coordination_strength);
    bool mapPhonemeToNeuralAssembly(const LanguageSystem::PhonemeCluster& phoneme,
                                   const std::vector<float>& acoustic_features);
    bool activateSpeechProductionNeurons(const std::vector<LanguageSystem::PhonemeCluster>& phonemes,
                                        const std::vector<float>& timing_pattern);
    bool integrateLipSyncWithNeuralSubstrate(const std::vector<std::vector<float>>& lip_motion_sequence);
    bool mapProsodyToNeuralModulation(const std::vector<float>& prosody_contour);
    bool createSpeechMotorMemory(const std::string& word, 
                                const LanguageSystem::SpeechProductionFeatures& features);
    bool reinforceSpeechMotorPattern(const std::string& word, float reinforcement_strength);
    bool activateNeuralSpeechOutput(const LanguageSystem::SpeechProductionFeatures& features);
    void updateSpeechProductionNeuralState(float delta_time);
    float calculateSpeechNeuralCoherence() const;

    // Multimodal parallel stream integration
    bool initializeMultimodalStreamRegions();
    bool createAudioProcessingStream(const std::vector<float>& audio_features);
    bool createVisualProcessingStream(const std::vector<float>& visual_features);
    bool createGazeCoordinationStream(const std::vector<float>& gaze_targets);
    bool synchronizeMultimodalStreams(float temporal_alignment_threshold = 0.8f);
    bool activateParallelNeuralStreams(const LanguageSystem::SpeechProductionFeatures& speech_features,
                                      const LanguageSystem::VisualLanguageFeatures& visual_features);
    void updateMultimodalStreamCoherence(float delta_time);
    bool integrateAudioVisualBinding(const std::vector<float>& audio_pattern,
                                    const std::vector<float>& visual_pattern,
                                    float temporal_window = 0.2f);
    bool processMultimodalAttentionMap(const std::vector<float>& attention_weights,
                                      const std::vector<std::string>& active_modalities);
    float calculateMultimodalNeuralCoherence() const;
    
    // Cross-modal neural stream coordination
    bool establishCrossModalConnections();
    bool reinforceCrossModalBinding(const std::string& modality_a, const std::string& modality_b,
                                   float binding_strength);
    void propagateActivationAcrossModalities(float propagation_strength = 0.3f);
    bool processJointAttentionNeurally(const std::vector<float>& shared_attention_target,
                                      const std::string& associated_token);
    void updateCrossModalNeuralState(float delta_time);

    // Prosodic pattern neural integration
    bool processProsodicPatternNeurally(const LanguageSystem::AcousticFeatures& features,
                                       const std::string& co_occurring_token);
    bool reinforceProsodicNeuralPattern(const std::string& pattern_name, float reinforcement);
    float calculateNeuralProsodicSalience(const LanguageSystem::AcousticFeatures& features) const;

    // Learning system integration
    void integrateWithLearningSystem();
    void applyLanguageSpecificLearning(float delta_time);
    void modulateAttentionForLanguageLearning(const std::unordered_map<NeuroForge::NeuronID, float>& attention_map);

    // Substrate-driven language processing
    void processSubstrateLanguageStep(float delta_time);
    void propagateLanguageActivations();
    void updateNeuralLanguageRepresentations();

    // Performance optimization
    void optimizeNeuralBindings();
    void pruneInactiveBindings(float inactivity_threshold = 0.1f);
    void consolidateNeuralPatterns();

    // Statistics and monitoring
    Statistics getStatistics() const;
    void resetStatistics();
    float calculateIntegrationCoherence() const;
    std::string generateIntegrationReport() const;

    // State management
    bool isInitialized() const { return is_initialized_.load(); }
    bool isActive() const { return is_active_.load(); }
    void setActive(bool active) { is_active_.store(active); }

private:
    // Internal helper methods
    void initializeNeuralRegions();
    void setupCrossRegionConnectivity();
    void configureRegionLearningParameters();
    
    NeuroForge::NeuronID selectPrimaryNeuronForToken(std::size_t token_id, 
                                                    const std::vector<float>& embedding);
    std::vector<NeuroForge::NeuronID> formNeuralAssembly(NeuroForge::NeuronID primary_neuron, 
                                                        std::size_t assembly_size);
    
    void updateIntegrationStatistics();
    float calculateBindingStrength(const NeuralTokenBinding& binding) const;
    float calculatePatternStability(const NeuralProtoWordPattern& pattern) const;
    
    // Neural substrate utility methods
    void activateNeuralAssembly(const std::vector<NeuroForge::NeuronID>& assembly, float strength);
    void strengthenNeuralConnections(const std::vector<NeuroForge::SynapseID>& synapses, float factor);
    float measureNeuralCoherence(const std::vector<NeuroForge::NeuronID>& neurons) const;
    
    // Multimodal stream state tracking
    mutable std::mutex audio_stream_mutex_;
    mutable std::mutex visual_stream_mutex_;
    mutable std::mutex gaze_stream_mutex_;
    
    AudioStreamState current_audio_stream_;
    VisualStreamState current_visual_stream_;
    GazeStreamState current_gaze_stream_;
};

} // namespace Core
} // namespace NeuroForge