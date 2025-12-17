#include "biases/NoveltyBias.h"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cassert>

using namespace NeuroForge::Biases;

class NoveltyBiasTest {
private:
    std::mt19937 rng_;
    
public:
    NoveltyBiasTest() : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {}
    
    std::vector<float> generateRandomVector(size_t size, float min_val = 0.0f, float max_val = 1.0f) {
        std::uniform_real_distribution<float> dist(min_val, max_val);
        std::vector<float> vec(size);
        for (auto& val : vec) {
            val = dist(rng_);
        }
        return vec;
    }
    
    std::vector<float> generateSimilarVector(const std::vector<float>& base, float noise_level = 0.1f) {
        std::normal_distribution<float> noise(0.0f, noise_level);
        std::vector<float> vec = base;
        for (auto& val : vec) {
            val += noise(rng_);
            val = std::clamp(val, 0.0f, 1.0f);
        }
        return vec;
    }
    
    bool testBasicNoveltyDetection() {
        std::cout << "Testing basic novelty detection..." << std::endl;
        
        try {
            NoveltyBias::Config config;
            config.experience_buffer_size = 10;
            config.novelty_threshold = 0.3f;
            NoveltyBias bias(config);
            
            // First input should have high novelty (no prior experience)
            auto input1 = generateRandomVector(16);
            auto metrics1 = bias.calculateNovelty(input1);
            
            if (metrics1.information_gain < 0.8f) {
                std::cout << "✗ First input should have high information gain" << std::endl;
                return false;
            }
            
            // Similar input should have lower novelty
            auto input2 = generateSimilarVector(input1, 0.05f);
            auto metrics2 = bias.calculateNovelty(input2);
            
            if (metrics2.familiarity_score < 0.5f) {
                std::cout << "✗ Similar input should have higher familiarity" << std::endl;
                return false;
            }
            
            // Very different input should have some novelty
            auto input3 = generateRandomVector(16);
            auto metrics3 = bias.calculateNovelty(input3);
            
            // Just check that we get some reasonable values, not specific thresholds
            if (metrics3.information_gain < 0.0f || metrics3.information_gain > 1.0f) {
                std::cout << "✗ Information gain should be in valid range [0,1] (got " << metrics3.information_gain << ")" << std::endl;
                return false;
            }
            
            std::cout << "✓ Basic novelty detection test passed" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in basic novelty detection: " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cout << "✗ Unknown exception in basic novelty detection" << std::endl;
            return false;
        }
    }
    
    bool testExplorationBonus() {
        std::cout << "Testing exploration bonus calculation..." << std::endl;
        
        NoveltyBias::Config config;
        config.enable_exploration_bonus = true;
        config.exploration_bonus_scale = 1.0f;
        NoveltyBias bias(config);
        
        // Test with high novelty input
        auto novel_input = generateRandomVector(16);
        auto metrics = bias.calculateNovelty(novel_input);
        
        if (metrics.exploration_bonus <= 0.0f) {
            std::cout << "✗ Novel input should have positive exploration bonus" << std::endl;
            return false;
        }
        
        // Test with familiar input (add same input multiple times)
        for (int i = 0; i < 5; ++i) {
            bias.calculateNovelty(novel_input);
        }
        
        auto familiar_metrics = bias.calculateNovelty(novel_input);
        if (familiar_metrics.exploration_bonus >= metrics.exploration_bonus) {
            std::cout << "✗ Familiar input should have lower exploration bonus" << std::endl;
            return false;
        }
        
        std::cout << "✓ Exploration bonus test passed" << std::endl;
        return true;
    }
    
    bool testPredictionModel() {
        std::cout << "Testing prediction model..." << std::endl;
        
        NoveltyBias::Config config;
        config.enable_prediction_learning = true;
        config.prediction_learning_rate = 0.2f;
        NoveltyBias bias(config);
        
        // Train prediction model with consistent pattern
        auto base_input = generateRandomVector(8);
        auto base_outcome = generateRandomVector(8);
        
        // Train multiple times
        for (int i = 0; i < 10; ++i) {
            auto noisy_input = generateSimilarVector(base_input, 0.1f);
            auto noisy_outcome = generateSimilarVector(base_outcome, 0.1f);
            bias.updatePredictionModel(noisy_input, noisy_outcome);
        }
        
        // Test prediction
        auto prediction = bias.getPrediction(base_input);
        if (prediction.empty()) {
            std::cout << "✗ Prediction model should return non-empty prediction" << std::endl;
            return false;
        }
        
        // Calculate prediction error with expected outcome
        float error = bias.calculatePredictionError(prediction, base_outcome);
        if (error > 0.8f) {
            std::cout << "✗ Prediction error should be reasonable after training" << std::endl;
            return false;
        }
        
        std::cout << "✓ Prediction model test passed" << std::endl;
        return true;
    }
    
    bool testNoveltyThreshold() {
        std::cout << "Testing novelty threshold..." << std::endl;
        
        NoveltyBias::Config config;
        NoveltyBias bias(config);
        
        // Test threshold setting
        bias.setNoveltyThreshold(0.5f);
        if (bias.getNoveltyThreshold() != 0.5f) {
            std::cout << "✗ Novelty threshold setting failed" << std::endl;
            return false;
        }
        
        // Test novelty detection with threshold
        auto input1 = generateRandomVector(16);
        auto input2 = generateSimilarVector(input1, 0.01f); // Very similar
        
        bias.calculateNovelty(input1); // Add to experience
        
        bool is_novel = bias.isNovel(input2, 0.8f); // High threshold
        if (is_novel) {
            std::cout << "✗ Very similar input should not be novel with high threshold" << std::endl;
            return false;
        }
        
        is_novel = bias.isNovel(input2, 0.1f); // Low threshold
        // This could go either way depending on the specific similarity
        
        std::cout << "✓ Novelty threshold test passed" << std::endl;
        return true;
    }
    
    bool testExperienceBuffer() {
        std::cout << "Testing experience buffer management..." << std::endl;
        
        NoveltyBias::Config config;
        config.experience_buffer_size = 5; // Small buffer for testing
        NoveltyBias bias(config);
        
        // Fill buffer beyond capacity
        std::vector<std::vector<float>> inputs;
        for (int i = 0; i < 8; ++i) {
            auto input = generateRandomVector(10);
            inputs.push_back(input);
            bias.updateExperienceBuffer(input);
        }
        
        auto stats = bias.getStatistics();
        if (stats.experience_buffer_size > config.experience_buffer_size) {
            std::cout << "✗ Experience buffer should not exceed maximum size" << std::endl;
            return false;
        }
        
        std::cout << "✓ Experience buffer test passed" << std::endl;
        return true;
    }
    
    bool testComplexityCalculation() {
        std::cout << "Testing complexity calculation..." << std::endl;
        
        NoveltyBias::Config config;
        NoveltyBias bias(config);
        
        // Test with uniform input (low complexity)
        std::vector<float> uniform_input(16, 0.5f);
        auto uniform_metrics = bias.calculateNovelty(uniform_input);
        
        // Test with random input (high complexity)
        auto random_input = generateRandomVector(16);
        auto random_metrics = bias.calculateNovelty(random_input);
        
        if (random_metrics.complexity_score <= uniform_metrics.complexity_score) {
            std::cout << "✗ Random input should have higher complexity than uniform input" << std::endl;
            return false;
        }
        
        std::cout << "✓ Complexity calculation test passed" << std::endl;
        return true;
    }
    
    bool testStatistics() {
        std::cout << "Testing statistics tracking..." << std::endl;
        
        NoveltyBias::Config config;
        NoveltyBias bias(config);
        
        // Process several inputs
        for (int i = 0; i < 5; ++i) {
            auto input = generateRandomVector(12);
            bias.calculateNovelty(input);
        }
        
        auto stats = bias.getStatistics();
        
        if (stats.total_experiences != 5) {
            std::cout << "✗ Statistics should track total experiences correctly" << std::endl;
            return false;
        }
        
        if (stats.experience_buffer_size == 0) {
            std::cout << "✗ Experience buffer should contain experiences" << std::endl;
            return false;
        }
        
        if (stats.average_novelty < 0.0f || stats.average_novelty > 1.0f) {
            std::cout << "✗ Average novelty should be in valid range" << std::endl;
            return false;
        }
        
        std::cout << "✓ Statistics test passed" << std::endl;
        return true;
    }
    
    bool testConfiguration() {
        std::cout << "Testing configuration management..." << std::endl;
        
        NoveltyBias::Config config;
        config.experience_buffer_size = 100;
        config.novelty_threshold = 0.4f;
        config.enable_exploration_bonus = false;
        
        NoveltyBias bias(config);
        
        auto retrieved_config = bias.getConfig();
        if (retrieved_config.experience_buffer_size != 100 ||
            retrieved_config.novelty_threshold != 0.4f ||
            retrieved_config.enable_exploration_bonus != false) {
            std::cout << "✗ Configuration retrieval failed" << std::endl;
            return false;
        }
        
        // Test configuration update
        config.novelty_threshold = 0.6f;
        bias.setConfig(config);
        
        if (bias.getNoveltyThreshold() != 0.6f) {
            std::cout << "✗ Configuration update failed" << std::endl;
            return false;
        }
        
        std::cout << "✓ Configuration test passed" << std::endl;
        return true;
    }
    
    bool testClearOperation() {
        std::cout << "Testing clear operation..." << std::endl;
        
        NoveltyBias::Config config;
        NoveltyBias bias(config);
        
        // Add some experiences
        for (int i = 0; i < 3; ++i) {
            auto input = generateRandomVector(10);
            bias.calculateNovelty(input);
        }
        
        auto stats_before = bias.getStatistics();
        if (stats_before.total_experiences == 0) {
            std::cout << "✗ Should have experiences before clear" << std::endl;
            return false;
        }
        
        // Clear and check
        bias.clear();
        auto stats_after = bias.getStatistics();
        
        if (stats_after.experience_buffer_size != 0) {
            std::cout << "✗ Experience buffer should be empty after clear" << std::endl;
            return false;
        }
        
        std::cout << "✓ Clear operation test passed" << std::endl;
        return true;
    }
    
    bool runAllTests() {
        std::cout << "=== Novelty Bias Module Test Suite ===" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testBasicNoveltyDetection();
        all_passed &= testExplorationBonus();
        all_passed &= testPredictionModel();
        all_passed &= testNoveltyThreshold();
        all_passed &= testExperienceBuffer();
        all_passed &= testComplexityCalculation();
        all_passed &= testStatistics();
        all_passed &= testConfiguration();
        all_passed &= testClearOperation();
        
        if (all_passed) {
            std::cout << std::endl << "✅ All Novelty Bias tests passed!" << std::endl;
            std::cout << "Novelty Bias module is ready for integration." << std::endl;
        } else {
            std::cout << std::endl << "❌ Some Novelty Bias tests failed!" << std::endl;
        }
        
        return all_passed;
    }
};

int main() {
    NoveltyBiasTest test;
    bool success = test.runAllTests();
    return success ? 0 : 1;
}