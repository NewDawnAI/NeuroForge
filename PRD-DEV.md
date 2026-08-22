# PRD-DEV.md — NeuroForge Development Reference

> **Living document.** Updated by the development agent after every significant change.
> Last updated: **2026-03-01**

---

## 1. Project Overview

**NeuroForge** is a cognitive architecture research system implemented in **C++20** (MinGW/GCC 13.2, Ninja build). It treats cognition as interacting loops — substrate learning, persistent memory, metacognitive reliability, self-explanation, and bounded self-revision — producing systems whose internal changes are observable and auditable.

**Core principle:** Architecture-first coherence, traceability, and safety-bounded adaptation over benchmark chasing.

---

## 2. Architecture Summary

```
┌─────────────────────────────────────────────────────────────────┐
│                        RUNNERS (CLI entry)                       │
│  --genesis │ --alive-bounded │ --web-passive │ --n9-auto-gated  │
│  --enable-parallel │ --camera (opt-in real webcam vision)        │
└────────┬────────────────────┬──────────────────┬────────────────┘
         │                    │                  │
    ┌────▼──────┐     ┌──────▼──────┐    ┌──────▼──────┐
    │ GridWorld  │     │ WebSandbox  │    │ LanguageSys │
    │ (Embodied) │     │ (Browser)   │    │ (Vocab+Emb) │
    └────┬──────┘     └──────┬──────┘    └──────┬──────┘
         │                   │                  │
    ┌────▼───────────────────▼──────────────────▼────┐
    │           WorldModelCortex (JEPA-inspired)      │
    │  Visual │ Motion │ Temporal │ Linguistic │ ...  │
    └────────────────────┬───────────────────────────┘
                         │
    ┌────────────────────▼───────────────────────────┐
    │         UNIVERSAL LEARNING SIGNAL               │
    │   Δw = η · δ · ∇_w I(gain)                     │
    │   δ = prediction error  │  I_gain = curiosity   │
    │   η_eff = η₀(1 + α|δ|) │  sparsity pressure    │
    └────────────────────┬───────────────────────────┘
                         │
    ┌────────────────────▼───────────────────────────┐
    │              COGNITIVE STACK                    │
    │  Phase 6: Reasoner      Phase 10: Self-Explain │
    │  Phase 7: Reflection    Phase 11: Self-Revision│
    │  Phase 8: Goal System   Phase 12: Consistency  │
    │  Phase 9: Metacognition Phase 13: Autonomy Env │
    │                         Phase 14: Meta-Reasoner│
    │                         Phase 15: Ethics Reg.  │
    └────────────────────┬───────────────────────────┘
                         │
    ┌────────────────────▼───────────────────────────┐
    │              MEMORY SYSTEMS                     │
    │  MemoryDB (SQLite) │ Working │ Episodic        │
    │  Semantic │ Procedural │ Sleep Consolidation    │
    │  Dream Processor │ Memory Integrator            │
    │  CompositionMetrics (emergent mergers)           │
    └────────────────────────────────────────────────┘
```

---

## 3. Milestone Status

| Milestone | Description | Status |
|-----------|------------|--------|
| **M0–M4** | Neural substrate, synapses, regions, basic learning | ✅ Complete |
| **M5** | Language system, multimodal integration | ✅ Complete |
| **M6** | Semantic runners (N1–N6), bounded execution | ✅ Complete |
| **M7** | Autonomy metrics, unified bounded runner | ✅ Complete |
| **Stage C** | Cross-modal integration (v1–v6) | ✅ Complete |
| **Stage D** | Meta-cognition (PID control, self-monitoring) | ✅ Complete |
| **Phase A** | Baby multimodal mimicry | ✅ Complete |
| **Internet Grounding** | Relation gates → live perception → curiosity nav → verification | ✅ Complete |
| **N7 Bounded** | Safe bounded GridWorld runners (Model-based, Minimal, Bounded) | ✅ Complete |
| **N8 Human-Gated** | Human-authorization for goal execution | ✅ Complete |
| **N9 Auto-Gated** | Automated confidence gating (collision risk + prediction) | ✅ Complete |
| **N-Web** | WebSandbox (WebView2) + Language acquisition from web | ✅ Complete |
| **N-Web+Bias** | Attentional awakening (novelty/social bias on web input) | ✅ Complete |
| **N-Genesis** | Grand Unified Runner (all subsystems in one loop) | ✅ Complete |
| **Universal Learning** | Biological equation Δw = η·δ·∇I(gain) as unified signal | ✅ Complete |
| **Parallel Loop** | 5-thread parallel genesis (perception, grid, web, cognition, consolidation) | ✅ Complete |
| **Compositional Emergence** | K(whole\|parts) proxy for emergent composition detection | ✅ Complete |
| **Brain Persistence** | JSON-based save/load of explicit cognitive state (Language, Episodic, Semantic, Procedural) | ✅ Complete |
| **Conversational Interface** | Native STT (PowerShell System.Speech) + TTS parallel loop integration for user dialogue | ✅ Complete |
| **Real Vision Grounding** | OpenCV webcam capture → 8×8 grayscale → 64-float visual input → WorldModelCortex (Phase 4) | ✅ Complete |
| **Emergent Concept Composition** | CompositionMetrics wired into DreamProcessor::processREMDreams for autonomous concept invention during sleep (Phase 5) | ✅ Complete |
| **Memory Optimization** | Episodic dual-storage eliminated, EpisodeAdapter→encodePattern(), LanguageSystem strings→DEBUG TAG, SemanticMemory `ConceptNode`→`[[deprecated]]` + `concept_graph_`→DEBUG/COMPAT, ProceduralMemory `Skill/MotorAction/Habit`→`[[deprecated]]` + 5 legacy maps→DEBUG/COMPAT — vector-only cognition principle fully applied | ✅ Complete |

---

## 4. Subsystem Inventory

### 4.1 Runners (`include/runtime/`)

| Runner | Flag | Description |
|--------|------|-------------|
| `GrandUnifiedRunner` | `--genesis` | **All subsystems**: Web + Grid + Language + Phase 7/8/9 + Memory |
| `UnifiedAliveRunner` | `--alive-bounded` | Infinite GridWorld loop with gating |
| `WebSandboxRunner` | `--web-passive` | Read-only web observer + language learning |
| `N9AutomatedGatedRunner` | `--n9-auto-gated` | Automated confidence gating |
| `N8HumanGatedRunner` | `--n8-human-gated` | Human authorization required |
| `N7BoundedRunner` | `--n7-bounded` | Safe bounded GridWorld |
| `N7ModelBasedRunner` | `--n7-model` | Model-based planning |
| `N7MinimalRunner` | `--n7-minimal` | Minimal observation loop |
| `UnifiedBoundedRunner` | `--unified-bounded` | Multi-stage bounded evaluation |
| `SemanticN2–N6Runner` | `--semantic-n2`..`n6` | Incremental semantic runners |
| `SemanticInjectionRunner` | `--test-semantic-injection` | Semantic injection test |

### 4.2 Cognitive Stack (`include/core/`)

| Phase | Header | Role |
|-------|--------|------|
| 6 | `Phase6Reasoner.h` | Logical inference engine |
| 7 | `Phase7Reflection.h` + `Phase7AffectiveState.h` | Narrative reflection + affect |
| 8 | `Phase8GoalSystem.h` | Hierarchical goal formation + stability decay |
| 9 | `Phase9Metacognition.h` | Self-trust via prediction tracking |
| 10 | `Phase10SelfExplanation.h` | Self-explanation generation |
| 11 | `Phase11SelfRevision.h` | Bounded self-revision |
| 12 | `Phase12Consistency.h` | Consistency checking |
| 13 | `Phase13AutonomyEnvelope.h` | Autonomy boundary management |
| 14 | `Phase14MetaReasoner.h` | Meta-reasoning over reasoning traces |
| 15 | `Phase15EthicsRegulator.h` | Ethical boundary enforcement |

### 4.2b Learning Signal & Plasticity (`include/core/`)

| Module | Header | Role |
|--------|--------|------|
| Universal Learning Signal | `UniversalLearningSignal.h` | Unified biological equation Δw = η·δ·∇I(gain). Computes EMA-smoothed δ, error-proportional η, blended reward. Thread-safe atomic broadcast. |
| Composition Metrics | `CompositionMetrics.h` | Reconstruction-error proxy for K(whole\|parts). Gaussian noise perturbation for creative recombination. Merger history with EMA tracking. |
| Synapse Plasticity | `Synapse.h` | Rules: Hebbian, STDP, BCM, Oja, **Universal** (Δw = η·δ·∇I). Eligibility traces, weight guardrails. |

### 4.3 Memory (`include/memory/`)

| Module | Purpose |
|--------|---------|
| `WorkingMemory` | Short-term active context (9-slot activation buffers) |
| `EpisodicMemoryManager` | Episode storage and retrieval (vector-only `EpisodicPattern` primary; legacy `Episode` deprecated) |
| `SemanticMemory` | Long-term concept knowledge (Hopfield attractor settling; `attractors_` primary, `concept_graph_` legacy) |
| `ProceduralMemory` | Learned skill patterns (Hebbian weight update; `traces_`/`weights_` primary, `skills_` legacy) |
| `SleepConsolidation` | Offline memory replay + synaptic scaling (clean — no string storage) |
| `DreamProcessor` | Generative rehearsal during consolidation + Phase 5 emergent composition |
| `MemoryIntegrator` | Cross-memory-system integration |
| `DevelopmentalConstraints` | Stage-gated learning limits (metadata strings only — fixed set) |
| `EpisodeAdapter` | ReplayFrame → EpisodicMemory bridge (vector-only via `encodePattern()`) |
| `SkillExtractor` | Action sequence → ProceduralMemory bridge |

**Memory architecture principle:** Internal cognition operates on **vectors/IDs/weights only**. String fields exist as `DEBUG TAG` labels for human-readable logging, TTS output, and serialization keys — never for internal retrieval or decision-making. `BrainPersistence` serializes only the vector-based primary storage (patterns, attractors, traces, embeddings).

**Thread safety model:** All memory modules use `std::shared_mutex` (C++17) for brain-like parallel access. Read methods (`recallByCue`, `settle`, `recall`, `findSimilarConcepts`, etc.) use `std::shared_lock` for concurrent reads. Write methods (`encodePattern`, `reinforce`, `consolidatePatterns`, etc.) use exclusive `std::lock_guard`. Internal lock-free `*Locked()` helpers prevent deadlocks in call chains.

### 4.4 Perception (`include/perception/`)

| Module | Purpose |
|--------|---------|
| `WorldModelCortex` | Central JEPA-inspired multimodal processor |
| `WorldEncoder` | Observation → latent encoding |
| `WorldPredictor` | Forward prediction model |
| `WorldState` | Internal state representation |
| `SemanticProjection` | JEPA-safe language → latent projection |
| `LivePerceptionLoop` | Real-time sensor processing pipeline |

### 4.5 Sandbox & Embodiment (`include/sandbox/`)

| Module | Purpose |
|--------|---------|
| `GridWorld` | 7×7 navigation environment (goal, obstacles) |
| `WebSandbox` | WebView2 browser (navigate, click, DOM extract, screenshot) |
| `ScreenshotCapture` | GDI viewport capture |
| `LinearActionModel` | Observation → action mapping |
| `LinearLatentDecoder` | Latent → spatial decoding |

### 4.6 Language (`include/core/` + `include/language/`)

| Module | Purpose |
|--------|---------|
| `LanguageSystem` | Vocabulary management + embedding (64-dim); string symbols are `DEBUG TAG` only — cognition uses embeddings |
| `LanguageAcquisitionLoop` | Web content → vocabulary learning |
| `NeuralLanguageBindings` | Language ↔ neural substrate bridge |
| `SubstrateLanguageIntegration` | Deep integration with substrate |

### 4.7 Biases (`include/biases/`)

| Bias | Purpose |
|------|---------|
| `NoveltyBias` | Curiosity-driven exploration bonus |
| `SurvivalBias` | Self-preservation signals |
| `FaceDetectionBias` | Social perception prioritization |
| `SpatialNavigationBias` | Navigation-relevant attention |

### 4.8 Social & Norms (`include/social/` + `include/norms/`)

| Module | Purpose |
|--------|---------|
| `SocialNormEngine` | Norm learning and enforcement |
| `NormInductionEngine` | Inferring norms from observations |
| `NormNegotiationEngine` | Multi-agent norm negotiation |
| `ReputationModel` | Trust/reputation tracking |
| `NormativeReasoner` | Normative judgment generation |

### 4.9 Vision (`include/vision/` + `include/audio/`)

| Module | Purpose |
|--------|---------|
| `BrowserVision` | OCR + element detection from screenshots |
| `AudioInputSystem` | Native Windows STT via PowerShell System.Speech |
| OpenCV `VideoCapture` | Real webcam capture (guarded by `NF_HAVE_OPENCV`) |

### 4.10 Other Systems

| Directory | Contents |
|-----------|----------|
| `accountability/` | Audit trail and accountability systems |
| `actuation/` | Motor output and actuation systems |
| `alignment/` | Value alignment modules |
| `arbitration/` | Conflict resolution between subsystems |
| `contracts/` | Behavioral contracts and guarantees |
| `expression/` | Emotional expression generation |
| `identity/` | Identity formation and maintenance |
| `roles/` | Role-playing and social role management |
| `encoders/` | Text, visual, and multi-modal encoders |
| `serialization/` | State persistence (Cap'n Proto) |
| `replay/` | Experience replay systems |

---

## 5. Build System

| Tool | Version |
|------|---------|
| Compiler | GCC 13.2 (MinGW-w64) |
| Build Generator | Ninja |
| Build System | CMake 3.x |
| C++ Standard | C++20 |
| Platform | Windows (Win32 API for sandbox) |

**Optional dependencies:**
- SQLite3 — MemoryDB telemetry (graceful degradation without)
- WebView2 — Browser sandbox (graceful degradation without)
- OpenCV — Vision processing (synthetic fallback without)
- Cap'n Proto — Serialization

**Build commands:**
```powershell
cmake -B build -S . -G Ninja
cmake --build build --config Release
```

---

## 6. CLI Reference (Key Flags)

```
# Grand Unified Runner (all-in-one)
--genesis                    All subsystems active
--genesis-url=<URL>          Starting web page
--genesis-vision             Enable screenshot capture
--camera                     Enable real webcam vision (OpenCV)
--genesis-steps=<N>          Bounded run (N steps then exit)
--no-webview                 Disable web (GridWorld-only)

# Parallel Mode (Stage 2)
--enable-parallel            Multi-threaded genesis (5 workers)
--perception-hz=<N>          Perception thread rate (default 50)
--grid-hz=<N>                Grid motor thread rate (default 20)
--web-hz=<N>                 Web acquisition thread rate (default 2)
--cognition-hz=<N>           Cognition thread rate (default 1)

# Individual Runners
--alive-bounded              Infinite GridWorld loop
--web-passive                Read-only web observer
--n9-auto-gated              Automated confidence gating
--n8-human-gated             Human-authorized goals
--n7-bounded                 Safe bounded GridWorld
--unified-bounded            Multi-stage bounded eval

# Logging
--log-json=<path>            JSON event logging
--log-csv=<path>             CSV metric logging

# Cognitive
--phase8=on|off              Goal system toggle
```

---

## 7. Key Design Decisions

1. **JEPA-inspired World Model** — Prediction in latent space, not pixel space. Reduces hallucination.
2. **Multi-timescale loop** — Web extraction (~400ms) runs at lower frequency than grid ticks (~20ms).
3. **Graceful degradation** — Missing SQLite → no cognition logging. Missing WebView2 → grid-only mode.
4. **Phase wiring via setter injection** — `Phase7.setPhase8Components()`, etc. Decoupled initialization.
5. **Non-owning shared_ptr for MemoryDB** — Runners don't own the DB lifetime; `main.cpp` does.
6. **Attention modulator** — Boredom-driven navigation. Interest < 0.1 for 30+ ticks → navigate to random page.
7. **Safety layers** — Phases 13 (autonomy envelope) → 14 (meta-reasoning) → 15 (ethics) form a hard gate.
8. **Universal Learning Signal** — Unified biological equation Δw = η·δ·∇I(gain). All subsystems consume one broadcast signal instead of separate δ + I_gain channels.
9. **Parallel genesis** — Opt-in 5-thread architecture with `SharedState` atomics. Sequential mode preserved as default (zero regression risk).
10. **Compositional emergence** — Reconstruction-error proxy K(whole|parts) ≈ ||whole − reconstruct(a,b)||² detects when merged representations achieve genuine compositionality.
11. **Brain Persistence vs Substrate Storage** — High-level explicit memory (Words, Episodes) is saved as human-readable JSON vectors to `data/brain_state` for easy inspection. Low-level implicit substrate (Millions of Synapses) remains in high-performance Cap'n Proto binary files.
12. **Brain-like parallel memory** — All memory systems use `std::shared_mutex` with `shared_lock` for reads (37 methods) and exclusive `lock_guard` for writes. Multiple memory systems can be queried simultaneously just like biological parallel recall.
13. **Lock hierarchy with `*Locked()` helpers** — Internal functions (`encodePatternLocked`, `consolidatePatternsLocked`, `forgetWeakPatternsLocked`, `decayMemoryTracesLocked`, `pruneWeakMemoriesLocked`) run without acquiring locks, preventing deadlocks in call chains.
14. **Named constants over magic numbers** — `EpisodicMemoryManager` uses `kDefaultSalience`, `kMinSearchSimilarity`, `kActivationBoost`, `kConsolidationStep`, `kPruneActivationThreshold`, `kDefaultMinForgetStrength`, `kMaxRecentEpisodes` instead of hardcoded values.
15. **Real vision grounding** — OpenCV `cv::VideoCapture(0)` captures webcam frames, converts to 8×8 grayscale, normalizes to 64-float vector matching `visual_input_dim`, and feeds directly into `WorldModelCortex::processCycle()`. Guarded by `#ifdef NF_HAVE_OPENCV` for graceful degradation.
16. **Emergent concept composition in sleep** — `DreamProcessor::processREMDreams()` scans recent episodic memory pairs, evaluates compositional stability via `CompositionMetrics::evaluate()`, and auto-creates `Composite` `ConceptNode` entries in `SemanticMemory` when `is_compositional == true`. Unsupervised abstraction invention.
17. **Vector-only cognition** — Internal memory and language cognition operates exclusively on vectors, IDs, and weight matrices. String fields (e.g., `SymbolicToken.symbol`, `ConceptNode.label`) are `DEBUG TAG` labels for human logging and TTS — never used for internal retrieval or decision-making. A real brain doesn't store ASCII strings; neither should NeuroForge. `BrainPersistence` serializes only vector-based primary storage.
18. **Dual-storage deprecation** — Legacy string-based storage (`episodes_`, `concept_graph_`, `skills_`) is systematically deprecated. Primary storage uses `patterns_` (EpisodicPattern), `attractors_` (ConceptAttractor), `traces_` (ProceduralTrace), `weights_` matrices. `EpisodeAdapter` migrated from `storeEpisode()` to `encodePattern()` — no narrative strings generated or stored.

---

## 8. Change Log

| Date | Change | Runner/Module |
|------|--------|---------------|
| 2026-03-01 | Memory Optimization: EpisodeAdapter migrated from `storeEpisode()` to `encodePattern()` — no narrative strings generated/stored; `hashToVector()` for deterministic context encoding | `EpisodeAdapter` |
| 2026-03-01 | Vector-Only Cognition: 30+ string fields across 12 LanguageSystem structs annotated as `DEBUG TAG`/`DEBUG INDEX`; all string-keyed hashmaps marked as convenience-only | `LanguageSystem` |
| 2026-03-01 | TokenTracker Embedding Clustering: replaced `symbol.substr(0,2)` string-based clustering with cosine similarity on `embedding` vectors (≥0.7 threshold); added `embeddingCosineSimilarity()` helper | `LanguageSystem_TokenTracker.cpp` |
| 2026-03-02 | SQLite3 Optional Persistence: installed `sqlite3:x64-windows` via vcpkg; wired `NF_HAVE_SQLITE3=1` define + linkage into `neuroforge_core`; MemoryDB (4001 lines) now fully operational for debugging/explainability | `CMakeLists.txt`, `MemoryDB.cpp` |
| 2026-03-02 | Parallel Cognition Enhancement: T4 Cognition thread no longer skips when `!memdb`; added parallel memory recall dispatch — episodic `recallByCue()`, semantic `settle()`, procedural `recall()` — on every tick | `GrandUnifiedRunner.cpp` |
| 2026-03-01 | WebView2 Runtime Fix: installed vcpkg `webview2:x64-windows@1.0.3800.47` + `wil` dependency; CMake now detects `unofficial::webview2::webview2` and defines `NF_HAVE_WEBVIEW2=1`; embedded browser sandbox enabled | `CMakeLists.txt`, `WebSandbox.cpp` |
| 2026-03-01 | SemanticMemory Legacy Cleanup: `ConceptNode` → `[[deprecated]]`, `concept_graph_`/`label_to_id_`/`type_index_`/`keyword_index_` marked `DEBUG/COMPAT`; 4 legacy APIs (`createConcept`/`addConcept`/`retrieveConcept`/`retrieveConceptByLabel`) deprecated; callers in `DreamProcessor`/`MemoryIntegrator` wrapped with `#pragma` suppression | `SemanticMemory`, `DreamProcessor`, `MemoryIntegrator` |
| 2026-03-01 | ProceduralMemory Legacy Cleanup: `Skill`/`MotorAction`/`Habit` → `[[deprecated]]`, 7 string fields → `DEBUG TAG`, 5 legacy maps → `DEBUG/COMPAT`; primary API is `reinforce()`/`recall()` via weight matrix | `ProceduralMemory` |
| 2026-03-01 | Phase 6 Memory Cleanup: Removed `episodes_`/`memory_traces_` dual-storage from EpisodicMemoryManager; deprecated legacy `Episode` struct; legacy APIs reconstruct on-the-fly from `patterns_` | `EpisodicMemoryManager` |
| 2026-03-01 | Phase 5: Emergent Concept Composition — wired `CompositionMetrics::evaluate()` into `DreamProcessor::processREMDreams()` for autonomous concept invention during sleep | `DreamProcessor`, `CompositionMetrics`, `GrandUnifiedRunner` |
| 2026-03-01 | Phase 4: Real Vision Grounding — `cv::VideoCapture(0)` → 8×8 grayscale → 64-float → `WorldModelCortex::processCycle()` | `GrandUnifiedRunner` |
| 2026-03-01 | CLI: `--camera` flag for webcam input | `main.cpp`, `GrandUnifiedRunner.h` |
| 2026-02-24 | Phase 2: Conversational Interface (STT/TTS loop integration) | `AudioInputSystem` |
| 2026-02-24 | Brain Persistence Phase 1: High-level cognitive state serialization to JSON and auto-load integration | `BrainPersistence`, `GrandUnifiedRunner` |
| 2026-02-24 | Brain-like parallel memory: `shared_mutex` + `shared_lock` on 37 read methods across all 7 memory modules | All Memory Systems |
| 2026-02-24 | Thread safety: mutex locks on 9+ EpisodicMemoryManager methods, lock-free `*Locked()` helpers for deadlock prevention | `EpisodicMemoryManager` |
| 2026-02-24 | Dead code removal: `consolidated_episodes_` member and unused `getStatistics` loop | `EpisodicMemoryManager` |
| 2026-02-24 | Named constants: 7 `constexpr` constants replacing magic numbers in EpisodicMemoryManager | `EpisodicMemoryManager` |
| 2026-02-24 | Stage 3: `CompositionMetrics.h/cpp` — K(whole\|parts) proxy for emergent composition | `CompositionMetrics` |
| 2026-02-24 | Stage 2: Parallel genesis loop (5 threads, SharedState, atomic signals) | `GrandUnifiedRunner` |
| 2026-02-24 | Stage 1: `UniversalLearningSignal.h/cpp` — Δw = η·δ·∇I(gain) | `UniversalLearningSignal` |
| 2026-02-24 | Added `PlasticityRule::Universal` + LearningSystem integration | `Synapse.h`, `LearningSystem.h` |
| 2026-02-24 | CLI: `--enable-parallel`, `--perception-hz`, `--grid-hz`, `--web-hz`, `--cognition-hz` | `main.cpp` |
| 2026-02-23 | Created `GrandUnifiedRunner` (N-Genesis) | `--genesis` |
| 2026-02-23 | Created `N9AutomatedGatedRunner` (Stage D3) | `--n9-auto-gated` |
| 2026-02-23 | Wired attentional biases to web inputs | `WebSandboxRunner` |
| 2026-02-23 | Created PRD-DEV.md | Documentation |

---

## 9. What's Next (Candidate Work Items)

> These are **potential** directions. None are committed until explicitly tasked.

| Priority | Item | Notes |
|----------|------|-------|
| ✅ Complete | **SemanticMemory legacy cleanup** | `ConceptNode` deprecated; `concept_graph_` + 4 string-keyed hashmaps marked `DEBUG/COMPAT`; 4 legacy APIs deprecated |
| ✅ Complete | **ProceduralMemory legacy cleanup** | `Skill`/`MotorAction`/`Habit` deprecated; 5 legacy maps marked `DEBUG/COMPAT`; primary API is `reinforce()`/`recall()` |
| ✅ Complete | **TokenTracker embedding clustering** | Replaced `symbol.substr(0,2)` with cosine similarity on embedding vectors |
| ✅ Complete | **SQLite3 integration** | `NF_HAVE_SQLITE3=1` wired; MemoryDB persistence available (optional — for debugging/explainability) |
| ✅ Complete | **WebView2 runtime fix** | vcpkg `webview2:x64-windows` installed; `NF_HAVE_WEBVIEW2=1` enabled; browser sandbox operational |
| ✅ Complete | **Parallelize cognitive loop** | T4 always runs; parallel memory recall (episodic/semantic/procedural) on every tick |
| ✅ Complete | **EpisodicMemory dual-storage removal** | `episodes_`/`memory_traces_` removed; legacy `Episode` deprecated |
| ✅ Complete | **EpisodeAdapter migration** | `storeEpisode()` → `encodePattern()`; `generateNarrative()` deleted |
| ✅ Complete | **LanguageSystem debug annotations** | All string fields marked `DEBUG TAG`; vector-only cognition principle established |
| ✅ Complete | **DreamProcessor composition** | `CompositionMetrics` wired into `DreamProcessor::processREMDreams()` |
| 🟢 Low | **Active web interaction** | Click, type, form-fill (currently read-only) |
| 🟢 Low | **Eigen vectorization** | Replace hand-rolled cosine similarity / matrix multiply with Eigen (premature at 64-dim) |
| 🟢 Low | **Dashboard web UI** | `HOWTO-dashboard.md` exists |

---

*This document is maintained by the development agent. It reflects the actual state of the codebase, not aspirational goals. If you see something wrong, flag it and I'll update.*
