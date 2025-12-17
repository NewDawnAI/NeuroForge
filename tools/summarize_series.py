#!/usr/bin/env python3
import argparse, json, os, sys, statistics

def safe_float(x, default=0.0):
    try:
        return float(x) if x is not None else default
    except Exception:
        return default

def summarize_series(series):
    steps = [s.get("step") for s in series if s.get("step") is not None]
    avgc = [safe_float(s.get("avg_coherence"), None) for s in series]
    asm  = [safe_float(s.get("assemblies"), 0.0) for s in series]
    bnd  = [safe_float(s.get("bindings"), 0.0) for s in series]
    gvel = [safe_float(s.get("growth_velocity"), None) for s in series]

    def stats(arr):
        arr = [a for a in arr if a is not None]
        if len(arr) == 0:
            return {"count": 0}
        return {
            "count": len(arr),
            "min": min(arr),
            "max": max(arr),
            "mean": sum(arr) / len(arr),
            "variance": statistics.pvariance(arr) if len(arr) > 1 else 0.0,
        }

    totals = [a + b for a, b in zip(asm, bnd)]
    total_growth = totals[-1] - totals[0] if len(totals) >= 2 else 0.0

    return {
        "steps": {"min": min(steps) if steps else None, "max": max(steps) if steps else None},
        "avg_coherence": stats(avgc),
        "assemblies": stats(asm),
        "bindings": stats(bnd),
        "growth_velocity": stats(gvel),
        "total_growth": total_growth,
        "rows": len(series),
    }

def main():
    ap = argparse.ArgumentParser(description="Summarize exported substrate_states series JSON into simple stats")
    ap.add_argument("--json", default="web/substrate_states.json")
    ap.add_argument("--out", default="Artifacts/JSON/phasec_mem_summary.json")
    args = ap.parse_args()

    if not os.path.exists(args.json):
        print(f"ERROR: JSON not found: {args.json}", file=sys.stderr)
        sys.exit(2)

    with open(args.json, "r", encoding="utf-8") as f:
        payload = json.load(f)
    series = payload.get("series", [])
    summary = summarize_series(series)
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)
    print(f"Wrote summary → {args.out}\nRows: {summary['rows']} | Steps: {summary['steps']}\nCoherence variance: {summary['avg_coherence'].get('variance', 'n/a')} | Growth total: {summary['total_growth']}")

if __name__ == "__main__":
    main()

