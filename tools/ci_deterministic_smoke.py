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


def run_once(exe: Path, output_csv: Path, seed: int, steps: int, step_ms: int) -> int:
    cmd = [
        str(exe),
        f"--steps={steps}",
        f"--step-ms={step_ms}",
        f"--phase-c-seed={seed}",
        f"--snapshot-csv={output_csv}",
    ]
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
    args = ap.parse_args()

    exe = Path(args.exe)
    if not exe.exists():
        print(f"[det-smoke] SKIP: executable not found: {exe}")
        return 0

    workdir = Path(args.workdir)
    if workdir.exists():
        shutil.rmtree(workdir)
    workdir.mkdir(parents=True, exist_ok=True)

    out1 = workdir / "snapshot_1.csv"
    out2 = workdir / "snapshot_2.csv"

    rc1 = run_once(exe, out1, args.seed, args.steps, args.step_ms)
    if rc1 != 0:
        return rc1
    rc2 = run_once(exe, out2, args.seed, args.steps, args.step_ms)
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
        print("[det-smoke] FAIL: deterministic snapshot mismatch")
        return 1

    print("[det-smoke] OK: deterministic snapshot match")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
