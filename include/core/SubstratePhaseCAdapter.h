#pragma once

#include "core/SubstratePhaseC.h"
#include "core/PhaseC.h"
#include "biases/SurvivalBias.h"
#include "core/FirstPersonMazeRenderer.h"
#include <memory>
#include <map>
#include <string>

namespace NeuroForge {
namespace Sandbox {
    class WebSandbox;
}

namespace Core {

class LanguageSystem;
class Phase6Reasoner;

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
                          PhaseCCSVLogger& logger,
                          const SubstratePhaseC::Config& cfg = SubstratePhaseC::Config());

    /**
     * @brief Construct adapter wrapping an existing SubstratePhaseC instance
     */
    SubstratePhaseCAdapter(SubstratePhaseC& phase_c,
                          HypergraphBrain& brain,
                          PhaseCCSVLogger& logger);
    
    /**
     * @brief Substrate-driven binding step (replaces external computation)
     */
    void stepBinding(int step);
    
    /**
     * @brief Substrate-driven sequence step (replaces external computation)
     */
    void stepSequence(int step);
    
    /**
     * @brief Get substrate statistics
     */
    SubstratePhaseC::Statistics getSubstrateStatistics() const;
    
    /**
     * @brief Set working memory parameters
     */
    void setWorkingMemoryParams(std::size_t capacity, float decay);
    
    /**
     * @brief Set sequence window
     */
    void setSequenceWindow(std::size_t window);

    /**
     * @brief Enable or disable SurvivalBias-based reward emission per step
     */
    void setEmitSurvivalRewards(bool enable);

    /**
     * @brief Inject SurvivalBias into substrate to modulate assembly coherence
     */
    void setSurvivalBias(std::shared_ptr<NeuroForge::Biases::SurvivalBias> bias);

    /**
     * @brief Set hazard coherence modulation weight
     */
    void setHazardCoherenceWeight(float weight);

    /**
     * @brief Set survival reward scale
     */
    void setSurvivalRewardScale(float scale);

    /**
     * @brief Process a percept through the substrate (general purpose)
     */
    void processPercept(const std::unordered_map<std::string, std::string>& percept, int step);

    /**
     * @brief Set Language System for acoustic/multimodal integration
     */
    void setLanguageSystem(std::shared_ptr<LanguageSystem> ls);
    
    /**
     * @brief Set Phase 6 Reasoner for language-reasoning integration
     */
    void setPhase6Reasoner(std::shared_ptr<Phase6Reasoner> reasoner);

    /**
     * @brief Get current substrate configuration
     */
    virtual SubstratePhaseC::Config getConfig() const;

    /**
     * @brief Update substrate configuration
     */
    virtual void setConfig(const SubstratePhaseC::Config& config);

    /**
     * @brief Process a single step of embodiment interaction
     * 
     * @param agent Current agent state (updated in-place)
     * @param renderer Renderer to generate visual input
     * @param step Current simulation step
     * @param audio_input Optional audio input buffer (PCM samples)
     * @return Reward received in this step
     */
    float processEmbodimentStep(FirstPersonMazeRenderer::AgentState& agent, 
                               FirstPersonMazeRenderer& renderer, 
                               int step,
                               const std::vector<float>& audio_input = {});

    /**
     * @brief Process a single step of passive observation (e.g., watching video)
     * 
     * @param visual_input Visual frame data (flattened)
     * @param step Current simulation step
     * @param audio_input Optional audio input buffer (PCM samples)
     * @return Reward received in this step (intrinsic only)
     */
    float processObservationStep(const std::vector<float>& visual_input,
                                int step,
                                const std::vector<float>& audio_input = {});

    struct EmbodimentMetrics {
        float cip = 0.0f;                    // Coherence Integration (avg coherence)
        float self_model_confidence = 0.0f;  // Prediction confidence
        float adaptation_rate = 0.0f;        // Average steps to success (lower is better)
        float success_rate = 0.0f;           // Successes / Trials
        int steps_in_current_episode = 0;
        int total_episodes = 0;
        int successful_episodes = 0;
        long long total_success_steps = 0;
    };

    /**
     * @brief Process a single step of web interaction
     * 
     * @param step Current simulation step
     * @param visual_input Visual frame data (flattened)
     * @return Reward received in this step
     */
    float processWebStep(int step, const std::vector<float>& visual_input);

    /**
     * @brief Set Web Sandbox for autonomous web interaction
     */
    void setWebSandbox(std::shared_ptr<NeuroForge::Sandbox::WebSandbox> sandbox);

    EmbodimentMetrics getEmbodimentMetrics() const { return metrics_; }

protected:
    // Protected constructor for testing/mocking
    SubstratePhaseCAdapter() = default;

private:
    std::shared_ptr<HypergraphBrain> brain_;
    HypergraphBrain* brain_ptr_ = nullptr;
    std::shared_ptr<SubstrateWorkingMemory> working_memory_;
    PhaseCCSVLogger* logger_ = nullptr;
    std::unique_ptr<SubstratePhaseC> owned_phase_c_;
    SubstratePhaseC* substrate_phase_c_;
    std::shared_ptr<LanguageSystem> language_system_;
    std::shared_ptr<Phase6Reasoner> reasoner_;
    std::shared_ptr<NeuroForge::Sandbox::WebSandbox> web_sandbox_;

    EmbodimentMetrics metrics_;
    std::string decodeMotorCommand(const std::string& predicted_token);
    std::string decodeWebAction(const std::string& predicted_token, int step);
};

} // namespace Core
} // namespace NeuroForge
