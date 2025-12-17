#pragma once

#include "core/Types.h"
#include "core/Region.h"
#include "core/LearningSystem.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <deque>
#include <functional>
#include <random>
#include <chrono>

namespace NeuroForge {
namespace Core {

/**
 * @brief Phase 5 Language System for developmental language acquisition
 * 
 * Implements proto-language learning through:
 * - Mimicry-based phoneme/word learning
 * - Internal narration and symbolic token generation
 * - Multimodal grounding (vision/audio/action → language)
 * - Developmental progression: babbling → copying → self-directed communication
 */
class LanguageSystem {
public:
    using NeuronBiasCallback = std::function<void(NeuroForge::NeuronID, float)>;

    /**
     * @brief Language development stages
     */
    enum class DevelopmentalStage {
        Chaos,          ///< Random activation, no structure
        Babbling,       ///< Proto-phoneme generation
        Mimicry,        ///< Copying teacher patterns
        Grounding,      ///< Associating symbols with experiences
        Reflection,     ///< Internal narration
        Communication   ///< Goal-directed language use
    };

    /**
     * @brief Token types for symbolic representation
     */
    enum class TokenType {
        Phoneme,        ///< Basic sound unit
        Word,           ///< Semantic unit
        Action,         ///< Motor command token
        Perception,     ///< Sensory description token
        Emotion,        ///< Affective state token
        Relation,       ///< Spatial/temporal relationship
        Meta           ///< Self-referential token
    };

    /**
     * @brief Symbolic token representation
     */
    struct SymbolicToken {
        std::string symbol;                    ///< String representation
        TokenType type;                       ///< Token category
        std::vector<float> embedding;         ///< Neural embedding vector
        float activation_strength = 0.0f;     ///< Current activation level
        std::uint64_t usage_count = 0;        ///< Frequency of use
        std::chrono::steady_clock::time_point last_used; ///< Temporal tracking
        
        // Grounding associations
        std::vector<NeuroForge::NeuronID> associated_neurons; ///< Linked neural patterns
        std::unordered_map<std::string, float> sensory_associations; ///< Modality links
    };

    /**
     * @brief Acoustic features for prosodic analysis
     */
    struct AcousticFeatures {
        float pitch_contour = 0.0f;           ///< Fundamental frequency trajectory
        float energy_envelope = 0.0f;         ///< Amplitude envelope
        float rhythm_pattern = 0.0f;          ///< Temporal rhythm score
        float formant_f1 = 0.0f;             ///< First formant frequency
        float formant_f2 = 0.0f;             ///< Second formant frequency
        float voicing_strength = 0.0f;        ///< Voiced/unvoiced classification
        float spectral_centroid = 0.0f;      ///< Spectral brightness
        float intonation_slope = 0.0f;       ///< Rising/falling intonation
        
        // Salience metrics
        float attention_score = 0.0f;         ///< Computed attention weight
        float novelty_score = 0.0f;          ///< Acoustic novelty measure
        float motherese_score = 0.0f;        ///< Infant-directed speech features
    };

    /**
     * @brief Phoneme cluster for acoustic-based token generation
     */
    struct PhonemeCluster {
        std::string phonetic_symbol;          ///< IPA-like representation
        AcousticFeatures acoustic_profile;    ///< Associated acoustic features
        std::vector<float> formant_pattern;   ///< Formant frequency pattern
        float vowel_consonant_ratio = 0.5f;   ///< V/C classification score
        std::vector<std::string> variants;    ///< Acoustic variations
        float stability_score = 0.0f;        ///< Cluster coherence measure
        
        // Proto-word crystallization support
        std::uint64_t usage_frequency = 0;    ///< How often this phoneme is used
        float crystallization_strength = 0.0f; ///< Tendency to form stable patterns
        std::vector<std::string> proto_word_candidates; ///< Emerging word patterns
        std::chrono::steady_clock::time_point last_reinforced; ///< Last strengthening event
    };

    /**
     * @brief Proto-word structure for tracking emerging word patterns
     */
    struct ProtoWord {
        std::string pattern;                  ///< Phoneme sequence pattern (e.g., "ma-ma")
        std::vector<std::string> phoneme_sequence; ///< Individual phonemes
        float stability_score = 0.0f;        ///< Pattern stability measure
        std::uint64_t occurrence_count = 0;   ///< Number of times pattern occurred
        float caregiver_response_strength = 0.0f; ///< Caregiver attention/response level
        
        // Cross-modal associations
        std::vector<float> visual_associations; ///< Associated visual patterns
        std::vector<float> contextual_embeddings; ///< Situational context vectors
        float grounding_strength = 0.0f;      ///< Object/concept association strength
        
        // Developmental tracking
        std::chrono::steady_clock::time_point first_occurrence; ///< When pattern first emerged
        std::chrono::steady_clock::time_point last_occurrence;  ///< Most recent occurrence
        bool is_crystallized = false;         ///< Whether pattern has stabilized
        float crystallization_threshold = 0.7f; ///< Threshold for crystallization
    };

    /**
     * @brief Enhanced multimodal attention system for babbling stage
     */
    struct MultimodalAttentionState {
        float face_attention_weight = 0.0f;   ///< Current face attention strength
        float speech_attention_weight = 0.0f; ///< Current speech attention strength
        float joint_attention_score = 0.0f;   ///< Combined attention measure
        std::vector<float> attention_history; ///< Recent attention patterns
        
        // Babbling stage specific attention
        float proto_word_attention_boost = 0.0f; ///< Boost for emerging proto-words
        float caregiver_face_priority = 0.0f;    ///< Priority for caregiver faces
        bool is_joint_attention_active = false;  ///< Whether joint attention is occurring
        
        // Temporal dynamics
        std::chrono::steady_clock::time_point last_attention_peak;
        float attention_persistence = 0.0f;      ///< How long attention is maintained
        std::vector<std::pair<float, std::chrono::steady_clock::time_point>> attention_events;
    };

    /**
     * @brief Grounding association system for word-object mappings
     */
    struct GroundingAssociation {
        std::size_t token_id;                     ///< Associated language token
        std::string object_category;              ///< Object category ("ball", "toy", etc.)
        std::vector<float> visual_features;       ///< Visual object features
        std::vector<float> tactile_features;      ///< Tactile/haptic features
        std::vector<float> auditory_features;     ///< Object-related sounds
        
        // Association strength and learning
        float grounding_strength = 0.0f;          ///< Overall grounding strength
        float visual_grounding_confidence = 0.0f; ///< Visual association confidence
        float tactile_grounding_confidence = 0.0f; ///< Tactile association confidence
        float auditory_grounding_confidence = 0.0f; ///< Auditory association confidence
        
        // Contextual information
        std::vector<float> spatial_context;       ///< Where object was encountered
        std::vector<float> temporal_context;      ///< When object was encountered
        std::string interaction_type;             ///< How object was interacted with
        
        // Learning dynamics
        std::uint64_t exposure_count = 0;         ///< Number of exposures to this object
        std::chrono::steady_clock::time_point first_encounter; ///< First time object was seen
        std::chrono::steady_clock::time_point last_encounter;  ///< Most recent encounter
        float learning_rate = 0.05f;             ///< Rate of association strengthening
        bool is_stable_grounding = false;        ///< Whether grounding has stabilized
    };

    /**
     * @brief Enhanced prosodic pattern learning system
     */
    struct ProsodicPattern {
        std::string pattern_name;                 ///< Name/identifier for the pattern
        std::vector<float> pitch_trajectory;      ///< Pitch contour over time
        std::vector<float> energy_trajectory;     ///< Energy envelope over time
        std::vector<float> rhythm_pattern;        ///< Temporal rhythm structure
        float pattern_stability = 0.0f;          ///< How stable/consistent this pattern is
        
        // Learning and recognition
        std::uint64_t occurrence_count = 0;       ///< How many times pattern was detected
        float recognition_confidence = 0.0f;     ///< Confidence in pattern recognition
        std::vector<std::string> associated_tokens; ///< Tokens that co-occur with pattern
        
        // Attention and learning effects
        float attention_weight = 0.0f;           ///< How much attention this pattern draws
        float learning_boost_factor = 0.0f;     ///< Learning enhancement from this pattern
        bool is_motherese_pattern = false;      ///< Whether this is infant-directed speech
        
        // Temporal dynamics
        std::chrono::steady_clock::time_point first_detected; ///< When first detected
        std::chrono::steady_clock::time_point last_detected;  ///< Most recent detection
        std::vector<std::chrono::steady_clock::time_point> detection_history; ///< Detection timeline
    };

    /**
     * @brief Caregiver interaction context for social learning
     */
    struct CaregiverContext {
        std::vector<float> face_embedding;        ///< Caregiver face features
        float emotional_valence = 0.0f;          ///< Positive/negative emotional response
        float attention_level = 0.0f;            ///< Caregiver attention intensity
        std::string interaction_type;            ///< Type of interaction ("praise", "correction", etc.)
        std::chrono::steady_clock::time_point timestamp; ///< When interaction occurred
        float response_strength = 0.0f;          ///< Overall response strength
    };

    /**
     * @brief Intonation-guided attention system
     */
    struct IntonationGuidedAttention {
        float current_intonation_salience = 0.0f; ///< Current intonation attention level
        std::vector<float> intonation_history;     ///< Recent intonation patterns
        float rising_intonation_preference = 0.8f; ///< Preference for rising intonation
        float falling_intonation_preference = 0.3f; ///< Preference for falling intonation
        
        // Pattern-based attention modulation
        std::unordered_map<std::string, float> pattern_attention_weights; ///< Pattern → attention weight
        float prosodic_attention_threshold = 0.4f; ///< Threshold for prosodic attention activation
        bool is_prosodic_attention_active = false; ///< Whether prosodic attention is currently active
        
        // Learning guidance
        float intonation_learning_boost = 0.0f;   ///< Current learning boost from intonation
        std::vector<std::pair<float, float>> intonation_learning_history; ///< (intonation, learning_boost) pairs
        float adaptive_threshold = 0.4f;          ///< Adaptive threshold based on experience
    };

    /**
     * @brief Face-speech coupling system for proto-word associations
     */
    struct FaceSpeechCoupling {
        std::unordered_map<std::string, float> proto_word_face_associations; ///< Proto-word → face association strength
        std::vector<float> current_face_embedding;                           ///< Current face features
        float coupling_strength = 0.0f;                                     ///< Overall coupling strength
        std::vector<float> coupling_history;                                ///< History of coupling strengths
        std::chrono::steady_clock::time_point last_update;                   ///< Last coupling update
        float motherese_detection_strength = 0.0f;                          ///< Motherese detection strength
        float coupling_learning_rate = 0.1f;                                ///< Learning rate for coupling updates
        float temporal_synchrony = 0.0f;                                    ///< Temporal synchrony measure
        bool is_caregiver_interaction = false;                               ///< Whether current interaction is with caregiver
        float caregiver_recognition_confidence = 0.0f;                      ///< Confidence in caregiver recognition
        float stability_measure = 0.0f;                                     ///< Stability measure for coupling
    };

    /**
     * @brief Sensory experience data structure
     */
    struct SensoryExperience {
        std::string experience_type;                                         ///< Type of sensory experience
        std::vector<float> sensory_data;                                     ///< Raw sensory data
        float salience_score = 0.0f;                                        ///< Importance/attention score
        std::chrono::steady_clock::time_point timestamp;                     ///< When experience occurred
        std::string associated_context;                                      ///< Contextual information
        float emotional_valence = 0.0f;                                     ///< Emotional response
        std::vector<std::string> co_occurring_tokens;                        ///< Tokens that occurred with this experience
        std::vector<float> sensory_pattern;                                  ///< Processed sensory pattern
        float novelty_score = 0.0f;                                         ///< Novelty of the experience
        float experience_quality = 0.0f;                                    ///< Quality of the experience
        float reliability_score = 0.0f;                                     ///< Reliability of the experience
        int repetition_count = 0;                                           ///< Number of repetitions
    };

    /**
     * @brief Visual-linguistic integration features
     */
    struct VisualLanguageFeatures {
        float face_salience = 0.0f;           ///< Face detection confidence
        float gaze_alignment = 0.0f;          ///< Gaze-speech synchronization
        float lip_sync_score = 0.0f;          ///< Lip movement correlation
        float attention_focus = 0.0f;         ///< Visual attention weight
        std::vector<float> face_embedding;    ///< Face recognition features
        std::vector<float> gaze_vector;       ///< Gaze direction coordinates
        std::vector<float> lip_features;      ///< Lip shape/movement features
        std::vector<float> object_features;   ///< Object recognition features
        
        // Cross-modal binding strength
        float speech_vision_coupling = 0.0f;  ///< Temporal alignment score
        float motherese_face_boost = 0.0f;    ///< Infant-directed speech + face
    };

    /**
     * @brief Cross-modal association entry
     */
    struct CrossModalAssociation {
        std::size_t token_id;                 ///< Associated language token
        std::string modality;                 ///< Sensory modality ("vision", "audio", etc.)
        std::vector<float> pattern;           ///< Sensory pattern vector
        float association_strength = 0.0f;    ///< Binding strength
        float temporal_alignment = 0.0f;      ///< Synchronization score
        std::chrono::steady_clock::time_point last_reinforced; ///< Temporal tracking
        
        // Visual-specific features
        VisualLanguageFeatures visual_features; ///< Face/gaze/lip features
        float face_language_confidence = 0.0f;  ///< Face-speech binding confidence
    };

    /**
     * @brief Speech production and output features
     */
    struct SpeechProductionFeatures {
        std::vector<PhonemeCluster> phoneme_sequence;  ///< Sequence of phonemes to produce
        std::vector<float> timing_pattern;             ///< Temporal timing for each phoneme
        std::vector<float> prosody_contour;            ///< Pitch/stress pattern for utterance
        float speech_rate = 1.0f;                     ///< Speaking rate multiplier
        float emotional_coloring = 0.0f;              ///< Emotional expression level
        
        // Visual synchronization
        std::vector<std::vector<float>> lip_motion_sequence; ///< Lip shapes for each phoneme
        std::vector<float> gaze_targets;               ///< Gaze direction during speech
        float facial_expression_intensity = 0.0f;     ///< Expression strength
        
        // Feedback and monitoring
        float confidence_score = 0.0f;                ///< Production confidence
        bool requires_feedback = true;                ///< Whether to monitor output
        std::chrono::steady_clock::time_point start_time; ///< Production start timestamp
    };

    /**
     * @brief Speech output synchronization state
     */
    struct SpeechOutputState {
        bool is_speaking = false;                      ///< Currently producing speech
        std::size_t current_phoneme_index = 0;        ///< Current position in sequence
        float current_time_offset = 0.0f;             ///< Time within current phoneme
        std::vector<float> current_lip_shape;         ///< Current lip configuration
        std::vector<float> current_gaze_direction;    ///< Current gaze target
        
        // Feedback monitoring
        std::vector<float> acoustic_feedback;         ///< Heard audio during production
        float self_monitoring_score = 0.0f;          ///< Self-assessment of output quality
        bool caregiver_attention_detected = false;   ///< Listener attention status
    };

    /**
     * @brief Language learning configuration
     */
    struct Config {
        // Developmental parameters
        float mimicry_learning_rate = 0.01f;     ///< Rate of teacher imitation
        float grounding_strength = 0.5f;         ///< Sensory-symbol association strength
        float narration_threshold = 0.3f;        ///< Activation threshold for internal speech
        
        // Acoustic processing parameters
        float prosody_attention_weight = 0.4f;   ///< Boost for prosodic salience (increased from 0.3f)
        float intonation_threshold = 0.1f;      ///< Hz change for attention trigger (lowered from 0.3f)
        float motherese_boost = 0.4f;           ///< Amplification for infant-directed speech
        float formant_clustering_threshold = 50.0f; ///< Hz threshold for phoneme clustering
        
        // Visual-linguistic integration
        float face_language_coupling = 0.6f;    ///< Face-speech binding strength
        float gaze_attention_weight = 0.4f;     ///< Gaze direction influence
        float lip_sync_threshold = 0.3f;        ///< Minimum lip-speech correlation
        float visual_grounding_boost = 0.5f;    ///< Visual modality reinforcement
        float cross_modal_decay = 0.002f;       ///< Association decay rate (reduced from 0.005f)
        
        // Token similarity and cohesion parameters
        float token_similarity_threshold = 0.3f; ///< Similarity threshold for token clustering (reduced from 0.5f)
        float cohesion_boost_factor = 2.0f;     ///< Boost factor for co-occurring tokens (increased from 1.5f)
        float co_occurrence_bonus = 0.02f;      ///< Bonus per repeated token pair
        
        // Speech production parameters
        float speech_production_rate = 1.0f;    ///< Default speaking rate
        float lip_sync_precision = 0.8f;        ///< Lip-speech synchronization accuracy
        float gaze_coordination_strength = 0.6f; ///< Gaze-speech coupling
        float self_monitoring_weight = 0.4f;    ///< Self-feedback importance
        float caregiver_mimicry_boost = 0.5f;   ///< Boost for caregiver imitation
        bool enable_speech_output = true;       ///< Enable speech production
        bool enable_lip_sync = true;            ///< Enable lip movement generation
        bool enable_gaze_coordination = true;   ///< Enable gaze-speech coupling
        
        // Token management
        std::size_t max_vocabulary_size = 10000; ///< Maximum number of tokens
        std::size_t embedding_dimension = 256;   ///< Token embedding size
        float token_decay_rate = 0.001f;         ///< Unused token forgetting rate
        
        // Developmental timing
        std::uint64_t babbling_duration = 1000;  ///< Steps in babbling stage
        std::uint64_t mimicry_duration = 5000;   ///< Steps in mimicry stage
        std::uint64_t grounding_duration = 10000; ///< Steps in grounding stage
        
        // Multimodal integration
        bool enable_vision_grounding = true;     ///< Link tokens to visual patterns
        bool enable_audio_grounding = true;      ///< Link tokens to audio patterns
        bool enable_action_grounding = true;     ///< Link tokens to motor patterns
        bool enable_face_language_bias = true;   ///< Boost face-speech associations
        
        // Teacher system
        bool enable_teacher_mode = false;        ///< Use external teacher signals
        float teacher_influence = 0.8f;          ///< Strength of teacher guidance
        
        // Babbling Stage enhancements
        bool enable_acoustic_preprocessing = true;      ///< Enable acoustic-first babbling approach
        bool enable_prosodic_embeddings = true;         ///< Enable prosodic pattern embeddings
        bool enable_sound_attention_bias = true;        ///< Enable sound-based attention biasing
        float proto_word_crystallization_rate = 0.05f; ///< Rate of proto-word formation
        float phoneme_stability_threshold = 0.6f;      ///< Threshold for stable phoneme patterns
        float caregiver_response_boost = 0.8f;         ///< Boost for caregiver-reinforced patterns
        std::uint64_t min_occurrences_for_crystallization = 3; ///< Minimum pattern repetitions
        float pattern_similarity_threshold = 0.8f;     ///< Similarity threshold for pattern matching
        
        // Cross-modal integration enhancements
        float multimodal_attention_weight = 0.7f;      ///< Weight for multimodal attention
        float joint_attention_threshold = 0.6f;        ///< Threshold for joint attention detection
        float joint_attention_learning_boost = 0.8f;   ///< Learning boost from joint attention events
        std::uint32_t attention_history_length = 10;   ///< Length of attention history buffer
        float face_speech_coupling_rate = 0.08f;       ///< Rate of face-speech coupling learning
        float caregiver_recognition_boost = 0.9f;      ///< Boost for recognized caregiver faces
        bool enable_enhanced_multimodal_attention = true; ///< Enable enhanced attention system
        float proto_word_face_association_strength = 0.5f; ///< Strength of proto-word face associations
        
        // Grounding association parameters
        float grounding_association_strength = 0.6f;       ///< Base strength for new grounding associations
        float visual_grounding_weight = 0.4f;              ///< Weight for visual grounding
        float tactile_grounding_weight = 0.3f;             ///< Weight for tactile grounding
        float auditory_grounding_weight = 0.3f;            ///< Weight for auditory grounding
        float grounding_stability_threshold = 0.7f;        ///< Threshold for stable grounding
        std::uint64_t min_exposures_for_stable_grounding = 5; ///< Minimum exposures for stability
        
        // Prosodic pattern learning parameters
        float prosodic_pattern_learning_rate = 0.06f;      ///< Rate of prosodic pattern learning
        float intonation_attention_boost = 0.7f;           ///< Attention boost for salient intonation
        float motherese_pattern_boost = 0.9f;              ///< Extra boost for motherese patterns
        float motherese_face_coupling_boost = 0.5f;        ///< Boost for face-speech coupling during motherese
        float prosodic_pattern_stability_threshold = 0.6f; ///< Threshold for stable prosodic patterns
        std::uint64_t min_pattern_occurrences = 3;         ///< Minimum occurrences for pattern recognition
        
        // Intonation-guided learning parameters
        float rising_intonation_learning_boost = 0.8f;     ///< Learning boost for rising intonation
        float falling_intonation_learning_boost = 0.4f;    ///< Learning boost for falling intonation
        float prosodic_attention_adaptation_rate = 0.05f;  ///< Rate of attention threshold adaptation
        bool enable_prosodic_pattern_learning = true;      ///< Enable prosodic pattern learning system
        std::uint64_t prosodic_pattern_history_length = 30; ///< Length of prosodic pattern history
        
        // Sensory experience processing parameters
        std::uint64_t sensory_experience_history_length = 100; ///< Length of sensory experience history
        float salience_threshold = 0.5f;                   ///< Threshold for salient sensory experiences
        float sensory_experience_learning_rate = 0.02f;    ///< Learning rate for sensory experience integration
        
        // Target proto-words for biased generation
        std::vector<std::string> target_proto_words;        ///< Target proto-word patterns for biased generation
    };

    /**
     * @brief Internal narration entry
     */
    struct NarrationEntry {
        std::vector<SymbolicToken> token_sequence; ///< Sequence of tokens
        std::chrono::steady_clock::time_point timestamp; ///< When generated
        float confidence = 0.0f;                 ///< System confidence in narration
        std::string context;                      ///< Situational context
        bool is_self_generated = true;           ///< vs. teacher-provided
    };

    /**
     * @brief Language system statistics
     */
    struct Statistics {
        std::uint64_t total_tokens_generated = 0;
        std::uint64_t successful_mimicry_attempts = 0;
        std::uint64_t grounding_associations_formed = 0;
        std::uint64_t narration_entries = 0;
        float average_token_activation = 0.0f;
        float vocabulary_diversity = 0.0f;
        std::size_t active_vocabulary_size = 0;
        std::size_t total_vocabulary_size = 0;
        float average_cluster_stability = 0.0f;
        float token_activation_entropy = 0.0f;
        std::size_t tokens_stable_over_0_5 = 0;
        DevelopmentalStage current_stage = DevelopmentalStage::Chaos;
    };

private:
    Config config_;
    std::atomic<DevelopmentalStage> current_stage_;
    std::atomic<std::uint64_t> development_step_counter_;
    
    // Core vocabulary and token management
    std::vector<SymbolicToken> vocabulary_;
    std::unordered_map<std::string, std::size_t> token_lookup_;
    mutable std::recursive_mutex vocabulary_mutex_;
    
    // Statistics and tracking
    Statistics stats_;
    mutable std::recursive_mutex stats_mutex_;
    
    // Random number generation
    mutable std::mt19937 rng_;
    mutable std::uniform_real_distribution<float> uniform_dist_;
    
    // Narration system
    std::atomic<bool> narration_active_;
    std::deque<NarrationEntry> internal_narration_;
    mutable std::recursive_mutex narration_mutex_;
    
    // Proto-word crystallization system
    std::vector<ProtoWord> proto_words_;
    std::unordered_map<std::string, std::size_t> proto_word_lookup_;
    mutable std::recursive_mutex proto_word_mutex_;
    
    // Grounding associations system
    std::vector<GroundingAssociation> grounding_associations_;
    std::unordered_map<std::string, std::vector<std::size_t>> object_to_grounding_lookup_;
    std::unordered_map<std::size_t, std::vector<std::size_t>> token_to_grounding_lookup_;
    mutable std::recursive_mutex grounding_associations_mutex_;

    NeuronBiasCallback neuron_bias_callback_;
    
    // Prosodic pattern learning and intonation-guided attention
    std::vector<ProsodicPattern> prosodic_patterns_;
    std::unordered_map<std::string, std::size_t> prosodic_pattern_lookup_;
    IntonationGuidedAttention intonation_attention_state_;
    mutable std::recursive_mutex prosodic_pattern_mutex_;
    
    // Enhanced acoustic processing and pattern recognition
    std::deque<AcousticFeatures> recent_acoustic_features_;
    std::vector<float> prosodic_attention_history_;
    std::unordered_map<std::string, float> learned_prosodic_preferences_;
    mutable std::recursive_mutex acoustic_processing_mutex_;
    
    // Acoustic pattern memory and clustering (restored)
    std::vector<PhonemeCluster> phoneme_clusters_;
    std::unordered_map<std::string, AcousticFeatures> acoustic_memory_;
    mutable std::recursive_mutex acoustic_mutex_;
    
    // Caregiver recognition and interaction tracking
    std::vector<std::vector<float>> known_caregiver_faces_;
    std::unordered_map<std::string, float> caregiver_face_confidences_;
    std::deque<CaregiverContext> recent_caregiver_interactions_;
    CaregiverContext current_caregiver_context_;
    std::vector<CaregiverContext> caregiver_interaction_history_;
    mutable std::recursive_mutex caregiver_recognition_mutex_;
    mutable std::recursive_mutex caregiver_mutex_;
    
    // Enhanced phoneme clustering with stability tracking
    std::unordered_map<std::string, float> phoneme_stability_scores_;
    std::vector<std::pair<std::string, std::uint64_t>> phoneme_usage_history_;
    mutable std::recursive_mutex phoneme_tracking_mutex_;
    
    // Prosodic attention state
    std::vector<float> attention_history_;
    float current_salience_threshold_ = 0.3f;
    std::deque<AcousticFeatures> acoustic_stream_buffer_;
    
    // Sensory experience processing
    std::deque<SensoryExperience> sensory_experience_history_;
    std::unordered_map<std::string, std::vector<SensoryExperience>> experience_type_lookup_;
    std::vector<float> current_sensory_context_;
    mutable std::recursive_mutex sensory_experience_mutex_;
    
    // Teacher system
    std::vector<std::vector<float>> teacher_embeddings_;
    std::vector<std::string> teacher_labels_;
    mutable std::recursive_mutex teacher_mutex_;
    
    // Grounding associations
    std::unordered_map<NeuroForge::NeuronID, std::vector<std::size_t>> neuron_to_tokens_;
    std::unordered_map<std::string, std::vector<std::size_t>> modality_to_tokens_;
    std::vector<CrossModalAssociation> cross_modal_associations_;
    std::unordered_map<std::size_t, std::vector<VisualLanguageFeatures>> token_visual_features_;
    mutable std::recursive_mutex grounding_mutex_;
    
    // Visual-linguistic integration state
    std::vector<float> current_attention_map_;
    std::deque<VisualLanguageFeatures> visual_stream_buffer_;
    float current_face_salience_threshold_ = 0.4f;
    mutable std::recursive_mutex visual_mutex_;
    
    // Speech production state
    SpeechOutputState speech_output_state_;
    std::deque<SpeechProductionFeatures> speech_production_queue_;
    std::vector<float> self_monitoring_history_;
    float current_speech_quality_threshold_ = 0.6f;
    mutable std::recursive_mutex speech_mutex_;
    
    // Multimodal attention state
    MultimodalAttentionState multimodal_attention_state_;
    mutable std::recursive_mutex multimodal_attention_mutex_;
    
    // Face-speech coupling state
    FaceSpeechCoupling face_speech_coupling_;

public:
    explicit LanguageSystem(const Config& config);
    ~LanguageSystem() = default;
    
    // Core lifecycle
    bool initialize();
    void shutdown();
    void reset();
    
    // Developmental progression
    void updateDevelopment(float delta_time);
    DevelopmentalStage getCurrentStage() const noexcept { return current_stage_.load(); }
    void advanceToStage(DevelopmentalStage stage);
    
    // Token management
    std::size_t createToken(const std::string& symbol, TokenType type, 
                           const std::vector<float>& embedding = {});
    SymbolicToken* getToken(const std::string& symbol);
    SymbolicToken* getToken(std::size_t token_id);
    const SymbolicToken* getToken(const std::string& symbol) const;
    const SymbolicToken* getToken(std::size_t token_id) const;
    // Utility: get token id by symbol (returns true if found)
    bool getTokenId(const std::string& symbol, std::size_t& out_token_id) const;
    std::vector<std::size_t> findSimilarTokens(const std::vector<float>& embedding, 
                                              float threshold = 0.8f) const;
    
    // Mimicry learning
    void setTeacherEmbedding(const std::string& label, const std::vector<float>& embedding);
    void processTeacherSignal(const std::string& label, float reward_signal = 1.0f);
    std::vector<float> generateMimicryResponse(const std::vector<float>& teacher_embedding);
    
    // Enhanced multimodal grounding with visual-linguistic integration
    void associateTokenWithNeuron(std::size_t token_id, NeuroForge::NeuronID neuron_id, 
                                  float association_strength = 1.0f);
    void associateTokenWithModality(std::size_t token_id, const std::string& modality, 
                                   const std::vector<float>& pattern, float strength = 1.0f);
    void associateTokenWithVisualFeatures(std::size_t token_id, 
                                         const VisualLanguageFeatures& visual_features,
                                         float confidence = 1.0f);
    
    // Face-speech coupling and cross-modal binding
    void processFaceSpeechEvent(const std::vector<float>& face_embedding,
                               const std::vector<float>& gaze_vector,
                               const std::vector<float>& lip_features,
                               const std::string& spoken_token,
                               float temporal_alignment = 1.0f);
    
    // Sensory experience processing
    void processSensoryExperience(const SensoryExperience& experience);
    
    void updateCrossModalAssociations(const std::vector<CrossModalAssociation>& associations);
    float calculateFaceLanguageConfidence(const VisualLanguageFeatures& visual_features,
                                         const AcousticFeatures& acoustic_features) const;
    
    // Visual attention and salience integration
    void processVisualAttentionMap(const std::vector<float>& attention_map,
                                  const std::vector<std::string>& active_tokens);
    void reinforceVisualGrounding(std::size_t token_id, 
                                 const std::vector<float>& visual_pattern,
                                 float salience_score);
    
    // Speech production and multimodal output
    SpeechProductionFeatures generateSpeechOutput(const std::string& text) const;
    SpeechProductionFeatures generateSpeechOutput(const std::vector<std::string>& token_sequence) const;
    std::vector<PhonemeCluster> generatePhonemeSequence(const std::string& text) const;
    std::vector<std::vector<float>> generateLipMotionSequence(
        const std::vector<PhonemeCluster>& phonemes) const;
    std::vector<float> generateProsodyContour(const std::vector<PhonemeCluster>& phonemes,
                                             float emotional_intensity = 0.0f) const;
    
    // Speech-visual synchronization
    void startSpeechProduction(const SpeechProductionFeatures& speech_features);
    void updateSpeechProduction(float delta_time);
    void stopSpeechProduction();
    SpeechOutputState getCurrentSpeechState() const { return speech_output_state_; }
    
    // Self-monitoring and feedback
    void processSelfAcousticFeedback(const std::vector<float>& heard_audio);
    void processCaregiverResponse(const VisualLanguageFeatures& caregiver_reaction,
                                 const AcousticFeatures& caregiver_audio);
    float calculateSpeechProductionQuality(const SpeechProductionFeatures& intended,
                                          const std::vector<float>& actual_audio) const;
    
    // Caregiver mimicry and joint attention
    void reinforceCaregiverMimicry(const std::string& spoken_token,
                                  const VisualLanguageFeatures& caregiver_features);
    void processJointAttentionEvent(const std::vector<float>& shared_gaze_target,
                                   const std::string& spoken_token);
    
    // Developmental trajectory tracking
    void enableTrajectoryTracking(const std::string& log_directory = "trajectory_logs");
    void captureTrajectorySnapshot();
    void generateDevelopmentalReport();
    
    // Cross-modal pattern retrieval
    std::vector<std::size_t> getTokensForNeuralPattern(const std::vector<NeuroForge::NeuronID>& neurons) const;
    std::vector<std::size_t> getTokensForVisualPattern(const std::vector<float>& visual_pattern,
                                                       float similarity_threshold = 0.7f) const;
    std::vector<CrossModalAssociation> getCrossModalAssociations(std::size_t token_id) const;
    
    // Internal narration
    void enableNarration(bool enable = true) { narration_active_.store(enable); }
    void generateNarration(const std::vector<float>& context_embedding, 
                          const std::string& context_description = "");
    std::vector<NarrationEntry> getRecentNarration(std::size_t count = 10) const;
    void logSelfNarration(const std::vector<std::string>& token_sequence, 
                         float confidence, const std::string& context = "");
    
    // Acoustic processing and prosody analysis
    AcousticFeatures extractAcousticFeatures(const std::vector<float>& audio_samples, 
                                            float sample_rate = 16000.0f) const;
    float calculateSoundSalience(const AcousticFeatures& features) const;
    PhonemeCluster generatePhonemeCluster(const AcousticFeatures& features) const;
    std::vector<PhonemeCluster> clusterAcousticPatterns(
        const std::vector<AcousticFeatures>& feature_sequence) const;
    
    // Missing utility functions for acoustic processing
    std::string phonemeToIPA(const AcousticFeatures& features) const;
    std::vector<float> generateAudioSnippet(const PhonemeCluster& phoneme, float duration_ms) const;
    std::vector<float> generateProsodicallyEnhancedEmbedding(const AcousticFeatures& acoustic_features) const;
    
    // Proto-word crystallization and babbling stage enhancements
    void processProtoWordCrystallization();
    std::size_t createProtoWord(const std::string& pattern, const std::vector<std::string>& phonemes);
    void reinforceProtoWord(std::size_t proto_word_id, float reinforcement_strength);
    void updatePhonemeStability(const std::string& phoneme, float usage_boost = 1.0f);
    float calculatePatternSimilarity(const std::string& pattern1, const std::string& pattern2) const;
    bool shouldCrystallizePattern(const ProtoWord& proto_word) const;
    
    // Grounding associations and semantic anchoring
    std::size_t createGroundingAssociation(std::size_t token_id, const std::string& object_category,
                                          const std::vector<float>& visual_features,
                                          const std::vector<float>& tactile_features = {},
                                          const std::vector<float>& auditory_features = {});
    void reinforceGroundingAssociation(std::size_t grounding_id, float reinforcement_strength);
    void updateGroundingAssociation(std::size_t grounding_id, const std::string& interaction_type,
                                   const std::vector<float>& spatial_context);
    bool isStableGrounding(const GroundingAssociation& grounding) const;
    std::vector<std::size_t> findGroundingAssociationsForToken(std::size_t token_id) const;
    std::vector<std::size_t> findGroundingAssociationsForObject(const std::string& object_category) const;
    
    // Multimodal grounding and semantic anchoring
    void processMultimodalGroundingEvent(const std::string& spoken_token,
                                        const std::vector<float>& visual_features,
                                        const std::vector<float>& tactile_features,
                                        const std::vector<float>& auditory_features,
                                        const std::string& object_category);
    void strengthenSemanticAnchoring(std::size_t token_id, const std::vector<float>& sensory_pattern,
                                    const std::string& modality, float anchoring_strength);
    float calculateSemanticGroundingStrength(std::size_t token_id) const;
    void promoteToSemanticallGrounded(std::size_t token_id);
    
    // Prosodic pattern learning and intonation-guided attention
    void processProsodicPatternLearning(const AcousticFeatures& acoustic_features,
                                       const std::string& co_occurring_token = "");
    std::size_t detectProsodicPattern(const AcousticFeatures& acoustic_features);
    void reinforceProsodicPattern(std::size_t pattern_id, float reinforcement_strength);
    bool isStableProsodicPattern(const ProsodicPattern& pattern) const;
    std::vector<std::size_t> findSimilarProsodicPatterns(const AcousticFeatures& acoustic_features,
                                                        float similarity_threshold = 0.7f) const;
    
    // Intonation-guided attention and learning
    void updateIntonationGuidedAttention(const AcousticFeatures& acoustic_features);
    float calculateIntonationLearningBoost(const AcousticFeatures& acoustic_features) const;
    void adaptProsodicAttentionThreshold(float current_intonation_salience);
    void processIntonationGuidedLearning(const std::string& vocalization,
                                        const AcousticFeatures& acoustic_features);
    
    // Enhanced prosodic analysis and pattern recognition
    ProsodicPattern extractProsodicPattern(const std::vector<AcousticFeatures>& acoustic_sequence) const;
    float calculateProsodicPatternSimilarity(const ProsodicPattern& pattern1,
                                           const ProsodicPattern& pattern2) const;
    void updateProsodicPreferences(const std::string& pattern_name, float preference_update);
    std::vector<std::string> identifyMotheresePatternsInSequence(
        const std::vector<AcousticFeatures>& acoustic_sequence) const;
    
    // Prosodic-guided proto-word learning
    void enhanceProtoWordWithProsodicPattern(const std::string& proto_word_pattern,
                                            const ProsodicPattern& prosodic_pattern);
    float calculateProsodicBoostForProtoWord(const std::string& proto_word_pattern,
                                           const AcousticFeatures& acoustic_features) const;
    void processProsodicallGuidedBabbling(std::size_t num_phonemes,
                                         const ProsodicPattern& target_pattern);
    
    // Advanced acoustic processing for babbling stage
    void processEnhancedAcousticFeatures(const AcousticFeatures& features);
    void trackAcousticPatternEvolution(const AcousticFeatures& features);
    float calculateAcousticNovelty(const AcousticFeatures& features) const;
    void updateAcousticAttentionWeights(const AcousticFeatures& features);
    
    // Enhanced face-speech coupling for babbling stage
    void updateFaceSpeechCoupling(const std::vector<float>& face_embedding,
                                 const AcousticFeatures& acoustic_features,
                                 const std::string& vocalization);
    float calculateFaceSpeechCouplingStrength(const std::vector<float>& face_embedding,
                                             const AcousticFeatures& acoustic_features) const;
    bool detectMotherese(const AcousticFeatures& acoustic_features) const;
    void processCaregiverFaceRecognition(const std::vector<float>& face_embedding);
    
    // Caregiver recognition and learning
    void registerCaregiverFace(const std::vector<float>& face_embedding, 
                              const std::string& caregiver_id);
    bool isCaregiverFace(const std::vector<float>& face_embedding, 
                        float recognition_threshold = 0.8f) const;
    std::string identifyCaregiver(const std::vector<float>& face_embedding) const;
    void updateCaregiverInteractionHistory(const std::string& caregiver_id, 
                                          float interaction_quality);
    
    // Attention-guided proto-word learning
    void processAttentionGuidedLearning(const std::string& vocalization,
                                       const MultimodalAttentionState& attention_state);
    void boostProtoWordBasedOnAttention(const std::string& proto_word_pattern,
                                       float attention_boost);
    float calculateAttentionBasedLearningRate(const MultimodalAttentionState& attention_state) const;
    
    // Enhanced babbling with proto-word bias
    void performEnhancedBabbling(std::size_t num_phonemes = 5);
    std::string generateBiasedPhoneme(float proto_word_bias = 0.3f);
    void trackPhonemeSequencePatterns(const std::vector<std::string>& phoneme_sequence);
    std::vector<std::string> generateProtoWordSequence(const std::string& target_pattern);
    
    // Pattern analysis and crystallization
    void analyzeEmergingPatterns();
    std::vector<std::string> extractPatternsFromVocalization(const std::string& vocalization);
    void promotePatternToCrystallized(std::size_t proto_word_id);
    float calculateCrystallizationReadiness(const ProtoWord& proto_word) const;
    
    // Prosodic attention and salience
    float computeMothereseBias(const AcousticFeatures& features) const;
    float computeIntonationSalience(const std::vector<float>& pitch_contour) const;
    void updateAttentionWeights(const std::vector<AcousticFeatures>& acoustic_stream);
    
    // Enhanced teacher signal processing with acoustic similarity
    void processAcousticTeacherSignal(const std::vector<float>& teacher_audio, 
                                     const std::string& label, float confidence = 1.0f);
    float calculateAcousticSimilarity(const AcousticFeatures& features1, 
                                     const AcousticFeatures& features2) const;
    
    // Babbling and exploration
    std::vector<float> generateRandomEmbedding() const;
    void performBabbling(std::size_t num_tokens = 5);
    void performAcousticBabbling(std::size_t num_phonemes = 5);
    void exploreTokenCombinations(std::size_t sequence_length = 3);
    
    // Integration with neural substrate
    void processNeuralActivation(const std::vector<std::pair<NeuroForge::NeuronID, float>>& activations);
    void influenceNeuralActivation(const std::vector<std::size_t>& token_ids, 
                                  float influence_strength = 0.5f);
    void setNeuronBiasCallback(NeuronBiasCallback cb) { neuron_bias_callback_ = std::move(cb); }
    void setRandomSeed(std::uint32_t seed);
    
    // Analysis and introspection
    Statistics getStatistics() const;
    void updateStatistics();
    std::string generateLanguageReport() const;
    std::vector<std::string> getActiveVocabulary(float activation_threshold = 0.1f) const;
    
    // Serialization
    std::string exportVocabularyToJson() const;
    bool importVocabularyFromJson(const std::string& json_data);
    std::string exportNarrationToJson() const;
    
    // Configuration
    void updateConfig(const Config& new_config);
    Config getConfig() const { return config_; }

    // Token Association Trajectory Tracking structures
    struct TokenAssociationSnapshot {
        std::chrono::steady_clock::time_point timestamp;
        std::size_t token_id;
        std::string symbol;
        float activation_strength;
        std::size_t usage_count;
        std::vector<float> embedding;
        std::vector<std::string> associated_tokens;
        float cluster_stability;
        float cross_modal_strength;
        DevelopmentalStage stage_at_snapshot;
    };

    struct ClusterEvolutionData {
        std::string cluster_name;
        std::vector<std::string> member_tokens;
        float cohesion_score;
        float stability_over_time;
        std::size_t formation_step;
        bool is_proto_word;
    };

    // Token trajectory logging for debugging and analysis
    class TokenTrajectoryLogger {
    private:
        std::vector<TokenAssociationSnapshot> trajectory_log_;
        std::vector<ClusterEvolutionData> cluster_evolution_;
        std::size_t snapshot_interval_;
        std::size_t current_step_;
        
        float calculateCrossModalStrength(const LanguageSystem& language_system, std::size_t token_id);
        void analyzeTrajectoryProgression(std::ostringstream& report, const LanguageSystem& language_system);
        void analyzeCrossModalBinding(std::ostringstream& report, const LanguageSystem& language_system);
        void generateStagePredictions(std::ostringstream& report, const LanguageSystem& language_system);
        
    public:
        std::string log_directory_;
        
        TokenTrajectoryLogger(const std::string& log_dir = "trajectory_logs", 
                             std::size_t interval = 10);
        void captureSnapshot(const LanguageSystem& language_system, std::size_t token_id);
        void writeTrajectoryLog();
        void writeClusterEvolutionLog();
        std::string generateDevelopmentalReport(const LanguageSystem& language_system);
        void reset();
    };
    
    std::unique_ptr<TokenTrajectoryLogger> trajectory_logger_;

private:
    // Internal processing methods
    void processChaosStage(float delta_time);
    void processBabblingStage(float delta_time);
    void processMimicryStage(float delta_time);
    void processGroundingStage(float delta_time);
    void processReflectionStage(float delta_time);
    void processCommunicationStage(float delta_time);
    
    // Token operations
    float calculateTokenSimilarity(const SymbolicToken& token1, const SymbolicToken& token2) const;
    void updateTokenActivation(std::size_t token_id, float activation_delta);
    void decayUnusedTokens(float decay_rate);
    void pruneVocabulary();
    
    // Embedding operations
    std::vector<float> normalizeEmbedding(const std::vector<float>& embedding) const;
    float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) const;
    std::vector<float> interpolateEmbeddings(const std::vector<float>& a, 
                                           const std::vector<float>& b, float alpha) const;
    
    // Caregiver response functions
    float calculateCaregiverResponseStrength(const CaregiverContext& context) const;
    void reinforceBasedOnCaregiverFeedback(const std::string& vocalization, const CaregiverContext& context);
    
    // Multimodal attention functions
    void updateMultimodalAttention(const VisualLanguageFeatures& visual_features, const AcousticFeatures& acoustic_features);
    void processJointAttentionEvent(const std::vector<float>& shared_gaze_target, const std::string& spoken_token, float attention_strength);
    float calculateJointAttentionScore(const VisualLanguageFeatures& visual_features) const;
    void reinforceProtoWordFaceAssociation(const std::string& proto_word_pattern, const std::vector<float>& face_embedding, float association_strength);
    
    // Sensory experience integration
    void integrateSensoryExperienceWithProtoWords(const SensoryExperience& experience);
    void updateSensoryContext(const std::vector<float>& sensory_data);
    void boostProtoWordFromSensoryExperience(const std::string& proto_word, float boost_strength);
    float calculateSensoryExperienceNovelty(const SensoryExperience& experience) const;
    std::vector<SensoryExperience> getSimilarExperiences(const SensoryExperience& experience, float threshold) const;
    void processExperienceDrivenLearning(const std::string& vocalization, const std::vector<SensoryExperience>& concurrent_experiences);
    float calculateExperienceBasedLearningBoost(const SensoryExperience& experience) const;
    
    // Developmental transitions
    bool shouldAdvanceStage() const;
    void onStageTransition(DevelopmentalStage from_stage, DevelopmentalStage to_stage);
    
    // Utility methods
    std::string stageToString(DevelopmentalStage stage) const;
    TokenType inferTokenType(const std::string& symbol) const;
    void logDevelopmentalEvent(const std::string& event, const std::string& details = "");
};

} // namespace Core
} // namespace NeuroForge
