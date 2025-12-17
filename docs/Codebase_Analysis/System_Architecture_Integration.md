# System Architecture & Integration

This document explains how the components of NeuroForge come together to form a cohesive, autonomous AGI system.

## 1. The Core Execution Loop

The heart of NeuroForge is the `HypergraphBrain::process()` loop, typically driven by `main.cpp` or an `AutonomousScheduler`.

### The Cycle (Perception -> Cognition -> Action)

1.  **Sensory Ingestion (Encoders)**
    *   Raw data (Images, Audio) is captured in `main.cpp`.
    *   `VisionEncoder` and `AudioEncoder` transform this data into feature vectors.
    *   `HypergraphBrain::feedExternalPattern()` injects these vectors into the input regions (`VisualCortex`, `AuditoryCortex`).

2.  **Neural Processing (HypergraphBrain)**
    *   The `HypergraphBrain` iterates through all `Region`s.
    *   **Spike Propagation**: Neurons fire, sending signals via `Synapse`s to other neurons (local and long-range).
    *   **Biases**: `SurvivalBias` and others modulate global activation thresholds based on "Hazard" or "Novelty" signals.
    *   **Plasticity**: `LearningSystem` updates synaptic weights (STDP/Hebbian) based on activity.

3.  **Cognitive Phases (The "Self" Pipeline)**
    *   Parallel to the neural loop, the high-level cognitive phases execute (often triggered by specific neural states or time intervals).
    *   **Phase 6 (Reasoner)**: Evaluates potential actions.
    *   **Phase 7 (Reflection)**: Analyzes recent episodes for contradictions.
    *   **Phase 9 (Metacognition)**: Monitors the "Self-Trust" metric.
    *   **Phase 15 (Ethics)**: Vetoes unsafe actions.

4.  **Action Generation (Decoders/Effectors)**
    *   Activity in the `MotorCortex` is read out via `HypergraphBrain::readoutVector()`.
    *   `SubstrateLanguageAdapter` translates neural patterns into symbolic tokens (Speech).
    *   `ProceduralMemory` may trigger automated motor sequences (Habits).

## 2. Data Flow & Interconnection

### The "Bridge" (Symbol Grounding)
The critical link between the **Substrate** (Spikes) and **Symbolic** (Language/Logic) worlds is managed by:
*   **`SubstrateLanguageManager`**: Orchestrates the translation.
*   **`NeuralLanguageBindings`**: Maps specific neuron clusters to semantic tokens.
*   **`MemoryDB`**: Acts as the "Blackboard" where both systems write their state.

### Memory Hierarchy Flow
1.  **Working Memory**: Holds active tokens/percepts (short-term).
2.  **Episodic Memory**: Flushes WM content into "Episodes" (medium-term).
3.  **Sleep/Consolidation**:
    *   `SleepConsolidation` moves Salient Episodes into **Semantic Memory** (Facts) and **Procedural Memory** (Skills).
    *   `HippocampalSnapshots` allow the neural net to "reload" a past state for re-training (Replay).

## 3. Autonomy & Self-Regulation

The system is designed to be autonomous, meaning it sets its own goals.
*   **`AutonomousLearningCycle`**: The "Driver". It assesses "Scaffolds" (external help) and tries to remove them.
*   **`IntrinsicMotivationSystem`**: Generates internal rewards for "Curiosity" and "Competence" when external rewards are absent.
*   **`Phase13AutonomyEnvelope`**: Acts as a governor. If the system performs well (High Trust), it expands the envelope (allows more complex tasks). If it fails, it tightens the envelope.

## 4. Diagrammatic Overview

```
[Sensors] --> [Encoders] --> [ Sensory Regions (V1/A1) ]
                                      |
                                      v
                             [ Association Cortex ] <--> [ Working Memory ]
                                      |                       ^
                                      v                       |
[Action] <-- [Decoders] <-- [ Motor Cortex ]          [ Cognitive Phases ]
                                      |                       |
                                      v                       v
                             [ LearningSystem ] <--> [ MemoryDB (SQLite) ]
```

## 5. Key Integration Points in Code
*   **`main.cpp`**: The wiring harness. It instantiates the Brain, Encoders, and Phases, and runs the `while(running)` loop.
*   **`HypergraphBrain.cpp`**: The physical simulation.
*   **`AutonomousLearningCycle.cpp`**: The psychological simulation (Growth/Autonomy).

