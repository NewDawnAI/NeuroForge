#pragma once

#include "memory/EpisodicMemoryManager.h"
#include "memory/SemanticMemory.h"
#include "memory/WorkingMemory.h"
#include "memory/SleepConsolidation.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <random>
#include <mutex>
#include <atomic>

namespace NeuroForge {

// Forward declarations
namespace Core {
    class HypergraphBrain;
    class LearningSystem;
}

namespace Memory {

/**
 * @brief Dream Processing System
 * 
 * Implements explicit dream generation, narrative construction, and creative synthesis
 * during REM sleep phases. Extends the basic sleep consolidation with sophisticated
 * dream content generation, emotional processing, and creative problem-solving.
 */
class DreamProcessor {
public:
    /**
     * @brief Dream content types
     */
    enum class DreamType {
        Episodic,           ///< Dreams based on episodic memories
        Semantic,           ///< Dreams involving semantic knowledge
        Creative,           ///< Creative synthesis dreams
        Emotional,          ///< Emotionally-driven dreams
        ProblemSolving,     ///< Problem-solving dreams
        Nightmare,          ///< Stress/fear processing dreams
        Lucid               ///< Self-aware dreams
    };

    /**
     * @brief Dream narrative structure
     */
    struct DreamNarrative {
        DreamType dream_type{DreamType::Episodic};
        std::string narrative_text;
        std::vector<float> sensory_content;     ///< Visual/auditory dream content
        std::vector<float> emotional_content;   ///< Emotional associations
        std::vector<float> symbolic_content;    ///< Abstract/symbolic elements
        std::vector<std::string> memory_sources; ///< Source memories used
        float coherence_score{0.0f};            ///< Narrative coherence [0,1]
        float creativity_score{0.0f};           ///< Creative novelty [0,1]
        float emotional_intensity{0.0f};        ///< Emotional strength [0,1]
        std::uint64_t dream_duration_ms{0};     ///< Dream duration
        std::uint64_t timestamp{0};             ///< When dream occurred
    };

    /**
     * @brief Dream generation configuration
     */
    struct DreamConfig {
        // Dream frequency and timing
        float dream_probability{0.8f};          ///< Probability of dreaming during REM [0,1]
        std::uint64_t min_dream_duration_ms{5000}; ///< Minimum dream duration
        std::uint64_t max_dream_duration_ms{30000}; ///< Maximum dream duration
        float dream_intensity_factor{1.5f};     ///< Dream content intensity multiplier
        
        // Content generation parameters
        float episodic_weight{0.4f};            ///< Weight for episodic content
        float semantic_weight{0.3f};            ///< Weight for semantic content
        float creative_weight{0.2f};            ///< Weight for creative synthesis
        float emotional_weight{0.1f};           ///< Weight for emotional processing
        
        // Narrative construction
        bool enable_narrative_construction{true}; ///< Enable story-like dreams
        bool enable_symbolic_processing{true};   ///< Enable symbolic dream content
        bool enable_creative_synthesis{true};    ///< Enable creative combinations
        bool enable_problem_solving{true};       ///< Enable problem-solving dreams
        
        // Dream types
        float creative_dream_probability{0.3f};  ///< Probability of creative dreams
        float nightmare_probability{0.1f};       ///< Probability of nightmares
        float lucid_dream_probability{0.05f};    ///< Probability of lucid dreams
        float problem_solving_probability{0.2f}; ///< Probability of problem-solving dreams
        
        // Memory integration
        std::size_t max_memory_sources{10};      ///< Max memories to combine per dream
        float memory_distortion_factor{0.3f};    ///< How much to distort memories [0,1]
        float cross_modal_blending{0.5f};        ///< Cross-modal content blending [0,1]
        
        // Emotional processing
        bool enable_emotional_regulation{true};  ///< Enable emotional processing
        float stress_processing_weight{0.7f};    ///< Weight for stress/trauma processing
        float positive_emotion_boost{0.2f};      ///< Boost for positive emotions
    };

private:
    // Configuration and state
    DreamConfig config_;
    mutable std::mutex dream_mutex_;
    std::atomic<bool> dreaming_active_{false};
    std::atomic<DreamType> current_dream_type_{DreamType::Episodic};
    
    // Memory system references
    EpisodicMemoryManager* episodic_memory_{nullptr};
    SemanticMemory* semantic_memory_{nullptr};
    WorkingMemory* working_memory_{nullptr};
    SleepConsolidation* sleep_consolidation_{nullptr};
    NeuroForge::Core::HypergraphBrain* brain_{nullptr};
    NeuroForge::Core::LearningSystem* learning_system_{nullptr};
    
    // Dream storage and analysis
    std::vector<DreamNarrative> dream_history_;
    std::unordered_map<DreamType, std::vector<DreamNarrative>> dreams_by_type_;
    std::vector<std::string> symbolic_dictionary_;
    std::vector<std::string> narrative_templates_;
    
    // Statistics
    std::atomic<std::uint64_t> total_dreams_generated_{0};
    std::atomic<std::uint64_t> total_dream_time_ms_{0};
    std::atomic<std::uint64_t> creative_dreams_count_{0};
    std::atomic<std::uint64_t> problem_solving_dreams_count_{0};
    std::atomic<std::uint64_t> nightmares_count_{0};
    std::atomic<std::uint64_t> lucid_dreams_count_{0};
    
    // Random generation
    mutable std::mt19937 dream_generator_;
    
public:
    /**
     * @brief Constructor with configuration
     * @param config Dream processing configuration
     */
    explicit DreamProcessor(const DreamConfig& config);
    
    /**
     * @brief Destructor
     */
    ~DreamProcessor() = default;
    
    // System Registration
    
    /**
     * @brief Register episodic memory system
     * @param episodic_memory Episodic memory manager
     */
    void registerEpisodicMemory(EpisodicMemoryManager* episodic_memory);
    
    /**
     * @brief Register semantic memory system
     * @param semantic_memory Semantic memory manager
     */
    void registerSemanticMemory(SemanticMemory* semantic_memory);
    
    /**
     * @brief Register working memory system
     * @param working_memory Working memory manager
     */
    void registerWorkingMemory(WorkingMemory* working_memory);
    
    /**
     * @brief Register sleep consolidation system
     * @param sleep_consolidation Sleep consolidation manager
     */
    void registerSleepConsolidation(SleepConsolidation* sleep_consolidation);
    
    /**
     * @brief Register brain system
     * @param brain Hypergraph brain
     */
    void registerBrain(NeuroForge::Core::HypergraphBrain* brain);
    
    /**
     * @brief Register learning system
     * @param learning_system Learning system
     */
    void registerLearningSystem(NeuroForge::Core::LearningSystem* learning_system);
    
    // Dream Generation
    
    /**
     * @brief Generate a dream during REM sleep
     * @param rem_duration_ms Duration of REM sleep phase
     * @param emotional_state Current emotional state vector
     * @param stress_level Current stress level [0,1]
     * @return Generated dream narrative
     */
    DreamNarrative generateDream(std::uint64_t rem_duration_ms,
                                const std::vector<float>& emotional_state = {},
                                float stress_level = 0.0f);
    
    /**
     * @brief Generate specific type of dream
     * @param dream_type Type of dream to generate
     * @param duration_ms Dream duration
     * @param context_data Optional context for dream generation
     * @return Generated dream narrative
     */
    DreamNarrative generateSpecificDream(DreamType dream_type,
                                        std::uint64_t duration_ms,
                                        const std::vector<float>& context_data = {});
    
    /**
     * @brief Process dreams during REM sleep phase
     * @param rem_duration_ms Duration of REM phase
     * @param emotional_context Current emotional context
     * @return Number of dreams processed
     */
    std::size_t processREMDreams(std::uint64_t rem_duration_ms,
                                const std::vector<float>& emotional_context = {});
    
    // Dream Content Generation
    
    /**
     * @brief Generate episodic dream content
     * @param source_episodes Episodes to base dream on
     * @param distortion_factor How much to distort memories [0,1]
     * @return Dream content vector
     */
    std::vector<float> generateEpisodicContent(const std::vector<EnhancedEpisode>& source_episodes,
                                              float distortion_factor = 0.3f);
    
    /**
     * @brief Generate creative synthesis content
     * @param concept_vectors Semantic concepts to combine
     * @param creativity_level Creativity intensity [0,1]
     * @return Creative dream content
     */
    std::vector<float> generateCreativeContent(const std::vector<std::vector<float>>& concept_vectors,
                                              float creativity_level = 0.7f);
    
    /**
     * @brief Generate problem-solving dream content
     * @param problem_context Problem representation
     * @param solution_hints Potential solution elements
     * @return Problem-solving dream content
     */
    std::vector<float> generateProblemSolvingContent(const std::vector<float>& problem_context,
                                                    const std::vector<std::vector<float>>& solution_hints = {});
    
    /**
     * @brief Generate emotional processing content
     * @param emotional_memories Emotionally charged memories
     * @param regulation_target Target emotional state
     * @return Emotional processing content
     */
    std::vector<float> generateEmotionalContent(const std::vector<EnhancedEpisode>& emotional_memories,
                                               const std::vector<float>& regulation_target = {});
    
    // Narrative Construction
    
    /**
     * @brief Construct dream narrative from content
     * @param dream_content Raw dream content vector
     * @param dream_type Type of dream
     * @param coherence_target Target coherence level [0,1]
     * @return Constructed narrative text
     */
    std::string constructNarrative(const std::vector<float>& dream_content,
                                  DreamType dream_type,
                                  float coherence_target = 0.6f);
    
    /**
     * @brief Add symbolic elements to dream
     * @param base_content Base dream content
     * @param symbolic_intensity Intensity of symbolic processing [0,1]
     * @return Content with symbolic elements
     */
    std::vector<float> addSymbolicElements(const std::vector<float>& base_content,
                                          float symbolic_intensity = 0.5f);
    
    /**
     * @brief Blend cross-modal content
     * @param visual_content Visual dream elements
     * @param auditory_content Auditory dream elements
     * @param tactile_content Tactile dream elements
     * @param blend_factor Blending intensity [0,1]
     * @return Blended multimodal content
     */
    std::vector<float> blendCrossModalContent(const std::vector<float>& visual_content,
                                             const std::vector<float>& auditory_content,
                                             const std::vector<float>& tactile_content,
                                             float blend_factor = 0.5f);
    
    // Dream Analysis and Storage
    
    /**
     * @brief Analyze dream for insights and learning
     * @param dream Dream narrative to analyze
     * @return Analysis results and insights
     */
    struct DreamAnalysis {
        float novelty_score{0.0f};
        float problem_solving_potential{0.0f};
        float emotional_processing_value{0.0f};
        float memory_consolidation_benefit{0.0f};
        std::vector<std::string> insights;
        std::vector<std::string> creative_connections;
        bool requires_further_processing{false};
    };
    DreamAnalysis analyzeDream(const DreamNarrative& dream);
    
    /**
     * @brief Store dream in dream history
     * @param dream Dream to store
     * @param analysis Optional analysis results
     */
    void storeDream(const DreamNarrative& dream, const DreamAnalysis& analysis);
    
    /**
     * @brief Get dreams by type
     * @param dream_type Type of dreams to retrieve
     * @param max_dreams Maximum number of dreams to return
     * @return Dreams of specified type
     */
    std::vector<DreamNarrative> getDreamsByType(DreamType dream_type, std::size_t max_dreams = 10) const;
    
    /**
     * @brief Get recent dreams
     * @param hours_back How many hours back to look
     * @param max_dreams Maximum number of dreams to return
     * @return Recent dreams
     */
    std::vector<DreamNarrative> getRecentDreams(std::uint64_t hours_back = 24, std::size_t max_dreams = 10) const;
    
    // Statistics and Configuration
    
    /**
     * @brief Dream processing statistics
     */
    struct Statistics {
        std::uint64_t total_dreams_generated{0};
        std::uint64_t total_dream_time_ms{0};
        std::uint64_t creative_dreams_count{0};
        std::uint64_t problem_solving_dreams_count{0};
        std::uint64_t nightmares_count{0};
        std::uint64_t lucid_dreams_count{0};
        float average_dream_duration_ms{0.0f};
        float average_coherence_score{0.0f};
        float average_creativity_score{0.0f};
        float average_emotional_intensity{0.0f};
        DreamType most_common_dream_type{DreamType::Episodic};
        bool dreaming_active{false};
        bool all_systems_registered{false};
    };
    
    /**
     * @brief Get comprehensive dream processing statistics
     * @return Current statistics
     */
    Statistics getStatistics() const;
    
    /**
     * @brief Update dream processing configuration
     * @param new_config New configuration
     */
    void setConfig(const DreamConfig& new_config);
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    const DreamConfig& getConfig() const;
    
    /**
     * @brief Check if all required systems are registered
     * @return True if all systems are available
     */
    bool areAllSystemsRegistered() const;
    
    /**
     * @brief Check if dream processor is operational
     * @return True if system is working properly
     */
    bool isOperational() const;
    
    /**
     * @brief Check if currently dreaming
     * @return True if dream generation is active
     */
    bool isDreaming() const;
    
private:
    // Internal dream generation methods
    
    /**
     * @brief Select dream type based on context
     * @param emotional_state Current emotional state
     * @param stress_level Current stress level
     * @param recent_experiences Recent memory episodes
     * @return Selected dream type
     */
    DreamType selectDreamType(const std::vector<float>& emotional_state,
                             float stress_level,
                             const std::vector<EnhancedEpisode>& recent_experiences);
    
    /**
     * @brief Calculate dream duration
     * @param dream_type Type of dream
     * @param available_time Available REM time
     * @return Calculated dream duration
     */
    std::uint64_t calculateDreamDuration(DreamType dream_type, std::uint64_t available_time);
    
    /**
     * @brief Select source memories for dream
     * @param dream_type Type of dream
     * @param max_sources Maximum number of source memories
     * @return Selected source memories
     */
    std::vector<EnhancedEpisode> selectSourceMemories(DreamType dream_type, std::size_t max_sources);
    
    /**
     * @brief Apply memory distortion
     * @param original_content Original memory content
     * @param distortion_factor Distortion intensity [0,1]
     * @return Distorted content
     */
    std::vector<float> applyMemoryDistortion(const std::vector<float>& original_content,
                                            float distortion_factor);
    
    /**
     * @brief Initialize symbolic dictionary
     */
    void initializeSymbolicDictionary();
    
    /**
     * @brief Initialize narrative templates
     */
    void initializeNarrativeTemplates();
    
    /**
     * @brief Calculate coherence score
     * @param dream_content Dream content vector
     * @param narrative_text Narrative text
     * @return Coherence score [0,1]
     */
    float calculateCoherenceScore(const std::vector<float>& dream_content,
                                 const std::string& narrative_text);
    
    /**
     * @brief Calculate creativity score
     * @param dream_content Dream content vector
     * @param source_memories Source memories used
     * @return Creativity score [0,1]
     */
    float calculateCreativityScore(const std::vector<float>& dream_content,
                                  const std::vector<EnhancedEpisode>& source_memories);
    
    /**
     * @brief Get current timestamp in milliseconds
     * @return Current timestamp
     */
    std::uint64_t getCurrentTimestamp() const;
};

} // namespace Memory
} // namespace NeuroForge