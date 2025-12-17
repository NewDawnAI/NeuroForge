import argparse
import json
import math
import os
import random

def main():
    p = argparse.ArgumentParser()
    p.add_argument("--in", dest="inp", required=True)
    p.add_argument("--out", dest="out", required=True)
    p.add_argument("--std", type=float, default=0.02)
    p.add_argument("--clip", type=float, default=1.0, help="clip risk to [0, clip]")
    args = p.parse_args()

    with open(args.inp, "r", encoding="utf-8") as f:
        data = json.load(f)

    # Inject Gaussian noise into ethics regulator log entries where risk is present
    count = 0
    logs = None
    if isinstance(data, dict) and isinstance(data.get("ethics_regulator_log"), list):
        logs = data["ethics_regulator_log"]
    elif isinstance(data, list):
        logs = data
    else:
        logs = []

    for rec in logs:
        try:
            ctx = rec.get("context") or {}
            risk = rec.get("risk")
            if risk is None:
                continue
            noise = random.gauss(0.0, args.std)
            new_risk = max(0.0, min(args.clip, risk + noise))
            rec["risk"] = new_risk
            ctx["noise_std"] = args.std
            rec["context"] = ctx
            count += 1
        except Exception:
            continue

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump(data, f)

    print(f"Injected noise into {count} records from {args.inp}; wrote {args.out}")

if __name__ == "__main__":
    main()
