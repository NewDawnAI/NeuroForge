# Cognitive Phases Analysis

This document details the high-level cognitive pipeline of NeuroForge, structured as a series of "Phases" that handle reasoning, reflection, goal setting, and self-regulation. These components reside in `src/core` and `include/core`.

## 1. Phase 6: Reasoner

### Location
- `include/core/Phase6Reasoner.h`
- `src/core/Phase6Reasoner.cpp`

### Functional Description
A Bayesian reasoning engine that evaluates potential actions ("Options"). It scores options based on a utility function: `Expected Reward - (Alpha * Complexity)`.

### Key Features
- **Bayesian Update**: Maintains posterior mean estimates for the utility of actions based on observed outcomes.
- **Traceability**: Logs reasoning steps and confidence levels to `MemoryDB`.
- **Integration**: Can trigger Phase 7 (Reflection) and Phase 8 (Goals).

---

## 2. Phase 7: Affective State & Reflection

### Location
- `include/core/Phase7AffectiveState.h`
- `include/core/Phase7Reflection.h`

### Functional Description
*Inferred from headers and usage:*
- **Affective State**: Manages the emotional context of the agent (e.g., frustration, satisfaction) which modulates decision-making.
- **Reflection**: A system for retrospective analysis of episodes. It identifies contradictions or failures and generates "Reflections" that feed into the Goal System.

---

## 3. Phase 8: Goal System

### Location
- `include/core/Phase8GoalSystem.h`
- `src/core/Phase8GoalSystem.cpp`

### Functional Description
Manages the hierarchy of goals and drives. It ingests "Reflections" from Phase 7 to create or update goals.

### Key Features
- **Goal Hierarchy**: Supports parent/child goal relationships.
- **Stability**: Goals have stability scores that decay over time if not reinforced.
- **Motivation**: Tracks motivation levels and coherence of the goal set.

### Usage Example
```cpp
Phase8GoalSystem goals(mem_db, run_id);
goals.createGoal("Explore environment", 0.8);
goals.ingestReflection(reflection_id, "Found new area", "{}", 0.5);
```

---

## 4. Phase 9: Metacognition

### Location
- `include/core/Phase9Metacognition.h`
- `src/core/Phase9Metacognition.cpp`

### Functional Description
The "Self-Monitoring" system. It tracks the agent's performance in predicting outcomes and maintaining coherence.

### Key Features
- **Self-Trust**: A dynamic metric (0.0-1.0) representing the system's confidence in its own cognition.
- **Narrative Prediction**: Predicts future coherence and checks actuals against predictions.
- **Integration**: Orchestrates lower phases (10-15) based on trust levels.

---

## 5. Phase 15: Ethics Regulator

### Location
- `include/core/Phase15EthicsRegulator.h`
- `src/core/Phase15EthicsRegulator.cpp`

### Functional Description
A safety, governance, and compliance layer. It intercepts decisions and evaluates them against safety constraints.

### Key Features
- **Risk Assessment**: Calculates risk scores for actions.
- **Decisions**: Returns `allow`, `review`, or `deny`.
- **Audit**: Logs all ethics evaluations to `MemoryDB`.

---

## 6. Phase A: Multimodal Mimicry

### Location
- `include/core/PhaseAMimicry.h`
- `src/core/PhaseAMimicry.cpp`

### Functional Description
Facilitates learning through imitation. It aligns the agent's internal representations with external "Teacher" embeddings (from CLIP, Whisper, BERT).

### Key Features
- **Multimodal Alignment**: Aligns Vision, Text, and Audio modalities.
- **Teacher Embeddings**: Stores embeddings from pre-trained models to serve as ground truth.
- **Mimicry Reward**: Generates intrinsic rewards based on similarity to teacher examples + novelty.
- **Substrate Integration**: Can drive the `HypergraphBrain` directly in "Mirror" or "Train" modes.
- **Replay Buffer (New)**: Periodically reinforces top‑reward attempts to stabilize student embeddings. Config: `replay_interval_steps`, `replay_top_k`.

### Dependencies
- `LanguageSystem`
- `MemoryDB`
- External Encoders (CLIP, etc. - abstract interfaces)

References: `include/core/PhaseAMimicry.h:138–140`, `src/core/PhaseAMimicry.cpp:460–465`, `src/core/PhaseAMimicry.cpp:1323`.
