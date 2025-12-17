#pragma once

#include "core/Types.h"
#include "core/LanguageSystem.h"
#include "core/MemoryDB.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <functional>
#include <random>

namespace NeuroForge {
namespace Core {

// Forward declaration to allow pointer member without including full header
class HypergraphBrain;

/**
 * @brief Phase A: Baby Multimodal Mimicry System
 * 
 * Integrates external teacher encoders (CLIP, Whisper, BERT) with NeuroForge's
 * Phase 5 language system to provide structured multimodal learning through
 * mimicry rewards and semantic grounding.
 */
class PhaseAMimicry {
public:
    /**
     * @brief Teacher encoder types for multimodal input
     */
    enum class TeacherType {
        CLIP_Vision,    ///< CLIP image/video encoder
        CLIP_Text,      ///< CLIP text encoder
        Whisper_Audio,  ///< Whisper audio encoder
        Wav2Vec_Audio,  ///< Wav2Vec2 audio encoder
        BERT_Text,      ///< BERT text encoder
        Custom          ///< Custom encoder type
    };

    /**
     * @brief Modality types for multimodal alignment
     */
    using Modality = ::NeuroForge::Modality; // alias to global Modality

    /**
     * @brief Teacher embedding with metadata
     */
    struct TeacherEmbedding {
        std::vector<float> embedding;              ///< Teacher embedding vector
        TeacherType teacher_type;                  ///< Source encoder type
        Modality modality;                         ///< Content modality
        std::string content_id;                    ///< Content identifier
        std::string raw_content;                   ///< Original content (text/filename)
        std::chrono::steady_clock::time_point timestamp; ///< When created
        float confidence = 1.0f;                  ///< Teacher confidence
        std::unordered_map<std::string, float> metadata; ///< Additional properties
    };

    /**
     * @brief Student representation entry for learnable embeddings
     */
    struct StudentEntry {
        std::vector<float> embedding;              ///< Learnable student embedding
        float activation = 0.0f;                   ///< Optional activation magnitude
        std::string content_id;                    ///< Associated teacher content id
        float lr = 0.05f;                          ///< Per-entry learning rate
        float competence = 0.0f;                   ///< Competence estimate in [0,1]
    };

    /**
     * @brief Student mimicry attempt with evaluation
     */
    struct MimicryAttempt {
        std::vector<float> student_embedding;      ///< Student's generated embedding
        std::vector<float> teacher_embedding;     ///< Target teacher embedding
        TeacherType teacher_type;                  ///< Source teacher type
        Modality modality;                         ///< Content modality
        float similarity_score = 0.0f;            ///< Cosine similarity to teacher
        float novelty_score = 0.0f;               ///< Novelty bonus (avoid over-copying)
        float total_reward = 0.0f;                ///< Combined mimicry reward
        std::string content_context;              ///< Associated content description
        std::chrono::steady_clock::time_point timestamp; ///< When attempted
        bool success = false;                     ///< Whether attempt was successful
        // New: teacher metadata for better grounding
        std::string teacher_content_id;           ///< Teacher content identifier used for lookup
        std::string teacher_label;                ///< Teacher-provided label (e.g., "dog_word")
        std::string teacher_aux_data;             ///< Auxiliary data (e.g., canonical word "dog")
    };

    /**
     * @brief Multimodal alignment entry for cross-modal learning
     */
    struct MultimodalAlignment {
        std::string alignment_id;                  ///< Unique alignment identifier
        std::vector<TeacherEmbedding> teacher_embeddings; ///< Multiple teacher inputs
        std::vector<std::size_t> associated_tokens; ///< Language system tokens
        float alignment_strength = 0.0f;          ///< Cross-modal alignment score
        std::chrono::steady_clock::time_point created_at; ///< Creation timestamp
        std::unordered_map<std::string, float> cross_modal_scores; ///< Modality pairs
    };

    /**
     * @brief Substrate routing mode for Phase A reward/neuromodulation
     */
    enum class SubstrateMode { Off, Mirror, Train, Native };

    /**
     * @brief Phase A configuration parameters
     */
        struct Config {
        // Mimicry reward parameters
        float similarity_weight = 0.7f;           ///< Weight for similarity reward
        float novelty_weight = 0.3f;              ///< Weight for novelty bonus
        float similarity_threshold = 0.6f;        ///< Minimum similarity for success
        float novelty_threshold = 0.1f;           ///< Minimum novelty for bonus
        
        // Teacher integration
        std::size_t max_teacher_embeddings = 10000; ///< Maximum stored teacher embeddings
        float teacher_embedding_decay = 0.001f;   ///< Decay rate for old embeddings
        bool enable_cross_modal_alignment = true; ///< Enable multimodal alignment
        
        // Learning parameters
        float mimicry_learning_rate = 0.02f;      ///< Rate of mimicry adaptation
        float grounding_strength = 0.8f;          ///< Strength of semantic grounding
        std::size_t alignment_history_size = 1000; ///< Max alignment history

        // Student representation learning
        bool enable_student_table = true;         ///< Enable learnable student embeddings
        float student_init_std = 0.5f;            ///< Stddev for student init (normalized)
        float student_learning_rate = 0.05f;      ///< Global student LR
        int negative_sampling_k = 0;              ///< Number of negatives per attempt (0 disables)
        float negative_weight = 0.0f;             ///< Repulsion weight for negatives
        bool enable_ema_stabilizer = true;        ///< Enable EMA stabilizer for student updates
        float ema_alpha_min = 0.02f;              ///< Minimum EMA coefficient
        float ema_alpha_max = 0.2f;              ///< Maximum EMA coefficient
            std::size_t replay_interval_steps = 100;
            std::size_t replay_top_k = 5;
            float replay_boost_factor = 1.0f;
            float replay_lr_scale = 1.0f;
            bool replay_include_hard_negatives = true;
            std::size_t replay_hard_k = 3;
            float replay_repulsion_weight = 0.5f;
        
        // Encoder integration
        bool enable_clip_vision = true;           ///< Enable CLIP vision encoder
        bool enable_clip_text = true;             ///< Enable CLIP text encoder
        bool enable_whisper_audio = true;         ///< Enable Whisper audio encoder
        bool enable_bert_text = true;             ///< Enable BERT text encoder
        
        // Performance tuning
        std::size_t embedding_dimension = 512;    ///< Standard embedding dimension
        std::size_t batch_size = 32;              ///< Batch size for processing
        float memory_consolidation_rate = 0.1f;   ///< Rate of memory consolidation
        std::int64_t initial_run_id = 1;          ///< Default run_id for MemoryDB ingestion
        float similarity_gamma = 0.92f;            ///< Exponent to gently boost high similarity
    };

    /**
     * @brief Phase A statistics and metrics
     */
    struct Statistics {
        std::uint64_t total_mimicry_attempts = 0;
        std::uint64_t successful_mimicry_attempts = 0;
        std::uint64_t teacher_embeddings_stored = 0;
        std::uint64_t multimodal_alignments_created = 0;
        float average_similarity_score = 0.0f;
        float average_novelty_score = 0.0f;
        float average_total_reward = 0.0f;
        float cross_modal_alignment_strength = 0.0f;
        std::unordered_map<std::string, std::uint64_t> modality_counts;
        std::unordered_map<std::string, float> teacher_type_performance;
    };

private:
    Config config_;
    std::shared_ptr<LanguageSystem> language_system_;
    std::shared_ptr<MemoryDB> memory_db_;
    
    // Teacher embeddings storage
    std::vector<TeacherEmbedding> teacher_embeddings_;
    std::unordered_map<std::string, std::size_t> content_to_embedding_;
    mutable std::recursive_mutex teacher_mutex_;

    // Student embeddings storage
    std::vector<StudentEntry> student_entries_;
    std::unordered_map<std::string, std::size_t> content_to_student_;
    mutable std::recursive_mutex student_mutex_;
    
    // Mimicry attempts history
    std::vector<MimicryAttempt> mimicry_history_;
    mutable std::recursive_mutex mimicry_mutex_;
    
    // Multimodal alignments
    std::vector<MultimodalAlignment> alignments_;
    std::unordered_map<std::string, std::size_t> alignment_lookup_;
    mutable std::recursive_mutex alignment_mutex_;
    
    // Statistics and performance tracking
    mutable Statistics stats_;
    mutable std::recursive_mutex stats_mutex_;
    
    // Random number generation
    mutable std::mt19937 rng_;
    std::uniform_real_distribution<float> uniform_dist_;
    mutable std::unordered_map<std::size_t, std::vector<float>> projection_weights_;

    // Substrate integration (non-owning pointer & runtime controls)
    HypergraphBrain* brain_ = nullptr;
    SubstrateMode substrate_mode_ = SubstrateMode::Off;
    float reward_scale_ = 1.0f;
    bool zero_reward_ = false;

public:
    explicit PhaseAMimicry(std::shared_ptr<LanguageSystem> language_system,
                          std::shared_ptr<MemoryDB> memory_db,
                          const Config& config);
    ~PhaseAMimicry() = default;
    
    // Core lifecycle
    bool initialize();
    void shutdown();
    void reset();

    // Substrate integration wiring (non-owning brain pointer and runtime controls)
    void setBrain(HypergraphBrain* brain) noexcept { brain_ = brain; }
    void setSubstrateMode(SubstrateMode mode) noexcept { substrate_mode_ = mode; }
    void setRewardScale(float s) noexcept { reward_scale_ = s; }
    void setZeroReward(bool zr) noexcept { zero_reward_ = zr; }
    
    // Teacher embedding management
    std::string addTeacherEmbedding(const std::vector<float>& embedding,
                                   TeacherType teacher_type,
                                   Modality modality,
                                   const std::string& content_id,
                                   const std::string& raw_content = "",
                                   float confidence = 1.0f);
    
    TeacherEmbedding* getTeacherEmbedding(const std::string& content_id);
    std::vector<TeacherEmbedding*> getTeacherEmbeddingsByModality(Modality modality);
    std::vector<TeacherEmbedding*> getTeacherEmbeddingsByType(TeacherType teacher_type);
    
    // Mimicry learning and evaluation
    MimicryAttempt attemptMimicry(const std::vector<float>& student_embedding,
                                 const std::string& teacher_content_id,
                                 const std::string& context = "");

    void loadFromMemoryDB(std::int64_t run_id, int limit);

    // Student table helpers
    StudentEntry* ensureStudentEntry(const std::string& content_id);
    StudentEntry* getStudentEntry(const std::string& content_id);
    void updateStudentEmbedding(const std::string& content_id,
                                const std::vector<float>& teacher_embedding,
                                float reward,
                                bool is_replay = false);
    void repelStudentEmbedding(const std::string& content_id,
                               const std::vector<float>& teacher_embedding,
                               float magnitude,
                               bool is_replay = false);
    
    float calculateSimilarityReward(const std::vector<float>& student_embedding,
                                   const std::vector<float>& teacher_embedding) const;
    
    float calculateNoveltyBonus(const std::vector<float>& student_embedding,
                                const std::vector<TeacherEmbedding>& reference_embeddings) const;
    
    void applyMimicryReward(const MimicryAttempt& attempt);
    
    // Multimodal alignment and cross-modal learning
    std::string createMultimodalAlignment(const std::vector<std::string>& teacher_content_ids,
                                         const std::vector<std::size_t>& language_token_ids,
                                         const std::string& alignment_context = "");
    
    MultimodalAlignment* getAlignment(const std::string& alignment_id);
    std::vector<MultimodalAlignment*> getAlignmentsByTokens(const std::vector<std::size_t>& token_ids);
    
    float calculateCrossModalAlignment(const std::vector<TeacherEmbedding>& embeddings) const;
    void strengthenAlignment(const std::string& alignment_id, float strength_delta);
    
    // Integration with Language System
    void groundLanguageTokens(const std::vector<std::string>& teacher_content_ids,
                             const std::vector<std::string>& token_symbols);
    
    void updateLanguageNarration(const std::vector<std::string>& grounded_tokens,
                                const std::string& context);
    
    std::vector<std::string> generateGroundedNarration(const std::vector<std::string>& teacher_content_ids);
    
    // Teacher encoder integration (placeholder interfaces)
    std::vector<float> processCLIPVision(const std::string& image_path);
    std::vector<float> processCLIPText(const std::string& text);
    std::vector<float> processWhisperAudio(const std::string& audio_path);
    std::vector<float> processWav2VecAudio(const std::string& audio_path);
    std::vector<float> processBERTText(const std::string& text);
    
    // Batch processing for efficiency
    std::vector<std::string> processBatchTeacherEmbeddings(
        const std::vector<std::pair<std::string, TeacherType>>& content_batch,
        Modality modality);
    
    std::vector<MimicryAttempt> processBatchMimicry(
        const std::vector<std::vector<float>>& student_embeddings,
        const std::vector<std::string>& teacher_content_ids);
    
    // Memory and persistence
    void consolidateMemory();
    void saveToMemoryDB();
    void loadFromMemoryDB();
    
    // Statistics and analysis
    Statistics getStatistics() const;
    void updateStatistics();
    std::string generatePhaseAReport() const;
    
    // Configuration management
    void updateConfig(const Config& new_config);
    Config getConfig() const { return config_; }
    
    // Serialization and export
    std::string exportTeacherEmbeddingsToJson() const;
    std::string exportMimicryHistoryToJson() const;
    std::string exportAlignmentsToJson() const;
    bool importTeacherEmbeddingsFromJson(const std::string& json_data);
    
private:
    // Internal processing methods
    void decayOldEmbeddings();
    void pruneEmbeddingHistory();
    void updateCrossModalScores();
    void runReplayCycle();
    std::vector<float> projectStudent(const std::vector<float>& embedding) const;
    
    // Utility methods
    float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) const;
    std::vector<float> normalizeEmbedding(const std::vector<float>& embedding) const;
    float calculateEmbeddingDistance(const std::vector<float>& a, const std::vector<float>& b) const;
    float l2Norm(const std::vector<float>& v) const;
    
    // Teacher encoder helpers (placeholder implementations)
    std::vector<float> callExternalEncoder(const std::string& content,
                                          TeacherType encoder_type) const;
    
    bool validateEmbedding(const std::vector<float>& embedding) const;
    std::string generateContentId(const std::string& content, TeacherType teacher_type) const;
    
    // Statistics helpers
    void updateModalityStats(Modality modality);
    void updateTeacherTypeStats(TeacherType teacher_type, float performance_score);
    
    // Logging and debugging
    void logMimicryAttempt(const MimicryAttempt& attempt) const;
    void logAlignment(const MultimodalAlignment& alignment) const;
    void logTeacherEmbedding(const TeacherEmbedding& embedding) const;
};

/**
 * @brief Factory for creating and managing Phase A Mimicry systems
 */
class PhaseAMimicryFactory {
public:
    static std::unique_ptr<PhaseAMimicry> create(
        std::shared_ptr<LanguageSystem> language_system,
        std::shared_ptr<MemoryDB> memory_db,
        const PhaseAMimicry::Config& config);
    
    static PhaseAMimicry::Config createDefaultConfig();
    static PhaseAMimicry::Config createLightweightConfig();
    static PhaseAMimicry::Config createResearchConfig();
};

} // namespace Core
} // namespace NeuroForge
