#pragma once
#include <cstdint>
#include <deque>
#include <string>
#include <optional>

namespace NeuroForge {
namespace Core {

class MemoryDB;
class Phase10SelfExplanation;
class Phase11SelfRevision;
class Phase12Consistency;
class Phase13AutonomyEnvelope;
class Phase14MetaReasoner;
class Phase15EthicsRegulator;

class Phase9Metacognition {
public:
    Phase9Metacognition(MemoryDB* db, std::int64_t run_id)
        : db_(db), run_id_(run_id) {}

    // Optional Phase 10 explainer injection
    void setPhase10SelfExplanation(Phase10SelfExplanation* explainer) { phase10_selfexplainer_ = explainer; }

    // Optional Phase 11 revision injection
    void setPhase11SelfRevision(Phase11SelfRevision* revision) { phase11_revision_ = revision; }

    // Optional Phase 12 consistency injection
    void setPhase12Consistency(Phase12Consistency* consistency) { phase12_consistency_ = consistency; }

    // Optional Phase 13 autonomy envelope injection
    void setPhase13AutonomyEnvelope(Phase13AutonomyEnvelope* autonomy) { phase13_autonomy_ = autonomy; }

    // Optional Phase 14 meta-reason injection
    void setPhase14MetaReasoner(Phase14MetaReasoner* meta) { phase14_metareason_ = meta; }

    // Optional Phase 15 ethics regulator injection
    void setPhase15EthicsRegulator(Phase15EthicsRegulator* ethics) { phase15_ethics_ = ethics; }

    void registerNarrativePrediction(std::int64_t reflection_id,
                                     double predicted_coherence_delta,
                                     double confidence,
                                     std::int64_t horizon_ms,
                                     const std::string& targets_json = "{}");

    void resolveActuals(double actual_coherence,
                        double actual_goal_shift,
                        const std::string& notes = "");

    double getSelfTrust() const { return self_trust_; }

    // Optional: cap pending to avoid unbounded growth
    void setPendingLimit(std::size_t limit) { pending_limit_ = limit; }

private:
    struct PendingPred {
        std::int64_t ts_ms;
        std::int64_t prediction_id; // newly added: link to DB row
        std::int64_t reflection_id;
        double predicted_coherence_delta;
        double confidence;
        std::int64_t horizon_ms;
        std::string targets_json;
    };

    void persistMetacognitionRow(std::int64_t ts_ms,
                                 double self_trust,
                                 double narrative_rmse,
                                 double goal_mae,
                                 double ece,
                                 const std::string& notes,
                                 std::optional<double> trust_delta,
                                 std::optional<double> coherence_delta,
                                 std::optional<double> goal_accuracy_delta);

    void persistNarrativePrediction(PendingPred& p); // changed to non-const to capture id

    MemoryDB* db_ = nullptr;
    Phase10SelfExplanation* phase10_selfexplainer_ = nullptr;
    Phase11SelfRevision* phase11_revision_ = nullptr;
    Phase12Consistency* phase12_consistency_ = nullptr;
    Phase13AutonomyEnvelope* phase13_autonomy_ = nullptr;
    Phase14MetaReasoner* phase14_metareason_ = nullptr;
    Phase15EthicsRegulator* phase15_ethics_ = nullptr;
    std::int64_t run_id_ = 0;
    std::deque<PendingPred> pending_;
    std::size_t pending_limit_ = 64;
    double self_trust_ = 0.5;
    std::optional<double> prev_self_trust_{};
    std::optional<double> prev_coherence_err_{};
    std::optional<double> prev_goal_mae_{};
};

} // namespace Core
} // namespace NeuroForge
