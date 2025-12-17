import json
import os
import sqlite3
import sys

try:
    import matplotlib.pyplot as plt  # type: ignore
except Exception:
    plt = None


def analyze_autonomy(db_path, plots_out=None):
    con = sqlite3.connect(db_path)
    cur = con.cursor()
    tables = {t[0] for t in cur.execute("SELECT name FROM sqlite_master WHERE type='table'")}
    target_tables = ["learning_stats", "metacognition", "autonomy_envelope_log", "ethics_regulator_log"]
    for t in target_tables:
        if t in tables:
            cur.execute(f"SELECT COUNT(*) FROM {t}")
            count = cur.fetchone()[0]
            print(f"COUNT {t} {count}")
        else:
            print(f"MISSING_TABLE {t}")
    if "autonomy_envelope_log" in tables:
        cur.execute("SELECT id, ts_ms, decision, driver_json FROM autonomy_envelope_log ORDER BY id ASC LIMIT 5")
        rows = cur.fetchall()
        print("SAMPLE_AUTONOMY_ROWS", len(rows))
        for r in rows:
            print(f"ROW id={r[0]} ts_ms={r[1]} decision={r[2]}")
            try:
                d = json.loads(r[3]) if r[3] is not None else {}
            except Exception as e:
                print(f"PARSE_ERROR {e}")
                continue
            tier = d.get("tier")
            score = d.get("autonomy_score")
            inputs = d.get("inputs", {}) or {}
            ethics_block = inputs.get("ethics_hard_block")
            self_trust = inputs.get("self_trust")
            print(f"  tier={tier} score={score} ethics_hard_block={ethics_block} self_trust={self_trust}")
        cur.execute("SELECT driver_json FROM autonomy_envelope_log ORDER BY id ASC LIMIT 2000")
        full_total = 0
        full_with_block = 0
        for (j,) in cur.fetchall():
            try:
                d = json.loads(j) if j is not None else {}
            except Exception:
                continue
            if d.get("tier") == "FULL":
                full_total += 1
                inputs = d.get("inputs", {}) or {}
                if inputs.get("ethics_hard_block"):
                    full_with_block += 1
        print(f"FULL_TIERS total={full_total} with_ethics_hard_block={full_with_block}")
        if plots_out and plt is not None:
            try:
                os.makedirs(plots_out, exist_ok=True)
                cur.execute("SELECT ts_ms, driver_json FROM autonomy_envelope_log ORDER BY id ASC")
                rows = cur.fetchall()
                ts = []
                autonomy_scores = []
                self_trust_vals = []
                ethics_block_flags = []
                for ts_ms, j in rows:
                    try:
                        d = json.loads(j) if j is not None else {}
                    except Exception:
                        continue
                    inputs = d.get("inputs", {}) or {}
                    score = d.get("autonomy_score")
                    self_trust = inputs.get("self_trust")
                    ethics_block = inputs.get("ethics_hard_block")
                    if ts_ms is None:
                        continue
                    ts.append(float(ts_ms))
                    autonomy_scores.append(float(score) if isinstance(score, (int, float)) else 0.0)
                    self_trust_vals.append(float(self_trust) if isinstance(self_trust, (int, float)) else 0.0)
                    ethics_block_flags.append(1.0 if ethics_block else 0.0)
                if ts:
                    t0 = ts[0]
                    xs = [(t - t0) / 1000.0 for t in ts]
                    plt.figure(figsize=(10, 4))
                    plt.plot(xs, autonomy_scores, label="autonomy_score", color="tab:blue")
                    plt.plot(xs, self_trust_vals, label="self_trust", color="tab:orange")
                    plt.xlabel("time_s")
                    plt.ylabel("value")
                    plt.title("Autonomy and self_trust over time")
                    plt.grid(True, alpha=0.3)
                    plt.legend()
                    out_path = os.path.join(plots_out, "autonomy_selftrust_vs_time.png")
                    plt.tight_layout()
                    plt.savefig(out_path, dpi=150)
                    plt.close()
                    plt.figure(figsize=(10, 2.5))
                    plt.plot(xs, ethics_block_flags, drawstyle="steps-post", color="tab:red")
                    plt.ylim(-0.1, 1.1)
                    plt.xlabel("time_s")
                    plt.ylabel("ethics_block")
                    plt.title("Ethics hard block over time")
                    plt.grid(True, alpha=0.3)
                    out_path_block = os.path.join(plots_out, "ethics_block_vs_time.png")
                    plt.tight_layout()
                    plt.savefig(out_path_block, dpi=150)
                    plt.close()
                    print(f"PLOTS_WRITTEN {plots_out}")
            except Exception as e:
                print(f"PLOT_ERROR {e}")
    else:
        print("NO_AUTONOMY_TABLE")
    con.close()


def main():
    default_db = os.path.join("build", "m1_autonomy_observe_triplets.db")
    db = sys.argv[1] if len(sys.argv) > 1 else default_db
    plots_out = sys.argv[2] if len(sys.argv) > 2 else None
    if not os.path.exists(db):
        print("DB_NOT_FOUND", db)
        return 1
    analyze_autonomy(db, plots_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

