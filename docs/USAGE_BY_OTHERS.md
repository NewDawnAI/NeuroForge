# Usage by Others

## Who should use this
- Internal governance and safety teams evaluating learning systems under constraints
- Research labs running repeatable learning experiments with auditable telemetry
- Auditors and reviewers who need artifact-based verification

## Inputs you provide
- A MemoryDB SQLite artifact produced by a run (containing `runs` and `autonomy_envelope_log`)
- Optionally, a JSONL learning trace file for basic parseability checks

## Outputs you get
- A deterministic pass/fail result under strict verification rules
- A structured report (`json` or `md`) describing which invariants were checked and their outcomes

## What NeuroForge does not do for you
- It does not provide a live integration hook into your production system
- It does not grant authority escalation or autonomy features beyond the frozen governance boundary
- It does not replace safety policy, audit processes, or threat modeling

