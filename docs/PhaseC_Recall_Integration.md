# Phase C Recall Hooks (Stub Plan)

Version: v0.15.0-rc1-12-gdf2999b5
Timestamp: 2025-12-08

Updated: 2026-01-15

## Current Implementation (Open Internet Runner)

NeuroForge’s open-internet learning runner implements episodic recall over a rolling window of recent page snapshots, with visible provenance and evaluator-only review:

- Rolling retrieval window for Q/A (cross-episode recall)
- Answer provenance suffix: `source`, `window`, `evidence` (and optional `seen_in_pages`)
- Agent-2 epistemic reviewer (advisory only) with deterministic uncertainty signaling
- Reasoning-first working memory objects (`FactNode`, `HypothesisBuffer`, `ReasoningTrace`) and counter-evidence scanning for disagreement

Entry points:
- `src/neuroforge_learn.cpp` (runner + Q/A loop)
- `include/core/Agent2EpistemicReviewer.h`, `src/core/Agent2EpistemicReviewer.cpp`
- `include/core/ReasoningTrace.h`, `src/core/ReasoningTrace.cpp`

## Objectives
- Enable cross-modal recall: word→image and image→word
- Bind assemblies to anchors for prediction

## Interfaces
- Recall query CSV: anchor_type, anchor_id, predicted_ids
- Assembly log CSV: region, assembly_id, members

## Telemetry
- Log recall precision, sequence consistency, and binding coherence per run

