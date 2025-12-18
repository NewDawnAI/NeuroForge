# NeuroForge (Front Door)

NeuroForge is a cognitive architecture research system aimed at building agents whose internal state, internal change, and internal justifications are observable and auditable over time.

Status: Foundational research prototype — not a product.

## Who This Is For

- Researchers and reviewers evaluating a cognitive-architecture claim
- Engineers who want to inspect an architecture-first research system
- Readers looking for a defensible narrative of internal change (not only outputs)

## What Problem Does It Solve?

NeuroForge focuses on the gap between “an agent did something” and “we can explain what changed inside the agent and why.” The project treats auditability, persistence, and bounded adaptation as first-class goals.

In practice, the goal is to make it possible to answer questions like:
- What changed internally?
- What evidence or trigger drove the change?
- What constraints were checked?
- What was the system’s justification?

## What Makes It Different?

- Architecture-first: structured loops and interfaces before benchmark claims
- Persistent telemetry: internal signals are logged for later inspection and replay
- Explainable adaptation: meaningful internal changes are accompanied by structured explanations
- Safety-bounded self-modification: adaptation is constrained and rate-limited

## One-Sentence Mental Model

NeuroForge treats “cognition” as interacting loops: a substrate that updates from experience, memory that persists state and signals, metacognition that tracks reliability, explanation that records internal change, and constrained revision that proposes bounded updates.

## What It Is Not

- Not an ML library or training framework
- Not a product agent SDK
- Not a benchmark-first model release
- Not a biologically faithful brain simulation

This is a cognitive architecture research system, not a library or product.

## Where To Go Next

- Reading paths: `HOW_TO_READ_THIS.md`
- Non-negotiables: `CORE_PRINCIPLES.md`
- Scope control vs LLM comparisons: `WHY_NOT_LLM.md`
- Shared definitions: `GLOSSARY.md`

Then use the tier indexes as navigation maps:
- Conceptual foundation: `../01_FOUNDATION/INDEX.md`
- System blueprint: `../02_ARCHITECTURE/INDEX.md`
- Mechanisms: `../03_MECHANISMS/INDEX.md`
- Experiments/evidence: `../04_EXPERIMENTS/INDEX.md`
- Current state/roadmap: `../05_STATUS_AND_ROADMAP/INDEX.md`

