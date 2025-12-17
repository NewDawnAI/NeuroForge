#include "core/LanguageSystem.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <fstream>
#include <iomanip>
#include <thread>

// Ensure M_PI is available across MSVC builds
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace NeuroForge::Core;

class SpeechProductionTestSuite {
private:
    std::unique_ptr<LanguageSystem> language_system_;
    std::mt19937 rng_;
    bool verbose_output_ = true;

public:
    SpeechProductionTestSuite(bool verbose = true) 
        : rng_(std::random_device{}()), verbose_output_(verbose) {
        
        // Configure for speech production
        LanguageSystem::Config config;
        config.enable_acoustic_preprocessing = true;
        config.enable_prosodic_embeddings = true;
        config.enable_vision_grounding = true;
        config.enable_face_language_bias = true;
        config.enable_speech_output = true;
        config.enable_lip_sync = true;
        config.enable_gaze_coordination = true;
        config.speech_production_rate = 1.0f;
        config.lip_sync_precision = 0.8f;
        config.gaze_coordination_strength = 0.6f;
        config.self_monitoring_weight = 0.4f;
        config.caregiver_mimicry_boost = 0.5f;
        
        language_system_ = std::make_unique<LanguageSystem>(config);
        language_system_->initialize();
    }

    bool testPhonemeSequenceGeneration() {
        std::cout << "Test 1: Phoneme Sequence Generation... ";
        
        try {
            std::string test_text = "mama";
            auto phonemes = language_system_->generatePhonemeSequence(test_text);
            
            if (phonemes.empty()) {
                std::cout << "FAILED (no phonemes generated)\n";
                return false;
            }
            
            // Check phoneme properties
            bool has_vowels = false;
            bool has_consonants = false;
            
            for (const auto& phoneme : phonemes) {
                if (phoneme.vowel_consonant_ratio > 0.5f) {
                    has_vowels = true;
                } else {
                    has_consonants = true;
                }
            }
            
            if (verbose_output_) {
                std::cout << "\n  Input text: \"" << test_text << "\"";
                std::cout << "\n  Generated phonemes: " << phonemes.size();
                std::cout << "\n  Phoneme sequence: ";
                for (const auto& phoneme : phonemes) {
                    std::cout << phoneme.phonetic_symbol << " ";
                }
                std::cout << "\n  Has vowels: " << (has_vowels ? "Yes" : "No");
                std::cout << "\n  Has consonants: " << (has_consonants ? "Yes" : "No");
            }
            
            bool success = has_vowels && has_consonants && phonemes.size() >= 2;
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testLipMotionGeneration() {
        std::cout << "Test 2: Lip Motion Generation... ";
        
        try {
            auto phonemes = language_system_->generatePhonemeSequence("hello");
            auto lip_motions = language_system_->generateLipMotionSequence(phonemes);
            
            if (lip_motions.size() != phonemes.size()) {
                std::cout << "FAILED (lip motion count mismatch)\n";
                return false;
            }
            
            // Check lip motion properties
            bool has_variation = false;
            float total_variation = 0.0f;
            
            for (const auto& lip_shape : lip_motions) {
                if (lip_shape.size() != 16) {
                    std::cout << "FAILED (incorrect lip shape dimension)\n";
                    return false;
                }
                
                // Calculate variation in lip shape
                float shape_variation = 0.0f;
                for (std::size_t i = 1; i < lip_shape.size(); ++i) {
                    shape_variation += std::abs(lip_shape[i] - lip_shape[i-1]);
                }
                total_variation += shape_variation;
                
                if (shape_variation > 0.1f) {
                    has_variation = true;
                }
            }
            
            if (verbose_output_) {
                std::cout << "\n  Phonemes: " << phonemes.size();
                std::cout << "\n  Lip motions: " << lip_motions.size();
                std::cout << "\n  Lip shape dimension: " << (lip_motions.empty() ? 0 : lip_motions[0].size());
                std::cout << "\n  Total variation: " << std::fixed << std::setprecision(3) << total_variation;
                std::cout << "\n  Has variation: " << (has_variation ? "Yes" : "No");
                
                // Show first lip shape
                if (!lip_motions.empty()) {
                    std::cout << "\n  First lip shape: [";
                    for (std::size_t i = 0; i < std::min(static_cast<std::size_t>(4), lip_motions[0].size()); ++i) {
                        std::cout << std::fixed << std::setprecision(2) << lip_motions[0][i];
                        if (i < 3) std::cout << ", ";
                    }
                    std::cout << "...]";
                }
            }
            
            bool success = has_variation && total_variation > 0.5f;
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testProsodyGeneration() {
        std::cout << "Test 3: Prosody Contour Generation... ";
        
        try {
            auto phonemes = language_system_->generatePhonemeSequence("mama");
            auto prosody = language_system_->generateProsodyContour(phonemes, 0.5f); // 50% emotional intensity
            
            if (prosody.size() != phonemes.size()) {
                std::cout << "FAILED (prosody count mismatch)\n";
                return false;
            }
            
            // Check prosody properties
            float min_pitch = *std::min_element(prosody.begin(), prosody.end());
            float max_pitch = *std::max_element(prosody.begin(), prosody.end());
            float pitch_range = max_pitch - min_pitch;
            
            // Calculate pitch variation
            float pitch_variation = 0.0f;
            for (std::size_t i = 1; i < prosody.size(); ++i) {
                pitch_variation += std::abs(prosody[i] - prosody[i-1]);
            }
            
            if (verbose_output_) {
                std::cout << "\n  Phonemes: " << phonemes.size();
                std::cout << "\n  Prosody points: " << prosody.size();
                std::cout << "\n  Pitch range: " << std::fixed << std::setprecision(1) 
                         << min_pitch << " - " << max_pitch << " Hz";
                std::cout << "\n  Pitch variation: " << std::fixed << std::setprecision(2) << pitch_variation;
                std::cout << "\n  Prosody contour: [";
                for (std::size_t i = 0; i < prosody.size(); ++i) {
                    std::cout << std::fixed << std::setprecision(0) << prosody[i];
                    if (i < prosody.size() - 1) std::cout << ", ";
                }
                std::cout << "]";
            }
            
            bool success = pitch_range > 20.0f && pitch_variation > 10.0f; // Reasonable variation
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testSpeechProductionFeatures() {
        std::cout << "Test 4: Speech Production Features Generation... ";
        
        try {
            std::string test_text = "hello mama";
            auto speech_features = language_system_->generateSpeechOutput(test_text);
            
            // Check all components are generated
            bool has_phonemes = !speech_features.phoneme_sequence.empty();
            bool has_timing = !speech_features.timing_pattern.empty();
            bool has_prosody = !speech_features.prosody_contour.empty();
            bool has_lip_motion = !speech_features.lip_motion_sequence.empty();
            bool has_gaze = !speech_features.gaze_targets.empty();
            
            // Check consistency
            bool consistent_sizes = (speech_features.phoneme_sequence.size() == speech_features.timing_pattern.size()) &&
                                   (speech_features.phoneme_sequence.size() == speech_features.prosody_contour.size()) &&
                                   (speech_features.phoneme_sequence.size() == speech_features.lip_motion_sequence.size());
            
            if (verbose_output_) {
                std::cout << "\n  Input: \"" << test_text << "\"";
                std::cout << "\n  Phonemes: " << speech_features.phoneme_sequence.size();
                std::cout << "\n  Timing pattern: " << speech_features.timing_pattern.size();
                std::cout << "\n  Prosody contour: " << speech_features.prosody_contour.size();
                std::cout << "\n  Lip motions: " << speech_features.lip_motion_sequence.size();
                std::cout << "\n  Gaze targets: " << speech_features.gaze_targets.size();
                std::cout << "\n  Speech rate: " << speech_features.speech_rate;
                std::cout << "\n  Confidence: " << std::fixed << std::setprecision(2) << speech_features.confidence_score;
                std::cout << "\n  Consistent sizes: " << (consistent_sizes ? "Yes" : "No");
            }
            
            bool success = has_phonemes && has_timing && has_prosody && has_lip_motion && consistent_sizes;
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testSpeechProductionControl() {
        std::cout << "Test 5: Speech Production Control... ";
        
        try {
            auto speech_features = language_system_->generateSpeechOutput("test");
            
            // Test starting speech production
            language_system_->startSpeechProduction(speech_features);
            auto initial_state = language_system_->getCurrentSpeechState();
            
            if (!initial_state.is_speaking) {
                std::cout << "FAILED (speech not started)\n";
                return false;
            }
            
            // Test updating speech production
            language_system_->updateSpeechProduction(0.1f); // 100ms update
            auto updated_state = language_system_->getCurrentSpeechState();
            
            // Test stopping speech production
            language_system_->stopSpeechProduction();
            auto final_state = language_system_->getCurrentSpeechState();
            
            if (verbose_output_) {
                std::cout << "\n  Initial state - Speaking: " << (initial_state.is_speaking ? "Yes" : "No");
                std::cout << "\n  Initial phoneme index: " << initial_state.current_phoneme_index;
                std::cout << "\n  Updated time offset: " << std::fixed << std::setprecision(1) 
                         << updated_state.current_time_offset;
                std::cout << "\n  Final state - Speaking: " << (final_state.is_speaking ? "Yes" : "No");
                std::cout << "\n  Lip shape size: " << initial_state.current_lip_shape.size();
                std::cout << "\n  Gaze direction size: " << initial_state.current_gaze_direction.size();
            }
            
            bool success = initial_state.is_speaking && !final_state.is_speaking &&
                          !initial_state.current_lip_shape.empty();
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testSelfMonitoring() {
        std::cout << "Test 6: Self-Monitoring and Feedback... ";
        
        try {
            auto speech_features = language_system_->generateSpeechOutput("mama");
            language_system_->startSpeechProduction(speech_features);
            
            // Generate synthetic acoustic feedback
            std::vector<float> acoustic_feedback(1600, 0.0f); // 100ms at 16kHz
            for (std::size_t i = 0; i < acoustic_feedback.size(); ++i) {
                float t = i / 16000.0f;
                acoustic_feedback[i] = 0.5f * std::sin(2.0f * M_PI * 150.0f * t); // 150Hz tone
            }
            
            auto initial_state = language_system_->getCurrentSpeechState();
            float initial_monitoring_score = initial_state.self_monitoring_score;
            
            // Process self-acoustic feedback
            language_system_->processSelfAcousticFeedback(acoustic_feedback);
            
            auto updated_state = language_system_->getCurrentSpeechState();
            float updated_monitoring_score = updated_state.self_monitoring_score;
            
            // Test caregiver response
            LanguageSystem::VisualLanguageFeatures caregiver_reaction;
            caregiver_reaction.face_salience = 0.8f;
            caregiver_reaction.gaze_alignment = 0.9f;
            caregiver_reaction.lip_sync_score = 0.7f;
            caregiver_reaction.attention_focus = 0.8f;  // Required for caregiver attention detection
            caregiver_reaction.motherese_face_boost = 0.6f;  // Additional field for better response
            
            LanguageSystem::AcousticFeatures caregiver_audio;
            caregiver_audio.energy_envelope = 0.6f;
            caregiver_audio.motherese_score = 0.8f;
            
            language_system_->processCaregiverResponse(caregiver_reaction, caregiver_audio);
            
            auto final_state = language_system_->getCurrentSpeechState();
            
            if (verbose_output_) {
                std::cout << "\n  Acoustic feedback size: " << acoustic_feedback.size();
                std::cout << "\n  Initial monitoring score: " << std::fixed << std::setprecision(3) 
                         << initial_monitoring_score;
                std::cout << "\n  Updated monitoring score: " << std::fixed << std::setprecision(3) 
                         << updated_monitoring_score;
                std::cout << "\n  Caregiver attention detected: " 
                         << (final_state.caregiver_attention_detected ? "Yes" : "No");
                std::cout << "\n  Final monitoring score: " << std::fixed << std::setprecision(3) 
                         << final_state.self_monitoring_score;
            }
            
            language_system_->stopSpeechProduction();
            
            bool success = updated_monitoring_score >= initial_monitoring_score &&
                          final_state.caregiver_attention_detected;
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testCaregiverMimicryReinforcement() {
        std::cout << "Test 7: Caregiver Mimicry Reinforcement... ";
        
        try {
            // Create token for testing
            std::size_t mama_token = language_system_->createToken("mama", LanguageSystem::TokenType::Word);
            auto initial_token = language_system_->getToken(mama_token);
            float initial_activation = initial_token->activation_strength;
            auto initial_stats = language_system_->getStatistics();
            
            // Create caregiver features indicating successful mimicry
            LanguageSystem::VisualLanguageFeatures caregiver_features;
            caregiver_features.face_salience = 0.9f;
            caregiver_features.gaze_alignment = 0.8f;
            caregiver_features.lip_sync_score = 0.7f;
            caregiver_features.motherese_face_boost = 0.6f;
            caregiver_features.speech_vision_coupling = 0.9f;
            
            // Process caregiver mimicry reinforcement
            language_system_->reinforceCaregiverMimicry("mama", caregiver_features);
            
            auto final_token = language_system_->getToken(mama_token);
            float final_activation = final_token->activation_strength;
            auto final_stats = language_system_->getStatistics();
            
            if (verbose_output_) {
                std::cout << "\n  Initial activation: " << std::fixed << std::setprecision(3) << initial_activation;
                std::cout << "\n  Final activation: " << std::fixed << std::setprecision(3) << final_activation;
                std::cout << "\n  Activation increase: " << std::fixed << std::setprecision(3) 
                         << (final_activation - initial_activation);
                std::cout << "\n  Usage count: " << final_token->usage_count;
                std::cout << "\n  Mimicry attempts: " << initial_stats.successful_mimicry_attempts 
                         << " -> " << final_stats.successful_mimicry_attempts;
                std::cout << "\n  Face salience: " << caregiver_features.face_salience;
                std::cout << "\n  Lip sync score: " << caregiver_features.lip_sync_score;
            }
            
            bool success = final_activation > initial_activation &&
                          final_stats.successful_mimicry_attempts > initial_stats.successful_mimicry_attempts;
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testJointAttentionLearning() {
        std::cout << "Test 8: Joint Attention Learning... ";
        
        try {
            std::vector<float> shared_gaze_target = {0.3f, 0.7f}; // Gaze coordinates
            std::string spoken_token = "ball";
            
            auto initial_vocab_size = language_system_->getActiveVocabulary().size();
            auto initial_stats = language_system_->getStatistics();
            
            // Process joint attention event
            language_system_->processJointAttentionEvent(shared_gaze_target, spoken_token);
            
            auto final_vocab_size = language_system_->getActiveVocabulary().size();
            auto final_stats = language_system_->getStatistics();
            
            // Check if token was created/enhanced
            auto ball_token = language_system_->getToken(spoken_token);
            
            if (verbose_output_) {
                std::cout << "\n  Shared gaze target: [" << shared_gaze_target[0] 
                         << ", " << shared_gaze_target[1] << "]";
                std::cout << "\n  Spoken token: \"" << spoken_token << "\"";
                std::cout << "\n  Vocabulary size: " << initial_vocab_size << " -> " << final_vocab_size;
                std::cout << "\n  Grounding associations: " << initial_stats.grounding_associations_formed 
                         << " -> " << final_stats.grounding_associations_formed;
                if (ball_token) {
                    std::cout << "\n  Token activation: " << std::fixed << std::setprecision(3) 
                             << ball_token->activation_strength;
                    std::cout << "\n  Joint attention X: " 
                             << ball_token->sensory_associations.at("joint_attention_x");
                    std::cout << "\n  Joint attention Y: " 
                             << ball_token->sensory_associations.at("joint_attention_y");
                }
            }
            
            bool success = ball_token && ball_token->activation_strength > 0.5f &&
                          final_stats.grounding_associations_formed > initial_stats.grounding_associations_formed;
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    void runAllTests() {
        std::cout << "=== NeuroForge Speech Production and Multimodal Output Tests ===\n\n";
        
        int passed = 0;
        int total = 8;
        
        if (testPhonemeSequenceGeneration()) passed++;
        if (testLipMotionGeneration()) passed++;
        if (testProsodyGeneration()) passed++;
        if (testSpeechProductionFeatures()) passed++;
        if (testSpeechProductionControl()) passed++;
        if (testSelfMonitoring()) passed++;
        if (testCaregiverMimicryReinforcement()) passed++;
        if (testJointAttentionLearning()) passed++;
        
        std::cout << "\n=== Test Results ===\n";
        std::cout << "Passed: " << passed << "/" << total << " tests\n";
        std::cout << "Success Rate: " << std::fixed << std::setprecision(1) 
                  << (100.0f * passed / total) << "%\n";
        
        if (passed == total) {
            std::cout << "🎉 All tests passed! Speech production system is working correctly.\n";
            std::cout << "✅ Phoneme sequence generation functional\n";
            std::cout << "✅ Lip-sync motion generation active\n";
            std::cout << "✅ Prosody contour generation working\n";
            std::cout << "✅ Speech production control operational\n";
            std::cout << "✅ Self-monitoring and feedback enabled\n";
            std::cout << "✅ Caregiver mimicry reinforcement active\n";
            std::cout << "✅ Joint attention learning functional\n";
            std::cout << "🚀 Ready for multimodal speech output integration!\n";
        } else {
            std::cout << "⚠️  Some tests failed. Check implementation details.\n";
        }
    }
};

int main() {
    try {
        SpeechProductionTestSuite test_suite(true);
        test_suite.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test suite failed with exception: " << e.what() << std::endl;
        return 1;
    }
}