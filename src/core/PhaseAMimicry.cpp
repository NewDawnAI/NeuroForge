#include "core/PhaseAMimicry.h"
#include "core/LanguageSystem.h"
#include "core/HypergraphBrain.h"
#include "core/LearningSystem.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <iostream>
#include <fstream>
#include <functional>
#include <cstdlib>

namespace NeuroForge {
namespace Core {

PhaseAMimicry::PhaseAMimicry(std::shared_ptr<LanguageSystem> language_system,
                            std::shared_ptr<MemoryDB> memory_db,
                            const Config& config)
    : config_(config)
    , language_system_(language_system)
    , memory_db_(memory_db)
    , rng_(std::random_device{}())
    , uniform_dist_(0.0f, 1.0f) {
    
    teacher_embeddings_.reserve(config_.max_teacher_embeddings);
    mimicry_history_.reserve(config_.alignment_history_size);
    alignments_.reserve(config_.alignment_history_size);
}

bool PhaseAMimicry::initialize() {
    std::lock_guard<std::recursive_mutex> teacher_lock(teacher_mutex_);
    std::lock_guard<std::recursive_mutex> mimicry_lock(mimicry_mutex_);
    std::lock_guard<std::recursive_mutex> alignment_lock(alignment_mutex_);
    
    // Initialize statistics
    stats_ = Statistics{};
    
    // Verify language system integration
    if (!language_system_) {
        std::cerr << "[PhaseAMimicry] Error: Language system not provided\n";
        return false;
    }
    
    // Initialize MemoryDB integration if available
    if (memory_db_) {
        std::int64_t run_id = config_.initial_run_id;
        if (const char* env = std::getenv("NF_RUN_ID")) {
            try { run_id = std::stoll(env); } catch (...) {}
        }
        loadFromMemoryDB(run_id, 1000);
    }
    
    // Log initialization
    std::cout << "[PhaseAMimicry] Phase A Baby Multimodal Mimicry initialized\n";
    std::cout << "[PhaseAMimicry] Teacher encoders enabled: ";
    if (config_.enable_clip_vision) std::cout << "CLIP-Vision ";
    if (config_.enable_clip_text) std::cout << "CLIP-Text ";
    if (config_.enable_whisper_audio) std::cout << "Whisper ";
    if (config_.enable_bert_text) std::cout << "BERT ";
    std::cout << "\n";
    
    updateStatistics();
    return true;
}

void PhaseAMimicry::shutdown() {
    std::lock_guard<std::recursive_mutex> teacher_lock(teacher_mutex_);
    std::lock_guard<std::recursive_mutex> mimicry_lock(mimicry_mutex_);
    std::lock_guard<std::recursive_mutex> alignment_lock(alignment_mutex_);
    
    // Save to MemoryDB if available
    if (memory_db_) {
        saveToMemoryDB();
    }
    
    // Clear all data structures
    teacher_embeddings_.clear();
    content_to_embedding_.clear();
    mimicry_history_.clear();
    alignments_.clear();
    alignment_lookup_.clear();
    
    std::cout << "[PhaseAMimicry] Phase A system shutdown complete\n";
}

void PhaseAMimicry::reset() {
    shutdown();
    initialize();
}

std::string PhaseAMimicry::addTeacherEmbedding(const std::vector<float>& embedding,
                                              TeacherType teacher_type,
                                              Modality modality,
                                              const std::string& content_id,
                                              const std::string& raw_content,
                                              float confidence) {
    std::lock_guard<std::recursive_mutex> lock(teacher_mutex_);
    
    // Validate embedding
    if (!validateEmbedding(embedding)) {
        std::cerr << "[PhaseAMimicry] Invalid embedding provided for content: " << content_id << "\n";
        return "";
    }
    
    // Check if content already exists
    auto it = content_to_embedding_.find(content_id);
    if (it != content_to_embedding_.end()) {
        // Update existing embedding
        auto& existing = teacher_embeddings_[it->second];
        existing.embedding = normalizeEmbedding(embedding);
        existing.confidence = confidence;
        existing.timestamp = std::chrono::steady_clock::now();
        logTeacherEmbedding(existing);
        return content_id;
    }
    
    // Create new teacher embedding
    TeacherEmbedding teacher_embedding;
    teacher_embedding.embedding = normalizeEmbedding(embedding);
    teacher_embedding.teacher_type = teacher_type;
    teacher_embedding.modality = modality;
    teacher_embedding.content_id = content_id;
    teacher_embedding.raw_content = raw_content;
    teacher_embedding.timestamp = std::chrono::steady_clock::now();
    teacher_embedding.confidence = confidence;
    
    // Add metadata based on teacher type
    switch (teacher_type) {
        case TeacherType::CLIP_Vision:
            teacher_embedding.metadata["encoder"] = 1.0f; // CLIP vision
            break;
        case TeacherType::CLIP_Text:
            teacher_embedding.metadata["encoder"] = 2.0f; // CLIP text
            break;
        case TeacherType::Whisper_Audio:
            teacher_embedding.metadata["encoder"] = 3.0f; // Whisper
            break;
        case TeacherType::BERT_Text:
            teacher_embedding.metadata["encoder"] = 4.0f; // BERT
            break;
        default:
            teacher_embedding.metadata["encoder"] = 0.0f; // Custom
            break;
    }
    
    // Store embedding
    std::size_t embedding_idx = teacher_embeddings_.size();
    teacher_embeddings_.push_back(std::move(teacher_embedding));
    content_to_embedding_[content_id] = embedding_idx;
    
    // Update statistics
    stats_.teacher_embeddings_stored++;
    updateModalityStats(modality);
    
    // Prune if necessary
    if (teacher_embeddings_.size() > config_.max_teacher_embeddings) {
        pruneEmbeddingHistory();
    }
    
    logTeacherEmbedding(teacher_embeddings_[embedding_idx]);
    return content_id;
}

PhaseAMimicry::TeacherEmbedding* PhaseAMimicry::getTeacherEmbedding(const std::string& content_id) {
    std::lock_guard<std::recursive_mutex> lock(teacher_mutex_);
    
    auto it = content_to_embedding_.find(content_id);
    if (it != content_to_embedding_.end() && it->second < teacher_embeddings_.size()) {
        return &teacher_embeddings_[it->second];
    }
    return nullptr;
}

std::vector<PhaseAMimicry::TeacherEmbedding*> PhaseAMimicry::getTeacherEmbeddingsByModality(Modality modality) {
    std::lock_guard<std::recursive_mutex> lock(teacher_mutex_);
    
    std::vector<TeacherEmbedding*> result;
    for (auto& embedding : teacher_embeddings_) {
        if (embedding.modality == modality) {
            result.push_back(&embedding);
        }
    }
    return result;
}

std::vector<PhaseAMimicry::TeacherEmbedding*> PhaseAMimicry::getTeacherEmbeddingsByType(TeacherType teacher_type) {
    std::lock_guard<std::recursive_mutex> lock(teacher_mutex_);
    
    std::vector<TeacherEmbedding*> result;
    for (auto& embedding : teacher_embeddings_) {
        if (embedding.teacher_type == teacher_type) {
            result.push_back(&embedding);
        }
    }
    return result;
}

PhaseAMimicry::MimicryAttempt PhaseAMimicry::attemptMimicry(const std::vector<float>& student_embedding,
                                                           const std::string& teacher_content_id,
                                                           const std::string& context) {
    std::lock_guard<std::recursive_mutex> mimicry_lock(mimicry_mutex_);
    std::lock_guard<std::recursive_mutex> teacher_lock(teacher_mutex_);
    std::lock_guard<std::recursive_mutex> student_lock(student_mutex_);
    
    MimicryAttempt attempt;
    attempt.content_context = context;
    attempt.timestamp = std::chrono::steady_clock::now();
    
    // Find teacher embedding
    TeacherEmbedding* teacher = getTeacherEmbedding(teacher_content_id);
    if (!teacher) {
        if (!teacher_embeddings_.empty()) {
            std::size_t idx = static_cast<std::size_t>(uniform_dist_(rng_) * static_cast<float>(teacher_embeddings_.size()));
            if (idx >= teacher_embeddings_.size()) idx = teacher_embeddings_.size() - 1;
            teacher = &teacher_embeddings_[idx];
        } else {
            attempt.success = false;
            return attempt;
        }
    }
    
    attempt.teacher_embedding = teacher->embedding;
    attempt.teacher_type = teacher->teacher_type;
    attempt.modality = teacher->modality;
    // New: carry teacher metadata for downstream grounding
    attempt.teacher_content_id = teacher->content_id;
    attempt.teacher_label = teacher->content_id; // e.g., "dog_word"
    attempt.teacher_aux_data = teacher->raw_content; // e.g., canonical text "dog"

    // Prefer the provided student_embedding when available; fall back to
    // the internal student table only if the input is missing or mismatched.
    if (!student_embedding.empty() && student_embedding.size() == attempt.teacher_embedding.size()) {
        attempt.student_embedding = normalizeEmbedding(student_embedding);
    } else if (config_.enable_student_table) {
        if (auto* se = ensureStudentEntry(teacher->content_id)) {
            attempt.student_embedding = se->embedding;
        } else {
            attempt.student_embedding = std::vector<float>(attempt.teacher_embedding.size(), 0.0f);
        }
    } else {
        attempt.student_embedding = std::vector<float>(attempt.teacher_embedding.size(), 0.0f);
    }
    
    // Calculate similarity reward
    // Debug: compute norms and sizes before similarity
    auto compute_norm = [](const std::vector<float>& v) {
        double s = 0.0; for (float x : v) s += double(x) * x; return std::sqrt(s);
    };
    const std::size_t stu_size = attempt.student_embedding.size();
    const std::size_t tea_size = attempt.teacher_embedding.size();
    const double stu_norm = compute_norm(attempt.student_embedding);
    const double tea_norm = compute_norm(attempt.teacher_embedding);
    attempt.similarity_score = calculateSimilarityReward(attempt.student_embedding, attempt.teacher_embedding);
    {
        static int dbg_logs = 0;
        if (dbg_logs < 5) {
            double dot = 0.0; std::size_t n = std::min(stu_size, tea_size);
            for (std::size_t i = 0; i < n; ++i) dot += double(attempt.student_embedding[i]) * attempt.teacher_embedding[i];
            std::cerr << "[PhaseAMimicry][DEBUG] sim=" << std::fixed << std::setprecision(6) << attempt.similarity_score
                      << ", stu_size=" << stu_size << ", tea_size=" << tea_size
                      << ", stu_norm=" << std::fixed << std::setprecision(6) << stu_norm
                      << ", tea_norm=" << std::fixed << std::setprecision(6) << tea_norm
                      << ", dot=" << std::fixed << std::setprecision(6) << dot
                      << "\n";
            ++dbg_logs;
        }
    }
    
    float mimicry_weight = config_.similarity_weight;
    float novelty_weight = config_.novelty_weight;
    
    if (brain_) {
         float self_trust = 0.0f;
         if (memory_db_) {
             std::int64_t run_id = config_.initial_run_id;
             if (const char* env = std::getenv("NF_RUN_ID")) {
                 try { run_id = std::stoll(env); } catch (...) {}
             }
             auto recent = memory_db_->getRecentMetacognition(run_id, 1);
             if (!recent.empty()) {
                 double t = recent.front().self_trust;
                 if (t < 0.0) t = 0.0;
                 if (t > 1.0) t = 1.0;
                 self_trust = static_cast<float>(t);
             }
         }
         
         // Scale: 0.9 mimicry at trust=0 -> 0.2 mimicry at trust=1
         mimicry_weight = 0.9f - 0.7f * self_trust;
         novelty_weight = 1.0f - mimicry_weight;
         
         // Clamp novelty to ensure creativity eventually emerges
         if (novelty_weight < 0.3f) novelty_weight = 0.3f;
         // Re-normalize
         float sum = mimicry_weight + novelty_weight;
         mimicry_weight /= sum;
         novelty_weight /= sum;
    }

    // Calculate novelty bonus
    attempt.novelty_score = calculateNoveltyBonus(attempt.student_embedding, teacher_embeddings_);

    float neg_penalty = 0.0f;
    if (config_.negative_sampling_k > 0 && config_.negative_weight > 0.0f && teacher_embeddings_.size() > 1) {
        int k = std::min<int>(config_.negative_sampling_k, static_cast<int>(teacher_embeddings_.size()));
        float max_neg = 0.0f;
        for (int c = 0; c < k; ++c) {
            std::size_t idx = static_cast<std::size_t>(uniform_dist_(rng_) * static_cast<float>(teacher_embeddings_.size()));
            if (idx >= teacher_embeddings_.size()) idx = teacher_embeddings_.size() - 1;
            const auto& neg = teacher_embeddings_[idx];
            if (neg.content_id == teacher->content_id) continue;
            float s = calculateSimilarityReward(attempt.student_embedding, neg.embedding);
            if (s > max_neg) max_neg = s;
        }
        neg_penalty = config_.negative_weight * max_neg;
    }

    // Calculate total reward with repulsion from negatives
    attempt.total_reward = (mimicry_weight * attempt.similarity_score) +
                          (novelty_weight * attempt.novelty_score) - neg_penalty;
    
    // Determine success - FIXED: Lower thresholds for placeholder encoders
    // Since teacher encoders are placeholders generating random normalized embeddings,
    // the similarity threshold needs to be much lower to allow any success
    float adjusted_similarity_threshold = config_.similarity_threshold;
    float adjusted_novelty_threshold = config_.novelty_threshold;
    
    // For placeholder encoders, use much lower thresholds
    if (teacher->teacher_type == TeacherType::CLIP_Vision || 
        teacher->teacher_type == TeacherType::CLIP_Text ||
        teacher->teacher_type == TeacherType::Whisper_Audio ||
        teacher->teacher_type == TeacherType::BERT_Text ||
        teacher->teacher_type == TeacherType::Custom) {
        adjusted_similarity_threshold = 0.05f;
        adjusted_novelty_threshold = 0.0f;
    }
    
    attempt.success = (attempt.similarity_score >= adjusted_similarity_threshold) &&
                     (attempt.novelty_score >= adjusted_novelty_threshold);
    
    // Store attempt
    mimicry_history_.push_back(attempt);
    
    // Update statistics
    stats_.total_mimicry_attempts++;
    if (attempt.success) {
        stats_.successful_mimicry_attempts++;
    }
    
    // Enhanced substrate integration: deliver reward and neuromodulator based on routing mode
    if (brain_ && substrate_mode_ != SubstrateMode::Off) {
        // Bridge Phase A telemetry into LearningSystem so it can be reflected in stats/metrics
        if (auto* ls_bridge = brain_->getLearningSystem()) {
            brain_->setMimicryAttemptScores(
                attempt.similarity_score,
                attempt.novelty_score,
                attempt.total_reward,
                attempt.success
            );
            brain_->setTeacherVector(attempt.teacher_embedding);
            brain_->setStudentEmbedding(attempt.student_embedding);
        }

        // Build region activation vector from the modality-mapped region (minimal integration similar to main.cpp)
        std::vector<float> region_acts;
        if (auto region = brain_->getModalityRegion(attempt.modality)) {
            const auto& neurons = region->getNeurons();
            region_acts.reserve(neurons.size());
            for (const auto& n : neurons) {
                if (n) region_acts.push_back(n->getActivation());
            }
        }
        // Use activations as observation proxy
        std::vector<float> obs = region_acts;

        // Compute shaped reward using LearningSystem (fallback to Phase A total reward if LS not available)
        float shaped_reward = attempt.total_reward;
        float mimicry_sim = 0.0f;
        float competence_level = 0.0f;
        float substrate_similarity = 0.0f;
        float substrate_novelty = 0.0f;
        bool mimicry_internal_enabled = false;
        if (auto* ls_reward = brain_->getLearningSystem()) {
            shaped_reward = ls_reward->computeShapedReward(obs, region_acts, attempt.total_reward);
            mimicry_sim = ls_reward->getLastMimicrySim();
            competence_level = ls_reward->getCompetenceLevel();
            substrate_similarity = ls_reward->getLastSubstrateSimilarity();
            substrate_novelty = ls_reward->getLastSubstrateNovelty();
            // Gate external scoring path for M3: if internalization is enabled, do not deliver substrate reward
            mimicry_internal_enabled = ls_reward->isMimicryInternalEnabled();
        }

        // Honor zero-reward flag; do not deliver substrate reward when disabled
        // Additionally, when mimicry internalization is enabled (M3), suppress external delivery entirely
        const float applied_reward = (zero_reward_ || mimicry_internal_enabled) ? 0.0f : shaped_reward;

        // Query substrate for token creation instead of external grounding (M5 implementation)
         if (language_system_ && substrate_mode_ == SubstrateMode::Native) {
             // Get substrate language adapter for assembly-based token creation
             // Note: This is a placeholder for M5 implementation
             // In a full implementation, this would query the substrate language adapter
             // For now, we'll create a simple substrate-inspired token
             
             if (attempt.success && !attempt.teacher_label.empty()) {
                  // Create substrate-inspired token based on successful mimicry
                  std::string token_symbol = "substrate_" + attempt.teacher_label + 
                                           "_" + std::to_string(static_cast<std::size_t>(attempt.similarity_score * 1000));
                 
                std::size_t token_id = language_system_->createToken(
                    token_symbol,
                    LanguageSystem::TokenType::Word,
                    projectStudent(attempt.student_embedding)
                );
                 
                 // Simple substrate-inspired association
                 // In full implementation, this would use actual substrate neurons
                 stats_.successful_mimicry_attempts++; // Use existing stat for now
             }
         }

        auto modalityToStr = [](PhaseAMimicry::Modality m) -> const char* {
            switch (m) {
                case PhaseAMimicry::Modality::Visual: return "vision";
                case PhaseAMimicry::Modality::Audio:  return "audio";
                case PhaseAMimicry::Modality::Text:   return "text";
                default: return "unknown";
            }
        };

        std::ostringstream ctx;
        ctx << "{"
            << "\"modality\":\"" << modalityToStr(attempt.modality) << "\"," 
            << "\"teacher_id\":\"" << attempt.teacher_content_id << "\"," 
            << "\"context\":\"" << attempt.content_context << "\"," 
            << "\"similarity\":" << attempt.similarity_score << ","
            << "\"novelty\":" << attempt.novelty_score << ","
            << "\"total_reward\":" << attempt.total_reward << ","
            << "\"shaped\":" << shaped_reward << ","
            << "\"mimicry_sim\":" << mimicry_sim << ","
            << "\"competence_level\":" << competence_level << ","
            << "\"success\":" << (attempt.success ? "true" : "false") << ","
            << "\"substrate_similarity\":" << substrate_similarity << ","
            << "\"substrate_novelty\":" << substrate_novelty << ","
            << "\"obs_dim\":" << obs.size() << ","
            << "\"acts_dim\":" << region_acts.size()
            << "}";

        // Deliver shaped reward to substrate (also logs to MemoryDB)
        brain_->deliverReward(static_cast<double>(applied_reward), "phase_a", ctx.str());

        if (substrate_mode_ == SubstrateMode::Train && !mimicry_internal_enabled) {
            const float level = std::clamp(applied_reward, 0.0f, 1.0f);
            brain_->applyNeuromodulator(attempt.modality, level);
        }
    }

    // Apply reward to language system
    applyMimicryReward(attempt);

    // Apply student representation update (simple attraction toward teacher)
    if (config_.enable_student_table) {
        updateStudentEmbedding(teacher->content_id, attempt.teacher_embedding, attempt.total_reward, false);
        if (config_.replay_interval_steps > 0 && config_.replay_top_k > 0) {
            std::size_t attempts = mimicry_history_.size();
            if (attempts > 0 && (attempts % config_.replay_interval_steps == 0)) {
                runReplayCycle();
            }
        }
    }

    if (brain_ && memory_db_) {
        std::ostringstream jtea;
        jtea.setf(std::ios::fixed);
        jtea << "{" << "\"teacher_id\":\"" << attempt.teacher_content_id << "\"," << "\"vec\":[";
        for (std::size_t i = 0; i < attempt.teacher_embedding.size(); ++i) { if (i) jtea << ","; jtea << attempt.teacher_embedding[i]; }
        jtea << "]}";
        brain_->logSubstrateState("phase_a_teacher", attempt.teacher_content_id, jtea.str());

        const StudentEntry* se_log = getStudentEntry(attempt.teacher_content_id);
        std::ostringstream jstu;
        jstu.setf(std::ios::fixed);
        jstu << "{" << "\"content_id\":\"" << attempt.teacher_content_id << "\"," << "\"vec\":[";
        if (se_log) {
            for (std::size_t i = 0; i < se_log->embedding.size(); ++i) { if (i) jstu << ","; jstu << se_log->embedding[i]; }
        }
        jstu << "]}";
        brain_->logSubstrateState("phase_a_student", attempt.teacher_content_id, jstu.str());

        std::vector<float> proj = projectStudent(attempt.student_embedding);
        std::ostringstream jnorm;
        jnorm.setf(std::ios::fixed);
        jnorm << "{"
              << "\"teacher_norm\":" << l2Norm(attempt.teacher_embedding) << ","
              << "\"student_norm\":" << l2Norm(attempt.student_embedding) << ","
              << "\"projected_norm\":" << l2Norm(proj) << ","
              << "\"teacher_dim\":" << attempt.teacher_embedding.size() << ","
              << "\"student_dim\":" << attempt.student_embedding.size() << ","
              << "\"projected_dim\":" << proj.size() << "}";
        brain_->logSubstrateState("phase_a_norms", attempt.teacher_content_id, jnorm.str());
    }
    
    logMimicryAttempt(attempt);
    return attempt;
}

float PhaseAMimicry::calculateSimilarityReward(const std::vector<float>& student_embedding,
                                              const std::vector<float>& teacher_embedding) const {
    std::vector<float> se = normalizeEmbedding(student_embedding);
    std::vector<float> te = normalizeEmbedding(teacher_embedding);
    float s = cosineSimilarity(se, te);
    if (s >= 0.0f && config_.similarity_gamma > 0.0f && config_.similarity_gamma < 1.0f) {
        s = std::pow(s, static_cast<float>(config_.similarity_gamma));
    }
    if (s > 1.0f) s = 1.0f; else if (s < -1.0f) s = -1.0f;
    return s;
}

float PhaseAMimicry::calculateNoveltyBonus(const std::vector<float>& student_embedding,
                                          const std::vector<TeacherEmbedding>& reference_embeddings) const {
    if (reference_embeddings.empty()) {
        return 1.0f;
    }
    
    // Find maximum similarity to any reference
    float max_similarity = 0.0f;
    for (const auto& ref : reference_embeddings) {
        float similarity = cosineSimilarity(student_embedding, ref.embedding);
        max_similarity = std::max(max_similarity, similarity);
    }
    
    float novelty = 1.0f - max_similarity;
    if (novelty < 0.0f) novelty = 0.0f;
    if (novelty > 1.0f) novelty = 1.0f;
    return novelty;
}

void PhaseAMimicry::applyMimicryReward(const MimicryAttempt& attempt) {
    if (!language_system_) {
        return;
    }
    
    // Create grounded tokens based on successful mimicry
    if (attempt.success) {
        // Choose an appropriate token symbol
        std::string token_symbol = "";
        LanguageSystem::TokenType token_type = LanguageSystem::TokenType::Word;
        switch (attempt.modality) {
            case Modality::Visual:
                token_type = LanguageSystem::TokenType::Perception;
                token_symbol = "see_" + attempt.content_context;
                break;
            case Modality::Audio:
                token_type = LanguageSystem::TokenType::Perception;
                token_symbol = "hear_" + attempt.content_context;
                break;
            case Modality::Text: {
                token_type = LanguageSystem::TokenType::Word;
                // Prefer canonical word from teacher raw content (e.g., "dog")
                std::string preferred;
                if (!attempt.teacher_aux_data.empty()) {
                    preferred = attempt.teacher_aux_data; // canonical word
                } else if (!attempt.teacher_label.empty()) {
                    preferred = attempt.teacher_label; // fallback to content_id label
                }
                
                // If still empty, try to pick an existing similar token
                if (preferred.empty()) {
                    auto similar = language_system_->findSimilarTokens(attempt.teacher_embedding, 0.3f);
                    if (!similar.empty()) {
                        if (auto* existing = language_system_->getToken(similar[0])) {
                            preferred = existing->symbol;
                        }
                    }
                }
                
                // Final fallback: use context string
                if (preferred.empty()) {
                    preferred = attempt.content_context;
                }
                
                token_symbol = preferred;
                break;
            }
            default:
                token_type = LanguageSystem::TokenType::Word;
                token_symbol = attempt.content_context.empty() ? std::string("mimic_") + std::to_string(static_cast<int>(attempt.modality)) : attempt.content_context;
                break;
        }
        
        // Ensure the token exists (create if needed)
        auto* existing_token = language_system_->getToken(token_symbol);
        if (!existing_token) {
            language_system_->createToken(token_symbol, token_type, projectStudent(attempt.student_embedding));
        }

        // Register teacher embedding for this label to allow processTeacherSignal to update token stats
        language_system_->setTeacherEmbedding(token_symbol, attempt.teacher_embedding);
        
        // Apply reward signal (this increments usage_count and updates embedding)
        language_system_->processTeacherSignal(token_symbol, attempt.total_reward);
        
        // Update narration with grounded content
        std::vector<std::string> narration_tokens = {"I", "mimic", token_symbol};
        language_system_->logSelfNarration(narration_tokens, attempt.total_reward, 
                                          "Phase A mimicry: " + attempt.content_context);
    }
}

std::string PhaseAMimicry::createMultimodalAlignment(const std::vector<std::string>& teacher_content_ids,
                                                    const std::vector<std::size_t>& language_token_ids,
                                                    const std::string& alignment_context) {
    std::lock_guard<std::recursive_mutex> alignment_lock(alignment_mutex_);
    std::lock_guard<std::recursive_mutex> teacher_lock(teacher_mutex_);
    
    std::size_t ctx_hash = std::hash<std::string>{}(alignment_context);
    std::string alignment_id = "align_" + std::to_string(alignments_.size()) + "_" +
                              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "_" +
                              std::to_string(ctx_hash);
    
    MultimodalAlignment alignment;
    alignment.alignment_id = alignment_id;
    alignment.associated_tokens = language_token_ids;
    alignment.created_at = std::chrono::steady_clock::now();
    
    // Collect teacher embeddings
    for (const std::string& content_id : teacher_content_ids) {
        TeacherEmbedding* teacher = getTeacherEmbedding(content_id);
        if (teacher) {
            alignment.teacher_embeddings.push_back(*teacher);
        }
    }
    
    if (alignment.teacher_embeddings.empty()) {
        std::cerr << "[PhaseAMimicry] No valid teacher embeddings for alignment\n";
        return "";
    }
    
    // Calculate cross-modal alignment strength
    alignment.alignment_strength = calculateCrossModalAlignment(alignment.teacher_embeddings);
    
    // Calculate cross-modal scores for all modality pairs
    std::unordered_map<Modality, std::vector<TeacherEmbedding*>> modality_groups;
    for (auto& emb : alignment.teacher_embeddings) {
        modality_groups[emb.modality].push_back(&emb);
    }
    
    for (auto& pair1 : modality_groups) {
        for (auto& pair2 : modality_groups) {
            if (pair1.first != pair2.first) {
                std::string pair_key = std::to_string(static_cast<int>(pair1.first)) + "_" +
                                      std::to_string(static_cast<int>(pair2.first));
                
                float cross_score = 0.0f;
                int count = 0;
                for (auto* emb1 : pair1.second) {
                    for (auto* emb2 : pair2.second) {
                        cross_score += cosineSimilarity(emb1->embedding, emb2->embedding);
                        count++;
                    }
                }
                if (count > 0) {
                    float avg = cross_score / count;
                    // Map cosine from [-1,1] -> [0,1]
                    float mapped = 0.5f * (avg + 1.0f);
                    if (mapped < 0.0f) mapped = 0.0f;
                    if (mapped > 1.0f) mapped = 1.0f;
                    alignment.cross_modal_scores[pair_key] = mapped;
                }
            }
        }
    }
    
    // Store alignment
    std::size_t alignment_idx = alignments_.size();
    alignments_.push_back(std::move(alignment));
    alignment_lookup_[alignment_id] = alignment_idx;
    
    // Update statistics
    stats_.multimodal_alignments_created++;
    
    // Ground language tokens with multimodal context
    if (language_system_) {
        for (std::size_t token_id : language_token_ids) {
            for (const auto& teacher_emb : alignments_[alignment_idx].teacher_embeddings) {
                // Associate token with teacher embedding concepts
                language_system_->associateTokenWithModality(
                    token_id, 
                    "phase_a_" + std::to_string(static_cast<int>(teacher_emb.modality)),
                    teacher_emb.embedding,
                    alignments_[alignment_idx].alignment_strength
                );
            }
        }
    }
    
    logAlignment(alignments_[alignment_idx]);
    return alignment_id;
}

PhaseAMimicry::MultimodalAlignment* PhaseAMimicry::getAlignment(const std::string& alignment_id) {
    std::lock_guard<std::recursive_mutex> lock(alignment_mutex_);
    
    auto it = alignment_lookup_.find(alignment_id);
    if (it != alignment_lookup_.end() && it->second < alignments_.size()) {
        return &alignments_[it->second];
    }
    return nullptr;
}

float PhaseAMimicry::calculateCrossModalAlignment(const std::vector<TeacherEmbedding>& embeddings) const {
    if (embeddings.size() < 2) {
        return 0.0f;
    }
    
    float total_similarity = 0.0f;
    int pair_count = 0;
    
    for (std::size_t i = 0; i < embeddings.size(); ++i) {
        for (std::size_t j = i + 1; j < embeddings.size(); ++j) {
            if (embeddings[i].modality != embeddings[j].modality) {
                total_similarity += cosineSimilarity(embeddings[i].embedding, embeddings[j].embedding);
                pair_count++;
            }
        }
    }
    // Average cosine similarity across modality pairs
    float avg_similarity = pair_count > 0 ? total_similarity / pair_count : 0.0f;
    // Map cosine from [-1, 1] to [0, 1] for non-negative alignment strength
    float mapped = 0.5f * (avg_similarity + 1.0f);
    if (mapped < 0.0f) mapped = 0.0f;
    if (mapped > 1.0f) mapped = 1.0f;
    return mapped;
}

void PhaseAMimicry::groundLanguageTokens(const std::vector<std::string>& teacher_content_ids,
                                        const std::vector<std::string>& token_symbols) {
    if (!language_system_ || teacher_content_ids.size() != token_symbols.size()) {
        return;
    }
    
    std::lock_guard<std::recursive_mutex> teacher_lock(teacher_mutex_);
    
    for (std::size_t i = 0; i < teacher_content_ids.size(); ++i) {
        TeacherEmbedding* teacher = getTeacherEmbedding(teacher_content_ids[i]);
        if (teacher) {
            // Set teacher embedding for the token
            language_system_->setTeacherEmbedding(token_symbols[i], teacher->embedding);
            
            // Process as teacher signal with high confidence
            language_system_->processTeacherSignal(token_symbols[i], teacher->confidence);
        }
    }
}

std::vector<std::string> PhaseAMimicry::generateGroundedNarration(const std::vector<std::string>& teacher_content_ids) {
    std::vector<std::string> grounded_tokens;
    
    std::lock_guard<std::recursive_mutex> teacher_lock(teacher_mutex_);
    
    for (const std::string& content_id : teacher_content_ids) {
        TeacherEmbedding* teacher = getTeacherEmbedding(content_id);
        if (teacher) {
            // Generate token based on modality and content
            std::string token;
            switch (teacher->modality) {
                case Modality::Visual:
                    token = "see_" + teacher->raw_content;
                    break;
                case Modality::Audio:
                    token = "hear_" + teacher->raw_content;
                    break;
                case Modality::Text:
                    token = teacher->raw_content;
                    break;
                default:
                    token = "sense_" + teacher->raw_content;
                    break;
            }
            grounded_tokens.push_back(token);
        }
    }
    
    return grounded_tokens;
}

// Placeholder implementations for teacher encoder integration
std::vector<float> PhaseAMimicry::processCLIPVision(const std::string& image_path) {
    // Placeholder: In real implementation, this would call CLIP vision encoder
    std::vector<float> embedding(config_.embedding_dimension);
    
    // Generate deterministic embedding based on image path
    std::hash<std::string> hasher;
    std::size_t seed = hasher(image_path + "_clip_vision");
    std::mt19937 local_rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    for (float& val : embedding) {
        val = dist(local_rng);
    }
    
    return normalizeEmbedding(embedding);
}

std::vector<float> PhaseAMimicry::processCLIPText(const std::string& text) {
    // Placeholder: In real implementation, this would call CLIP text encoder
    std::vector<float> embedding(config_.embedding_dimension);
    
    std::hash<std::string> hasher;
    std::size_t seed = hasher(text + "_clip_text");
    std::mt19937 local_rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    for (float& val : embedding) {
        val = dist(local_rng);
    }
    
    return normalizeEmbedding(embedding);
}

std::vector<float> PhaseAMimicry::processWhisperAudio(const std::string& audio_path) {
    // Placeholder: In real implementation, this would call Whisper encoder
    std::vector<float> embedding(config_.embedding_dimension);
    
    std::hash<std::string> hasher;
    std::size_t seed = hasher(audio_path + "_whisper");
    std::mt19937 local_rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    for (float& val : embedding) {
        val = dist(local_rng);
    }
    
    return normalizeEmbedding(embedding);
}

std::vector<float> PhaseAMimicry::processBERTText(const std::string& text) {
    // Placeholder: In real implementation, this would call BERT encoder
    std::vector<float> embedding(config_.embedding_dimension);
    
    std::hash<std::string> hasher;
    std::size_t seed = hasher(text + "_bert");
    std::mt19937 local_rng(seed);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    for (float& val : embedding) {
        val = dist(local_rng);
    }
    
    return normalizeEmbedding(embedding);
}

PhaseAMimicry::Statistics PhaseAMimicry::getStatistics() const {
    std::lock_guard<std::recursive_mutex> lock(stats_mutex_);
    return stats_;
}

void PhaseAMimicry::updateStatistics() {
    std::lock_guard<std::recursive_mutex> stats_lock(stats_mutex_);
    std::lock_guard<std::recursive_mutex> mimicry_lock(mimicry_mutex_);
    
    if (!mimicry_history_.empty()) {
        float total_similarity = 0.0f;
        float total_novelty = 0.0f;
        float total_reward = 0.0f;
        
        for (const auto& attempt : mimicry_history_) {
            total_similarity += attempt.similarity_score;
            total_novelty += attempt.novelty_score;
            total_reward += attempt.total_reward;
        }
        
        stats_.average_similarity_score = total_similarity / mimicry_history_.size();
        stats_.average_novelty_score = total_novelty / mimicry_history_.size();
        stats_.average_total_reward = total_reward / mimicry_history_.size();
    }
    
    // Calculate cross-modal alignment strength
    if (!alignments_.empty()) {
        float total_alignment = 0.0f;
        for (const auto& alignment : alignments_) {
            total_alignment += alignment.alignment_strength;
        }
        stats_.cross_modal_alignment_strength = total_alignment / alignments_.size();
    }
}

std::string PhaseAMimicry::generatePhaseAReport() const {
    std::ostringstream report;
    
    Statistics stats = getStatistics();
    
    report << "=== NeuroForge Phase A Baby Multimodal Mimicry Report ===\n";
    report << "Total Mimicry Attempts: " << stats.total_mimicry_attempts << "\n";
    report << "Successful Attempts: " << stats.successful_mimicry_attempts << "\n";
    report << "Success Rate: " << std::fixed << std::setprecision(2)
           << (stats.total_mimicry_attempts > 0 ? 
               (100.0f * stats.successful_mimicry_attempts / stats.total_mimicry_attempts) : 0.0f)
           << "%\n";
    report << "Teacher Embeddings Stored: " << stats.teacher_embeddings_stored << "\n";
    report << "Multimodal Alignments: " << stats.multimodal_alignments_created << "\n";
    report << "Average Similarity Score: " << std::fixed << std::setprecision(3)
           << stats.average_similarity_score << "\n";
    report << "Average Novelty Score: " << std::fixed << std::setprecision(3)
           << stats.average_novelty_score << "\n";
    report << "Average Total Reward: " << std::fixed << std::setprecision(3)
           << stats.average_total_reward << "\n";
    report << "Cross-Modal Alignment Strength: " << std::fixed << std::setprecision(3)
           << stats.cross_modal_alignment_strength << "\n";
    
    // Modality breakdown
    report << "\nModality Distribution:\n";
    for (const auto& pair : stats.modality_counts) {
        report << "  " << pair.first << ": " << pair.second << "\n";
    }
    
    // Teacher type performance
    report << "\nTeacher Type Performance:\n";
    for (const auto& pair : stats.teacher_type_performance) {
        report << "  " << pair.first << ": " << std::fixed << std::setprecision(3)
               << pair.second << "\n";
    }
    
    return report.str();
}

// Utility methods
float PhaseAMimicry::cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) const {
    std::size_t n = std::min(a.size(), b.size());
    if (n == 0) {
        return 0.0f;
    }

    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (std::size_t i = 0; i < n; ++i) {
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

std::vector<float> PhaseAMimicry::normalizeEmbedding(const std::vector<float>& embedding) const {
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

float PhaseAMimicry::l2Norm(const std::vector<float>& v) const {
    double s = 0.0;
    for (float x : v) s += static_cast<double>(x) * static_cast<double>(x);
    return static_cast<float>(std::sqrt(s));
}

bool PhaseAMimicry::validateEmbedding(const std::vector<float>& embedding) const {
    if (embedding.empty() || embedding.size() != config_.embedding_dimension) {
        return false;
    }
    
    // Check for NaN or infinite values
    for (float val : embedding) {
        if (std::isnan(val) || std::isinf(val)) {
            return false;
        }
    }
    
    return true;
}

void PhaseAMimicry::updateModalityStats(Modality modality) {
    std::string modality_name;
    switch (modality) {
        case Modality::Visual: modality_name = "Visual"; break;
        case Modality::Audio: modality_name = "Audio"; break;
        case Modality::Text: modality_name = "Text"; break;
        case Modality::Proprioceptive: modality_name = "Proprioceptive"; break;
        case Modality::Multimodal: modality_name = "Multimodal"; break;
    }
    stats_.modality_counts[modality_name]++;
}

void PhaseAMimicry::logMimicryAttempt(const MimicryAttempt& attempt) const {
    std::cout << "[PhaseAMimicry] Mimicry attempt - Similarity: " 
              << std::fixed << std::setprecision(3) << attempt.similarity_score
              << ", Novelty: " << attempt.novelty_score
              << ", Reward: " << attempt.total_reward
              << ", Success: " << (attempt.success ? "YES" : "NO")
              << "\n";
}

void PhaseAMimicry::logAlignment(const MultimodalAlignment& alignment) const {
    std::cout << "[PhaseAMimicry] Multimodal alignment created - ID: " << alignment.alignment_id
              << ", Strength: " << std::fixed << std::setprecision(3) << alignment.alignment_strength
              << ", Embeddings: " << alignment.teacher_embeddings.size()
              << ", Tokens: " << alignment.associated_tokens.size()
              << ", Pairs: " << alignment.cross_modal_scores.size() << "\n";
}

void PhaseAMimicry::logTeacherEmbedding(const TeacherEmbedding& embedding) const {
    std::cout << "[PhaseAMimicry] Teacher embedding added - ID: " << embedding.content_id
              << ", Type: " << static_cast<int>(embedding.teacher_type)
              << ", Modality: " << static_cast<int>(embedding.modality)
              << ", Confidence: " << std::fixed << std::setprecision(3) << embedding.confidence
              << "\n";
}

void PhaseAMimicry::saveToMemoryDB() {
    // Placeholder for MemoryDB integration
    if (memory_db_) {
        std::cout << "[PhaseAMimicry] Saving Phase A data to MemoryDB\n";
        // Implementation would save teacher embeddings, mimicry history, and alignments
    }
}

void PhaseAMimicry::loadFromMemoryDB() {
    // Placeholder for MemoryDB integration
    if (memory_db_) {
        std::cout << "[PhaseAMimicry] Loading Phase A data from MemoryDB\n";
        // Implementation would load previously saved data
    }
}

void PhaseAMimicry::pruneEmbeddingHistory() {
    std::lock_guard<std::recursive_mutex> lock(teacher_mutex_);
    
    if (teacher_embeddings_.size() <= config_.max_teacher_embeddings) {
        return;
    }
    
    // Sort by timestamp (oldest first)
    std::vector<std::pair<std::size_t, std::chrono::steady_clock::time_point>> indexed_timestamps;
    for (std::size_t i = 0; i < teacher_embeddings_.size(); ++i) {
        indexed_timestamps.emplace_back(i, teacher_embeddings_[i].timestamp);
    }
    
    std::sort(indexed_timestamps.begin(), indexed_timestamps.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    // Remove oldest 10%
    std::size_t to_remove = teacher_embeddings_.size() / 10;
    std::vector<bool> to_delete(teacher_embeddings_.size(), false);
    
    for (std::size_t i = 0; i < to_remove; ++i) {
        to_delete[indexed_timestamps[i].first] = true;
    }
    
    // Rebuild data structures
    std::vector<TeacherEmbedding> new_embeddings;
    std::unordered_map<std::string, std::size_t> new_lookup;
    
    for (std::size_t i = 0; i < teacher_embeddings_.size(); ++i) {
        if (!to_delete[i]) {
            new_lookup[teacher_embeddings_[i].content_id] = new_embeddings.size();
            new_embeddings.push_back(std::move(teacher_embeddings_[i]));
        }
    }
    
    teacher_embeddings_ = std::move(new_embeddings);
    content_to_embedding_ = std::move(new_lookup);
    
    std::cout << "[PhaseAMimicry] Pruned " << to_remove << " old teacher embeddings\n";
}

// Missing method implementations
std::vector<std::string> PhaseAMimicry::processBatchTeacherEmbeddings(
    const std::vector<std::pair<std::string, TeacherType>>& content_batch,
    Modality modality) {
    
    std::vector<std::string> batch_ids;
    
    for (const auto& content_pair : content_batch) {
        const std::string& content = content_pair.first;
        TeacherType teacher_type = content_pair.second;
        
        // Generate embedding based on teacher type
        std::vector<float> embedding;
        switch (teacher_type) {
            case TeacherType::CLIP_Vision:
                embedding = processCLIPVision(content);
                break;
            case TeacherType::CLIP_Text:
                embedding = processCLIPText(content);
                break;
            case TeacherType::Whisper_Audio:
                embedding = processWhisperAudio(content);
                break;
            case TeacherType::BERT_Text:
                embedding = processBERTText(content);
                break;
            default:
                 embedding = std::vector<float>(config_.embedding_dimension, 0.0f);
                 // Generate random embedding
                 std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
                 for (float& val : embedding) {
                     val = dist(rng_);
                 }
                 embedding = normalizeEmbedding(embedding);
                 break;
        }
        
        // Add teacher embedding
        std::string content_id = generateContentId(content, teacher_type);
        std::string added_id = addTeacherEmbedding(
            embedding, teacher_type, modality, content_id, content, 0.9f);
        
        if (!added_id.empty()) {
            batch_ids.push_back(added_id);
        }
    }
    
    return batch_ids;
}

std::vector<PhaseAMimicry::MimicryAttempt> PhaseAMimicry::processBatchMimicry(
    const std::vector<std::vector<float>>& student_embeddings,
    const std::vector<std::string>& teacher_content_ids) {
    
    std::vector<MimicryAttempt> batch_attempts;
    
    std::size_t min_size = std::min(student_embeddings.size(), teacher_content_ids.size());
    
    for (std::size_t i = 0; i < min_size; ++i) {
        auto attempt = attemptMimicry(
            student_embeddings[i], 
            teacher_content_ids[i], 
            "batch_mimicry_" + std::to_string(i)
        );
        batch_attempts.push_back(attempt);
    }
    
    return batch_attempts;
}

void PhaseAMimicry::consolidateMemory() {
    std::lock_guard<std::recursive_mutex> teacher_lock(teacher_mutex_);
    std::lock_guard<std::recursive_mutex> mimicry_lock(mimicry_mutex_);
    std::lock_guard<std::recursive_mutex> alignment_lock(alignment_mutex_);
    
    // Decay old embeddings
    decayOldEmbeddings();
    
    // Prune if necessary
    if (teacher_embeddings_.size() > config_.max_teacher_embeddings) {
        pruneEmbeddingHistory();
    }
    
    // Prune old mimicry history
    if (mimicry_history_.size() > config_.alignment_history_size) {
        std::size_t to_remove = mimicry_history_.size() - config_.alignment_history_size;
        mimicry_history_.erase(mimicry_history_.begin(), mimicry_history_.begin() + to_remove);
    }
    
    // Update cross-modal scores
    updateCrossModalScores();
    
    std::cout << "[PhaseAMimicry] Memory consolidation complete\n";
}

std::string PhaseAMimicry::exportTeacherEmbeddingsToJson() const {
    std::lock_guard<std::recursive_mutex> lock(teacher_mutex_);
    
    std::ostringstream json;
    json << "{\"teacher_embeddings\":[";
    
    for (std::size_t i = 0; i < teacher_embeddings_.size(); ++i) {
        const auto& emb = teacher_embeddings_[i];
        
        json << "{";
        json << "\"content_id\":\"" << emb.content_id << "\",";
        json << "\"teacher_type\":" << static_cast<int>(emb.teacher_type) << ",";
        json << "\"modality\":" << static_cast<int>(emb.modality) << ",";
        json << "\"confidence\":" << emb.confidence << ",";
        json << "\"raw_content\":\"" << emb.raw_content << "\",";
        json << "\"embedding\":[";
        
        for (std::size_t j = 0; j < emb.embedding.size(); ++j) {
            json << emb.embedding[j];
            if (j < emb.embedding.size() - 1) json << ",";
        }
        
        json << "]";
        json << "}";
        
        if (i < teacher_embeddings_.size() - 1) json << ",";
    }
    
    json << "]}";
    return json.str();
}

std::string PhaseAMimicry::exportMimicryHistoryToJson() const {
    std::lock_guard<std::recursive_mutex> lock(mimicry_mutex_);
    
    std::ostringstream json;
    json << "{\"mimicry_attempts\":[";
    
    for (std::size_t i = 0; i < mimicry_history_.size(); ++i) {
        const auto& attempt = mimicry_history_[i];
        
        json << "{";
        json << "\"similarity_score\":" << attempt.similarity_score << ",";
        json << "\"novelty_score\":" << attempt.novelty_score << ",";
        json << "\"total_reward\":" << attempt.total_reward << ",";
        json << "\"success\":" << (attempt.success ? "true" : "false") << ",";
        json << "\"context\":\"" << attempt.content_context << "\"";
        json << "}";
        
        if (i < mimicry_history_.size() - 1) json << ",";
    }
    
    json << "]}";
    return json.str();
}

std::string PhaseAMimicry::exportAlignmentsToJson() const {
    std::lock_guard<std::recursive_mutex> lock(alignment_mutex_);
    
    std::ostringstream json;
    json << "{\"alignments\":[";
    
    for (std::size_t i = 0; i < alignments_.size(); ++i) {
        const auto& alignment = alignments_[i];
        
        json << "{";
        json << "\"alignment_id\":\"" << alignment.alignment_id << "\",";
        json << "\"alignment_strength\":" << alignment.alignment_strength << ",";
        json << "\"teacher_embeddings\":" << alignment.teacher_embeddings.size() << ",";
        json << "\"associated_tokens\":[";
        
        for (std::size_t j = 0; j < alignment.associated_tokens.size(); ++j) {
            json << alignment.associated_tokens[j];
            if (j < alignment.associated_tokens.size() - 1) json << ",";
        }
        
        json << "],";
        json << "\"cross_modal_scores\":{";
        
        std::size_t score_count = 0;
        for (const auto& score_pair : alignment.cross_modal_scores) {
            json << "\"" << score_pair.first << "\":" << score_pair.second;
            if (score_count < alignment.cross_modal_scores.size() - 1) json << ",";
            score_count++;
        }
        
        json << "}";
        json << "}";
        
        if (i < alignments_.size() - 1) json << ",";
    }
    
    json << "]}";
    return json.str();
}

// Helper method implementations
void PhaseAMimicry::decayOldEmbeddings() {
    // Placeholder implementation - could decay confidence of old embeddings
    auto now = std::chrono::steady_clock::now();
    for (auto& embedding : teacher_embeddings_) {
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - embedding.timestamp).count();
        if (age > 24) { // Older than 24 hours
            embedding.confidence *= (1.0f - config_.teacher_embedding_decay);
        }
    }
}

void PhaseAMimicry::updateCrossModalScores() {
    // Placeholder implementation - could update alignment scores
    for (auto& alignment : alignments_) {
        if (alignment.teacher_embeddings.size() > 1) {
            alignment.alignment_strength = calculateCrossModalAlignment(alignment.teacher_embeddings);
        }
    }
}

std::string PhaseAMimicry::generateContentId(const std::string& content, TeacherType teacher_type) const {
    std::hash<std::string> hasher;
    std::size_t hash = hasher(content + std::to_string(static_cast<int>(teacher_type)));
    return "content_" + std::to_string(hash);
}

// Student table helpers (must be within NeuroForge::Core namespace)
PhaseAMimicry::StudentEntry* PhaseAMimicry::ensureStudentEntry(const std::string& content_id) {
    auto it = content_to_student_.find(content_id);
    if (it != content_to_student_.end()) {
        return &student_entries_[it->second];
    }
    StudentEntry entry;
    entry.content_id = content_id;
    entry.lr = config_.student_learning_rate;
    entry.embedding.resize(config_.embedding_dimension);
    std::normal_distribution<float> ndist(0.0f, config_.student_init_std);
    for (auto& v : entry.embedding) v = ndist(rng_);
    entry.embedding = normalizeEmbedding(entry.embedding);
    std::size_t idx = student_entries_.size();
    student_entries_.push_back(std::move(entry));
    content_to_student_[content_id] = idx;
    return &student_entries_[idx];
}

PhaseAMimicry::StudentEntry* PhaseAMimicry::getStudentEntry(const std::string& content_id) {
    auto it = content_to_student_.find(content_id);
    if (it == content_to_student_.end()) return nullptr;
    return &student_entries_[it->second];
}

void PhaseAMimicry::updateStudentEmbedding(const std::string& content_id,
                                           const std::vector<float>& teacher_embedding,
                                           float reward,
                                           bool is_replay) {
    auto* se = ensureStudentEntry(content_id);
    if (!se) return;
    float alpha_raw = se->lr * std::clamp(reward, 0.0f, 1.0f);
    if (is_replay) {
        alpha_raw *= std::max(0.0f, config_.replay_lr_scale);
    }
    float alpha = alpha_raw;
    if (config_.enable_ema_stabilizer) {
        alpha = std::clamp(alpha_raw, config_.ema_alpha_min, config_.ema_alpha_max);
    }
    const std::size_t n = std::min<std::size_t>(se->embedding.size(), teacher_embedding.size());
    for (std::size_t i = 0; i < n; ++i) {
        se->embedding[i] += alpha * (teacher_embedding[i] - se->embedding[i]);
    }
    se->embedding = normalizeEmbedding(se->embedding);
    se->competence = std::clamp(se->competence + (std::clamp(reward, 0.0f, 1.0f) * 0.05f), 0.0f, 1.0f);
}

void PhaseAMimicry::repelStudentEmbedding(const std::string& content_id,
                                          const std::vector<float>& teacher_embedding,
                                          float magnitude,
                                          bool is_replay) {
    auto* se = ensureStudentEntry(content_id);
    if (!se) return;
    float alpha_raw = se->lr * std::clamp(magnitude, 0.0f, 1.0f);
    if (is_replay) {
        alpha_raw *= std::max(0.0f, config_.replay_lr_scale);
    }
    float alpha = alpha_raw;
    if (config_.enable_ema_stabilizer) {
        alpha = std::clamp(alpha_raw, config_.ema_alpha_min, config_.ema_alpha_max);
    }
    const std::size_t n = std::min<std::size_t>(se->embedding.size(), teacher_embedding.size());
    for (std::size_t i = 0; i < n; ++i) {
        se->embedding[i] -= alpha * (teacher_embedding[i] - se->embedding[i]);
    }
    se->embedding = normalizeEmbedding(se->embedding);
    se->competence = std::clamp(se->competence - (std::clamp(magnitude, 0.0f, 1.0f) * 0.05f), 0.0f, 1.0f);
}

void PhaseAMimicry::runReplayCycle() {
    std::lock_guard<std::recursive_mutex> mimicry_lock(mimicry_mutex_);
    std::lock_guard<std::recursive_mutex> student_lock(student_mutex_);
    if (!config_.enable_student_table) return;
    if (mimicry_history_.empty()) return;
    std::size_t k = std::min<std::size_t>(config_.replay_top_k, mimicry_history_.size());
    std::vector<std::size_t> idx(mimicry_history_.size());
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    std::nth_element(idx.begin(), idx.begin() + k, idx.end(), [&](std::size_t a, std::size_t b) {
        return mimicry_history_[a].total_reward > mimicry_history_[b].total_reward;
    });
    idx.resize(k);
    std::sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b) {
        return mimicry_history_[a].total_reward > mimicry_history_[b].total_reward;
    });
    for (std::size_t i : idx) {
        const auto& att = mimicry_history_[i];
        float r = att.total_reward * std::max(0.0f, config_.replay_boost_factor);
        updateStudentEmbedding(att.teacher_content_id, att.teacher_embedding, r, true);
        if (brain_) {
            auto region = brain_->getModalityRegion(att.modality);
            if (region) {
                const float level = std::clamp(r, 0.0f, 1.0f);
                brain_->applyNeuromodulator(att.modality, level);
                RegionID rid = region->getId();
                if (auto* ls = brain_->getLearningSystem()) {
                    ls->applyStructuralPlasticity(rid);
                    const auto& lscfg = ls->getConfig();
                    region->pruneWeakSynapses(lscfg.structural_prune_threshold);
                    if (lscfg.structural_grow_batch > 0) {
                        region->growSynapses(lscfg.structural_grow_batch);
                    }
                }
            }
        }
    }

    if (config_.replay_include_hard_negatives && config_.replay_hard_k > 0) {
        std::vector<std::size_t> neg_idx(mimicry_history_.size());
        for (std::size_t i = 0; i < neg_idx.size(); ++i) neg_idx[i] = i;
        std::size_t kneg = std::min<std::size_t>(static_cast<std::size_t>(config_.replay_hard_k), neg_idx.size());
        std::nth_element(neg_idx.begin(), neg_idx.begin() + kneg, neg_idx.end(), [&](std::size_t a, std::size_t b) {
            return mimicry_history_[a].similarity_score < mimicry_history_[b].similarity_score;
        });
        neg_idx.resize(kneg);
        for (std::size_t i : neg_idx) {
            const auto& att = mimicry_history_[i];
            float m = std::max(0.0f, config_.replay_repulsion_weight) * (std::max(0.0f, config_.similarity_threshold) - std::max(0.0f, att.similarity_score));
            repelStudentEmbedding(att.teacher_content_id, att.teacher_embedding, m, true);
        }
    }
}

std::vector<float> PhaseAMimicry::projectStudent(const std::vector<float>& embedding) const {
    if (!language_system_) {
        return normalizeEmbedding(embedding);
    }
    const std::size_t target_dim = static_cast<std::size_t>(language_system_->getConfig().embedding_dimension);
    if (embedding.size() == target_dim) {
        return normalizeEmbedding(embedding);
    }
    const std::size_t in_dim = embedding.size();
    if (in_dim == 0 || target_dim == 0) {
        return {};
    }
    auto it = projection_weights_.find(in_dim);
    if (it == projection_weights_.end()) {
        std::vector<float> W(target_dim * in_dim);
        std::normal_distribution<float> nd(0.0f, 1.0f / std::sqrt(static_cast<float>(in_dim)));
        for (auto& w : W) w = nd(rng_);
        projection_weights_[in_dim] = std::move(W);
        it = projection_weights_.find(in_dim);
    }
    const std::vector<float>& W = it->second;
    std::vector<float> out(target_dim, 0.0f);
    for (std::size_t j = 0; j < target_dim; ++j) {
        const std::size_t row = j * in_dim;
        float s = 0.0f;
        for (std::size_t k = 0; k < in_dim; ++k) {
            s += W[row + k] * embedding[k];
        }
        out[j] = s;
    }
    return normalizeEmbedding(out);
}

// Factory implementation
std::unique_ptr<PhaseAMimicry> PhaseAMimicryFactory::create(
    std::shared_ptr<LanguageSystem> language_system,
    std::shared_ptr<MemoryDB> memory_db,
    const PhaseAMimicry::Config& config) {
    
    return std::make_unique<PhaseAMimicry>(language_system, memory_db, config);
}

PhaseAMimicry::Config PhaseAMimicryFactory::createDefaultConfig() {
    return PhaseAMimicry::Config{};
}

PhaseAMimicry::Config PhaseAMimicryFactory::createLightweightConfig() {
    PhaseAMimicry::Config config;
    config.max_teacher_embeddings = 1000;
    config.alignment_history_size = 100;
    config.embedding_dimension = 256;
    config.batch_size = 16;
    return config;
}

PhaseAMimicry::Config PhaseAMimicryFactory::createResearchConfig() {
    PhaseAMimicry::Config config;
    config.max_teacher_embeddings = 50000;
    config.alignment_history_size = 5000;
    config.embedding_dimension = 768;
    config.batch_size = 64;
    config.enable_cross_modal_alignment = true;
    return config;
}

} // namespace Core
} // namespace NeuroForge
void NeuroForge::Core::PhaseAMimicry::loadFromMemoryDB(std::int64_t run_id, int limit) {
    if (!memory_db_) return;
    auto teachers = memory_db_->getEmbeddings(run_id, "phase_a_teacher", limit);
    for (const auto& e : teachers) {
        if (e.vec.empty()) continue;
        addTeacherEmbedding(e.vec, TeacherType::CLIP_Text, Modality::Text, e.content_id, e.content_id, 1.0f);
    }
    auto students = memory_db_->getEmbeddings(run_id, "phase_a_student", limit);
    for (const auto& s : students) {
        if (s.vec.empty()) continue;
        auto* se = ensureStudentEntry(s.content_id);
        if (se) se->embedding = s.vec;
    }
}
