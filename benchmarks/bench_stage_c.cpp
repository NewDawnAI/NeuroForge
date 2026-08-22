#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <random>
#include <map>
#include <algorithm>
#include <iomanip>

#include "core/SubstratePhaseC.h"
#include "core/HypergraphBrain.h"
#include "core/SubstrateWorkingMemory.h"
#include "core/metrics/IntegrationHeuristics.h"
#include <set>
#include "connectivity/ConnectivityManager.h"

using namespace NeuroForge::Core;

// Helper to create a minimal brain
std::shared_ptr<HypergraphBrain> createBrain() {
    auto conn_manager = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
    return std::make_shared<HypergraphBrain>(conn_manager);
}

// Benchmark 1: Memory Capacity
// Measures how many unique bindings can be maintained in Working Memory before degradation.
void benchmarkMemoryCapacity() {
    std::cout << "\n=== Benchmark: Memory Capacity ===\n";
    std::cout << "Target: Determine max unique bindings with >90% recall.\n";
    
    for (int load = 5; load <= 20; load += 5) {
        std::cout << "Starting Load " << load << "..." << std::endl;
        
        std::cout << "Creating brain..." << std::endl;
    auto brain = createBrain();
    
    // Enable learning for memory formation
    if (auto ls = brain->getLearningSystem()) {
        auto config = ls->getConfig();
        config.global_learning_rate = 0.05f;
        config.hebbian_rate = 0.01f;
        config.stdp_rate = 0.01f;
        ls->updateConfig(config);
    }

    SubstrateWorkingMemory::Config wm_config;
        auto wm = std::make_shared<SubstrateWorkingMemory>(brain, wm_config);
        SubstratePhaseC::Config phase_c_config;
        phase_c_config.binding_regions = 24; // 12 pairs for high capacity
    phase_c_config.neurons_per_region = 128; // Reduced for speed
    phase_c_config.recurrent_strength = 0.9f; // Strong recurrence for persistence
    phase_c_config.binding_threshold = 0.3f; // Lower threshold to detect weak signals
    phase_c_config.competition_strength = 0.05f; // Low competition to allow higher capacity
    std::cout << "Creating Phase C..." << std::endl;
    auto phase_c = std::make_shared<SubstratePhaseC>(brain, wm, phase_c_config);
    
    std::cout << "Initializing Phase C..." << std::endl;
    phase_c->initialize();
    std::cout << "Phase C initialized." << std::endl;
    
        // Generate bindings
        std::vector<std::pair<std::string, std::string>> binding_pairs;
        for (int i = 0; i < load; ++i) {
            binding_pairs.push_back({"Role_" + std::to_string(i), "Filler_" + std::to_string(i)});
        }
        
        // Train/Inject
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (const auto& b : binding_pairs) {
            std::map<std::string, std::string> params;
            params["role"] = b.first;
            params["filler"] = b.second;
            
            // Activate for 5 steps
            for (int k = 0; k < 5; ++k) {
                phase_c->setGoal("binding", params);
                brain->processStep(0.1f);
                phase_c->processStep(k, 0.1f);
            }
        }
        
        // Delay/Maintenance (e.g. 100 steps)
        for (int k = 0; k < 100; ++k) {
            brain->processStep(0.1f);
            phase_c->processStep(100 + k, 0.1f);
        }
        
        // Recall/Check
        auto results = phase_c->getBindingResults(200);
        
        int correct = 0;
        std::set<std::pair<std::string, std::string>> retrieved;
        for (const auto& res : results) {
             retrieved.insert({res.role, res.filler});
        }
        
        for (const auto& b : binding_pairs) {
            if (retrieved.find(b) != retrieved.end()) {
                correct++;
            }
        }
        
        float recall = static_cast<float>(correct) / load;
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        
        std::cout << "Load: " << load << " | Recall: " << std::fixed << std::setprecision(2) << recall * 100.0f 
                  << "% | Time: " << duration << "ms" << std::endl;
                  
        if (recall < 0.9f) {
            std::cout << "Capacity limit reached at load " << load << std::endl;
            break;
        }
    }
}

// Benchmark 2: Learning Rate
// Measures convergence speed for a simple sequence prediction task.
void benchmarkLearningRate() {
    std::cout << "\n=== Benchmark: Learning Rate ===\n";
    std::cout << "Target: Steps to reach < 0.1 prediction error on Sequence A->B.\n";
    
    auto brain = createBrain();

    // Enable learning for sequence acquisition
    if (auto ls = brain->getLearningSystem()) {
        auto config = ls->getConfig();
        config.global_learning_rate = 0.05f;
        config.hebbian_rate = 0.02f; // Higher for rapid sequence learning
        config.stdp_rate = 0.02f;
        ls->updateConfig(config);
    }

    SubstrateWorkingMemory::Config wm_config;
    auto wm = std::make_shared<SubstrateWorkingMemory>(brain, wm_config);
    SubstratePhaseC::Config phase_c_config;
    auto phase_c = std::make_shared<SubstratePhaseC>(brain, wm, phase_c_config);
    phase_c->initialize();
    
    // Task: A -> B
    // We'll repeatedly present A and check if B is predicted.
    
    int max_steps = 1000;
    bool converged = false;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Determine where "B" should be (force token registration)
    phase_c->getTokenIndex("A");
    phase_c->getTokenIndex("B");
    
    for (int step = 0; step < max_steps; ++step) {
        // Revised Strategy: Pulse "A"
        // Present "A" for 5 steps, then stop for 5 steps. Check prediction in the gap.
        // During gap, "A" decays, "B" (triggered) might persist or fire?
        
        std::map<std::string, std::string> params;
        bool input_phase = (step % 20) < 10;
        
        if (input_phase) {
             params["sequence_input"] = "A";
             phase_c->setGoal("sequence", params);
        }
        
        brain->processStep(0.1f);
        phase_c->processStep(step, 0.1f);
        
        // Check result
        auto result = phase_c->getSequenceResult(step);
        
        float error = 1.0f;
        if (result.predicted == "B") {
             error = 0.0f; // Perfect
        } else if (result.predicted == "A") {
             if (!input_phase) error = 0.5f; // "A" lingering
             else error = 1.0f; // Masked by input
        }
        
        if (step % 50 == 0) {
             std::cout << "Step " << step << " | Error: " << error << " | Pred: " << result.predicted << " | Phase: " << (input_phase ? "Input" : "Gap") << std::endl;
        }
        
        if (error < 0.1f) {
            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            std::cout << "Converged in " << step << " steps (" << duration << "ms)." << std::endl;
            converged = true;
            break;
        }
        
        // Provide Feedback/Target "B" to drive learning when "A" is ending or in gap
        // (Simulating external supervision or successful outcome)
        if (!input_phase && step % 20 == 10) { // Just once per cycle
            params["sequence_target"] = "B";
            phase_c->setGoal("sequence_learn", params); 
            // Note: sequence_learn logic needs to be implemented in SubstratePhaseC or just use "sequence" with target?
            // Existing code uses "sequence" with "target".
            // Let's use that.
             std::map<std::string, std::string> train_params;
             train_params["target"] = "B";
             phase_c->setGoal("sequence", train_params);
        }
    }
    
    if (!converged) {
        std::cout << "Failed to converge within " << max_steps << " steps." << std::endl;
    }
}

int main() {
    try {
        benchmarkMemoryCapacity();
        benchmarkLearningRate();
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
