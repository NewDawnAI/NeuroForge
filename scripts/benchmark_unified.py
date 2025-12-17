#!/usr/bin/env python3
"""
Unified benchmarking harness for NeuroForge's core neural substrate.

Experiments:
 - adaptive_vs_fixed: compare adaptive reflection on vs off
 - scale_sweep: sweep neuron counts to check smooth scaling
 - bias_ablation: disable SurvivalBias to demonstrate affect layer role

Outputs (default locations):
 - Artifacts/JSON/benchmarks/<exp>/<tag>_series.json              (exported telemetry series)
 - Artifacts/JSON/benchmarks/<exp>/<tag>_summary.json             (per-run summary stats)
 - Artifacts/PNG/benchmarks/<exp>/<tag>_{coherence,velocity,assemblies}.png  (optional plots)
 - Artifacts/JSON/benchmark_suite_summary.json                    (roll-up summary across runs)

Requires:
 - tools/db_export.py
 - tools/summarize_series.py
 - build/neuroforge.exe (or provide via --exe)
"""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

def run_neuroforge(exe: Path, db: Path, steps: int, wm_neurons: int, phasec_neurons: int,
                   hebb_rate: float, stdp_rate: float, memdb_interval: int,
                   adaptive: bool, survival_bias: bool, extra_flags=None) -> int:
    flags = [
        str(exe),
        "--unified-substrate=on",
        "--enable-learning",
        f"--hebbian-rate={hebb_rate}",
        f"--stdp-rate={stdp_rate}",
        f"--wm-neurons={wm_neurons}",
        f"--phasec-neurons={phasec_neurons}",
        f"--steps={steps}",
        f"--memory-db={db}",
        f"--memdb-interval={memdb_interval}",
        f"--adaptive={'on' if adaptive else 'off'}",
        f"--survival-bias={'on' if survival_bias else 'off'}",
        "--viewer=off",
    ]
    if extra_flags:
        flags.extend(extra_flags)
    print("[bench] Running:", ' '.join(flags))
    r = subprocess.run(flags, cwd=str(exe.parent), capture_output=True, text=True)
    if r.returncode != 0:
        print("[bench] ERROR: run failed\nSTDOUT:\n" + r.stdout + "\nSTDERR:\n" + r.stderr, file=sys.stderr)
    else:
        print("[bench] OK")
    return r.returncode

def export_series(db: Path, out_json: Path, table: str = "substrate_states", run: str = "latest") -> None:
    cmd = [sys.executable, str(Path("tools") / "db_export.py"), "--db", str(db), "--run", str(run), "--table", table, "--out", str(out_json)]
    print("[bench] Export series:", ' '.join(cmd))
    r = subprocess.run(cmd, cwd=str(Path(__file__).resolve().parent.parent), capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"Export failed: {r.stderr}\n{r.stdout}")
    print(r.stdout.strip())

def summarize_series(in_json: Path, out_json: Path) -> None:
    cmd = [sys.executable, str(Path("tools") / "summarize_series.py"), "--json", str(in_json), "--out", str(out_json)]
    print("[bench] Summarize:", ' '.join(cmd))
    r = subprocess.run(cmd, cwd=str(Path(__file__).resolve().parent.parent), capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"Summarize failed: {r.stderr}\n{r.stdout}")
    print(r.stdout.strip())

def maybe_plot(in_json: Path, out_dir: Path) -> None:
    try:
        import matplotlib.pyplot as plt
        payload = json.load(open(in_json, 'r', encoding='utf-8'))
        series = payload.get('series', [])
        if not series:
            print("[bench] WARN: No series to plot for", in_json)
            return
        steps = [s.get('step') for s in series]
        coh = [s.get('avg_coherence') or 0.0 for s in series]
        vel = [s.get('growth_velocity') or 0.0 for s in series]
        asm = [s.get('assemblies') or 0.0 for s in series]
        out_dir.mkdir(parents=True, exist_ok=True)
        plt.figure(figsize=(10,4)); plt.plot(steps, coh); plt.title('Coherence vs Step'); plt.xlabel('step'); plt.ylabel('avg_coherence'); plt.tight_layout(); plt.savefig(out_dir / 'coherence_vs_step.png', dpi=150); plt.close()
        plt.figure(figsize=(10,4)); plt.plot(steps, vel); plt.title('Growth Velocity vs Step'); plt.xlabel('step'); plt.ylabel('Δ(assemblies+bindings)'); plt.tight_layout(); plt.savefig(out_dir / 'growth_velocity_vs_step.png', dpi=150); plt.close()
        plt.figure(figsize=(10,4)); plt.plot(steps, asm); plt.title('Assemblies vs Step'); plt.xlabel('step'); plt.ylabel('assemblies'); plt.tight_layout(); plt.savefig(out_dir / 'assemblies_vs_step.png', dpi=150); plt.close()
        print(f"[bench] Wrote plots to {out_dir}")
    except Exception as e:
        print(f"[bench] WARN: plotting failed: {e}", file=sys.stderr)

def record_result(rollup: dict, exp: str, tag: str, summary_path: Path):
    try:
        summary = json.load(open(summary_path, 'r', encoding='utf-8'))
        rollup.setdefault(exp, {})[tag] = summary
    except Exception as e:
        print(f"[bench] WARN: failed to record {summary_path}: {e}", file=sys.stderr)

def main():
    ap = argparse.ArgumentParser(description='Unified substrate benchmarking harness')
    ap.add_argument('--exe', default=str(Path('build') / 'neuroforge.exe'))
    ap.add_argument('--db', default=str(Path('build') / 'phasec_mem.db'))
    ap.add_argument('--out', default=str(Path('Artifacts') / 'JSON' / 'benchmarks'))
    ap.add_argument('--png-out', default=str(Path('Artifacts') / 'PNG' / 'benchmarks'))
    ap.add_argument('--steps', type=int, default=1500)
    ap.add_argument('--wm-neurons', type=int, default=256)
    ap.add_argument('--phasec-neurons', type=int, default=256)
    ap.add_argument('--hebb', type=float, default=0.01)
    ap.add_argument('--stdp', type=float, default=0.005)
    ap.add_argument('--memdb-interval', type=int, default=500)
    ap.add_argument('--plots', action='store_true')
    ap.add_argument('--scale', type=int, nargs='+', default=[256, 512])
    args = ap.parse_args()

    exe = Path(args.exe).resolve(); db = Path(args.db).resolve()
    out_root = Path(args.out).resolve(); png_root = Path(args.png_out).resolve()
    out_root.mkdir(parents=True, exist_ok=True); png_root.mkdir(parents=True, exist_ok=True)

    rollup = {}

    # 1) Adaptive vs Fixed
    for adaptive_flag in [True, False]:
        tag = f"adaptive_{'on' if adaptive_flag else 'off'}"
        rc = run_neuroforge(exe, db, args.steps, args.wm_neurons, args.phasec_neurons, args.hebb, args.stdp, args.memdb_interval, adaptive_flag, True)
        if rc != 0:
            continue
        series_json = out_root / 'adaptive_vs_fixed' / f"{tag}_series.json"
        summary_json = out_root / 'adaptive_vs_fixed' / f"{tag}_summary.json"
        export_series(db, series_json, run='latest')
        summarize_series(series_json, summary_json)
        if args.plots:
            maybe_plot(series_json, png_root / 'adaptive_vs_fixed' / tag)
        record_result(rollup, 'adaptive_vs_fixed', tag, summary_json)

    # 2) Scale sweep
    for size in args.scale:
        tag = f"scale_{size}"
        rc = run_neuroforge(exe, db, args.steps, size, size, args.hebb, args.stdp, args.memdb_interval, True, True)
        if rc != 0:
            continue
        series_json = out_root / 'scale_sweep' / f"{tag}_series.json"
        summary_json = out_root / 'scale_sweep' / f"{tag}_summary.json"
        export_series(db, series_json, run='latest')
        summarize_series(series_json, summary_json)
        if args.plots:
            maybe_plot(series_json, png_root / 'scale_sweep' / tag)
        record_result(rollup, 'scale_sweep', tag, summary_json)

    # 3) Bias ablation (SurvivalBias off)
    tag = "survival_bias_off"
    rc = run_neuroforge(exe, db, args.steps, args.wm_neurons, args.phasec_neurons, args.hebb, args.stdp, args.memdb_interval, True, False)
    if rc == 0:
        series_json = out_root / 'bias_ablation' / f"{tag}_series.json"
        summary_json = out_root / 'bias_ablation' / f"{tag}_summary.json"
        export_series(db, series_json, run='latest')
        summarize_series(series_json, summary_json)
        if args.plots:
            maybe_plot(series_json, png_root / 'bias_ablation' / tag)
        record_result(rollup, 'bias_ablation', tag, summary_json)

    # Write roll-up
    suite_out = Path('Artifacts') / 'JSON' / 'benchmark_suite_summary.json'
    suite_out = suite_out.resolve()
    suite_out.parent.mkdir(parents=True, exist_ok=True)
    with open(suite_out, 'w', encoding='utf-8') as f:
        json.dump(rollup, f, indent=2)
    print(f"[bench] Wrote roll-up → {suite_out}")

if __name__ == '__main__':
    main()

