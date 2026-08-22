# Middle Cognitive Phases Analysis (Phases 10-14)

This document analyzes the "Self-Regulation" phases of the NeuroForge cognitive pipeline. These phases provide the agent with the ability to explain its actions, revise its beliefs, and maintain operational consistency.

## 1. Phase 10: Self-Explanation

### Location
- `include/core/Phase10SelfExplanation.h`
- `src/core/Phase10SelfExplanation.cpp`

### Functional Description
Generates structured narratives to explain *why* the system's self-trust changed, and persists them for later audit and downstream analysis.

### Key Features
- **Input**: Recent Phase 9 metacognition outcomes (including prediction/resolution results when available).
- **Output**: Structured JSON explanation stored in `metacognition.self_explanation_json`.
- **Persistence**: Exposed to later phases via `MemoryDB::getRecentExplanations`.

---

## 2. Phase 11: Self-Revision

### Location
- `include/core/Phase11SelfRevision.h`
- `src/core/Phase11SelfRevision.cpp`

### Functional Description
Analyzes recent Phase 10 explanations plus Phase 9 metacognition trends and produces safe parameter revisions, persisted to the MemoryDB.

### Key Features
- **Triggers**: Periodic interval, trust drift, or detected patterns in recent explanations.
- **Action**: Proposes and clamps parameter deltas; validates safety; logs revisions into `self_revision_log` and `parameter_history`.
- **Explainability**: Stores a JSON `driver_explanation` that embeds a structured Phase 10 revision narrative under `phase10_revision_explanation`.

---

## 3. Phase 12: Consistency

### Location
- `include/core/Phase12Consistency.h` (inferred)

### Functional Description
Ensures the agent's long-term behavior remains consistent with its core identity and past actions.

### Key Features
- **Check**: Compares current intent with historical "Personality" or "Policy" vectors.
- **Correction**: If a proposed action deviates too much from the norm without justification, it signals a "Consistency Violation".

---

## 4. Phase 13: Autonomy Envelope

### Location
- `include/core/Phase13AutonomyEnvelope.h`
- `src/core/Phase13AutonomyEnvelope.cpp`

### Functional Description
A dynamic control system that expands or contracts the agent's freedom based on performance.

### Key Features
- **Envelope States**: `Tighten`, `Normal`, `Expand`, `Freeze`.
- **Hysteresis**: Prevents rapid oscillation between states (uses time windows).
- **Logic**:
  - High Trust + High Consistency -> **Expand** (Allow more complex/risky goals).
  - Low Trust or Consistency Failure -> **Tighten** (Restrict actions to safe primitives).

---

## 5. Phase 14: Meta-Reasoner

### Location
- `include/core/Phase14MetaReasoner.h` (inferred)

### Functional Description
The "Manager" of the reasoning process itself. It decides *how* to think.

### Key Features
- **Strategy Selection**: Decides whether to use fast heuristics (System 1) or deep simulation (System 2).
- **Resource Allocation**: Allocates compute time to different goals based on urgency and complexity.

