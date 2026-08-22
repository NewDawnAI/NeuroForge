NeuroForge is a cognitive architecture research system focused on making internal state, internal change, and internal justifications observable and auditable over time.
It treats cognition as interacting loops—substrate learning, persistent memory, metacognitive reliability signals, self-explanation, and bounded self-revision—so that “why did it change?” is answerable from artifacts, not inference.
The project prioritizes architecture-first coherence, traceability, and safety-bounded adaptation over benchmark chasing.
Status: foundational research prototype (not a library, SDK, or product).
NeuroForge is a reference implementation for governed learning systems with verifiable authority constraints.
A canonical Stage 3.5 (RWCI) run under frozen Stage C v1 governance has been executed and archived, demonstrating real-world learning without autonomy escalation.
NeuroForge exposes read-only application adapters for evaluation, benchmarking, and interpretability while core governance remains frozen.

## License
NeuroForge is source-available for research and non-commercial use. See `LICENSE.md` for details.

## Measured status (2026-08-22)

Status tables elsewhere record what is *implemented*. This records what was *measured* by
building and running the tree; the two are not the same thing.

| measurement | result |
|---|---|
| test targets that build | **37 / 37** |
| test targets that pass | **34 / 37** |
| translation units executing in a default run | **82 / 114 (71%)** |
| Phase 28 role stress tests | **8 / 8** |
| Phase 29 contract stress tests | **20 / 20** |

Method: `--coverage` instrumentation on `--steps=500 --enable-learning`, plus a per-target
build and run sweep. Toolchain in `docs/Build_Instructions_v2.md`.

Known-failing, honestly reported: `test_memorydb`, `test_phase2_memory`, and 1 of 128
assertions in `test_substrate_language_integration`.

Caveats worth carrying:

- `neuroforge_tests` links **one** of the 42 test files; the rest are separate targets.
- The 32 non-executing units are mostly input-gated (`biases/`, encoders, vision) rather
  than dead - a headless run supplies no camera, microphone or browser.
- `validation_results.json` at the repository root validates **paper submission
  formatting** (page counts, anonymisation), not experimental results.
- Stage C validation databases hold 1-3 runs (61-101 experiences).
