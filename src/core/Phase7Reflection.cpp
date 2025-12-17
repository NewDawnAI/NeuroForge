#include "core/Phase7Reflection.h"
#include "core/MemoryDB.h"
#include "core/Phase8GoalSystem.h"
#include "core/Phase9Metacognition.h" // Phase 9 wiring
#include "core/SelfModel.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <string>
#include <algorithm>
#include <cmath>
#include <optional>

namespace NeuroForge {
namespace Core {

// Define constructor missing from link step
Phase7Reflection::Phase7Reflection(MemoryDB* memdb, std::int64_t run_id)
    : memdb_(memdb), run_id_(run_id) {}

static std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// Helper to make a simple JSON string without external deps
static std::string make_rationale_json(double contradiction_rate,
                                       double avg_reward,
                                       double valence,
                                       double arousal,
                                       const std::optional<double>& identity_confidence,
                                       const std::string& message) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(4);
    oss << "{\"contradiction_rate\":" << contradiction_rate
        << ",\"avg_reward\":" << avg_reward
        << ",\"valence\":" << valence
        << ",\"arousal\":" << arousal;
    if (identity_confidence.has_value()) {
        oss << ",\"identity_confidence\":" << identity_confidence.value();
    }
    oss << ",\"message\":\"";
    // Basic escaping for quotes in message
    for (char c : message) {
        if (c == '"') oss << "\\\"";
        else if (c == '\\') oss << "\\\\";
        else if (c == '\n') oss << "\\n";
        else oss << c;
    }
    oss << "\"}";
    return oss.str();
}

void Phase7Reflection::maybeReflect(std::int64_t episode_index,
                                    double contradiction_rate,
                                    double avg_reward,
                                    double valence,
                                    double arousal) {
    if (!memdb_) return;

    // Only reflect every few episodes to avoid spam
    if (episode_index - last_reflection_episode_ < MIN_EPISODE_GAP) {
        return;
    }

    // Simple heuristic: reflect when contradiction rate or reward volatility is high
    double trigger_score = contradiction_rate * 0.6 + std::abs(avg_reward) * 0.4;
    if (trigger_score < 0.3) {
        return; // Not enough signal to reflect
    }

    std::optional<double> identity_confidence;
    if (self_model_ && self_model_->isLoaded()) {
        const auto& identity = self_model_->identity();
        if (identity.confidence.has_value()) {
            identity_confidence = identity.confidence;
        }
    }

    // Compose reflection text
    std::ostringstream reflection;
    reflection << "Observations: contradiction rate=" << std::fixed << std::setprecision(3)
               << contradiction_rate << ", avg_reward=" << avg_reward
               << ". Suggest: adjust policy weighting, monitor stability.";
    std::string reflection_text = reflection.str();

    // Title and rationale JSON
    std::string title = "Phase7 Reflection";
    std::string rationale_json = make_rationale_json(contradiction_rate,
                                                     avg_reward,
                                                     valence,
                                                     arousal,
                                                     identity_confidence,
                                                     reflection_text);

    // Impact proxy from avg_reward magnitude, clamped to [0,1]
    double impact = std::min(1.0, std::abs(avg_reward));

    // Insert into DB using the MemoryDB API
    std::int64_t reflection_id = 0;
    memdb_->insertReflection(now_ms(),
                             title,
                             rationale_json,
                             impact,
                             std::nullopt, // no specific episode context
                             run_id_,
                             reflection_id);

    // Phase 8: Ingest reflection for goal extraction and motivation update
    if (phase8_goals_) {
        phase8_goals_->ingestReflection(reflection_id, title, rationale_json, impact);
    }

    last_reflection_episode_ = episode_index;
    ++reflection_count_;

    // Periodic narrative summary every NARRATIVE_PERIOD reflections
    if (reflection_count_ % NARRATIVE_PERIOD == 0) {
        double coherence = 0.5;
        if (phase8_goals_) {
            coherence = phase8_goals_->getLastCoherence();
        }
        std::ostringstream narrative;
        narrative << "Narrative: coherence=" << std::fixed << std::setprecision(3) << coherence
                  << ", recent avg_reward=" << avg_reward
                  << ", contradiction_rate=" << contradiction_rate
                  << ". Theme: " << (coherence >= 0.6 ? "stability" : "exploration")
                  << "; Continue adjustments and monitor.";
        std::string narrative_text = narrative.str();
        std::string narrative_title = "Phase7 Narrative Summary";
        std::string narrative_rationale = make_rationale_json(contradiction_rate,
                                                              avg_reward,
                                                              valence,
                                                              arousal,
                                                              identity_confidence,
                                                              narrative_text);
        double narrative_impact = std::clamp(0.2 + (1.0 - coherence) * 0.3, 0.0, 1.0);
        std::int64_t narrative_id = 0;
        memdb_->insertReflection(now_ms(),
                                 narrative_title,
                                 narrative_rationale,
                                 narrative_impact,
                                 std::nullopt,
                                 run_id_,
                                 narrative_id);
        if (phase8_goals_) {
            phase8_goals_->ingestReflection(narrative_id, narrative_title, narrative_rationale, narrative_impact);
        }
        // Phase 9: register a narrative prediction tied to this summary
        if (metacog_) {
            // Predict coherence delta relative to neutral 0.5; use coherence as confidence
            double predicted_delta = coherence - 0.5;
            double confidence = std::clamp(coherence, 0.0, 1.0);

            // Set prediction horizon as a heuristic of stability and reward
            // Higher coherence, lower contradiction, and better reward lengthen the horizon.
            double bounded_coherence = std::clamp(coherence, 0.0, 1.0);
            double bounded_contradiction = std::clamp(contradiction_rate, 0.0, 1.0);
            double reward_term = std::clamp((avg_reward + 1.0) / 2.0, 0.0, 1.0);

            double stability = bounded_coherence * (1.0 - 0.5 * bounded_contradiction);
            double horizon_factor = 0.3 + 0.4 * stability + 0.3 * reward_term;
            horizon_factor = std::clamp(horizon_factor, 0.0, 1.0);

            // Horizon ranges from ~30s (unstable) to ~2 minutes (highly stable)
            std::int64_t horizon_ms = static_cast<std::int64_t>(30000.0 + horizon_factor * 90000.0);
            std::ostringstream targets;
            targets << "{\"coherence\": " << std::fixed << std::setprecision(3) << coherence
                    << ", \"avg_reward\": " << avg_reward
                    << ", \"contradiction_rate\": " << contradiction_rate << "}";
            metacog_->registerNarrativePrediction(narrative_id, predicted_delta, confidence, horizon_ms, targets.str());
        }
    }
}

} // namespace Core
} // namespace NeuroForge
