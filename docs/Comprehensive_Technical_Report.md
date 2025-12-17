# NeuroForge Comprehensive Technical Report

Version: v0.15.0-rc1-12-gdf2999b5 (commit df2999b5)
Timestamp: 2025-12-08

## 1. Component Inventory

### Overview
- Core substrate coordinated by `HypergraphBrain` with `Region`, `Neuron`, `Synapse`, `LearningSystem`, and rich telemetry via `MemoryDB`.
- Optional subsystems gated by build flags: CUDA acceleration, sandbox (WebView2), viewer (GLFW/GLAD/OpenGL), OpenCV-based encoders.

### UI Components
- Viewer (`src/viewer/ViewerMain.cpp`): Renders 3D neuron/synapse graphs from CSV snapshots/spikes; depends on `glfw`, `glad`, and `OpenGL` when `NEUROFORGE_WITH_VIEWER` is enabled.
- Web pages (`pages/` and `web/`): Static dashboards and JSON/CSV artifacts (e.g., `pages/dashboard/phase9.html`, `web/phase9.html`).
- Sandbox (`src/sandbox/WebSandbox.cpp`): Optional Windows WebView2 embedded window (`NF_HAVE_WEBVIEW2`) with constrained browser interaction, DPI awareness, and event handling.

### Core Modules (src/core, include/core)
- `HypergraphBrain` (facade/orchestrator): Modality routing, region registry, connectivity, learning loops, telemetry integration, spike observers.
- `Region`: Manages neuron collections, local/inter-region connections, processing hooks, statistics, and thread-safety.
- `Neuron`: Activation/state, refractory handling, spike callbacks, input/output synapse lists.
- `Synapse`: Weighted connections with plasticity (Hebbian, STDP, BCM, Oja), delays, eligibility traces, statistics.
- `LearningSystem`: Hebbian/STDP updates, reward/attention integration, statistics, consolidation hooks.
- `MemoryDB`: SQLite telemetry for runs, experiences, episodes, rewards, metacognition, consistency, goals, motivation, substrate snapshots.
- `CUDAAccel`: GPU acceleration facade; wraps kernels in `src/cuda/kernels.cu` when `ENABLE_CUDA`.
- Phase systems: Phase A mimicry (`PhaseAMimicry`), Phase C substrate bindings/sequences (`SubstratePhaseC`), higher phases (6–15) for cognition, ethics, autonomy, and metacognition.
- `ContextHooks`, `ActionFilter`: Peer coupling, sampling integrations, and unified action gating.

### Biases (src/biases)
- `NoveltyBias`, `MotionBias`, `VoiceBias`, `TemporalBias` and others modulate attention, rewards, and coherence for modality-specific inputs; integrate with learning and ethics gating.

### Encoders
- `VisionEncoder`, `AudioEncoder` (include/encoders, src/encoders): Extract features for cortical regions; OpenCV integration where available.

### CUDA (src/cuda)
- `kernels.cu`: GPU kernels for Hebbian/STDP, leaky integrate, reduce mean, mitochondrial dynamics; host wrappers via `nf_cuda_*` functions.

### Scripts (scripts/) and Tools (tools/)
- Bench/build runners (`CMakeLists.txt`, `run_benchmark.ps1`).
- Analysis and dashboards (`phase_c_run.py`, `memdb_dashboard.py`, `generate_report.py`).
- DB/data utilities (`db_export.py`, `db_repl.py`, `summarize_*`, `stat_tests.py`).

### Dependencies & Relationships
- `HypergraphBrain` orchestrates regions and learning; logs to `MemoryDB`; exposes spike observers for viewer.
- Encoders feed modality vectors into regions; biases modulate signals; Phase A/C influence learning and telemetry.
- CUDAAccel provides accelerated paths for core updates when enabled.
- Sandbox constrains IO and interacts with foveation; viewer visualizes runtime state.

### Deprecated/Unused Components
- No explicit `DEPRECATED`/`OBSOLETE` markers found across code and docs.
- Several demo/test executables are gated by build flags and may be inactive in some configurations.

## 2. Documentation Review

### Completeness & Accuracy
- Strong coverage: System overview (`docs/README_SYSTEM.md`), API references (`documentation/technical/API_DOCUMENTATION.md`, `docs/API_Reference_Enhanced.md`), GPU/build guides, phase guides (`Phase A`, `Phase C`, `6–9`, `15`, `17`).
- Telemetry: `docs/telemetry.md` and SQL schema files present; rich tables but lacking a canonical, versioned schema reference.

### Mismatches & Gaps
- MemoryDB: Missing consolidated, versioned schema with migrations, indexing, retention policies, and performance tuning.
- Substrate APIs: No formal API documents for `Neuron`, `Synapse`, `Region` data models and update rules; limited serialization and guardrail descriptions.
- CUDAAccel: Missing class API, kernel coverage, batching thresholds, device memory layout, fallbacks, perf benchmarks, and error matrix.
- Viewer: Minimal documentation; lacks user/developer guide (controls, feeds, streaming, embedding, build notes, troubleshooting).
- Biases: No centralized catalog of bias parameters, calibration methodology, interactions with ethics regulator.
- Version skew: API docs (`v1.0`, `v2.0`, `v2.1`) need harmonization; build guide predates some flags/features.

### Recommended Doc Additions
- `docs/MemoryDB_Schema_Reference.md`
- `docs/Neurons_Synapses_Regions_API.md`
- `docs/CUDAAccel_API.md`
- `docs/Viewer_User_and_Developer_Guide.md`
- `docs/Biases_Calibration_and_Ethics.md`
- Version alignment across API/build docs and a component-to-doc index.

## 3. Code Quality Assessment

### Structure & Standards
- Namespaces and naming are generally consistent; headers use `#pragma once`.
- Smart pointers widely used; some raw allocations remain (`audio_capture.cpp`).

### Error Handling & Logging
- Mixed exceptions/boolean returns; few `noexcept` annotations.
- Logging is fragmented with `std::cout`/`std::cerr` and per-module CSV; no unified logging framework.

### Concurrency
- Mutexes/atomics used across core; occasional `recursive_mutex` and non-idiomatic optional locking (`unique_ptr<lock_guard>`).

### Build/Test Hygiene
- CMake targets are clean with guarded optional deps; global includes increase coupling.
- Tests exist across C++/Python, but no unified test framework or coverage tooling.

### Technical Debt & Improvements
- Introduce unified logging (levels/sinks/structured fields).
- Enforce RAII for platform resources (WASAPI buffers).
- Replace non-owning `shared_ptr` aliases with safer handles; consider `enable_shared_from_this` or references with clear lifetimes.
- Reduce `recursive_mutex`; standardize lock ordering; prefer `scoped_lock`.
- Clarify error contracts; annotate `noexcept` and use structured error types.
- Tighten CMake includes to per-target scopes.
- Modernize tests with a single framework and enable coverage; add static analysis and sanitizers in CI.

## 4. Architecture Analysis

### Components & Patterns
- Facade: `HypergraphBrain`.
- Factory: Phase A mimicry factory; region/neuron creation APIs.
- Observer: Spike observers and processing callbacks.

### Data Flows
```
Encoders -> HypergraphBrain -> Region -> Neuron <-> Synapse -> LearningSystem
Biases   -> modulate Region/LearningSystem
PhaseA/PhaseC <-> HypergraphBrain -> MemoryDB
HypergraphBrain -> Viewer
WebSandbox -> constrained IO/foveation
```

### Telemetry Flow
```
Core/Phases/Biases -> MemoryDB tables: runs, experiences, episodes,
learning_stats, substrate_states, snapshots, context_log, goals,
motivation, metacognition, consistency, autonomy_envelope, self_revision
```

### Key Decisions
- Unified substrate for continuous learning and shared representations.
- Mixed plasticity (Hebbian/STDP) with eligibility and reward shaping.
- Sparse/procedural connectivity for scale.
- Deep telemetry and dashboards for research validation.
- Safety via sandbox and ethics regulator; visualization via optional viewer.

## 5. Actionable Recommendations

- Logging: Adopt `spdlog` or equivalent; standardize levels/sinks and structured fields; consider integrating existing CSV writers.
- RAII: Wrap WASAPI buffer management; ensure deterministic cleanup.
- Ownership: Eliminate non-owning `shared_ptr` to `this`; prefer `weak_ptr`/references or `enable_shared_from_this` where appropriate.
- Concurrency: Remove `recursive_mutex` by refactoring; document lock order; use `scoped_lock`.
- Errors: Define module-level policies; annotate `noexcept`; consider `expected<T,E>` for recoverable errors.
- Build: Restrict include visibility per-target; reduce global coupling.
- Testing: Consolidate under one framework; enable coverage and CI gates; add sanitizers and `clang-tidy`.
- Docs: Add the five recommended docs; align versions; create a component-to-doc index.

## 6. Critical Findings (Immediate Attention)

- Non-owning `shared_ptr` aliases to `this` risk dangling references and undefined behavior.
- Use of `recursive_mutex` indicates complex re-entrancy; may hide lock-order issues.
- Raw `new[]/delete[]` in `audio_capture.cpp` without RAII increases leak/crash risk.
- Fragmented logging and inconsistent error contracts hinder observability and robustness.

## References (Examples)
- Core: `src/core/Region.cpp`, `src/core/Neuron.cpp`, `src/core/Synapse.cpp`, `src/core/LearningSystem.cpp`, `src/core/MemoryDB.cpp`.
- Phases: `src/core/PhaseAMimicry.cpp`, `src/core/SubstratePhaseC.cpp`.
- UI: `src/viewer/ViewerMain.cpp`, `src/sandbox/WebSandbox.cpp`.
- CUDA: `src/cuda/kernels.cu`, `src/core/CUDAAccel.cpp`.
- Build: `CMakeLists.txt`.

