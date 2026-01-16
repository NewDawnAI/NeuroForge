#pragma once

#include "core/Types.h"
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <random>

namespace NeuroForge {
namespace Memory { class DevelopmentalConstraints; }
namespace Core {

class HypergraphBrain; // forward declaration

class LearningSystem {
public:
    struct Statistics {
        // Reward telemetry
        float cumulative_reward = 0.0f;
        float last_reward = 0.0f;
        std::size_t reward_events = 0;
        // Learning update telemetry
        uint64_t total_updates = 0;
        uint64_t hebbian_updates = 0;
        uint64_t stdp_updates = 0;
        uint64_t reward_updates = 0;
        uint64_t potentiated_synapses = 0;
        uint64_t depressed_synapses = 0;
        float average_weight_change = 0.0f;
        uint64_t attention_modulation_events = 0;
        float mean_attention_weight = 0.0f;
        // System-level
        uint64_t active_synapses = 0;
        float memory_consolidation_rate = 0.0f;
        uint64_t consolidation_events = 0;
        // M7: Intrinsic motivation signals
        float uncertainty_signal = 0.0f;
        float surprise_signal = 0.0f;
        float prediction_error = 0.0f;
        float intrinsic_motivation = 0.0f;
        float avg_energy = 0.0f;
        float metabolic_hazard = 0.0f;
    };

    struct Config {
        // Base learning rates
        float global_learning_rate = 0.01f;
        float hebbian_rate = 0.0f;
        float stdp_rate = 0.0f;
        float stdp_rate_multiplier = 1.0f;
        float decay_rate = 0.0f;
        bool enable_homeostasis = false;
        float homeostasis_eta = 0.0f;

        // Attention modulation
        bool enable_attention_modulation = false;
        float attention_boost_factor = 1.0f; // base boost factor
        float attention_Amin = 1.0f;
        float attention_Amax = 2.0f;
        int attention_anneal_ms = 0; // remaining-time based decay window
        enum class AttentionMode { Off, ExternalMap, Saliency, TopK };
        AttentionMode attention_mode = AttentionMode::Off;

        // Update cadence and gating
        std::chrono::milliseconds update_interval{16};
        float p_gate = 1.0f; // stochastic gating probability for sparse plasticity

        // Novelty shaping
        float novelty_obs_weight = 1.0f;
        float novelty_substrate_weight = 0.0f;

        // Competence shaping
        enum class CompetenceMode { Off, EMA, ScaleLearningRates, ScalePGate };
        CompetenceMode competence_mode = CompetenceMode::EMA;
        float competence_rho = 0.1f; // EMA rate for competence updates

        // Consolidation
        float consolidation_strength = 0.0f;

        // M7: Intrinsic motivation parameters
        bool enable_intrinsic_motivation = false;
        float uncertainty_weight = 0.1f;
        float surprise_weight = 0.1f;
        float prediction_error_weight = 0.1f;
        float intrinsic_motivation_decay = 0.95f;
        int prediction_history_size = 10;

        // Phase-5 additional fields used in main.cpp
        int chaos_steps = 0;
        int consolidate_steps = 0;
        int novelty_window = 1;
        float prune_threshold = 0.0f;

        // Optional GPU acceleration preference (honored only when CUDA is available)
        bool prefer_gpu = false;

        // Structural plasticity (neurogenesis/pruning/synaptogenesis)
        bool enable_structural_plasticity = false;
        float structural_prune_threshold = 0.05f;
        std::size_t structural_spawn_batch = 0;
        std::size_t structural_grow_batch = 0;
        float structural_energy_gate = 0.5f;
        std::size_t structural_interval_steps = 100;
        std::size_t structural_max_regions_per_cycle = 1;
    };

    // Convenience aliases so implementation can use unqualified names
    using CompetenceMode = Config::CompetenceMode;
    using AttentionMode = Config::AttentionMode;

    // Algorithms for statistics
    enum class Algorithm { Hebbian, STDP, RewardModulated };

    // Runtime synapse state alias used in implementation
    struct SynState { float eligibility = 0.0f; };
    using SynapseRuntime = SynState;

    struct SynapseSnapshot {
        NeuroForge::NeuronID pre_neuron{};
        NeuroForge::NeuronID post_neuron{};
        float weight = 0.0f;
    };

    enum class ConsolidationPhase { Consolidation };

    LearningSystem() = default;
    LearningSystem(NeuroForge::Core::HypergraphBrain* brain, const Config& config);
    ~LearningSystem();

    bool initialize();
    void shutdown();

    void updateConfig(const Config& cfg);
    void updateLearning(float delta_time);

    // STDP / Hebbian helpers
    void applySTDPLearning(NeuroForge::RegionID region_id,
                           const std::vector<NeuroForge::SynapsePtr>& synapses,
                           const std::unordered_map<NeuroForge::NeuronID, NeuroForge::TimePoint>& spike_times);
    void applyHebbianLearning(NeuroForge::RegionID region_id, float learning_rate);

    // Memory consolidation
    void consolidateMemories(const std::vector<NeuroForge::RegionID>& regions,
                             ConsolidationPhase phase);
    // Overload for callers that don't care about phase
    void consolidateMemories(const std::vector<NeuroForge::RegionID>& regions);

    // ===== Phase 4: Reward-Modulated Plasticity =====
    void notePrePost(NeuroForge::SynapseID sid, float pre, float post);
    void applyExternalReward(float r);
    void configurePhase4(float lambda, float etaElig, float kappa, float alpha, float gamma, float eta);
    float getElig(NeuroForge::SynapseID sid) const;

    // Shaped reward
    float computeShapedReward(const std::vector<float>& obs,
                              const std::vector<float>& regionActs,
                              float taskReward);

    // Attention modulation entry point
    void applyAttentionModulation(const std::unordered_map<NeuroForge::NeuronID, float>& attention_map,
                                  float learning_boost);

    void setRandomSeed(std::uint32_t seed);

    // Toggle automatic eligibility accumulation used by HypergraphBrain post-processing
    void setAutoEligibilityAccumulation(bool enabled);
    bool isAutoEligibilityAccumulationEnabled() const;

    // Developmental constraints integration
    void setDevelopmentalConstraints(NeuroForge::Memory::DevelopmentalConstraints* constraints);

    // Mimicry API (Phase A bridging)
    inline void setMimicryEnabled(bool enabled) {
        std::lock_guard<std::mutex> lg(mimicry_mutex_);
        mimicry_enabled_ = enabled;
    }
    inline bool isMimicryEnabled() const {
        std::lock_guard<std::mutex> lg(mimicry_mutex_);
        return mimicry_enabled_;
    }
    inline void setMimicryWeight(float mu) {
        std::lock_guard<std::mutex> lg(mimicry_mutex_);
        mimicry_weight_mu_ = mu;
    }
    inline void setTeacherVector(const std::vector<float>& teacher) {
        std::lock_guard<std::mutex> lg(mimicry_mutex_);
        teacher_embed_ = teacher;
    }
    inline void setStudentEmbedding(const std::vector<float>& student) {
        std::lock_guard<std::mutex> lg(mimicry_mutex_);
        student_embed_ = student;
    }

    inline float getLastMimicrySim() const {
        std::lock_guard<std::mutex> lg(mimicry_mutex_);
        return last_mimicry_sim_;
    }

    // Internalize Phase A: gate use of Phase A attempt scores inside computeShapedReward
    inline void setMimicryInternal(bool enabled) {
        std::lock_guard<std::mutex> lg(mimicry_mutex_);
        mimicry_internal_enabled_ = enabled;
    }
    // Getter added for M3 to allow components to query internalization state
    inline bool isMimicryInternalEnabled() const {
        // Reading a bool without lock is fine but keep consistent with existing patterns
        std::lock_guard<std::mutex> lg(mimicry_mutex_);
        return mimicry_internal_enabled_;
    }
    // Provide last Phase A attempt scores (used when mimicry-internal is enabled)
    inline void setMimicryAttemptScores(float similarity, float novelty, float total_reward, bool success) {
        std::lock_guard<std::mutex> lg(mimicry_mutex_);
        last_phase_a_similarity_ = similarity;
        last_phase_a_novelty_ = novelty;
        last_phase_a_total_reward_ = total_reward;
        last_phase_a_success_ = success;
        has_phase_a_scores_ = true;
        // Keep last_mimicry_sim_ coherent with Phase A similarity for telemetry
        last_mimicry_sim_ = similarity;
    }

    // Telemetry accessors
    inline float getLastSubstrateSimilarity() const { return last_substrate_similarity_; }
    inline float getLastSubstrateNovelty() const { return last_substrate_novelty_; }
    inline float getCompetenceLevel() const { return competence_level_.load(std::memory_order_relaxed); }

    Statistics getStatistics() const;
    void resetStatistics();

    // Configuration accessor
    const Config& getConfig() const;

    // Global learning-rate control
    void setLearningRate(float lr);
    float getLearningRate() const;

    // Attention inspection helpers
    float getLastAttentionBoostBase() const;

    // Region querying helpers
    std::vector<NeuroForge::SynapsePtr> getRegionSynapses(NeuroForge::RegionID region_id) const;
    std::vector<NeuroForge::NeuronPtr> getRegionNeurons(NeuroForge::RegionID region_id) const;
    std::vector<SynapseSnapshot> getSynapseSnapshot() const;

    // Structural plasticity orchestrator
    void applyStructuralPlasticity(NeuroForge::RegionID region_id);

    // Event hooks
    void updateSpikeTime(NeuroForge::NeuronID neuron_id, NeuroForge::TimePoint spike_time);
    void onNeuronSpike(NeuroForge::NeuronID neuron_id, NeuroForge::TimePoint spike_time);

    // M7: Intrinsic motivation methods
    float calculateUncertaintySignal() const;
    float calculateSurpriseSignal(const std::vector<float>& current_state);
    float calculatePredictionError(const std::vector<float>& predicted_state, 
                                   const std::vector<float>& actual_state);
    float getIntrinsicMotivation() const;
    void updateIntrinsicMotivation(const std::vector<float>& current_state);

    // M6/M7: Substrate and autonomous operation methods
    void setSubstrateTrainingMode(bool enabled);
    bool isSubstrateTrainingMode() const;
    void setScaffoldElimination(bool enabled);
    bool isScaffoldEliminationEnabled() const;
    void setMotivationDecay(float decay);
    float getMotivationDecay() const;
    void setExplorationBonus(float bonus);
    float getExplorationBonus() const;
    void setNoveltyMemorySize(std::size_t size);
    std::size_t getNoveltyMemorySize() const;

    // Utilities
    float calculateSTDPDelta(NeuroForge::TimePoint pre_time, NeuroForge::TimePoint post_time) const;
    void applyWeightDecay(const std::vector<NeuroForge::SynapsePtr>& synapses);
    void applyHomeostasis(NeuroForge::RegionID region_id);
    void updateStatistics(LearningSystem::Algorithm algorithm, float weight_change);
    NeuroForge::SynapsePtr findSynapseById(NeuroForge::SynapseID sid) const;

private:
    // Core brain & config
    NeuroForge::Core::HypergraphBrain* brain_ = nullptr;
    Config config_{};

    // Activity flags
    std::atomic<bool> is_active_{false};
    std::atomic<bool> is_paused_{false};

    // STDP spike timing cache
    mutable std::mutex spike_times_mutex_;
    std::unordered_map<NeuroForge::NeuronID, NeuroForge::TimePoint> last_spike_times_;

    // Phase 4 runtime state
    std::unordered_map<NeuroForge::SynapseID, SynState> syn_state_;
    mutable std::mutex syn_state_mutex_;
    std::atomic<float> pending_reward_{0.0f};

    // RNG for stochastic gating
    std::mt19937 rng_{std::random_device{ }()};
    std::uniform_real_distribution<float> dist01_{0.0f, 1.0f};

    // Phase 4 parameters
    float lambda_ = 0.9f;
    float etaElig_ = 1.0f;
    float kappa_ = 0.15f;
    float alpha_ = 0.2f;
    float gamma_ = 1.0f;
    float eta_ = 0.05f;

    // Observation running mean for novelty
    std::vector<float> obs_mean_;
    // Substrate (region activations) running mean for novelty
    std::vector<float> region_mean_;

    // Attention runtime state
    std::unordered_map<NeuroForge::NeuronID, float> attention_weights_;
    float last_attention_boost_base_ = 1.0f;
    int attention_anneal_elapsed_ms_ = 0;
    float attention_boost_effective_ = 1.0f;

    // Consolidation
    std::mutex consolidation_mutex_;
    std::unordered_map<NeuroForge::RegionID, float> consolidation_strengths_;

    Statistics statistics_{};

    // ===== Mimicry state =====
    mutable std::mutex mimicry_mutex_;
    bool mimicry_enabled_ = false;
    float mimicry_weight_mu_ = 0.0f;
    std::vector<float> teacher_embed_;
    std::vector<float> student_embed_;
    float last_mimicry_sim_ = 0.0f;
    // Internalized Phase A attempt state
    bool mimicry_internal_enabled_ = false;
    bool has_phase_a_scores_ = false;
    float last_phase_a_similarity_ = 0.0f;
    float last_phase_a_novelty_ = 0.0f;
    float last_phase_a_total_reward_ = 0.0f;
    bool last_phase_a_success_ = false;

    // Milestone-3 telemetry state
    float last_substrate_similarity_ = 0.0f;
    float last_substrate_novelty_ = 0.0f;
    std::atomic<float> competence_level_{0.0f};

    // Auto eligibility accumulation toggle (default disabled)
    std::atomic<bool> auto_eligibility_accumulation_enabled_{false};

    // M7: Intrinsic motivation state
    mutable std::mutex intrinsic_motivation_mutex_;
    std::vector<std::vector<float>> prediction_history_;
    float current_uncertainty_ = 0.0f;
    float current_surprise_ = 0.0f;
    float current_prediction_error_ = 0.0f;
    float current_intrinsic_motivation_ = 0.0f;
    std::vector<float> last_state_;

    // Developmental constraints (optional)
    NeuroForge::Memory::DevelopmentalConstraints* developmental_constraints_{nullptr};

    // M6/M7: Substrate and autonomous operation state
    std::atomic<bool> substrate_training_mode_{false};
    std::atomic<bool> scaffold_elimination_enabled_{false};
    std::atomic<float> motivation_decay_{0.95f};
    std::atomic<float> exploration_bonus_{0.2f};
    std::atomic<std::size_t> novelty_memory_size_{100};

    std::atomic<std::uint64_t> last_structural_cycle_{0};
};

} // namespace Core
} // namespace NeuroForge
