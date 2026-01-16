#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <chrono>

#include "core/MemoryDB.h"

namespace NeuroForge {
namespace Core {

// Forward declarations
class Phase7AffectiveState;
class Phase7Reflection;
class Phase8GoalSystem;
class Phase9Metacognition;
class SelfModel;
class AutonomyEnvelope;

struct ReasonOption {
    std::string key;            // semantic key, e.g. action label
    std::string source;         // e.g. "planner" or module name
    std::string payload_json;   // details for traceability
    double confidence{0.5};     // input confidence
    double complexity{0.0};     // simple cost term
};

struct ReasonScore {
    std::size_t best_index{0};
    double best_score{0.0};
    std::vector<double> scores; // per-option scores
};

// Minimal Phase 6 Reasoner skeleton with Bayesian mean update
class Phase6Reasoner {
public:
    Phase6Reasoner(MemoryDB* memdb, std::int64_t run_id, double alpha_complexity = 0.1)
        : memdb_(memdb), run_id_(run_id), alpha_(alpha_complexity) {}

    // Set Phase 7 components (optional)
    void setPhase7Components(Phase7AffectiveState* affect, Phase7Reflection* reflect) {
        phase7_affect_ = affect;
        phase7_reflect_ = reflect;
    }

    // Set Phase 8 components (optional)
    void setPhase8Components(Phase8GoalSystem* goals) {
        phase8_goals_ = goals;
    }

    // Set Phase 9 Metacognition (optional)
    void setPhase9Metacognition(Phase9Metacognition* meta) {
        metacog_ = meta;
    }

    Phase9Metacognition* getMetacognition() const noexcept {
        return metacog_;
    }

    void setSelfModel(SelfModel* self_model) {
        self_model_ = self_model;
    }

    void setAutonomyEnvelope(const AutonomyEnvelope* env) {
        autonomy_env_ = env;
    }

    void setDebug(bool d) { debug_ = d; }

    // Register incoming options, persist to MemoryDB, return DB option ids
    std::vector<std::int64_t> registerOptions(const std::vector<ReasonOption>& options,
                                              std::uint64_t step,
                                              std::int64_t ts_ms,
                                              std::optional<std::size_t> selected_index = std::nullopt);

    // Score options using simple expected utility: mean_reward - alpha * complexity
    // Uses internal posterior means as priors; defaults to 0 for unseen options
    ReasonScore scoreOptions(const std::vector<ReasonOption>& options);

    // Apply observed outcome for selected option, updating posterior mean
    // Also emits verification record when strong mismatch is detected
    void applyOptionResult(std::int64_t option_id,
                           const std::string& option_key,
                           double observed_reward,
                           std::int64_t ts_ms,
                           bool emit_verification = true);

    // Access current posterior mean for a key (0.0 if unseen)
    double getPosteriorMean(const std::string& key) const;

    // Episode-end reflection trigger
    void onEpisodeEnd(std::int64_t episode_index, double contradiction_rate, double avg_reward);

private:
    struct Posterior {
        std::uint64_t n{0};
        double mean{0.0};
        std::int64_t last_ms{0};
    };

    MemoryDB* memdb_{nullptr};
    std::int64_t run_id_{0};
    double alpha_{0.1};
    std::unordered_map<std::string, Posterior> posteriors_; // keyed by option.key

    // Phase 7 components (optional)
    Phase7AffectiveState* phase7_affect_{nullptr};
    Phase7Reflection* phase7_reflect_{nullptr};

    // Phase 8 components (optional)
    Phase8GoalSystem* phase8_goals_{nullptr};

    // Phase 9 component (optional)
    Phase9Metacognition* metacog_{nullptr};

    SelfModel* self_model_{nullptr};

    const AutonomyEnvelope* autonomy_env_{nullptr};
    bool debug_{false};

    // Phase 7 bridge: track contradiction state and last intent correction node
    std::unordered_map<std::string, bool> last_contradiction_{}; // per key
    std::unordered_map<std::string, std::int64_t> last_intent_node_{}; // per key

    // Emit intent formation/resolution events into MemoryDB intent tables
    void maybeEmitIntentFormation(const std::string& key,
                                  std::int64_t ts_ms,
                                  bool contradiction,
                                  double observed_reward,
                                  double posterior_mean);

    // Hierarchical Reasoning: score subgoals recursively
    double scoreHierarchicalBonus(const std::string& key);
};

} // namespace Core
} // namespace NeuroForge
