# NeuroForge Architecture (Public Overview)

NeuroForge is organized around a small number of interacting loops that make internal state, internal change, and internal justification observable.

This is a foundational research prototype — not a product.

## Architecture At A Glance

NeuroForge can be read as a pipeline plus feedback loops:
- Experience updates internal state
- Internal state produces actions/outputs
- Telemetry persists internal signals
- Metacognition interprets those signals
- Explanation converts triggers into structured narratives
- Revision proposes bounded parameter updates under safety constraints

```mermaid
flowchart LR
  E[Environment / Tasks] --> X[Experience Stream]
  X --> S[Substrate + State Update]
  S --> A[Actions / Outputs]
  A --> E

  S --> T[Telemetry Signals]
  T --> P[(Persistent Log / MemoryDB)]

  P --> M[Metacognition Metrics]
  M --> EX[Self-Explanation]
  EX --> R[Self-Revision Proposals]
  R -->|bounded + rate-limited| S
```

## Core Components (Conceptual)

- Substrate: Maintains a running internal state that updates from experience.
- Memory / Persistence: Stores experience and internal measurements so they can be inspected later.
- Metacognition: Computes higher-level reliability signals (e.g., trust/coherence trends) from persisted telemetry.
- Self-Explanation: Produces a structured narrative for “why a change is being proposed.”
- Self-Revision: Proposes limited parameter deltas when triggers fire, subject to explicit safety constraints.
- Safety / Envelope: Constrains what changes are allowed, how frequently they can occur, and what must be recorded.

## Audit Trail (Change + Justification)

The architecture is designed so “change” and “justification” are coupled as an auditable event.

```mermaid
sequenceDiagram
  participant Runtime as Runtime Loop
  participant Metrics as Metrics/Triggers
  participant Explain as Self-Explanation
  participant Revise as Self-Revision
  participant Store as Persistent Store

  Runtime->>Metrics: produce/update internal signals
  Metrics->>Revise: trigger revision proposal (when thresholds/patterns fire)
  Revise->>Explain: request structured justification
  Explain-->>Revise: explanation (structured narrative)
  Revise->>Store: write revision + explanation (single event)
  Store-->>Runtime: revision becomes part of the next loop
```

## Why This Shape

The goal is not maximal capability. The goal is a coherent substrate where you can ask, for any internal change:
- What metric triggered it?
- Why was this parameter selected?
- Why was this delta considered safe?
- Where is the record of the change and its justification?
