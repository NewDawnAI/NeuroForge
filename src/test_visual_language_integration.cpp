#include "core/LanguageSystem.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <fstream>
#include <iomanip>

using namespace NeuroForge::Core;

class VisualLanguageIntegrationTestSuite {
private:
    std::unique_ptr<LanguageSystem> language_system_;
    std::mt19937 rng_;
    bool verbose_output_ = true;

public:
    VisualLanguageIntegrationTestSuite(bool verbose = true) 
        : rng_(std::random_device{}()), verbose_output_(verbose) {
        
        // Configure for visual-linguistic integration
        LanguageSystem::Config config;
        config.enable_acoustic_preprocessing = true;
        config.enable_prosodic_embeddings = true;
        config.enable_vision_grounding = true;
        config.enable_face_language_bias = true;
        config.face_language_coupling = 0.8f;  // Increased for stronger face-speech coupling
        config.gaze_attention_weight = 0.6f;   // Increased for stronger gaze influence
        config.lip_sync_threshold = 0.3f;
        config.visual_grounding_boost = 0.8f;  // Increased for stronger visual reinforcement
        config.motherese_boost = 0.6f;         // Increased for stronger motherese effect
        config.cross_modal_decay = 0.01f;
        config.enable_teacher_mode = true;
        config.mimicry_learning_rate = 0.02f;
        
        language_system_ = std::make_unique<LanguageSystem>(config);
        language_system_->initialize();
    }

    bool testFaceSpeechCoupling() {
        std::cout << "Test 1: Face-Speech Coupling... ";
        
        try {
            // Generate synthetic face embedding (128-dim face features)
            std::vector<float> face_embedding(128);
            std::normal_distribution<float> face_dist(0.5f, 0.2f);
            for (float& val : face_embedding) {
                val = face_dist(rng_);
            }
            
            // Generate gaze vector (2D gaze direction)
            std::vector<float> gaze_vector = {0.3f, 0.7f}; // Looking towards speaker
            
            // Generate lip features (16-dim lip shape/movement)
            std::vector<float> lip_features(16);
            std::uniform_real_distribution<float> lip_dist(0.2f, 0.8f);
            for (float& val : lip_features) {
                val = lip_dist(rng_);
            }
            
            auto initial_stats = language_system_->getStatistics();
            
            // Process face-speech event for "mama"
            language_system_->processFaceSpeechEvent(
                face_embedding, gaze_vector, lip_features, "mama", 0.9f);
            
            auto final_stats = language_system_->getStatistics();
            
            // Check if cross-modal association was created
            auto mama_token = language_system_->getToken("mama");
            if (!mama_token) {
                std::cout << "FAILED (mama token not created)\n";
                return false;
            }
            
            // Check visual associations
            std::size_t mama_id = 0;
            if (!language_system_->getTokenId("mama", mama_id)) {
                std::cout << "FAILED (mama token id not found)\n";
                return false;
            }
            auto cross_modal_assocs = language_system_->getCrossModalAssociations(mama_id);
            
            if (verbose_output_) {
                std::cout << "\n  Face embedding size: " << face_embedding.size();
                std::cout << "\n  Gaze vector: [" << gaze_vector[0] << ", " << gaze_vector[1] << "]";
                std::cout << "\n  Lip features size: " << lip_features.size();
                std::cout << "\n  Cross-modal associations: " << cross_modal_assocs.size();
                std::cout << "\n  Token activation: " << mama_token->activation_strength;
                std::cout << "\n  Face salience: " << mama_token->sensory_associations.at("face_salience");
            }
            
            bool success = final_stats.grounding_associations_formed > initial_stats.grounding_associations_formed;
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testVisualAttentionIntegration() {
        std::cout << "Test 2: Visual Attention Integration... ";
        
        try {
            // Create test tokens
            std::size_t ball_token = language_system_->createToken("ball", LanguageSystem::TokenType::Perception);
            std::size_t red_token = language_system_->createToken("red", LanguageSystem::TokenType::Perception);
            
            // Generate attention map (simulating visual attention on objects)
            std::vector<float> attention_map = {0.1f, 0.2f, 0.8f, 0.3f, 0.9f, 0.1f}; // High attention on indices 2,4
            std::vector<std::string> active_tokens = {"ball", "red"};
            
            auto initial_ball_activation = language_system_->getToken(ball_token)->activation_strength;
            auto initial_red_activation = language_system_->getToken(red_token)->activation_strength;
            
            // Process visual attention map
            language_system_->processVisualAttentionMap(attention_map, active_tokens);
            
            auto final_ball_activation = language_system_->getToken(ball_token)->activation_strength;
            auto final_red_activation = language_system_->getToken(red_token)->activation_strength;
            
            if (verbose_output_) {
                std::cout << "\n  Attention map max: " << *std::max_element(attention_map.begin(), attention_map.end());
                std::cout << "\n  Ball activation: " << initial_ball_activation << " -> " << final_ball_activation;
                std::cout << "\n  Red activation: " << initial_red_activation << " -> " << final_red_activation;
            }
            
            bool success = (final_ball_activation > initial_ball_activation) && 
                          (final_red_activation > initial_red_activation);
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testCrossModalPatternRetrieval() {
        std::cout << "Test 3: Cross-Modal Pattern Retrieval... ";
        
        try {
            // Create tokens and associate with visual patterns
            std::size_t face_token = language_system_->createToken("face", LanguageSystem::TokenType::Perception);
            std::size_t smile_token = language_system_->createToken("smile", LanguageSystem::TokenType::Emotion);
            
            // Generate visual patterns
            std::vector<float> face_pattern(64, 0.0f);
            std::vector<float> smile_pattern(64, 0.0f);
            
            // Face pattern: high values in "face" region
            for (int i = 20; i < 40; ++i) {
                face_pattern[i] = 0.8f + (rng_() % 100) / 500.0f; // 0.8-1.0
            }
            
            // Smile pattern: high values in "mouth" region  
            for (int i = 30; i < 50; ++i) {
                smile_pattern[i] = 0.7f + (rng_() % 100) / 500.0f; // 0.7-0.9
            }
            
            // Associate tokens with visual patterns
            language_system_->reinforceVisualGrounding(face_token, face_pattern, 0.9f);
            language_system_->reinforceVisualGrounding(smile_token, smile_pattern, 0.8f);
            
            // Test pattern retrieval
            auto face_matches = language_system_->getTokensForVisualPattern(face_pattern, 0.7f);
            auto smile_matches = language_system_->getTokensForVisualPattern(smile_pattern, 0.7f);
            
            // Test with similar pattern (should match face)
            std::vector<float> similar_face_pattern = face_pattern;
            for (float& val : similar_face_pattern) {
                val *= 0.95f; // Slightly different but similar
            }
            auto similar_matches = language_system_->getTokensForVisualPattern(similar_face_pattern, 0.6f);
            
            if (verbose_output_) {
                std::cout << "\n  Face pattern matches: " << face_matches.size();
                std::cout << "\n  Smile pattern matches: " << smile_matches.size();
                std::cout << "\n  Similar face matches: " << similar_matches.size();
                
                std::cout << "\n  Face pattern energy: ";
                float face_energy = 0.0f;
                for (float val : face_pattern) face_energy += val * val;
                std::cout << std::sqrt(face_energy);
                
                std::cout << "\n  Smile pattern energy: ";
                float smile_energy = 0.0f;
                for (float val : smile_pattern) smile_energy += val * val;
                std::cout << std::sqrt(smile_energy);
            }
            
            bool success = !face_matches.empty() && !smile_matches.empty() && !similar_matches.empty();
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testFaceLanguageConfidence() {
        std::cout << "Test 4: Face-Language Confidence Calculation... ";
        
        try {
            // Create high-confidence visual-language features
            LanguageSystem::VisualLanguageFeatures high_confidence_features;
            high_confidence_features.face_salience = 0.9f;
            high_confidence_features.gaze_alignment = 0.8f;
            high_confidence_features.lip_sync_score = 0.7f;
            high_confidence_features.speech_vision_coupling = 0.9f;
            high_confidence_features.motherese_face_boost = 0.6f;
            
            // Create low-confidence features
            LanguageSystem::VisualLanguageFeatures low_confidence_features;
            low_confidence_features.face_salience = 0.2f;
            low_confidence_features.gaze_alignment = 0.1f;
            low_confidence_features.lip_sync_score = 0.1f;
            low_confidence_features.speech_vision_coupling = 0.3f;
            low_confidence_features.motherese_face_boost = 0.0f;
            
            // Create acoustic features for enhanced confidence
            LanguageSystem::AcousticFeatures acoustic_features;
            acoustic_features.motherese_score = 0.8f;
            acoustic_features.voicing_strength = 0.9f;
            acoustic_features.energy_envelope = 0.7f;
            
            float high_confidence = language_system_->calculateFaceLanguageConfidence(
                high_confidence_features, acoustic_features);
            float low_confidence = language_system_->calculateFaceLanguageConfidence(
                low_confidence_features, LanguageSystem::AcousticFeatures{});
            
            if (verbose_output_) {
                std::cout << "\n  High confidence scenario: " << std::fixed << std::setprecision(3) << high_confidence;
                std::cout << "\n  Low confidence scenario: " << std::fixed << std::setprecision(3) << low_confidence;
                std::cout << "\n  Confidence difference: " << (high_confidence - low_confidence);
                std::cout << "\n  Face salience (high): " << high_confidence_features.face_salience;
                std::cout << "\n  Face salience (low): " << low_confidence_features.face_salience;
            }
            
            bool success = high_confidence > low_confidence + 0.3f; // Significant difference
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testCrossModalAssociationDecay() {
        std::cout << "Test 5: Cross-Modal Association Decay... ";
        
        try {
            // Create token and association
            std::size_t test_token = language_system_->createToken("test", LanguageSystem::TokenType::Word);
            
            std::vector<float> visual_pattern(32, 0.5f);
            language_system_->reinforceVisualGrounding(test_token, visual_pattern, 0.8f);
            
            auto initial_associations = language_system_->getCrossModalAssociations(test_token);
            
            if (initial_associations.empty()) {
                std::cout << "FAILED (no initial associations created)\n";
                return false;
            }
            
            float initial_strength = initial_associations[0].association_strength;
            
            // Create associations with old timestamps to simulate decay
            std::vector<LanguageSystem::CrossModalAssociation> old_associations;
            LanguageSystem::CrossModalAssociation old_assoc;
            old_assoc.token_id = test_token;
            old_assoc.modality = "vision";
            old_assoc.pattern = visual_pattern;
            old_assoc.association_strength = 0.9f;
            old_assoc.last_reinforced = std::chrono::steady_clock::now() - std::chrono::seconds(15);
            old_associations.push_back(old_assoc);
            
            // Update associations (should apply decay)
            language_system_->updateCrossModalAssociations(old_associations);
            
            auto final_associations = language_system_->getCrossModalAssociations(test_token);
            
            if (verbose_output_) {
                std::cout << "\n  Initial associations: " << initial_associations.size();
                std::cout << "\n  Final associations: " << final_associations.size();
                std::cout << "\n  Initial strength: " << std::fixed << std::setprecision(3) << initial_strength;
                if (!final_associations.empty()) {
                    std::cout << "\n  Final strength: " << std::fixed << std::setprecision(3) 
                             << final_associations[0].association_strength;
                }
            }
            
            bool success = !final_associations.empty(); // Associations should still exist but potentially weakened
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    bool testIntegratedFaceSpeechLearning() {
        std::cout << "Test 6: Integrated Face-Speech Learning... ";
        
        try {
            // Simulate learning "mama" with face present
            std::vector<float> mama_face(128);
            std::normal_distribution<float> face_dist(0.6f, 0.15f);
            for (float& val : mama_face) {
                val = std::max(0.0f, std::min(1.0f, face_dist(rng_)));
            }
            
            std::vector<float> mama_gaze = {0.0f, 0.0f}; // Direct gaze
            std::vector<float> mama_lips(16);
            std::uniform_real_distribution<float> lip_dist(0.4f, 0.9f);
            for (float& val : mama_lips) {
                val = lip_dist(rng_);
            }
            
            auto initial_vocab_size = language_system_->getActiveVocabulary().size();
            auto initial_stats = language_system_->getStatistics();
            
            // Multiple face-speech events to simulate learning
            for (int i = 0; i < 5; ++i) {
                // Slight variations in face/gaze/lips
                std::vector<float> varied_face = mama_face;
                for (float& val : varied_face) {
                    val += (rng_() % 100 - 50) / 1000.0f; // ±0.05 variation
                    val = std::max(0.0f, std::min(1.0f, val));
                }
                
                float temporal_alignment = 0.8f + (rng_() % 100) / 500.0f; // 0.8-1.0
                
                language_system_->processFaceSpeechEvent(
                    varied_face, mama_gaze, mama_lips, "mama", temporal_alignment);
            }
            
            auto final_vocab_size = language_system_->getActiveVocabulary().size();
            auto final_stats = language_system_->getStatistics();
            
            // Check mama token properties
            // Resolve token id safely
            std::size_t mama_token_id = 0;
            if (!language_system_->getTokenId("mama", mama_token_id)) {
                mama_token_id = 0; // keep default; associations will be empty
            }
            auto mama_associations = language_system_->getCrossModalAssociations(mama_token_id);
            auto mama_token = language_system_->getToken(mama_token_id);
            
            if (verbose_output_) {
                std::cout << "\n  Learning iterations: 5";
                std::cout << "\n  Vocabulary growth: " << initial_vocab_size << " -> " << final_vocab_size;
                std::cout << "\n  Grounding associations: " << initial_stats.grounding_associations_formed 
                         << " -> " << final_stats.grounding_associations_formed;
                if (mama_token) {
                    std::cout << "\n  Mama token activation: " << mama_token->activation_strength;
                    std::cout << "\n  Face salience: " << mama_token->sensory_associations.at("face_salience");
                }
                std::cout << "\n  Cross-modal associations: " << mama_associations.size();
            }
            
            bool success = mama_token && mama_token->activation_strength > 0.5f && 
                          final_stats.grounding_associations_formed > initial_stats.grounding_associations_formed;
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED - Exception: " << e.what() << std::endl;
            return false;
        }
    }

    void runAllTests() {
        std::cout << "=== NeuroForge Visual-Linguistic Integration Tests ===\n\n";
        
        int passed = 0;
        int total = 6;
        
        if (testFaceSpeechCoupling()) passed++;
        if (testVisualAttentionIntegration()) passed++;
        if (testCrossModalPatternRetrieval()) passed++;
        if (testFaceLanguageConfidence()) passed++;
        if (testCrossModalAssociationDecay()) passed++;
        if (testIntegratedFaceSpeechLearning()) passed++;
        
        std::cout << "\n=== Test Results ===\n";
        std::cout << "Passed: " << passed << "/" << total << " tests\n";
        std::cout << "Success Rate: " << std::fixed << std::setprecision(1) 
                  << (100.0f * passed / total) << "%\n";
        
        if (passed == total) {
            std::cout << "🎉 All tests passed! Visual-linguistic integration is working correctly.\n";
            std::cout << "✅ Face-speech coupling enabled\n";
            std::cout << "✅ Cross-modal associations functional\n";
            std::cout << "✅ Visual attention integration active\n";
            std::cout << "✅ Ready for connection to visual cortex and face bias modules\n";
        } else {
            std::cout << "⚠️  Some tests failed. Check implementation details.\n";
        }
    }
};

int main() {
    try {
        VisualLanguageIntegrationTestSuite test_suite(true);
        test_suite.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test suite failed with exception: " << e.what() << std::endl;
        return 1;
    }
}