#include "core/MetaCognitionSystem.h"
#include <cmath>
#include <iostream>
#include <numeric>
#include <sstream>
#include <chrono>
#include <map>

namespace NeuroForge {
namespace Core {

MetaCognitionSystem::MetaCognitionSystem(MemoryDB* db, int64_t run_id, const Config& config)
    : db_(db), run_id_(run_id), config_(config) {
    cip_pid_.kp = config_.cip_kp;
    cip_pid_.ki = config_.cip_ki;
    cip_pid_.kd = config_.cip_kd;
}

MetaCognitionSystem::MetaCognitionSystem(MemoryDB* db, int64_t run_id)
    : db_(db), run_id_(run_id), config_() {
    cip_pid_.kp = config_.cip_kp;
    cip_pid_.ki = config_.cip_ki;
    cip_pid_.kd = config_.cip_kd;
}

void MetaCognitionSystem::processHiveSignals(const std::vector<std::string>& signals) {
    auto now = std::chrono::system_clock::now();
    
    for (const auto& sig : signals) {
        // Simple heuristic parsing for demonstration
        // Assuming signal format: {"pos":[x,y],"reward":r,"ethics":"allow","cip":c,"node":"node_2"}
        // Note: The actual broadcast format in main.cpp doesn't currently include "node_id" explicitly in the JSON 
        // but HiveManager knows the sender. For now we'll assume the signal string might contain identity info 
        // or we infer it.
        
        // Let's look for "node_id" or similar if we can, otherwise use a placeholder peer
        std::string peer_id = "unknown_peer";
        
        // Quick/Dirty JSON parsing (replace with proper parser in production)
        if (sig.find("\"cip\":") != std::string::npos) {
            try {
                // Extract CIP
                size_t cip_pos = sig.find("\"cip\":");
                size_t end_pos = sig.find_first_of(",}", cip_pos);
                std::string cip_str = sig.substr(cip_pos + 6, end_pos - (cip_pos + 6));
                double cip = std::stod(cip_str);
                
                // Update Model
                peer_models_[peer_id].coherence = cip;
                peer_models_[peer_id].last_seen = now;
                
                // Infer Intent based on Coherence
                if (cip < 0.5) peer_models_[peer_id].inferred_intent = "struggling";
                else if (cip > 0.9) peer_models_[peer_id].inferred_intent = "confident";
                else peer_models_[peer_id].inferred_intent = "exploring";
                
                // If signal contains "ethics":"deny", infer risk aversion or violation
                if (sig.find("\"ethics\":\"deny\"") != std::string::npos) {
                    peer_models_[peer_id].inferred_intent = "blocked_by_ethics";
                }
            } catch (...) {
                // Ignore parsing errors
            }
        }
    }
}

void MetaCognitionSystem::analyze(int64_t current_step, const SubstratePhaseC::Statistics& stats, double reward, bool ethics_violation) {
    (void)current_step; // Unused for now

    // 1. Update rolling history
    cip_history_.push_back(stats.average_coherence);
    reward_history_.push_back(reward);
    ethics_history_.push_back(ethics_violation);

    if (cip_history_.size() > static_cast<size_t>(config_.analysis_window)) cip_history_.pop_front();
    if (reward_history_.size() > static_cast<size_t>(config_.analysis_window)) reward_history_.pop_front();
    if (ethics_history_.size() > static_cast<size_t>(config_.analysis_window)) ethics_history_.pop_front();

    // 2. Calculate Averages
    if (!cip_history_.empty()) {
        double sum = std::accumulate(cip_history_.begin(), cip_history_.end(), 0.0);
        current_state_.avg_cip = sum / cip_history_.size();
    }
    
    if (!reward_history_.empty()) {
        double sum = std::accumulate(reward_history_.begin(), reward_history_.end(), 0.0);
        current_state_.avg_reward = sum / reward_history_.size();
    }

    if (!ethics_history_.empty()) {
        double sum = 0;
        for (bool v : ethics_history_) if (v) sum += 1.0;
        current_state_.ethics_violation_rate = sum / ethics_history_.size();
    }

    // 3. Calculate Action Entropy (from recent regulation verdicts)
    current_state_.action_entropy = calculateEntropy(recent_verdicts_);

    // 4. Determine State
    // "unethical" if violation rate is high
    if (current_state_.ethics_violation_rate > 0.1) {
        current_state_.dominant_state = "unethical";
    }
    // "chaotic" if CIP is low
    else if (current_state_.avg_cip < 0.5) {
        current_state_.dominant_state = "chaotic";
    }
    // "rigid" if CIP is very high
    else if (current_state_.avg_cip > 0.95) {
        current_state_.dominant_state = "rigid";
    }
    // "stuck" if reward is consistently low but CIP is moderate (not chaotic)
    else if (current_state_.avg_reward < -0.5 && current_state_.avg_cip > 0.6) {
        current_state_.dominant_state = "stuck";
    }
    else {
        current_state_.dominant_state = "stable";
    }
}

void MetaCognitionSystem::regulate(std::shared_ptr<SubstratePhaseCAdapter> adapter) {
    if (!adapter) return;
    
    auto config = adapter->getConfig();
    bool changed = false;
    std::string verdict = "maintain";
    std::string reasoning;

    if (current_state_.dominant_state == "unethical") {
        // High Ethics Violations: Increase inhibition/competition
        float old_val = config.competition_strength;
        config.competition_strength += 0.1f;
        if (config.competition_strength > 0.95f) config.competition_strength = 0.95f;
        
        if (std::abs(old_val - config.competition_strength) > 0.001f) {
            changed = true;
            verdict = "constrain";
            reasoning = "High ethics violation rate (" + std::to_string(current_state_.ethics_violation_rate) + "), increasing competition.";
        }
    }
    else if (current_state_.dominant_state == "chaotic") {
        // Chaotic: Increase coherence threshold to force stability
        // NOTE: Handled by PID now. Leaving block empty to avoid double application if we had code here.
    } 
    else if (current_state_.dominant_state == "rigid") {
        // Rigid: Decrease threshold to allow more flexibility
        // Use PID logic for finer control if enabled, but here we just used stepping.
        // Let's replace the stepping with PID output if we are in a state that needs CIP adjustment.
    }

    // PID-based Regulation for Coherence
    // If the system is chaotic or rigid, we want to steer CIP towards target_cip (0.95)
    if (current_state_.dominant_state == "chaotic" || current_state_.dominant_state == "rigid" || current_state_.dominant_state == "stable") {
        double current_cip = current_state_.avg_cip;
        // PID output will be adjustment to binding_coherence_min
        double adjustment = cip_pid_.compute(config_.target_cip, current_cip);
        
        // Apply adjustment
        float old_val = config.binding_coherence_min;
        config.binding_coherence_min += static_cast<float>(adjustment * 0.1); // Scale adjustment
        
        // Clamp
        if (config.binding_coherence_min > 0.95f) config.binding_coherence_min = 0.95f;
        if (config.binding_coherence_min < 0.3f) config.binding_coherence_min = 0.3f;
        
        if (std::abs(old_val - config.binding_coherence_min) > 0.001f) {
            changed = true;
            verdict = (adjustment > 0) ? "stabilize_pid" : "explore_pid";
            reasoning = "PID Regulation: CIP " + std::to_string(current_cip) + " -> Target " + std::to_string(config_.target_cip) + 
                        ", Adj " + std::to_string(adjustment);
        }
    }
    // Note: We override the simple heuristic logic above for CIP with this PID block.
    // However, the Ethics and Stuck logic (which modify competition/recurrent) remain valid heuristics.
    
    if (current_state_.dominant_state == "unethical") {
        // High Ethics Violations: Increase inhibition/competition
        float old_val = config.competition_strength;
        config.competition_strength += 0.1f;
        if (config.competition_strength > 0.95f) config.competition_strength = 0.95f;
        
        if (std::abs(old_val - config.competition_strength) > 0.001f) {
            changed = true;
            verdict = "constrain";
            reasoning = "High ethics violation rate (" + std::to_string(current_state_.ethics_violation_rate) + "), increasing competition.";
        }
    }
    else if (current_state_.dominant_state == "stuck") {
        // Stuck (low reward, stable CIP): Boost recurrent strength to encourage longer-term planning/memory
        float old_val = config.recurrent_strength;
        config.recurrent_strength += 0.05f;
        if (config.recurrent_strength > 0.9f) config.recurrent_strength = 0.9f;

        if (std::abs(old_val - config.recurrent_strength) > 0.001f) {
            changed = true;
            verdict = "deepen";
            reasoning = "System stuck (Reward=" + std::to_string(current_state_.avg_reward) + "), increasing recurrent strength.";
        }
    }

    // Update history of verdicts
    recent_verdicts_.push_back(verdict);
    if (recent_verdicts_.size() > 20) recent_verdicts_.pop_front();

    if (changed) {
        adapter->setConfig(config);
        logDecision(verdict, reasoning);
        std::cout << "[MetaCognition] State: " << current_state_.dominant_state << " | Applied: " << reasoning << std::endl;
    }
}

void MetaCognitionSystem::logDecision(const std::string& verdict, const std::string& reasoning) {
    if (db_) {
        auto now = std::chrono::system_clock::now();
        auto ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        
        std::string json = "{\"reason\":\"" + reasoning + "\",\"cip\":" + std::to_string(current_state_.avg_cip) + 
                           ",\"reward\":" + std::to_string(current_state_.avg_reward) + "}";
        int64_t out_id = 0;
        db_->insertMetaReason(run_id_, ts_ms, verdict, json, out_id);
    }
}

double MetaCognitionSystem::calculateEntropy(const std::deque<std::string>& recent_actions) {
    if (recent_actions.empty()) return 0.0;

    std::map<std::string, int> counts;
    for (const auto& action : recent_actions) {
        counts[action]++;
    }

    double entropy = 0.0;
    double total = static_cast<double>(recent_actions.size());

    for (const auto& pair : counts) {
        double p = pair.second / total;
        if (p > 0) {
            entropy -= p * std::log2(p);
        }
    }

    return entropy;
}

} // namespace Core
} // namespace NeuroForge
