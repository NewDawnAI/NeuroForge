# NeuroForge Development Kickoff Plan

## Objective
Begin execution of post-freeze development with production-grade reliability and bounded adaptive governance.

## Guiding Constraints
- Keep constitutional safety constraints non-bypassable.
- Allow learned governance policy improvements only inside hard boundaries.
- Preserve full auditability for authority changes, action gating, and self-revision.

## Sprint 0 (Kickoff: 1-2 weeks)

### 1) Engineering Baseline
- Freeze compiler warning baseline and fail CI on warning regressions.
- Add deterministic build + smoke test workflow for Linux and Windows.
- Publish a canonical run configuration for reproducible benchmarking.

### 2) Runtime Reliability
- Define loop-level SLOs:
  - cycle jitter target,
  - max step latency,
  - watchdog recovery behavior.
- Add supervisor health checks for:
  - scheduler stalls,
  - telemetry write failure,
  - action-gate subsystem failure.

### 3) State & Memory Foundation
- Define canonical state contract across:
  - synaptic weights,
  - working context,
  - episodic snapshots,
  - governance state.
- Introduce checkpoint invariants for save/load parity.
- Add replay parity test (same seed + same checkpoint => bounded divergence).

### 4) Governance Hardening
- Document fixed invariants that learned policy cannot override.
- Add explicit telemetry assertions for:
  - authority transition logging,
  - action gate decisions,
  - self-revision traceability.
- Add "self-exemption" regression checks in governance tests.

## Sprint 1 (2-4 weeks)

### 1) Parallel Loop Stabilization
- Isolate loop orchestration from CLI/demo logic.
- Add bounded queueing and backpressure handling.
- Add failure-mode tests for timeout/starvation paths.

### 2) Cap'n Proto State Path
- Re-enable and validate schema-backed state serialization path in CI.
- Add schema version-compatibility tests and migration checks.
- Add corruption/fuzz tests for state file loading.

### 3) Observability
- Standardize critical telemetry tables/dashboards for:
  - loop health,
  - governance decisions,
  - adaptation deltas,
  - checkpoint integrity.

## Done Criteria (Kickoff)
- CI runs deterministic smoke tests on every PR.
- Warning count does not regress.
- Governance invariants are tested and enforced.
- Save/load/replay path is validated with checkpoint parity tests.
- Sprint 1 stories are created and prioritized with owners.

## First Tickets to Open
1. CI warning regression gate.
2. Deterministic smoke test harness.
3. Loop SLO + watchdog instrumentation.
4. Checkpoint parity and replay test.
5. Governance self-exemption regression test.
6. Cap'n Proto schema compatibility suite.


## Initial Implementations
- `tools/check_warning_regression.py`: warning regression gate using build logs and a JSON baseline.
- `tools/ci_deterministic_smoke.py`: deterministic smoke harness that runs the engine twice and compares snapshot hashes.
- `tools/run_kickoff_ci.sh`: one-command local CI gate (configure/build, warning regression check, deterministic smoke).

### Suggested CI Commands
```bash
python tools/check_warning_regression.py --build-log build.log --write-baseline .ci/warnings-baseline.json
python tools/check_warning_regression.py --build-log build.log --baseline .ci/warnings-baseline.json
python tools/ci_deterministic_smoke.py --exe build/neuroforge --seed 123 --steps 5 --step-ms 1
./tools/run_kickoff_ci.sh
```

> Note: `run_kickoff_ci.sh` currently runs deterministic smoke in transitional mode (`--allow-mismatch`) to surface non-determinism without blocking kickoff execution. Remove this flag once determinism parity is stable.
