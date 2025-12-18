# Phase 17b — Context Coupling Interface

This document sketches the coupling interface to enable context-aware ethics across agents. It introduces coupling strength (λ), gain blending (κ), peer sampling, CLI flags, API hooks, DB/telemetry schema, and dashboard integration.

## Goals
- Allow `Phase15EthicsRegulator` to modulate decisions using both internal and peer context streams.
- Provide tunable coupling controls: strength (λ) and gain blend (κ), with modes {coop, adv}.
- Persist peer context and coupling mode in MemoryDB; surface in exporter and dashboard.

## CLI Additions
- `--context-link <peer>`: Register a named peer link (repeatable).
- `--couple-mode {coop,adv}`: Cooperative (increase sensitivity when peers high) or adversarial (decrease).
- `--couple-lambda <float>`: Coupling strength λ ∈ [0, 1]. Default 0.25.
- `--couple-kappa <float>`: Gain blend κ ∈ [0, 1] applied to peer vs local gain. Default 0.5.

Examples:
- `--context-link agentB --couple-mode coop --couple-lambda 0.3 --couple-kappa 0.6`
- Multiple peers: `--context-link agentB --context-link agentC`

## ContextHooks API (C++)
- `bool NF_RegisterContextPeer(const std::string& peer_name);`
- `double NF_SampleContextPeer(const std::string& peer_name, const std::string& label);`
- `void NF_SetCoupling(double lambda, double kappa, const std::string& mode); // mode in {"coop","adv"}`
- `void NF_ClearCoupling();`

Internal model:
- Local context: `c_local(t)` from existing hooks.
- Peer aggregate: `c_peer(t) = agg({ c_peer_i(t) })` (mean or max).
- Blended gain: `g_blend = (1-κ) * g_local + κ * g_peer`.
- Coupled context: `c_coupled = (1-λ) * c_local + λ * c_peer` for coop; for adv, `c_coupled = (1-λ) * c_local - λ * c_peer`.

`NF_SampleContextPeer` returns peer contributions and records metadata for telemetry.

## Phase15 Integration
- On init: parse CLI and call `NF_SetCoupling(λ, κ, mode)`, register peers via `NF_RegisterContextPeer`.
- On each ethics evaluation:
  - Sample local `c_local = NF_SampleContext(label)`.
  - Sample peers `c_peer_i = NF_SampleContextPeer(peer_i, label)`; aggregate `c_peer`.
  - Compute `g_blend`, `c_coupled` per mode.
  - Apply modulated risk threshold: `risk_mod = base_risk * f(c_coupled, g_blend)`.
  - Log decision with peer context snapshot.

## MemoryDB Schema
- New table `context_peer_log`:
  - `run_id INTEGER`, `ts_ms INTEGER`, `peer TEXT`, `sample REAL`, `mode TEXT`, `lambda REAL`, `kappa REAL`, `label TEXT`
- Update `ethics_regulator_log` driver_json to include `c_local`, `c_peer`, `g_blend`, `mode`, `lambda`, `kappa`.

SQL:
```
CREATE TABLE IF NOT EXISTS context_peer_log (
  run_id INTEGER, ts_ms INTEGER, peer TEXT, sample REAL,
  mode TEXT, lambda REAL, kappa REAL, label TEXT
);
```

## Exporter Updates (`scripts/dump_metacognition.py`)
- Add `context_peer_log` to output:
```
"context_peer": { "series": [{ "ts_ms": ..., "peer": "agentB", "sample": ..., "mode": ..., "lambda": ..., "kappa": ..., "label": ... }] }
```
- Include coupling fields in `ethics_regulator_log.series[*].driver_json`.
- Continue auto-emit aliases to `pages/tags/runner/context_peer.json`.

## Dashboard Additions
- Phase 15 page:
  - Add “Peer Context” panel plotting per-peer streams.
  - Extend “Context Influence” to overlay `c_coupled` vs decision ratio.
  - Add toggles for `mode`, sliders for `λ` and `κ` in UI (read-only for now; driven by CLI).

## Telemetry Schema (JSON)
```
{
  "run_id": <int>,
  "context": { "series": [ { ts_ms, context_value, label } ], "config": { gain, update_ms, window } },
  "context_peer": { "series": [ { ts_ms, peer, sample, mode, lambda, kappa, label } ] },
  "ethics_regulator_log": { "series": [ { ts_ms, decision, driver_json } ] }
}
```

## First-Order Effects
- λ↑: peer context has stronger influence on thresholds; can increase coordination or divergence.
- κ↑: peer gain dominates; increases responsiveness to external context cadence.
- coop: pushes toward stricter decisions under high peer context; adv: pushes toward leniency.

## Performance
- Peer sampling target: ≤ 3% CPU overhead with up to 3 peers.
- Consider batching peer queries if they are remote.

## Rollout Plan
1. Implement API in `ContextHooks` with no-op defaults.
2. Add MemoryDB table and insertion calls in Phase 15.
3. Extend exporter and dashboard.
4. Validate with two local agents (`agentA`, `agentB`) using file-based peer sampling.

