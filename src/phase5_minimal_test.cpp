#include <iostream>
#include <chrono>
#include <memory>
#include <thread>

// Minimal includes for testing
#include "core/HypergraphBrain.h"
#include "core/LanguageSystem.h"
#include "connectivity/ConnectivityManager.h"

using namespace NeuroForge;
using namespace NeuroForge::Core;

class MinimalPhase5Test {
private:
    std::unique_ptr<HypergraphBrain> brain_;
    std::unique_ptr<LanguageSystem> language_system_;
    
public:
    MinimalPhase5Test() {
        std::cout << "=== Minimal Phase 5 Initialization Test ===\n\n";
    }
    
    bool testStep1_BrainCreation() {
        std::cout << "Step 1: Creating HypergraphBrain..." << std::flush;
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            // Create a connectivity manager first
            auto connectivity_manager = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
            
            // Create brain with connectivity manager
            brain_ = std::make_unique<HypergraphBrain>(connectivity_manager);
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start);
            
            if (brain_) {
                std::cout << " ✅ (" << elapsed.count() << "ms)\n";
                return true;
            } else {
                std::cout << " ❌ Failed to create brain (" << elapsed.count() << "ms)\n";
                return false;
            }
            
        } catch (const std::exception& e) {
            std::cout << " ❌ Exception: " << e.what() << "\n";
            return false;
        }
    }
    
    bool testStep2_BrainInitialization() {
        std::cout << "Step 2: Initializing HypergraphBrain..." << std::flush;
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            if (!brain_) {
                std::cout << " ❌ Brain not created\n";
                return false;
            }
            
            bool result = brain_->initialize();
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start);
            
            if (result) {
                std::cout << " ✅ (" << elapsed.count() << "ms)\n";
                return true;
            } else {
                std::cout << " ❌ Failed (" << elapsed.count() << "ms)\n";
                return false;
            }
            
        } catch (const std::exception& e) {
            std::cout << " ❌ Exception: " << e.what() << "\n";
            return false;
        }
    }
    
    bool testStep3_RegionCreation() {
        std::cout << "Step 3: Creating brain regions..." << std::flush;
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            if (!brain_) {
                std::cout << " ❌ Brain not initialized\n";
                return false;
            }
            
            // Create a minimal set of regions for testing
            auto visual_region = brain_->createRegion("Visual", Region::Type::Cortical);
            auto language_region = brain_->createRegion("Language", Region::Type::Cortical);
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start);
            
            if (visual_region && language_region) {
                std::cout << " ✅ (" << elapsed.count() << "ms)\n";
                return true;
            } else {
                std::cout << " ❌ Failed to create regions (" << elapsed.count() << "ms)\n";
                return false;
            }
            
        } catch (const std::exception& e) {
            std::cout << " ❌ Exception: " << e.what() << "\n";
            return false;
        }
    }
    
    bool testStep4_NeuronCreation() {
        std::cout << "Step 4: Creating neurons in regions..." << std::flush;
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            if (!brain_) {
                std::cout << " ❌ Brain not initialized\n";
                return false;
            }
            
            // Get regions and create minimal neurons
            const auto& regions = brain_->getRegions();
            
            for (const auto& [region_id, region] : regions) {
                if (region) {
                    // Create just a few neurons for testing
                    region->createNeurons(10);
                }
            }
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start);
            
            std::cout << " ✅ (" << elapsed.count() << "ms)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << " ❌ Exception: " << e.what() << "\n";
            return false;
        }
    }
    
    bool testStep5_LanguageSystemCreation() {
        std::cout << "Step 5: Creating LanguageSystem..." << std::flush;
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            // Create LanguageSystem config
            LanguageSystem::Config config;
            config.max_vocabulary_size = 1000;
            config.enable_vision_grounding = false;
            config.enable_audio_grounding = false;
            config.enable_action_grounding = false;
            config.enable_teacher_mode = false;
            
            language_system_ = std::make_unique<LanguageSystem>(config);
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start);
            std::cout << " ✅ (" << elapsed.count() << "ms)\n";
            return true;
            
        } catch (const std::exception& e) {
            std::cout << " ❌ Exception: " << e.what() << "\n";
            return false;
        }
    }
    
    bool testStep6_LanguageSystemInitialization() {
        std::cout << "Step 6: Initializing LanguageSystem..." << std::flush;
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            if (!language_system_) {
                std::cout << " ❌ LanguageSystem not created\n";
                return false;
            }
            
            bool result = language_system_->initialize();
            
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start);
            
            if (result) {
                std::cout << " ✅ (" << elapsed.count() << "ms)\n";
                return true;
            } else {
                std::cout << " ❌ Failed (" << elapsed.count() << "ms)\n";
                return false;
            }
            
        } catch (const std::exception& e) {
            std::cout << " ❌ Exception: " << e.what() << "\n";
            return false;
        }
    }
    
    void runAllTests() {
        std::cout << "Starting systematic initialization test...\n\n";
        
        bool success = true;
        
        success &= testStep1_BrainCreation();
        if (!success) { std::cout << "❌ FAILED at Step 1\n"; return; }
        
        success &= testStep2_BrainInitialization();
        if (!success) { std::cout << "❌ FAILED at Step 2\n"; return; }
        
        success &= testStep3_RegionCreation();
        if (!success) { std::cout << "❌ FAILED at Step 3\n"; return; }
        
        success &= testStep4_NeuronCreation();
        if (!success) { std::cout << "❌ FAILED at Step 4\n"; return; }
        
        success &= testStep5_LanguageSystemCreation();
        if (!success) { std::cout << "❌ FAILED at Step 5\n"; return; }
        
        success &= testStep6_LanguageSystemInitialization();
        if (!success) { std::cout << "❌ FAILED at Step 6\n"; return; }
        
        std::cout << "\n✅ ALL TESTS PASSED - Initialization successful!\n";
        std::cout << "The blocking issue is likely in subsequent steps or configuration.\n";
    }
};

int main(int argc, char* argv[]) {
    try {
        std::cout << "Phase 5 Language Demo - Minimal Initialization Test\n";
        std::cout << "This test isolates each initialization step to identify blocking points.\n\n";
        
        MinimalPhase5Test test;
        test.runAllTests();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred\n";
        return 1;
    }
}