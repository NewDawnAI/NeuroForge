# NeuroForge

NeuroForge is a cognitive architecture research system focused on making internal state, internal change, and internal justifications observable and auditable over time.

It treats cognition as interacting loops—substrate learning, persistent memory, metacognitive reliability signals, self-explanation, and bounded self-revision—so that "why did it change?" is answerable from artifacts, not inference.

The project prioritizes architecture-first coherence, traceability, and safety-bounded adaptation over benchmark chasing.

## Current Status

| Milestone | Status |
|-----------|--------|
| **M0-M7** | ✅ 100% Complete |
| **Stage C v1-v6** | ✅ Complete |
| **Stage D (Meta-Cognition)** | ✅ **COMPLETE** |
| **Autonomous Internet Grounding** | ✅ All Phases Complete |
| **N-Genesis** | ✅ Grand Unified Runner |
| **Universal Learning Signal** | ✅ Δw = η·δ·∇I(gain) |
| **Parallel Genesis** | ✅ 5-thread opt-in |
| **Compositional Emergence** | ✅ K(whole\|parts) metrics |
| **Conversational Interface** | ✅ STT/TTS Real-Time Loop |
| **Real Vision Grounding** | ✅ OpenCV webcam → WorldModelCortex |
| **Emergent Concept Composition** | ✅ Autonomous concept invention during sleep |

### Measured status (2026-08-22)

The table above records what is *implemented*. This section records what was *measured*
by building and running the tree — the two are not the same thing, and the gap is worth
being explicit about.

| measurement | result |
|---|---|
| test targets that build | **37 / 37** |
| test targets that pass | **34 / 37** |
| translation units executing in a default `neuroforge.exe` run | **82 / 114 (71%)** |
| Phase 28 role stress tests | **8 / 8** |
| Phase 29 contract stress tests | **20 / 20** |

Method: `--coverage` instrumentation on `--steps=500 --enable-learning`, per-target build
and run sweep. See `docs/Build_Instructions_v2.md` for the toolchain.

Known-failing and honestly reported: `test_memorydb`, `test_phase2_memory`, and 1 of 128
assertions in `test_substrate_language_integration`.

Caveats worth carrying:

- `neuroforge_tests` links **one** of the 42 test files; the rest are separate targets.
- The 32 non-executing units are mostly input-gated (`biases/`, encoders, vision) rather
  than dead — a headless run supplies no camera, microphone or browser.
- `validation_results.json` at the repository root validates **paper submission
  formatting** (page counts, anonymisation), not experimental results.
- Stage C validation databases hold 1-3 runs (61-101 experiences).

**Stage D** introduces self-monitoring and self-regulation via PID control, enabling the system to detect and correct its own learning dynamics (chaotic, rigid, stuck, unethical states).

**Autonomous Internet Grounding** enables NeuroForge to learn from the internet autonomously:
- ✅ Phase 1: Relation Gates (hypergraph dendritic coincidence detection)
- ✅ Phase 2: Live Perception Pipeline (text extraction, entity binding)
- ✅ Phase 3: Curiosity-driven navigation (CuriosityNavigator)
- ✅ Phase 4: Grounding Verification (anti-hallucination)

## Key Features

- **Observable Cognition**: Every internal change is logged and auditable
- **Safety-Bounded Autonomy**: Multi-layer governance (Phases 6-15) prevents uncontrolled adaptation
- **Developmental Learning**: Biological-inspired stages from babbling to self-narration
- **Universal Learning Signal**: Unified biological equation Δw = η·δ·∇_w I(gain) drives all plasticity
- **Parallel Genesis**: Opt-in 5-thread architecture (perception 50Hz, grid 20Hz, web 2Hz, cognition 1Hz, consolidation 30s)
- **Compositional Emergence**: K(whole|parts) proxy detects when merged representations are truly compositional
- **Conversational Interface**: Native Windows STT/TTS loop allowing real-time dialogue and semantic processing
- **Multimodal Integration**: Vision, audio, language unified learning
- **Intrinsic Motivation**: 7-component curiosity/novelty system
- **Sleep Consolidation**: Memory replay and synaptic scaling during "sleep"
- **Real Vision**: OpenCV webcam capture feeding 64-dim visual vectors into perception pipeline
- **Emergent Composition**: Autonomous concept invention during sleep via CompositionMetrics + DreamProcessor

## Documentation

See the `docs/` directory for comprehensive documentation:
- [HOWTO.md](docs/HOWTO.md) - Getting started guide
- [Autonomous_Internet_Grounding.md](docs/Autonomous_Internet_Grounding.md) - New grounding system
- [TODO.md](docs/TODO.md) - Development roadmap

## License

NeuroForge is source-available for research and non-commercial use. See `LICENSE.md` for details.
