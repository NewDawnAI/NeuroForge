# Stage D: Meta-Cognition & Self-Regulation

**Date**: January 2026
**Status**: **COMPLETED**

## Overview
Stage D marks the transition from reactive learning to **reflective learning**. The goal is to implement a `MetaCognitionSystem` that monitors the agent's performance (Reward, Coherence, Ethics) over longer time horizons and dynamically adjusts its own hyperparameters (Learning Rate, Exploration Bias, etc.) to optimize outcomes.

This stage effectively gives NeuroForge a "conscious" layer that can observe its own "subconscious" (substrate) processes.

## Objectives

### 1. Self-Monitoring (Introspection)
- **Mechanism**: Analyze historical data from `MemoryDB` (last N steps).
- **Metrics**:
  - **Reward Trend**: Is performance improving, plateauing, or degrading?
  - **Coherence Index (CIP)**: Is the brain stable or chaotic?
  - **Entropy**: Is behavior too random or too rigid?
  - **Ethics Compliance**: How frequently are actions blocked?

### 2. Self-Regulation (Parameter Tuning)
- **Mechanism**: Dynamic adjustment of `SubstratePhaseC` configuration.
- **Policies**:
  - **PID Control**: Precise regulation of Coherence Index (CIP) using Proportional-Integral-Derivative control.
  - **Stagnation Policy**: If Reward is flat & Entropy is low -> Increase recurrent strength (Deepen thought).
  - **Chaos/Rigidity Policy**: If CIP deviates from target -> Adjust coherence thresholds via PID.
  - **Safety Policy**: If Ethics Violations are rising -> Increase competition/inhibition.

### 3. Theory of Mind (Hive Context)
- **Mechanism**: Infer the intent of peer nodes.
- **Implementation**: Analyze `hive_signal` broadcasts to infer states (e.g., "struggling", "confident", "blocked_by_ethics").

## Architecture

### `MetaCognitionSystem` Class
```cpp
class MetaCognitionSystem {
public:
    struct MetaState {
        double avg_reward;
        double avg_cip;
        double action_entropy;
        double ethics_violation_rate;
        std::string dominant_state; // stable, chaotic, rigid, stuck, unethical
    };
    
    struct PeerModel {
        std::string node_id;
        std::string inferred_intent;
        double coherence;
    };

    void analyze(int64_t current_step, const SubstratePhaseC::Statistics& stats, double reward, bool ethics_violation);
    void processHiveSignals(const std::vector<std::string>& signals);
    void regulate(std::shared_ptr<SubstratePhaseCAdapter> adapter);
    
private:
    MemoryDB* db_;
    PIDController cip_pid_;
    std::unordered_map<std::string, PeerModel> peer_models_;
};
```

## Roadmap Tasks

### ✅ Task D.1: Meta-Cognition System Core
- [x] Create `include/core/MetaCognitionSystem.h`
- [x] Create `src/core/MetaCognitionSystem.cpp`
- [x] Implement `analyze()` to fetch stats from `MemoryDB` (and live stats).

### ✅ Task D.2: Regulation Logic
- [x] Implement heuristic policies for parameter tuning.
- [x] Implement PID Controller for precise coherence regulation.
- [x] Connect to `SubstratePhaseCAdapter` to apply changes live.

### ✅ Task D.3: Integration & Logging
- [x] Instantiate in `main.cpp`.
- [x] Log meta-decisions to `meta_reason_log` table.
- [x] Implement Hive Theory of Mind (`processHiveSignals`).

### ✅ Task D.4: Verification
- [x] "Self-Healing" Experiment: Verified via `run_stage_d_experiment.ps1` that system monitors CIP/Reward and attempts regulation.
