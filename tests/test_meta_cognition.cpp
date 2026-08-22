#include <iostream>
#include <cassert>
#include <memory>
#include <cmath>
#include "core/MetaCognitionSystem.h"
#include "core/SubstratePhaseCAdapter.h"
#include "core/SubstratePhaseC.h"

// Mock Adapter to capture config changes
class MockAdapter : public NeuroForge::Core::SubstratePhaseCAdapter {
public:
    MockAdapter() : NeuroForge::Core::SubstratePhaseCAdapter() {}

    // Override to store config locally
    NeuroForge::Core::SubstratePhaseC::Config getConfig() const override {
        return config_;
    }

    void setConfig(const NeuroForge::Core::SubstratePhaseC::Config& config) override {
        config_ = config;
        config_updated_ = true;
    }

    NeuroForge::Core::SubstratePhaseC::Config config_;
    bool config_updated_ = false;
};

void test_state_classification() {
    std::cout << "Testing State Classification..." << std::endl;
    
    NeuroForge::Core::MetaCognitionSystem::Config config;
    config.analysis_window = 10; // Short window for testing
    NeuroForge::Core::MetaCognitionSystem meta_system(nullptr, 0, config);
    
    // Test 1: Stable State
    // Feed stable stats: CIP ~ 0.8, Reward ~ 0.5, No Ethics Violations
    for (int i = 0; i < 20; ++i) {
        NeuroForge::Core::SubstratePhaseC::Statistics stats;
        stats.average_coherence = 0.8f;
        meta_system.analyze(i, stats, 0.5f, false);
    }
    
    auto state = meta_system.getState();
    if (state.dominant_state == "stable") {
        std::cout << "PASS: Correctly identified STABLE state." << std::endl;
    } else {
        std::cout << "FAIL: Expected STABLE, got " << state.dominant_state << std::endl;
        exit(1);
    }

    // Test 2: Chaotic State
    // Feed chaotic stats: CIP < 0.5
    for (int i = 0; i < 20; ++i) {
        NeuroForge::Core::SubstratePhaseC::Statistics stats;
        stats.average_coherence = 0.3f;
        meta_system.analyze(i + 20, stats, 0.1f, false);
    }
    
    state = meta_system.getState();
    if (state.dominant_state == "chaotic") {
        std::cout << "PASS: Correctly identified CHAOTIC state." << std::endl;
    } else {
        std::cout << "FAIL: Expected CHAOTIC, got " << state.dominant_state << std::endl;
        exit(1);
    }

    // Test 3: Unethical State
    // Feed ethics violations
    for (int i = 0; i < 20; ++i) {
        NeuroForge::Core::SubstratePhaseC::Statistics stats;
        stats.average_coherence = 0.8f;
        meta_system.analyze(i + 40, stats, 1.0f, true); // Always violation
    }
    
    state = meta_system.getState();
    if (state.dominant_state == "unethical") {
        std::cout << "PASS: Correctly identified UNETHICAL state." << std::endl;
    } else {
        std::cout << "FAIL: Expected UNETHICAL, got " << state.dominant_state << std::endl;
        exit(1);
    }
}

void test_regulation() {
    std::cout << "Testing Regulation Logic..." << std::endl;
    
    NeuroForge::Core::MetaCognitionSystem::Config config;
    config.analysis_window = 10;
    NeuroForge::Core::MetaCognitionSystem meta_system(nullptr, 0, config);
    
    // Create a mock adapter (we need a shared_ptr to pass to regulate)
    // Since we can't easily instantiate a full SubstratePhaseCAdapter without a Brain,
    // we'll rely on the fact that regulate() calls getConfig() and setConfig() which are virtual.
    // Wait, SubstratePhaseCAdapter constructor takes references/pointers that are used in initialization.
    // Creating a full mock might be tricky if the constructor dereferences them.
    // Let's check SubstratePhaseCAdapter constructor again.
    // It has a constructor that takes (SubstratePhaseC&, HypergraphBrain&, PhaseCCSVLogger&).
    // It stores pointers. It does NOT dereference them in constructor body of that overload.
    
    // We need dummy objects for the reference constructor
    // Use reinterpret_cast to fake references for the MockAdapter base constructor (dangerous but standard for mocking without DI)
    // Actually, MockAdapter defined above does exactly this.
    
    auto adapter = std::make_shared<MockAdapter>();
    
    // Set initial config
    NeuroForge::Core::SubstratePhaseC::Config init_config;
    init_config.binding_coherence_min = 0.5f;
    init_config.competition_strength = 0.5f;
    adapter->setConfig(init_config);
    adapter->config_updated_ = false;

    // Case 1: Chaotic -> PID should increase Coherence Threshold
    // Target CIP is usually 0.95. Current is 0.3. Error = 0.65.
    // PID (Kp=0.1) -> Adjustment ~ 0.065. Scaled by 0.1 -> 0.0065 added to threshold.
    // Let's pump stats to make it chaotic
    for (int i = 0; i < 20; ++i) {
        NeuroForge::Core::SubstratePhaseC::Statistics stats;
        stats.average_coherence = 0.3f;
        meta_system.analyze(i, stats, 0.0f, false);
    }
    
    meta_system.regulate(adapter);
    
    if (adapter->config_updated_) {
        std::cout << "PASS: Regulation updated config." << std::endl;
        std::cout << "  Old Threshold: " << init_config.binding_coherence_min << std::endl;
        std::cout << "  New Threshold: " << adapter->config_.binding_coherence_min << std::endl;
        
        if (adapter->config_.binding_coherence_min > init_config.binding_coherence_min) {
             std::cout << "PASS: Correctly INCREASED threshold to stabilize chaos." << std::endl;
        } else {
             std::cout << "FAIL: Threshold did not increase." << std::endl;
             exit(1);
        }
    } else {
        std::cout << "FAIL: Regulation did not trigger update." << std::endl;
        exit(1);
    }

    // Case 2: Unethical -> Should increase Competition Strength
    adapter->config_updated_ = false;
    for (int i = 0; i < 20; ++i) {
        NeuroForge::Core::SubstratePhaseC::Statistics stats;
        stats.average_coherence = 0.8f;
        meta_system.analyze(i + 100, stats, 0.0f, true); // Violations
    }
    
    meta_system.regulate(adapter);
    
    if (adapter->config_.competition_strength > 0.5f) {
        std::cout << "PASS: Correctly INCREASED competition strength to curb unethical behavior." << std::endl;
        std::cout << "  New Competition: " << adapter->config_.competition_strength << std::endl;
    } else {
        std::cout << "FAIL: Competition strength did not increase." << std::endl;
        exit(1);
    }
}

int main() {
    try {
        test_state_classification();
        test_regulation();
        std::cout << "All MetaCognition tests passed." << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
