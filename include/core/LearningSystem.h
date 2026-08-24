#pragma once

#include <unordered_set>
#include "EligibilityTraces.h"
#include "core/Types.h"
#include <atomic>
#include <chrono>
#include <mutex>
#include <random>
#include <unordered_map>
#include <vector>
#include "core/DeterministicRng.h"

namespace NeuroForge {
namespace Memory {
class DevelopmentalConstraints;
}
namespace Core {

// Forward declaration for Universal Learning Signal
class UniversalLearningSignal;

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

    // Optional GPU acceleration preference (honored only when CUDA is
    // available)
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
  struct SynState {
    float eligibility = 0.0f;
  };
  using SynapseRuntime = SynState;

  struct SynapseSnapshot {
    NeuroForge::NeuronID pre_neuron{};
    NeuroForge::NeuronID post_neuron{};
    float weight = 0.0f;
  };

  enum class ConsolidationPhase { Consolidation };

  LearningSystem() = default;
  LearningSystem(NeuroForge::Core::HypergraphBrain *brain,
                 const Config &config);
  ~LearningSystem();

  bool initialize();
  void shutdown();

  void updateConfig(const Config &cfg);
  void updateLearning(float delta_time);

  // STDP / Hebbian helpers
  void applySTDPLearning(
      NeuroForge::RegionID region_id,
      const std::vector<NeuroForge::SynapsePtr> &synapses,
      const std::unordered_map<NeuroForge::NeuronID, NeuroForge::TimePoint>
          &spike_times);
  void applyHebbianLearning(NeuroForge::RegionID region_id,
                            float learning_rate);

  // Memory consolidation
  void consolidateMemories(const std::vector<NeuroForge::RegionID> &regions,
                           ConsolidationPhase phase);
  // Overload for callers that don't care about phase
  void consolidateMemories(const std::vector<NeuroForge::RegionID> &regions);

  // ===== Phase 4: Reward-Modulated Plasticity =====
  void notePrePost(NeuroForge::SynapseID sid, float pre, float post);
  void applyExternalReward(float r);
  void configurePhase4(float lambda, float etaElig, float kappa, float alpha,
                       float gamma, float eta);
  float getElig(NeuroForge::SynapseID sid) const;

  // Shaped reward
  float computeShapedReward(const std::vector<float> &obs,
                            const std::vector<float> &regionActs,
                            float taskReward);

  // Attention modulation entry point
  void applyAttentionModulation(
      const std::unordered_map<NeuroForge::NeuronID, float> &attention_map,
      float learning_boost);

  void setRandomSeed(std::uint32_t seed);

  // Toggle automatic eligibility accumulation used by HypergraphBrain
  // post-processing
  /**
   * @brief Restrict eligibility to synapses landing on these neurons.
   *
   * WHY
   *
   * onNeuronSpike used to bump EVERY synapse of EVERY spiking neuron by a flat
   * 0.1. With 83% of neurons active in the anatomical brain, that trace records
   * "was recently active" rather than "was responsible", and
   * dw = kappa*R*eligibility becomes approximately one scalar applied to
   * everything -- shifting all activations together and preserving their
   * ordering, which is exactly what a decision rule cares about. Measured
   * 2026-08-24: 237,409 Phase-4 updates moved the choice distribution from
   * {18,46,33,2}% to {18,46,34,2}%.
   *
   * Credit assignment is the entire content of a reinforcement learning
   * algorithm. A uniform trace is not a weak version of it; it is its absence.
   *
   * WHAT THIS DOES
   *
   * Passing the neurons of the SELECTED action channel confines eligibility to
   * the pathway that produced the action taken, so reward reaches those synapses
   * and not the ones that merely happened to be firing. This is the
   * basal-ganglia arrangement: the selected channel is disinhibited and only it
   * is eligible for reinforcement; the losing channels are not.
   *
   * An empty set restores the previous behaviour, where every spiking neuron
   * accumulates.
   */
  void setEligibleTargets(const std::vector<NeuroForge::NeuronID> &ids);
  void clearEligibleTargets();
  std::size_t eligibleTargetCount() const;

  /**
   * @brief Scale for the three-factor eligibility increment.
   *
   * The increment is `rate * pre_activation * post_activation` rather than a
   * constant, so a synapse earns credit in proportion to the coincidence of its
   * own endpoints. A synapse attached to a spiking neuron but whose source was
   * silent contributed nothing and now receives nothing.
   */
  void setEligibilityRate(float rate);

  /**
   * @brief Per-step multiplier applied to every eligibility trace.
   *
   * 1.0 disables decay and reproduces the previous behaviour, where a trace
   * saturated and never fell. Lower values give the trace a time constant, which
   * is what lets a reward be attributed to a recent action rather than to
   * anything that has ever fired. See EligibilityTraces::decay().
   */
  void setEligibilityDecay(float lambda);

  /**
   * @brief Subtract a running estimate of expected reward before applying it.
   *
   * WHY
   *
   * The Phase-4 rule is `dw = kappa * R * eligibility * lr`, with no baseline.
   * In the phototaxis task R is the CHANGE in brightness, so for a near-random
   * policy its mean is approximately zero: an action that closes the distance is
   * rewarded and the next that opens it is punished by a similar magnitude. The
   * update therefore alternates sign and cancels -- a high-variance, zero-mean
   * random walk on the weights.
   *
   * Measured 2026-08-24, three runs per condition with credit assignment and
   * motor-channel selection both correct: potentiated 5.5-5.9M against depressed
   * 4.1-4.5M, nearly balanced every time, and a behavioural effect of +0.011
   * against a within-condition spread of 0.317. Reward reached the weights and
   * went nowhere.
   *
   * The learned prefrontal policy succeeds on the SAME reward signal because its
   * update carries `(1[a==chosen] - p[a])`, an advantage term. Credit assignment
   * decides WHICH synapses; a baseline decides HOW MUCH BETTER THAN EXPECTED the
   * outcome was. Without the second, correct credit assignment just delivers
   * well-aimed noise.
   *
   * Biologically this is the difference between reward and reward PREDICTION
   * ERROR. Dopamine encodes the latter; the rule without a baseline is the
   * pre-Schultz model.
   *
   * Off by default, so existing behaviour is unchanged.
   */
  /**
   * @brief Use node perturbation for the eligibility trace.
   *
   * WHY
   *
   * The three-factor trace is `rate * pre * post`, a COINCIDENCE measure. Reward
   * times coincidence is reward-modulated Hebbian: it strengthens whatever was
   * co-active when reward was positive, and does not ascend the gradient of
   * expected reward. Measured 2026-08-24, with credit assignment and
   * motor-channel selection both correct and a reward baseline added, the
   * substrate still sat at the random-walk baseline.
   *
   * Node perturbation replaces `post` with `post - E[post]`, the neuron's
   * deviation from its own running expectation:
   *
   *     trace = rate * pre * (post - E[post])
   *
   * That is a local estimator of the policy gradient. The deviation is the
   * "perturbation"; correlating it with reward estimates which way the weight
   * should move, in the same way REINFORCE's `(1[a==chosen] - p[a])` does for an
   * explicit softmax — but computed from quantities available AT THE SYNAPSE,
   * which is the architectural commitment this project has made.
   *
   * Traces become signed: a neuron firing below its expectation yields a
   * negative trace, and that sign carries the information.
   *
   * @param rate EMA rate for each neuron's expected activation.
   */
  /**
   * @brief Learn V(s) and modulate plasticity by TD error instead of raw reward.
   *
   * WHY A SCALAR BASELINE WAS NOT ENOUGH
   *
   * `--reward-baseline` subtracts a running MEAN of reward. Measured 2026-08-24
   * that is a null (+0.006 against a within-condition spread of 0.367), because
   * reward here is a CHANGE in brightness whose mean is approximately zero: a
   * mean-zero baseline subtracts nothing. A baseline only reduces variance when
   * the reward has a non-zero mean.
   *
   * A STATE-DEPENDENT baseline does not have that problem. V(s) estimates the
   * reward expected FROM THIS STATE, so the modulator
   *
   *     td = R + gamma * V(s') - V(s)
   *
   * stays informative even when raw reward averages to zero: it asks whether the
   * state improved, not whether the reward was positive. This is what dopamine
   * is usually modelled as encoding, and the rule without it is the pre-Schultz
   * model.
   *
   * V is linear in the features supplied by observeState(), learned online by
   * `w += lr * td * phi(s)`.
   *
   * @param lr     critic learning rate
   * @param gamma  discount on the successor state's value
   */
  void setCritic(bool enabled, float lr = 0.05f, float gamma = 0.9f);
  bool isCriticEnabled() const;
  /// Current V(s) for the most recently observed state.
  float criticValue() const;

  /**
   * @brief Supply the state features the critic evaluates.
   *
   * Call once per cycle. The system keeps the previous feature vector, so when a
   * reward arrives it can form `R + gamma*V(s') - V(s)` with s the state the
   * action was taken in and s' the state that followed.
   */
  void observeState(const std::vector<float> &features);

  void setNodePerturbation(bool enabled, float rate = 0.05f);
  bool isNodePerturbationEnabled() const;

  void setRewardBaseline(bool enabled, float rate = 0.01f);
  bool isRewardBaselineEnabled() const;
  /// Current running estimate of expected reward.
  float rewardBaseline() const;

  void setAutoEligibilityAccumulation(bool enabled);
  bool isAutoEligibilityAccumulationEnabled() const;

  // Developmental constraints integration
  void setDevelopmentalConstraints(
      NeuroForge::Memory::DevelopmentalConstraints *constraints);

  // Universal Learning Signal integration
  void setUniversalSignal(UniversalLearningSignal *signal);
  UniversalLearningSignal *getUniversalSignal() const;

  // Mimicry API (Phase A bridging)
  inline void setMimicryEnabled(bool enabled) {
    std::lock_guard<std::mutex> lg(mimicry_mutex_);
    mimicry_enabled_ = enabled;
  }
  inline void setMimicryWeight(float mu) {
    std::lock_guard<std::mutex> lg(mimicry_mutex_);
    mimicry_weight_mu_ = mu;
  }
  inline void setTeacherVector(const std::vector<float> &teacher) {
    std::lock_guard<std::mutex> lg(mimicry_mutex_);
    teacher_embed_ = teacher;
  }
  inline void setStudentEmbedding(const std::vector<float> &student) {
    std::lock_guard<std::mutex> lg(mimicry_mutex_);
    student_embed_ = student;
  }

  inline float getLastMimicrySim() const {
    std::lock_guard<std::mutex> lg(mimicry_mutex_);
    return last_mimicry_sim_;
  }

  // Internalize Phase A: gate use of Phase A attempt scores inside
  // computeShapedReward
  inline void setMimicryInternal(bool enabled) {
    std::lock_guard<std::mutex> lg(mimicry_mutex_);
    mimicry_internal_enabled_ = enabled;
  }
  // Getter added for M3 to allow components to query internalization state
  inline bool isMimicryInternalEnabled() const {
    // Reading a bool without lock is fine but keep consistent with existing
    // patterns
    std::lock_guard<std::mutex> lg(mimicry_mutex_);
    return mimicry_internal_enabled_;
  }
  // Provide last Phase A attempt scores (used when mimicry-internal is enabled)
  inline void setMimicryAttemptScores(float similarity, float novelty,
                                      float total_reward, bool success) {
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
  inline float getLastSubstrateSimilarity() const {
    return last_substrate_similarity_;
  }
  inline float getLastSubstrateNovelty() const {
    return last_substrate_novelty_;
  }
  inline float getCompetenceLevel() const {
    return competence_level_.load(std::memory_order_relaxed);
  }

  Statistics getStatistics() const;

  /// Recount existing input synapses across the brain and store the result in
  /// statistics_.active_synapses. Called from getStatistics(); it used to run
  /// inside updateStatistics() on every weight update, which made each
  /// simulation step O(synapses x neurons). See the note at its definition.
  void refreshActiveSynapseCount() const;
  void resetStatistics();

  // Configuration accessor
  const Config &getConfig() const;

  // Global learning-rate control
  void setLearningRate(float lr);
  float getLearningRate() const;

  // Attention inspection helpers
  float getLastAttentionBoostBase() const;

  // Region querying helpers
  std::vector<NeuroForge::SynapsePtr>
  getRegionSynapses(NeuroForge::RegionID region_id) const;
  std::vector<NeuroForge::NeuronPtr>
  getRegionNeurons(NeuroForge::RegionID region_id) const;
  std::vector<SynapseSnapshot> getSynapseSnapshot() const;

  // Structural plasticity orchestrator
  void applyStructuralPlasticity(NeuroForge::RegionID region_id);

  // Event hooks
  void updateSpikeTime(NeuroForge::NeuronID neuron_id,
                       NeuroForge::TimePoint spike_time);
  void onNeuronSpike(NeuroForge::NeuronID neuron_id,
                     NeuroForge::TimePoint spike_time);

  // M7: Intrinsic motivation methods
  float calculateUncertaintySignal() const;
  float calculateSurpriseSignal(const std::vector<float> &current_state);
  float calculatePredictionError(const std::vector<float> &predicted_state,
                                 const std::vector<float> &actual_state);
  float getIntrinsicMotivation() const;
  void updateIntrinsicMotivation(const std::vector<float> &current_state);

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
  float calculateSTDPDelta(NeuroForge::TimePoint pre_time,
                           NeuroForge::TimePoint post_time) const;
  void applyWeightDecay(const std::vector<NeuroForge::SynapsePtr> &synapses);
  void applyHomeostasis(NeuroForge::RegionID region_id);
  void updateStatistics(LearningSystem::Algorithm algorithm,
                        float weight_change);
  NeuroForge::SynapsePtr findSynapseById(NeuroForge::SynapseID sid) const;

private:
  // Core brain & config
  NeuroForge::Core::HypergraphBrain *brain_ = nullptr;
  Config config_{};

  // Activity flags
  std::atomic<bool> is_active_{false};
  std::atomic<bool> is_paused_{false};

  // STDP spike timing cache
  mutable std::mutex spike_times_mutex_;
  std::unordered_map<NeuroForge::NeuronID, NeuroForge::TimePoint>
      last_spike_times_;

  // Phase 4 runtime state
  /// Per-synapse eligibility, flat and lock-free. Replaces an
  /// unordered_map<SynapseID, SynState> that held a single float per entry and
  /// was written ~128 times per spike under syn_state_mutex_ -- see
  /// EligibilityTraces.h for the measurements that motivated the change.
  EligibilityTraces elig_;
  /// Empty = accumulate for every spiking neuron (the original behaviour).
  std::unordered_set<NeuroForge::NeuronID> eligible_targets_;
  mutable std::mutex eligible_targets_mutex_;
  std::atomic<float> eligibility_rate_{0.5f};
  std::atomic<float> eligibility_decay_{0.85f};
  std::atomic<bool> reward_baseline_enabled_{false};
  std::atomic<float> reward_baseline_{0.0f};
  std::atomic<float> reward_baseline_rate_{0.01f};
  std::atomic<bool> critic_enabled_{false};
  float critic_lr_ = 0.05f;
  float critic_gamma_ = 0.9f;
  std::vector<float> critic_w_;
  std::vector<float> critic_prev_phi_;
  std::vector<float> critic_curr_phi_;
  bool critic_have_prev_ = false;
  mutable std::mutex critic_mutex_;
  std::atomic<bool> node_perturbation_enabled_{false};
  std::atomic<float> node_perturbation_rate_{0.05f};
  /// Per-neuron running expectation of activation, for node perturbation.
  std::unordered_map<NeuroForge::NeuronID, float> activation_expectation_;
  mutable std::mutex activation_expectation_mutex_;
  /// NOTE: this mutex no longer guards eligibility. It still guards rng_,
  /// attention_weights_ and statistics_, which are unrelated state that happened
  /// to share it.
  mutable std::mutex syn_state_mutex_;
  /// id -> synapse, filled on first successful lookup.
  ///
  /// findSynapseById() walks every region's internal synapses, input connection
  /// vectors and output connection vectors looking for one id -- a full scan of
  /// the connectome per call. The Phase-4 reward path calls it once per synapse
  /// carrying eligibility, so with reward arriving every cycle that became
  /// thousands of full scans per cycle. Measured 2026-08-24: the closed-loop
  /// task with --auto-eligibility=on did not finish 60 steps in 500 seconds.
  ///
  /// weak_ptr so a pruned synapse expires here rather than being resurrected;
  /// an expired entry falls back to the scan and re-caches.
  mutable std::mutex synapse_cache_mutex_;
  mutable std::unordered_map<NeuroForge::SynapseID,
                             std::weak_ptr<NeuroForge::Synapse>>
      synapse_cache_;
  std::atomic<float> pending_reward_{0.0f};

  // RNG for stochastic gating
  std::mt19937 rng_{NeuroForge::Core::DeterministicRng::seedFor("LearningSystem")};
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

  /// mutable: refreshActiveSynapseCount() updates the synapse count from the
  /// const read path, where the count is actually needed.
  mutable Statistics statistics_{};

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
  NeuroForge::Memory::DevelopmentalConstraints *developmental_constraints_{
      nullptr};

  // M6/M7: Substrate and autonomous operation state
  std::atomic<bool> substrate_training_mode_{false};
  std::atomic<bool> scaffold_elimination_enabled_{false};
  std::atomic<float> motivation_decay_{0.95f};
  std::atomic<float> exploration_bonus_{0.2f};
  std::atomic<std::size_t> novelty_memory_size_{100};

  std::atomic<std::uint64_t> last_structural_cycle_{0};

  // Universal Learning Signal (non-owning, set externally)
  UniversalLearningSignal *universal_signal_{nullptr};
};

} // namespace Core
} // namespace NeuroForge
