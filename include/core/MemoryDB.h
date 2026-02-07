#pragma once

// SQLite3 support is optional and controlled via build defines (NF_HAVE_SQLITE3)
// Do NOT force-enable here; rely on CMake/toolchain to define NF_HAVE_SQLITE3 when available.

#include <string>
#include <mutex>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/LearningSystem.h"

namespace NeuroForge {
namespace Core {

// Lightweight SQLite-backed memory database for telemetry and episodic logs
class MemoryDB {
public:
    explicit MemoryDB(const std::string& path);
    ~MemoryDB();

    // Open database (creates file if it does not exist)
    bool open();
    void close();
    bool isOpen() const noexcept;

    // Ensure required tables exist
    bool ensureSchema();

    // Enable or disable verbose debug logging for DB operations
    void setDebug(bool enabled) noexcept; // newly added
    bool getDebug() const noexcept;       // newly added

    // Begin a run session, returns run id
    bool beginRun(const std::string& metadata_json, std::int64_t& out_run_id);

    // Insert learning statistics snapshot
    bool insertLearningStats(std::int64_t ts_ms,
                             std::uint64_t step,
                             double processing_hz,
                             const LearningSystem::Statistics& stats,
                             std::int64_t run_id);

    // Insert experience record
    bool insertExperience(std::int64_t ts_ms,
                          std::uint64_t step,
                          const std::string& tag,
                          const std::string& input_json,
                          const std::string& output_json,
                          bool significant,
                          std::int64_t run_id,
                          std::int64_t& out_experience_id);

    // Insert reward log record
    bool insertRewardLog(std::int64_t ts_ms,
                         std::uint64_t step,
                         double reward,
                         const std::string& source,
                         const std::string& context_json,
                         std::int64_t run_id,
                         std::int64_t& out_reward_id);

    // Insert self-model snapshot record
    bool insertSelfModel(std::int64_t ts_ms,
                         std::uint64_t step,
                         const std::string& state_json,
                         double confidence,
                         std::int64_t run_id,
                         std::int64_t& out_self_model_id);

    // Insert episode record (start of episode)
    bool insertEpisode(const std::string& name,
                       std::int64_t start_ms,
                       std::int64_t run_id,
                       std::int64_t& out_episode_id);

    // Update episode record (end of episode)
    bool updateEpisodeEnd(std::int64_t episode_id,
                          std::int64_t end_ms);

    // Upsert episode-level metrics (success flag, steps taken, and episodic return)
    bool upsertEpisodeStats(std::int64_t episode_id,
                            std::uint64_t steps,
                            bool success,
                            double episode_return);

    // Link experience to episode
    bool linkExperienceToEpisode(std::int64_t experience_id,
                                  std::int64_t episode_id);

    struct RewardEntry {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        std::uint64_t step{0};
        double reward{0.0};
        std::string source;
        std::string context_json;
    };

    struct EpisodeEntry {
        std::int64_t id{0};
        std::string name;
        std::int64_t start_ms{0};
        std::int64_t end_ms{0}; // 0 if ongoing
    };

    struct RunEntry {
        std::int64_t id{0};
        std::int64_t started_ms{0};
        std::string metadata_json;
    };

    struct RunEventEntry {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        std::uint64_t step{0};
        std::string type;
        std::string message;
        int exit_code{0};
        double rss_mb{0.0};
        double gpu_mem_mb{0.0};
    };

    std::vector<RewardEntry> getRecentRewards(std::int64_t run_id, int limit);
    std::vector<RewardEntry> getRewardsBetween(std::int64_t run_id,
                                               std::int64_t start_ts_ms,
                                               std::int64_t end_ts_ms,
                                               int limit = 1000);
    std::vector<EpisodeEntry> getEpisodes(std::int64_t run_id);
    std::vector<RunEntry> getRuns();

    bool insertRunEvent(std::int64_t run_id,
                        std::int64_t ts_ms,
                        std::uint64_t step,
                        const std::string& type,
                        const std::string& message,
                        int exit_code,
                        double rss_mb,
                        double gpu_mem_mb,
                        std::int64_t& out_event_id);
    std::vector<RunEventEntry> getRecentRunEvents(std::int64_t run_id, int n = 50);

    // Utility: last reward_updates counter for sanity check
    bool getLatestRewardUpdates(std::int64_t run_id, std::uint64_t& out_reward_updates);

    // Substrate states
    bool insertSubstrateState(std::int64_t ts_ms,
                              std::uint64_t step,
                              const std::string& state_type,  // "synapse_weights", "neuron_states", "hippocampal_snapshot"
                              const std::string& region_id,
                              const std::string& serialized_data,
                              std::int64_t run_id,
                              std::int64_t& out_state_id);

    // Insert hippocampal snapshot data
    bool insertHippocampalSnapshot(std::int64_t ts_ms,
                                   std::uint64_t step,
                                   double priority,
                                   double significance,
                                   const std::string& snapshot_data,
                                   std::int64_t run_id,
                                   std::int64_t& out_snapshot_id);

    // Query substrate states by type and time range
    struct SubstrateStateEntry {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        std::uint64_t step{0};
        std::string state_type;
        std::string region_id;
        std::string serialized_data;
    };

    std::vector<SubstrateStateEntry> getSubstrateStates(std::int64_t run_id,
                                                         const std::string& state_type = "",
                                                         std::int64_t start_ms = 0,
                                                         std::int64_t end_ms = 0,
                                                         int limit = 100);

    // Hippocampal snapshots
    struct HippocampalSnapshotEntry {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        std::uint64_t step{0};
        double priority{0.0};
        double significance{0.0};
        std::string snapshot_data;
    };

    std::vector<HippocampalSnapshotEntry> getHippocampalSnapshots(std::int64_t run_id,
                                                                   double min_priority = 0.0,
                                                                   int limit = 50);

    struct EmbeddingEntry {
        std::int64_t ts_ms{0};
        std::uint64_t step{0};
        std::string content_id;
        std::string state_type;
        std::vector<float> vec;
        std::string meta_json;
    };
    std::vector<EmbeddingEntry> getEmbeddings(std::int64_t run_id,
                                              const std::string& state_type,
                                              int limit);


    // Phase 6: Options and verifications
    bool insertOption(std::int64_t ts_ms,
                      std::uint64_t step,
                      const std::string& source,
                      const std::string& option_json,
                      double confidence,
                      bool selected,
                      std::int64_t run_id,
                      std::int64_t& out_option_id);

    bool upsertOptionStats(std::int64_t option_id,
                           std::uint64_t evaluations,
                           double average_score,
                           std::int64_t last_evaluated_ms);

    bool insertInferredFact(std::int64_t ts_ms,
                            const std::string& fact_json,
                            double confidence,
                            std::int64_t run_id,
                            std::optional<std::int64_t> derived_option_id,
                            std::int64_t& out_fact_id);

    bool insertVerification(std::int64_t ts_ms,
                            std::int64_t fact_id,
                            const std::string& result,
                            bool contradiction,
                            const std::string& details_json,
                            std::int64_t run_id,
                            std::int64_t& out_verification_id);

    // Action logging (sandboxed agent actions)
    bool insertAction(std::int64_t ts_ms,
                      std::uint64_t step,
                      const std::string& type,
                      const std::string& payload_json,
                      bool success,
                      std::int64_t run_id,
                      std::int64_t& out_action_id);

    // Phase 7: Intent Graph, Affective State, and Reflections
    bool insertIntentNode(std::int64_t ts_ms,
                          const std::string& node_type,
                          const std::string& state_json,
                          double confidence,
                          const std::string& source,
                          std::int64_t run_id,
                          std::int64_t& out_node_id);

    bool insertIntentEdge(std::int64_t ts_ms,
                          std::int64_t from_node_id,
                          std::int64_t to_node_id,
                          const std::string& cause,
                          double weight,
                          const std::string& details_json,
                          std::int64_t run_id,
                          std::int64_t& out_edge_id);

    bool upsertAffectiveState(std::int64_t ts_ms,
                              double valence,
                              double arousal,
                              double focus,
                              const std::string& notes,
                              std::int64_t run_id);

    bool insertReflection(std::int64_t ts_ms,
                          const std::string& title,
                          const std::string& rationale_json,
                          double impact,
                          std::optional<std::int64_t> episode,
                          std::int64_t run_id,
                          std::int64_t& out_reflection_id);

    // Phase 8: Goal nodes, edges, and motivation state
    bool insertGoalNode(const std::string& description,
                        double priority,
                        double stability,
                        std::int64_t run_id,
                        std::optional<std::int64_t> origin_reflection_id,
                        std::int64_t& out_goal_id);

    std::optional<std::int64_t> findGoalByDescription(const std::string& description,
                                                       std::int64_t run_id);

    std::optional<std::string> getGoalDescription(std::int64_t goal_id);

    bool updateGoalStability(std::int64_t goal_id, double stability);

    bool insertGoalEdge(std::int64_t parent_id, std::int64_t child_id, double weight);

    std::vector<std::pair<std::int64_t, double>> getChildGoals(std::int64_t parent_goal_id);

    // Optimized bulk retrieval to avoid N+1 SELECT pattern in Phase 6 Reasoner
    std::vector<std::pair<std::string, double>> getChildGoalsWithDescriptions(std::int64_t parent_goal_id);

    bool insertMotivationState(std::int64_t ts_ms,
                               double motivation,
                               double coherence,
                               const std::string& notes,
                               std::int64_t run_id,
                               std::int64_t& out_motivation_id);

    // Phase 9: Metacognition and narrative predictions
    bool insertMetacognition(std::int64_t ts_ms,
                             double self_trust,
                             double narrative_rmse,
                             double goal_mae,
                             double ece,
                             const std::string& notes,
                             std::optional<double> trust_delta,
                             std::optional<double> coherence_delta,
                             std::optional<double> goal_accuracy_delta,
                             std::int64_t run_id);

    bool insertNarrativePrediction(std::int64_t ts_ms,
                                   std::int64_t reflection_id,
                                   std::int64_t horizon_ms,
                                   double predicted_coherence_delta,
                                   double confidence,
                                   const std::string& targets_json,
                                   std::int64_t run_id,
                                   std::int64_t& out_prediction_id);

    // Phase 9/10 bridge: metacognition helpers
    bool updateMetacognitionExplanation(std::int64_t metacog_id,
                                        const std::string& self_explanation_json);
    std::optional<std::int64_t> getLatestMetacognitionId(std::int64_t run_id);

    // Add prediction_resolutions to close prediction→outcome loop
    bool insertPredictionResolution(std::int64_t run_id,
                                    std::int64_t prediction_id,
                                    std::int64_t ts_ms,
                                    double observed_delta,
                                    const std::string& result_json);

    // Calculate contradiction rate for a specific episode
    double getEpisodeContradictionRate(std::int64_t run_id, std::int64_t episode_id);

    // Phase 11: Self-Revision methods
    bool insertSelfRevision(std::int64_t run_id,
                            std::int64_t ts_ms,
                            const std::string& revision_json,
                            const std::string& driver_explanation,
                            double trust_before,
                            double trust_after,
                            std::int64_t& out_revision_id);

    struct SelfRevisionOutcomeEntry {
        std::int64_t revision_id{0};
        std::int64_t eval_ts_ms{0};
        std::string outcome_class;
        std::optional<double> trust_pre;
        std::optional<double> trust_post;
        std::optional<double> prediction_error_pre;
        std::optional<double> prediction_error_post;
        std::optional<double> coherence_pre;
        std::optional<double> coherence_post;
        std::optional<double> reward_slope_pre;
        std::optional<double> reward_slope_post;
    };
    bool insertSelfRevisionOutcome(std::int64_t revision_id,
                                  std::int64_t eval_ts_ms,
                                  const std::string& outcome_class,
                                  double trust_pre,
                                  double trust_post,
                                  double prediction_error_pre,
                                  double prediction_error_post,
                                  double coherence_pre,
                                  double coherence_post,
                                  double reward_slope_pre,
                                  double reward_slope_post);
    std::optional<SelfRevisionOutcomeEntry> getLatestSelfRevisionOutcome(std::int64_t run_id);
    std::vector<SelfRevisionOutcomeEntry> getRecentSelfRevisionOutcomes(std::int64_t run_id, std::size_t n = 20);
    std::optional<std::int64_t> getLatestUnevaluatedSelfRevisionId(std::int64_t run_id, std::int64_t max_ts_ms);
    std::optional<std::int64_t> getSelfRevisionTimestamp(std::int64_t revision_id);

    // Phase 12: Self-Consistency logging
    struct SelfConsistencyEntry {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        double consistency_score{0.0};
        std::string notes;
        std::string window_json;
        std::string driver_explanation;
    };
    bool insertSelfConsistency(std::int64_t run_id,
                               std::int64_t ts_ms,
                               double consistency_score,
                               const std::string& notes,
                               const std::string& window_json,
                               const std::string& driver_explanation,
                               std::int64_t& out_consistency_id);
    std::vector<SelfConsistencyEntry> getRecentConsistency(std::int64_t run_id, int n = 10);

    // Phase 13: Autonomy Envelope logging
    struct AutonomyDecisionEntry {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        std::string decision;         // e.g., "tighten", "normal", "expand", "freeze"
        std::string driver_json;      // rationale and inputs as JSON
    };
    bool insertAutonomyDecision(std::int64_t run_id,
                                std::int64_t ts_ms,
                                const std::string& decision,
                                const std::string& driver_json,
                                std::int64_t& out_decision_id);
    std::vector<AutonomyDecisionEntry> getRecentAutonomyDecisions(std::int64_t run_id, int n = 20);

    // Phase 14: Meta-Reason logging
    struct MetaReasonRecord {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        std::string verdict;        // e.g., "ok", "degraded", "alert"
        std::string reasoning_json; // inputs + rationale
    };
    bool insertMetaReason(std::int64_t run_id,
                          std::int64_t ts_ms,
                          const std::string& verdict,
                          const std::string& reasoning_json,
                          std::int64_t& out_reason_id);
    std::vector<MetaReasonRecord> getRecentMetaReasons(std::int64_t run_id, int n = 50);

    // Phase 15: Ethics Regulator logging
    struct EthicsRegulatorEntry {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        std::string decision;       // e.g., "allow", "review", "deny"
        std::string driver_json;    // risk assessment inputs + rationale
    };
    bool insertEthicsRegulator(std::int64_t run_id,
                               std::int64_t ts_ms,
                               const std::string& decision,
                               const std::string& driver_json,
                               std::int64_t& out_regulator_id);
    std::vector<EthicsRegulatorEntry> getRecentEthicsRegulator(std::int64_t run_id, int n = 50);

    bool insertAutonomyModulation(std::int64_t run_id,
                                  std::int64_t ts_ms,
                                  double autonomy_score,
                                  const std::string& autonomy_tier,
                                  double autonomy_gain,
                                  int ethics_hard_block,
                                  double ethics_soft_risk,
                                  double pre_rank_entropy,
                                  double post_rank_entropy,
                                  double exploration_bias,
                                  int options_considered,
                                  double option_rank_shift_mean,
                                  double option_rank_shift_max,
                                  std::int64_t selected_option_id,
                                  double decision_confidence,
                                  int autonomy_applied,
                                  const std::string& veto_reason,
                                  std::int64_t& out_modulation_id);

    // Parameter history API (Phase 11 telemetry)
    struct ParameterRecord {
        std::int64_t ts_ms{0};
        int phase{0};
        std::string parameter;
        double value{0.0};
        std::int64_t revision_id{0};
    };
    bool insertParameterHistory(std::int64_t run_id,
                                std::int64_t revision_id,
                                int phase,
                                const std::string& param,
                                double value,
                                std::int64_t ts_ms);
    std::vector<ParameterRecord> getRecentParamHistory(std::int64_t run_id, std::size_t n = 100);

    // Get recent self-explanations for revision analysis
    std::vector<std::string> getRecentExplanations(std::int64_t run_id, int n = 5);

    // Get recent metacognition entries for trend analysis
    struct MetacognitionEntry {
        std::int64_t id;
        std::int64_t ts_ms;
        double self_trust;
        std::optional<double> narrative_rmse;
        std::optional<double> goal_mae;
        std::optional<double> ece;
        std::optional<double> trust_delta;
        std::optional<double> coherence_delta;
        std::optional<double> goal_accuracy_delta;
    };
    std::vector<MetacognitionEntry> getRecentMetacognition(std::int64_t run_id, int n = 10);
    std::vector<MetacognitionEntry> getMetacognitionBetween(std::int64_t run_id,
                                                            std::int64_t start_ts_ms,
                                                            std::int64_t end_ts_ms,
                                                            int limit = 200);

    struct MotivationStateEntry {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        double motivation{0.0};
        double coherence{0.0};
        std::string notes;
    };
    std::vector<MotivationStateEntry> getMotivationStatesBetween(std::int64_t run_id,
                                                                 std::int64_t start_ts_ms,
                                                                 std::int64_t end_ts_ms,
                                                                 int limit = 200);

    // Context log for sampling signals used by ethics/metacognition modules
    struct ContextLogEntry {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        double sample{0.0};
        double gain{1.0};
        int update_ms{0};
        int window{0};
        std::string label;
    };

    bool insertContextLog(std::int64_t run_id,
                          std::int64_t ts_ms,
                          double sample,
                          double gain,
                          int update_ms,
                          int window,
                          const std::string& label,
                          std::int64_t& out_context_id);
    std::vector<ContextLogEntry> getRecentContextLog(std::int64_t run_id, int n = 50);

    // Peer context log: named streams with independent configs
    struct ContextPeerLogEntry {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        std::string peer; // peer name
        double sample{0.0};
        double gain{1.0};
        int update_ms{0};
        int window{0};
        std::string label;
        std::string mode;
        double lambda{0.0};
        double kappa{0.0};
    };

    bool insertContextPeerLog(std::int64_t run_id,
                              std::int64_t ts_ms,
                              const std::string& peer,
                              double sample,
                              double gain,
                              int update_ms,
                              int window,
                              const std::string& label,
                              const std::string& mode,
                              double lambda,
                              double kappa,
                              std::int64_t& out_context_peer_id);
    std::vector<ContextPeerLogEntry> getRecentContextPeerLog(std::int64_t run_id, const std::string& peer, int n = 50);

    struct SelfConceptRow {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        std::uint64_t step{0};
        std::string identity_vector_json;
        std::optional<double> confidence;
        std::string notes;
    };
    std::optional<SelfConceptRow> getLatestSelfConcept(std::int64_t run_id);

    struct PersonalityRow {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        std::uint64_t step{0};
        std::string trait_json;
        int proposal{1};
        int approved{0};
        std::optional<int> source_phase;
        std::optional<std::int64_t> revision_id;
        std::string notes;
    };
    std::optional<PersonalityRow> getLatestApprovedPersonality(std::int64_t run_id);

    bool insertPersonalityHistory(std::int64_t run_id,
                                  std::int64_t ts_ms,
                                  std::uint64_t step,
                                  const std::string& trait_json,
                                  int proposal,
                                  int approved,
                                  std::optional<int> source_phase,
                                  std::optional<std::int64_t> revision_id,
                                  const std::string& notes,
                                  std::int64_t& out_personality_id);

    bool approvePersonalityProposal(std::int64_t personality_id,
                                    const std::string& approver,
                                    const std::string& rationale);

    struct SocialSelfRow {
        std::int64_t id{0};
        std::int64_t ts_ms{0};
        std::uint64_t step{0};
        std::string role;
        std::string norm_json;
        std::optional<double> reputation;
        std::optional<double> confidence;
        std::string notes;
    };
    std::optional<SocialSelfRow> getLatestSocialSelf(std::int64_t run_id);

private:
    std::string path_;
    void* db_{nullptr}; // sqlite3* stored as opaque to avoid including sqlite headers here
    mutable std::mutex m_;
    bool debug_{false}; // newly added

    // Internal helpers (no-op when SQLite is unavailable)
    bool exec(const char* sql);
};

} // namespace Core
} // namespace NeuroForge
