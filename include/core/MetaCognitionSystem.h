#pragma once

#include "core/MemoryDB.h"
#include "core/SubstratePhaseCAdapter.h"
#include <memory>
#include <vector>
#include <string>
#include <deque>
#include <map>
#include <unordered_map>

namespace NeuroForge {
namespace Core {

/**
 * @brief PID Controller for precise parameter regulation
 */
struct PIDController {
    double kp = 0.0;
    double ki = 0.0;
    double kd = 0.0;
    
    double integral = 0.0;
    double prev_error = 0.0;
    
    double compute(double setpoint, double measured, double dt = 1.0) {
        double error = setpoint - measured;
        integral += error * dt;
        double derivative = (error - prev_error) / dt;
        prev_error = error;
        return kp * error + ki * integral + kd * derivative;
    }

    void reset() {
        integral = 0.0;
        prev_error = 0.0;
    }
};

/**
 * @brief Meta-Cognition System (Stage D)
 * 
 * Implements self-reflection and self-regulation.
 * Monitors performance trends (Reward, CIP, Ethics) and dynamically tunes
 * hyperparameters to optimize long-term outcomes.
 */
class MetaCognitionSystem {
public:
    struct Config {
        int analysis_window = 100;           ///< Steps to look back
        int regulation_interval = 500;       ///< Steps between regulation actions
        double target_cip = 0.95;            ///< Target Coherence Index
        double min_entropy = 0.5;            ///< Minimum action entropy (to avoid loops)
        
        // PID gains for Coherence Regulation
        double cip_kp = 0.5;
        double cip_ki = 0.05;
        double cip_kd = 0.1;
    };

    struct MetaState {
        double avg_reward = 0.0;
        double avg_cip = 0.0;
        double action_entropy = 0.0;
        double ethics_violation_rate = 0.0;
        std::string dominant_state = "stable"; // stable, stuck, chaotic, unethical
    };

    struct PeerModel {
        std::string node_id;
        std::string inferred_intent; // e.g., "exploring_left", "avoiding_risk"
        double coherence = 0.0;
        double alignment_score = 0.0; // How similar to self
        std::chrono::system_clock::time_point last_seen;
    };

    MetaCognitionSystem(MemoryDB* db, int64_t run_id, const Config& config);
    MetaCognitionSystem(MemoryDB* db, int64_t run_id);

    /**
     * @brief Analyze recent history and update internal MetaState
     */
    void analyze(int64_t current_step, const SubstratePhaseC::Statistics& stats, double reward, bool ethics_violation);

    /**
     * @brief Process signals from the Hive to infer peer intent (Theory of Mind)
     */
    void processHiveSignals(const std::vector<std::string>& signals);

    /**
     * @brief Apply regulation policies based on current MetaState
     */
    void regulate(std::shared_ptr<SubstratePhaseCAdapter> adapter);

    /**
     * @brief Get current state description
     */
    MetaState getState() const { return current_state_; }

private:
    MemoryDB* db_;
    int64_t run_id_;
    Config config_;
    MetaState current_state_;

    // PID Controllers
    PIDController cip_pid_;

    // Hive Theory of Mind
    std::unordered_map<std::string, PeerModel> peer_models_;

    // History buffers
    std::deque<double> reward_history_;
    std::deque<double> cip_history_;
    std::deque<bool> ethics_history_;
    
    int64_t last_regulation_step_ = 0;

    std::deque<std::string> recent_verdicts_;
    
    // Helper methods
    double calculateEntropy(const std::deque<std::string>& recent_actions);
    void logDecision(const std::string& verdict, const std::string& reasoning);
};

} // namespace Core
} // namespace NeuroForge
