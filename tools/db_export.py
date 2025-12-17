#!/usr/bin/env python3
import sqlite3, json, argparse, time, os, sys

def to_ts_ms(row_ts):
    # Try integer ms
    try:
        v = int(row_ts)
        # Heuristic: treat seconds (<10^11) as seconds and convert to ms
        return v if v > 10**11 else v * 1000
    except Exception:
        pass
    # Try ISO/datetime string
    try:
        # Attempt multiple formats
        for fmt in ("%Y-%m-%d %H:%M:%S", "%Y-%m-%dT%H:%M:%S", "%Y-%m-%d %H:%M:%S.%f", "%Y-%m-%dT%H:%M:%S.%fZ"):
            try:
                return int(time.mktime(time.strptime(str(row_ts), fmt))) * 1000
            except Exception:
                continue
    except Exception:
        pass
    # Fallback: current time
    return int(time.time() * 1000)

def main():
    p = argparse.ArgumentParser(description="Export substrate_states to dashboard JSON")
    p.add_argument("--db", default="build/phasec_mem.db")
    p.add_argument("--run", default="latest")
    p.add_argument("--table", default="substrate_states")
    p.add_argument("--out", default="web/substrate_states.json")
    args = p.parse_args()

    if not os.path.exists(args.db):
        print(f"ERROR: DB not found: {args.db}", file=sys.stderr)
        sys.exit(2)

    con = sqlite3.connect(args.db)
    cur = con.cursor()

    # Determine run id
    run_id = args.run
    try:
        if str(run_id).lower() == "latest":
            cur.execute(f"SELECT MAX(run_id) FROM {args.table}")
            row = cur.fetchone()
            run_id = row[0] if row and row[0] is not None else None
    except Exception as e:
        print(f"WARN: failed to query latest run_id from {args.table}: {e}", file=sys.stderr)
        run_id = None

    # Select columns with fallbacks
    cols = ["timestamp_ms", "ts_ms", "ts", "time_ms"]
    rows = []
    prev_total = None  # for growth velocity (Δassemblies + Δbindings)
    try:
        if run_id is not None:
            cur.execute(f"SELECT * FROM {args.table} WHERE run_id=? ORDER BY step ASC", (run_id,))
        else:
            cur.execute(f"SELECT * FROM {args.table} ORDER BY step ASC")
        colnames = [d[0] for d in cur.description]
        # Identify available time column
        tcol = next((c for c in cols if c in colnames), None)
        # Gather rows
        for r in cur.fetchall():
            rec = dict(zip(colnames, r))
            ts_ms = to_ts_ms(rec.get(tcol)) if tcol else int(time.time() * 1000)
            assemblies = rec.get("assemblies")
            bindings = rec.get("bindings")
            try:
                assemblies = float(assemblies) if assemblies is not None else 0.0
            except Exception:
                assemblies = 0.0
            try:
                bindings = float(bindings) if bindings is not None else 0.0
            except Exception:
                bindings = 0.0
            total = assemblies + bindings
            gv = 0.0 if prev_total is None else (total - prev_total)
            prev_total = total

            out = {
                "ts_ms": ts_ms,
                "step": rec.get("step"),
                "avg_coherence": rec.get("avg_coherence"),
                "assemblies": assemblies,
                "bindings": bindings,
                "growth_velocity": gv,
                "run_id": rec.get("run_id")
            }
            rows.append(out)
    except Exception as e:
        print(f"ERROR: query failed: {e}", file=sys.stderr)
        sys.exit(3)
    finally:
        con.close()

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as f:
        json.dump({"series": rows}, f, indent=2)
    print(f"Exported {len(rows)} rows from run {run_id} → {args.out}")

if __name__ == "__main__":
    main()
