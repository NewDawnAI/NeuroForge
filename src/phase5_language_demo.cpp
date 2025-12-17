/**
 * @file phase5_language_demo.cpp
 * @brief Phase 5 Language Learning Demo for NeuroForge
 * 
 * This demo showcases the integration of the LanguageSystem with the main NeuroForge
 * brain simulation, demonstrating developmental language acquisition through:
 * - Mimicry-based learning from teacher signals
 * - Multimodal grounding with vision and audio
 * - Internal narration and self-reflection
 * - Progressive developmental stages
 */

#include "core/HypergraphBrain.h"
#include "core/LanguageSystem.h"
#include "core/LearningSystem.h"
#include "connectivity/ConnectivityManager.h"
#include "regions/CorticalRegions.h"
#include "encoders/VisionEncoder.h"
#include "encoders/AudioEncoder.h"
#include "core/MotorCortex.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <random>
#include <vector>
#include <memory>
#include <string>
#include <fstream>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace NeuroForge;
using namespace NeuroForge::Core;
using namespace NeuroForge::Connectivity;
using namespace NeuroForge::Encoders;

class Phase5LanguageDemo {
private:
    std::shared_ptr<ConnectivityManager> connectivity_manager_;
    std::unique_ptr<HypergraphBrain> brain_;
    std::unique_ptr<LanguageSystem> language_system_;
    std::unique_ptr<VisionEncoder> vision_encoder_;
    std::unique_ptr<AudioEncoder> audio_encoder_;
    std::unique_ptr<MotorCortex> motor_cortex_;
    std::mt19937 rng_;
    bool verbose_output_;
    
    // Language-grounded control signals (tracked to build MotorCortex state)
    float recent_visual_intensity_ = 0.0f;
    float recent_audio_energy_ = 0.0f;
    float intent_move_ = 0.0f;
    float intent_stop_ = 0.0f;
    float intent_see_ = 0.0f;
    float intent_hear_ = 0.0f;
    int teacher_cooldown_ = 0;
    bool had_teacher_recent_ = false;
    std::string last_teacher_word_;
    LanguageSystem::Statistics last_lang_stats_{};
    // Track last-step reward components for logging
    float last_r_intent_ = 0.0f;
    float last_r_teacher_ = 0.0f;
    float last_r_lang_ = 0.0f;
    float last_reward_d_ = 0.0f;
    float last_reward_c_ = 0.0f;
     
     // Demo configuration
     struct DemoConfig {
         int total_steps = 1000;
         int step_duration_ms = 50;
         bool enable_vision_grounding = true;
         bool enable_audio_grounding = true;
         bool enable_teacher_mode = true;
         bool save_progress_log = true;
         std::string log_filename = "phase5_language_progress.csv";
         int teacher_interval = 15; // steps between teacher signals (<=0 disables)
         int log_interval = 50;     // steps between progress logs
        // Reward shaping weights and gating
        float w_intent = 1.0f;
        float w_teacher = 1.0f;
        float w_lang = 1.0f;
        bool cont_include_lang = true;
        // 5.1 MotorCortex Hub controls
        bool mc_hub_mode = false;
        int episode_length = 100;
        int env_id = 1; // 1: locomotion, 2: attention switch, 3: stop/go gating
        bool save_action_log = true;
        std::string action_log_filename = "phase5_actions.csv";
     } config_;
     
     std::ofstream progress_log_;
     std::ofstream action_log_;
     int episode_step_ = 0;
     int episode_index_ = 0;
     int last_discrete_action_ = -1;
     std::vector<float> last_continuous_action_;
     float last_t0_ = 0.0f, last_t1_ = 0.0f;
     
  public:
    // Configuration setters (to be used from main after parsing CLI)
    void setRewardWeights(float w_intent, float w_teacher, float w_lang) {
        config_.w_intent = w_intent;
        config_.w_teacher = w_teacher;
        config_.w_lang = w_lang;
    }
    void setContinuousLangShaping(bool include) { config_.cont_include_lang = include; }
    void setTotalSteps(int steps) { if (steps > 0) config_.total_steps = steps; }
    void setStepDurationMs(int ms) { if (ms > 0) config_.step_duration_ms = ms; }
    void setTeacherInterval(int k) { config_.teacher_interval = k; }
    void setLogInterval(int k) { if (k > 0) config_.log_interval = k; }
    void setLogFilename(const std::string& f) { if (!f.empty()) config_.log_filename = f; }
    
    // Motor Cortex Hub (5.1) setters
    void enableMotorHub(bool on) { config_.mc_hub_mode = on; }
    void setEpisodeLength(int n) { if (n > 0) config_.episode_length = n; }
    void setEnvId(int e) { if (e >= 1 && e <= 3) config_.env_id = e; }
    void setActionLogFilename(const std::string& f) { if (!f.empty()) config_.action_log_filename = f; }
    
     Phase5LanguageDemo(bool verbose = false) 
         : rng_(std::random_device{}()), verbose_output_(verbose) {
        
        // Initialize core components
        connectivity_manager_ = std::make_shared<ConnectivityManager>();
        brain_ = std::make_unique<HypergraphBrain>(connectivity_manager_);
        
        // Configure language system for developmental learning
        LanguageSystem::Config lang_config;
        lang_config.mimicry_learning_rate = 0.02f;
        lang_config.grounding_strength = 0.8f;
        lang_config.narration_threshold = 0.4f;
        lang_config.max_vocabulary_size = 2000;
        lang_config.embedding_dimension = 256;
        lang_config.babbling_duration = 200;
        lang_config.mimicry_duration = 400;
        lang_config.grounding_duration = 600;
        lang_config.enable_teacher_mode = true;
        lang_config.teacher_influence = 0.9f;
        lang_config.enable_vision_grounding = true;
        lang_config.enable_audio_grounding = true;
        lang_config.enable_action_grounding = true;
        
        language_system_ = std::make_unique<LanguageSystem>(lang_config);
        
        // Initialize encoders
        VisionEncoder::Config vision_config;
        vision_config.grid_size = 16;
        vision_config.use_edge = true;
        vision_config.edge_weight = 0.7f;
        vision_config.intensity_weight = 0.3f;
        vision_encoder_ = std::make_unique<VisionEncoder>(vision_config);
        
        AudioEncoder::Config audio_config;
        audio_config.sample_rate = 16000;
        audio_config.feature_bins = 128;
        audio_config.spectral_bins = 64;
        audio_config.mel_bands = 32;
        audio_encoder_ = std::make_unique<AudioEncoder>(audio_config);

        // Initialize MotorCortex with simple configs
        MotorCortex::Config mc_cfg;
        mc_cfg.q_cfg.num_actions = 4;
        mc_cfg.ppo_cfg.action_dim = 2;
        motor_cortex_ = std::make_unique<MotorCortex>(mc_cfg);
     }
    
    ~Phase5LanguageDemo() {
        if (progress_log_.is_open()) {
            progress_log_.close();
        }
        if (action_log_.is_open()) {
            action_log_.close();
        }
    }
    
    bool initialize() {
        std::cout << "=== Initializing Phase 5 Language Learning Demo ===\n\n";
        
        // Initialize brain
        if (!brain_->initialize()) {
            std::cerr << "Failed to initialize brain\n";
            return false;
        }
        
        // Create basic brain regions for language grounding
        auto visual_cortex = brain_->createRegion("VisualCortex", Region::Type::Cortical);
        auto auditory_cortex = brain_->createRegion("AuditoryCortex", Region::Type::Cortical);
        auto motor_cortex = brain_->createRegion("MotorCortex", Region::Type::Cortical);
        auto language_area = brain_->createRegion("LanguageArea", Region::Type::Cortical);
        
        if (!visual_cortex || !auditory_cortex || !motor_cortex || !language_area) {
            std::cerr << "Failed to create brain regions\n";
            return false;
        }
        
        // Add neurons to regions
        visual_cortex->createNeurons(1000);
        auditory_cortex->createNeurons(800);
        motor_cortex->createNeurons(600);
        language_area->createNeurons(1200);
        
        // Establish inter-region connectivity
        brain_->connectRegions(visual_cortex->getId(), language_area->getId(), 0.15f, {0.2f, 0.8f});
        brain_->connectRegions(auditory_cortex->getId(), language_area->getId(), 0.12f, {0.3f, 0.7f});
        brain_->connectRegions(motor_cortex->getId(), language_area->getId(), 0.10f, {0.1f, 0.6f});
        
        // Initialize language system
        if (!language_system_->initialize()) {
            std::cerr << "Failed to initialize language system\n";
            return false;
        }
        
        // Initialize learning system
        LearningSystem::Config learning_config;
        learning_config.hebbian_rate = 0.001f;
        learning_config.stdp_rate = 0.002f;
        learning_config.enable_homeostasis = true;
        learning_config.enable_attention_modulation = true;
        
        if (!brain_->initializeLearning(learning_config)) {
            std::cerr << "Failed to initialize learning system\n";
            return false;
        }
        
        // Set up teacher embeddings for common words
        setupTeacherVocabulary();
        
        // Initialize progress logging
        if (config_.save_progress_log) {
            progress_log_.open(config_.log_filename);
            if (progress_log_.is_open()) {
                progress_log_ << "step,stage,vocabulary_size,active_tokens,narration_entries,"
                             << "mimicry_success,grounding_associations,avg_activation,mc_average_reward,teacher_event,"
                             << "r_intent,r_teacher,r_lang,reward_d,reward_c\n";
            }
        }
        // Initialize action trace logging (5.1 Hub)
        if (config_.save_action_log) {
            action_log_.open(config_.action_log_filename);
            if (action_log_.is_open()) {
                action_log_ << "step,episode,episode_step,action,a0,a1,t0,t1,reward_d,reward_c,mc_avg_reward,teacher_event,stage\n";
            }
        }
        
        std::cout << "✅ Phase 5 Language Demo initialized successfully\n\n";
        return true;
    }
    
    void runDemo() {
        std::cout << "=== Starting Phase 5 Language Development Simulation ===\n\n";
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (int step = 0; step < config_.total_steps; ++step) {
            // Update brain processing
            brain_->processStep(0.01f);
            
            // Update language development
            language_system_->updateDevelopment(0.01f);
            
            // Simulate sensory input and grounding
            if (step % 10 == 0) {
                simulateMultimodalInput(step);
            }
            
            // Provide teacher signals
            if (config_.enable_teacher_mode && config_.teacher_interval > 0 && (step % config_.teacher_interval == 0)) {
                provideTeacherSignal(step);
            }
            
            // Generate internal narration
            if (step % 20 == 0) {
                stimulateInternalNarration(step);
            }

            // Periodically exercise MotorCortex with synthetic states and rewards
            int mc_period = config_.mc_hub_mode ? 1 : 10;
            if (step % mc_period == 0) {
                updateMotorCortex(step);
                if (config_.mc_hub_mode) {
                    episode_step_++;
                    if (episode_step_ >= config_.episode_length) {
                        if (motor_cortex_) motor_cortex_->reset();
                        episode_step_ = 0;
                        episode_index_++;
                    }
                }
            }
            
            // Log progress
            if (step % config_.log_interval == 0) {
                logProgress(step);
                
                if (verbose_output_) {
                    displayProgress(step);
                }
            }
            
            // Brief pause for realistic timing
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.step_duration_ms));
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
        
        std::cout << "\n=== Phase 5 Language Development Complete ===\n";
        std::cout << "Total simulation time: " << duration.count() << " seconds\n\n";
        
        // Generate final report
        generateFinalReport();
    }
    
private:
    void setupTeacherVocabulary() {
        std::cout << "Setting up teacher vocabulary...\n";
        
        // Basic vocabulary with embeddings
        std::vector<std::pair<std::string, std::vector<float>>> teacher_words = {
            {"hello", generateSemanticEmbedding("greeting")},
            {"goodbye", generateSemanticEmbedding("farewell")},
            {"yes", generateSemanticEmbedding("affirmation")},
            {"no", generateSemanticEmbedding("negation")},
            {"red", generateSemanticEmbedding("color_red")},
            {"blue", generateSemanticEmbedding("color_blue")},
            {"big", generateSemanticEmbedding("size_large")},
            {"small", generateSemanticEmbedding("size_small")},
            {"move", generateSemanticEmbedding("action_move")},
            {"stop", generateSemanticEmbedding("action_stop")},
            {"see", generateSemanticEmbedding("perception_visual")},
            {"hear", generateSemanticEmbedding("perception_audio")},
            {"think", generateSemanticEmbedding("cognition_think")},
            {"feel", generateSemanticEmbedding("emotion_feel")},
            {"I", generateSemanticEmbedding("self_reference")},
            {"you", generateSemanticEmbedding("other_reference")}
        };
        
        for (const auto& word_pair : teacher_words) {
            language_system_->setTeacherEmbedding(word_pair.first, word_pair.second);
        }
        
        std::cout << "✅ Teacher vocabulary set up with " << teacher_words.size() << " words\n";
    }
    
    std::vector<float> generateSemanticEmbedding(const std::string& semantic_category) {
        std::vector<float> embedding(256);
        
        // Generate deterministic but varied embeddings based on semantic category
        std::hash<std::string> hasher;
        std::size_t seed = hasher(semantic_category);
        std::mt19937 local_rng(static_cast<uint32_t>(seed));
        std::normal_distribution<float> dist(0.0f, 1.0f);
        
        for (float& val : embedding) {
            val = dist(local_rng);
        }
        
        // Normalize embedding
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
        
        return embedding;
    }
    
    void simulateMultimodalInput(int step) {
        // Simulate visual input
        if (config_.enable_vision_grounding) {
            std::vector<float> visual_input = generateSyntheticVisualInput(step);
            auto visual_features = vision_encoder_->encode(visual_input);
            
            // Associate visual features with language tokens
            if (step % 30 == 0) {
                std::string visual_word = selectVisualWord(visual_features);
                if (!visual_word.empty()) {
                    auto* token = language_system_->getToken(visual_word);
                    if (token) {
                        // Simulate neural activation from visual input
                        std::vector<std::pair<NeuronID, float>> visual_activations;
                        for (int i = 0; i < 10; ++i) {
                            visual_activations.emplace_back(1000 + i, visual_features[i % visual_features.size()]);
                        }
                        language_system_->processNeuralActivation(visual_activations);
                    }
                }
            }
        }
        
        // Simulate audio input
        if (config_.enable_audio_grounding) {
            std::vector<float> audio_input = generateSyntheticAudioInput(step);
            auto audio_features = audio_encoder_->encode(audio_input);
            
            // Associate audio features with language tokens
            if (step % 25 == 0) {
                std::string audio_word = selectAudioWord(audio_features);
                if (!audio_word.empty()) {
                    auto* token = language_system_->getToken(audio_word);
                    if (token) {
                        // Simulate neural activation from audio input
                        std::vector<std::pair<NeuronID, float>> audio_activations;
                        for (int i = 0; i < 8; ++i) {
                            audio_activations.emplace_back(2000 + i, audio_features[i % audio_features.size()]);
                        }
                        language_system_->processNeuralActivation(audio_activations);
                    }
                }
            }
        }
    }
    
    std::vector<float> generateSyntheticVisualInput(int step) {
        std::vector<float> input(16 * 16); // 16x16 grid
        
        // Generate simple patterns that change over time
        float pattern_phase = static_cast<float>(step) * 0.1f;
        
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                int idx = y * 16 + x;
                
                // Create moving patterns
                float value = 0.5f + 0.3f * std::sin(pattern_phase + x * 0.3f + y * 0.2f);
                input[idx] = std::max(0.0f, std::min(1.0f, value));
            }
        }
        
        return input;
    }
    
    std::vector<float> generateSyntheticAudioInput(int step) {
        std::vector<float> input(1024); // Audio samples
        
        // Generate synthetic audio with varying frequency
        float frequency = 440.0f + 100.0f * std::sin(static_cast<float>(step) * 0.05f);
        float sample_rate = 16000.0f;
        
        for (std::size_t i = 0; i < input.size(); ++i) {
            float t = static_cast<float>(i) / sample_rate;
            input[i] = 0.3f * static_cast<float>(std::sin(2.0 * M_PI * static_cast<double>(frequency) * static_cast<double>(t)));
        }
        
        return input;
    }
    
    std::string selectVisualWord(const std::vector<float>& visual_features) {
        // Simple heuristic to select words based on visual features
        float avg_intensity = 0.0f;
        for (float val : visual_features) {
            avg_intensity += val;
        }
        avg_intensity /= visual_features.size();
        // record as part of MC state features
        recent_visual_intensity_ = avg_intensity;
        intent_see_ = std::max(intent_see_, avg_intensity);
        
        if (avg_intensity > 0.7f) {
            intent_see_ = std::max(intent_see_, 1.0f);
            return "bright";
        } else if (avg_intensity < 0.3f) {
            intent_see_ = std::max(intent_see_, 0.8f);
            return "dark";
        } else if (visual_features[0] > 0.6f) {
            intent_see_ = std::max(intent_see_, 0.7f);
            return "red";
        } else if (visual_features[1] > 0.6f) {
            intent_see_ = std::max(intent_see_, 0.7f);
            return "blue";
        }
        
        return "see";
    }
    
    std::string selectAudioWord(const std::vector<float>& audio_features) {
        // Simple heuristic to select words based on audio features
        float energy = 0.0f;
        for (float val : audio_features) {
            energy += val * val;
        }
        energy = std::sqrt(energy);
        // record as part of MC state features
        recent_audio_energy_ = energy;
        intent_hear_ = std::max(intent_hear_, std::min(1.0f, energy));
        
        if (energy > 0.5f) {
            intent_hear_ = std::max(intent_hear_, 1.0f);
            return "hear";
        } else if (energy > 0.2f) {
            intent_hear_ = std::max(intent_hear_, 0.6f);
            return "sound";
        }
        
        return "quiet";
    }
    
    // Minimal MotorCortex smoke test step (now as a class method)
    void updateMotorCortex(int step) {
        (void)step; // suppress unused parameter warning
        if (!motor_cortex_) return;
        
        // Build a language-grounded state
        State s;
        s.features = {
            static_cast<float>(static_cast<int>(language_system_->getCurrentStage()) / 5.0f),
            recent_visual_intensity_,
            recent_audio_energy_,
            had_teacher_recent_ ? 1.0f : 0.0f,
            intent_move_, intent_stop_, intent_see_, intent_hear_
        };
        
        // Discrete branch: map 4 actions to semantic intents
        // 0: idle/hold (stop), 1: attend vision (see), 2: attend audio (hear), 3: locomote (move)
        int action = motor_cortex_->selectDiscreteAction(s);
        
        // Reward shaping: intent alignment
        float r_intent = 0.0f;
        switch (action) {
            case 0: r_intent = intent_stop_ - 0.2f * intent_move_; break;
            case 1: r_intent = intent_see_; break;
            case 2: r_intent = intent_hear_; break;
            case 3: r_intent = intent_move_ - 0.2f * intent_stop_; break;
            default: r_intent = -0.05f; break;
        }
        
        // Teacher shaping: align with recent teacher word semantics
        auto teacherAlign = [&](const std::string& w, int a){
            if (w == "move") return (a == 3) ? 0.5f : -0.1f;
            if (w == "stop") return (a == 0) ? 0.5f : -0.1f;
            if (w == "see" || w == "red" || w == "blue" || w == "bright" || w == "dark") return (a == 1) ? 0.3f : 0.0f;
            if (w == "hear" || w == "sound" || w == "quiet") return (a == 2) ? 0.3f : 0.0f;
            return 0.0f;
        };
        float r_teacher = (had_teacher_recent_ ? teacherAlign(last_teacher_word_, action) : 0.0f);
        
        // Language learning improvement shaping (delta-based)
        auto stats_now = language_system_->getStatistics();
        float delta_tokens = static_cast<float>(stats_now.total_tokens_generated - last_lang_stats_.total_tokens_generated);
        float delta_mimic  = static_cast<float>(stats_now.successful_mimicry_attempts - last_lang_stats_.successful_mimicry_attempts);
        float delta_ground = static_cast<float>(stats_now.grounding_associations_formed - last_lang_stats_.grounding_associations_formed);
        float delta_narr   = static_cast<float>(stats_now.narration_entries - last_lang_stats_.narration_entries);
        float delta_act    = stats_now.average_token_activation - last_lang_stats_.average_token_activation;
        float r_lang = 0.10f * delta_tokens + 0.50f * delta_mimic + 0.20f * delta_ground + 0.05f * delta_narr + 0.10f * std::max(0.0f, delta_act);
        
        // Apply weights
        float wr_intent = config_.w_intent * r_intent;
        float wr_teacher = config_.w_teacher * r_teacher;
        float wr_lang = config_.w_lang * r_lang;
        
        // Total discrete reward (weighted)
        float reward_d = wr_intent + wr_teacher + wr_lang;
        
        // Build a simple next state by slight decay/perturbation
        State s_next = s;
        s_next.features[4] *= 0.95f; // move intent decay
        s_next.features[5] *= 0.95f; // stop intent decay
        s_next.features[6] *= 0.97f; // see
        s_next.features[7] *= 0.97f; // hear
        
        // Environment-specific shaping (5.1 Hub)
        float env_bonus_d = 0.0f;
        float env_bonus_c = 0.0f;
        // Continuous branch: target encodes desired locomotion and attention balance
        auto a = motor_cortex_->selectContinuousAction(s);
        float t0 = std::clamp(intent_move_ - intent_stop_, -1.0f, 1.0f);
        float t1 = std::clamp(intent_see_ - intent_hear_, -1.0f, 1.0f);
        
        switch (config_.env_id) {
            case 1: // E1: locomotion preference
                env_bonus_d += (action == 3 ? 0.2f : 0.0f);
                env_bonus_c += 0.1f * std::max(0.0f, t0);
                break;
            case 2: { // E2: attention switching every 50 steps
                bool want_vision = ((step / 50) % 2 == 0);
                env_bonus_d += want_vision ? (action == 1 ? 0.2f : 0.0f)
                                           : (action == 2 ? 0.2f : 0.0f);
                // Encourage t1 alignment (+1 vision, -1 audio)
                float desired_t1 = want_vision ? 1.0f : -1.0f;
                float align = 1.0f - std::min(1.0f, std::abs(t1 - desired_t1));
                env_bonus_c += 0.1f * std::max(0.0f, align);
                break; }
            case 3: // E3: stop/go based on recent teacher cues
                if (last_teacher_word_ == "stop") env_bonus_d += (action == 0 ? 0.3f : -0.05f);
                if (last_teacher_word_ == "move") env_bonus_d += (action == 3 ? 0.3f : -0.05f);
                break;
            default: break;
        }
        reward_d += env_bonus_d;
        
        DiscreteExperience de{ s, action, reward_d, s_next, false };
        motor_cortex_->stepDiscrete(de);
        
        float d0 = (a.size() > 0) ? (a[0] - t0) : 0.0f;
        float d1 = (a.size() > 1) ? (a[1] - t1) : 0.0f;
        float r_cont = 1.0f - (d0 * d0 + d1 * d1) + (config_.cont_include_lang ? 0.5f * wr_lang : 0.0f);
        r_cont += env_bonus_c; // add env shaping
        ContinuousExperience ce;
        ce.s = s;
        ce.a = a;
        ce.r = r_cont;
        ce.s_next = s_next;
        ce.done = false;
        ce.advantage = 0.0f;
        ce.old_log_prob = 0.0f;
        motor_cortex_->stepContinuous({ce});
        
        // Save last action components for logging
        last_discrete_action_ = action;
        last_continuous_action_ = a;
        last_t0_ = t0; last_t1_ = t1;
        
        // Save last reward components for logging
        last_r_intent_ = wr_intent;
        last_r_teacher_ = wr_teacher;
        last_r_lang_ = wr_lang;
        last_reward_d_ = reward_d;
        last_reward_c_ = r_cont;
        
        // Action trace logging (per MC step)
        if (action_log_.is_open()) {
            auto mc_stats = motor_cortex_->getStatistics();
            action_log_ << step << "," << episode_index_ << "," << episode_step_ << "," << action << ","
                        << std::fixed << std::setprecision(3)
                        << (a.size() > 0 ? a[0] : 0.0f) << ","
                        << (a.size() > 1 ? a[1] : 0.0f) << ","
                        << last_t0_ << "," << last_t1_ << ","
                        << last_reward_d_ << "," << last_reward_c_ << ","
                        << mc_stats.average_reward << ","
                        << (had_teacher_recent_ ? 1 : 0) << ","
                        << static_cast<int>(language_system_->getCurrentStage())
                        << "\n";
            action_log_.flush();
        }
        
        // Update baselines and decay flags
        last_lang_stats_ = stats_now;
        if (teacher_cooldown_ > 0) { --teacher_cooldown_; had_teacher_recent_ = true; } else { had_teacher_recent_ = false; }
        intent_move_ *= 0.95f; intent_stop_ *= 0.95f; intent_see_ *= 0.97f; intent_hear_ *= 0.97f;
     }
    
    void provideTeacherSignal(int step) {
        // Rotate through teacher vocabulary
        std::vector<std::string> teacher_words = {
            "hello", "goodbye", "yes", "no", "red", "blue", 
            "big", "small", "move", "stop", "see", "hear",
            "think", "feel", "I", "you"
        };
        
        std::string word = teacher_words[step % teacher_words.size()];
        float reward = 0.8f + 0.2f * std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_);
        
        language_system_->processTeacherSignal(word, reward);
        
        // expose teacher signal to MotorCortex state intents
        last_teacher_word_ = word;
        teacher_cooldown_ = 5;
        had_teacher_recent_ = true;
        if (word == "move") intent_move_ = std::max(intent_move_, 1.0f);
        if (word == "stop") intent_stop_ = std::max(intent_stop_, 1.0f);
        if (word == "see" || word == "red" || word == "blue") intent_see_ = std::max(intent_see_, 1.0f);
        if (word == "hear" || word == "sound" || word == "quiet") intent_hear_ = std::max(intent_hear_, 1.0f);
        
        if (verbose_output_ && step % 100 == 0) {
            std::cout << "Teacher signal: '" << word << "' (reward: " 
                     << std::fixed << std::setprecision(2) << reward << ")\n";
        }
    }
    
    void stimulateInternalNarration(int step) {
        (void)step; // suppress unused parameter warning on MSVC
        // Generate context-based internal narration
        auto current_stage = language_system_->getCurrentStage();
        
        std::vector<std::string> narration_tokens;
        std::string context;
        
        switch (current_stage) {
            case LanguageSystem::DevelopmentalStage::Chaos:
                // Random activation
                narration_tokens = {"noise", "random", "chaos"};
                context = "Chaotic neural activity";
                break;
                
            case LanguageSystem::DevelopmentalStage::Babbling:
                // Proto-language sounds
                narration_tokens = {"ba", "ma", "da", "ga"};
                context = "Babbling exploration";
                break;
                
            case LanguageSystem::DevelopmentalStage::Mimicry:
                // Copying teacher words
                narration_tokens = {"hello", "copy", "mimic"};
                context = "Teacher imitation";
                break;
                
            case LanguageSystem::DevelopmentalStage::Grounding:
                // Associating words with experiences
                narration_tokens = {"I", "see", "red", "hear", "sound"};
                context = "Sensory grounding";
                break;
                
            case LanguageSystem::DevelopmentalStage::Reflection:
                // Self-aware narration
                narration_tokens = {"I", "think", "therefore", "I", "am"};
                context = "Self-reflection";
                break;
                
            case LanguageSystem::DevelopmentalStage::Communication:
                // Goal-directed communication
                narration_tokens = {"I", "want", "to", "communicate"};
                context = "Intentional communication";
                break;
        }
        
        // convert narration to intent signals (proto-goals)
        for (const auto& tok : narration_tokens) {
            if (tok == "move") intent_move_ = std::max(intent_move_, 0.8f);
            if (tok == "stop") intent_stop_ = std::max(intent_stop_, 0.8f);
            if (tok == "see"  || tok == "red" || tok == "blue") intent_see_ = std::max(intent_see_, 0.6f);
            if (tok == "hear" || tok == "sound" || tok == "quiet") intent_hear_ = std::max(intent_hear_, 0.6f);
        }
        
        float confidence = 0.3f + 0.4f * (static_cast<int>(current_stage) / 5.0f);
        language_system_->logSelfNarration(narration_tokens, confidence, context);
    }
    
    void logProgress(int step) {
        if (!progress_log_.is_open()) {
            return;
        }
        
        auto stats = language_system_->getStatistics();
        auto mc_stats = motor_cortex_ ? motor_cortex_->getStatistics() : MotorCortex::Statistics{};
 
        progress_log_ << step << ","
                     << static_cast<int>(stats.current_stage) << ","
                     << stats.active_vocabulary_size << ","
                     << stats.total_tokens_generated << ","
                     << stats.narration_entries << ","
                     << stats.successful_mimicry_attempts << ","
                     << stats.grounding_associations_formed << ","
                     << std::fixed << std::setprecision(3) << stats.average_token_activation << ","
                     << std::fixed << std::setprecision(3) << mc_stats.average_reward << ","
                     << (had_teacher_recent_ ? 1 : 0) << ","
                     << std::fixed << std::setprecision(3) << last_r_intent_ << ","
                     << std::fixed << std::setprecision(3) << last_r_teacher_ << ","
                     << std::fixed << std::setprecision(3) << last_r_lang_ << ","
                     << std::fixed << std::setprecision(3) << last_reward_d_ << ","
                     << std::fixed << std::setprecision(3) << last_reward_c_
                      << "\n";
 
        progress_log_.flush();
    }
    
    void displayProgress(int step) {
        auto stats = language_system_->getStatistics();
        auto mc_stats = motor_cortex_ ? motor_cortex_->getStatistics() : MotorCortex::Statistics{};
 
        std::cout << "Step " << std::setw(4) << step 
                 << " | Stage: " << stageToString(stats.current_stage)
                 << " | Vocab: " << std::setw(3) << stats.active_vocabulary_size
                 << " | Narration: " << std::setw(3) << stats.narration_entries
                 << " | Mimicry: " << std::setw(3) << stats.successful_mimicry_attempts
                 << " | Avg Activation: " << std::fixed << std::setprecision(3) 
                 << stats.average_token_activation
                 << " | MC AvgR: " << std::fixed << std::setprecision(3) << mc_stats.average_reward
                 << "\n";
    }
    
    std::string stageToString(LanguageSystem::DevelopmentalStage stage) {
        switch (stage) {
            case LanguageSystem::DevelopmentalStage::Chaos: return "Chaos";
            case LanguageSystem::DevelopmentalStage::Babbling: return "Babbling";
            case LanguageSystem::DevelopmentalStage::Mimicry: return "Mimicry";
            case LanguageSystem::DevelopmentalStage::Grounding: return "Grounding";
            case LanguageSystem::DevelopmentalStage::Reflection: return "Reflection";
            case LanguageSystem::DevelopmentalStage::Communication: return "Communication";
        }
        return "Unknown";
    }
    
    void generateFinalReport() {
        std::cout << "\n" << language_system_->generateLanguageReport() << "\n";
        
        // Display recent narration samples
        auto recent_narration = language_system_->getRecentNarration(5);
        if (!recent_narration.empty()) {
            std::cout << "Recent Internal Narration Samples:\n";
            for (const auto& entry : recent_narration) {
                std::cout << "  [" << std::fixed << std::setprecision(2) << entry.confidence << "] ";
                for (const auto& token : entry.token_sequence) {
                    std::cout << token.symbol << " ";
                }
                std::cout << "(" << entry.context << ")\n";
            }
            std::cout << "\n";
        }
        
        // Display active vocabulary
        auto active_vocab = language_system_->getActiveVocabulary(0.2f);
        std::cout << "Active Vocabulary (" << active_vocab.size() << " tokens):\n";
        for (std::size_t i = 0; i < std::min(active_vocab.size(), std::size_t(20)); ++i) {
            std::cout << "  " << active_vocab[i];
            if (i < std::min(active_vocab.size(), std::size_t(20)) - 1) {
                std::cout << ", ";
            }
        }
        if (active_vocab.size() > 20) {
            std::cout << ", ... (" << (active_vocab.size() - 20) << " more)";
        }
        std::cout << "\n\n";
        
        // Save final vocabulary and narration
        std::ofstream vocab_file("phase5_final_vocabulary.json");
        if (vocab_file.is_open()) {
            vocab_file << language_system_->exportVocabularyToJson();
            vocab_file.close();
            std::cout << "✅ Final vocabulary saved to phase5_final_vocabulary.json\n";
        }
        
        std::ofstream narration_file("phase5_final_narration.json");
        if (narration_file.is_open()) {
            narration_file << language_system_->exportNarrationToJson();
            narration_file.close();
            std::cout << "✅ Final narration saved to phase5_final_narration.json\n";
        }
        
        if (config_.save_progress_log) {
            std::cout << "✅ Progress log saved to " << config_.log_filename << "\n";
        }
        
        std::cout << "\n=== Phase 5 Language Learning Demo Complete ===\n";
    }

};


int main(int argc, char* argv[]) {
    bool verbose = false;
    // Defaults for CLI-configurable parameters
    float w_intent = 1.0f, w_teacher = 1.0f, w_lang = 1.0f;
    bool no_lang_cont = false;
    int steps_override = -1;
    int step_ms_override = -1;
    int teacher_interval_override = -1;
    int log_interval_override = -1;
    std::string log_file_override;
    // Added for Motor Cortex Hub (5.1) CLI controls
    bool mc_hub = false;
    int env_override = 0;
    int episode_len_override = -1;
    std::string action_log_override;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else if (arg == "--w_intent" && i + 1 < argc) {
            w_intent = std::stof(argv[++i]);
        } else if (arg == "--w_teacher" && i + 1 < argc) {
            w_teacher = std::stof(argv[++i]);
        } else if (arg == "--w_lang" && i + 1 < argc) {
            w_lang = std::stof(argv[++i]);
        } else if (arg == "--no_lang_cont") {
            no_lang_cont = true;
        } else if (arg == "--steps" && i + 1 < argc) {
            steps_override = std::stoi(argv[++i]);
        } else if (arg == "--step_ms" && i + 1 < argc) {
            step_ms_override = std::stoi(argv[++i]);
        } else if (arg == "--teacher_interval" && i + 1 < argc) {
            teacher_interval_override = std::stoi(argv[++i]);
        } else if (arg == "--log_interval" && i + 1 < argc) {
            log_interval_override = std::stoi(argv[++i]);
        } else if (arg == "--log_file" && i + 1 < argc) {
            log_file_override = argv[++i];
        } else if (arg == "--mc_hub") {
            mc_hub = true;
        } else if (arg == "--env" && i + 1 < argc) {
            env_override = std::stoi(argv[++i]);
        } else if (arg == "--episode_len" && i + 1 < argc) {
            episode_len_override = std::stoi(argv[++i]);
        } else if (arg == "--action_log_file" && i + 1 < argc) {
            action_log_override = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Phase 5 Language Learning Demo\n";
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "Options:\n";
            std::cout << "  --verbose, -v         Enable verbose output\n";
            std::cout << "  --w_intent <f>        Weight for intent alignment reward (default 1.0)\n";
            std::cout << "  --w_teacher <f>       Weight for teacher alignment reward (default 1.0)\n";
            std::cout << "  --w_lang <f>          Weight for language improvement reward (default 1.0)\n";
            std::cout << "  --no_lang_cont        Disable language shaping in continuous reward\n";
            std::cout << "  --steps <n>           Override total steps (default 1000)\n";
            std::cout << "  --step_ms <n>         Override per-step sleep ms (default 50)\n";
            std::cout << "  --teacher_interval<n> Force teacher signal cadence in steps (<=0 disables) (default 15)\n";
            std::cout << "  --log_interval <n>    Logging cadence in steps (default 50)\n";
            std::cout << "  --log_file <path>     Override CSV output filename\n";
            std::cout << "  --mc_hub              Enable Motor Cortex Hub (5.1) action loop mode\n";
            std::cout << "  --env <1|2|3>         Select Hub curriculum env: 1=locomotion, 2=attention switch, 3=stop/go\n";
            std::cout << "  --episode_len <n>     Episode length for Hub mode (default 100)\n";
            std::cout << "  --action_log_file <p> Override action trace CSV filename (default phase5_actions.csv)\n";
            std::cout << "  --help, -h            Show this help\n";
            return 0;
        }
    }
    
    try {
        Phase5LanguageDemo demo(verbose);
        // Apply CLI-configured parameters
        demo.setRewardWeights(w_intent, w_teacher, w_lang);
        demo.setContinuousLangShaping(!no_lang_cont);
        if (steps_override > 0) demo.setTotalSteps(steps_override);
        if (step_ms_override > 0) demo.setStepDurationMs(step_ms_override);
        if (teacher_interval_override != -1) demo.setTeacherInterval(teacher_interval_override);
        if (log_interval_override > 0) demo.setLogInterval(log_interval_override);
        if (!log_file_override.empty()) demo.setLogFilename(log_file_override);
        demo.enableMotorHub(mc_hub);
        if (env_override != 0) demo.setEnvId(env_override);
        if (episode_len_override > 0) demo.setEpisodeLength(episode_len_override);
        if (!action_log_override.empty()) demo.setActionLogFilename(action_log_override);
        
        if (!demo.initialize()) {
            std::cerr << "Failed to initialize Phase 5 demo\n";
            return 1;
        }
        
        demo.runDemo();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
  }