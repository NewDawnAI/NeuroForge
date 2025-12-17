#include "core/Phase11SelfRevision.h"
#include "core/MemoryDB.h"
#include "core/AutonomyEnvelope.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <sstream>
#include <regex>

namespace {

std::string escape_json_string(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        switch (c) {
        case '\"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                out += '\\';
                out += 'u';
                const char* hex = "0123456789abcdef";
                unsigned char uc = static_cast<unsigned char>(c);
                out += '0';
                out += '0';
                out += hex[(uc >> 4) & 0xF];
                out += hex[uc & 0xF];
            } else {
                out += c;
            }
        }
    }
    return out;
}

}

namespace NeuroForge {
namespace Core {

bool Phase11SelfRevision::maybeRevise(const std::string& context) {
    if (!shouldTriggerRevision()) {
        return false;
    }
    return runForLatest(context);
}

bool Phase11SelfRevision::runForLatest(const std::string& context) {
    if (!db_) return false;
    
    // Check if we should trigger a revision
    auto trigger = checkRevisionTriggers();
    if (!trigger.has_value()) {
        return false;
    }
    
    // Get recent explanations and metacognition data
    auto explanations = db_->getRecentExplanations(run_id_, analysis_window_);
    auto metacog_entries = db_->getRecentMetacognition(run_id_, analysis_window_);
    
    if (explanations.empty() && metacog_entries.empty()) {
        return false; // Not enough data for analysis
    }
    
    // Analyze patterns and generate parameter deltas
    std::vector<ParameterDelta> deltas;
    
    // Analyze failure patterns from explanations
    if (!explanations.empty()) {
        auto pattern_deltas = analyzeFailurePatterns(explanations);
        deltas.insert(deltas.end(), pattern_deltas.begin(), pattern_deltas.end());
    }
    
    // Analyze trust trends from metacognition
    if (!metacog_entries.empty()) {
        auto trend_deltas = analyzeTrustTrends(metacog_entries);
        deltas.insert(deltas.end(), trend_deltas.begin(), trend_deltas.end());
    }
    
    if (deltas.empty()) {
        return false; // No revisions needed
    }
    
    // Safety validation and clamping
    if (!validateRevisionSafety(deltas)) {
        std::cerr << "[Phase11] Revision failed safety validation" << std::endl;
        return false;
    }
    clampParameterDeltas(deltas);
    
    std::string revision_json = generateRevisionJson(deltas);
    std::string driver_explanation = synthesizeDriverExplanation(trigger.value(), deltas);
    if (autonomy_env_ && autonomy_env_->valid) {
        driver_explanation += " autonomy_score=" + std::to_string(autonomy_env_->autonomy_score);
    }
    
    // Get current trust for before/after tracking
    double trust_before = 0.5;
    if (!metacog_entries.empty()) {
        trust_before = metacog_entries[0].self_trust; // Most recent
    }
    
    std::int64_t revision_id = 0;
    std::int64_t ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    bool success = db_->insertSelfRevision(run_id_, ts_ms, revision_json, 
                                          driver_explanation, trust_before, 
                                          trust_before, revision_id);
    
    if (success) {
        for (const auto& d : deltas) {
            current_revision_params_[d.parameter_name] += d.delta_value;
            double new_val = current_revision_params_[d.parameter_name];
            int phase_num = 0;
            std::smatch m;
            std::regex re("^phase(\\d+)\\.");
            if (std::regex_search(d.parameter_name, m, re) && m.size() >= 2) {
                try { phase_num = std::stoi(m[1].str()); } catch (...) { phase_num = 0; }
            }
            db_->insertParameterHistory(run_id_, revision_id, phase_num, d.parameter_name, new_val, ts_ms);
        }

        std::uint64_t proposal_step = 0;
        if (!context.empty()) {
            std::smatch step_match;
            std::regex step_re("step=([0-9]+)");
            if (std::regex_search(context, step_match, step_re) && step_match.size() >= 2) {
                try {
                    proposal_step = static_cast<std::uint64_t>(std::stoll(step_match[1].str()));
                } catch (...) {
                    proposal_step = 0;
                }
            }
        }

        std::string trigger_type_str;
        switch (trigger->type) {
        case RevisionTrigger::TRUST_DRIFT: trigger_type_str = "TRUST_DRIFT"; break;
        case RevisionTrigger::PERIODIC: trigger_type_str = "PERIODIC"; break;
        case RevisionTrigger::PATTERN_DETECTED: trigger_type_str = "PATTERN_DETECTED"; break;
        case RevisionTrigger::MANUAL: trigger_type_str = "MANUAL"; break;
        default: trigger_type_str = "UNKNOWN"; break;
        }

        std::ostringstream trait;
        trait << "{";
        trait << "\"source\":\"phase11_self_revision\",";
        trait << "\"trigger_type\":\"" << trigger_type_str << "\",";
        trait << "\"trigger_confidence\":" << trigger->confidence << ",";
        trait << "\"trigger_description\":\"" << escape_json_string(trigger->description) << "\",";
        trait << "\"deltas\":[";
        for (std::size_t i = 0; i < deltas.size(); ++i) {
            const auto& d = deltas[i];
            if (i > 0) trait << ",";
            trait << "{";
            trait << "\"parameter\":\"" << escape_json_string(d.parameter_name) << "\",";
            trait << "\"delta\":" << d.delta_value << ",";
            trait << "\"confidence\":" << d.confidence;
            trait << "}";
        }
        trait << "]";
        trait << "}";
        std::string trait_json = trait.str();

        std::optional<int> source_phase = 11;
        std::optional<std::int64_t> revision_ref;
        if (revision_id > 0) {
            revision_ref = revision_id;
        }
        std::int64_t personality_id = 0;
        int proposal_flag = 1;
        int approved_flag = 0;
        db_->insertPersonalityHistory(run_id_,
                                      ts_ms,
                                      proposal_step,
                                      trait_json,
                                      proposal_flag,
                                      approved_flag,
                                      source_phase,
                                      revision_ref,
                                      driver_explanation,
                                      personality_id);

        last_revision_ts_ = ts_ms;
        std::cerr << "[Phase11] Generated revision " << revision_id 
                  << " with " << deltas.size() << " parameter changes" << std::endl;
    }
    
    return success;
}

std::optional<Phase11SelfRevision::RevisionTrigger> Phase11SelfRevision::checkRevisionTriggers() {
    // Check time-based trigger
    std::int64_t current_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::int64_t last_revision = getLastRevisionTimestamp();
    if (current_ts - last_revision > revision_interval_ms_) {
        return RevisionTrigger{RevisionTrigger::PERIODIC, 0.8, "Periodic revision interval reached"};
    }
    
    // Check trust drift trigger
    auto recent_metacog = db_->getRecentMetacognition(run_id_, 5);
    if (recent_metacog.size() >= 3) {
        double recent_trust = recent_metacog[0].self_trust;
        double older_trust = recent_metacog[recent_metacog.size()-1].self_trust;
        double drift = older_trust - recent_trust; // Positive = trust decreased
        
        if (drift > trust_drift_threshold_) {
            return RevisionTrigger{RevisionTrigger::TRUST_DRIFT, 0.9, 
                                 "Significant trust drift detected: -" + std::to_string(drift)};
        }
    }
    
    // Check pattern-based triggers
    auto explanations = db_->getRecentExplanations(run_id_, analysis_window_);
    if (!explanations.empty()) {
        if (detectOverconfidencePattern(explanations)) {
            return RevisionTrigger{RevisionTrigger::PATTERN_DETECTED, 0.85, 
                                 "Overconfidence pattern detected"};
        }
        if (detectUnderexplorationPattern(explanations)) {
            return RevisionTrigger{RevisionTrigger::PATTERN_DETECTED, 0.8, 
                                 "Under-exploration pattern detected"};
        }
        if (detectReflectionCadenceIssues(explanations)) {
            return RevisionTrigger{RevisionTrigger::PATTERN_DETECTED, 0.75, 
                                 "Reflection cadence issues detected"};
        }
    }
    
    return std::nullopt;
}

std::vector<Phase11SelfRevision::ParameterDelta> Phase11SelfRevision::analyzeFailurePatterns(
    const std::vector<std::string>& explanations) {
    
    std::vector<ParameterDelta> deltas;
    
    // Count pattern occurrences
    int overconfidence_count = 0;
    int underexploration_count = 0;
    int coherence_issues = 0;
    
    for (const auto& explanation : explanations) {
        // Simple pattern matching on explanation text
        if (explanation.find("overconfident") != std::string::npos ||
            explanation.find("too confident") != std::string::npos) {
            overconfidence_count++;
        }
        if (explanation.find("exploration") != std::string::npos ||
            explanation.find("not enough variety") != std::string::npos) {
            underexploration_count++;
        }
        if (explanation.find("coherence") != std::string::npos ||
            explanation.find("inconsistent") != std::string::npos) {
            coherence_issues++;
        }
    }
    
    double total_explanations = static_cast<double>(explanations.size());
    
    // Generate deltas based on pattern frequency
    if (overconfidence_count / total_explanations > 0.4) {
        deltas.push_back({"phase6.learning_rate", -0.02, 0.8, 
                         "Reduce learning rate due to overconfidence pattern"});
        deltas.push_back({"phase9.trust_adjustment_rate", 0.01, 0.7,
                         "Increase trust sensitivity to overconfidence"});
    }
    
    if (underexploration_count / total_explanations > 0.3) {
        deltas.push_back({"phase6.exploration_bonus", 0.05, 0.75,
                         "Increase exploration due to under-exploration pattern"});
    }
    
    if (coherence_issues / total_explanations > 0.5) {
        deltas.push_back({"phase7.reflection_interval", -1000, 0.7,
                         "Increase reflection frequency for coherence issues"});
        deltas.push_back({"phase8.decay_scale", 0.02, 0.6,
                         "Adjust goal decay for better coherence"});
    }
    
    return deltas;
}

std::vector<Phase11SelfRevision::ParameterDelta> Phase11SelfRevision::analyzeTrustTrends(
    const std::vector<MemoryDB::MetacognitionEntry>& entries) {
    
    std::vector<ParameterDelta> deltas;
    
    if (entries.size() < 3) return deltas;
    
    // Calculate trust trend
    double trust_sum = 0.0;
    double trust_trend = 0.0;
    for (size_t i = 0; i < entries.size(); ++i) {
        trust_sum += entries[i].self_trust;
        if (i > 0) {
            trust_trend += (entries[i-1].self_trust - entries[i].self_trust); // Recent - older
        }
    }
    
    double avg_trust = trust_sum / entries.size();
    trust_trend /= (entries.size() - 1);
    
    // Analyze error patterns
    double avg_narrative_rmse = 0.0;
    double avg_goal_mae = 0.0;
    int rmse_count = 0, mae_count = 0;
    
    for (const auto& entry : entries) {
        if (entry.narrative_rmse.has_value()) {
            avg_narrative_rmse += entry.narrative_rmse.value();
            rmse_count++;
        }
        if (entry.goal_mae.has_value()) {
            avg_goal_mae += entry.goal_mae.value();
            mae_count++;
        }
    }
    
    if (rmse_count > 0) avg_narrative_rmse /= rmse_count;
    if (mae_count > 0) avg_goal_mae /= mae_count;
    
    // Generate deltas based on trends
    if (trust_trend < -0.05) { // Trust declining
        if (avg_narrative_rmse > 0.3) {
            deltas.push_back({"phase7.narrative_weight", 0.1, 0.8,
                             "Increase narrative weight due to declining trust and high RMSE"});
        }
        if (avg_goal_mae > 0.25) {
            deltas.push_back({"phase8.goal_stability_threshold", 0.05, 0.75,
                             "Adjust goal stability due to declining trust and high MAE"});
        }
    }
    
    if (avg_trust < 0.3) { // Very low trust
        deltas.push_back({"phase6.exploration_bonus", 0.08, 0.9,
                         "Increase exploration due to very low trust"});
        deltas.push_back({"phase7.reflection_interval", -2000, 0.85,
                         "Increase reflection frequency due to low trust"});
    }
    
    return deltas;
}

bool Phase11SelfRevision::detectOverconfidencePattern(const std::vector<std::string>& explanations) {
    int overconfidence_indicators = 0;
    for (const auto& explanation : explanations) {
        if (explanation.find("overconfident") != std::string::npos ||
            explanation.find("too certain") != std::string::npos ||
            explanation.find("high confidence") != std::string::npos) {
            overconfidence_indicators++;
        }
    }
    return overconfidence_indicators >= static_cast<int>(explanations.size() * 0.4);
}

bool Phase11SelfRevision::detectUnderexplorationPattern(const std::vector<std::string>& explanations) {
    int exploration_indicators = 0;
    for (const auto& explanation : explanations) {
        if (explanation.find("exploration") != std::string::npos ||
            explanation.find("variety") != std::string::npos ||
            explanation.find("same pattern") != std::string::npos) {
            exploration_indicators++;
        }
    }
    return exploration_indicators >= static_cast<int>(explanations.size() * 0.3);
}

bool Phase11SelfRevision::detectReflectionCadenceIssues(const std::vector<std::string>& explanations) {
    int cadence_indicators = 0;
    for (const auto& explanation : explanations) {
        if (explanation.find("reflection") != std::string::npos ||
            explanation.find("timing") != std::string::npos ||
            explanation.find("frequency") != std::string::npos) {
            cadence_indicators++;
        }
    }
    return cadence_indicators >= static_cast<int>(explanations.size() * 0.25);
}

std::string Phase11SelfRevision::generateRevisionJson(const std::vector<ParameterDelta>& deltas) {
    std::ostringstream json;
    json << "{\n";
    
    for (size_t i = 0; i < deltas.size(); ++i) {
        json << "  \"" << deltas[i].parameter_name << "\": " << deltas[i].delta_value;
        if (i < deltas.size() - 1) json << ",";
        json << "\n";
    }
    
    json << "}";
    return json.str();
}

std::string Phase11SelfRevision::synthesizeDriverExplanation(
    const RevisionTrigger& trigger, const std::vector<ParameterDelta>& deltas) {
    
    std::ostringstream explanation;
    explanation << "Revision triggered by: " << trigger.description 
                << " (confidence: " << trigger.confidence << "). ";
    
    explanation << "Applied " << deltas.size() << " parameter adjustments: ";
    
    for (size_t i = 0; i < deltas.size(); ++i) {
        explanation << deltas[i].parameter_name << " " 
                   << (deltas[i].delta_value > 0 ? "+" : "") << deltas[i].delta_value
                   << " (" << deltas[i].rationale << ")";
        if (i < deltas.size() - 1) explanation << "; ";
    }
    
    return explanation.str();
}

bool Phase11SelfRevision::validateRevisionSafety(const std::vector<ParameterDelta>& deltas) {
    // Check for reasonable delta magnitudes
    for (const auto& delta : deltas) {
        if (std::abs(delta.delta_value) > 0.5) { // No single change > 50%
            return false;
        }
        if (delta.confidence < 0.5) { // Require minimum confidence
            return false;
        }
    }
    
    // Check for conflicting changes
    std::map<std::string, double> param_changes;
    for (const auto& delta : deltas) {
        param_changes[delta.parameter_name] += delta.delta_value;
    }
    
    for (const auto& [param, total_change] : param_changes) {
        if (std::abs(total_change) > 0.3) { // Total change per parameter < 30%
            return false;
        }
    }
    
    return true;
}

void Phase11SelfRevision::clampParameterDeltas(std::vector<ParameterDelta>& deltas) {
    for (auto& delta : deltas) {
        // Clamp individual deltas
        delta.delta_value = std::max(-0.2, std::min(0.2, delta.delta_value));
        
        // Apply confidence-based scaling
        delta.delta_value *= delta.confidence;
    }
}

bool Phase11SelfRevision::shouldTriggerRevision() {
    std::int64_t current_ts = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::int64_t last_revision = getLastRevisionTimestamp();
    
    // Enforce minimum gap between revisions
    return (current_ts - last_revision) >= min_revision_gap_ms_;
}

std::int64_t Phase11SelfRevision::getLastRevisionTimestamp() {
    if (last_revision_ts_.has_value()) {
        return last_revision_ts_.value();
    }
    
    // Query database for last revision
    // For now, return 0 to allow first revision
    return 0;
}

} // namespace Core
} // namespace NeuroForge
