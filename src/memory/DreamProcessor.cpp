#include "memory/DreamProcessor.h"
#include "core/HypergraphBrain.h"
#include "core/LearningSystem.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <cmath>
#include <numeric>

namespace NeuroForge {
namespace Memory {

DreamProcessor::DreamProcessor(const DreamConfig& config)
    : config_(config)
    , dream_generator_(std::chrono::steady_clock::now().time_since_epoch().count())
{
    initializeSymbolicDictionary();
    initializeNarrativeTemplates();
}

// System Registration

void DreamProcessor::registerEpisodicMemory(EpisodicMemoryManager* episodic_memory) {
    std::lock_guard<std::mutex> lock(dream_mutex_);
    episodic_memory_ = episodic_memory;
}

void DreamProcessor::registerSemanticMemory(SemanticMemory* semantic_memory) {
    std::lock_guard<std::mutex> lock(dream_mutex_);
    semantic_memory_ = semantic_memory;
}

void DreamProcessor::registerWorkingMemory(WorkingMemory* working_memory) {
    std::lock_guard<std::mutex> lock(dream_mutex_);
    working_memory_ = working_memory;
}

void DreamProcessor::registerSleepConsolidation(SleepConsolidation* sleep_consolidation) {
    std::lock_guard<std::mutex> lock(dream_mutex_);
    sleep_consolidation_ = sleep_consolidation;
}

void DreamProcessor::registerBrain(NeuroForge::Core::HypergraphBrain* brain) {
    std::lock_guard<std::mutex> lock(dream_mutex_);
    brain_ = brain;
}

void DreamProcessor::registerLearningSystem(NeuroForge::Core::LearningSystem* learning_system) {
    std::lock_guard<std::mutex> lock(dream_mutex_);
    learning_system_ = learning_system;
}

// Dream Generation

DreamProcessor::DreamNarrative DreamProcessor::generateDream(
    std::uint64_t rem_duration_ms,
    const std::vector<float>& emotional_state,
    float stress_level)
{
    std::lock_guard<std::mutex> lock(dream_mutex_);
    
    if (!areAllSystemsRegistered()) {
        return DreamNarrative{}; // Return empty dream if systems not ready
    }
    
    dreaming_active_ = true;
    
    // Get recent experiences for context
    std::vector<EnhancedEpisode> recent_experiences;
    if (episodic_memory_) {
        recent_experiences = episodic_memory_->getRecentEpisodes(24 * 60 * 60 * 1000, 20); // Last 24 hours
    }
    
    // Select dream type based on context
    DreamType dream_type = selectDreamType(emotional_state, stress_level, recent_experiences);
    current_dream_type_ = dream_type;
    
    // Calculate dream duration
    std::uint64_t dream_duration = calculateDreamDuration(dream_type, rem_duration_ms);
    
    // Generate the dream
    DreamNarrative dream = generateSpecificDream(dream_type, dream_duration, emotional_state);
    
    // Store and analyze the dream
    DreamAnalysis analysis = analyzeDream(dream);
    storeDream(dream, analysis);
    
    // Update statistics
    total_dreams_generated_++;
    total_dream_time_ms_ += dream_duration;
    
    switch (dream_type) {
        case DreamType::Creative:
            creative_dreams_count_++;
            break;
        case DreamType::ProblemSolving:
            problem_solving_dreams_count_++;
            break;
        case DreamType::Nightmare:
            nightmares_count_++;
            break;
        case DreamType::Lucid:
            lucid_dreams_count_++;
            break;
        default:
            break;
    }
    
    dreaming_active_ = false;
    return dream;
}

DreamProcessor::DreamNarrative DreamProcessor::generateSpecificDream(
    DreamType dream_type,
    std::uint64_t duration_ms,
    const std::vector<float>& context_data)
{
    DreamNarrative dream;
    dream.dream_type = dream_type;
    dream.dream_duration_ms = duration_ms;
    dream.timestamp = getCurrentTimestamp();
    
    // Select source memories
    std::vector<EnhancedEpisode> source_memories = selectSourceMemories(dream_type, config_.max_memory_sources);
    
    // Generate dream content based on type
    std::vector<float> dream_content;
    
    switch (dream_type) {
        case DreamType::Episodic:
            dream_content = generateEpisodicContent(source_memories, config_.memory_distortion_factor);
            break;
            
        case DreamType::Semantic: {
            std::vector<std::vector<float>> concept_vectors;
            if (semantic_memory_) {
                // Get semantic concepts related to recent experiences
                for (const auto& episode : source_memories) {
                    // Use context_tag as content identifier for semantic lookup
                    if (!episode.context_tag.empty()) {
                        // First try to find the concept by label to get its ID
                        auto concept_ptr = semantic_memory_->retrieveConceptByLabel(episode.context_tag);
                        if (concept_ptr) {
                            // Find the concept ID from label_to_id mapping
                            // Since we can't access private members, we'll use a different approach
                            // Let's use findSimilarConcepts with the concept's feature vector
                            auto similar_concepts = semantic_memory_->findSimilarConcepts(concept_ptr->feature_vector, 5);
                            for (const auto& concept_pair : similar_concepts) {
                                concept_vectors.push_back(concept_pair.first.feature_vector);
                            }
                        }
                    }
                }
            }
            dream_content = generateCreativeContent(concept_vectors, 0.5f);
            break;
        }
        
        case DreamType::Creative:
            dream_content = generateCreativeContent(std::vector<std::vector<float>>(), 0.8f);
            break;
            
        case DreamType::Emotional:
            dream_content = generateEmotionalContent(source_memories, context_data);
            break;
            
        case DreamType::ProblemSolving:
            dream_content = generateProblemSolvingContent(context_data);
            break;
            
        case DreamType::Nightmare: {
            // Generate stress/fear processing content
            std::vector<EnhancedEpisode> stressful_memories;
            for (const auto& episode : source_memories) {
                if (episode.emotional_weight > 0.7f) { // High emotional intensity
                    stressful_memories.push_back(episode);
                }
            }
            dream_content = generateEmotionalContent(stressful_memories, context_data);
            // Amplify negative emotions for nightmare processing
            for (auto& val : dream_content) {
                if (val < 0) val *= 1.5f; // Amplify negative values
            }
            break;
        }
        
        case DreamType::Lucid: {
            // Generate self-aware dream content
            dream_content = generateCreativeContent(std::vector<std::vector<float>>(), 0.9f);
            // Add metacognitive elements
            std::vector<float> metacognitive_elements(100, 0.0f);
            std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
            for (auto& val : metacognitive_elements) {
                val = dist(dream_generator_);
            }
            dream_content.insert(dream_content.end(), metacognitive_elements.begin(), metacognitive_elements.end());
            break;
        }
    }
    
    // Add symbolic elements if enabled
    if (config_.enable_symbolic_processing) {
        dream_content = addSymbolicElements(dream_content, 0.6f);
    }
    
    // Store content and metadata
    dream.sensory_content = dream_content;
    
    // Extract emotional and symbolic content
    if (dream_content.size() >= 300) {
        dream.emotional_content = std::vector<float>(dream_content.begin() + 100, dream_content.begin() + 200);
        dream.symbolic_content = std::vector<float>(dream_content.begin() + 200, dream_content.begin() + 300);
    }
    
    // Store memory sources
    for (size_t i = 0; i < source_memories.size(); ++i) {
        dream.memory_sources.push_back("Episode_" + std::to_string(i) + "_" + std::to_string(source_memories[i].timestamp_ms));
    }
    
    // Construct narrative if enabled
    if (config_.enable_narrative_construction) {
        dream.narrative_text = constructNarrative(dream_content, dream_type, 0.6f);
    }
    
    // Calculate scores
    dream.coherence_score = calculateCoherenceScore(dream_content, dream.narrative_text);
    dream.creativity_score = calculateCreativityScore(dream_content, source_memories);
    
    // Calculate emotional intensity
    if (!dream.emotional_content.empty()) {
        float sum = std::accumulate(dream.emotional_content.begin(), dream.emotional_content.end(), 0.0f,
                                   [](float acc, float val) { return acc + std::abs(val); });
        dream.emotional_intensity = sum / dream.emotional_content.size();
    }
    
    return dream;
}

std::size_t DreamProcessor::processREMDreams(
    std::uint64_t rem_duration_ms,
    const std::vector<float>& emotional_context)
{
    std::size_t dreams_generated = 0;
    std::uint64_t remaining_time = rem_duration_ms;
    
    while (remaining_time > config_.min_dream_duration_ms) {
        // Check if we should generate a dream
        std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
        if (prob_dist(dream_generator_) > config_.dream_probability) {
            break; // No more dreams this REM cycle
        }
        
        // Generate a dream
        DreamNarrative dream = generateDream(remaining_time, emotional_context);
        
        if (dream.dream_duration_ms > 0) {
            dreams_generated++;
            remaining_time -= dream.dream_duration_ms;
            
            // Add some time between dreams
            std::uint64_t inter_dream_time = std::min(remaining_time, static_cast<std::uint64_t>(2000));
            remaining_time -= inter_dream_time;
        } else {
            break; // Failed to generate dream
        }
    }
    
    return dreams_generated;
}

// Dream Content Generation

std::vector<float> DreamProcessor::generateEpisodicContent(
    const std::vector<EnhancedEpisode>& source_episodes,
    float distortion_factor)
{
    std::vector<float> content;
    content.reserve(1000); // Reserve space for efficiency
    
    if (source_episodes.empty()) {
        // Generate random episodic-like content
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        for (int i = 0; i < 500; ++i) {
            content.push_back(dist(dream_generator_));
        }
        return content;
    }
    
    // Combine and distort episodic memories
    for (const auto& episode : source_episodes) {
        // Use sensory_state as the primary content for memory distortion
        std::vector<float> distorted_content = applyMemoryDistortion(episode.sensory_state, distortion_factor);
        content.insert(content.end(), distorted_content.begin(), distorted_content.end());
        
        if (content.size() > 1000) break; // Limit content size
    }
    
    // Ensure minimum size
    while (content.size() < 500) {
        std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
        content.push_back(dist(dream_generator_));
    }
    
    return content;
}

std::vector<float> DreamProcessor::generateCreativeContent(
    const std::vector<std::vector<float>>& concept_vectors,
    float creativity_level)
{
    std::vector<float> content;
    content.reserve(1000);
    
    if (concept_vectors.empty()) {
        // Generate purely creative content
        std::uniform_real_distribution<float> dist(-creativity_level, creativity_level);
        for (int i = 0; i < 800; ++i) {
            content.push_back(dist(dream_generator_));
        }
    } else {
        // Blend and transform concept vectors creatively
        std::uniform_int_distribution<std::size_t> concept_dist(0, concept_vectors.size() - 1);
        std::uniform_real_distribution<float> blend_dist(0.0f, creativity_level);
        
        for (int i = 0; i < 800; ++i) {
            float value = 0.0f;
            
            // Blend multiple concepts
            for (int j = 0; j < 3; ++j) {
                const auto& concept_vector = concept_vectors[concept_dist(dream_generator_)];
                if (!concept_vector.empty()) {
                    std::uniform_int_distribution<std::size_t> idx_dist(0, concept_vector.size() - 1);
                    value += concept_vector[idx_dist(dream_generator_)] * blend_dist(dream_generator_);
                }
            }
            
            // Add creative noise
            value += blend_dist(dream_generator_) * (blend_dist(dream_generator_) - 0.5f);
            content.push_back(std::tanh(value)); // Normalize
        }
    }
    
    return content;
}

std::vector<float> DreamProcessor::generateProblemSolvingContent(
    const std::vector<float>& problem_context,
    const std::vector<std::vector<float>>& solution_hints)
{
    std::vector<float> content;
    content.reserve(1000);
    
    // Start with problem context
    if (!problem_context.empty()) {
        content.insert(content.end(), problem_context.begin(), problem_context.end());
    }
    
    // Add solution exploration
    std::uniform_real_distribution<float> exploration_dist(-0.8f, 0.8f);
    for (int i = 0; i < 400; ++i) {
        content.push_back(exploration_dist(dream_generator_));
    }
    
    // Incorporate solution hints if available
    for (const auto& hint : solution_hints) {
        if (content.size() >= 1000) break;
        
        // Transform and blend hints
        for (float val : hint) {
            if (content.size() >= 1000) break;
            content.push_back(val * exploration_dist(dream_generator_));
        }
    }
    
    // Ensure minimum size
    while (content.size() < 600) {
        content.push_back(exploration_dist(dream_generator_));
    }
    
    return content;
}

std::vector<float> DreamProcessor::generateEmotionalContent(
    const std::vector<EnhancedEpisode>& emotional_memories,
    const std::vector<float>& regulation_target)
{
    std::vector<float> content;
    content.reserve(1000);
    
    // Process emotional memories
    for (const auto& memory : emotional_memories) {
        // Extract emotional components from sensory state
        std::vector<float> emotional_component;
        if (memory.sensory_state.size() > 100) {
            emotional_component = std::vector<float>(memory.sensory_state.begin(), memory.sensory_state.begin() + 100);
        } else {
            emotional_component = memory.sensory_state;
        }
        
        // Amplify emotional content
        for (float& val : emotional_component) {
            val *= (1.0f + memory.emotional_weight * config_.stress_processing_weight);
        }
        
        content.insert(content.end(), emotional_component.begin(), emotional_component.end());
        
        if (content.size() > 800) break;
    }
    
    // Add regulation target if provided
    if (!regulation_target.empty()) {
        std::uniform_real_distribution<float> regulation_dist(0.5f, 1.0f);
        for (float target_val : regulation_target) {
            if (content.size() >= 1000) break;
            content.push_back(target_val * regulation_dist(dream_generator_));
        }
    }
    
    // Ensure minimum size
    std::uniform_real_distribution<float> emotion_dist(-1.0f, 1.0f);
    while (content.size() < 600) {
        content.push_back(emotion_dist(dream_generator_));
    }
    
    return content;
}

// Narrative Construction

std::string DreamProcessor::constructNarrative(
    const std::vector<float>& dream_content,
    DreamType dream_type,
    float coherence_target)
{
    if (dream_content.empty() || narrative_templates_.empty()) {
        return "A formless dream of swirling sensations and fleeting impressions.";
    }
    
    std::ostringstream narrative;
    
    // Select narrative template based on dream type
    std::string template_base;
    switch (dream_type) {
        case DreamType::Episodic:
            template_base = "I found myself reliving a memory, but everything was different. ";
            break;
        case DreamType::Creative:
            template_base = "In this dream, impossible things became possible. ";
            break;
        case DreamType::ProblemSolving:
            template_base = "The solution appeared to me in a dream, clear as daylight. ";
            break;
        case DreamType::Emotional:
            template_base = "Emotions flowed through the dream like a river of feeling. ";
            break;
        case DreamType::Nightmare:
            template_base = "The dream began with unease, growing into something darker. ";
            break;
        case DreamType::Lucid:
            template_base = "I realized I was dreaming, and with that awareness came power. ";
            break;
        default:
            template_base = "The dream unfolded like a story written in light and shadow. ";
            break;
    }
    
    narrative << template_base;
    
    // Analyze dream content to generate narrative elements
    std::vector<std::string> narrative_elements;
    
    // Sample content to generate narrative elements
    std::uniform_int_distribution<std::size_t> content_dist(0, dream_content.size() - 1);
    
    for (int i = 0; i < 5; ++i) {
        float content_val = dream_content[content_dist(dream_generator_)];
        
        if (content_val > 0.7f) {
            narrative_elements.push_back("brilliant light illuminated the scene");
        } else if (content_val > 0.3f) {
            narrative_elements.push_back("familiar faces appeared and disappeared");
        } else if (content_val > -0.3f) {
            narrative_elements.push_back("the landscape shifted and changed");
        } else if (content_val > -0.7f) {
            narrative_elements.push_back("shadows danced at the edges of perception");
        } else {
            narrative_elements.push_back("darkness enveloped everything");
        }
    }
    
    // Construct coherent narrative
    for (std::size_t i = 0; i < narrative_elements.size(); ++i) {
        narrative << narrative_elements[i];
        if (i < narrative_elements.size() - 1) {
            narrative << ", and then ";
        } else {
            narrative << ". ";
        }
    }
    
    // Add dream-specific ending
    switch (dream_type) {
        case DreamType::Lucid:
            narrative << "With lucid awareness, I shaped the dream to my will.";
            break;
        case DreamType::Nightmare:
            narrative << "I awoke with a start, the dream's intensity still lingering.";
            break;
        case DreamType::ProblemSolving:
            narrative << "The answer crystallized in my mind as the dream faded.";
            break;
        default:
            narrative << "The dream dissolved like morning mist, leaving only impressions.";
            break;
    }
    
    return narrative.str();
}

std::vector<float> DreamProcessor::addSymbolicElements(
    const std::vector<float>& base_content,
    float symbolic_intensity)
{
    std::vector<float> enhanced_content = base_content;
    
    if (symbolic_dictionary_.empty()) {
        return enhanced_content; // No symbolic processing available
    }
    
    // Add symbolic transformations
    std::uniform_real_distribution<float> symbol_dist(-symbolic_intensity, symbolic_intensity);
    std::uniform_int_distribution<std::size_t> position_dist(0, enhanced_content.size() - 1);
    
    // Apply symbolic transformations at random positions
    int num_transformations = static_cast<int>(enhanced_content.size() * symbolic_intensity * 0.1f);
    
    for (int i = 0; i < num_transformations; ++i) {
        std::size_t pos = position_dist(dream_generator_);
        
        // Apply symbolic transformation (mathematical operations representing symbolic processing)
        float original_val = enhanced_content[pos];
        float symbolic_transform = symbol_dist(dream_generator_);
        
        // Combine original and symbolic elements
        enhanced_content[pos] = std::tanh(original_val + symbolic_transform);
    }
    
    return enhanced_content;
}

std::vector<float> DreamProcessor::blendCrossModalContent(
    const std::vector<float>& visual_content,
    const std::vector<float>& auditory_content,
    const std::vector<float>& tactile_content,
    float blend_factor)
{
    std::vector<float> blended_content;
    
    std::size_t max_size = std::max({visual_content.size(), auditory_content.size(), tactile_content.size()});
    blended_content.reserve(max_size);
    
    for (std::size_t i = 0; i < max_size; ++i) {
        float blended_val = 0.0f;
        int modality_count = 0;
        
        if (i < visual_content.size()) {
            blended_val += visual_content[i] * blend_factor;
            modality_count++;
        }
        
        if (i < auditory_content.size()) {
            blended_val += auditory_content[i] * blend_factor;
            modality_count++;
        }
        
        if (i < tactile_content.size()) {
            blended_val += tactile_content[i] * blend_factor;
            modality_count++;
        }
        
        if (modality_count > 0) {
            blended_val /= modality_count;
        }
        
        blended_content.push_back(std::tanh(blended_val)); // Normalize
    }
    
    return blended_content;
}

// Dream Analysis and Storage

DreamProcessor::DreamAnalysis DreamProcessor::analyzeDream(const DreamNarrative& dream) {
    DreamAnalysis analysis;
    
    // Calculate novelty score based on content uniqueness
    if (!dream.sensory_content.empty()) {
        float content_variance = 0.0f;
        float content_mean = std::accumulate(dream.sensory_content.begin(), dream.sensory_content.end(), 0.0f) / dream.sensory_content.size();
        
        for (float val : dream.sensory_content) {
            content_variance += (val - content_mean) * (val - content_mean);
        }
        content_variance /= dream.sensory_content.size();
        
        analysis.novelty_score = std::min(1.0f, content_variance * 2.0f);
    }
    
    // Analyze problem-solving potential
    if (dream.dream_type == DreamType::ProblemSolving) {
        analysis.problem_solving_potential = 0.8f + (dream.creativity_score * 0.2f);
    } else {
        analysis.problem_solving_potential = dream.creativity_score * 0.5f;
    }
    
    // Analyze emotional processing value
    analysis.emotional_processing_value = dream.emotional_intensity;
    if (dream.dream_type == DreamType::Emotional || dream.dream_type == DreamType::Nightmare) {
        analysis.emotional_processing_value *= 1.5f;
    }
    
    // Calculate memory consolidation benefit
    analysis.memory_consolidation_benefit = (dream.coherence_score + analysis.novelty_score) * 0.5f;
    
    // Generate insights based on dream characteristics
    if (dream.creativity_score > 0.7f) {
        analysis.insights.push_back("High creative potential - novel connections formed");
    }
    
    if (dream.emotional_intensity > 0.8f) {
        analysis.insights.push_back("Intense emotional processing - potential stress regulation");
    }
    
    if (dream.coherence_score > 0.8f) {
        analysis.insights.push_back("Highly coherent narrative - strong memory integration");
    }
    
    if (dream.dream_type == DreamType::Lucid) {
        analysis.insights.push_back("Lucid awareness detected - metacognitive processing active");
    }
    
    // Determine if further processing is needed
    analysis.requires_further_processing = (analysis.novelty_score > 0.8f) || 
                                          (analysis.problem_solving_potential > 0.7f) ||
                                          (dream.dream_type == DreamType::Nightmare && dream.emotional_intensity > 0.9f);
    
    return analysis;
}

void DreamProcessor::storeDream(const DreamNarrative& dream, const DreamAnalysis& analysis) {
    std::lock_guard<std::mutex> lock(dream_mutex_);
    
    // Store in main history
    dream_history_.push_back(dream);
    
    // Store by type
    dreams_by_type_[dream.dream_type].push_back(dream);
    
    // Limit storage size to prevent memory bloat
    const std::size_t max_dreams = 1000;
    if (dream_history_.size() > max_dreams) {
        dream_history_.erase(dream_history_.begin());
    }
    
    // Limit dreams by type
    const std::size_t max_dreams_per_type = 200;
    for (auto& [type, dreams] : dreams_by_type_) {
        if (dreams.size() > max_dreams_per_type) {
            dreams.erase(dreams.begin());
        }
    }
}

std::vector<DreamProcessor::DreamNarrative> DreamProcessor::getDreamsByType(
    DreamType dream_type, 
    std::size_t max_dreams) const 
{
    std::lock_guard<std::mutex> lock(dream_mutex_);
    
    auto it = dreams_by_type_.find(dream_type);
    if (it == dreams_by_type_.end()) {
        return {};
    }
    
    const auto& dreams = it->second;
    std::size_t start_idx = dreams.size() > max_dreams ? dreams.size() - max_dreams : 0;
    
    return std::vector<DreamNarrative>(dreams.begin() + start_idx, dreams.end());
}

std::vector<DreamProcessor::DreamNarrative> DreamProcessor::getRecentDreams(
    std::uint64_t hours_back, 
    std::size_t max_dreams) const 
{
    std::lock_guard<std::mutex> lock(dream_mutex_);
    
    std::uint64_t cutoff_time = getCurrentTimestamp() - (hours_back * 60 * 60 * 1000);
    std::vector<DreamNarrative> recent_dreams;
    
    for (auto it = dream_history_.rbegin(); it != dream_history_.rend() && recent_dreams.size() < max_dreams; ++it) {
        if (it->timestamp >= cutoff_time) {
            recent_dreams.push_back(*it);
        }
    }
    
    return recent_dreams;
}

// Statistics and Configuration

DreamProcessor::Statistics DreamProcessor::getStatistics() const {
    Statistics stats;
    
    stats.total_dreams_generated = total_dreams_generated_.load();
    stats.total_dream_time_ms = total_dream_time_ms_.load();
    stats.creative_dreams_count = creative_dreams_count_.load();
    stats.problem_solving_dreams_count = problem_solving_dreams_count_.load();
    stats.nightmares_count = nightmares_count_.load();
    stats.lucid_dreams_count = lucid_dreams_count_.load();
    
    if (stats.total_dreams_generated > 0) {
        stats.average_dream_duration_ms = static_cast<float>(stats.total_dream_time_ms) / stats.total_dreams_generated;
    }
    
    // Calculate averages from recent dreams
    std::lock_guard<std::mutex> lock(dream_mutex_);
    if (!dream_history_.empty()) {
        float total_coherence = 0.0f;
        float total_creativity = 0.0f;
        float total_emotion = 0.0f;
        
        std::size_t recent_count = std::min(dream_history_.size(), static_cast<std::size_t>(100));
        for (std::size_t i = dream_history_.size() - recent_count; i < dream_history_.size(); ++i) {
            total_coherence += dream_history_[i].coherence_score;
            total_creativity += dream_history_[i].creativity_score;
            total_emotion += dream_history_[i].emotional_intensity;
        }
        
        stats.average_coherence_score = total_coherence / recent_count;
        stats.average_creativity_score = total_creativity / recent_count;
        stats.average_emotional_intensity = total_emotion / recent_count;
    }
    
    // Find most common dream type
    DreamType most_common = DreamType::Episodic;
    std::size_t max_count = 0;
    for (const auto& [type, dreams] : dreams_by_type_) {
        if (dreams.size() > max_count) {
            max_count = dreams.size();
            most_common = type;
        }
    }
    stats.most_common_dream_type = most_common;
    
    stats.dreaming_active = dreaming_active_.load();
    stats.all_systems_registered = areAllSystemsRegistered();
    
    return stats;
}

void DreamProcessor::setConfig(const DreamConfig& new_config) {
    std::lock_guard<std::mutex> lock(dream_mutex_);
    config_ = new_config;
}

const DreamProcessor::DreamConfig& DreamProcessor::getConfig() const {
    return config_;
}

bool DreamProcessor::areAllSystemsRegistered() const {
    return episodic_memory_ != nullptr && 
           semantic_memory_ != nullptr && 
           working_memory_ != nullptr && 
           sleep_consolidation_ != nullptr &&
           brain_ != nullptr &&
           learning_system_ != nullptr;
}

bool DreamProcessor::isOperational() const {
    return areAllSystemsRegistered() && !symbolic_dictionary_.empty() && !narrative_templates_.empty();
}

bool DreamProcessor::isDreaming() const {
    return dreaming_active_.load();
}

// Private Methods

DreamProcessor::DreamType DreamProcessor::selectDreamType(
    const std::vector<float>& emotional_state,
    float stress_level,
    const std::vector<EnhancedEpisode>& recent_experiences)
{
    std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
    float rand_val = prob_dist(dream_generator_);
    
    // Adjust probabilities based on context
    float creative_prob = config_.creative_dream_probability;
    float nightmare_prob = config_.nightmare_probability + (stress_level * 0.3f);
    float lucid_prob = config_.lucid_dream_probability;
    float problem_solving_prob = config_.problem_solving_probability;
    
    // Check for high emotional content in recent experiences
    bool high_emotion = false;
    for (const auto& episode : recent_experiences) {
        if (episode.emotional_weight > 0.7f) {
            high_emotion = true;
            break;
        }
    }
    
    if (high_emotion) {
        nightmare_prob *= 1.5f;
        creative_prob *= 0.7f;
    }
    
    // Select dream type based on probabilities
    if (rand_val < nightmare_prob) {
        return DreamType::Nightmare;
    } else if (rand_val < nightmare_prob + lucid_prob) {
        return DreamType::Lucid;
    } else if (rand_val < nightmare_prob + lucid_prob + creative_prob) {
        return DreamType::Creative;
    } else if (rand_val < nightmare_prob + lucid_prob + creative_prob + problem_solving_prob) {
        return DreamType::ProblemSolving;
    } else if (high_emotion) {
        return DreamType::Emotional;
    } else {
        // Default to episodic or semantic
        return prob_dist(dream_generator_) < 0.6f ? DreamType::Episodic : DreamType::Semantic;
    }
}

std::uint64_t DreamProcessor::calculateDreamDuration(DreamType dream_type, std::uint64_t available_time) {
    std::uint64_t base_duration = config_.min_dream_duration_ms;
    
    // Adjust duration based on dream type
    switch (dream_type) {
        case DreamType::Creative:
        case DreamType::ProblemSolving:
            base_duration = static_cast<std::uint64_t>(config_.min_dream_duration_ms * 1.5f);
            break;
        case DreamType::Lucid:
            base_duration = static_cast<std::uint64_t>(config_.min_dream_duration_ms * 2.0f);
            break;
        case DreamType::Nightmare:
            base_duration = static_cast<std::uint64_t>(config_.min_dream_duration_ms * 0.8f);
            break;
        default:
            break;
    }
    
    // Ensure we don't exceed available time or maximum duration
    std::uint64_t max_duration = std::min(available_time, config_.max_dream_duration_ms);
    return std::min(base_duration, max_duration);
}

std::vector<EnhancedEpisode> DreamProcessor::selectSourceMemories(DreamType dream_type, std::size_t max_sources) {
    std::vector<EnhancedEpisode> selected_memories;
    
    if (!episodic_memory_) {
        return selected_memories;
    }
    
    // Get recent episodes
    std::vector<EnhancedEpisode> recent_episodes = episodic_memory_->getRecentEpisodes(7 * 24 * 60 * 60 * 1000, max_sources * 2); // Last week
    
    if (recent_episodes.empty()) {
        return selected_memories;
    }
    
    // Select memories based on dream type
    switch (dream_type) {
        case DreamType::Emotional:
        case DreamType::Nightmare: {
            // Select emotionally intense memories
            std::sort(recent_episodes.begin(), recent_episodes.end(),
                     [](const EnhancedEpisode& a, const EnhancedEpisode& b) {
                         return a.emotional_weight > b.emotional_weight;
                     });
            break;
        }
        
        case DreamType::Creative:
        case DreamType::ProblemSolving: {
            // Select diverse memories for creative combination
            std::shuffle(recent_episodes.begin(), recent_episodes.end(), dream_generator_);
            break;
        }
        
        default: {
            // Select recent memories with some randomization
            std::uniform_int_distribution<std::size_t> shuffle_dist(0, std::min(recent_episodes.size(), max_sources * 2) - 1);
            std::shuffle(recent_episodes.begin(), recent_episodes.begin() + shuffle_dist(dream_generator_) + 1, dream_generator_);
            break;
        }
    }
    
    // Take the first max_sources memories
    std::size_t count = std::min(max_sources, recent_episodes.size());
    selected_memories.assign(recent_episodes.begin(), recent_episodes.begin() + count);
    
    return selected_memories;
}

std::vector<float> DreamProcessor::applyMemoryDistortion(
    const std::vector<float>& original_content,
    float distortion_factor)
{
    std::vector<float> distorted_content = original_content;
    
    std::uniform_real_distribution<float> distortion_dist(-distortion_factor, distortion_factor);
    
    for (float& val : distorted_content) {
        float distortion = distortion_dist(dream_generator_);
        val = std::tanh(val + distortion); // Apply distortion and normalize
    }
    
    return distorted_content;
}

void DreamProcessor::initializeSymbolicDictionary() {
    symbolic_dictionary_ = {
        "transformation", "journey", "flight", "falling", "water", "fire",
        "mirror", "door", "key", "bridge", "mountain", "ocean", "forest",
        "light", "shadow", "mask", "spiral", "circle", "tower", "cave",
        "garden", "storm", "rainbow", "star", "moon", "sun", "wind",
        "river", "desert", "ice", "crystal", "flower", "tree", "bird",
        "snake", "butterfly", "wolf", "lion", "eagle", "whale", "dragon"
    };
}

void DreamProcessor::initializeNarrativeTemplates() {
    narrative_templates_ = {
        "The dream began in a familiar place, but everything was subtly wrong.",
        "I found myself in a world where the laws of physics didn't apply.",
        "The landscape shifted and morphed with each step I took.",
        "Faces from my past appeared and disappeared like ghosts.",
        "Time moved in strange ways, moments stretching into eternities.",
        "I could fly, but only when I wasn't thinking about it.",
        "The colors were more vivid than anything in waking life.",
        "I was searching for something, though I couldn't remember what.",
        "The dream felt more real than reality itself.",
        "I was both observer and participant in the unfolding story."
    };
}

float DreamProcessor::calculateCoherenceScore(
    const std::vector<float>& dream_content,
    const std::string& narrative_text)
{
    if (dream_content.empty()) {
        return 0.0f;
    }
    
    // Calculate content consistency (low variance indicates coherence)
    float mean = std::accumulate(dream_content.begin(), dream_content.end(), 0.0f) / dream_content.size();
    float variance = 0.0f;
    
    for (float val : dream_content) {
        variance += (val - mean) * (val - mean);
    }
    variance /= dream_content.size();
    
    // Lower variance = higher coherence
    float content_coherence = std::exp(-variance * 2.0f);
    
    // Narrative length as coherence indicator
    float narrative_coherence = std::min(1.0f, narrative_text.length() / 200.0f);
    
    return (content_coherence + narrative_coherence) * 0.5f;
}

float DreamProcessor::calculateCreativityScore(
    const std::vector<float>& dream_content,
    const std::vector<EnhancedEpisode>& source_memories)
{
    if (dream_content.empty()) {
        return 0.0f;
    }
    
    // Calculate content diversity (high variance indicates creativity)
    float mean = std::accumulate(dream_content.begin(), dream_content.end(), 0.0f) / dream_content.size();
    float variance = 0.0f;
    
    for (float val : dream_content) {
        variance += (val - mean) * (val - mean);
    }
    variance /= dream_content.size();
    
    // Higher variance = higher creativity
    float content_creativity = std::min(1.0f, variance * 3.0f);
    
    // Number of source memories indicates creative combination
    float source_creativity = std::min(1.0f, source_memories.size() / 10.0f);
    
    return (content_creativity + source_creativity) * 0.5f;
}

std::uint64_t DreamProcessor::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

} // namespace Memory
} // namespace NeuroForge