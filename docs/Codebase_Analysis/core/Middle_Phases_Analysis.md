# Middle Cognitive Phases Analysis (Phases 10-14)

This document analyzes the "Self-Regulation" phases of the NeuroForge cognitive pipeline. These phases provide the agent with the ability to explain its actions, revise its beliefs, and maintain operational consistency.

## Governance Layers (Stages 7 → 7.5 → C)

NeuroForge separates capability from governance explicitly.

Later stages introduce mechanisms that evaluate and constrain behavior without increasing autonomy or learning power.

Self-Revision (Stage 7)
        ↓
Outcome Evaluation (Stage 7.5)
        ↓
Autonomy Gating (Stage C v1)
        ↓
Existing Action & Learning Systems

Stage 7.5 is frozen at the `stage7_5-freeze` tag.

## 1. Phase 10: Self-Explanation

### Location
- `include/core/Phase10SelfExplanation.h`
- `src/core/Phase10SelfExplanation.cpp`

### Functional Description
Generates structured narratives to explain *why* the system's self-trust changed.

### Key Features
- **Input**: Metacognition metrics (Trust Delta, Prediction Error).
- **Output**: Natural language (or structured) explanation (e.g., "Trust decreased because prediction error for action X was high").
- **Persistence**: Logs explanations to `MemoryDB` for audit trails.

---

## 2. Phase 11: Self-Revision

### Location
- `include/core/Phase11SelfRevision.h` (inferred)

### Functional Description
The mechanism for bounded, rate-limited parameter revision based on recent metacognition signals and explanation context.

### Key Features
- **Trigger**: Periodic cadence and/or trend thresholds (for example sustained error or trust drift).
- **Action**: Proposes and applies safe deltas to internal parameters within configured bounds.
- **Audit**: Persists applied revisions to `self_revision_log` and `parameter_history` for later inspection.

### Stage 7.5: Post-Revision Outcome Evaluation (Evaluation-Only, Frozen)
Stage 7.5 evaluates the observed outcomes of each self-revision over time and persists a classification (`Beneficial`, `Neutral`, `Harmful`) plus pre/post metrics into `self_revision_outcomes`.

This layer is evaluation-only: it does not approve revisions, does not modify learning rules, and does not grant additional autonomy. It is frozen at the `stage7_5-freeze` tag.

### Stage C v1: Governance-Only Autonomy Gating
Stage C v1 reads the historical outcomes produced by Stage 7.5 (read-only), derives a conservative reputation-like signal over a recent window, and applies an autonomy cap multiplier to the existing autonomy envelope.

Stage C v1 does not add new capabilities, goals, or learning behaviors.

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

