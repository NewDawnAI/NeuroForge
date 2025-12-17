#include "core/SubstratePhaseC.h"
#include "core/PhaseC.h"
#include "biases/SurvivalBias.h"
#include <memory>

namespace NeuroForge {
namespace Core {

/**
 * @brief Adapter class that integrates SubstratePhaseC with existing Phase C interface
 * 
 * This adapter allows seamless migration from external Phase C computation to 
 * substrate-driven behavior while maintaining compatibility with existing code.
 */
class SubstratePhaseCAdapter {
public:
    SubstratePhaseCAdapter(std::shared_ptr<HypergraphBrain> brain,
                          std::shared_ptr<SubstrateWorkingMemory> working_memory,
                          PhaseCCSVLogger& logger)
        : brain_(brain), working_memory_(working_memory), logger_(logger) {
        
        // Initialize substrate Phase C
        SubstratePhaseC::Config config;
    substrate_phase_c_ = std::make_unique<SubstratePhaseC>(brain, working_memory, config);
    substrate_phase_c_->initialize();
    // Wire logger JSON sink into substrate for telemetry
    substrate_phase_c_->setJsonSink(logger_.getJsonSink());
    }
    
    /**
     * @brief Substrate-driven binding step (replaces external computation)
     */
    void stepBinding(int step) {
        // Set binding goal in substrate instead of external computation
        std::map<std::string, std::string> binding_params;
        
        // Generate binding parameters from substrate state or external input
        std::vector<std::string> colors = {"red", "green", "blue"};
        std::vector<std::string> shapes = {"square", "circle", "triangle"};
        
        binding_params["color"] = colors[step % colors.size()];
        binding_params["shape"] = shapes[(step / 2) % shapes.size()];
        
        // Set goal in substrate
        substrate_phase_c_->setGoal("binding", binding_params);
        
        // Process substrate step
        substrate_phase_c_->processStep(step, 0.1f);
        
        // Get results from substrate behavior
        auto binding_results = substrate_phase_c_->getBindingResults(step);
        
        // Log results using existing logger interface
        for (const auto& binding : binding_results) {
            logger_.logBinding(binding);
        }
        
        // Get assemblies for timeline logging
        auto assemblies = substrate_phase_c_->getCurrentAssemblies();
        if (!assemblies.empty()) {
            // Convert substrate assembly to Phase C assembly format
            Assembly winner;
            winner.id = 0;
            winner.symbol = assemblies[0].symbol;
            winner.score = assemblies[0].coherence_score;
            
            logger_.logTimeline(step, winner);
            
            // Log all assemblies
            std::vector<Assembly> phase_c_assemblies;
            for (std::size_t i = 0; i < assemblies.size(); ++i) {
                Assembly assembly;
                assembly.id = static_cast<int>(i);
                assembly.symbol = assemblies[i].symbol;
                assembly.score = assemblies[i].coherence_score;
                phase_c_assemblies.push_back(assembly);
            }
            logger_.logAssemblies(step, phase_c_assemblies);
        }
        
        // Log working memory state
        if (working_memory_) {
            auto bindings = working_memory_->getCurrentBindings();
            std::vector<NeuroForge::WorkingMemoryItem> wm_items;
            for (const auto& binding : bindings) {
                NeuroForge::WorkingMemoryItem item;
                item.role = binding.role_label;
                item.filler = binding.filler_label;
                item.strength = binding.strength;
                wm_items.push_back(item);
            }
            logger_.logWorkingMemory(step, wm_items);
        }
    }
    
    /**
     * @brief Substrate-driven sequence step (replaces external computation)
     */
    void stepSequence(int step) {
        // Set sequence goal in substrate
        std::map<std::string, std::string> sequence_params;
        std::vector<std::string> seq_tokens = {"A", "B", "C", "D"};
        
        sequence_params["target"] = seq_tokens[step % seq_tokens.size()];
        
        // Set goal in substrate
        substrate_phase_c_->setGoal("sequence", sequence_params);
        
        // Process substrate step
        substrate_phase_c_->processStep(step, 0.1f);
        
        // Get sequence result from substrate behavior
        auto sequence_result = substrate_phase_c_->getSequenceResult(step);
        
        // Log sequence result
        if (!sequence_result.predicted.empty()) {
            logger_.logSequence(sequence_result);
        }
        
        // Get assemblies for timeline logging
        auto assemblies = substrate_phase_c_->getCurrentAssemblies();
        if (!assemblies.empty()) {
            // Find best sequence assembly
            Assembly winner;
            winner.id = 0;
            winner.symbol = sequence_result.predicted;
            winner.score = 0.8f; // Default score
            
            for (const auto& assembly : assemblies) {
                if (assembly.symbol.find("sequence") != std::string::npos) {
                    winner.symbol = assembly.symbol;
                    winner.score = assembly.coherence_score;
                    break;
                }
            }
            
            logger_.logTimeline(step, winner);
            
            // Log all assemblies
            std::vector<Assembly> phase_c_assemblies;
            for (std::size_t i = 0; i < assemblies.size(); ++i) {
                Assembly assembly;
                assembly.id = static_cast<int>(i);
                assembly.symbol = assemblies[i].symbol;
                assembly.score = assemblies[i].coherence_score;
                phase_c_assemblies.push_back(assembly);
            }
            logger_.logAssemblies(step, phase_c_assemblies);
        }
        
        // Log working memory state
        if (working_memory_) {
            auto bindings = working_memory_->getCurrentBindings();
            std::vector<NeuroForge::WorkingMemoryItem> wm_items;
            for (const auto& binding : bindings) {
                NeuroForge::WorkingMemoryItem item;
                item.role = binding.role_label;
                item.filler = binding.filler_label;
                item.strength = binding.strength;
                wm_items.push_back(item);
            }
            logger_.logWorkingMemory(step, wm_items);
        }
    }
    
    /**
     * @brief Get substrate statistics
     */
    SubstratePhaseC::Statistics getSubstrateStatistics() const {
        if (substrate_phase_c_) {
            return substrate_phase_c_->getStatistics();
        }
        return SubstratePhaseC::Statistics{};
    }
    
    /**
     * @brief Set working memory parameters
     */
    void setWorkingMemoryParams(std::size_t capacity, float decay) {
        if (working_memory_) {
            // Update working memory configuration
            SubstrateWorkingMemory::Config config;
            config.max_binding_capacity = capacity;
            config.decay_rate = decay;
            working_memory_->updateConfig(config);
        }
    }
    
    /**
     * @brief Set sequence window
     */
    void setSequenceWindow(std::size_t window) {
        // Update substrate Phase C configuration without resetting other fields
        if (substrate_phase_c_) {
            substrate_phase_c_->setMaxAssemblies(window);
        }
    }

    /**
     * @brief Enable or disable SurvivalBias-based reward emission per step
     */
    void setEmitSurvivalRewards(bool enable) {
        if (substrate_phase_c_) {
            substrate_phase_c_->setEmitSurvivalRewards(enable);
        }
    }

    /**
     * @brief Inject SurvivalBias into substrate to modulate assembly coherence
     */
    void setSurvivalBias(std::shared_ptr<NeuroForge::Biases::SurvivalBias> bias) {
        if (substrate_phase_c_) {
            substrate_phase_c_->setSurvivalBias(std::move(bias));
        }
    }

private:
    std::shared_ptr<HypergraphBrain> brain_;
    std::shared_ptr<SubstrateWorkingMemory> working_memory_;
    PhaseCCSVLogger& logger_;
    std::unique_ptr<SubstratePhaseC> substrate_phase_c_;
};

} // namespace Core
} // namespace NeuroForge