#include "core/LanguageSystem.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <thread>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace NeuroForge::Core;

class DevelopmentalTrackingDemo {
private:
    std::unique_ptr<LanguageSystem> language_system_;
    std::mt19937 rng_;
    
public:
    DevelopmentalTrackingDemo() : rng_(std::random_device{}()) {
        // Configure for developmental tracking
        LanguageSystem::Config config;
        config.enable_acoustic_preprocessing = true;
        config.enable_prosodic_embeddings = true;
        config.enable_vision_grounding = true;
        config.enable_face_language_bias = true;
        
        // Babbling-stage tuned settings (v2.0)
        config.prosody_attention_weight = 0.4f;       // Stronger prosody guidance
        config.intonation_threshold = 0.1f;           // Infant-like intonation sensitivity
        config.cross_modal_decay = 0.002f;            // Reduced decay for retention
        config.token_similarity_threshold = 0.3f;     // Easier clustering for proto-words
        config.cohesion_boost_factor = 2.0f;          // Stronger association boost
        config.co_occurrence_bonus = 0.02f;           // Reward repeated token pairs
        config.motherese_boost = 0.6f;                // Strong caregiver preference
        
        language_system_ = std::make_unique<LanguageSystem>(config);
        language_system_->initialize();
        
        // Enable trajectory tracking
        language_system_->enableTrajectoryTracking("developmental_demo_logs");
        
        std::cout << "🧠 NeuroForge Developmental Tracking Demo Initialized" << std::endl;
        std::cout << "📊 Trajectory logging enabled in: developmental_demo_logs/" << std::endl;
    }
    
    void runDevelopmentalSimulation(int total_steps = 200) {
        std::cout << "\n🚀 Starting Developmental Simulation (" << total_steps << " steps)" << std::endl;
        std::cout << "Stage progression: Chaos → Babbling → Mimicry → Grounding" << std::endl;
        
        for (int step = 0; step < total_steps; ++step) {
            auto current_stage = language_system_->getCurrentStage();
            
            // Simulate different activities based on developmental stage
            switch (current_stage) {
                case LanguageSystem::DevelopmentalStage::Chaos:
                    simulateChaosStage(step);
                    break;
                case LanguageSystem::DevelopmentalStage::Babbling:
                    simulateBabblingStage(step);
                    break;
                case LanguageSystem::DevelopmentalStage::Mimicry:
                    simulateMimicryStage(step);
                    break;
                case LanguageSystem::DevelopmentalStage::Grounding:
                    simulateGroundingStage(step);
                    break;
                default:
                    simulateAdvancedStage(step);
                    break;
            }
            
            // Capture trajectory snapshot
            language_system_->captureTrajectorySnapshot();
            
            // Update development
            language_system_->updateDevelopment(0.1f);
            
            // Progress reporting
            if (step % 20 == 0) {
                reportProgress(step, current_stage);
            }
            
            // Brief pause for realistic timing
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        std::cout << "\n✅ Developmental simulation complete!" << std::endl;
        
        // Generate comprehensive report
        language_system_->generateDevelopmentalReport();
        
        // Final statistics
        reportFinalStatistics();
    }
    
private:
    void simulateChaosStage(int step) {
        // Random acoustic babbling - exploring sound space
        if (step % 3 == 0) {
            language_system_->performAcousticBabbling(2 + (step % 4));
        }
        
        // Occasional random teacher signals
        if (step % 15 == 0) {
            std::vector<std::string> chaos_sounds = {"ah", "eh", "oo", "mm", "ba", "da"};
            std::uniform_int_distribution<> sound_dist(0, chaos_sounds.size() - 1);
            
            auto teacher_audio = generateSyntheticAudio(chaos_sounds[sound_dist(rng_)], 150.0f);
            language_system_->processAcousticTeacherSignal(teacher_audio, chaos_sounds[sound_dist(rng_)], 0.7f);
        }

        // Proto-word crystallization reinforcement (early "mama" events)
        if (step % 12 == 0) {
            auto teacher_audio = generateSyntheticAudio("mama", 180.0f, true); // motherese
            language_system_->processAcousticTeacherSignal(teacher_audio, "mama", 1.0f);
            // Joint attention to accelerate grounding signals
            std::vector<float> gaze = {0.25f, 0.55f};
            language_system_->processJointAttentionEvent(gaze, "mama");
        }
    }
    
    void simulateBabblingStage(int step) {
        // More structured babbling with proto-phonemes
        if (step % 2 == 0) {
            language_system_->performAcousticBabbling(3 + (step % 3));
        }
        
        // Caregiver-like interactions with motherese
        if (step % 8 == 0) {
            std::vector<std::string> babbling_sounds = {"mama", "baba", "dada", "gaga", "nana"};
            std::uniform_int_distribution<> sound_dist(0, babbling_sounds.size() - 1);
            
            // Higher pitch for motherese
            auto teacher_audio = generateSyntheticAudio(babbling_sounds[sound_dist(rng_)], 200.0f, true);
            language_system_->processAcousticTeacherSignal(teacher_audio, babbling_sounds[sound_dist(rng_)], 0.9f);
        }
        
        // Visual-linguistic integration
        if (step % 12 == 0) {
            simulateVisualLanguageEvent("mama"); // strengthen face-speech coupling on proto-word
            // Additional joint attention with object mapping
            std::vector<std::pair<std::string, std::vector<float>>> early_objects = {
                {"mama", {0.0f, 0.0f}},
                {"ball", {0.2f, 0.3f}},
                {"cup",  {0.7f, 0.4f}}
            };
            std::uniform_int_distribution<> obj_dist(0, early_objects.size() - 1);
            auto [word, gaze] = early_objects[obj_dist(rng_)];
            language_system_->processJointAttentionEvent(gaze, word);
        }
    }
    
    void simulateMimicryStage(int step) {
        // Focused mimicry attempts
        if (step % 4 == 0) {
            std::vector<std::string> mimicry_targets = {"mama", "papa", "bye", "hi", "more"};
            std::uniform_int_distribution<> target_dist(0, mimicry_targets.size() - 1);
            
            std::string target = mimicry_targets[target_dist(rng_)];
            auto teacher_audio = generateSyntheticAudio(target, 180.0f, true);
            language_system_->processAcousticTeacherSignal(teacher_audio, target, 1.0f);
            
            // Attempt to mimic
            language_system_->performAcousticBabbling(1);
        }
        
        // Joint attention events
        if (step % 10 == 0) {
            std::vector<float> gaze_target = {0.3f + (step % 5) * 0.1f, 0.5f};
            language_system_->processJointAttentionEvent(gaze_target, "look");
        }
    }
    
    void simulateGroundingStage(int step) {
        // Grounded word learning with context
        if (step % 3 == 0) {
            std::vector<std::pair<std::string, std::vector<float>>> grounded_words = {
                {"ball", {0.2f, 0.3f}},
                {"cup", {0.7f, 0.4f}},
                {"book", {0.5f, 0.6f}},
                {"toy", {0.1f, 0.8f}}
            };
            
            std::uniform_int_distribution<> word_dist(0, grounded_words.size() - 1);
            auto [word, gaze] = grounded_words[word_dist(rng_)];
            
            // Teach word with visual grounding
            auto teacher_audio = generateSyntheticAudio(word, 160.0f);
            language_system_->processAcousticTeacherSignal(teacher_audio, word, 1.0f);
            language_system_->processJointAttentionEvent(gaze, word);
            
            // Simulate visual-linguistic integration
            simulateVisualLanguageEvent(word);
        }
    }
    
    void simulateAdvancedStage(int step) {
        // Advanced communication patterns
        if (step % 5 == 0) {
            std::vector<std::string> advanced_words = {"please", "thank", "help", "want", "like"};
            std::uniform_int_distribution<> word_dist(0, advanced_words.size() - 1);
            
            std::string word = advanced_words[word_dist(rng_)];
            auto teacher_audio = generateSyntheticAudio(word, 170.0f);
            language_system_->processAcousticTeacherSignal(teacher_audio, word, 1.0f);
        }
    }
    
    void simulateVisualLanguageEvent(const std::string& word) {
        // Create visual-linguistic features
        LanguageSystem::VisualLanguageFeatures visual_features;
        visual_features.face_salience = 0.9f + (rng_() % 10) * 0.01f; // emphasize face
        visual_features.gaze_alignment = 0.8f + (rng_() % 20) * 0.01f; // stronger alignment
        visual_features.lip_sync_score = 0.7f + (rng_() % 30) * 0.01f; // improved sync
        visual_features.motherese_face_boost = 0.7f + (rng_() % 20) * 0.01f; // more motherese
        visual_features.speech_vision_coupling = 0.9f;                 // stronger coupling
        
        // Generate face embedding (simplified)
        visual_features.face_embedding.resize(128);
        std::uniform_real_distribution<float> embedding_dist(-1.0f, 1.0f);
        for (auto& val : visual_features.face_embedding) {
            val = embedding_dist(rng_);
        }
        
        // Generate gaze and lip features
        visual_features.gaze_vector = {0.0f, 0.0f}; // Direct gaze
        visual_features.lip_features.resize(16);
        for (auto& val : visual_features.lip_features) {
            val = embedding_dist(rng_) * 0.5f; // Moderate lip movement
        }
        
        // Process face-speech event
        language_system_->processFaceSpeechEvent(
            visual_features.face_embedding,
            visual_features.gaze_vector,
            visual_features.lip_features,
            word,
            0.9f
        );
    }
    
    std::vector<float> generateSyntheticAudio(const std::string& phonemes, float base_pitch, bool motherese = false) {
        const float sample_rate = 16000.0f;
        const float duration = 0.5f;
        const std::size_t num_samples = static_cast<std::size_t>(duration * sample_rate);
        
        std::vector<float> audio(num_samples, 0.0f);
        
        float pitch = base_pitch;
        if (motherese) {
            pitch *= 1.3f; // Higher pitch for motherese
        }
        
        for (std::size_t i = 0; i < num_samples; ++i) {
            float t = i / sample_rate;
            
            // Add pitch modulation for motherese
            float pitch_mod = motherese ? (1.0f + 0.2f * std::sin(2.0f * M_PI * 3.0f * t)) : 1.0f;
            float freq = pitch * pitch_mod;
            
            // Generate harmonic series
            float sample = 0.0f;
            sample += 0.5f * std::sin(2.0f * M_PI * freq * t);           // Fundamental
            sample += 0.3f * std::sin(2.0f * M_PI * freq * 2.0f * t);    // 2nd harmonic
            sample += 0.2f * std::sin(2.0f * M_PI * freq * 3.0f * t);    // 3rd harmonic
            
            // Add formants for vowel-like sounds
            if (phonemes.find('a') != std::string::npos) {
                sample += 0.2f * std::sin(2.0f * M_PI * 700.0f * t);  // F1
                sample += 0.1f * std::sin(2.0f * M_PI * 1200.0f * t); // F2
            }
            
            // Apply envelope
            float envelope = std::exp(-t * 1.5f);
            audio[i] = sample * envelope * 0.3f;
        }
        
        return audio;
    }
    
    void reportProgress(int step, LanguageSystem::DevelopmentalStage stage) {
        auto stats = language_system_->getStatistics();
        
        std::cout << "\n📊 Step " << step << " [" << stageToString(stage) << "]:" << std::endl;
        std::cout << "   Vocabulary: " << stats.active_vocabulary_size << " tokens" << std::endl;
        std::cout << "   Generated: " << stats.total_tokens_generated << " total" << std::endl;
        std::cout << "   Mimicry: " << stats.successful_mimicry_attempts << " attempts" << std::endl;
        std::cout << "   Grounding: " << stats.grounding_associations_formed << " associations" << std::endl;
        std::cout << "   Avg Activation: " << std::fixed << std::setprecision(3) << stats.average_token_activation << std::endl;
    }
    
    void reportFinalStatistics() {
        auto stats = language_system_->getStatistics();
        auto final_stage = language_system_->getCurrentStage();
        
        std::cout << "\n🎯 Final Developmental Assessment:" << std::endl;
        std::cout << "=================================" << std::endl;
        std::cout << "Final Stage: " << stageToString(final_stage) << std::endl;
        std::cout << "Vocabulary Size: " << stats.active_vocabulary_size << " tokens" << std::endl;
        std::cout << "Total Tokens Generated: " << stats.total_tokens_generated << std::endl;
        std::cout << "Successful Mimicry Attempts: " << stats.successful_mimicry_attempts << std::endl;
        std::cout << "Grounding Associations: " << stats.grounding_associations_formed << std::endl;
        std::cout << "Average Token Activation: " << std::fixed << std::setprecision(3) << stats.average_token_activation << std::endl;
        std::cout << "Vocabulary Diversity: " << std::fixed << std::setprecision(3) << stats.vocabulary_diversity << std::endl;
        
        // Calculate developmental progress
        float progress = calculateDevelopmentalProgress(final_stage, stats);
        std::cout << "Developmental Progress: " << std::fixed << std::setprecision(1) << progress * 100 << "%" << std::endl;
        
        std::cout << "\n📈 Trajectory Analysis:" << std::endl;
        std::cout << "Check 'developmental_demo_logs/' for detailed reports:" << std::endl;
        std::cout << "- token_trajectories.csv: Token evolution over time" << std::endl;
        std::cout << "- cluster_evolution.csv: Phoneme cluster formation" << std::endl;
        std::cout << "- developmental_report.md: Comprehensive analysis" << std::endl;
    }
    
    float calculateDevelopmentalProgress(LanguageSystem::DevelopmentalStage stage, const LanguageSystem::Statistics& stats) {
        float base_progress = static_cast<float>(static_cast<int>(stage)) / 6.0f; // 6 total stages
        
        // Add bonus for achievements within stage
        float achievement_bonus = 0.0f;
        achievement_bonus += std::min(0.1f, static_cast<float>(stats.active_vocabulary_size) / 100.0f);
        achievement_bonus += std::min(0.1f, static_cast<float>(stats.successful_mimicry_attempts) / 50.0f);
        achievement_bonus += std::min(0.1f, static_cast<float>(stats.grounding_associations_formed) / 20.0f);
        
        return std::min(1.0f, base_progress + achievement_bonus);
    }
    
    std::string stageToString(LanguageSystem::DevelopmentalStage stage) {
        switch (stage) {
            case LanguageSystem::DevelopmentalStage::Chaos: return "Chaos";
            case LanguageSystem::DevelopmentalStage::Babbling: return "Babbling";
            case LanguageSystem::DevelopmentalStage::Mimicry: return "Mimicry";
            case LanguageSystem::DevelopmentalStage::Grounding: return "Grounding";
            case LanguageSystem::DevelopmentalStage::Reflection: return "Reflection";
            case LanguageSystem::DevelopmentalStage::Communication: return "Communication";
            default: return "Unknown";
        }
    }
};

int main(int argc, char** argv) {
    try {
        std::cout << "🧠 NeuroForge Developmental Trajectory Tracking Demo" << std::endl;
        std::cout << "====================================================" << std::endl;
        std::cout << "This demo simulates infant-like language development" << std::endl;
        std::cout << "and tracks token association trajectories over time." << std::endl;
        
        // Allow CLI override for number of steps (default 150)
        int steps = 150;
        if (argc > 1) {
            try {
                steps = std::max(1, std::stoi(argv[1]));
            } catch (...) {
                std::cerr << "Warning: Invalid steps argument. Using default 150." << std::endl;
            }
        }

        DevelopmentalTrackingDemo demo;
        demo.runDevelopmentalSimulation(steps); // Run for provided steps
        
        std::cout << "\n🎉 Demo completed successfully!" << std::endl;
        std::cout << "Check the generated reports for detailed analysis." << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Demo failed with exception: " << e.what() << std::endl;
        return 1;
    }
}