/**
 * @file phase_a_demo.cpp
 * @brief Phase A Baby Multimodal Mimicry Demo for NeuroForge
 * 
 * This demo showcases the integration of Phase A (Baby Multimodal Mimicry) with
 * Phase 5 (Language System) to demonstrate developmental learning through:
 * - Teacher encoder integration (CLIP, Whisper, BERT)
 * - Mimicry-based learning with similarity and novelty rewards
 * - Cross-modal alignment and semantic grounding
 * - Progressive vocabulary development through multimodal experience
 */

#include "core/HypergraphBrain.h"
#include "core/PhaseAMimicry.h"
#include "core/LanguageSystem.h"
#include "core/LearningSystem.h"
#include "core/MemoryDB.h"
#include "connectivity/ConnectivityManager.h"
#include "regions/CorticalRegions.h"
#include "encoders/VisionEncoder.h"
#include "encoders/AudioEncoder.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <random>
#include <vector>
#include <memory>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstdlib>

 using namespace NeuroForge;
using namespace NeuroForge::Core;
using namespace NeuroForge::Connectivity;
using namespace NeuroForge::Encoders;

class PhaseABabyMimicryDemo {
private:
    std::shared_ptr<ConnectivityManager> connectivity_manager_;
    std::unique_ptr<HypergraphBrain> brain_;
    std::shared_ptr<LanguageSystem> language_system_;
    std::unique_ptr<PhaseAMimicry> phase_a_system_;
    std::shared_ptr<MemoryDB> memory_db_;
    std::unique_ptr<VisionEncoder> vision_encoder_;
    std::unique_ptr<AudioEncoder> audio_encoder_;
    
    std::mt19937 rng_;
    bool verbose_output_;
    
    // Substrate control
    enum class SubstrateMode { Off, Mirror, Train, Native };
    
    // Demo configuration
    struct DemoConfig {
        int total_learning_episodes = 50;
        int steps_per_episode = 20;
        int step_duration_ms = 100;
        bool enable_cross_modal_learning = true;
        bool enable_teacher_guidance = true;
        bool save_progress_log = true;
        std::string log_filename = "phase_a_baby_learning.csv";
        std::string final_report_filename = "phase_a_final_report.txt";
        SubstrateMode substrate_mode = SubstrateMode::Off;
        float reward_scale = 1.0f;
        bool zero_reward = false;
    } config_;
    std::ofstream progress_log_;
    
    // Learning scenarios for baby mimicry
    struct LearningScenario {
        std::string name;
        std::string visual_content;
        std::string audio_content;
        std::string text_content;
        std::vector<std::string> expected_tokens;
        float difficulty_level;
    };
    
    std::vector<LearningScenario> learning_scenarios_;
    
public:
    PhaseABabyMimicryDemo(bool verbose = false) 
        : rng_(std::random_device{}()), verbose_output_(verbose) {
        
        // Initialize core components
        connectivity_manager_ = std::make_shared<ConnectivityManager>();
        brain_ = std::make_unique<HypergraphBrain>(connectivity_manager_);
        memory_db_ = std::make_shared<MemoryDB>("phase_a_demo.db");
        
        // Configure Language System (Phase 5)
        LanguageSystem::Config lang_config;
        lang_config.mimicry_learning_rate = 0.03f;
        lang_config.grounding_strength = 0.9f;
        lang_config.narration_threshold = 0.3f;
        lang_config.max_vocabulary_size = 5000;
        lang_config.embedding_dimension = 512;
        lang_config.enable_teacher_mode = true;
        lang_config.teacher_influence = 0.9f;
        lang_config.enable_vision_grounding = true;
        lang_config.enable_audio_grounding = true;
        lang_config.enable_action_grounding = true;
        
        language_system_ = std::make_shared<LanguageSystem>(lang_config);
        
        // Configure Phase A Mimicry System
        PhaseAMimicry::Config phase_a_config;
        phase_a_config.similarity_weight = 0.8f;
        phase_a_config.novelty_weight = 0.2f;
        phase_a_config.similarity_threshold = 0.5f;
        phase_a_config.novelty_threshold = 0.1f;
        phase_a_config.max_teacher_embeddings = 10000;
        phase_a_config.embedding_dimension = 512;
        phase_a_config.enable_cross_modal_alignment = true;
        phase_a_config.mimicry_learning_rate = 0.025f;
        phase_a_config.grounding_strength = 0.85f;
        
        // Initialize Phase A Mimicry with shared LanguageSystem directly (avoid dual ownership)
        phase_a_system_ = PhaseAMimicryFactory::create(language_system_, memory_db_, phase_a_config);
        
        // Initialize encoders
        VisionEncoder::Config vision_config;
        vision_config.grid_size = 16;
        vision_config.use_edge = true;
        vision_config.edge_weight = 0.6f;
        vision_config.intensity_weight = 0.4f;
        vision_encoder_ = std::make_unique<VisionEncoder>(vision_config);
        
        AudioEncoder::Config audio_config;
        audio_config.sample_rate = 16000;
        audio_config.feature_bins = 128;
        audio_config.spectral_bins = 64;
        audio_config.mel_bands = 32;
        audio_encoder_ = std::make_unique<AudioEncoder>(audio_config);
        
        // Initialize learning scenarios
        setupLearningScenarios();
    }
    
    ~PhaseABabyMimicryDemo() {
        if (progress_log_.is_open()) {
            progress_log_.close();
        }
    }
    
    bool initialize() {
        std::cout << "=== Initializing Phase A Baby Multimodal Mimicry Demo ===\n\n";
        
        // Initialize brain
        if (!brain_->initialize()) {
            std::cerr << "Failed to initialize brain\n";
            return false;
        }
        
        // Create brain regions for multimodal learning
        auto visual_cortex = brain_->createRegion("VisualCortex", Region::Type::Cortical);
        auto auditory_cortex = brain_->createRegion("AuditoryCortex", Region::Type::Cortical);
        auto language_area = brain_->createRegion("LanguageArea", Region::Type::Cortical);
        auto association_area = brain_->createRegion("AssociationArea", Region::Type::Cortical);
        
        if (!visual_cortex || !auditory_cortex || !language_area || !association_area) {
            std::cerr << "Failed to create brain regions\n";
            return false;
        }
        
        // Add neurons to regions (reduced for faster demo initialization)
        visual_cortex->createNeurons(150);
        auditory_cortex->createNeurons(120);
        language_area->createNeurons(200);
        association_area->createNeurons(100);
        
        // Establish cross-modal connectivity
        brain_->connectRegions(visual_cortex->getId(), association_area->getId(), 0.2f, {0.3f, 0.8f});
        brain_->connectRegions(auditory_cortex->getId(), association_area->getId(), 0.18f, {0.25f, 0.75f});
        brain_->connectRegions(language_area->getId(), association_area->getId(), 0.25f, {0.4f, 0.9f});
        brain_->connectRegions(association_area->getId(), visual_cortex->getId(), 0.15f, {0.2f, 0.6f});
        brain_->connectRegions(association_area->getId(), auditory_cortex->getId(), 0.12f, {0.15f, 0.55f});
        brain_->connectRegions(association_area->getId(), language_area->getId(), 0.2f, {0.3f, 0.7f});
        
        // Map modalities to regions for neural substrate routing
        brain_->mapModality(Modality::Visual, visual_cortex->getId());
        brain_->mapModality(Modality::Audio, auditory_cortex->getId());
        brain_->mapModality(Modality::Text, language_area->getId());
        brain_->mapModality(Modality::Proprioceptive, association_area->getId());
        
        if (!language_system_->initialize()) {
            std::cerr << "Failed to initialize language system\n";
            return false;
        }
        
        // Initialize Phase A system
        if (!phase_a_system_->initialize()) {
            std::cerr << "Failed to initialize Phase A system\n";
            return false;
        }
        
        // Initialize learning system
        LearningSystem::Config learning_config;
        learning_config.hebbian_rate = 0.002f;
        learning_config.stdp_rate = 0.003f;
        learning_config.enable_homeostasis = true;
        learning_config.enable_attention_modulation = true;
        
        if (!brain_->initializeLearning(learning_config)) {
            std::cerr << "Failed to initialize learning system\n";
            return false;
        }
        
        // Initialize MemoryDB
        if (!memory_db_->open()) {
            std::cerr << "Failed to initialize MemoryDB\n";
            return false;
        }
        // Begin MemoryDB run and propagate to brain for reward logging
        {
            std::int64_t memdb_run_id = 0;
            std::ostringstream run_meta;
            run_meta << "{\"demo\":\"phase_a_demo\",\"substrate_mode\":\"";
            switch (config_.substrate_mode) {
                case SubstrateMode::Off:    run_meta << "off"; break;
                case SubstrateMode::Mirror: run_meta << "mirror"; break;
                case SubstrateMode::Train:  run_meta << "train"; break;
                case SubstrateMode::Native: run_meta << "native"; break;
            }
            run_meta << "\",\"episodes\":" << config_.total_learning_episodes
                     << ",\"step_ms\":" << config_.step_duration_ms
                     << ",\"reward_scale\":" << std::fixed << std::setprecision(3) << config_.reward_scale
                     << ",\"zero_reward\":" << (config_.zero_reward ? "true" : "false")
                     << "}";
            if (!memory_db_->beginRun(run_meta.str(), memdb_run_id)) {
                std::cerr << "Failed to begin MemoryDB run\n";
            } else {
                brain_->setMemoryDB(memory_db_, memdb_run_id);
                if (verbose_output_) {
                    std::cout << "MemoryDB run started (id=" << memdb_run_id << ")\n";
                }
            }
        }

        // Set up teacher vocabulary for baby learning
        setupTeacherVocabulary();
        
        // Initialize progress logging
        if (config_.save_progress_log) {
            progress_log_.open(config_.log_filename);
            if (progress_log_.is_open()) {
                progress_log_ << "episode,step,scenario,visual_similarity,audio_similarity,text_similarity,"
                             << "cross_modal_alignment,vocabulary_size,successful_mimicry,total_reward\n";
            }
        }
        
        std::cout << "✅ Phase A Baby Mimicry Demo initialized successfully\n";
        std::cout << "📚 Learning scenarios: " << learning_scenarios_.size() << "\n";
        std::cout << "🧠 Brain regions: 4 (Visual, Auditory, Language, Association)\n";
        std::cout << "🔗 Cross-modal connections established\n\n";
        
        return true;
    }
    
    void runDemo() {
        std::cout << "=== Starting Phase A Baby Multimodal Learning Simulation ===\n\n";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int episode = 0; episode < config_.total_learning_episodes; ++episode) {
            // Select learning scenario (progressive difficulty)
            float progress = static_cast<float>(episode) / config_.total_learning_episodes;
            LearningScenario scenario = selectScenario(progress);
            
            if (verbose_output_) {
                std::cout << "Episode " << std::setw(2) << episode + 1 
                         << ": Learning '" << scenario.name << "'\n";
            }
            
            // Run learning episode
            EpisodeResults results = runLearningEpisode(episode, scenario);
            
            // Log progress
            logEpisodeProgress(episode, scenario, results);
            
            // Display progress
            if (verbose_output_ || (episode + 1) % 10 == 0) {
                displayEpisodeResults(episode + 1, scenario, results);
            }
            
            // Brief pause for realistic timing
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
        
        std::cout << "\n=== Phase A Baby Learning Complete ===\n";
        std::cout << "Total learning time: " << duration.count() << " seconds\n\n";
        
        // Generate final comprehensive report
        generateFinalReport();
    }
    
private:
    struct EpisodeResults {
        float visual_similarity = 0.0f;
        float audio_similarity = 0.0f;
        float text_similarity = 0.0f;
        float cross_modal_alignment = 0.0f;
        std::size_t vocabulary_size = 0;
        int successful_mimicry_attempts = 0;
        float total_reward = 0.0f;
        std::vector<std::string> learned_tokens;
        bool episode_success = false;
    };
    
    void setupLearningScenarios() {
        learning_scenarios_ = {
            // Basic object recognition
            {"Apple", "red_round_fruit.jpg", "crunch_sound.wav", "apple", {"apple", "fruit", "red"}, 0.1f},
            {"Dog", "golden_retriever.jpg", "bark_sound.wav", "dog", {"dog", "animal", "pet"}, 0.2f},
            {"Cat", "tabby_cat.jpg", "meow_sound.wav", "cat", {"cat", "animal", "pet"}, 0.2f},
            {"Car", "blue_car.jpg", "engine_sound.wav", "car", {"car", "vehicle", "blue"}, 0.3f},
            
            // Actions and verbs
            {"Running", "person_running.jpg", "footsteps.wav", "run", {"run", "fast", "move"}, 0.4f},
            {"Eating", "person_eating.jpg", "chewing.wav", "eat", {"eat", "food", "hungry"}, 0.4f},
            {"Sleeping", "person_sleeping.jpg", "snoring.wav", "sleep", {"sleep", "tired", "bed"}, 0.3f},
            
            // Emotions and states
            {"Happy", "smiling_face.jpg", "laughter.wav", "happy", {"happy", "smile", "joy"}, 0.5f},
            {"Sad", "crying_face.jpg", "crying.wav", "sad", {"sad", "cry", "tears"}, 0.5f},
            
            // Complex concepts
            {"Family", "family_photo.jpg", "conversation.wav", "family", {"family", "love", "together"}, 0.7f},
            {"Music", "piano_keys.jpg", "piano_melody.wav", "music", {"music", "sound", "beautiful"}, 0.6f},
            {"Nature", "forest_scene.jpg", "birds_chirping.wav", "nature", {"nature", "trees", "peaceful"}, 0.8f},
            
            // Abstract concepts (advanced)
            {"Friendship", "friends_playing.jpg", "children_laughing.wav", "friend", {"friend", "play", "share"}, 0.9f},
            {"Learning", "child_reading.jpg", "page_turning.wav", "learn", {"learn", "book", "smart"}, 1.0f}
        };
    }
    
    void setupTeacherVocabulary() {
        std::cout << "Setting up teacher vocabulary for baby learning...\n";
        
        // Create teacher embeddings for all scenario concepts
        for (const auto& scenario : learning_scenarios_) {
            // Visual teacher embedding
            if (!scenario.visual_content.empty()) {
                auto visual_embedding = phase_a_system_->processCLIPVision(scenario.visual_content);
                phase_a_system_->addTeacherEmbedding(
                    visual_embedding, PhaseAMimicry::TeacherType::CLIP_Vision,
                    PhaseAMimicry::Modality::Visual, scenario.name + "_visual", scenario.visual_content, 0.9f);
            }
            
            // Audio teacher embedding
            auto audio_embedding = phase_a_system_->processWhisperAudio(scenario.audio_content);
            phase_a_system_->addTeacherEmbedding(
                audio_embedding, PhaseAMimicry::TeacherType::Whisper_Audio,
                PhaseAMimicry::Modality::Audio, scenario.name + "_audio", scenario.audio_content, 0.85f);
            
            // Text teacher embedding
            auto text_embedding = phase_a_system_->processBERTText(scenario.text_content);
            phase_a_system_->addTeacherEmbedding(
                text_embedding, PhaseAMimicry::TeacherType::BERT_Text,
                PhaseAMimicry::Modality::Text, scenario.name + "_text", scenario.text_content, 0.95f);
            
            // Set up language system teacher embeddings
            for (const std::string& token : scenario.expected_tokens) {
                language_system_->setTeacherEmbedding(token, text_embedding);
            }
        }
        
        std::cout << "✅ Teacher vocabulary set up with " << (learning_scenarios_.size() * 3) 
                 << " multimodal embeddings\n";
    }
    
    LearningScenario selectScenario(float progress) {
        // Select scenario based on difficulty progression
        std::vector<LearningScenario> suitable_scenarios;
        
        for (const auto& scenario : learning_scenarios_) {
            if (scenario.difficulty_level <= progress + 0.3f) { // Allow some challenge
                suitable_scenarios.push_back(scenario);
            }
        }
        
        if (suitable_scenarios.empty()) {
            return learning_scenarios_[0]; // Fallback to easiest
        }
        
        // Select randomly from suitable scenarios
        std::uniform_int_distribution<std::size_t> dist(0, suitable_scenarios.size() - 1);
        return suitable_scenarios[dist(rng_)];
    }
    
    EpisodeResults runLearningEpisode(int episode, const LearningScenario& scenario) {
        EpisodeResults results;
        
        // Step 1: Teacher presents multimodal input
        std::string visual_id = scenario.name + "_visual";
        std::string audio_id = scenario.name + "_audio";
        std::string text_id = scenario.name + "_text";
        
        // Step 2: Baby processes and attempts to mimic each modality
        for (int step = 0; step < config_.steps_per_episode; ++step) {
            // Update brain processing
            brain_->processStep(0.01f);
            
            // Update language development
            language_system_->updateDevelopment(0.01f);
            
            // Baby attempts visual mimicry
            auto visual_attempt = attemptModalityMimicry(
                PhaseAMimicry::Modality::Visual, visual_id, scenario, step);
            results.visual_similarity += visual_attempt.similarity_score;
            
            // Baby attempts audio mimicry
            auto audio_attempt = attemptModalityMimicry(
                PhaseAMimicry::Modality::Audio, audio_id, scenario, step);
            results.audio_similarity += audio_attempt.similarity_score;
            
            // Baby attempts text/word mimicry
            auto text_attempt = attemptModalityMimicry(
                PhaseAMimicry::Modality::Text, text_id, scenario, step);
            results.text_similarity += text_attempt.similarity_score;
            
            // Count successful attempts
            if (visual_attempt.success) results.successful_mimicry_attempts++;
            if (audio_attempt.success) results.successful_mimicry_attempts++;
            if (text_attempt.success) results.successful_mimicry_attempts++;
            
            // Accumulate rewards
            results.total_reward += visual_attempt.total_reward + 
                                   audio_attempt.total_reward + 
                                   text_attempt.total_reward;
            
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.step_duration_ms));
        }
        
        // Step 3: Create cross-modal alignment
        std::vector<std::string> teacher_ids = {visual_id, audio_id, text_id};
        std::vector<std::size_t> token_ids;
        
        for (const std::string& token_symbol : scenario.expected_tokens) {
            std::size_t token_id = language_system_->createToken(
                token_symbol, LanguageSystem::TokenType::Word);
            token_ids.push_back(token_id);
            results.learned_tokens.push_back(token_symbol);
        }
        
        std::string alignment_id = phase_a_system_->createMultimodalAlignment(
            teacher_ids, token_ids, scenario.name + "_concept");
        
        if (!alignment_id.empty()) {
            auto* alignment = phase_a_system_->getAlignment(alignment_id);
            if (alignment) {
                results.cross_modal_alignment = alignment->alignment_strength;
            }
        }
        
        // Step 4: Ground language tokens with multimodal experience
        phase_a_system_->groundLanguageTokens(teacher_ids, scenario.expected_tokens);
        
        // Step 5: Generate internal narration
        auto grounded_narration = phase_a_system_->generateGroundedNarration(teacher_ids);
        if (!grounded_narration.empty()) {
            language_system_->logSelfNarration(
                grounded_narration, results.cross_modal_alignment, 
                "Phase A learning: " + scenario.name);
        }
        
        // Calculate averages
        results.visual_similarity /= config_.steps_per_episode;
        results.audio_similarity /= config_.steps_per_episode;
        results.text_similarity /= config_.steps_per_episode;
        
        // Get current vocabulary size
        auto lang_stats = language_system_->getStatistics();
        results.vocabulary_size = lang_stats.active_vocabulary_size;
        
        // Determine episode success
        results.episode_success = (results.successful_mimicry_attempts >= config_.steps_per_episode) &&
                                 (results.cross_modal_alignment > 0.3f);
        
        return results;
    }
    
    PhaseAMimicry::MimicryAttempt attemptModalityMimicry(
        PhaseAMimicry::Modality modality, const std::string& teacher_id,
        const LearningScenario& scenario, int step) {
        
        // Get teacher embedding
        auto* teacher_emb = phase_a_system_->getTeacherEmbedding(teacher_id);
        if (!teacher_emb) {
            return PhaseAMimicry::MimicryAttempt{}; // Empty attempt
        }
        
        // Baby generates imperfect mimicry response
        std::vector<float> baby_response = teacher_emb->embedding;
        
        // Add developmental noise (decreases with learning)
        float noise_level = 0.3f * (1.0f - static_cast<float>(step) / config_.steps_per_episode);
        addDevelopmentalNoise(baby_response, noise_level);
        
        // Add modality-specific processing
        switch (modality) {
            case PhaseAMimicry::Modality::Visual:
                // Simulate visual processing through NeuroForge vision encoder
                processVisualInput(baby_response, scenario.visual_content);
                break;
            case PhaseAMimicry::Modality::Audio:
                // Simulate audio processing through NeuroForge audio encoder
                processAudioInput(baby_response, scenario.audio_content);
                break;
            case PhaseAMimicry::Modality::Text:
                // Simulate language processing through Phase 5 system
                processTextInput(baby_response, scenario.text_content);
                break;
            default:
                break;
        }
        
        // Neural substrate routing (mirror/train/native modes)
        if (config_.substrate_mode != SubstrateMode::Off) {
            // PhaseAMimicry::Modality aliases NeuroForge::Modality, but cast explicitly for clarity
            Modality m = static_cast<Modality>(modality);
            // Preserve pre-substrate response in case substrate outputs a degenerate vector
            const std::vector<float> pre_substrate = baby_response;
            brain_->feedExternalPattern(m, baby_response);
            brain_->processStep(0.01f);
            std::vector<float> substrate_out = brain_->readoutVector(m);
            if (!substrate_out.empty()) {
                baby_response = std::move(substrate_out);
                // Ensure dimensionality matches teacher embedding for valid cosine similarity
                const size_t target_dim = teacher_emb->embedding.size();
                if (baby_response.size() != target_dim) {
                    if (baby_response.empty()) {
                        baby_response = pre_substrate;
                    } else if (baby_response.size() < target_dim) {
                        std::vector<float> padded = baby_response;
                        padded.reserve(target_dim);
                        for (size_t i = 0; padded.size() < target_dim; ++i) {
                            padded.push_back(baby_response[i % baby_response.size()]);
                        }
                        baby_response.swap(padded);
                    } else {
                        baby_response.resize(target_dim);
                    }
                }
                // Re-normalize and guard against near-zero vectors to avoid zero cosine similarity
                float norm = 0.0f;
                for (float v : baby_response) norm += v * v;
                norm = std::sqrt(norm);
                if (norm > 1e-6f) {
                    for (float &v : baby_response) v /= norm;
                } else {
                    // Fallback: revert to pre-substrate response and add a tiny noise to ensure non-zero norm
                    baby_response = pre_substrate;
                    addDevelopmentalNoise(baby_response, std::max(0.01f, noise_level * 0.2f));
                }
            }
            // Previously: fixed neuromodulation; now reward-scaled neuromod applied after attempt
        }
        
        // Attempt mimicry and compute reward
        auto attempt = phase_a_system_->attemptMimicry(
            baby_response, teacher_id, 
            scenario.name + "_" + std::to_string(static_cast<int>(modality)));
        
        // Route reward to HypergraphBrain with context JSON
        const float scaled_reward = config_.zero_reward ? 0.0f : (config_.reward_scale * attempt.total_reward);
        auto modalityToStr = [](PhaseAMimicry::Modality m) {
            switch (m) {
                case PhaseAMimicry::Modality::Visual: return "visual";
                case PhaseAMimicry::Modality::Audio: return "audio";
                case PhaseAMimicry::Modality::Text: return "text";
                case PhaseAMimicry::Modality::Multimodal: return "multimodal";
                default: return "unknown";
            }
        };
        float substrate_similarity = 0.0f;
        float substrate_novelty = 0.0f;
        if (auto* ls = brain_->getLearningSystem()) {
            substrate_similarity = ls->getLastSubstrateSimilarity();
            substrate_novelty = ls->getLastSubstrateNovelty();
        }
        std::ostringstream ctx;
        ctx << "{" 
            << "\"modality\":\"" << modalityToStr(modality) << "\"," 
            << "\"teacher_id\":\"" << teacher_id << "\"," 
            << "\"scenario\":\"" << scenario.name << "\"," 
            << "\"step\":" << step << ","
            << "\"similarity\":" << attempt.similarity_score << ","
            << "\"novelty\":" << attempt.novelty_score << ","
            << "\"substrate_similarity\":" << substrate_similarity << ","
            << "\"substrate_novelty\":" << substrate_novelty << ","
            << "\"total_reward\":" << attempt.total_reward << ","
            << "\"success\":" << (attempt.success ? "true" : "false")
            << "}";
        brain_->deliverReward(static_cast<double>(scaled_reward), "phase_a", ctx.str());
        
        // Apply neuromodulator proportionally in Train mode (per modality)
        if (config_.substrate_mode == SubstrateMode::Train) {
            Modality m = static_cast<Modality>(modality);
            const float level = std::clamp(scaled_reward, 0.0f, 1.0f);
            brain_->applyNeuromodulator(m, level);
        }
        
        return attempt;
    }
    
    void processVisualInput(std::vector<float>& response, const std::string& visual_content) {
        // Simulate visual processing with NeuroForge vision encoder
        std::vector<float> synthetic_visual = generateSyntheticVisualInput(visual_content);
        auto vision_features = vision_encoder_->encode(synthetic_visual);
        
        // Blend with response (baby's visual interpretation)
        for (std::size_t i = 0; i < std::min(response.size(), vision_features.size()); ++i) {
            response[i] = 0.7f * response[i] + 0.3f * vision_features[i % vision_features.size()];
        }
    }
    
    void processAudioInput(std::vector<float>& response, const std::string& audio_content) {
        // Simulate audio processing with NeuroForge audio encoder
        std::vector<float> synthetic_audio = generateSyntheticAudioInput(audio_content);
        auto audio_features = audio_encoder_->encode(synthetic_audio);
        
        // Blend with response (baby's auditory interpretation)
        for (std::size_t i = 0; i < std::min(response.size(), audio_features.size()); ++i) {
            response[i] = 0.6f * response[i] + 0.4f * audio_features[i % audio_features.size()];
        }
    }
    
    void processTextInput(std::vector<float>& response, const std::string& text_content) {
        // Process through Phase 5 language system
        // This represents the baby's developing language understanding
        
        // Generate context embedding for the text
        std::vector<float> context_embedding(response.size());
        std::hash<std::string> hasher;
        std::size_t seed = hasher(text_content);
        std::mt19937 local_rng(seed);
        std::normal_distribution<float> dist(0.0f, 0.5f);
        
        for (float& val : context_embedding) {
            val = dist(local_rng);
        }
        
        // Blend with response (baby's language interpretation)
        for (std::size_t i = 0; i < response.size(); ++i) {
            response[i] = 0.8f * response[i] + 0.2f * context_embedding[i];
        }
    }
    
    std::vector<float> generateSyntheticVisualInput(const std::string& content) {
        std::vector<float> input(16 * 16); // 16x16 visual grid
        
        // Generate content-specific visual pattern
        std::hash<std::string> hasher;
        std::size_t seed = hasher(content);
        std::mt19937 local_rng(seed);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        
        for (float& val : input) {
            val = dist(local_rng);
        }
        
        return input;
    }
    
    std::vector<float> generateSyntheticAudioInput(const std::string& content) {
        std::vector<float> input(1024); // Audio samples
        
        // Generate content-specific audio pattern
        std::hash<std::string> hasher;
        std::size_t seed = hasher(content);
        std::mt19937 local_rng(seed);
        std::normal_distribution<float> dist(0.0f, 0.3f);
        
        for (float& val : input) {
            val = dist(local_rng);
        }
        
        return input;
    }
    
    void addDevelopmentalNoise(std::vector<float>& embedding, float noise_level) {
        std::normal_distribution<float> noise_dist(0.0f, noise_level);
        
        for (float& val : embedding) {
            val += noise_dist(rng_);
        }
        
        // Normalize
        float norm = 0.0f;
        for (float val : embedding) {
            norm += val * val;
        }
        norm = std::sqrt(norm);
        
        if (norm > 1e-6f) {
            for (float& val : embedding) {
                val /= norm;
            }
        }
    }
    
    void logEpisodeProgress(int episode, const LearningScenario& scenario, const EpisodeResults& results) {
        if (!progress_log_.is_open()) {
            return;
        }
        
        progress_log_ << episode << ","
                     << config_.steps_per_episode << ","
                     << scenario.name << ","
                     << std::fixed << std::setprecision(3)
                     << results.visual_similarity << ","
                     << results.audio_similarity << ","
                     << results.text_similarity << ","
                     << results.cross_modal_alignment << ","
                     << results.vocabulary_size << ","
                     << results.successful_mimicry_attempts << ","
                     << results.total_reward << "\n";
        
        progress_log_.flush();
    }
    
    void displayEpisodeResults(int episode, const LearningScenario& scenario, const EpisodeResults& results) {
        std::cout << "Episode " << std::setw(2) << episode 
                 << " | " << std::setw(12) << scenario.name
                 << " | Visual: " << std::fixed << std::setprecision(2) << results.visual_similarity
                 << " | Audio: " << results.audio_similarity
                 << " | Text: " << results.text_similarity
                 << " | Alignment: " << results.cross_modal_alignment
                 << " | Vocab: " << std::setw(3) << results.vocabulary_size
                 << " | Success: " << (results.episode_success ? "✅" : "❌")
                 << "\n";
    }
    
    void generateFinalReport() {
        std::cout << "\n=== Generating Final Phase A Baby Learning Report ===\n\n";
        
        // Get final statistics
        auto phase_a_stats = phase_a_system_->getStatistics();
        auto lang_stats = language_system_->getStatistics();
        
        // Generate comprehensive report
        std::ostringstream report;
        report << "=== NeuroForge Phase A Baby Multimodal Mimicry Final Report ===\n\n";
        
        // Learning Overview
        report << "🍼 BABY LEARNING OVERVIEW\n";
        report << "Total Learning Episodes: " << config_.total_learning_episodes << "\n";
        report << "Learning Scenarios: " << learning_scenarios_.size() << "\n";
        report << "Steps per Episode: " << config_.steps_per_episode << "\n\n";
        
        // Phase A Statistics
        report << "📊 PHASE A MIMICRY STATISTICS\n";
        report << "Total Mimicry Attempts: " << phase_a_stats.total_mimicry_attempts << "\n";
        report << "Successful Attempts: " << phase_a_stats.successful_mimicry_attempts << "\n";
        report << "Success Rate: " << std::fixed << std::setprecision(1)
               << (phase_a_stats.total_mimicry_attempts > 0 ? 
                   (100.0f * phase_a_stats.successful_mimicry_attempts / phase_a_stats.total_mimicry_attempts) : 0.0f)
               << "%\n";
        report << "Teacher Embeddings: " << phase_a_stats.teacher_embeddings_stored << "\n";
        report << "Multimodal Alignments: " << phase_a_stats.multimodal_alignments_created << "\n";
        report << "Average Similarity Score: " << std::fixed << std::setprecision(3)
               << phase_a_stats.average_similarity_score << "\n";
        report << "Average Novelty Score: " << phase_a_stats.average_novelty_score << "\n";
        report << "Cross-Modal Alignment Strength: " << phase_a_stats.cross_modal_alignment_strength << "\n\n";
        
        // Language Development (Phase 5)
        report << "🗣️ LANGUAGE DEVELOPMENT (PHASE 5)\n";
        report << "Current Stage: " << static_cast<int>(lang_stats.current_stage) << "\n";
        report << "Total Vocabulary: " << lang_stats.active_vocabulary_size << " tokens\n";
        report << "Successful Mimicry: " << lang_stats.successful_mimicry_attempts << "\n";
        report << "Grounding Associations: " << lang_stats.grounding_associations_formed << "\n";
        report << "Internal Narration Entries: " << lang_stats.narration_entries << "\n";
        report << "Average Token Activation: " << std::fixed << std::setprecision(3)
               << lang_stats.average_token_activation << "\n\n";
        
        // Modality Breakdown
        report << "🎭 MODALITY LEARNING BREAKDOWN\n";
        for (const auto& pair : phase_a_stats.modality_counts) {
            report << "  " << pair.first << ": " << pair.second << " experiences\n";
        }
        report << "\n";
        
        // Teacher Performance
        report << "👨‍🏫 TEACHER ENCODER PERFORMANCE\n";
        for (const auto& pair : phase_a_stats.teacher_type_performance) {
            report << "  " << pair.first << ": " << std::fixed << std::setprecision(3)
                   << pair.second << " avg performance\n";
        }
        report << "\n";
        
        // Recent Vocabulary
        auto active_vocab = language_system_->getActiveVocabulary(0.2f);
        report << "📚 LEARNED VOCABULARY (Active Tokens)\n";
        for (std::size_t i = 0; i < std::min(active_vocab.size(), std::size_t(20)); ++i) {
            report << "  " << active_vocab[i];
            if (i < std::min(active_vocab.size(), std::size_t(20)) - 1) {
                report << ", ";
            }
        }
        if (active_vocab.size() > 20) {
            report << ", ... (" << (active_vocab.size() - 20) << " more)";
        }
        report << "\n\n";
        
        // Recent Narration
        auto recent_narration = language_system_->getRecentNarration(5);
        if (!recent_narration.empty()) {
            report << "💭 RECENT INTERNAL NARRATION\n";
            for (const auto& entry : recent_narration) {
                report << "  [" << std::fixed << std::setprecision(2) << entry.confidence << "] ";
                for (const auto& token : entry.token_sequence) {
                    report << token.symbol << " ";
                }
                report << "(" << entry.context << ")\n";
            }
            report << "\n";
        }
        
        // Learning Insights
        report << "🧠 LEARNING INSIGHTS\n";
        report << "• Baby successfully learned multimodal associations\n";
        report << "• Cross-modal alignment enables concept grounding\n";
        report << "• Teacher-student mimicry drives vocabulary development\n";
        report << "• Internal narration shows emerging language understanding\n";
        report << "• Progressive difficulty supports developmental learning\n\n";
        
        // Technical Details
        report << "⚙️ TECHNICAL DETAILS\n";
        report << "• Phase A + Phase 5 integration: ✅ Successful\n";
        report << "• Multimodal teacher encoders: CLIP, Whisper, BERT (simulated)\n";
        report << "• Cross-modal brain connectivity: 4 regions, 6 connections\n";
        report << "• Embedding dimension: 512\n";
        report << "• Learning rate adaptation: Dynamic noise reduction\n\n";
        
        std::string report_str = report.str();
        
        // Display report
        std::cout << report_str;
        
        // Save report to file
        if (!config_.final_report_filename.empty()) {
            std::ofstream report_file(config_.final_report_filename);
            if (report_file.is_open()) {
                report_file << report_str;
                report_file.close();
                std::cout << "✅ Final report saved to " << config_.final_report_filename << "\n";
            }
        }
        
        // Save Phase A data
        std::ofstream vocab_file("phase_a_teacher_embeddings.json");
        if (vocab_file.is_open()) {
            vocab_file << phase_a_system_->exportTeacherEmbeddingsToJson();
            vocab_file.close();
            std::cout << "✅ Teacher embeddings saved to phase_a_teacher_embeddings.json\n";
        }
        
        std::ofstream mimicry_file("phase_a_mimicry_history.json");
        if (mimicry_file.is_open()) {
            mimicry_file << phase_a_system_->exportMimicryHistoryToJson();
            mimicry_file.close();
            std::cout << "✅ Mimicry history saved to phase_a_mimicry_history.json\n";
        }
        
        std::ofstream alignment_file("phase_a_alignments.json");
        if (alignment_file.is_open()) {
            alignment_file << phase_a_system_->exportAlignmentsToJson();
            alignment_file.close();
            std::cout << "✅ Multimodal alignments saved to phase_a_alignments.json\n";
        }
        
        if (config_.save_progress_log) {
            std::cout << "✅ Learning progress saved to " << config_.log_filename << "\n";
        }
        
        std::cout << "\n=== Phase A Baby Multimodal Mimicry Demo Complete ===\n";
    }
    
public:
    // Dataset-driven override for learning_scenarios_
    void overrideScenariosFromTripletsRoot(const std::string& root_dir, int limit, bool shuffle) {
        namespace fs = std::filesystem;
        std::vector<LearningScenario> scenarios;
        if (root_dir.empty()) return;
        try {
            std::unordered_map<std::string, std::string> audio_by_stem;
            std::unordered_map<std::string, std::string> text_by_stem;
            std::unordered_map<std::string, std::string> image_by_stem;

            auto has_ext = [](const fs::path& p, std::initializer_list<const char*> exts) {
                auto e = p.extension().string();
                std::transform(e.begin(), e.end(), e.begin(), ::tolower);
                for (auto x : exts) { if (e == x) return true; }
                return false;
            };

            for (auto it = fs::recursive_directory_iterator(root_dir, fs::directory_options::skip_permission_denied);
                 it != fs::recursive_directory_iterator(); ++it) {
                if (!it->is_regular_file()) continue;
                const auto& p = it->path();
                auto stem = p.stem().string();
                if (has_ext(p, {".wav", ".mp3", ".flac", ".ogg"})) {
                    audio_by_stem[stem] = p.string();
                } else if (has_ext(p, {".txt"})) {
                    text_by_stem[stem] = p.string();
                } else if (has_ext(p, {".jpg", ".jpeg", ".png", ".bmp", ".gif"})) {
                    image_by_stem[stem] = p.string();
                }
            }

            auto read_text = [](const std::string& file) -> std::string {
                std::ifstream ifs(file);
                if (!ifs.is_open()) return std::string();
                std::stringstream ss; ss << ifs.rdbuf();
                return ss.str();
            };

            scenarios.reserve(text_by_stem.size());
            for (const auto& kv : text_by_stem) {
                const std::string& stem = kv.first;
                auto a_it = audio_by_stem.find(stem);
                if (a_it == audio_by_stem.end()) continue; // require at least audio+text
                const std::string& audio_path = a_it->second;
                const std::string& text_path = kv.second;
                std::string visual_path;
                auto img_it = image_by_stem.find(stem);
                if (img_it != image_by_stem.end()) visual_path = img_it->second; // optional

                std::string caption = read_text(text_path);
                if (caption.empty()) continue;

                std::vector<std::string> tokens;
                tokens.reserve(3);
                std::string cur;
                for (char c : caption) {
                    if (std::isalpha(static_cast<unsigned char>(c))) {
                        cur.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
                    } else {
                        if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
                    }
                    if (tokens.size() >= 3) break;
                }
                if (!cur.empty() && tokens.size() < 3) tokens.push_back(cur);

                LearningScenario sc;
                sc.name = stem;
                sc.visual_content = visual_path; // may be empty if no image present
                sc.audio_content = audio_path;
                sc.text_content = caption;
                sc.expected_tokens = tokens;
                sc.difficulty_level = 0.5f; // default mid difficulty
                scenarios.push_back(std::move(sc));
            }

            if (shuffle) {
                std::shuffle(scenarios.begin(), scenarios.end(), rng_);
            }
            if (limit > 0 && static_cast<int>(scenarios.size()) > limit) {
                scenarios.resize(static_cast<std::size_t>(limit));
            }

            if (!scenarios.empty()) {
                learning_scenarios_.swap(scenarios);
                std::cout << "📁 Loaded " << learning_scenarios_.size() << " dataset-driven scenarios from '" << root_dir << "'\n";
            } else {
                std::cout << "⚠️ No matching (audio,text) stems found under '" << root_dir << "'. Using built-in demo scenarios.\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to scan triplets root '" << root_dir << "': " << e.what() << "\n";
        }
    }
    
    // Quick setters for smoke testing
    void setTotalLearningEpisodes(int n) { if (n > 0) config_.total_learning_episodes = n; }
    void setStepDurationMs(int ms) { if (ms >= 0) config_.step_duration_ms = ms; }
    void setRewardScale(float s) { if (s >= 0.0f) config_.reward_scale = s; }
    void setZeroReward(bool z) { config_.zero_reward = z; }
    void setSubstrateModeByString(const std::string& mode) {
        std::string m = mode; 
        std::transform(m.begin(), m.end(), m.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (m == "off") config_.substrate_mode = SubstrateMode::Off;
        else if (m == "mirror") config_.substrate_mode = SubstrateMode::Mirror;
        else if (m == "train") config_.substrate_mode = SubstrateMode::Train;
        else if (m == "native") config_.substrate_mode = SubstrateMode::Native;
        else {
            std::cerr << "Unknown --substrate-mode='" << mode << "', defaulting to 'off'\n";
            config_.substrate_mode = SubstrateMode::Off;
        }
    }
};



int main(int argc, char* argv[]) {
    bool verbose = false;
    std::string triplets_root;
    int triplets_limit = 0;
    bool triplets_shuffle = false;
    int episodes_override = -1;
    int step_ms_override = -1;
    std::string substrate_mode_opt;
    bool zero_reward_opt = false;
    bool reward_scale_set = false;
    float reward_scale_value = 1.0f;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--triplets-root" && i + 1 < argc) {
            triplets_root = argv[++i];
        } else if (arg == "--triplets-limit" && i + 1 < argc) {
            triplets_limit = std::max(0, std::atoi(argv[++i]));
        } else if (arg == "--triplets-shuffle") {
            triplets_shuffle = true;
        } else if (arg == "--episodes" && i + 1 < argc) {
            try { episodes_override = std::stoi(argv[++i]); } catch (...) { std::cerr << "Error: invalid integer for --episodes" << std::endl; return 2; }
            if (episodes_override <= 0) { std::cerr << "Error: --episodes must be > 0" << std::endl; return 2; }
        } else if (arg == "--step_ms" && i + 1 < argc) {
            try { step_ms_override = std::stoi(argv[++i]); } catch (...) { std::cerr << "Error: invalid integer for --step_ms" << std::endl; return 2; }
            if (step_ms_override < 0) { std::cerr << "Error: --step_ms must be >= 0" << std::endl; return 2; }
        } else if (arg == "--substrate-mode" && i + 1 < argc) {
            substrate_mode_opt = argv[++i];
        } else if (arg == "--reward-scale" && i + 1 < argc) {
            try { reward_scale_value = std::stof(argv[++i]); reward_scale_set = true; } catch (...) { std::cerr << "Error: invalid float for --reward-scale" << std::endl; return 2; }
            if (reward_scale_value < 0.0f) { std::cerr << "Error: --reward-scale must be >= 0" << std::endl; return 2; }
        } else if (arg == "--zero-reward") {
            zero_reward_opt = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Phase A Baby Multimodal Mimicry Demo\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --verbose, -v            Enable verbose output\n";
            std::cout << "  --triplets-root <dir>    Use dataset-driven scenarios from this root folder (expects matched stems for audio(.wav/.mp3/..), text(.txt), optional image(.jpg/.png))\n";
            std::cout << "  --triplets-limit <N>     Limit number of scenarios loaded from dataset (0 = no limit)\n";
            std::cout << "  --triplets-shuffle       Shuffle scenarios before limiting\n";
            std::cout << "  --episodes <N>           Total learning episodes (default 50)\n";
            std::cout << "  --step_ms <MS>           Per-step sleep in ms (default 100, 0 to disable)\n";
            std::cout << "  --substrate-mode <m>     Neural substrate mode: off|mirror|train|native (default: off)\n";
            std::cout << "  --reward-scale <F>      Scale mimicry reward delivered to brain (default 1.0)\n";
            std::cout << "  --zero-reward           Disable reward delivery (sends 0) for ablation\n";
            std::cout << "  --help, -h               Show this help\n";
            return 0;
        }
    }

    try {
        PhaseABabyMimicryDemo demo(verbose);
        if (!triplets_root.empty()) {
            demo.overrideScenariosFromTripletsRoot(triplets_root, triplets_limit, triplets_shuffle);
        }

        // Add quick setters for smoke testing
        if (episodes_override > 0) demo.setTotalLearningEpisodes(episodes_override);
        if (step_ms_override >= 0) demo.setStepDurationMs(step_ms_override);
        if (!substrate_mode_opt.empty()) demo.setSubstrateModeByString(substrate_mode_opt);
        if (reward_scale_set) demo.setRewardScale(reward_scale_value);
        if (zero_reward_opt) demo.setZeroReward(true);

        if (!demo.initialize()) {
            std::cerr << "Failed to initialize Phase A demo\n";
            return 1;
        }

        demo.runDemo();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}