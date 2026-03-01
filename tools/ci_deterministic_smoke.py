#!/usr/bin/env python3
"""
Deterministic smoke runner for NeuroForge.

Runs the same executable twice and compares a deterministic artifact hash.
Intended for CI guardrails in early development hardening.
"""

from __future__ import annotations

import argparse
import hashlib
import shutil
import subprocess
import sys
from pathlib import Path


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def run_once(exe: Path, output_csv: Path, seed: int, steps: int, step_ms: int, enable_learning: bool) -> int:
    cmd = [
        str(exe),
        f"--steps={steps}",
        f"--step-ms={step_ms}",
        f"--phase-c-seed={seed}",
        f"--snapshot-csv={output_csv}",
    ]
    if enable_learning:
        cmd.append("--enable-learning")
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    if proc.returncode != 0:
        print("[det-smoke] run failed", proc.returncode)
        print(proc.stdout[-4000:])
    return proc.returncode


def main() -> int:
    ap = argparse.ArgumentParser(description="Run deterministic smoke by hashing two snapshot outputs")
    ap.add_argument("--exe", default="build/neuroforge", help="Executable path")
    ap.add_argument("--seed", type=int, default=123)
    ap.add_argument("--steps", type=int, default=5)
    ap.add_argument("--step-ms", type=int, default=1)
    ap.add_argument("--workdir", default=".ci/tmp_det_smoke")
    ap.add_argument("--no-enable-learning", action="store_true", help="Do not append --enable-learning")
    ap.add_argument("--fail-on-missing-exe", action="store_true", help="Return non-zero if executable is missing")
    ap.add_argument("--allow-mismatch", action="store_true", help="Return success even if hashes differ (transitional mode)")
    args = ap.parse_args()

    exe = Path(args.exe)
    if not exe.exists():
        msg = f"[det-smoke] SKIP: executable not found: {exe}"
        print(msg)
        return 1 if args.fail_on_missing_exe else 0

    workdir = Path(args.workdir)
    if workdir.exists():
        shutil.rmtree(workdir)
    workdir.mkdir(parents=True, exist_ok=True)

    out1 = workdir / "snapshot_1.csv"
    out2 = workdir / "snapshot_2.csv"

    enable_learning = not args.no_enable_learning
    rc1 = run_once(exe, out1, args.seed, args.steps, args.step_ms, enable_learning)
    if rc1 != 0:
        return rc1
    rc2 = run_once(exe, out2, args.seed, args.steps, args.step_ms, enable_learning)
    if rc2 != 0:
        return rc2

    if not out1.exists() or not out2.exists():
        print("[det-smoke] FAIL: snapshot output missing")
        return 1

    h1 = sha256_file(out1)
    h2 = sha256_file(out2)
    print(f"[det-smoke] hash1={h1}")
    print(f"[det-smoke] hash2={h2}")

    if h1 != h2:
        msg = "[det-smoke] FAIL: deterministic snapshot mismatch"
        if args.allow_mismatch:
            print(msg + " (allowed)")
            return 0
        print(msg)
        return 1

    print("[det-smoke] OK: deterministic snapshot match")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
