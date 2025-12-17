#include "core/LanguageSystem.h"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cassert>

using namespace NeuroForge::Core;

/**
 * @brief Simplified test for core Babbling Stage functionality
 * 
 * Tests the basic babbling stage features that should compile without issues:
 * 1. Proto-word Crystallization basics
 * 2. Prosodic Pattern Learning basics
 * 3. Basic system functionality
 */
class SimpleBabblingStageTest {
private:
    std::unique_ptr<LanguageSystem> language_system_;
    
public:
    SimpleBabblingStageTest() {
        // Configure system for babbling stage testing
        LanguageSystem::Config config;
        
        // Enable core babbling stage features (using available config members)
        config.enable_speech_output = true;
        config.enable_lip_sync = true;
        
        // Basic parameters
        config.mimicry_learning_rate = 0.08f;
        config.grounding_strength = 0.5f;
        config.caregiver_mimicry_boost = 0.5f;
        config.prosody_attention_weight = 0.08f;
        config.intonation_attention_boost = 0.8f;
        
        language_system_ = std::make_unique<LanguageSystem>(config);
        language_system_->initialize();
        
        // Advance to babbling stage for testing
        language_system_->advanceToStage(LanguageSystem::DevelopmentalStage::Babbling);
    }
    
    bool testBasicBabblingStage() {
        std::cout << "Test 1: Basic Babbling Stage... ";
        
        try {
            auto initial_stats = language_system_->getStatistics();
            
            // Perform basic babbling
            for (int i = 0; i < 5; ++i) {
                language_system_->performEnhancedBabbling(2);
                language_system_->updateDevelopment(0.1f);
            }
            
            auto final_stats = language_system_->getStatistics();
            auto active_vocab = language_system_->getActiveVocabulary(0.1f);
            
            // Success criteria: token generation increased
            bool success = (final_stats.total_tokens_generated > initial_stats.total_tokens_generated) &&
                          (active_vocab.size() >= 2);
            
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (Exception: " << e.what() << ")" << std::endl;
            return false;
        }
    }
    
    bool testProsodicPatternLearning() {
        std::cout << "Test 2: Basic Prosodic Pattern Learning... ";
        
        try {
            // Create basic acoustic features
            LanguageSystem::AcousticFeatures features;
            features.pitch_contour = 200.0f;
            features.energy_envelope = 0.7f;
            features.intonation_slope = 0.3f;
            features.motherese_score = 0.8f;
            features.attention_score = 0.9f;
            
            // Process prosodic patterns
            for (int i = 0; i < 3; ++i) {
                language_system_->processProsodicPatternLearning(features, "mama");
                language_system_->processIntonationGuidedLearning("mama", features);
                language_system_->updateDevelopment(0.1f);
            }
            
            auto final_stats = language_system_->getStatistics();
            auto active_vocab = language_system_->getActiveVocabulary(0.1f);
            
            // Success criteria: vocabulary developed
            bool success = (active_vocab.size() >= 1) && 
                          (final_stats.average_token_activation > 0.0f);
            
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (Exception: " << e.what() << ")" << std::endl;
            return false;
        }
    }
    
    bool testProtoWordCreation() {
        std::cout << "Test 3: Proto-word Creation... ";
        
        try {
            // Create a proto-word
            std::vector<std::string> phonemes = {"ma", "ma"};
            std::size_t proto_word_id = language_system_->createProtoWord("ma-ma", phonemes);
            
            // Reinforce it
            language_system_->reinforceProtoWord(proto_word_id, 0.3f);
            
            auto final_stats = language_system_->getStatistics();
            
            // Success criteria: proto-word was created
            bool success = (proto_word_id != std::numeric_limits<std::size_t>::max());
            
            std::cout << (success ? "PASSED" : "FAILED") << std::endl;
            return success;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (Exception: " << e.what() << ")" << std::endl;
            return false;
        }
    }
    
    void runAllTests() {
        std::cout << "=== Simple Babbling Stage Test Suite ===" << std::endl;
        std::cout << std::endl;
        
        int passed = 0;
        int total = 3;
        
        if (testBasicBabblingStage()) passed++;
        if (testProsodicPatternLearning()) passed++;
        if (testProtoWordCreation()) passed++;
        
        std::cout << std::endl;
        std::cout << "=== Test Results ===" << std::endl;
        std::cout << "Passed: " << passed << "/" << total << " (" 
                  << (100.0f * passed / total) << "%)" << std::endl;
        
        if (passed == total) {
            std::cout << "🎉 All basic tests PASSED! Core babbling functionality working." << std::endl;
        } else {
            std::cout << "⚠️  Some tests failed. Check implementation." << std::endl;
        }
        
        // Generate final report
        std::cout << std::endl;
        std::cout << "=== System Status ===" << std::endl;
        auto final_stats = language_system_->getStatistics();
        std::cout << "- Current Stage: " << 
            (final_stats.current_stage == LanguageSystem::DevelopmentalStage::Babbling ? "Babbling" : "Other") << std::endl;
        std::cout << "- Vocabulary Size: " << language_system_->getActiveVocabulary(0.1f).size() << " tokens" << std::endl;
        std::cout << "- Total Tokens Generated: " << final_stats.total_tokens_generated << std::endl;
        std::cout << "- Average Token Activation: " << final_stats.average_token_activation << std::endl;
    }
};

int main() {
    try {
        SimpleBabblingStageTest test_suite;
        test_suite.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test suite failed with exception: " << e.what() << std::endl;
        return 1;
    }
}