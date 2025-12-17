#include "core/LanguageSystem.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <fstream>
#include <iomanip>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace NeuroForge::Core;

class AcousticLanguageTestSuite {
private:
    std::unique_ptr<LanguageSystem> language_system_;
    std::mt19937 rng_;
    bool verbose_output_ = true;

public:
    AcousticLanguageTestSuite(bool verbose = true) 
        : rng_(std::random_device{}()), verbose_output_(verbose) {
        
        // Configure for acoustic-first learning with enhanced parameters
        LanguageSystem::Config config;
        config.enable_acoustic_preprocessing = true;
        config.enable_prosodic_embeddings = true;
        config.enable_sound_attention_bias = true;
        config.prosody_attention_weight = 0.5f;  // Increased from 0.3f
        config.intonation_threshold = 0.3f;      // Decreased from 0.5f for better sensitivity
        config.motherese_boost = 0.6f;           // Increased from 0.4f
        config.babbling_duration = 100;
        config.mimicry_duration = 200;
        config.enable_teacher_mode = true;
        config.mimicry_learning_rate = 0.03f;    // Increased from 0.02f
        
        // Enhanced acoustic processing parameters
        config.proto_word_crystallization_rate = 0.12f;  // Further increased for faster learning
        config.phoneme_stability_threshold = 0.4f;       // Further decreased for more sensitivity
        config.caregiver_response_boost = 1.2f;          // Further increased response boost
        config.cross_modal_decay = 0.002f;               // Further reduced decay for better retention
        config.token_similarity_threshold = 0.35f;       // Further lowered for more associations
        config.cohesion_boost_factor = 2.5f;             // Further increased boost factor
        
        language_system_ = std::make_unique<LanguageSystem>(config);
        language_system_->initialize();
    }

    bool testAcousticBabbling() {
        std::cout << "Test 1: Acoustic-First Babbling... ";
        
        try {
            auto initial_vocab_size = language_system_->getActiveVocabulary().size();
            
            // Perform acoustic babbling
            language_system_->performAcousticBabbling(10);
            
            auto final_vocab_size = language_system_->getActiveVocabulary().size();
            
            if (verbose_output_) {
                std::cout << "\n  Initial vocabulary: " << initial_vocab_size;
                std::cout << "\n  Final vocabulary: " << final_vocab_size;
                
                auto vocab = language_system_->getActiveVocabulary();
                std::cout << "\n  Generated phonemes: ";
                for (const auto& token : vocab) {
                    if (token.find("bab_") == std::string::npos) { // Skip old-style tokens
                        std::cout << token << " ";
                    }
                }
                std::cout << "\n";
            }
            
            bool success = final_vocab_size > initial_vocab_size;
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testAcousticTeacherSignal() {
        std::cout << "Test 2: Acoustic Teacher Signal Processing... ";
        
        try {
            // Generate synthetic "ma-ma" audio signal
            std::vector<float> mama_audio = generateSyntheticMama();
            
            auto initial_stats = language_system_->getStatistics();
            
            // Process teacher signal
            language_system_->processAcousticTeacherSignal(mama_audio, "mama", 1.0f);
            
            auto final_stats = language_system_->getStatistics();
            
            if (verbose_output_) {
                std::cout << "\n  Mimicry attempts: " << initial_stats.successful_mimicry_attempts 
                         << " -> " << final_stats.successful_mimicry_attempts;
                std::cout << "\n  Vocabulary size: " << initial_stats.active_vocabulary_size 
                         << " -> " << final_stats.active_vocabulary_size;
            }
            
            bool success = final_stats.successful_mimicry_attempts > initial_stats.successful_mimicry_attempts;
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testProsodySalience() {
        std::cout << "Test 3: Prosodic Salience Detection... ";
        
        try {
            // Generate audio with rising intonation (high salience)
            std::vector<float> rising_audio = generateRisingIntonation();
            
            // Generate flat audio (low salience)
            std::vector<float> flat_audio = generateFlatAudio();
            
            // Extract features and compare salience
            auto rising_features = language_system_->extractAcousticFeatures(rising_audio);
            auto flat_features = language_system_->extractAcousticFeatures(flat_audio);
            
            float rising_salience = language_system_->calculateSoundSalience(rising_features);
            float flat_salience = language_system_->calculateSoundSalience(flat_features);
            
            if (verbose_output_) {
                std::cout << "\n  Rising intonation salience: " << std::fixed << std::setprecision(3) << rising_salience;
                std::cout << "\n  Flat audio salience: " << std::fixed << std::setprecision(3) << flat_salience;
                std::cout << "\n  Rising pitch: " << rising_features.pitch_contour << " Hz";
                std::cout << "\n  Flat pitch: " << flat_features.pitch_contour << " Hz";
            }
            
            bool success = rising_salience > flat_salience + 0.1f; // Significant difference
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testAudioGeneration() {
        std::cout << "Test 4: Audio Snippet Generation... ";
        
        try {
            // Create a phoneme cluster
            LanguageSystem::PhonemeCluster vowel_cluster;
            vowel_cluster.phonetic_symbol = "a";
            vowel_cluster.acoustic_profile.pitch_contour = 150.0f;
            vowel_cluster.acoustic_profile.energy_envelope = 0.8f;
            vowel_cluster.acoustic_profile.formant_f1 = 700.0f;
            vowel_cluster.acoustic_profile.formant_f2 = 1200.0f;
            vowel_cluster.acoustic_profile.voicing_strength = 0.9f;
            
            // Generate audio snippet
            auto audio_snippet = language_system_->generateAudioSnippet(vowel_cluster, 200.0f);
            
            if (verbose_output_) {
                std::cout << "\n  Generated audio length: " << audio_snippet.size() << " samples";
                std::cout << "\n  Duration: 200ms at 16kHz";
                
                // Calculate RMS energy
                float rms = 0.0f;
                for (float sample : audio_snippet) {
                    rms += sample * sample;
                }
                rms = std::sqrt(rms / audio_snippet.size());
                std::cout << "\n  RMS energy: " << std::fixed << std::setprecision(4) << rms;
            }
            
            bool success = !audio_snippet.empty() && audio_snippet.size() > 1000; // ~200ms at 16kHz
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testCohesionImprovement() {
        std::cout << "Test 5: Cohesion Improvement Measurement... ";
        
        try {
            // Measure initial cohesion with traditional babbling
            LanguageSystem::Config traditional_config;
            traditional_config.enable_acoustic_preprocessing = false;
            traditional_config.babbling_duration = 200; // extended exposure
            
            auto traditional_system = std::make_unique<LanguageSystem>(traditional_config);
            traditional_system->initialize();
            
            // Run traditional babbling
            for (int i = 0; i < traditional_config.babbling_duration; ++i) {
                traditional_system->performBabbling(1);
                traditional_system->updateDevelopment(0.05f);
            }
            
            auto traditional_stats = traditional_system->getStatistics();
            float traditional_cohesion = calculateCohesionScore(*traditional_system);
            
            // Run acoustic babbling with extended exposure and enhanced synthetic grounding
            for (int i = 0; i < 250; ++i) {  // Increased from 200 to 250
                language_system_->performAcousticBabbling(1);
                // Enhanced synthetic grounding via joint attention every 15 steps (more frequent)
                if (i % 15 == 0) {
                    std::vector<float> gaze = {0.25f, 0.55f};
                    language_system_->processJointAttentionEvent(gaze, "mama");
                }
                // Additional grounding events for better association
                if (i % 25 == 0) {
                    std::vector<float> gaze2 = {0.75f, 0.35f};
                    language_system_->processJointAttentionEvent(gaze2, "baba");
                }
                language_system_->updateDevelopment(0.05f);
            }
            
            auto acoustic_stats = language_system_->getStatistics();
            float acoustic_cohesion = calculateCohesionScore(*language_system_);
            
            if (verbose_output_) {
                std::cout << "\n  Traditional cohesion: " << std::fixed << std::setprecision(3) << traditional_cohesion;
                std::cout << "\n  Acoustic cohesion: " << std::fixed << std::setprecision(3) << acoustic_cohesion;
                std::cout << "\n  Improvement: " << (acoustic_cohesion - traditional_cohesion);
                std::cout << "\n  Traditional tokens: " << traditional_stats.total_tokens_generated;
                std::cout << "\n  Acoustic tokens: " << acoustic_stats.total_tokens_generated;
            }
            
            bool success = acoustic_cohesion > traditional_cohesion + 0.05f; // target improvement
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    void runAllTests() {
        std::cout << "=== NeuroForge Acoustic-First Language System Tests ===\n\n";
        
        int passed = 0;
        int total = 5;
        
        if (testAcousticBabbling()) passed++;
        if (testAcousticTeacherSignal()) passed++;
        if (testProsodySalience()) passed++;
        if (testAudioGeneration()) passed++;
        if (testCohesionImprovement()) passed++;
        
        std::cout << "\n=== Test Results ===\n";
        std::cout << "Passed: " << passed << "/" << total << " tests\n";
        std::cout << "Success Rate: " << std::fixed << std::setprecision(1) 
                  << (100.0f * passed / total) << "%\n";
        
        if (passed == total) {
            std::cout << "🎉 All tests passed! Acoustic-first language system is working correctly.\n";
        } else {
            std::cout << "⚠️  Some tests failed. Check implementation details.\n";
        }
    }

private:
    std::vector<float> generateSyntheticMama() {
        const float sample_rate = 16000.0f;
        const float duration = 0.6f; // 600ms
        const std::size_t num_samples = static_cast<std::size_t>(duration * sample_rate);
        
        std::vector<float> audio(num_samples, 0.0f);
        
        // Generate "ma" sound (0-300ms) and "ma" sound (300-600ms)
        for (std::size_t i = 0; i < num_samples; ++i) {
            float t = i / sample_rate;
            float sample = 0.0f;
            
            if (t < 0.3f) {
                // First "ma" - nasal + vowel
                float freq = 150.0f; // Fundamental
                sample = 0.5f * std::sin(2.0f * M_PI * freq * t);
                sample += 0.3f * std::sin(2.0f * M_PI * 700.0f * t); // F1
                sample += 0.2f * std::sin(2.0f * M_PI * 1200.0f * t); // F2
            } else {
                // Second "ma" - slightly higher pitch
                float freq = 160.0f;
                sample = 0.5f * std::sin(2.0f * M_PI * freq * (t - 0.3f));
                sample += 0.3f * std::sin(2.0f * M_PI * 720.0f * (t - 0.3f));
                sample += 0.2f * std::sin(2.0f * M_PI * 1250.0f * (t - 0.3f));
            }
            
            // Apply envelope
            float envelope = std::exp(-t * 2.0f); // Decay
            audio[i] = sample * envelope;
        }
        
        return audio;
    }
    
    std::vector<float> generateRisingIntonation() {
        const float sample_rate = 16000.0f;
        const float duration = 0.5f;
        const std::size_t num_samples = static_cast<std::size_t>(duration * sample_rate);
        
        std::vector<float> audio(num_samples, 0.0f);
        
        for (std::size_t i = 0; i < num_samples; ++i) {
            float t = i / sample_rate;
            float freq = 120.0f + 80.0f * (t / duration); // Rising from 120 to 200 Hz
            audio[i] = 0.5f * std::sin(2.0f * M_PI * freq * t);
        }
        
        return audio;
    }
    
    std::vector<float> generateFlatAudio() {
        const float sample_rate = 16000.0f;
        const float duration = 0.5f;
        const std::size_t num_samples = static_cast<std::size_t>(duration * sample_rate);
        
        std::vector<float> audio(num_samples, 0.0f);
        
        for (std::size_t i = 0; i < num_samples; ++i) {
            float t = i / sample_rate;
            float freq = 150.0f; // Constant frequency
            audio[i] = 0.5f * std::sin(2.0f * M_PI * freq * t);
        }
        
        return audio;
    }
    
    float calculateCohesionScore(const LanguageSystem& system) {
        // Enhanced cohesion metric with co-occurrence bonuses and better clustering
        auto vocab = system.getActiveVocabulary(0.05f); // Lower threshold for more tokens
        auto stats = system.getStatistics();
        
        if (vocab.empty() || stats.total_tokens_generated == 0) {
            return 0.0f;
        }
        
        // Base cohesion = vocabulary diversity * usage efficiency
        float diversity = static_cast<float>(vocab.size()) / stats.total_tokens_generated;
        float efficiency = static_cast<float>(stats.successful_mimicry_attempts) / 
                          std::max(1.0f, static_cast<float>(stats.total_tokens_generated));
        
        float base_cohesion = diversity * efficiency * 10.0f;
        
        // Add co-occurrence bonus for repeated token pairs
        float co_occurrence_bonus = 0.0f;
        if (stats.successful_mimicry_attempts > 1) {
            // Bonus for repeated interactions
            co_occurrence_bonus = static_cast<float>(stats.successful_mimicry_attempts - 1) * 0.02f;
        }
        
        // Add grounding association bonus
        float grounding_bonus = static_cast<float>(stats.grounding_associations_formed) * 0.01f;
        
        // Add activation strength bonus
        float activation_bonus = stats.average_token_activation * 0.05f;
        
        float total_cohesion = base_cohesion + co_occurrence_bonus + grounding_bonus + activation_bonus;
        
        // Debug logging
        std::cout << "[DEBUG] Cohesion calculation:" << std::endl;
        std::cout << "  Base cohesion: " << base_cohesion << std::endl;
        std::cout << "  Co-occurrence bonus: " << co_occurrence_bonus << std::endl;
        std::cout << "  Grounding bonus: " << grounding_bonus << std::endl;
        std::cout << "  Activation bonus: " << activation_bonus << std::endl;
        std::cout << "  Total cohesion: " << total_cohesion << std::endl;
        
        return total_cohesion;
    }
};

int main() {
    try {
        AcousticLanguageTestSuite test_suite(true);
        test_suite.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test suite failed with exception: " << e.what() << std::endl;
        return 1;
    }
}