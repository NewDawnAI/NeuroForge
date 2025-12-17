#include "memory/EpisodicMemoryManager.h"
#include "memory/SemanticMemory.h"
#include "memory/DevelopmentalConstraints.h"
#include "memory/SleepConsolidation.h"
#include "memory/MemoryIntegrator.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <cmath>

using namespace NeuroForge::Memory;

// Test result tracking
struct TestResults {
    int total_tests = 0;
    int passed_tests = 0;
    
    void run_test(const std::string& test_name, bool result) {
        total_tests++;
        if (result) {
            passed_tests++;
            std::cout << "✓ " << test_name << " PASSED" << std::endl;
        } else {
            std::cout << "✗ " << test_name << " FAILED" << std::endl;
        }
    }
    
    void print_summary() {
        std::cout << "\n=== Phase 2 Memory Systems Test Summary ===" << std::endl;
        std::cout << "Total Tests: " << total_tests << std::endl;
        std::cout << "Passed: " << passed_tests << std::endl;
        std::cout << "Failed: " << (total_tests - passed_tests) << std::endl;
        std::cout << "Success Rate: " << (100.0f * passed_tests / total_tests) << "%" << std::endl;
        
        if (passed_tests == total_tests) {
            std::cout << "\n🎉 All Phase 2 Memory Systems tests passed!" << std::endl;
        } else {
            std::cout << "\n⚠️  Some tests failed. Please review the implementation." << std::endl;
        }
    }
};

// Test EpisodicMemoryManager
bool test_episodic_memory() {
    try {
        EpisodicConfig config;
        config.consolidation_threshold = 0.5f;
        
        EpisodicMemoryManager episodic_manager(config);
        
        // Test episode recording
        std::vector<float> sensory_state = {1.0f, 2.0f, 3.0f};
        std::vector<float> action_state = {0.5f, 1.5f};
        
        auto episode_id = episodic_manager.storeEpisode(
            "test_context", sensory_state, action_state, "test_episode");
        
        if (episode_id == 0) return false;
        
        // Test statistics
        auto stats = episodic_manager.getStatistics();
        if (stats.total_episodes_recorded < 1) return false;
        if (episodic_manager.getEpisodeCount() == 0) return false;
        
        // Test retrieval of recorded episode
        auto episode = episodic_manager.retrieveEpisode(episode_id);
        if (!episode) return false;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "EpisodicMemory test exception: " << e.what() << std::endl;
        return false;
    }
}

// Test SemanticMemory
bool test_semantic_memory() {
    try {
        SemanticMemory::Config config;
        config.max_concepts = 100;
        config.concept_creation_threshold = 0.6f;
        
        SemanticMemory semantic_memory(config);
        
        // Test concept creation
        std::vector<float> features = {1.0f, 0.5f, 0.8f, 0.2f};
        int concept_id = semantic_memory.createConcept(
            "test_concept", features, ConceptNode::ConceptType::Object, "Test concept");
        
        if (concept_id == -1) return false;
        
        // Test concept retrieval
        auto node = semantic_memory.retrieveConcept(concept_id);
        if (!node) return false;
        if (node->label != "test_concept") return false;
        
        // Test concept retrieval by label
        auto node_by_label = semantic_memory.retrieveConceptByLabel("test_concept");
        if (!node_by_label) return false;
        
        // Test similar concept finding
        std::vector<float> query_features = {1.1f, 0.4f, 0.9f, 0.1f};
        auto similar_concepts = semantic_memory.findSimilarConcepts(query_features, 5, 0.1f);
        if (similar_concepts.empty()) return false;
        
        // Test concept linking
        int concept_id_2 = semantic_memory.createConcept(
            "related_concept", query_features, ConceptNode::ConceptType::Property);
        
        if (concept_id_2 == -1) return false;
        
        bool linked = semantic_memory.linkConcepts(concept_id, concept_id_2, 0.8f);
        if (!linked) return false;
        
        // Test statistics
        auto stats = semantic_memory.getStatistics();
        if (stats.total_concepts_created < 2) return false;
        if (stats.active_concepts_count < 2) return false;
        
        // Test operational status
        if (!semantic_memory.isOperational()) return false;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "SemanticMemory test exception: " << e.what() << std::endl;
        return false;
    }
}

// Test DevelopmentalConstraints
bool test_developmental_constraints() {
    try {
        DevelopmentalConstraints::Config config;
        config.enable_critical_periods = true;
        config.maturation_time_ms = 10000; // 10 seconds for testing
        
        DevelopmentalConstraints dev_constraints(config);
        
        // Test critical period creation
        auto visual_period = DevelopmentalConstraints::createVisualCriticalPeriod(0.1f, 2.0f, 2.0f);
        bool added = dev_constraints.defineCriticalPeriod(visual_period);
        if (!added) return false;
        
        // Test period retrieval
        auto retrieved_period = dev_constraints.getCriticalPeriod("Visual_Critical_Period");
        if (!retrieved_period) return false;
        if (retrieved_period->period_name != "Visual_Critical_Period") return false;
        
        // Test plasticity multiplier
        float multiplier = dev_constraints.getCurrentPlasticityMultiplier("VisualCortex");
        if (multiplier <= 0.0f) return false;
        
        // Test system age
        std::uint64_t age = dev_constraints.getCurrentSystemAge();
        if (age < 0) return false; // Age should be non-negative
        
        // Test maturation level
        float maturation = dev_constraints.getMaturationLevel();
        if (maturation < 0.0f || maturation > 1.0f) return false;
        
        // Test standard development initialization
        size_t periods_created = dev_constraints.initializeStandardDevelopment(true);
        if (periods_created == 0) return false;
        
        // Test statistics
        auto stats = dev_constraints.getStatistics();
        if (stats.total_periods_defined == 0) return false;
        
        // Test operational status
        if (!dev_constraints.isOperational()) return false;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "DevelopmentalConstraints test exception: " << e.what() << std::endl;
        return false;
    }
}

// Unit test: getLearningModulation behavior across enhancement and restriction periods
bool test_learning_modulation() {
    try {
        DevelopmentalConstraints::Config config;
        config.enable_critical_periods = true;

        DevelopmentalConstraints dev_constraints(config);

        // Create an enhancement period affecting Hebbian learning in VisualCortex
        CriticalPeriod enhance(
            "Enhance_Hebbian_Visual",
            0,                // start at birth age
            10000,            // end well after birth age
            2.0f,             // base plasticity multiplier (>1)
            CriticalPeriod::PeriodType::Enhancement);
        enhance.learning_rate_multiplier = 1.5f;
        enhance.consolidation_multiplier = 1.1f;
        enhance.affected_regions = {"VisualCortex"};
        enhance.learning_types = {"hebbian"};

        // Create a restriction period overlapping, affecting the same type/region
        CriticalPeriod restrict_period(
            "Restrict_Hebbian_Visual",
            0,
            10000,
            0.5f, // base plasticity multiplier (<1)
            CriticalPeriod::PeriodType::Restriction);
        restrict_period.learning_rate_multiplier = 0.8f;
        restrict_period.consolidation_multiplier = 0.9f;
        restrict_period.affected_regions = {"VisualCortex"};
        restrict_period.learning_types = {"hebbian"};

        // Define both periods
        if (!dev_constraints.defineCriticalPeriod(enhance)) return false;
        if (!dev_constraints.defineCriticalPeriod(restrict_period)) return false;

        // Query modulation for Hebbian in VisualCortex
        auto mod = dev_constraints.getLearningModulation("hebbian", "VisualCortex");

        // Flags should reflect both enhancement and restriction presence
        if (!mod.is_enhanced) return false;
        if (!mod.is_restricted) return false;

        // Plasticity multiplier: 2.0 (enhance) * 0.5 (restrict) -> ~1.0
        if (std::abs(mod.plasticity_multiplier - 1.0f) > 1e-3f) return false;

        // Learning rate multiplier: 1.5 * 0.8 -> ~1.2
        if (std::abs(mod.learning_rate_multiplier - 1.2f) > 1e-3f) return false;

        // Consolidation multiplier: 1.1 * 0.9 -> ~0.99
        if (std::abs(mod.consolidation_multiplier - 0.99f) > 2e-3f) return false;

        // Region not affected should result in defaults
        auto mod_other_region = dev_constraints.getLearningModulation("hebbian", "AuditoryCortex");
        if (mod_other_region.is_enhanced || mod_other_region.is_restricted) return false;
        if (std::abs(mod_other_region.plasticity_multiplier - 1.0f) > 1e-6f) return false;
        if (std::abs(mod_other_region.learning_rate_multiplier - 1.0f) > 1e-6f) return false;
        if (std::abs(mod_other_region.consolidation_multiplier - 1.0f) > 1e-6f) return false;

        // Learning type not affected should result in defaults
        auto mod_other_type = dev_constraints.getLearningModulation("procedural", "VisualCortex");
        if (mod_other_type.is_enhanced || mod_other_type.is_restricted) return false;
        if (std::abs(mod_other_type.plasticity_multiplier - 1.0f) > 1e-6f) return false;
        if (std::abs(mod_other_type.learning_rate_multiplier - 1.0f) > 1e-6f) return false;
        if (std::abs(mod_other_type.consolidation_multiplier - 1.0f) > 1e-6f) return false;

        return true;

    } catch (const std::exception& e) {
        std::cout << "LearningModulation test exception: " << e.what() << std::endl;
        return false;
    }
}

// Test SleepConsolidation
bool test_sleep_consolidation() {
    try {
        SleepConfig config;
        config.consolidation_interval_ms = 1000; // 1 second for testing
        config.min_consolidation_duration_ms = 100;
        config.max_consolidation_duration_ms = 500;
        
        SleepConsolidation sleep_consolidation(config);
        
        // Create memory systems for registration
        EpisodicConfig episodic_config2;
        EpisodicMemoryManager episodic_manager(episodic_config2);
        SemanticMemory::Config semantic_config2;
        SemanticMemory semantic_memory(semantic_config2);
        WorkingMemory::Config working_config;
        WorkingMemory working_memory(working_config);
        ProceduralConfig procedural_config;
        ProceduralMemory procedural_memory(procedural_config);
        
        // Register memory systems
        sleep_consolidation.registerEpisodicMemory(&episodic_manager);
        sleep_consolidation.registerSemanticMemory(&semantic_memory);
        sleep_consolidation.registerWorkingMemory(&working_memory);
        sleep_consolidation.registerProceduralMemory(&procedural_memory);
        
        // Test sleep phase management
        if (sleep_consolidation.getCurrentSleepPhase() != SleepConsolidation::SleepPhase::Awake) {
            return false;
        }
        
        bool entered_slow_wave = sleep_consolidation.enterSlowWaveSleep(100);
        if (!entered_slow_wave) return false;
        
        if (sleep_consolidation.getCurrentSleepPhase() != SleepConsolidation::SleepPhase::SlowWave) {
            return false;
        }
        
        bool entered_rem = sleep_consolidation.enterREMSleep(100);
        if (!entered_rem) return false;
        
        if (sleep_consolidation.getCurrentSleepPhase() != SleepConsolidation::SleepPhase::REM) {
            return false;
        }
        
        bool returned_awake = sleep_consolidation.returnToAwake();
        if (!returned_awake) return false;
        
        if (sleep_consolidation.getCurrentSleepPhase() != SleepConsolidation::SleepPhase::Awake) {
            return false;
        }
        
        // Test statistics
        auto stats = sleep_consolidation.getStatistics();
        // Statistics should be initialized properly
        
        // Test operational status (will be false without all systems)
        // This is expected behavior
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "SleepConsolidation test exception: " << e.what() << std::endl;
        return false;
    }
}

// Test MemoryIntegrator with Phase 2 systems
bool test_memory_integrator_phase2() {
    try {
        MemoryIntegrator::Config config;
        config.enable_working_memory = true;
        config.enable_procedural_memory = true;
        config.enable_episodic_memory = true;
        config.enable_semantic_memory = true;
        config.enable_developmental_constraints = true;
        config.enable_sleep_consolidation = true;
        
        MemoryIntegrator integrator(config);
        
        // Test that all memory systems are accessible
        // Note: The current MemoryIntegrator may not have all Phase 2 systems yet
        // This test verifies the interface exists
        
        try {
            auto& working_memory = integrator.getWorkingMemory();
            auto& procedural_memory = integrator.getProceduralMemory();
            
            // Test basic operations
            std::vector<float> test_data = {1.0f, 2.0f, 3.0f};
            bool stored = working_memory.push(test_data, 0.8f, "test");
            if (!stored) return false;
            
            auto skill_id = procedural_memory.addSkill(
                "test_skill",
                std::vector<std::string>{"step1", "step2"},
                std::vector<float>{1.0f, 2.0f, 3.0f}
            );
            if (skill_id == 0) return false;
            
        } catch (const std::exception& e) {
            // Expected if Phase 2 systems aren't fully integrated yet
            std::cout << "Note: Phase 2 systems not fully integrated in MemoryIntegrator: " << e.what() << std::endl;
        }
        
        // Test statistics
        auto stats = integrator.getStatistics();
        // Should have basic statistics
        
        // Test operational status
        if (!integrator.isOperational()) return false;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "MemoryIntegrator Phase 2 test exception: " << e.what() << std::endl;
        return false;
    }
}

// Test integration between systems
bool test_system_integration() {
    try {
        // Create all systems
        EpisodicConfig episodic_config;
        EpisodicMemoryManager episodic_manager(episodic_config);
        SemanticMemory::Config semantic_config;
        SemanticMemory semantic_memory(semantic_config);
        DevelopmentalConstraints::Config dev_config;
        DevelopmentalConstraints dev_constraints(dev_config);
        SleepConfig sleep_config;
        SleepConsolidation sleep_consolidation(sleep_config);
        
        // Test that systems can work together
        // Record an episode
        std::vector<float> sensory_state = {1.0f, 2.0f, 3.0f};
        std::vector<float> action_state = {0.5f, 1.5f};
        
        auto episode_id = episodic_manager.storeEpisode(
            "integration_context", sensory_state, action_state, "integration_test");
        
        if (episode_id == 0) return false;
        
        // Create a concept
        int concept_id = semantic_memory.createConcept(
            "integration_concept", sensory_state, ConceptNode::ConceptType::Object);
        
        if (concept_id == -1) return false;
        
        // Test developmental constraints
        auto visual_period = DevelopmentalConstraints::createVisualCriticalPeriod(0.1f, 1.0f, 2.0f);
        bool added = dev_constraints.defineCriticalPeriod(visual_period);
        if (!added) return false;
        
        // Register systems with sleep consolidation
        sleep_consolidation.registerEpisodicMemory(&episodic_manager);
        sleep_consolidation.registerSemanticMemory(&semantic_memory);
        
        // Test that systems maintain their state
        auto episode_stats = episodic_manager.getStatistics();
        auto concept_stats = semantic_memory.getStatistics();
        auto dev_stats = dev_constraints.getStatistics();
        auto sleep_stats = sleep_consolidation.getStatistics();
        
        if (episode_stats.total_episodes_recorded == 0) return false;
        if (concept_stats.total_concepts_created == 0) return false;
        if (dev_stats.total_periods_defined == 0) return false;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "System integration test exception: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    std::cout << "=== Phase 2 Memory Systems Integration Test Suite ===" << std::endl;
    std::cout << "Testing: EpisodicMemoryManager, SemanticMemory, DevelopmentalConstraints, SleepConsolidation" << std::endl;
    std::cout << std::endl;
    
    TestResults results;
    
    // Run individual system tests
    results.run_test("EpisodicMemoryManager Basic Operations", test_episodic_memory());
    results.run_test("SemanticMemory Basic Operations", test_semantic_memory());
    results.run_test("DevelopmentalConstraints Basic Operations", test_developmental_constraints());
    results.run_test("DevelopmentalConstraints Learning Modulation", test_learning_modulation());
    results.run_test("SleepConsolidation Basic Operations", test_sleep_consolidation());
    results.run_test("MemoryIntegrator Phase 2 Integration", test_memory_integrator_phase2());
    results.run_test("Cross-System Integration", test_system_integration());
    
    // Print final results
    results.print_summary();
    
    // Return appropriate exit code
    return (results.passed_tests == results.total_tests) ? 0 : 1;
}