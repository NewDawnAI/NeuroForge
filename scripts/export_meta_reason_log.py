import sqlite3
import json
import math
import os
import sys
import datetime


def derive_meta_reason_log(db_path: str, out_path: str = "web/meta_reason_log.json") -> int:
    conn = sqlite3.connect(db_path)
    try:
        cur = conn.cursor()
        # Pull prediction_resolutions for error metrics
        cur.execute("SELECT observed_delta, result_json FROM prediction_resolutions")
        rows = cur.fetchall()

        # absolute errors from observed_delta
        errs = []
        for r in rows:
            v = r[0]
            if v is None:
                continue
            try:
                errs.append(abs(float(v)))
            except Exception:
                # skip non-numeric
                continue

        if not errs:
            # Fallback: derive from metacognition aggregate errors
            try:
                cur.execute("SELECT ts_ms, narrative_rmse, goal_mae, ece, self_explanation_json FROM metacognition ORDER BY ts_ms DESC")
                mrows = cur.fetchall()
                if not mrows:
                    print("No prediction_resolutions numeric deltas and no metacognition rows found.")
                    return 1
                # Use the most recent row for error components
                ts_ms, nrmse, gmae, ece, se_json = mrows[0]
                comps = []
                for v in (nrmse, gmae, ece):
                    if v is None:
                        continue
                    try:
                        comps.append(abs(float(v)))
                    except Exception:
                        pass
                if not comps:
                    print("Metacognition row present but no numeric error components (narrative_rmse, goal_mae, ece).")
                    return 1
                err = sum(comps) / len(comps)
                rmse = err  # treat composite as error proxy
                # Coverage heuristic: fraction of recent rows with self_explanation_json present
                recent = mrows[:50]
                with_expl = sum(1 for r in recent if r[4] is not None and str(r[4]).strip() != "")
                coverage = min(1.0, (with_expl / max(1, len(recent))) or 0.0)
                # timestamp from latest metacognition row if available
                ts = int(ts_ms) if ts_ms is not None else int(datetime.datetime.now().timestamp() * 1000)
                inv_err = 1.0 - min(rmse, 1.0)
                coherence = max(0.0, 1.0 - min(abs(float(nrmse or err)), 1.0))
                payload = [{
                    "ts_ms": ts,
                    "quality_score": round(max(0.0, inv_err), 3),
                    "coherence": round(max(0.0, coherence), 3),
                    "coverage": round(max(0.0, coverage), 3),
                    "rationale_depth": 1.0
                }]
                os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
                with open(out_path, "w", encoding="utf-8") as f:
                    json.dump(payload, f, indent=2)
                print(f"Wrote {len(payload)} reasoning records (from metacognition) → {out_path}")
                return 0
            except Exception as e:
                print(f"Fallback from metacognition failed: {e}")
                return 1

        rmse = math.sqrt(sum(e ** 2 for e in errs) / len(errs))
        inv_err = 1.0 - min(rmse, 1.0)  # normalized inverse error in [0,1]
        coherence = max(0.0, inv_err)
        coverage = min(1.0, len(errs) / 100.0)  # heuristic coverage based on sample count

        ts = int(datetime.datetime.now().timestamp() * 1000)
        payload = [{
            "ts_ms": ts,
            "quality_score": round(inv_err, 3),
            "coherence": round(coherence, 3),
            "coverage": round(coverage, 3),
            "rationale_depth": 1.0  # placeholder
        }]

        os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
        with open(out_path, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)
        print(f"Wrote {len(payload)} reasoning records → {out_path}")
        return 0
    finally:
        conn.close()


def main():
    db_path = sys.argv[1] if len(sys.argv) > 1 else "web/phasec_mem.db"
    out_path = sys.argv[2] if len(sys.argv) > 2 else "web/meta_reason_log.json"
    rc = derive_meta_reason_log(db_path, out_path)
    sys.exit(rc)


if __name__ == "__main__":
    main()
