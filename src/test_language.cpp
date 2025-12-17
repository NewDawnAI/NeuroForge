/**
 * @file test_language.cpp
 * @brief Test program for validating Phase 5 Language System implementation in NeuroForge
 * 
 * This program demonstrates and validates the developmental language learning features
 * including mimicry, grounding, narration, and multi-stage language acquisition.
 */

#include "core/LanguageSystem.h"
#include "core/HypergraphBrain.h"
#include "connectivity/ConnectivityManager.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <random>
#include <vector>
#include <memory>
#include <thread>
#include <cassert>
#include <string>

using namespace NeuroForge;
using namespace NeuroForge::Core;
using namespace NeuroForge::Connectivity;

class LanguageTestSuite {
private:
    std::unique_ptr<LanguageSystem> language_system_;
    std::mt19937 rng_;
    bool enable_verbose_output_ = false;

public:
    LanguageTestSuite(bool verbose = false) 
        : rng_(std::random_device{}()), enable_verbose_output_(verbose) {
        
        LanguageSystem::Config config;
        config.mimicry_learning_rate = 0.05f;
        config.grounding_strength = 0.7f;
        config.narration_threshold = 0.3f;
        config.max_vocabulary_size = 1000;
        config.embedding_dimension = 128;
        config.babbling_duration = 100;
        config.mimicry_duration = 200;
        config.grounding_duration = 300;
        config.enable_teacher_mode = true;
        config.teacher_influence = 0.8f;
        
        language_system_ = std::make_unique<LanguageSystem>(config);
    }
    
    bool runAllTests() {
        std::cout << "=== NeuroForge Phase 5 Language System Test Suite ===\n\n";
        
        bool all_passed = true;
        
        all_passed &= testSystemInitialization();
        all_passed &= testTokenCreationAndManagement();
        all_passed &= testDevelopmentalStages();
        all_passed &= testMimicryLearning();
        all_passed &= testMultimodalGrounding();
        all_passed &= testInternalNarration();
        all_passed &= testBabblingAndExploration();
        all_passed &= testNeuralIntegration();
        all_passed &= testVocabularyManagement();
        all_passed &= testStatisticsAndReporting();
        all_passed &= testSerialization();
        all_passed &= testDevelopmentalProgression();
        
        std::cout << "\n=== Test Suite Summary ===\n";
        if (all_passed) {
            std::cout << "✅ All tests PASSED!\n";
        } else {
            std::cout << "❌ Some tests FAILED!\n";
        }
        
        return all_passed;
    }

private:
    bool testSystemInitialization() {
        std::cout << "Test 1: Language System Initialization... ";
        
        try {
            bool init_success = language_system_->initialize();
            if (!init_success) {
                std::cout << "FAILED (initialization returned false)\n";
                return false;
            }
            
            // Check initial state
            auto stats = language_system_->getStatistics();
            if (stats.current_stage != LanguageSystem::DevelopmentalStage::Chaos) {
                std::cout << "FAILED (wrong initial stage)\n";
                return false;
            }
            
            // Check basic tokens were created
            auto* start_token = language_system_->getToken("<START>");
            auto* self_token = language_system_->getToken("<SELF>");
            
            if (!start_token || !self_token) {
                std::cout << "FAILED (basic tokens not created)\n";
                return false;
            }
            
            std::cout << "PASSED\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testTokenCreationAndManagement() {
        std::cout << "Test 2: Token Creation and Management... ";
        
        try {
            // Create various token types
            std::size_t word_token = language_system_->createToken("hello", LanguageSystem::TokenType::Word);
            std::size_t action_token = language_system_->createToken("walk", LanguageSystem::TokenType::Action);
            std::size_t phoneme_token = language_system_->createToken("ba", LanguageSystem::TokenType::Phoneme);
            
            // Test token retrieval
            auto* hello_token = language_system_->getToken("hello");
            auto* walk_token = language_system_->getToken(word_token);
            
            if (!hello_token || !walk_token) {
                std::cout << "FAILED (token retrieval failed)\n";
                return false;
            }
            
            if (hello_token->symbol != "hello" || hello_token->type != LanguageSystem::TokenType::Word) {
                std::cout << "FAILED (token properties incorrect)\n";
                return false;
            }
            
            // Test token similarity
            // Use the embedding of an existing token to ensure a match
            auto* hello_token_ptr = language_system_->getToken("hello");
            if (!hello_token_ptr) {
                 std::cout << "FAILED (could not retrieve hello token)\n";
                 return false;
            }
            
            std::vector<float> query_embedding = hello_token_ptr->embedding;
            // Use a threshold that should definitely match the token itself (similarity 1.0)
            std::vector<std::size_t> similar = language_system_->findSimilarTokens(query_embedding, 0.9f);
            
            if (similar.empty()) {
                std::cout << "FAILED (similarity search failed for identical embedding)\n";
                return false;
            }
            
            // Verify the found token is indeed "hello"
            bool found_hello = false;
            for (auto id : similar) {
                auto* t = language_system_->getToken(id);
                if (t && t->symbol == "hello") {
                    found_hello = true;
                    break;
                }
            }
            
            if (!found_hello) {
                std::cout << "FAILED (similarity search did not return the query token)\n";
                return false;
            }
            
            std::cout << "PASSED (" << similar.size() << " similar tokens found)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testDevelopmentalStages() {
        std::cout << "Test 3: Developmental Stage Progression... ";
        
        try {
            // Test stage advancement
            auto initial_stage = language_system_->getCurrentStage();
            
            language_system_->advanceToStage(LanguageSystem::DevelopmentalStage::Babbling);
            auto babbling_stage = language_system_->getCurrentStage();
            
            if (babbling_stage != LanguageSystem::DevelopmentalStage::Babbling) {
                std::cout << "FAILED (stage advancement failed)\n";
                return false;
            }
            
            language_system_->advanceToStage(LanguageSystem::DevelopmentalStage::Mimicry);
            auto mimicry_stage = language_system_->getCurrentStage();
            
            if (mimicry_stage != LanguageSystem::DevelopmentalStage::Mimicry) {
                std::cout << "FAILED (mimicry stage advancement failed)\n";
                return false;
            }
            
            // Test stage-specific behavior
            language_system_->updateDevelopment(0.1f);
            
            std::cout << "PASSED (stages: Chaos -> Babbling -> Mimicry)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testMimicryLearning() {
        std::cout << "Test 4: Mimicry Learning System... ";
        
        try {
            // Set up teacher embeddings
            std::vector<float> teacher_embedding(128);
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            
            for (float& val : teacher_embedding) {
                val = dist(rng_);
            }
            
            language_system_->setTeacherEmbedding("mama", teacher_embedding);
            
            // Process teacher signal
            language_system_->processTeacherSignal("mama", 1.0f);
            
            // Test mimicry response
            auto response = language_system_->generateMimicryResponse(teacher_embedding);
            
            if (response.empty() || response.size() != teacher_embedding.size()) {
                std::cout << "FAILED (invalid mimicry response)\n";
                return false;
            }
            
            // Check if token was created/updated
            auto* mama_token = language_system_->getToken("mama");
            if (!mama_token) {
                std::cout << "FAILED (teacher token not created)\n";
                return false;
            }
            
            if (mama_token->usage_count == 0) {
                std::cout << "FAILED (token usage not tracked)\n";
                return false;
            }
            
            std::cout << "PASSED (mimicry response generated, token updated)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testMultimodalGrounding() {
        std::cout << "Test 5: Multimodal Grounding... ";
        
        try {
            // Create test tokens
            std::size_t red_token = language_system_->createToken("red", LanguageSystem::TokenType::Perception);
            std::size_t move_token = language_system_->createToken("move", LanguageSystem::TokenType::Action);
            
            // Test neuron associations
            NeuroForge::NeuronID test_neuron_1 = 12345;
            NeuroForge::NeuronID test_neuron_2 = 67890;
            
            language_system_->associateTokenWithNeuron(red_token, test_neuron_1, 0.8f);
            language_system_->associateTokenWithNeuron(move_token, test_neuron_2, 0.9f);
            
            // Test modality associations
            std::vector<float> visual_pattern = {0.8f, 0.2f, 0.1f, 0.9f};
            language_system_->associateTokenWithModality(red_token, "vision", visual_pattern, 0.7f);
            
            // Test pattern-based token retrieval
            std::vector<NeuroForge::NeuronID> active_neurons = {test_neuron_1, test_neuron_2};
            auto associated_tokens = language_system_->getTokensForNeuralPattern(active_neurons);
            
            if (associated_tokens.size() < 2) {
                std::cout << "FAILED (neural pattern association failed)\n";
                return false;
            }
            
            // Verify associations were created
            auto stats = language_system_->getStatistics();
            if (stats.grounding_associations_formed < 2) {
                std::cout << "FAILED (grounding associations not tracked)\n";
                return false;
            }
            
            std::cout << "PASSED (" << stats.grounding_associations_formed << " associations formed)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testInternalNarration() {
        std::cout << "Test 6: Internal Narration System... ";
        
        try {
            // Enable narration
            language_system_->enableNarration(true);
            
            // Generate test narration
            std::vector<float> context_embedding(128, 0.5f);
            language_system_->generateNarration(context_embedding, "Test context");
            
            // Log self-narration
            std::vector<std::string> token_sequence = {"I", "see", "red", "square"};
            language_system_->logSelfNarration(token_sequence, 0.8f, "Visual observation");
            
            // Check narration history
            auto recent_narration = language_system_->getRecentNarration(5);
            
            if (recent_narration.empty()) {
                std::cout << "FAILED (no narration entries found)\n";
                return false;
            }
            
            bool found_self_narration = false;
            for (const auto& entry : recent_narration) {
                if (entry.context == "Visual observation" && entry.confidence > 0.7f) {
                    found_self_narration = true;
                    break;
                }
            }
            
            if (!found_self_narration) {
                std::cout << "FAILED (self-narration not found)\n";
                return false;
            }
            
            auto stats = language_system_->getStatistics();
            if (stats.narration_entries == 0) {
                std::cout << "FAILED (narration entries not tracked)\n";
                return false;
            }
            
            std::cout << "PASSED (" << stats.narration_entries << " narration entries)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testBabblingAndExploration() {
        std::cout << "Test 7: Babbling and Token Exploration... ";
        
        try {
            auto initial_stats = language_system_->getStatistics();
            std::size_t initial_tokens = initial_stats.total_tokens_generated;
            
            // Test babbling
            language_system_->performBabbling(5);
            
            auto post_babbling_stats = language_system_->getStatistics();
            if (post_babbling_stats.total_tokens_generated <= initial_tokens) {
                std::cout << "FAILED (babbling did not generate tokens)\n";
                return false;
            }
            
            // Test token combination exploration
            language_system_->exploreTokenCombinations(3);
            
            // Check if exploration generated narration
            auto recent_narration = language_system_->getRecentNarration(3);
            bool found_exploration = false;
            
            for (const auto& entry : recent_narration) {
                if (entry.context == "Token exploration") {
                    found_exploration = true;
                    break;
                }
            }
            
            if (!found_exploration) {
                std::cout << "FAILED (token exploration not logged)\n";
                return false;
            }
            
            std::cout << "PASSED (" << (post_babbling_stats.total_tokens_generated - initial_tokens) 
                     << " tokens generated)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testNeuralIntegration() {
        std::cout << "Test 8: Neural Substrate Integration... ";
        
        try {
            // Simulate neural activations
            std::vector<std::pair<NeuroForge::NeuronID, float>> activations = {
                {1001, 0.8f},
                {1002, 0.6f},
                {1003, 0.9f},
                {1004, 0.3f}  // Below threshold
            };
            
            // Process neural activation
            language_system_->processNeuralActivation(activations);
            
            // Test influence on neural activation (placeholder test)
            std::vector<std::size_t> active_tokens = {0, 1, 2}; // Use first few tokens
            language_system_->influenceNeuralActivation(active_tokens, 0.5f);
            
            // This test mainly verifies the interface works without errors
            // Full integration would require a complete HypergraphBrain instance
            
            std::cout << "PASSED (neural integration interface functional)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testVocabularyManagement() {
        std::cout << "Test 9: Vocabulary Management... ";
        
        try {
            // Get active vocabulary
            auto active_vocab = language_system_->getActiveVocabulary(0.1f);
            
            if (active_vocab.empty()) {
                std::cout << "FAILED (no active vocabulary found)\n";
                return false;
            }
            
            // Test vocabulary growth
            std::size_t initial_size = active_vocab.size();
            
            // Create more tokens
            for (int i = 0; i < 10; ++i) {
                std::string token_name = "test_token_" + std::to_string(i);
                std::size_t token_id = language_system_->createToken(token_name, LanguageSystem::TokenType::Word);
                
                // Activate the token
                auto* token = language_system_->getToken(token_id);
                if (token) {
                    token->activation_strength = 0.5f;
                }
            }
            
            auto expanded_vocab = language_system_->getActiveVocabulary(0.1f);
            
            if (expanded_vocab.size() <= initial_size) {
                std::cout << "FAILED (vocabulary did not expand)\n";
                return false;
            }
            
            std::cout << "PASSED (vocabulary: " << initial_size << " -> " 
                     << expanded_vocab.size() << " tokens)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testStatisticsAndReporting() {
        std::cout << "Test 10: Statistics and Reporting... ";
        
        try {
            // Update statistics
            auto stats = language_system_->getStatistics();
            
            // Verify statistics are reasonable
            if (stats.total_tokens_generated == 0) {
                std::cout << "FAILED (no tokens generated tracked)\n";
                return false;
            }
            
            if (stats.active_vocabulary_size == 0) {
                std::cout << "FAILED (no active vocabulary)\n";
                return false;
            }
            
            // Generate language report
            std::string report = language_system_->generateLanguageReport();
            
            if (report.empty()) {
                std::cout << "FAILED (empty language report)\n";
                return false;
            }
            
            // Check report contains expected sections
            if (report.find("Language System Report") == std::string::npos ||
                report.find("Current Stage") == std::string::npos ||
                report.find("Vocabulary Size") == std::string::npos) {
                std::cout << "FAILED (incomplete language report)\n";
                return false;
            }
            
            if (enable_verbose_output_) {
                std::cout << "\n" << report << "\n";
            }
            
            std::cout << "PASSED (comprehensive statistics and reporting)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testSerialization() {
        std::cout << "Test 11: Serialization and Export... ";
        
        try {
            // Export vocabulary to JSON
            std::string vocab_json = language_system_->exportVocabularyToJson();
            
            if (vocab_json.empty()) {
                std::cout << "FAILED (empty vocabulary JSON)\n";
                return false;
            }
            
            // Check JSON structure
            if (vocab_json.find("vocabulary") == std::string::npos ||
                vocab_json.find("symbol") == std::string::npos) {
                std::cout << "FAILED (invalid vocabulary JSON structure)\n";
                return false;
            }
            
            // Export narration to JSON
            std::string narration_json = language_system_->exportNarrationToJson();
            
            if (narration_json.empty()) {
                std::cout << "FAILED (empty narration JSON)\n";
                return false;
            }
            
            if (enable_verbose_output_) {
                std::cout << "\nVocabulary JSON (first 200 chars): " 
                         << vocab_json.substr(0, 200) << "...\n";
            }
            
            std::cout << "PASSED (JSON serialization functional)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
    
    bool testDevelopmentalProgression() {
        std::cout << "Test 12: Complete Developmental Progression... ";
        
        try {
            // Reset to chaos stage
            language_system_->advanceToStage(LanguageSystem::DevelopmentalStage::Chaos);
            
            // Simulate developmental progression
            std::vector<LanguageSystem::DevelopmentalStage> expected_stages = {
                LanguageSystem::DevelopmentalStage::Chaos,
                LanguageSystem::DevelopmentalStage::Babbling,
                LanguageSystem::DevelopmentalStage::Mimicry,
                LanguageSystem::DevelopmentalStage::Grounding,
                LanguageSystem::DevelopmentalStage::Reflection,
                LanguageSystem::DevelopmentalStage::Communication
            };
            
            for (std::size_t i = 1; i < expected_stages.size(); ++i) {
                language_system_->advanceToStage(expected_stages[i]);
                
                // Simulate some development time
                for (int step = 0; step < 10; ++step) {
                    language_system_->updateDevelopment(0.1f);
                }
                
                auto current_stage = language_system_->getCurrentStage();
                if (current_stage != expected_stages[i]) {
                    std::cout << "FAILED (stage progression error at stage " << i << ")\n";
                    return false;
                }
            }
            
            // Final statistics check
            auto final_stats = language_system_->getStatistics();
            
            if (final_stats.current_stage != LanguageSystem::DevelopmentalStage::Communication) {
                std::cout << "FAILED (did not reach final stage)\n";
                return false;
            }
            
            std::cout << "PASSED (complete developmental progression: Chaos -> Communication)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "FAILED (exception: " << e.what() << ")\n";
            return false;
        }
    }
};

// Remove conditional compilation and always include main
int main(int argc, char** argv) {
    bool verbose = false;
    
    // Check for verbose flag
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--verbose" || std::string(argv[i]) == "-v") {
            verbose = true;
            break;
        }
    }
    
    LanguageTestSuite test_suite(verbose);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    bool all_passed = test_suite.runAllTests();
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "\nTest execution time: " << duration.count() << " ms\n";
    
    return all_passed ? 0 : 1;
}

// Standalone test functions for integration with other test suites
namespace NeuroForge {
namespace Testing {

bool testLanguageSystemBasics() {
    LanguageTestSuite suite(false);
    return suite.runAllTests();
}

bool testLanguageSystemVerbose() {
    LanguageTestSuite suite(true);
    return suite.runAllTests();
}

void demonstrateLanguageDevelopment() {
    std::cout << "=== NeuroForge Phase 5 Language Development Demo ===\n\n";
    
    LanguageSystem::Config config;
    config.babbling_duration = 50;
    config.mimicry_duration = 100;
    config.grounding_duration = 150;
    config.enable_teacher_mode = true;
    
    LanguageSystem language_system(config);
    language_system.initialize();
    
    // Demonstrate each developmental stage
    std::vector<std::pair<LanguageSystem::DevelopmentalStage, std::string>> stages = {
        {LanguageSystem::DevelopmentalStage::Chaos, "Random neural activation, no structure"},
        {LanguageSystem::DevelopmentalStage::Babbling, "Proto-phoneme generation and exploration"},
        {LanguageSystem::DevelopmentalStage::Mimicry, "Teacher imitation and pattern copying"},
        {LanguageSystem::DevelopmentalStage::Grounding, "Associating symbols with experiences"},
        {LanguageSystem::DevelopmentalStage::Reflection, "Internal narration and self-awareness"},
        {LanguageSystem::DevelopmentalStage::Communication, "Goal-directed language use"}
    };
    
    for (const auto& stage_info : stages) {
        language_system.advanceToStage(stage_info.first);
        
        std::cout << "Stage: " << static_cast<int>(stage_info.first) + 1 << " - " 
                 << stage_info.second << "\n";
        
        // Simulate development
        for (int i = 0; i < 20; ++i) {
            language_system.updateDevelopment(0.1f);
        }
        
        // Add stage-specific demonstrations
        switch (stage_info.first) {
            case LanguageSystem::DevelopmentalStage::Babbling:
                language_system.performBabbling(3);
                std::cout << "  Generated babbling tokens\n";
                break;
                
            case LanguageSystem::DevelopmentalStage::Mimicry: {
                std::vector<float> teacher_embedding(128, 0.5f);
                language_system.setTeacherEmbedding("hello", teacher_embedding);
                language_system.processTeacherSignal("hello", 1.0f);
                std::cout << "  Learned to mimic 'hello'\n";
                break;
            }
            
            case LanguageSystem::DevelopmentalStage::Grounding:
                language_system.associateTokenWithNeuron(0, 12345, 0.8f);
                std::cout << "  Associated tokens with neural patterns\n";
                break;
                
            case LanguageSystem::DevelopmentalStage::Reflection:
                language_system.enableNarration(true);
                language_system.logSelfNarration({"I", "think", "therefore", "I", "am"}, 0.9f, "Self-reflection");
                std::cout << "  Generated internal narration\n";
                break;
                
            case LanguageSystem::DevelopmentalStage::Communication: {
                std::vector<float> context(128, 0.7f);
                language_system.generateNarration(context, "Goal-directed communication");
                std::cout << "  Engaged in purposeful communication\n";
                break;
            }
            
            default:
                break;
        }
        
        auto stats = language_system.getStatistics();
        std::cout << "  Vocabulary size: " << stats.active_vocabulary_size 
                 << ", Narration entries: " << stats.narration_entries << "\n\n";
    }
    
    std::cout << "Final Language Report:\n";
    std::cout << language_system.generateLanguageReport() << "\n";
}

} // namespace Testing
} // namespace NeuroForge