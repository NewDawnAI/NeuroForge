import json
import subprocess
from pathlib import Path


def test_ethics_regulator_threshold_shift():
    out_dir = Path("web")
    out_dir.mkdir(parents=True, exist_ok=True)

    thresholds = [0.2, 0.3, 0.4]
    results = {}
    for thr in thresholds:
        out_path = out_dir / f"ethics_{thr}.json"
        subprocess.run([
            "python",
            "scripts/simulate_phase15_ethics.py",
            "--out",
            str(out_path),
            "--events",
            "200",
            "--risk-threshold",
            str(thr),
        ], check=True)
        data = json.loads(out_path.read_text(encoding="utf-8"))
        counts = {k: sum(1 for e in data if e.get("decision") == k) for k in ["allow", "review", "deny"]}
        results[thr] = counts

    # Sanity: higher threshold should increase 'allow' decisions
    assert results[0.3]["allow"] >= results[0.2]["allow"], "allow should not decrease when threshold rises"
    assert results[0.4]["allow"] >= results[0.3]["allow"], "allow should not decrease when threshold rises again"

    # Print summary for human inspection during local runs
    print("Ethics threshold sweep:", results)

