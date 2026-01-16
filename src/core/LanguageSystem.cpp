#include "core/LanguageSystem.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>

namespace NeuroForge {
namespace Core {

LanguageSystem::LanguageSystem(const Config& config)
    : config_(config)
    , current_stage_(DevelopmentalStage::Chaos)
    , development_step_counter_(0)
    , narration_active_(false)
    , rng_(std::random_device{}())
    , uniform_dist_(0.0f, 1.0f) {
    
    vocabulary_.reserve(config_.max_vocabulary_size);
    internal_narration_.resize(1000); // Ring buffer for narration history
}

bool LanguageSystem::initialize() {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    // Initialize basic vocabulary with fundamental tokens
    createToken("<START>", TokenType::Meta);
    createToken("<END>", TokenType::Meta);
    createToken("<UNK>", TokenType::Meta);
    createToken("<SELF>", TokenType::Meta);
    
    // Initialize basic action tokens
    createToken("move", TokenType::Action);
    createToken("stop", TokenType::Action);
    createToken("look", TokenType::Action);
    createToken("listen", TokenType::Action);
    
    // Initialize basic perception tokens
    createToken("see", TokenType::Perception);
    createToken("hear", TokenType::Perception);
    createToken("bright", TokenType::Perception);
    createToken("dark", TokenType::Perception);
    
    // Initialize basic relation tokens
    createToken("near", TokenType::Relation);
    createToken("far", TokenType::Relation);
    createToken("before", TokenType::Relation);
    createToken("after", TokenType::Relation);
    
    current_stage_.store(DevelopmentalStage::Chaos);
    development_step_counter_.store(0);
    
    updateStatistics();
    logDevelopmentalEvent("System initialized", "Starting in Chaos stage");
    
    return true;
}

void LanguageSystem::shutdown() {
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    std::lock_guard<std::recursive_mutex> narr_lock(narration_mutex_);
    
    vocabulary_.clear();
    token_lookup_.clear();
    internal_narration_.clear();
    
    logDevelopmentalEvent("System shutdown", "All language data cleared");
}

void LanguageSystem::reset() {
    shutdown();
    initialize();
}

void LanguageSystem::setRandomSeed(std::uint32_t seed) {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    rng_.seed(seed);
    uniform_dist_ = std::uniform_real_distribution<float>(0.0f, 1.0f);
}

void LanguageSystem::updateDevelopment(float delta_time) {
    development_step_counter_.fetch_add(1);
    
    switch (current_stage_.load()) {
        case DevelopmentalStage::Chaos:
            processChaosStage(delta_time);
            break;
        case DevelopmentalStage::Babbling:
            processBabblingStage(delta_time);
            break;
        case DevelopmentalStage::Mimicry:
            processMimicryStage(delta_time);
            break;
        case DevelopmentalStage::Grounding:
            processGroundingStage(delta_time);
            break;
        case DevelopmentalStage::Reflection:
            processReflectionStage(delta_time);
            break;
        case DevelopmentalStage::Communication:
            processCommunicationStage(delta_time);
            break;
    }
    
    // Check for stage advancement
    if (shouldAdvanceStage()) {
        DevelopmentalStage next_stage = static_cast<DevelopmentalStage>(
            static_cast<int>(current_stage_.load()) + 1);
        if (next_stage <= DevelopmentalStage::Communication) {
            advanceToStage(next_stage);
        }
    }
    
    // Decay unused tokens
    decayUnusedTokens(config_.token_decay_rate * delta_time);
    
    // Update statistics periodically
    if (development_step_counter_.load() % 100 == 0) {
        updateStatistics();
    }
}

void LanguageSystem::advanceToStage(DevelopmentalStage stage) {
    DevelopmentalStage old_stage = current_stage_.load();
    current_stage_.store(stage);
    development_step_counter_.store(0);
    
    onStageTransition(old_stage, stage);
    
    std::string event = "Stage transition: " + stageToString(old_stage) + 
                       " -> " + stageToString(stage);
    logDevelopmentalEvent(event);
    
    // Ensure statistics reflect the new stage immediately
    updateStatistics();
}

std::size_t LanguageSystem::createToken(const std::string& symbol, TokenType type, 
                                       const std::vector<float>& embedding) {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    // Check if token already exists
    auto it = token_lookup_.find(symbol);
    if (it != token_lookup_.end()) {
        return it->second;
    }
    
    // Create new token
    SymbolicToken token;
    token.symbol = symbol;
    token.type = type;
    token.activation_strength = 0.0f;
    token.usage_count = 0;
    token.last_used = std::chrono::steady_clock::now();
    
    // Initialize embedding
    if (embedding.empty()) {
        token.embedding = generateRandomEmbedding();
    } else {
        // Ensure embedding matches configured dimension (pad/truncate then normalize)
        std::vector<float> adjusted = embedding;
        const std::size_t target_dim = static_cast<std::size_t>(config_.embedding_dimension);
        if (adjusted.size() != target_dim) {
            if (adjusted.size() > target_dim) {
                adjusted.resize(target_dim); // truncate
            } else {
                adjusted.resize(target_dim, 0.0f); // pad with zeros
            }
            std::cerr << "[LanguageSystem] Warning: adjusted embedding dimension from "
                      << embedding.size() << " to " << target_dim
                      << " for token '" << symbol << "'\n";
        }
        token.embedding = normalizeEmbedding(adjusted);
    }
    
    std::size_t token_id = vocabulary_.size();
    vocabulary_.push_back(std::move(token));
    token_lookup_[symbol] = token_id;
    
    return token_id;
}

LanguageSystem::SymbolicToken* LanguageSystem::getToken(const std::string& symbol) {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    auto it = token_lookup_.find(symbol);
    if (it != token_lookup_.end()) {
        return &vocabulary_[it->second];
    }
    return nullptr;
}

LanguageSystem::SymbolicToken* LanguageSystem::getToken(std::size_t token_id) {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    if (token_id < vocabulary_.size()) {
        return &vocabulary_[token_id];
    }
    return nullptr;
}

std::vector<std::size_t> LanguageSystem::findSimilarTokens(const std::vector<float>& embedding, 
                                                          float threshold) const {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    std::vector<std::size_t> similar_tokens;
    
    for (std::size_t i = 0; i < vocabulary_.size(); ++i) {
        float similarity = cosineSimilarity(embedding, vocabulary_[i].embedding);
        if (similarity >= threshold) {
            similar_tokens.push_back(i);
        }
    }
    
    // Sort by similarity (descending)
    std::sort(similar_tokens.begin(), similar_tokens.end(),
              [this, &embedding](std::size_t a, std::size_t b) {
                  float sim_a = cosineSimilarity(embedding, vocabulary_[a].embedding);
                  float sim_b = cosineSimilarity(embedding, vocabulary_[b].embedding);
                  return sim_a > sim_b;
              });
    
    return similar_tokens;
}

void LanguageSystem::setTeacherEmbedding(const std::string& label, 
                                        const std::vector<float>& embedding) {
    std::lock_guard<std::recursive_mutex> lock(teacher_mutex_);
    
    teacher_labels_.push_back(label);
    teacher_embeddings_.push_back(normalizeEmbedding(embedding));
}

void LanguageSystem::processTeacherSignal(const std::string& label, float reward_signal) {
    std::vector<float> teacher_embedding;
    
    // Get teacher embedding
    {
        std::lock_guard<std::recursive_mutex> teacher_lock(teacher_mutex_);
        auto it = std::find(teacher_labels_.begin(), teacher_labels_.end(), label);
        if (it == teacher_labels_.end()) {
            return;
        }
        std::size_t teacher_idx = std::distance(teacher_labels_.begin(), it);
        teacher_embedding = teacher_embeddings_[teacher_idx];
    }
    
    // Handle token creation/update with single vocabulary lock
    {
        std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
        
        // Find existing token
        SymbolicToken* token = nullptr;
        auto it = token_lookup_.find(label);
        if (it != token_lookup_.end()) {
            token = &vocabulary_[it->second];
        } else {
            // Create new token
            SymbolicToken new_token;
            new_token.symbol = label;
            new_token.type = inferTokenType(label);
            new_token.activation_strength = 0.0f;
            new_token.usage_count = 0;
            new_token.last_used = std::chrono::steady_clock::now();
            new_token.embedding = normalizeEmbedding(teacher_embedding);
            
            std::size_t token_id = vocabulary_.size();
            vocabulary_.push_back(std::move(new_token));
            token_lookup_[label] = token_id;
            token = &vocabulary_[token_id];
        }
        
        if (token) {
            // Update token based on teacher signal
            float learning_rate = config_.mimicry_learning_rate * reward_signal;
            
            for (std::size_t i = 0; i < token->embedding.size() && i < teacher_embedding.size(); ++i) {
                token->embedding[i] += learning_rate * (teacher_embedding[i] - token->embedding[i]);
            }
            
            token->embedding = normalizeEmbedding(token->embedding);
            token->activation_strength += reward_signal * 0.1f;
            token->usage_count++;
            token->last_used = std::chrono::steady_clock::now();
            
            stats_.successful_mimicry_attempts++;
        }
    }
}

std::vector<float> LanguageSystem::generateMimicryResponse(const std::vector<float>& teacher_embedding) {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    // Find most similar token (internal implementation to avoid deadlock)
    std::vector<std::size_t> similar_tokens;
    float threshold = 0.3f;
    
    for (std::size_t i = 0; i < vocabulary_.size(); ++i) {
        float similarity = cosineSimilarity(teacher_embedding, vocabulary_[i].embedding);
        if (similarity >= threshold) {
            similar_tokens.push_back(i);
        }
    }
    
    // Sort by similarity (descending)
    std::sort(similar_tokens.begin(), similar_tokens.end(),
              [this, &teacher_embedding](std::size_t a, std::size_t b) {
                  float sim_a = cosineSimilarity(teacher_embedding, vocabulary_[a].embedding);
                  float sim_b = cosineSimilarity(teacher_embedding, vocabulary_[b].embedding);
                  return sim_a > sim_b;
              });
    
    if (similar_tokens.empty()) {
        // Generate random response if no similar tokens found
        return generateRandomEmbedding();
    }
    
    // Select token based on activation strength and similarity
    std::size_t selected_token = similar_tokens[0];
    float best_score = vocabulary_[selected_token].activation_strength * 
                      cosineSimilarity(teacher_embedding, vocabulary_[selected_token].embedding);
    
    for (std::size_t i = 1; i < std::min(similar_tokens.size(), std::size_t(5)); ++i) {
        std::size_t token_id = similar_tokens[i];
        float score = vocabulary_[token_id].activation_strength * 
                     cosineSimilarity(teacher_embedding, vocabulary_[token_id].embedding);
        
        if (score > best_score) {
            best_score = score;
            selected_token = token_id;
        }
    }
    
    // Add some noise for exploration
    std::vector<float> response = vocabulary_[selected_token].embedding;
    float noise_level = 0.1f;
    
    for (float& val : response) {
        val += noise_level * (uniform_dist_(rng_) - 0.5f);
    }
    
    return normalizeEmbedding(response);
}

void LanguageSystem::associateTokenWithNeuron(std::size_t token_id, NeuroForge::NeuronID neuron_id, 
                                             float association_strength) {
    std::lock_guard<std::recursive_mutex> grounding_lock(grounding_mutex_);
        std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    if (token_id >= vocabulary_.size()) {
        return;
    }
    
    // Add neuron to token's associations
    auto& token = vocabulary_[token_id];
    auto it = std::find(token.associated_neurons.begin(), token.associated_neurons.end(), neuron_id);
    if (it == token.associated_neurons.end()) {
        token.associated_neurons.push_back(neuron_id);
    }
    
    // Add token to neuron's associations
    neuron_to_tokens_[neuron_id].push_back(token_id);
    
    stats_.grounding_associations_formed++;
}

void LanguageSystem::associateTokenWithModality(std::size_t token_id, const std::string& modality, 
                                               const std::vector<float>& pattern, float strength) {
    std::lock_guard<std::recursive_mutex> grounding_lock(grounding_mutex_);
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    if (token_id >= vocabulary_.size()) {
        return;
    }
    
    auto& token = vocabulary_[token_id];
    
    // Store modality association
    std::ostringstream pattern_key;
    pattern_key << modality << "_pattern";
    token.sensory_associations[pattern_key.str()] = strength;
    
    // Add to modality lookup
    modality_to_tokens_[modality].push_back(token_id);
    
    stats_.grounding_associations_formed++;
}

std::vector<std::size_t> LanguageSystem::getTokensForNeuralPattern(
    const std::vector<NeuroForge::NeuronID>& neurons) const {
    
    std::lock_guard<std::recursive_mutex> lock(grounding_mutex_);
    
    std::unordered_map<std::size_t, int> token_votes;
    
    // Count votes for each token based on associated neurons
    for (NeuroForge::NeuronID neuron_id : neurons) {
        auto it = neuron_to_tokens_.find(neuron_id);
        if (it != neuron_to_tokens_.end()) {
            for (std::size_t token_id : it->second) {
                token_votes[token_id]++;
            }
        }
    }
    
    // Sort tokens by vote count
    std::vector<std::pair<std::size_t, int>> sorted_tokens(token_votes.begin(), token_votes.end());
    std::sort(sorted_tokens.begin(), sorted_tokens.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    std::vector<std::size_t> result;
    for (const auto& pair : sorted_tokens) {
        result.push_back(pair.first);
    }
    
    return result;
}

void LanguageSystem::generateNarration(const std::vector<float>& context_embedding, 
                                      const std::string& context_description) {
    if (!narration_active_.load()) {
        return;
    }
    
    std::lock_guard<std::recursive_mutex> narr_lock(narration_mutex_);
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    NarrationEntry entry;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.context = context_description;
    entry.is_self_generated = true;
    
    // Find relevant tokens based on context embedding
    std::vector<std::size_t> relevant_tokens = findSimilarTokens(context_embedding, config_.narration_threshold);
    
    // Generate token sequence (simple approach: select top activated tokens)
    std::size_t max_sequence_length = std::min(std::size_t(10), relevant_tokens.size());
    
    for (std::size_t i = 0; i < max_sequence_length; ++i) {
        std::size_t token_id = relevant_tokens[i];
        if (vocabulary_[token_id].activation_strength > config_.narration_threshold) {
            entry.token_sequence.push_back(vocabulary_[token_id]);
        }
    }
    
    // Calculate confidence based on token activations
    if (!entry.token_sequence.empty()) {
        float total_activation = 0.0f;
        for (const auto& token : entry.token_sequence) {
            total_activation += token.activation_strength;
        }
        entry.confidence = total_activation / entry.token_sequence.size();
        
        // Add to narration history (ring buffer)
        internal_narration_.push_back(entry);
        if (internal_narration_.size() > 1000) {
            internal_narration_.pop_front();
        }
        
        stats_.narration_entries++;
    }
}

std::vector<LanguageSystem::NarrationEntry> LanguageSystem::getRecentNarration(std::size_t count) const {
    std::lock_guard<std::recursive_mutex> lock(narration_mutex_);
    
    std::vector<NarrationEntry> recent;
    std::size_t start_idx = internal_narration_.size() > count ? 
                           internal_narration_.size() - count : 0;
    
    for (std::size_t i = start_idx; i < internal_narration_.size(); ++i) {
        recent.push_back(internal_narration_[i]);
    }
    
    return recent;
}

void LanguageSystem::logSelfNarration(const std::vector<std::string>& token_sequence, 
                                     float confidence, const std::string& context) {
    NarrationEntry entry;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.confidence = confidence;
    entry.context = context;
    entry.is_self_generated = true;
    
    // Convert string tokens to SymbolicTokens with single vocabulary lock
    {
        std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
        
        for (const std::string& token_str : token_sequence) {
            // Find existing token
            auto it = token_lookup_.find(token_str);
            if (it != token_lookup_.end()) {
                entry.token_sequence.push_back(vocabulary_[it->second]);
            } else {
                // Create new token if it doesn't exist
                SymbolicToken new_token;
                new_token.symbol = token_str;
                new_token.type = inferTokenType(token_str);
                new_token.activation_strength = 0.0f;
                new_token.usage_count = 0;
                new_token.last_used = std::chrono::steady_clock::now();
                new_token.embedding = generateRandomEmbedding();
                
                std::size_t token_id = vocabulary_.size();
                vocabulary_.push_back(new_token);
                token_lookup_[token_str] = token_id;
                entry.token_sequence.push_back(new_token);
            }
        }
    }
    
    // Add to narration history
    {
        std::lock_guard<std::recursive_mutex> narr_lock(narration_mutex_);
        internal_narration_.push_back(entry);
        if (internal_narration_.size() > 1000) {
            internal_narration_.pop_front();
        }
        stats_.narration_entries++;
    }
}

std::vector<float> LanguageSystem::generateRandomEmbedding() const {
    std::vector<float> embedding(config_.embedding_dimension);
    
    // Generate random values from normal distribution
    std::normal_distribution<float> normal_dist(0.0f, 1.0f);
    
    for (float& val : embedding) {
        val = normal_dist(const_cast<std::mt19937&>(rng_));
    }
    
    return normalizeEmbedding(embedding);
}

void LanguageSystem::performBabbling(std::size_t num_tokens) {
    // Use acoustic-first babbling if enabled
    if (config_.enable_acoustic_preprocessing) {
        performAcousticBabbling(num_tokens);
        return;
    }
    
    // Fallback to original token-based babbling
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    for (std::size_t i = 0; i < num_tokens; ++i) {
        // Generate random phoneme-like tokens
        std::string babble_token = "bab_" + std::to_string(i) + "_" + 
                                  std::to_string(development_step_counter_.load());
        
        // Check if token already exists
        auto it = token_lookup_.find(babble_token);
        if (it == token_lookup_.end()) {
            // Create new token
            SymbolicToken token;
            token.symbol = babble_token;
            token.type = TokenType::Phoneme;
            token.activation_strength = uniform_dist_(rng_);
            token.usage_count = 0;
            token.last_used = std::chrono::steady_clock::now();
            token.embedding = generateRandomEmbedding();
            
            std::size_t token_id = vocabulary_.size();
            vocabulary_.push_back(std::move(token));
            token_lookup_[babble_token] = token_id;
        } else {
            // Activate existing token
            vocabulary_[it->second].activation_strength = uniform_dist_(rng_);
        }
    }
    
    stats_.total_tokens_generated += num_tokens;
}

void LanguageSystem::exploreTokenCombinations(std::size_t sequence_length) {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    if (vocabulary_.size() < sequence_length) {
        return;
    }
    
    // Generate random token combinations
    std::vector<std::string> combination;
    
    for (std::size_t i = 0; i < sequence_length; ++i) {
        std::size_t random_idx = static_cast<std::size_t>(uniform_dist_(rng_) * vocabulary_.size());
        combination.push_back(vocabulary_[random_idx].symbol);
    }
    
    // Log as self-narration with low confidence
    logSelfNarration(combination, 0.1f, "Token exploration");
}

void LanguageSystem::processNeuralActivation(
    const std::vector<std::pair<NeuroForge::NeuronID, float>>& activations) {
    
    std::vector<NeuroForge::NeuronID> active_neurons;
    for (const auto& pair : activations) {
        if (pair.second > 0.5f) { // Threshold for "active"
            active_neurons.push_back(pair.first);
        }
    }
    
    // Find associated tokens
    std::vector<std::size_t> associated_tokens = getTokensForNeuralPattern(active_neurons);
    
    // Update token activations
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    for (std::size_t token_id : associated_tokens) {
        if (token_id < vocabulary_.size()) {
            vocabulary_[token_id].activation_strength += 0.1f;
            vocabulary_[token_id].activation_strength = std::min(vocabulary_[token_id].activation_strength, 1.0f);
        }
    }
    
    // Generate narration if enough tokens are active
    if (associated_tokens.size() >= 2 && narration_active_.load()) {
        std::vector<float> context_embedding = generateRandomEmbedding(); // Simplified
        generateNarration(context_embedding, "Neural activation pattern");
    }
}

void LanguageSystem::influenceNeuralActivation(const std::vector<std::size_t>& token_ids, 
                                              float influence_strength) {
    std::lock_guard<std::recursive_mutex> grounding_lock(grounding_mutex_);
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    // If no neural bias callback has been provided, there is nothing to influence
    if (!neuron_bias_callback_) {
        return;
    }

    const float clamped_strength = std::max(0.0f, influence_strength);
    for (std::size_t token_id : token_ids) {
        if (token_id < vocabulary_.size()) {
            const auto& token = vocabulary_[token_id];
            
            // Influence associated neurons by distributing the requested strength
            // across the token's linked neural pattern.
            if (token.associated_neurons.empty()) {
                continue;
            }

            const float per_neuron = clamped_strength /
                static_cast<float>(std::max<std::size_t>(1, token.associated_neurons.size()));
            for (NeuroForge::NeuronID neuron_id : token.associated_neurons) {
                neuron_bias_callback_(neuron_id, per_neuron);
            }
        }
    }
}

LanguageSystem::Statistics LanguageSystem::getStatistics() const {
    // Refresh statistics on-demand so callers always observe up-to-date values
    const_cast<LanguageSystem*>(this)->updateStatistics();
    std::lock_guard<std::recursive_mutex> lock(stats_mutex_);
    return stats_;
}

void LanguageSystem::updateStatistics() {
    std::lock_guard<std::recursive_mutex> stats_lock(stats_mutex_);
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    stats_.active_vocabulary_size = 0;
    stats_.total_vocabulary_size = vocabulary_.size();
    stats_.average_token_activation = 0.0f;
    stats_.average_cluster_stability = 0.0f;
    stats_.token_activation_entropy = 0.0f;
    stats_.tokens_stable_over_0_5 = 0;
    stats_.current_stage = current_stage_.load();
    
    float total_activation = 0.0f;
    
    for (const auto& token : vocabulary_) {
        if (token.activation_strength > 0.1f) {
            stats_.active_vocabulary_size++;
        }
        total_activation += token.activation_strength;
    }
    
    if (!vocabulary_.empty()) {
        const float denom = static_cast<float>(vocabulary_.size());
        stats_.average_token_activation = total_activation / denom;

        const float mean_activation = stats_.average_token_activation;
        float sum_stability = 0.0f;
        std::size_t stable_over = 0;
        for (const auto& token : vocabulary_) {
            const float usage = static_cast<float>(token.usage_count);
            const float usage_factor = usage / (usage + 5.0f);
            const float deviation = std::fabs(token.activation_strength - mean_activation);
            const float variability_factor = 1.0f - std::min(1.0f, deviation);
            float stability = 0.2f + 0.5f * usage_factor + 0.3f * variability_factor;
            stability = std::clamp(stability, 0.0f, 1.0f);
            sum_stability += stability;
            if (stability > 0.5f) {
                stable_over++;
            }
        }
        stats_.average_cluster_stability = sum_stability / denom;
        stats_.tokens_stable_over_0_5 = stable_over;

        if (total_activation > 1e-12f) {
            float entropy = 0.0f;
            for (const auto& token : vocabulary_) {
                const float p = token.activation_strength / total_activation;
                if (p > 0.0f) {
                    entropy -= p * std::log2(p);
                }
            }
            stats_.token_activation_entropy = entropy;
        }
        
        // Calculate vocabulary diversity (simplified entropy measure)
        std::unordered_map<TokenType, int> type_counts;
        for (const auto& token : vocabulary_) {
            type_counts[token.type]++;
        }
        
        float entropy = 0.0f;
        for (const auto& pair : type_counts) {
            float p = static_cast<float>(pair.second) / vocabulary_.size();
            if (p > 0) {
                entropy -= p * std::log2(p);
            }
        }
        stats_.vocabulary_diversity = entropy;
    }
}

std::string LanguageSystem::generateLanguageReport() const {
    std::ostringstream report;
    
    Statistics stats = getStatistics();
    
    report << "=== NeuroForge Language System Report ===\n";
    report << "Current Stage: " << stageToString(stats.current_stage) << "\n";
    report << "Development Step: " << development_step_counter_.load() << "\n";
    report << "Total Vocabulary Size: " << vocabulary_.size() << "\n";
    report << "Active Vocabulary Size: " << stats.active_vocabulary_size << "\n";
    report << "Average Token Activation: " << std::fixed << std::setprecision(3) 
           << stats.average_token_activation << "\n";
    report << "Vocabulary Diversity: " << std::fixed << std::setprecision(3) 
           << stats.vocabulary_diversity << "\n";
    report << "Total Tokens Generated: " << stats.total_tokens_generated << "\n";
    report << "Successful Mimicry Attempts: " << stats.successful_mimicry_attempts << "\n";
    report << "Grounding Associations: " << stats.grounding_associations_formed << "\n";
    report << "Narration Entries: " << stats.narration_entries << "\n";
    
    // Add recent narration samples
    auto recent_narration = getRecentNarration(3);
    if (!recent_narration.empty()) {
        report << "\nRecent Narration Samples:\n";
        for (const auto& entry : recent_narration) {
            report << "  [" << entry.confidence << "] ";
            for (const auto& token : entry.token_sequence) {
                report << token.symbol << " ";
            }
            report << "\n";
        }
    }
    
    return report.str();
}

std::vector<std::string> LanguageSystem::getActiveVocabulary(float activation_threshold) const {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    std::vector<std::string> active_vocab;
    
    for (const auto& token : vocabulary_) {
        if (token.activation_strength >= activation_threshold) {
            active_vocab.push_back(token.symbol);
        }
    }
    
    return active_vocab;
}

// Private implementation methods

void LanguageSystem::processChaosStage(float delta_time) {
    // Random activation and token generation
    if (development_step_counter_.load() % 10 == 0) {
        performBabbling(2);
    }
    
    // Random token activation
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    for (auto& token : vocabulary_) {
        token.activation_strength += (uniform_dist_(rng_) - 0.5f) * 0.1f;
        token.activation_strength = std::max(0.0f, std::min(1.0f, token.activation_strength));
    }
}

void LanguageSystem::processBabblingStage(float delta_time) {
    // Enhanced babbling with proto-word crystallization
    if (development_step_counter_.load() % 3 == 0) {
        performEnhancedBabbling(3);
    }
    
    // Process proto-word crystallization every few steps
    if (development_step_counter_.load() % 5 == 0) {
        processProtoWordCrystallization();
    }
    
    // Update multimodal attention state
    if (development_step_counter_.load() % 2 == 0) {
        // Create dummy features for attention update (in real system, these would come from sensors)
        VisualLanguageFeatures dummy_visual;
        AcousticFeatures dummy_acoustic;
        // updateMultimodalAttention(dummy_visual, dummy_acoustic); // Function not implemented yet
    }
    
    // Analyze emerging patterns for crystallization
    if (development_step_counter_.load() % 10 == 0) {
        analyzeEmergingPatterns();
    }
    
    // Start forming simple associations with proto-word bias
    exploreTokenCombinations(2);
}

void LanguageSystem::processMimicryStage(float delta_time) {
    // Focus on teacher imitation
    if (config_.enable_teacher_mode && !teacher_embeddings_.empty()) {
        std::size_t random_teacher = static_cast<std::size_t>(uniform_dist_(rng_) * teacher_embeddings_.size());
        processTeacherSignal(teacher_labels_[random_teacher], 1.0f);
    }
    
    // Reduce random babbling
    if (development_step_counter_.load() % 20 == 0) {
        performBabbling(1);
    }
}

void LanguageSystem::processGroundingStage(float delta_time) {
    // Focus on associating tokens with experiences
    // This stage would heavily integrate with sensory input and neural activations
    
    // Generate more complex token sequences
    exploreTokenCombinations(3);
    
    // Enable narration
    if (!narration_active_.load()) {
        enableNarration(true);
    }
}

void LanguageSystem::processReflectionStage(float delta_time) {
    // Internal narration becomes more prominent
    if (development_step_counter_.load() % 3 == 0) {
        std::vector<float> context = generateRandomEmbedding();
        generateNarration(context, "Self-reflection");
    }
    
    // Meta-cognitive token usage
    {
        std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
        auto it = token_lookup_.find("<SELF>");
        if (it != token_lookup_.end()) {
            vocabulary_[it->second].activation_strength += 0.05f;
        }
    }
}

void LanguageSystem::processCommunicationStage(float delta_time) {
    // Goal-directed language use
    // This would integrate with the brain's goal system and motor outputs
    
    // Generate purposeful narration
    if (development_step_counter_.load() % 2 == 0) {
        std::vector<float> context = generateRandomEmbedding();
        generateNarration(context, "Goal-directed communication");
    }
}

float LanguageSystem::calculateTokenSimilarity(const SymbolicToken& token1, 
                                              const SymbolicToken& token2) const {
    return cosineSimilarity(token1.embedding, token2.embedding);
}

void LanguageSystem::updateTokenActivation(std::size_t token_id, float activation_delta) {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    if (token_id < vocabulary_.size()) {
        vocabulary_[token_id].activation_strength += activation_delta;
        vocabulary_[token_id].activation_strength = std::max(0.0f, 
            std::min(1.0f, vocabulary_[token_id].activation_strength));
        vocabulary_[token_id].last_used = std::chrono::steady_clock::now();
    }
}

void LanguageSystem::decayUnusedTokens(float decay_rate) {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto& token : vocabulary_) {
        auto time_since_use = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - token.last_used).count();
        
        if (time_since_use > 10000) { // 10 seconds
            token.activation_strength *= (1.0f - decay_rate);
        }
    }
}

void LanguageSystem::pruneVocabulary() {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    if (vocabulary_.size() <= config_.max_vocabulary_size) {
        return;
    }
    
    // Remove least used tokens
    std::vector<std::pair<std::size_t, std::uint64_t>> token_usage;
    for (std::size_t i = 0; i < vocabulary_.size(); ++i) {
        token_usage.emplace_back(i, vocabulary_[i].usage_count);
    }
    
    std::sort(token_usage.begin(), token_usage.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    // Remove bottom 10%
    std::size_t tokens_to_remove = vocabulary_.size() / 10;
    for (std::size_t i = 0; i < tokens_to_remove; ++i) {
        std::size_t token_idx = token_usage[i].first;
        token_lookup_.erase(vocabulary_[token_idx].symbol);
    }
    
    // Rebuild vocabulary without removed tokens
    std::vector<SymbolicToken> new_vocabulary;
    std::unordered_map<std::string, std::size_t> new_lookup;
    
    for (std::size_t i = tokens_to_remove; i < token_usage.size(); ++i) {
        std::size_t old_idx = token_usage[i].first;
        new_lookup[vocabulary_[old_idx].symbol] = new_vocabulary.size();
        new_vocabulary.push_back(std::move(vocabulary_[old_idx]));
    }
    
    vocabulary_ = std::move(new_vocabulary);
    token_lookup_ = std::move(new_lookup);
}

std::vector<float> LanguageSystem::normalizeEmbedding(const std::vector<float>& embedding) const {
    std::vector<float> normalized = embedding;
    
    float norm = 0.0f;
    for (float val : normalized) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    
    if (norm > 1e-6f) {
        for (float& val : normalized) {
            val /= norm;
        }
    }
    
    return normalized;
}

float LanguageSystem::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) const {
    if (a.size() != b.size()) {
        return 0.0f;
    }
    
    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;
    
    for (std::size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    
    norm_a = std::sqrt(norm_a);
    norm_b = std::sqrt(norm_b);
    
    if (norm_a < 1e-6f || norm_b < 1e-6f) {
        return 0.0f;
    }
    
    return dot_product / (norm_a * norm_b);
}

std::vector<float> LanguageSystem::interpolateEmbeddings(const std::vector<float>& a, 
                                                        const std::vector<float>& b, 
                                                        float alpha) const {
    if (a.size() != b.size()) {
        return a;
    }
    
    std::vector<float> result(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        result[i] = (1.0f - alpha) * a[i] + alpha * b[i];
    }
    
    return normalizeEmbedding(result);
}

bool LanguageSystem::shouldAdvanceStage() const {
    std::uint64_t current_step = development_step_counter_.load();
    
    switch (current_stage_.load()) {
        case DevelopmentalStage::Chaos:
            return current_step > 500; // Advance after 500 steps
        case DevelopmentalStage::Babbling:
            return current_step > config_.babbling_duration;
        case DevelopmentalStage::Mimicry:
            return current_step > config_.mimicry_duration;
        case DevelopmentalStage::Grounding:
            return current_step > config_.grounding_duration;
        case DevelopmentalStage::Reflection:
            return current_step > 5000; // Advance after 5000 steps
        case DevelopmentalStage::Communication:
            return false; // Final stage
    }
    
    return false;
}

void LanguageSystem::onStageTransition(DevelopmentalStage from_stage, DevelopmentalStage to_stage) {
    // Perform stage-specific initialization
    switch (to_stage) {
        case DevelopmentalStage::Babbling:
            // Initialize phoneme tokens
            createToken("ba", TokenType::Phoneme);
            createToken("ma", TokenType::Phoneme);
            createToken("da", TokenType::Phoneme);
            break;
            
        case DevelopmentalStage::Mimicry:
            // Enable teacher mode if not already enabled
            if (!config_.enable_teacher_mode) {
                Config new_config = config_;
                new_config.enable_teacher_mode = true;
                updateConfig(new_config);
            }
            break;
            
        case DevelopmentalStage::Grounding: {
            // Enable multimodal associations
            Config grounding_config = config_;
            grounding_config.enable_vision_grounding = true;
            grounding_config.enable_audio_grounding = true;
            grounding_config.enable_action_grounding = true;
            updateConfig(grounding_config);
            break;
        }
            
        case DevelopmentalStage::Reflection:
            enableNarration(true);
            break;
            
        case DevelopmentalStage::Communication:
            // Final stage - all systems active
            break;
            
        default:
            break;
    }
}

std::string LanguageSystem::stageToString(DevelopmentalStage stage) const {
    switch (stage) {
        case DevelopmentalStage::Chaos: return "Chaos";
        case DevelopmentalStage::Babbling: return "Babbling";
        case DevelopmentalStage::Mimicry: return "Mimicry";
        case DevelopmentalStage::Grounding: return "Grounding";
        case DevelopmentalStage::Reflection: return "Reflection";
        case DevelopmentalStage::Communication: return "Communication";
    }
    return "Unknown";
}

LanguageSystem::TokenType LanguageSystem::inferTokenType(const std::string& symbol) const {
    // Simple heuristics for token type inference
    if (symbol.length() <= 2) {
        return TokenType::Phoneme;
    }
    
    if (symbol.find("move") != std::string::npos || 
        symbol.find("go") != std::string::npos ||
        symbol.find("stop") != std::string::npos) {
        return TokenType::Action;
    }
    
    if (symbol.find("see") != std::string::npos ||
        symbol.find("hear") != std::string::npos ||
        symbol.find("feel") != std::string::npos) {
        return TokenType::Perception;
    }
    
    if (symbol.find("near") != std::string::npos ||
        symbol.find("far") != std::string::npos ||
        symbol.find("before") != std::string::npos) {
        return TokenType::Relation;
    }
    
    if (symbol.find("<") != std::string::npos && symbol.find(">") != std::string::npos) {
        return TokenType::Meta;
    }
    
    return TokenType::Word; // Default
}

// Const versions of getToken methods
const LanguageSystem::SymbolicToken* LanguageSystem::getToken(const std::string& symbol) const {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    auto it = token_lookup_.find(symbol);
    if (it != token_lookup_.end()) {
        return &vocabulary_[it->second];
    }
    
    return nullptr;
}

const LanguageSystem::SymbolicToken* LanguageSystem::getToken(std::size_t token_id) const {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    if (token_id < vocabulary_.size()) {
        return &vocabulary_[token_id];
    }
    
    return nullptr;
}

bool LanguageSystem::getTokenId(const std::string& symbol, std::size_t& out_token_id) const {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    auto it = token_lookup_.find(symbol);
    if (it != token_lookup_.end()) {
        out_token_id = it->second;
        return true;
    }
    return false;
}

void LanguageSystem::logDevelopmentalEvent(const std::string& event, const std::string& details) {
    // Log developmental events for analysis
    // This could integrate with the MemoryDB system for persistent logging
    std::cout << "[LanguageSystem] " << event;
    if (!details.empty()) {
        std::cout << " - " << details;
    }
    std::cout << std::endl;
}

void LanguageSystem::updateConfig(const Config& new_config) {
    config_ = new_config;
    
    // Apply configuration changes
    if (vocabulary_.capacity() < config_.max_vocabulary_size) {
        vocabulary_.reserve(config_.max_vocabulary_size);
    }
}

// Serialization methods (simplified implementations)
std::string LanguageSystem::exportVocabularyToJson() const {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    std::ostringstream json;
    json << "{\"vocabulary\":[";
    
    for (std::size_t i = 0; i < vocabulary_.size(); ++i) {
        const auto& token = vocabulary_[i];
        json << "{\"symbol\":\"" << token.symbol << "\",";
        json << "\"type\":" << static_cast<int>(token.type) << ",";
        json << "\"activation\":" << token.activation_strength << ",";
        json << "\"usage_count\":" << token.usage_count << "}";
        
        if (i < vocabulary_.size() - 1) {
            json << ",";
        }
    }
    
    json << "]}";
    return json.str();
}

bool LanguageSystem::importVocabularyFromJson(const std::string& json_data) {
    // Simplified JSON parsing - in production, use a proper JSON library
    // This is a placeholder implementation
    return false;
}

std::string LanguageSystem::exportNarrationToJson() const {
    std::lock_guard<std::recursive_mutex> lock(narration_mutex_);
    
    std::ostringstream json;
    json << "{\"narration\":[";
    
    for (std::size_t i = 0; i < internal_narration_.size(); ++i) {
        const auto& entry = internal_narration_[i];
        json << "{\"confidence\":" << entry.confidence << ",";
        json << "\"context\":\"" << entry.context << "\",";
        json << "\"tokens\":[";
        
        for (std::size_t j = 0; j < entry.token_sequence.size(); ++j) {
            json << "\"" << entry.token_sequence[j].symbol << "\"";
            if (j < entry.token_sequence.size() - 1) {
                json << ",";
            }
        }
        
        json << "]}";
        if (i < internal_narration_.size() - 1) {
            json << ",";
        }
    }
    
    json << "]}";
    return json.str();
}

float LanguageSystem::calculateExperienceBasedLearningBoost(const SensoryExperience& experience) const {
    // Calculate learning boost based on sensory experience properties
    float boost = 1.0f;
    
    // Boost based on salience score
    boost *= (1.0f + experience.salience_score * config_.sensory_experience_learning_rate);
    
    // Additional boost for novel experiences
    float novelty = calculateSensoryExperienceNovelty(experience);
    boost *= (1.0f + novelty * 0.5f);
    
    // Cap the boost to prevent excessive values
    return std::min(boost, 3.0f);
}

// Proto-word crystallization implementation
void LanguageSystem::processProtoWordCrystallization() {
    std::lock_guard<std::recursive_mutex> lock(proto_word_mutex_);
    
    // Check existing proto-words for crystallization readiness
    for (std::size_t i = 0; i < proto_words_.size(); ++i) {
        ProtoWord& proto_word = proto_words_[i];
        
        if (!proto_word.is_crystallized && shouldCrystallizePattern(proto_word)) {
            promotePatternToCrystallized(i);
        }
        
        // Update stability scores based on recent usage
        float time_decay = 0.95f; // Slight decay over time
        proto_word.stability_score *= time_decay;
    }
}

std::size_t LanguageSystem::createProtoWord(const std::string& pattern, 
                                                             const std::vector<std::string>& phonemes) {
    std::lock_guard<std::recursive_mutex> lock(proto_word_mutex_);
    
    // Check if pattern already exists
    auto it = proto_word_lookup_.find(pattern);
    if (it != proto_word_lookup_.end()) {
        // Reinforce existing pattern
        reinforceProtoWord(it->second, config_.caregiver_response_boost);
        return it->second;
    }
    
    // Create new proto-word
    ProtoWord new_proto_word;
    new_proto_word.pattern = pattern;
    new_proto_word.phoneme_sequence = phonemes;
    new_proto_word.stability_score = 0.1f; // Initial stability
    new_proto_word.occurrence_count = 1;
    new_proto_word.first_occurrence = std::chrono::steady_clock::now();
    new_proto_word.last_occurrence = new_proto_word.first_occurrence;
    
    std::size_t proto_word_id = proto_words_.size();
    proto_words_.push_back(new_proto_word);
    proto_word_lookup_[pattern] = proto_word_id;
    
    return proto_word_id;
}

void LanguageSystem::reinforceProtoWord(std::size_t proto_word_id, 
                                                         float reinforcement_strength) {
    std::lock_guard<std::recursive_mutex> lock(proto_word_mutex_);
    
    if (proto_word_id >= proto_words_.size()) return;
    
    ProtoWord& proto_word = proto_words_[proto_word_id];
    proto_word.occurrence_count++;
    proto_word.stability_score += reinforcement_strength * config_.proto_word_crystallization_rate;
    proto_word.stability_score = std::min(1.0f, proto_word.stability_score);
    proto_word.last_occurrence = std::chrono::steady_clock::now();
    
    // Update phoneme stability for constituent phonemes
    for (const std::string& phoneme : proto_word.phoneme_sequence) {
        updatePhonemeStability(phoneme, reinforcement_strength);
    }
}

void LanguageSystem::updatePhonemeStability(const std::string& phoneme, 
                                                             float usage_boost) {
    std::lock_guard<std::recursive_mutex> lock(phoneme_tracking_mutex_);
    
    // Update stability score
    phoneme_stability_scores_[phoneme] += usage_boost * config_.proto_word_crystallization_rate;
    phoneme_stability_scores_[phoneme] = std::min(1.0f, phoneme_stability_scores_[phoneme]);
    
    // Track usage history
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    phoneme_usage_history_.push_back({phoneme, static_cast<std::uint64_t>(timestamp)});
    
    // Limit history size
    if (phoneme_usage_history_.size() > 1000) {
        phoneme_usage_history_.erase(phoneme_usage_history_.begin());
    }
}

float LanguageSystem::calculatePatternSimilarity(const std::string& pattern1, 
                                                                  const std::string& pattern2) const {
    if (pattern1 == pattern2) return 1.0f;
    
    // Simple edit distance-based similarity
    std::size_t len1 = pattern1.length();
    std::size_t len2 = pattern2.length();
    
    if (len1 == 0) return len2 == 0 ? 1.0f : 0.0f;
    if (len2 == 0) return 0.0f;
    
    // Calculate Levenshtein distance
    std::vector<std::vector<std::size_t>> dp(len1 + 1, std::vector<std::size_t>(len2 + 1));
    
    for (std::size_t i = 0; i <= len1; ++i) dp[i][0] = i;
    for (std::size_t j = 0; j <= len2; ++j) dp[0][j] = j;
    
    for (std::size_t i = 1; i <= len1; ++i) {
        for (std::size_t j = 1; j <= len2; ++j) {
            if (pattern1[i-1] == pattern2[j-1]) {
                dp[i][j] = dp[i-1][j-1];
            } else {
                dp[i][j] = 1 + std::min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
            }
        }
    }
    
    std::size_t max_len = std::max(len1, len2);
    return 1.0f - static_cast<float>(dp[len1][len2]) / static_cast<float>(max_len);
}

bool LanguageSystem::shouldCrystallizePattern(const ProtoWord& proto_word) const {
    // Check multiple criteria for crystallization
    bool stability_met = proto_word.stability_score >= proto_word.crystallization_threshold;
    bool frequency_met = proto_word.occurrence_count >= config_.min_occurrences_for_crystallization;
    bool caregiver_response = proto_word.caregiver_response_strength > 0.3f;
    
    return stability_met && frequency_met && (caregiver_response || proto_word.occurrence_count >= 5);
}

void LanguageSystem::processCaregiverResponse(const VisualLanguageFeatures& caregiver_reaction,
                                                                const AcousticFeatures& caregiver_audio) {
    std::lock_guard<std::recursive_mutex> lock(caregiver_mutex_);
    
    // Create a CaregiverContext from the provided parameters
    CaregiverContext context;
    context.attention_level = caregiver_reaction.attention_focus;
    context.emotional_valence = caregiver_reaction.motherese_face_boost > 0.5f ? 1.0f : 0.0f;
    context.face_embedding = caregiver_reaction.face_embedding;
    context.interaction_type = "response";
    context.timestamp = std::chrono::steady_clock::now();
    context.response_strength = std::max(caregiver_reaction.face_salience, caregiver_audio.energy_envelope);
    
    current_caregiver_context_ = context;
    caregiver_interaction_history_.push_back(context);
    
    // Limit history size
    if (caregiver_interaction_history_.size() > 100) {
        caregiver_interaction_history_.erase(caregiver_interaction_history_.begin());
    }
    
    // Update speech output state with caregiver attention detection
    // Caregiver attention is detected if attention level is high and there's significant response
    bool attention_detected = (context.attention_level > 0.5f) && (context.response_strength > 0.3f);
    speech_output_state_.caregiver_attention_detected = attention_detected;
    
    // If caregiver is responding to vocalization, reinforce recent patterns
    if (context.attention_level > 0.5f) {
        std::lock_guard<std::recursive_mutex> proto_lock(proto_word_mutex_);
        
        // Find recently used proto-words and reinforce them
        auto now = std::chrono::steady_clock::now();
        for (ProtoWord& proto_word : proto_words_) {
            auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(
                now - proto_word.last_occurrence).count();
            
            if (time_diff < 5) { // Within last 5 seconds
                float response_strength = context.response_strength * context.attention_level;
                proto_word.caregiver_response_strength += response_strength * config_.caregiver_response_boost;
                proto_word.caregiver_response_strength = std::min(1.0f, proto_word.caregiver_response_strength);
            }
        }
    }
}

float LanguageSystem::calculateCaregiverResponseStrength(const CaregiverContext& context) const {
    float strength = 0.0f;
    
    // Attention level contributes most
    strength += context.attention_level * 0.4f;
    
    // Positive emotional valence
    if (context.emotional_valence > 0.0f) {
        strength += context.emotional_valence * 0.3f;
    }
    
    // Interaction type bonus
    if (context.interaction_type == "praise" || context.interaction_type == "positive") {
        strength += 0.2f;
    }
    
    return std::min(1.0f, strength);
}

void LanguageSystem::reinforceBasedOnCaregiverFeedback(const std::string& vocalization, 
                                                                        const CaregiverContext& context) {
    // Extract patterns from vocalization and reinforce based on caregiver response
    std::vector<std::string> patterns = extractPatternsFromVocalization(vocalization);
    float response_strength = calculateCaregiverResponseStrength(context);
    
    std::lock_guard<std::recursive_mutex> lock(proto_word_mutex_);
    for (const std::string& pattern : patterns) {
        auto it = proto_word_lookup_.find(pattern);
        if (it != proto_word_lookup_.end()) {
            reinforceProtoWord(it->second, response_strength);
        }
    }
}

void LanguageSystem::performEnhancedBabbling(std::size_t num_phonemes) {
    std::lock_guard<std::recursive_mutex> lock(vocabulary_mutex_);
    
    std::vector<std::string> phoneme_sequence;
    
    for (std::size_t i = 0; i < num_phonemes; ++i) {
        std::string phoneme;
        
        // Bias towards proto-word patterns
        if (uniform_dist_(rng_) < 0.4f && !config_.target_proto_words.empty()) {
            // Generate phoneme biased towards target proto-words
            phoneme = generateBiasedPhoneme(0.6f);
        } else {
            // Generate random phoneme
            phoneme = generateBiasedPhoneme(0.0f);
        }
        
        phoneme_sequence.push_back(phoneme);
        
        // Create or update token
        std::string babble_token = "babble_" + phoneme + "_" + std::to_string(development_step_counter_.load());
        
        auto it = token_lookup_.find(babble_token);
        if (it == token_lookup_.end()) {
            SymbolicToken token;
            token.symbol = babble_token;
            token.type = TokenType::Phoneme;
            token.activation_strength = uniform_dist_(rng_);
            token.usage_count = 0;
            token.last_used = std::chrono::steady_clock::now();
            token.embedding = generateRandomEmbedding();
            
            std::size_t token_id = vocabulary_.size();
            vocabulary_.push_back(std::move(token));
            token_lookup_[babble_token] = token_id;
        } else {
            vocabulary_[it->second].activation_strength += 0.1f;
            vocabulary_[it->second].usage_count++;
            vocabulary_[it->second].last_used = std::chrono::steady_clock::now();
        }
    }
    
    // Track phoneme sequence patterns
    trackPhonemeSequencePatterns(phoneme_sequence);
    
    stats_.total_tokens_generated += num_phonemes;
}

std::string LanguageSystem::generateBiasedPhoneme(float proto_word_bias) {
    // If bias is high and we have target proto-words, bias towards them
    if (proto_word_bias > 0.3f && !config_.target_proto_words.empty()) {
        if (uniform_dist_(rng_) < proto_word_bias) {
            // Select from target proto-words
            std::size_t target_idx = static_cast<std::size_t>(uniform_dist_(rng_) * config_.target_proto_words.size());
            std::string target = config_.target_proto_words[target_idx];
            
            // Return first phoneme of target pattern
            if (!target.empty()) {
                return std::string(1, target[0]);
            }
        }
    }
    
    // Generate random phoneme (simplified)
    std::vector<std::string> common_phonemes = {"m", "b", "d", "p", "a", "e", "i", "o", "u"};
    std::size_t phoneme_idx = static_cast<std::size_t>(uniform_dist_(rng_) * common_phonemes.size());
    return common_phonemes[phoneme_idx];
}

void LanguageSystem::trackPhonemeSequencePatterns(const std::vector<std::string>& phoneme_sequence) {
    if (phoneme_sequence.size() < 2) return;
    
    // Look for repeating patterns (e.g., "ma-ma", "ba-ba")
    for (std::size_t i = 0; i < phoneme_sequence.size() - 1; ++i) {
        std::string pattern = phoneme_sequence[i] + "-" + phoneme_sequence[i];
        
        // Check if this creates a repeating pattern
        if (i + 1 < phoneme_sequence.size() && phoneme_sequence[i] == phoneme_sequence[i + 1]) {
            // Found repeating pattern, create or reinforce proto-word
            std::vector<std::string> pattern_phonemes = {phoneme_sequence[i], phoneme_sequence[i]};
            std::size_t proto_word_id = createProtoWord(pattern, pattern_phonemes);
            reinforceProtoWord(proto_word_id, config_.caregiver_response_boost);
        }
    }
    
    // Look for target proto-word patterns
    for (const std::string& target : config_.target_proto_words) {
        for (std::size_t i = 0; i < phoneme_sequence.size(); ++i) {
            if (phoneme_sequence[i] == std::string(1, target[0])) {
                // Found potential start of target pattern
                std::string pattern = target + "-" + target; // e.g., "ma-ma"
                std::vector<std::string> pattern_phonemes = {target, target};
                std::size_t proto_word_id = createProtoWord(pattern, pattern_phonemes);
                reinforceProtoWord(proto_word_id, config_.caregiver_response_boost * 0.5f);
            }
        }
    }
}

std::vector<std::string> LanguageSystem::generateProtoWordSequence(const std::string& target_pattern) {
    std::vector<std::string> sequence;
    
    // Simple pattern generation - repeat the pattern
    if (!target_pattern.empty()) {
        sequence.push_back(target_pattern);
        sequence.push_back(target_pattern); // Repetition for proto-word formation
    }
    
    return sequence;
}

void LanguageSystem::analyzeEmergingPatterns() {
    std::lock_guard<std::recursive_mutex> lock(proto_word_mutex_);
    
    // Analyze stability and usage patterns
    for (ProtoWord& proto_word : proto_words_) {
        float readiness = calculateCrystallizationReadiness(proto_word);
        
        // Update stability based on readiness
        proto_word.stability_score = std::max(proto_word.stability_score, readiness * 0.1f);
        
        // Check for crystallization
        if (!proto_word.is_crystallized && readiness > 0.8f) {
            proto_word.stability_score += config_.proto_word_crystallization_rate;
        }
    }
}

std::vector<std::string> LanguageSystem::extractPatternsFromVocalization(const std::string& vocalization) {
    std::vector<std::string> patterns;
    
    // Simple pattern extraction - look for repeated syllables
    if (vocalization.length() >= 4) {
        for (std::size_t i = 0; i < vocalization.length() - 1; ++i) {
            if (i + 1 < vocalization.length()) {
                std::string potential_pattern = std::string(1, vocalization[i]) + "-" + std::string(1, vocalization[i]);
                patterns.push_back(potential_pattern);
            }
        }
    }
    
    return patterns;
}

void LanguageSystem::promotePatternToCrystallized(std::size_t proto_word_id) {
    std::lock_guard<std::recursive_mutex> lock(proto_word_mutex_);
    
    if (proto_word_id >= proto_words_.size()) return;
    
    ProtoWord& proto_word = proto_words_[proto_word_id];
    proto_word.is_crystallized = true;
    proto_word.stability_score = 1.0f;
    
    // Create a stable token for this crystallized proto-word
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    std::string token_symbol = "proto_" + proto_word.pattern;
    
    auto it = token_lookup_.find(token_symbol);
    if (it == token_lookup_.end()) {
        SymbolicToken token;
        token.symbol = token_symbol;
        token.type = TokenType::Word; // Promote to word status
        token.activation_strength = 0.8f; // High initial activation
        token.usage_count = proto_word.occurrence_count;
        token.last_used = std::chrono::steady_clock::now();
        token.embedding = generateRandomEmbedding();
        
        std::size_t token_id = vocabulary_.size();
        vocabulary_.push_back(std::move(token));
        token_lookup_[token_symbol] = token_id;
    }
    
    logDevelopmentalEvent("Proto-word crystallized", proto_word.pattern);
}

float LanguageSystem::calculateCrystallizationReadiness(const ProtoWord& proto_word) const {
    float readiness = 0.0f;
    
    // Stability score contributes 40%
    readiness += proto_word.stability_score * 0.4f;
    
    // Occurrence frequency contributes 30%
    float frequency_score = std::min(1.0f, static_cast<float>(proto_word.occurrence_count) / 10.0f);
    readiness += frequency_score * 0.3f;
    
    // Caregiver response contributes 20%
    readiness += proto_word.caregiver_response_strength * 0.2f;
    
    // Time persistence contributes 10%
    auto now = std::chrono::steady_clock::now();
    auto time_span = std::chrono::duration_cast<std::chrono::minutes>(
        now - proto_word.first_occurrence).count();
    float persistence_score = std::min(1.0f, static_cast<float>(time_span) / 60.0f); // 1 hour = full score
    readiness += persistence_score * 0.1f;
    
    return std::min(1.0f, readiness);
}

// Enhanced multimodal attention and cross-modal integration implementation
void LanguageSystem::updateMultimodalAttention(
    const VisualLanguageFeatures& visual_features,
    const AcousticFeatures& acoustic_features) {
    
    std::lock_guard<std::recursive_mutex> lock(multimodal_attention_mutex_);
    
    // Calculate face attention weight
    multimodal_attention_state_.face_attention_weight = visual_features.face_salience * 
        config_.multimodal_attention_weight;
    
    // Calculate speech attention weight
    multimodal_attention_state_.speech_attention_weight = acoustic_features.attention_score * 
        config_.multimodal_attention_weight;
    
    // Calculate joint attention score
    multimodal_attention_state_.joint_attention_score = calculateJointAttentionScore(visual_features);
    
    // Update attention history
    float combined_attention = (multimodal_attention_state_.face_attention_weight + 
                               multimodal_attention_state_.speech_attention_weight) / 2.0f;
    multimodal_attention_state_.attention_history.push_back(combined_attention);
    
    // Limit history size
    if (multimodal_attention_state_.attention_history.size() > config_.attention_history_length) {
        multimodal_attention_state_.attention_history.erase(
            multimodal_attention_state_.attention_history.begin());
    }
    
    // Check for joint attention activation
    if (multimodal_attention_state_.joint_attention_score > config_.joint_attention_threshold) {
        multimodal_attention_state_.is_joint_attention_active = true;
        multimodal_attention_state_.last_attention_peak = std::chrono::steady_clock::now();
        
        // Record attention event
        auto now = std::chrono::steady_clock::now();
        multimodal_attention_state_.attention_events.push_back({combined_attention, now});
        
        // Limit attention events history
        if (multimodal_attention_state_.attention_events.size() > 20) {
            multimodal_attention_state_.attention_events.erase(
                multimodal_attention_state_.attention_events.begin());
        }
    } else {
        multimodal_attention_state_.is_joint_attention_active = false;
    }
    
    // Update proto-word attention boost based on current attention state
    if (multimodal_attention_state_.is_joint_attention_active) {
        multimodal_attention_state_.proto_word_attention_boost = 
            config_.joint_attention_learning_boost * combined_attention;
    } else {
        multimodal_attention_state_.proto_word_attention_boost *= 0.9f; // Gradual decay
    }
}

void LanguageSystem::processJointAttentionEvent(
    const std::vector<float>& shared_gaze_target,
    const std::string& spoken_token,
    float attention_strength) {
    
    std::lock_guard<std::recursive_mutex> attention_lock(multimodal_attention_mutex_);
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    // Find or create token for spoken word
    auto token_it = token_lookup_.find(spoken_token);
    if (token_it != token_lookup_.end()) {
        SymbolicToken& token = vocabulary_[token_it->second];
        
        // Boost token activation based on joint attention
        float attention_boost = attention_strength * config_.joint_attention_learning_boost;
        token.activation_strength += attention_boost;
        token.activation_strength = std::min(1.0f, token.activation_strength);
        
        // Store gaze target association
        if (!shared_gaze_target.empty()) {
            token.sensory_associations["gaze_target"] = shared_gaze_target[0]; // Simplified
        }
        
        // Update usage statistics
        token.usage_count++;
        token.last_used = std::chrono::steady_clock::now();
        
        stats_.grounding_associations_formed++;
    }
    
    // Update multimodal attention state
    multimodal_attention_state_.joint_attention_score = attention_strength;
    multimodal_attention_state_.is_joint_attention_active = true;
    multimodal_attention_state_.last_attention_peak = std::chrono::steady_clock::now();
}

float LanguageSystem::calculateJointAttentionScore(
    const VisualLanguageFeatures& visual_features) const {
    
    float joint_attention = 0.0f;
    
    // Face salience contributes to joint attention
    joint_attention += visual_features.face_salience * 0.4f;
    
    // Gaze alignment contributes
    joint_attention += visual_features.gaze_alignment * 0.3f;
    
    // Attention focus contributes
    joint_attention += visual_features.attention_focus * 0.2f;
    
    // Speech-vision coupling contributes
    joint_attention += visual_features.speech_vision_coupling * 0.1f;
    
    return std::min(1.0f, joint_attention);
}

void LanguageSystem::reinforceProtoWordFaceAssociation(
    const std::string& proto_word_pattern,
    const std::vector<float>& face_embedding,
    float association_strength) {
    
    std::lock_guard<std::recursive_mutex> coupling_lock(multimodal_attention_mutex_);
    
    // Update proto-word face associations
    face_speech_coupling_.proto_word_face_associations[proto_word_pattern] += 
        association_strength * config_.proto_word_face_association_strength;
    
    // Limit association strength
    face_speech_coupling_.proto_word_face_associations[proto_word_pattern] = 
        std::min(1.0f, face_speech_coupling_.proto_word_face_associations[proto_word_pattern]);
    
    // Update overall coupling strength
    face_speech_coupling_.coupling_strength += association_strength * 0.1f;
    face_speech_coupling_.coupling_strength = std::min(1.0f, face_speech_coupling_.coupling_strength);
    
    // Record coupling history
    face_speech_coupling_.coupling_history.push_back(face_speech_coupling_.coupling_strength);
    if (face_speech_coupling_.coupling_history.size() > 50) {
        face_speech_coupling_.coupling_history.erase(face_speech_coupling_.coupling_history.begin());
    }
    
    // Calculate stability measure
    if (face_speech_coupling_.coupling_history.size() > 10) {
        float mean = std::accumulate(face_speech_coupling_.coupling_history.end() - 10,
                                   face_speech_coupling_.coupling_history.end(), 0.0f) / 10.0f;
        float variance = 0.0f;
        for (auto it = face_speech_coupling_.coupling_history.end() - 10;
             it != face_speech_coupling_.coupling_history.end(); ++it) {
            variance += (*it - mean) * (*it - mean);
        }
        variance /= 10.0f;
        face_speech_coupling_.stability_measure = 1.0f / (1.0f + variance); // Higher stability = lower variance
    }
}

void LanguageSystem::updateFaceSpeechCoupling(
    const std::vector<float>& face_embedding,
    const AcousticFeatures& acoustic_features,
    const std::string& vocalization) {
    
    std::lock_guard<std::recursive_mutex> lock(multimodal_attention_mutex_);
    
    // Calculate coupling strength
    float coupling_strength = calculateFaceSpeechCouplingStrength(face_embedding, acoustic_features);
    
    // Update coupling state
    face_speech_coupling_.coupling_strength += coupling_strength * 
        face_speech_coupling_.coupling_learning_rate;
    face_speech_coupling_.coupling_strength = std::min(1.0f, face_speech_coupling_.coupling_strength);
    
    // Check for motherese detection
    if (detectMotherese(acoustic_features)) {
        face_speech_coupling_.motherese_detection_strength += 0.1f;
        face_speech_coupling_.motherese_detection_strength = 
            std::min(1.0f, face_speech_coupling_.motherese_detection_strength);
        
        // Apply motherese boost to coupling
        face_speech_coupling_.coupling_strength += config_.motherese_face_coupling_boost * 0.1f;
        face_speech_coupling_.coupling_strength = std::min(1.0f, face_speech_coupling_.coupling_strength);
    }
    
    // Process caregiver face recognition
    processCaregiverFaceRecognition(face_embedding);
    
    // Update temporal synchrony based on acoustic features
    face_speech_coupling_.temporal_synchrony = acoustic_features.attention_score * 0.5f + 
        face_speech_coupling_.temporal_synchrony * 0.5f; // Moving average
    
    // Extract patterns from vocalization and reinforce associations
    std::vector<std::string> patterns = extractPatternsFromVocalization(vocalization);
    for (const std::string& pattern : patterns) {
        reinforceProtoWordFaceAssociation(pattern, face_embedding, coupling_strength);
    }
}

float LanguageSystem::calculateFaceSpeechCouplingStrength(
    const std::vector<float>& face_embedding,
    const AcousticFeatures& acoustic_features) const {
    
    float coupling_strength = 0.0f;
    
    // Face presence contributes to coupling
    if (!face_embedding.empty()) {
        float face_magnitude = 0.0f;
        for (float val : face_embedding) {
            face_magnitude += val * val;
        }
        coupling_strength += std::sqrt(face_magnitude) / face_embedding.size() * 0.3f;
    }
    
    // Acoustic attention score contributes
    coupling_strength += acoustic_features.attention_score * 0.4f;
    
    // Prosodic features contribute
    coupling_strength += acoustic_features.motherese_score * 0.2f;
    coupling_strength += acoustic_features.intonation_slope * 0.1f;
    
    return std::min(1.0f, coupling_strength);
}

bool LanguageSystem::detectMotherese(const AcousticFeatures& acoustic_features) const {
    // Simple motherese detection based on acoustic features
    bool high_pitch = acoustic_features.pitch_contour > 200.0f; // Higher than normal speech
    bool rising_intonation = acoustic_features.intonation_slope > 0.1f;
    bool high_energy = acoustic_features.energy_envelope > 0.6f;
    bool motherese_features = acoustic_features.motherese_score > 0.5f;
    
    // Motherese typically has at least 2 of these features
    int feature_count = (high_pitch ? 1 : 0) + (rising_intonation ? 1 : 0) + 
                       (high_energy ? 1 : 0) + (motherese_features ? 1 : 0);
    
    return feature_count >= 2;
}

void LanguageSystem::processCaregiverFaceRecognition(
    const std::vector<float>& face_embedding) {
    
    std::lock_guard<std::recursive_mutex> lock(caregiver_recognition_mutex_);
    
    if (face_embedding.empty()) return;
    
    // Check if this is a known caregiver face
    std::string caregiver_id = identifyCaregiver(face_embedding);
    
    if (!caregiver_id.empty()) {
        // Update caregiver interaction
        face_speech_coupling_.is_caregiver_interaction = true;
        face_speech_coupling_.caregiver_recognition_confidence = 
            caregiver_face_confidences_[caregiver_id];
        
        // Update interaction history
        updateCaregiverInteractionHistory(caregiver_id, 1.0f);
        
        // Apply caregiver recognition boost
        std::lock_guard<std::recursive_mutex> attention_lock(multimodal_attention_mutex_);
        multimodal_attention_state_.caregiver_face_priority = config_.caregiver_recognition_boost;
    } else {
        face_speech_coupling_.is_caregiver_interaction = false;
        face_speech_coupling_.caregiver_recognition_confidence = 0.0f;
        multimodal_attention_state_.caregiver_face_priority = 0.0f;
    }
}

void LanguageSystem::registerCaregiverFace(
    const std::vector<float>& face_embedding, 
    const std::string& caregiver_id) {
    
    std::lock_guard<std::recursive_mutex> lock(caregiver_recognition_mutex_);
    
    if (face_embedding.empty()) return;
    
    // Store caregiver face embedding
    known_caregiver_faces_.push_back(face_embedding);
    caregiver_face_confidences_[caregiver_id] = 1.0f;
    
    logDevelopmentalEvent("Caregiver registered", caregiver_id);
}

bool LanguageSystem::isCaregiverFace(
    const std::vector<float>& face_embedding, 
    float recognition_threshold) const {
    
    std::lock_guard<std::recursive_mutex> lock(caregiver_recognition_mutex_);
    
    if (face_embedding.empty() || known_caregiver_faces_.empty()) {
        return false;
    }
    
    // Check similarity with known caregiver faces
    for (const auto& known_face : known_caregiver_faces_) {
        float similarity = cosineSimilarity(face_embedding, known_face);
        if (similarity >= recognition_threshold) {
            return true;
        }
    }
    
    return false;
}

std::string LanguageSystem::identifyCaregiver(
    const std::vector<float>& face_embedding) const {
    
    std::lock_guard<std::recursive_mutex> lock(caregiver_recognition_mutex_);
    
    if (face_embedding.empty()) return "";
    
    float best_similarity = 0.0f;
    std::string best_caregiver_id = "";
    
    // Find best matching caregiver
    for (const auto& [caregiver_id, confidence] : caregiver_face_confidences_) {
        // Find corresponding face embedding (simplified - in real system would use proper indexing)
        if (!known_caregiver_faces_.empty()) {
            float similarity = cosineSimilarity(face_embedding, known_caregiver_faces_[0]);
            if (similarity > best_similarity && similarity > 0.8f) {
                best_similarity = similarity;
                best_caregiver_id = caregiver_id;
            }
        }
    }
    
    return best_caregiver_id;
}

void LanguageSystem::updateCaregiverInteractionHistory(
    const std::string& caregiver_id, 
    float interaction_quality) {
    
    std::lock_guard<std::recursive_mutex> lock(caregiver_recognition_mutex_);
    
    // Add interaction to history
    CaregiverContext context;
    context.interaction_type = caregiver_id;
    context.response_strength = interaction_quality;
    context.timestamp = std::chrono::steady_clock::now();
    recent_caregiver_interactions_.push_back(context);
    
    // Limit history size
    if (recent_caregiver_interactions_.size() > 100) {
        recent_caregiver_interactions_.pop_front();
    }
    
    // Update caregiver confidence based on interaction quality
    if (caregiver_face_confidences_.find(caregiver_id) != caregiver_face_confidences_.end()) {
        caregiver_face_confidences_[caregiver_id] += interaction_quality * 0.01f;
        caregiver_face_confidences_[caregiver_id] = 
            std::min(1.0f, caregiver_face_confidences_[caregiver_id]);
    }
}

void LanguageSystem::processAttentionGuidedLearning(
    const std::string& vocalization,
    const MultimodalAttentionState& attention_state) {
    
    // Extract patterns from vocalization
    std::vector<std::string> patterns = extractPatternsFromVocalization(vocalization);
    
    // Calculate attention-based learning rate
    float learning_rate = calculateAttentionBasedLearningRate(attention_state);
    
    // Boost proto-words based on attention
    for (const std::string& pattern : patterns) {
        boostProtoWordBasedOnAttention(pattern, learning_rate);
    }
}

void LanguageSystem::boostProtoWordBasedOnAttention(
    const std::string& proto_word_pattern,
    float attention_boost) {
    
    std::lock_guard<std::recursive_mutex> lock(proto_word_mutex_);
    
    auto it = proto_word_lookup_.find(proto_word_pattern);
    if (it != proto_word_lookup_.end()) {
        ProtoWord& proto_word = proto_words_[it->second];
        proto_word.stability_score += attention_boost * config_.joint_attention_learning_boost;
        proto_word.stability_score = std::min(1.0f, proto_word.stability_score);
        
        // Update occurrence count
        proto_word.occurrence_count++;
        proto_word.last_occurrence = std::chrono::steady_clock::now();
    }
}

float LanguageSystem::calculateAttentionBasedLearningRate(
    const MultimodalAttentionState& attention_state) const {
    
    float learning_rate = 0.0f;
    
    // Joint attention provides strongest learning signal
    if (attention_state.is_joint_attention_active) {
        learning_rate += attention_state.joint_attention_score * 0.5f;
    }
    
    // Face attention contributes
    learning_rate += attention_state.face_attention_weight * 0.3f;
    
    // Speech attention contributes
    learning_rate += attention_state.speech_attention_weight * 0.2f;
    
    // Proto-word attention boost
    learning_rate += attention_state.proto_word_attention_boost;
    
    return std::min(1.0f, learning_rate);
}

// Grounding associations and semantic anchoring implementation
std::size_t LanguageSystem::createGroundingAssociation(
    std::size_t token_id, const std::string& object_category,
    const std::vector<float>& visual_features,
    const std::vector<float>& tactile_features,
    const std::vector<float>& auditory_features) {
    
    std::lock_guard<std::recursive_mutex> lock(grounding_associations_mutex_);
    
    // Create new grounding association
    GroundingAssociation grounding;
    grounding.token_id = token_id;
    grounding.object_category = object_category;
    grounding.visual_features = visual_features;
    grounding.tactile_features = tactile_features;
    grounding.auditory_features = auditory_features;
    
    // Calculate initial grounding strength based on available modalities
    float initial_strength = 0.0f;
    if (!visual_features.empty()) {
        grounding.visual_grounding_confidence = config_.visual_grounding_weight;
        initial_strength += config_.visual_grounding_weight;
    }
    if (!tactile_features.empty()) {
        grounding.tactile_grounding_confidence = config_.tactile_grounding_weight;
        initial_strength += config_.tactile_grounding_weight;
    }
    if (!auditory_features.empty()) {
        grounding.auditory_grounding_confidence = config_.auditory_grounding_weight;
        initial_strength += config_.auditory_grounding_weight;
    }
    
    grounding.grounding_strength = initial_strength * config_.grounding_association_strength;
    grounding.exposure_count = 1;
    grounding.first_encounter = std::chrono::steady_clock::now();
    grounding.last_encounter = grounding.first_encounter;
    
    // Add to associations
    std::size_t grounding_id = grounding_associations_.size();
    grounding_associations_.push_back(grounding);
    
    // Update lookup tables
    object_to_grounding_lookup_[object_category].push_back(grounding_id);
    token_to_grounding_lookup_[token_id].push_back(grounding_id);
    
    stats_.grounding_associations_formed++;
    logDevelopmentalEvent("Grounding association created", object_category + " -> token_" + std::to_string(token_id));
    
    return grounding_id;
}

void LanguageSystem::reinforceGroundingAssociation(
    std::size_t grounding_id, float reinforcement_strength) {
    
    std::lock_guard<std::recursive_mutex> lock(grounding_associations_mutex_);
    
    if (grounding_id >= grounding_associations_.size()) return;
    
    GroundingAssociation& grounding = grounding_associations_[grounding_id];
    
    // Increase grounding strength
    grounding.grounding_strength += reinforcement_strength * grounding.learning_rate;
    grounding.grounding_strength = std::min(1.0f, grounding.grounding_strength);
    
    // Update exposure count and timing
    grounding.exposure_count++;
    grounding.last_encounter = std::chrono::steady_clock::now();
    
    // Check if grounding has become stable
    if (!grounding.is_stable_grounding && isStableGrounding(grounding)) {
        grounding.is_stable_grounding = true;
        logDevelopmentalEvent("Grounding stabilized", grounding.object_category);
        
        // Promote associated token if it exists
        if (grounding.token_id < vocabulary_.size()) {
            promoteToSemanticallGrounded(grounding.token_id);
        }
    }
}

void LanguageSystem::updateGroundingAssociation(
    std::size_t grounding_id, const std::string& interaction_type,
    const std::vector<float>& spatial_context) {
    
    std::lock_guard<std::recursive_mutex> lock(grounding_associations_mutex_);
    
    if (grounding_id >= grounding_associations_.size()) return;
    
    GroundingAssociation& grounding = grounding_associations_[grounding_id];
    
    // Update interaction information
    grounding.interaction_type = interaction_type;
    grounding.spatial_context = spatial_context;
    
    // Update temporal context (simplified as timestamp)
    auto now = std::chrono::steady_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    grounding.temporal_context = {static_cast<float>(timestamp)};
    
    grounding.last_encounter = now;
}

bool LanguageSystem::isStableGrounding(const GroundingAssociation& grounding) const {
    // Check multiple criteria for stable grounding
    bool strength_met = grounding.grounding_strength >= config_.grounding_stability_threshold;
    bool exposure_met = grounding.exposure_count >= config_.min_exposures_for_stable_grounding;
    bool multimodal = (grounding.visual_grounding_confidence > 0.0f ? 1 : 0) +
                     (grounding.tactile_grounding_confidence > 0.0f ? 1 : 0) +
                     (grounding.auditory_grounding_confidence > 0.0f ? 1 : 0) >= 2;
    
    return strength_met && exposure_met && multimodal;
}

std::vector<std::size_t> LanguageSystem::findGroundingAssociationsForToken(
    std::size_t token_id) const {
    
    std::lock_guard<std::recursive_mutex> lock(grounding_associations_mutex_);
    
    auto it = token_to_grounding_lookup_.find(token_id);
    if (it != token_to_grounding_lookup_.end()) {
        return it->second;
    }
    return {};
}

std::vector<std::size_t> LanguageSystem::findGroundingAssociationsForObject(
    const std::string& object_category) const {
    
    std::lock_guard<std::recursive_mutex> lock(grounding_associations_mutex_);
    
    auto it = object_to_grounding_lookup_.find(object_category);
    if (it != object_to_grounding_lookup_.end()) {
        return it->second;
    }
    return {};
}

void LanguageSystem::processSensoryExperience(const SensoryExperience& experience) {
    std::lock_guard<std::recursive_mutex> lock(sensory_experience_mutex_);
    
    // Add experience to history
    sensory_experience_history_.push_back(experience);
    
    // Limit history size
    if (sensory_experience_history_.size() > config_.sensory_experience_history_length) {
        sensory_experience_history_.pop_front();
    }
    
    // Add to type-specific lookup
    experience_type_lookup_[experience.experience_type].push_back(experience);
    
    // Limit type-specific history
    if (experience_type_lookup_[experience.experience_type].size() > 50) {
        experience_type_lookup_[experience.experience_type].erase(
            experience_type_lookup_[experience.experience_type].begin());
    }
    
    // Integrate with proto-words if experience is salient
    if (experience.salience_score >= config_.salience_threshold) {
        integrateSensoryExperienceWithProtoWords(experience);
    }
    
    // Update current sensory context
    if (!experience.sensory_data.empty()) {
        updateSensoryContext(experience.sensory_data);
    }
}

void LanguageSystem::integrateSensoryExperienceWithProtoWords(
    const SensoryExperience& experience) {
    
    std::lock_guard<std::recursive_mutex> proto_lock(proto_word_mutex_);
    
    // Find proto-words that were active during this experience
    for (const std::string& token : experience.co_occurring_tokens) {
        // Extract potential proto-word patterns from token
        std::vector<std::string> patterns = extractPatternsFromVocalization(token);
        
        for (const std::string& pattern : patterns) {
            auto it = proto_word_lookup_.find(pattern);
            if (it != proto_word_lookup_.end()) {
                // Boost proto-word based on sensory experience
                boostProtoWordFromSensoryExperience(pattern, experience.salience_score);
            }
        }
    }
}

float LanguageSystem::calculateSensoryExperienceNovelty(
    const SensoryExperience& experience) const {
    
    std::lock_guard<std::recursive_mutex> lock(sensory_experience_mutex_);
    
    if (experience.sensory_data.empty()) return 0.0f;
    
    // Find similar experiences
    std::vector<SensoryExperience> similar = getSimilarExperiences(experience, 0.8f);
    
    // Novelty is inversely related to number of similar experiences
    float novelty = 1.0f / (1.0f + static_cast<float>(similar.size()));
    
    // Boost novelty if this is a new type of experience
    auto type_it = experience_type_lookup_.find(experience.experience_type);
    if (type_it == experience_type_lookup_.end() || type_it->second.empty()) {
        novelty += 0.3f;
    }
    
    return std::min(1.0f, novelty);
}

void LanguageSystem::updateSensoryContext(const std::vector<float>& new_context) {
    std::lock_guard<std::recursive_mutex> lock(sensory_experience_mutex_);
    
    // Update current sensory context with moving average
    if (current_sensory_context_.empty()) {
        current_sensory_context_ = new_context;
    } else {
        // Ensure same size
        if (current_sensory_context_.size() == new_context.size()) {
            for (std::size_t i = 0; i < current_sensory_context_.size(); ++i) {
                current_sensory_context_[i] = current_sensory_context_[i] * 0.7f + new_context[i] * 0.3f;
            }
        } else {
            current_sensory_context_ = new_context; // Replace if size mismatch
        }
    }
}

std::vector<LanguageSystem::SensoryExperience> 
LanguageSystem::getSimilarExperiences(
    const SensoryExperience& target_experience, float similarity_threshold) const {
    
    std::lock_guard<std::recursive_mutex> lock(sensory_experience_mutex_);
    
    std::vector<SensoryExperience> similar_experiences;
    
    // Search through experiences of the same type
    auto type_it = experience_type_lookup_.find(target_experience.experience_type);
    if (type_it != experience_type_lookup_.end()) {
        for (const SensoryExperience& experience : type_it->second) {
            if (experience.sensory_pattern.size() == target_experience.sensory_pattern.size()) {
                float similarity = cosineSimilarity(experience.sensory_pattern, 
                                                  target_experience.sensory_pattern);
                if (similarity >= similarity_threshold) {
                    similar_experiences.push_back(experience);
                }
            }
        }
    }
    
    return similar_experiences;
}

void LanguageSystem::processMultimodalGroundingEvent(
    const std::string& spoken_token,
    const std::vector<float>& visual_features,
    const std::vector<float>& tactile_features,
    const std::vector<float>& auditory_features,
    const std::string& object_category) {
    
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    // Find or create token
    std::size_t token_id;
    auto token_it = token_lookup_.find(spoken_token);
    if (token_it != token_lookup_.end()) {
        token_id = token_it->second;
    } else {
        // Create new token
        token_id = createToken(spoken_token, TokenType::Word);
    }
    
    // Create or reinforce grounding association
    std::vector<std::size_t> existing_groundings = findGroundingAssociationsForToken(token_id);
    
    bool found_matching_grounding = false;
    for (std::size_t grounding_id : existing_groundings) {
        if (grounding_associations_[grounding_id].object_category == object_category) {
            // Reinforce existing grounding
            reinforceGroundingAssociation(grounding_id, 0.2f);
            updateGroundingAssociation(grounding_id, "multimodal_event", {});
            found_matching_grounding = true;
            break;
        }
    }
    
    if (!found_matching_grounding) {
        // Create new grounding association
        std::size_t grounding_id = createGroundingAssociation(token_id, object_category,
                                                             visual_features, tactile_features, 
                                                             auditory_features);
        updateGroundingAssociation(grounding_id, "multimodal_event", {});
    }
    
    // Create sensory experience for this event
    SensoryExperience experience;
    experience.experience_type = "multimodal_grounding";
    experience.sensory_pattern = visual_features; // Use visual as primary pattern
    experience.salience_score = 0.8f; // High salience for multimodal events
    experience.novelty_score = calculateSensoryExperienceNovelty(experience);
    experience.timestamp = std::chrono::steady_clock::now();
    experience.co_occurring_tokens = {spoken_token};
    experience.experience_quality = 0.9f; // High quality for structured events
    experience.reliability_score = 0.8f;
    experience.repetition_count = 1;
    
    processSensoryExperience(experience);
}

void LanguageSystem::strengthenSemanticAnchoring(
    std::size_t token_id, const std::vector<float>& sensory_pattern,
    const std::string& modality, float anchoring_strength) {
    
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    if (token_id >= vocabulary_.size()) return;
    
    SymbolicToken& token = vocabulary_[token_id];
    
    // Update token's sensory associations
    std::string association_key = modality + "_anchoring";
    token.sensory_associations[association_key] += anchoring_strength;
    token.sensory_associations[association_key] = std::min(1.0f, token.sensory_associations[association_key]);
    
    // Boost token activation
    token.activation_strength += anchoring_strength * 0.1f;
    token.activation_strength = std::min(1.0f, token.activation_strength);
    
    // Update usage statistics
    token.usage_count++;
    token.last_used = std::chrono::steady_clock::now();
}

float LanguageSystem::calculateSemanticGroundingStrength(std::size_t token_id) const {
    std::lock_guard<std::recursive_mutex> lock(grounding_associations_mutex_);
    
    std::vector<std::size_t> groundings = findGroundingAssociationsForToken(token_id);
    
    if (groundings.empty()) return 0.0f;
    
    float total_strength = 0.0f;
    for (std::size_t grounding_id : groundings) {
        if (grounding_id < grounding_associations_.size()) {
            total_strength += grounding_associations_[grounding_id].grounding_strength;
        }
    }
    
    return std::min(1.0f, total_strength / static_cast<float>(groundings.size()));
}

void LanguageSystem::promoteToSemanticallGrounded(std::size_t token_id) {
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    if (token_id >= vocabulary_.size()) return;
    
    SymbolicToken& token = vocabulary_[token_id];
    
    // Mark token as semantically grounded
    token.sensory_associations["semantically_grounded"] = 1.0f;
    
    // Boost activation strength
    token.activation_strength += 0.2f;
    token.activation_strength = std::min(1.0f, token.activation_strength);
    
    // Change token type to Word if it was a Phoneme
    if (token.type == TokenType::Phoneme) {
        token.type = TokenType::Word;
    }
    
    logDevelopmentalEvent("Token semantically grounded", token.symbol);
}

void LanguageSystem::processExperienceDrivenLearning(
    const std::string& vocalization,
    const std::vector<SensoryExperience>& concurrent_experiences) {
    
    // Extract patterns from vocalization
    std::vector<std::string> patterns = extractPatternsFromVocalization(vocalization);
    
    // Process each concurrent experience
    for (const SensoryExperience& experience : concurrent_experiences) {
        // Calculate learning boost from this experience
        float learning_boost = calculateExperienceBasedLearningBoost(experience);
        
        // Apply boost to relevant proto-word patterns
        for (const std::string& pattern : patterns) {
            boostProtoWordFromSensoryExperience(pattern, experience.salience_score);
        }
        
        // Process the sensory experience
        processSensoryExperience(experience);
    }
}

void LanguageSystem::boostProtoWordFromSensoryExperience(
    const std::string& proto_word,
    float boost_strength) {
    
    std::lock_guard<std::recursive_mutex> lock(proto_word_mutex_);
    
    auto it = proto_word_lookup_.find(proto_word);
    if (it != proto_word_lookup_.end()) {
        ProtoWord& proto_word_obj = proto_words_[it->second];
        
        // Apply boost to proto-word stability
        proto_word_obj.stability_score += boost_strength * config_.sensory_experience_learning_rate;
        proto_word_obj.stability_score = std::min(1.0f, proto_word_obj.stability_score);
        
        // Update occurrence count
        proto_word_obj.occurrence_count++;
        proto_word_obj.last_occurrence = std::chrono::steady_clock::now();
    }
}

// Prosodic pattern learning and intonation-guided attention implementation
void LanguageSystem::processProsodicPatternLearning(
    const AcousticFeatures& acoustic_features, const std::string& co_occurring_token) {
    
    std::lock_guard<std::recursive_mutex> lock(prosodic_pattern_mutex_);
    
    // Update intonation-guided attention first
    updateIntonationGuidedAttention(acoustic_features);
    
    // Try to detect existing prosodic pattern
    std::size_t pattern_id = detectProsodicPattern(acoustic_features);
    
    if (pattern_id != std::numeric_limits<std::size_t>::max()) {
        // Reinforce existing pattern
        float reinforcement = config_.prosodic_pattern_learning_rate;
        
        // Apply motherese boost if detected
        if (detectMotherese(acoustic_features)) {
            reinforcement *= config_.motherese_pattern_boost;
            prosodic_patterns_[pattern_id].is_motherese_pattern = true;
        }
        
        reinforceProsodicPattern(pattern_id, reinforcement);
        
        // Associate with co-occurring token if provided
        if (!co_occurring_token.empty()) {
            auto& pattern = prosodic_patterns_[pattern_id];
            auto it = std::find(pattern.associated_tokens.begin(), 
                              pattern.associated_tokens.end(), co_occurring_token);
            if (it == pattern.associated_tokens.end()) {
                pattern.associated_tokens.push_back(co_occurring_token);
            }
        }
    } else {
        // Create new prosodic pattern if we have enough acoustic history
        if (recent_acoustic_features_.size() >= 3) {
            std::vector<AcousticFeatures> sequence(recent_acoustic_features_.end() - 3,
                                                  recent_acoustic_features_.end());
            sequence.push_back(acoustic_features);
            
            ProsodicPattern new_pattern = extractProsodicPattern(sequence);
            new_pattern.pattern_name = "pattern_" + std::to_string(prosodic_patterns_.size());
            new_pattern.first_detected = std::chrono::steady_clock::now();
            new_pattern.last_detected = new_pattern.first_detected;
            new_pattern.occurrence_count = 1;
            new_pattern.recognition_confidence = 0.3f; // Initial confidence
            
            if (!co_occurring_token.empty()) {
                new_pattern.associated_tokens.push_back(co_occurring_token);
            }
            
            // Check if this is a motherese pattern
            if (detectMotherese(acoustic_features)) {
                new_pattern.is_motherese_pattern = true;
                new_pattern.attention_weight = config_.motherese_pattern_boost;
                new_pattern.learning_boost_factor = config_.motherese_pattern_boost;
            }
            
            std::size_t new_pattern_id = prosodic_patterns_.size();
            prosodic_patterns_.push_back(new_pattern);
            prosodic_pattern_lookup_[new_pattern.pattern_name] = new_pattern_id;
            
            logDevelopmentalEvent("New prosodic pattern detected", new_pattern.pattern_name);
        }
    }
    
    // Update recent acoustic features history
    recent_acoustic_features_.push_back(acoustic_features);
    if (recent_acoustic_features_.size() > config_.prosodic_pattern_history_length) {
        recent_acoustic_features_.pop_front();
    }
}

std::size_t LanguageSystem::detectProsodicPattern(
    const AcousticFeatures& acoustic_features) {
    
    std::lock_guard<std::recursive_mutex> lock(prosodic_pattern_mutex_);
    
    if (recent_acoustic_features_.size() < 2) {
        return std::numeric_limits<std::size_t>::max(); // Not enough history
    }
    
    // Create a short sequence for pattern matching
    std::vector<AcousticFeatures> current_sequence(recent_acoustic_features_.end() - 2,
                                                   recent_acoustic_features_.end());
    current_sequence.push_back(acoustic_features);
    
    ProsodicPattern current_pattern = extractProsodicPattern(current_sequence);
    
    // Find most similar existing pattern
    float best_similarity = 0.0f;
    std::size_t best_pattern_id = std::numeric_limits<std::size_t>::max();
    
    for (std::size_t i = 0; i < prosodic_patterns_.size(); ++i) {
        float similarity = calculateProsodicPatternSimilarity(current_pattern, prosodic_patterns_[i]);
        if (similarity > best_similarity && similarity > 0.7f) {
            best_similarity = similarity;
            best_pattern_id = i;
        }
    }
    
    return best_pattern_id;
}

void LanguageSystem::reinforceProsodicPattern(
    std::size_t pattern_id, float reinforcement_strength) {
    
    std::lock_guard<std::recursive_mutex> lock(prosodic_pattern_mutex_);
    
    if (pattern_id >= prosodic_patterns_.size()) return;
    
    ProsodicPattern& pattern = prosodic_patterns_[pattern_id];
    
    // Increase pattern stability and confidence
    pattern.pattern_stability += reinforcement_strength;
    pattern.pattern_stability = std::min(1.0f, pattern.pattern_stability);
    
    pattern.recognition_confidence += reinforcement_strength * 0.1f;
    pattern.recognition_confidence = std::min(1.0f, pattern.recognition_confidence);
    
    // Update occurrence count and timing
    pattern.occurrence_count++;
    pattern.last_detected = std::chrono::steady_clock::now();
    pattern.detection_history.push_back(pattern.last_detected);
    
    // Limit detection history size
    if (pattern.detection_history.size() > 20) {
        pattern.detection_history.erase(pattern.detection_history.begin());
    }
    
    // Update attention weight based on stability
    pattern.attention_weight = pattern.pattern_stability * config_.intonation_attention_boost;
    
    // Update learning boost factor
    pattern.learning_boost_factor = pattern.pattern_stability * 
        (pattern.is_motherese_pattern ? config_.motherese_pattern_boost : 1.0f);
    
    // Update prosodic preferences
    updateProsodicPreferences(pattern.pattern_name, reinforcement_strength);
}

bool LanguageSystem::isStableProsodicPattern(const ProsodicPattern& pattern) const {
    bool stability_met = pattern.pattern_stability >= config_.prosodic_pattern_stability_threshold;
    bool occurrence_met = pattern.occurrence_count >= config_.min_pattern_occurrences;
    bool confidence_met = pattern.recognition_confidence >= 0.6f;
    
    return stability_met && occurrence_met && confidence_met;
}

std::vector<std::size_t> LanguageSystem::findSimilarProsodicPatterns(
    const AcousticFeatures& acoustic_features, float similarity_threshold) const {
    
    std::lock_guard<std::recursive_mutex> lock(prosodic_pattern_mutex_);
    
    std::vector<std::size_t> similar_patterns;
    
    if (recent_acoustic_features_.size() < 2) return similar_patterns;
    
    // Create current pattern for comparison
    std::vector<AcousticFeatures> current_sequence(recent_acoustic_features_.end() - 2,
                                                   recent_acoustic_features_.end());
    current_sequence.push_back(acoustic_features);
    
    ProsodicPattern current_pattern = extractProsodicPattern(current_sequence);
    
    // Find similar patterns
    for (std::size_t i = 0; i < prosodic_patterns_.size(); ++i) {
        float similarity = calculateProsodicPatternSimilarity(current_pattern, prosodic_patterns_[i]);
        if (similarity >= similarity_threshold) {
            similar_patterns.push_back(i);
        }
    }
    
    return similar_patterns;
}

void LanguageSystem::updateIntonationGuidedAttention(
    const AcousticFeatures& acoustic_features) {
    
    std::lock_guard<std::recursive_mutex> lock(prosodic_pattern_mutex_);
    
    // Calculate current intonation salience
    float intonation_salience = calculateSoundSalience(acoustic_features);
    intonation_attention_state_.current_intonation_salience = intonation_salience;
    
    // Update intonation history
    intonation_attention_state_.intonation_history.push_back(intonation_salience);
    if (intonation_attention_state_.intonation_history.size() > 20) {
        intonation_attention_state_.intonation_history.erase(
            intonation_attention_state_.intonation_history.begin());
    }
    
    // Check for prosodic attention activation
    if (intonation_salience > intonation_attention_state_.prosodic_attention_threshold) {
        intonation_attention_state_.is_prosodic_attention_active = true;
        
        // Calculate learning boost
        float learning_boost = calculateIntonationLearningBoost(acoustic_features);
        intonation_attention_state_.intonation_learning_boost = learning_boost;
        
        // Record learning event
        intonation_attention_state_.intonation_learning_history.push_back(
            {intonation_salience, learning_boost});
        
        // Limit learning history
        if (intonation_attention_state_.intonation_learning_history.size() > 50) {
            intonation_attention_state_.intonation_learning_history.erase(
                intonation_attention_state_.intonation_learning_history.begin());
        }
    } else {
        intonation_attention_state_.is_prosodic_attention_active = false;
        intonation_attention_state_.intonation_learning_boost *= 0.9f; // Gradual decay
    }
    
    // Adapt attention threshold based on experience
    adaptProsodicAttentionThreshold(intonation_salience);
}

float LanguageSystem::calculateIntonationLearningBoost(
    const AcousticFeatures& acoustic_features) const {
    
    float boost = 0.0f;
    
    // Rising intonation provides strong learning boost
    if (acoustic_features.intonation_slope > 0.1f) {
        boost += config_.rising_intonation_learning_boost * 
                (acoustic_features.intonation_slope / 10.0f); // Normalize slope
    }
    
    // Falling intonation provides moderate boost
    if (acoustic_features.intonation_slope < -0.1f) {
        boost += config_.falling_intonation_learning_boost * 
                (std::abs(acoustic_features.intonation_slope) / 10.0f);
    }
    
    // Motherese features provide additional boost
    if (acoustic_features.motherese_score > 0.5f) {
        boost += config_.motherese_pattern_boost * acoustic_features.motherese_score;
    }
    
    // High pitch provides attention boost
    if (acoustic_features.pitch_contour > 200.0f) {
        boost += 0.2f * (acoustic_features.pitch_contour / 400.0f); // Normalize to 0-0.2
    }
    
    return std::min(1.0f, boost);
}

void LanguageSystem::adaptProsodicAttentionThreshold(
    float current_intonation_salience) {
    
    std::lock_guard<std::recursive_mutex> lock(prosodic_pattern_mutex_);
    
    // Adaptive threshold based on recent experience
    if (intonation_attention_state_.intonation_history.size() > 10) {
        // Calculate mean of recent intonation salience
        float recent_mean = 0.0f;
        for (auto it = intonation_attention_state_.intonation_history.end() - 10;
             it != intonation_attention_state_.intonation_history.end(); ++it) {
            recent_mean += *it;
        }
        recent_mean /= 10.0f;
        
        // Adapt threshold towards recent mean with some offset
        float target_threshold = recent_mean + 0.1f; // Slightly above mean
        float adaptation_rate = config_.prosodic_attention_adaptation_rate;
        
        intonation_attention_state_.adaptive_threshold = 
            intonation_attention_state_.adaptive_threshold * (1.0f - adaptation_rate) +
            target_threshold * adaptation_rate;
        
        // Update prosodic attention threshold
        intonation_attention_state_.prosodic_attention_threshold = 
            intonation_attention_state_.adaptive_threshold;
    }
}

void LanguageSystem::processIntonationGuidedLearning(
    const std::string& vocalization, const AcousticFeatures& acoustic_features) {
    
    // Extract patterns from vocalization
    std::vector<std::string> patterns = extractPatternsFromVocalization(vocalization);
    
    // Calculate intonation learning boost
    float learning_boost = calculateIntonationLearningBoost(acoustic_features);
    
    // Apply boost to proto-words if intonation attention is active
    if (intonation_attention_state_.is_prosodic_attention_active) {
        std::lock_guard<std::recursive_mutex> proto_lock(proto_word_mutex_);
        
        for (const std::string& pattern : patterns) {
            auto it = proto_word_lookup_.find(pattern);
            if (it != proto_word_lookup_.end()) {
                ProtoWord& proto_word = proto_words_[it->second];
                
                // Apply intonation-guided learning boost
                proto_word.stability_score += learning_boost * config_.prosodic_pattern_learning_rate;
                proto_word.stability_score = std::min(1.0f, proto_word.stability_score);
                
                // Update occurrence count
                proto_word.occurrence_count++;
                proto_word.last_occurrence = std::chrono::steady_clock::now();
                
                logDevelopmentalEvent("Intonation-guided proto-word boost", pattern);
            }
        }
    }
}

LanguageSystem::ProsodicPattern 
LanguageSystem::extractProsodicPattern(
    const std::vector<AcousticFeatures>& acoustic_sequence) const {
    
    ProsodicPattern pattern;
    
    if (acoustic_sequence.empty()) return pattern;
    
    // Extract pitch trajectory
    for (const AcousticFeatures& features : acoustic_sequence) {
        pattern.pitch_trajectory.push_back(features.pitch_contour);
        pattern.energy_trajectory.push_back(features.energy_envelope);
        pattern.rhythm_pattern.push_back(features.rhythm_pattern);
    }
    
    // Calculate pattern stability based on consistency
    if (pattern.pitch_trajectory.size() > 1) {
        float pitch_variance = 0.0f;
        float pitch_mean = std::accumulate(pattern.pitch_trajectory.begin(),
                                         pattern.pitch_trajectory.end(), 0.0f) / 
                          pattern.pitch_trajectory.size();
        
        for (float pitch : pattern.pitch_trajectory) {
            pitch_variance += (pitch - pitch_mean) * (pitch - pitch_mean);
        }
        pitch_variance /= pattern.pitch_trajectory.size();
        
        // Lower variance = higher stability
        pattern.pattern_stability = 1.0f / (1.0f + pitch_variance / 100.0f);
    }
    
    return pattern;
}

float LanguageSystem::calculateProsodicPatternSimilarity(
    const ProsodicPattern& pattern1, const ProsodicPattern& pattern2) const {
    
    float similarity = 0.0f;
    float weight_sum = 0.0f;
    
    // Compare pitch trajectories
    if (!pattern1.pitch_trajectory.empty() && !pattern2.pitch_trajectory.empty()) {
        float pitch_similarity = cosineSimilarity(pattern1.pitch_trajectory, pattern2.pitch_trajectory);
        similarity += pitch_similarity * 0.4f;
        weight_sum += 0.4f;
    }
    
    // Compare energy trajectories
    if (!pattern1.energy_trajectory.empty() && !pattern2.energy_trajectory.empty()) {
        float energy_similarity = cosineSimilarity(pattern1.energy_trajectory, pattern2.energy_trajectory);
        similarity += energy_similarity * 0.3f;
        weight_sum += 0.3f;
    }
    
    // Compare rhythm patterns
    if (!pattern1.rhythm_pattern.empty() && !pattern2.rhythm_pattern.empty()) {
        float rhythm_similarity = cosineSimilarity(pattern1.rhythm_pattern, pattern2.rhythm_pattern);
        similarity += rhythm_similarity * 0.3f;
        weight_sum += 0.3f;
    }
    
    return weight_sum > 0.0f ? similarity / weight_sum : 0.0f;
}

void LanguageSystem::updateProsodicPreferences(
    const std::string& pattern_name, float preference_update) {
    
    std::lock_guard<std::recursive_mutex> lock(acoustic_processing_mutex_);
    
    learned_prosodic_preferences_[pattern_name] += preference_update;
    learned_prosodic_preferences_[pattern_name] = 
        std::min(1.0f, learned_prosodic_preferences_[pattern_name]);
}

std::vector<std::string> LanguageSystem::identifyMotheresePatternsInSequence(
    const std::vector<AcousticFeatures>& acoustic_sequence) const {
    
    std::vector<std::string> motherese_patterns;
    
    for (std::size_t i = 0; i < acoustic_sequence.size(); ++i) {
        if (detectMotherese(acoustic_sequence[i])) {
            std::string pattern_name = "motherese_" + std::to_string(i);
            motherese_patterns.push_back(pattern_name);
        }
    }
    
    return motherese_patterns;
}

void LanguageSystem::enhanceProtoWordWithProsodicPattern(
    const std::string& proto_word_pattern, const ProsodicPattern& prosodic_pattern) {
    
    std::lock_guard<std::recursive_mutex> proto_lock(proto_word_mutex_);
    
    auto it = proto_word_lookup_.find(proto_word_pattern);
    if (it != proto_word_lookup_.end()) {
        ProtoWord& proto_word = proto_words_[it->second];
        
        // Enhance proto-word with prosodic information
        float prosodic_boost = prosodic_pattern.learning_boost_factor;
        
        proto_word.stability_score += prosodic_boost * config_.prosodic_pattern_learning_rate;
        proto_word.stability_score = std::min(1.0f, proto_word.stability_score);
        
        // Store prosodic association (simplified - store pattern name)
        if (proto_word.contextual_embeddings.empty()) {
            proto_word.contextual_embeddings = prosodic_pattern.pitch_trajectory;
        } else {
            // Blend prosodic information
            for (std::size_t i = 0; i < std::min(proto_word.contextual_embeddings.size(),
                                               prosodic_pattern.pitch_trajectory.size()); ++i) {
                proto_word.contextual_embeddings[i] = 
                    proto_word.contextual_embeddings[i] * 0.7f + 
                    prosodic_pattern.pitch_trajectory[i] * 0.3f;
            }
        }
        
        proto_word.occurrence_count++;
        proto_word.last_occurrence = std::chrono::steady_clock::now();
    }
}

float LanguageSystem::calculateProsodicBoostForProtoWord(
    const std::string& proto_word_pattern, const AcousticFeatures& acoustic_features) const {
    
    float boost = 0.0f;
    
    // Check if we have learned preferences for this pattern
    std::lock_guard<std::recursive_mutex> lock(acoustic_processing_mutex_);
    auto pref_it = learned_prosodic_preferences_.find(proto_word_pattern);
    if (pref_it != learned_prosodic_preferences_.end()) {
        boost += pref_it->second * 0.3f;
    }
    
    // Apply intonation learning boost
    boost += calculateIntonationLearningBoost(acoustic_features);
    
    // Apply motherese boost if detected
    if (detectMotherese(acoustic_features)) {
        boost += config_.motherese_pattern_boost * 0.2f;
    }
    
    return std::min(1.0f, boost);
}

void LanguageSystem::processProsodicallGuidedBabbling(
    std::size_t num_phonemes, const ProsodicPattern& target_pattern) {
    
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    std::vector<std::string> phoneme_sequence;
    
    for (std::size_t i = 0; i < num_phonemes; ++i) {
        std::string phoneme;
        
        // Generate phoneme with prosodic guidance
        if (target_pattern.is_motherese_pattern) {
            // Bias towards vowels for motherese patterns
            std::vector<std::string> vowel_phonemes = {"a", "e", "i", "o", "u"};
            std::size_t vowel_idx = static_cast<std::size_t>(uniform_dist_(rng_) * vowel_phonemes.size());
            phoneme = vowel_phonemes[vowel_idx];
        } else {
            // Use regular biased phoneme generation
            phoneme = generateBiasedPhoneme(0.3f);
        }
        
        phoneme_sequence.push_back(phoneme);
        
        // Create token with prosodic enhancement
        std::string babble_token = "prosodic_babble_" + phoneme + "_" + 
                                  std::to_string(development_step_counter_.load());
        
        auto it = token_lookup_.find(babble_token);
        if (it == token_lookup_.end()) {
            SymbolicToken token;
            token.symbol = babble_token;
            token.type = TokenType::Phoneme;
            token.activation_strength = uniform_dist_(rng_) + target_pattern.attention_weight;
            token.activation_strength = std::min(1.0f, token.activation_strength);
            token.usage_count = 0;
            token.last_used = std::chrono::steady_clock::now();
            token.embedding = generateRandomEmbedding();
            
            // Store prosodic association
            token.sensory_associations["prosodic_pattern"] = target_pattern.learning_boost_factor;
            
            std::size_t token_id = vocabulary_.size();
            vocabulary_.push_back(std::move(token));
            token_lookup_[babble_token] = token_id;
        }
    }
    
    // Track phoneme sequence patterns with prosodic enhancement
    trackPhonemeSequencePatterns(phoneme_sequence);
    
    stats_.total_tokens_generated += num_phonemes;
}

void LanguageSystem::processEnhancedAcousticFeatures(
    const AcousticFeatures& features) {
    
    std::lock_guard<std::recursive_mutex> lock(acoustic_processing_mutex_);
    
    // Process prosodic pattern learning
    processProsodicPatternLearning(features);
    
    // Track acoustic pattern evolution
    trackAcousticPatternEvolution(features);
    
    // Update acoustic attention weights
    updateAcousticAttentionWeights(features);
    
    // Update prosodic attention history
    prosodic_attention_history_.push_back(features.attention_score);
    if (prosodic_attention_history_.size() > 100) {
        prosodic_attention_history_.erase(prosodic_attention_history_.begin());
    }
}

void LanguageSystem::trackAcousticPatternEvolution(
    const AcousticFeatures& features) {
    
    // Track how acoustic patterns evolve over time
    // This could be used for developmental analysis
    
    // Calculate novelty of current features
    float novelty = calculateAcousticNovelty(features);
    
    // Log significant acoustic events
    if (novelty > 0.7f) {
        logDevelopmentalEvent("High acoustic novelty detected", 
                            "novelty=" + std::to_string(novelty));
    }
    
    // Track prosodic development
    if (features.intonation_slope > 0.2f) {
        logDevelopmentalEvent("Strong rising intonation", 
                            "slope=" + std::to_string(features.intonation_slope));
    }
}

float LanguageSystem::calculateAcousticNovelty(
    const AcousticFeatures& features) const {
    
    std::lock_guard<std::recursive_mutex> lock(acoustic_processing_mutex_);
    
    if (recent_acoustic_features_.size() < 5) return 1.0f; // Everything is novel initially
    
    // Compare with recent acoustic features
    float total_similarity = 0.0f;
    std::size_t comparison_count = 0;
    
    for (auto it = recent_acoustic_features_.end() - 5; 
         it != recent_acoustic_features_.end(); ++it) {
        
        // Simple similarity based on key features
        float pitch_sim = 1.0f - std::abs(features.pitch_contour - it->pitch_contour) / 400.0f;
        float energy_sim = 1.0f - std::abs(features.energy_envelope - it->energy_envelope);
        float intonation_sim = 1.0f - std::abs(features.intonation_slope - it->intonation_slope) / 2.0f;
        
        float similarity = (pitch_sim + energy_sim + intonation_sim) / 3.0f;
        total_similarity += std::max(0.0f, similarity);
        comparison_count++;
    }
    
    float average_similarity = total_similarity / comparison_count;
    return 1.0f - average_similarity; // Novelty is inverse of similarity
}

void LanguageSystem::updateAcousticAttentionWeights(
    const AcousticFeatures& features) {
    
    // Update attention weights based on acoustic features
    float attention_weight = calculateSoundSalience(features);
    
    // Store in prosodic attention history
    std::lock_guard<std::recursive_mutex> lock(acoustic_processing_mutex_);
    prosodic_attention_history_.push_back(attention_weight);
    
    // Update learned preferences based on attention
    if (attention_weight > 0.6f) {
        // This is an attention-grabbing acoustic pattern
        std::string pattern_key = "high_attention_" + std::to_string(static_cast<int>(features.pitch_contour));
        updateProsodicPreferences(pattern_key, 0.1f);
    }
}

void LanguageSystem::performAcousticBabbling(std::size_t num_phonemes) {
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    std::lock_guard<std::recursive_mutex> acoustic_lock(acoustic_mutex_);
    
    for (std::size_t i = 0; i < num_phonemes; ++i) {
        // Generate acoustic features for babbling
        AcousticFeatures features;
        
        // Random pitch and formant values for babbling
        features.pitch_contour = uniform_dist_(rng_) * 200.0f + 100.0f; // 100-300 Hz
        features.formant_f1 = uniform_dist_(rng_) * 500.0f + 300.0f;    // 300-800 Hz
        features.formant_f2 = uniform_dist_(rng_) * 1000.0f + 800.0f;   // 800-1800 Hz
        features.energy_envelope = uniform_dist_(rng_) * 0.5f + 0.3f;   // 0.3-0.8
        features.attention_score = uniform_dist_(rng_);
        
        // Create phoneme cluster from acoustic features
        PhonemeCluster cluster;
        cluster.phonetic_symbol = "ph_" + std::to_string(i) + "_" + 
                                 std::to_string(development_step_counter_.load());
        cluster.acoustic_profile = features;
        cluster.usage_frequency = 1;
        cluster.last_reinforced = std::chrono::steady_clock::now();
        
        // Check if similar phoneme already exists
        auto it = token_lookup_.find(cluster.phonetic_symbol);
        if (it == token_lookup_.end()) {
            // Create new symbolic token for this phoneme
            SymbolicToken token;
            token.symbol = cluster.phonetic_symbol;
            token.type = TokenType::Phoneme;
            token.activation_strength = features.attention_score;
            token.usage_count = 1;
            token.last_used = std::chrono::steady_clock::now();
            token.embedding = generateRandomEmbedding();
            
            std::size_t token_id = vocabulary_.size();
            vocabulary_.push_back(std::move(token));
            token_lookup_[cluster.phonetic_symbol] = token_id;
            
            // Store phoneme cluster
            phoneme_clusters_.push_back(cluster);
        } else {
            // Reinforce existing token with new acoustic experience
            vocabulary_[it->second].activation_strength += features.attention_score * 0.1f;
            vocabulary_[it->second].activation_strength = (vocabulary_[it->second].activation_strength < 1.0f) ? vocabulary_[it->second].activation_strength : 1.0f;
        }
    }
    
    // Update statistics to track token generation
    stats_.total_tokens_generated += num_phonemes;
}

} // namespace Core
} // namespace NeuroForge
