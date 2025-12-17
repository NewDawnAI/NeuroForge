#pragma once
#include <cstdint>
#include <string>
#include <optional>
#include <vector>
#include <map>
#include "core/MemoryDB.h"

namespace NeuroForge {
namespace Core {

class MemoryDB;
class AutonomyEnvelope;

// Phase 11: Self-Revision
// Analyzes self-explanations and metacognition trends to generate parameter revisions
// that improve reasoning performance based on detected patterns and failure modes.
class Phase11SelfRevision {
public:
    Phase11SelfRevision(MemoryDB* db, std::int64_t run_id)
        : db_(db), run_id_(run_id) {}

    // Main entry point: analyze recent data and potentially generate revisions
    bool maybeRevise(const std::string& context = "");

    // Force revision analysis regardless of triggers
    bool runForLatest(const std::string& context = "");

    // Configuration
    void setRevisionInterval(std::int64_t interval_ms) { revision_interval_ms_ = interval_ms; }
    void setTrustDriftThreshold(double threshold) { trust_drift_threshold_ = threshold; }
    void setAnalysisWindow(int window_size) { analysis_window_ = window_size; }
    void setMinRevisionGap(std::int64_t gap_ms) { min_revision_gap_ms_ = gap_ms; }

    // Get current revision parameters for external inspection
    std::map<std::string, double> getCurrentRevisionParams() const { return current_revision_params_; }

    void setAutonomyEnvelope(const AutonomyEnvelope* env) { autonomy_env_ = env; }

public:
    struct RevisionTrigger {
        enum Type { TRUST_DRIFT, PERIODIC, PATTERN_DETECTED, MANUAL };
        Type type;
        double confidence;
        std::string description;
    };

    struct ParameterDelta {
        std::string parameter_name;
        double delta_value;
        double confidence;
        std::string rationale;
    };

private:
    // Analysis methods
    std::optional<RevisionTrigger> checkRevisionTriggers();
    std::vector<ParameterDelta> analyzeFailurePatterns(const std::vector<std::string>& explanations);
    std::vector<ParameterDelta> analyzeTrustTrends(const std::vector<MemoryDB::MetacognitionEntry>& entries);
    
    // Pattern detection
    bool detectOverconfidencePattern(const std::vector<std::string>& explanations);
    bool detectUnderexplorationPattern(const std::vector<std::string>& explanations);
    bool detectReflectionCadenceIssues(const std::vector<std::string>& explanations);
    
    // Revision generation
    std::string generateRevisionJson(const std::vector<ParameterDelta>& deltas);
    std::string synthesizeDriverExplanation(const RevisionTrigger& trigger, 
                                           const std::vector<ParameterDelta>& deltas);
    
    // Safety and validation
    bool validateRevisionSafety(const std::vector<ParameterDelta>& deltas);
    void clampParameterDeltas(std::vector<ParameterDelta>& deltas);
    
    // State tracking
    bool shouldTriggerRevision();
    std::int64_t getLastRevisionTimestamp();
    
    MemoryDB* db_ = nullptr;
    std::int64_t run_id_ = 0;
    
    // Configuration parameters
    std::int64_t revision_interval_ms_ = 300000; // 5 minutes default
    double trust_drift_threshold_ = 0.15; // Trigger if trust drops > 15%
    int analysis_window_ = 10; // Look at last 10 entries
    std::int64_t min_revision_gap_ms_ = 60000; // Min 1 minute between revisions
    
    // Current state
    std::map<std::string, double> current_revision_params_;
    std::optional<std::int64_t> last_revision_ts_;

    const AutonomyEnvelope* autonomy_env_{nullptr};
};

} // namespace Core
} // namespace NeuroForge
