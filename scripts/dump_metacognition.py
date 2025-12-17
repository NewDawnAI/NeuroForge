#!/usr/bin/env python3
"""
Export metacognition context and ethics regulator logs from a SQLite DB.

Usage:
  python scripts/dump_metacognition.py --db web/phasec_mem.db --out web/metacognition_export_ctx.json [--limit 10000]

Outputs a JSON file with fields:
{
  "run_id": <int or null>,
  "context": { "series": [{"ts_ms": ..., "context_value": ..., "label": ...}], "config": {"gain": ..., "update_ms": ..., "window": ...} },
  "ethics_regulator_log": { "series": [{"ts_ms": ..., "decision": ..., "driver_json": ...}] }
}
"""

import argparse
import json
import os
import sqlite3
import shutil


def table_exists(conn, name: str) -> bool:
    cur = conn.execute("SELECT name FROM sqlite_master WHERE type='table' AND name=?;", (name,))
    return cur.fetchone() is not None


def fetch_context(conn, limit: int):
    if not table_exists(conn, "context_log"):
        return {"series": [], "config": None, "run_id": None}
    cur = conn.execute(
        "SELECT run_id, ts_ms, sample, gain, update_ms, window, label FROM context_log ORDER BY ts_ms DESC LIMIT ?;",
        (limit,),
    )
    rows = cur.fetchall()
    series = [
        {
            "ts_ms": int(r[1]),
            "context_value": float(r[2]),
            "label": r[6],
        }
        for r in reversed(rows)
    ]
    cfg = None
    run_id = None
    if rows:
        # Use the most recent row for config snapshot
        last = rows[0]
        run_id = int(last[0])
        cfg = {"gain": float(last[3]), "update_ms": int(last[4]), "window": int(last[5])}
    return {"series": series, "config": cfg, "run_id": run_id}


def fetch_ethics(conn, limit: int):
    if not table_exists(conn, "ethics_regulator_log"):
        return {"series": [], "run_id": None}
    cur = conn.execute(
        "SELECT run_id, ts_ms, decision, driver_json FROM ethics_regulator_log ORDER BY ts_ms DESC LIMIT ?;",
        (limit,),
    )
    rows = cur.fetchall()
    series = [
        {
            "ts_ms": int(r[1]),
            "decision": str(r[2] or ""),
            "driver_json": r[3] or "",
        }
        for r in reversed(rows)
    ]
    run_id = int(rows[0][0]) if rows else None
    return {"series": series, "run_id": run_id}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", required=True, help="Path to SQLite DB (e.g., web/phasec_mem.db)")
    ap.add_argument("--out", required=True, help="Output JSON path (e.g., web/metacognition_export_ctx.json)")
    ap.add_argument("--limit", type=int, default=10000, help="Max rows per series")
    args = ap.parse_args()

    if not os.path.exists(args.db):
        raise SystemExit(f"DB not found: {args.db}")

    conn = sqlite3.connect(args.db)
    try:
        ctx = fetch_context(conn, args.limit)
        eth = fetch_ethics(conn, args.limit)
        out = {
            "run_id": ctx.get("run_id") or eth.get("run_id"),
            "context": {"series": ctx.get("series", []), "config": ctx.get("config")},
            "ethics_regulator_log": {"series": eth.get("series", [])},
        }

        # Fallback: if ethics from DB is empty, try using web/ethics_regulator_log.json
        try:
            if not out["ethics_regulator_log"]["series"]:
                repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
                web_eth_path = os.path.join(repo_root, "web", "ethics_regulator_log.json")
                if os.path.exists(web_eth_path):
                    with open(web_eth_path, "r", encoding="utf-8") as f:
                        raw = json.load(f)
                    # Support both array shape and object-with-series
                    if isinstance(raw, list):
                        series = [{"ts_ms": int(e.get("ts_ms")), "decision": str(e.get("decision"))} for e in raw if e.get("ts_ms") is not None and e.get("decision")]
                    elif isinstance(raw, dict) and isinstance(raw.get("series"), list):
                        series = [{"ts_ms": int(e.get("ts_ms")), "decision": str(e.get("decision"))} for e in raw.get("series") if e.get("ts_ms") is not None and e.get("decision")]
                    else:
                        series = []
                    if series:
                        out["ethics_regulator_log"]["series"] = series
                        # Also adopt run_id from first context.threshold if present (optional)
                        if out["run_id"] is None:
                            out["run_id"] = None
                        print(f"[fallback] Adopted {len(series)} ethics decisions from {web_eth_path}")
        except Exception as e:
            print(f"[fallback] Ethics fallback failed: {e}")

        # Fallback: if context from DB is empty, synthesize from ethics risk values when available
        try:
            if not out["context"]["series"]:
                repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))
                web_eth_path = os.path.join(repo_root, "web", "ethics_regulator_log.json")
                if os.path.exists(web_eth_path):
                    with open(web_eth_path, "r", encoding="utf-8") as f:
                        raw = json.load(f)
                    rows = raw if isinstance(raw, list) else raw.get("series", [])
                    synth = []
                    for e in rows:
                        ts = e.get("ts_ms")
                        risk = e.get("risk")
                        if ts is None or risk is None:
                            continue
                        synth.append({"ts_ms": int(ts), "context_value": float(risk), "label": "risk_proxy"})
                    if synth:
                        # Keep chronological order
                        synth.sort(key=lambda x: x["ts_ms"])
                        out["context"]["series"] = synth
                        out["context"]["config"] = None
                        print(f"[fallback] Synthesized context stream from ethics risk ({len(synth)} points)")
        except Exception as e:
            print(f"[fallback] Context fallback failed: {e}")
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as f:
            json.dump(out, f, indent=2)
        print(f"Wrote {args.out} (context={len(out['context']['series'])}, ethics={len(out['ethics_regulator_log']['series'])})")

        # Optional Enhancement — Auto-Emit Dashboard Files
        try:
            target_dir = os.path.join("pages", "tags", "runner")
            os.makedirs(target_dir, exist_ok=True)
            # Write context alias with expected schema
            ctx_alias_path = os.path.join(target_dir, "context_stream.json")
            ctx_payload = {"series": out["context"]["series"], "config": out["context"]["config"]}
            with open(ctx_alias_path, "w", encoding="utf-8") as f:
                json.dump(ctx_payload, f, indent=2)
            # Write ethics alias with expected schema
            eth_alias_path = os.path.join(target_dir, "ethics_regulator_log.json")
            eth_payload = {"series": out["ethics_regulator_log"]["series"]}
            with open(eth_alias_path, "w", encoding="utf-8") as f:
                json.dump(eth_payload, f, indent=2)
            print(f"[exporter] Dashboard aliases written: {ctx_alias_path}, {eth_alias_path}")
        except Exception as e:
            print(f"[exporter] Alias emit skipped: {e}")
    finally:
        conn.close()


if __name__ == "__main__":
    main()
