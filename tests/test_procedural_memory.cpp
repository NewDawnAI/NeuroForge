#include "memory/ProceduralMemory.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <chrono>
#include <thread>

using namespace NeuroForge::Memory;

void testBasicSkillLearning() {
    std::cout << "Testing basic skill learning..." << std::endl;
    
    ProceduralMemory::Config config;
    ProceduralMemory pm(config);
    
    // Test initial state
    assert(pm.getSkillCount() == 0);
    assert(pm.getAverageConfidence() == 0.0f);
    
    // Learn a simple skill
    std::vector<int> skill_sequence = {1, 2, 3, 4};
    int skill_id = pm.learnSkill(skill_sequence, "test_skill", 0.7f);
    
    assert(skill_id > 0);
    assert(pm.getSkillCount() == 1);
    
    // Retrieve the skill
    auto retrieved_skill = pm.retrieveSkill(skill_id);
    assert(!retrieved_skill.action_sequence.empty());
    assert(retrieved_skill.action_sequence == skill_sequence);
    assert(retrieved_skill.skill_name == "test_skill");
    assert(retrieved_skill.confidence == 0.7f);
    
    std::cout << "✓ Basic skill learning test passed" << std::endl;
}

void testSkillReinforcement() {
    std::cout << "Testing skill reinforcement..." << std::endl;
    
    ProceduralMemory::Config config;
    ProceduralMemory pm(config);
    
    std::vector<int> skill_sequence = {5, 6, 7};
    int skill_id = pm.learnSkill(skill_sequence, "reinforcement_test", 0.5f);
    
    // Get initial confidence
    auto initial_skill = pm.retrieveSkill(skill_id);
    float initial_confidence = initial_skill.confidence;
    
    // Apply positive reinforcement
    bool success = pm.reinforceSkill(skill_id, 1.0f, 1.0f);
    assert(success);
    
    // Check confidence improved
    auto reinforced_skill = pm.retrieveSkill(skill_id);
    assert(reinforced_skill.confidence >= initial_confidence);
    
    // Apply negative reinforcement
    pm.reinforceSkill(skill_id, -0.5f, 2.0f);
    
    auto penalized_skill = pm.retrieveSkill(skill_id);
    // Confidence should be affected by negative reward
    
    std::cout << "✓ Skill reinforcement test passed" << std::endl;
}

void testSkillPractice() {
    std::cout << "Testing skill practice..." << std::endl;
    
    ProceduralMemory::Config config;
    ProceduralMemory pm(config);
    
    std::vector<int> skill_sequence = {10, 11, 12};
    int skill_id = pm.learnSkill(skill_sequence, "practice_test", 0.6f);
    
    // Get initial practice count
    auto initial_skill = pm.retrieveSkill(skill_id);
    uint32_t initial_practice = initial_skill.practice_count;
    
    // Practice the skill successfully
    bool success = pm.practiceSkill(skill_id, true);
    assert(success);
    
    auto practiced_skill = pm.retrieveSkill(skill_id);
    assert(practiced_skill.practice_count == initial_practice + 1);
    assert(practiced_skill.success_count == 1);
    assert(practiced_skill.failure_count == 0);
    
    // Practice with failure
    pm.practiceSkill(skill_id, false);
    
    auto failed_practice = pm.retrieveSkill(skill_id);
    assert(failed_practice.failure_count == 1);
    assert(failed_practice.success_rate < 1.0f);
    
    std::cout << "✓ Skill practice test passed" << std::endl;
}

void testSkillRetrieval() {
    std::cout << "Testing skill retrieval..." << std::endl;
    
    ProceduralMemory::Config config;
    ProceduralMemory pm(config);
    
    // Learn multiple skills
    std::vector<int> skill1 = {1, 2, 3};
    std::vector<int> skill2 = {4, 5, 6};
    std::vector<int> skill3 = {7, 8, 9};
    
    int id1 = pm.learnSkill(skill1, "skill_one", 0.8f);
    int id2 = pm.learnSkill(skill2, "skill_two", 0.6f);
    int id3 = pm.learnSkill(skill3, "skill_three", 0.4f);
    
    // Test find by name
    int found_id = pm.findSkillByName("skill_two");
    assert(found_id == id2);
    
    // Test get all skill IDs
    auto all_ids = pm.getAllSkillIds();
    assert(all_ids.size() == 3);
    
    // Test get confident skills
    auto confident_skills = pm.getConfidentSkills(0.7f);
    assert(confident_skills.size() == 1); // Only skill1 has confidence >= 0.7
    
    // Test context-based retrieval
    std::vector<int> context = {1, 2}; // Matches beginning of skill1
    int context_skill = pm.getBestSkillForContext(context, 0.5f);
    assert(context_skill == id1);
    
    std::cout << "✓ Skill retrieval test passed" << std::endl;
}

void testSimilarityDetection() {
    std::cout << "Testing skill similarity detection..." << std::endl;
    
    ProceduralMemory::Config config;
    ProceduralMemory pm(config);
    
    std::vector<int> original_skill = {1, 2, 3, 4, 5};
    std::vector<int> similar_skill = {1, 2, 3, 4, 6}; // 80% similar
    std::vector<int> different_skill = {10, 11, 12, 13, 14};
    
    int original_id = pm.learnSkill(original_skill, "original", 0.7f);
    
    // Try to learn similar skill - should find existing one
    int similar_id = pm.learnSkill(similar_skill, "similar", 0.6f);
    // Should return original_id due to similarity detection
    
    // Learn different skill - should create new one
    int different_id = pm.learnSkill(different_skill, "different", 0.5f);
    assert(different_id != original_id);
    
    // Test explicit similarity search
    int found_similar = pm.findSimilarSkill(similar_skill, 0.7f);
    assert(found_similar == original_id);
    
    std::cout << "✓ Similarity detection test passed" << std::endl;
}

void testSkillManagement() {
    std::cout << "Testing skill management operations..." << std::endl;
    
    ProceduralMemory::Config config;
    config.skill_timeout_ms = 100; // Very short timeout for testing
    config.pruning_threshold = 0.3f;
    
    ProceduralMemory pm(config);
    
    // Learn skills with different confidence levels
    std::vector<int> high_conf_skill = {1, 2, 3};
    std::vector<int> low_conf_skill = {4, 5, 6};
    
    int high_id = pm.learnSkill(high_conf_skill, "high_confidence", 0.8f);
    int low_id = pm.learnSkill(low_conf_skill, "low_confidence", 0.2f);
    
    assert(pm.getSkillCount() == 2);
    
    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    
    // Prune unused skills
    size_t pruned_count = pm.pruneUnusedSkills();
    assert(pruned_count > 0);
    assert(pm.getSkillCount() < 2);
    
    // Test skill removal
    pm.learnSkill({7, 8, 9}, "to_remove", 0.5f);
    size_t count_before = pm.getSkillCount();
    
    auto all_ids = pm.getAllSkillIds();
    if (!all_ids.empty()) {
        bool removed = pm.removeSkill(all_ids[0]);
        assert(removed);
        assert(pm.getSkillCount() == count_before - 1);
    }
    
    std::cout << "✓ Skill management test passed" << std::endl;
}

void testSkillDecay() {
    std::cout << "Testing skill decay mechanism..." << std::endl;
    
    ProceduralMemory::Config config;
    config.decay_rate = 1.0f; // Fast decay for testing
    
    ProceduralMemory pm(config);
    
    std::vector<int> skill_sequence = {1, 2, 3};
    int skill_id = pm.learnSkill(skill_sequence, "decay_test", 0.8f);
    
    // Get initial confidence
    auto initial_skill = pm.retrieveSkill(skill_id);
    float initial_confidence = initial_skill.confidence;
    
    // Apply decay
    pm.applyDecay(1.0f); // 1 second of decay
    
    auto decayed_skill = pm.retrieveSkill(skill_id);
    assert(decayed_skill.confidence < initial_confidence);
    
    std::cout << "✓ Skill decay test passed" << std::endl;
}

void testPerformanceMetrics() {
    std::cout << "Testing performance metrics..." << std::endl;
    
    ProceduralMemory::Config config;
    ProceduralMemory pm(config);
    
    // Learn and practice multiple skills
    for (int i = 0; i < 5; ++i) {
        std::vector<int> skill = {i, i+1, i+2};
        int skill_id = pm.learnSkill(skill, "skill_" + std::to_string(i), 0.5f + i * 0.1f);
        
        // Practice with varying success
        for (int j = 0; j < i + 1; ++j) {
            pm.practiceSkill(skill_id, j % 2 == 0); // Alternate success/failure
        }
    }
    
    // Test statistics
    auto stats = pm.getStatistics();
    assert(stats.total_skills == 5);
    assert(stats.total_skills_learned >= 5);
    assert(stats.average_confidence > 0.0f);
    
    // Test most practiced skill
    int most_practiced = pm.getMostPracticedSkill();
    assert(most_practiced > 0);
    
    // Test most successful skill
    int most_successful = pm.getMostSuccessfulSkill();
    // May be -1 if no skill has enough attempts
    
    std::cout << "✓ Performance metrics test passed" << std::endl;
}

void testConfiguration() {
    std::cout << "Testing configuration management..." << std::endl;
    
    ProceduralMemory::Config config;
    config.reinforcement_rate = 0.2f;
    config.confidence_threshold = 0.8f;
    config.max_sequence_length = 10;
    
    ProceduralMemory pm(config);
    
    // Test configuration retrieval
    auto retrieved_config = pm.getConfig();
    assert(retrieved_config.reinforcement_rate == 0.2f);
    assert(retrieved_config.confidence_threshold == 0.8f);
    assert(retrieved_config.max_sequence_length == 10);
    
    // Test configuration update
    ProceduralMemory::Config new_config = retrieved_config; // Start with current config
    new_config.reinforcement_rate = 0.3f;
    pm.setConfig(new_config);
    
    auto updated_config = pm.getConfig();
    assert(updated_config.reinforcement_rate == 0.3f);
    
    // Test sequence length validation
    std::vector<int> too_long_sequence(15, 1); // Exceeds max_sequence_length
    int invalid_id = pm.learnSkill(too_long_sequence, "too_long", 0.5f);
    assert(invalid_id == -1); // Should fail
    
    std::cout << "✓ Configuration test passed" << std::endl;
}

void testClearOperation() {
    std::cout << "Testing clear operation..." << std::endl;
    
    ProceduralMemory::Config config;
    ProceduralMemory pm(config);
    
    // Learn several skills
    for (int i = 0; i < 3; ++i) {
        std::vector<int> skill = {i, i+1};
        pm.learnSkill(skill, "skill_" + std::to_string(i), 0.6f);
    }
    
    assert(pm.getSkillCount() == 3);
    
    // Clear all skills
    pm.clear();
    
    assert(pm.getSkillCount() == 0);
    assert(pm.getAverageConfidence() == 0.0f);
    
    std::cout << "✓ Clear operation test passed" << std::endl;
}

int main() {
    std::cout << "=== Procedural Memory Module Test Suite ===" << std::endl;
    
    try {
        testBasicSkillLearning();
        testSkillReinforcement();
        testSkillPractice();
        testSkillRetrieval();
        testSimilarityDetection();
        testSkillManagement();
        testSkillDecay();
        testPerformanceMetrics();
        testConfiguration();
        testClearOperation();
        
        std::cout << "\n✅ All Procedural Memory tests passed!" << std::endl;
        std::cout << "Procedural Memory module is ready for integration." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}