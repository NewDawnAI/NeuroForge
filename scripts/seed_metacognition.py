#!/usr/bin/env python3
import argparse
import json
import sqlite3
from typing import Optional, List, Dict, Any

def latest_run_id(conn: sqlite3.Connection) -> Optional[int]:
    cur = conn.cursor()
    cur.execute("SELECT MAX(id) FROM runs;")
    row = cur.fetchone()
    return int(row[0]) if row and row[0] is not None else None


def ensure_tables(conn: sqlite3.Connection) -> None:
    cur = conn.cursor()
    cur.execute(
        """
        CREATE TABLE IF NOT EXISTS metacognition (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id INTEGER,
            ts_ms INTEGER,
            self_trust REAL,
            narrative_rmse REAL,
            goal_mae REAL,
            ece REAL,
            notes TEXT,
            trust_delta REAL,
            coherence_delta REAL,
            goal_accuracy_delta REAL,
            self_explanation_json TEXT
        );
        """
    )
    cur.execute(
        """
        CREATE TABLE IF NOT EXISTS narrative_predictions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            run_id INTEGER,
            ts_ms INTEGER,
            reflection_id INTEGER,
            horizon_ms INTEGER,
            predicted_coherence_delta REAL,
            confidence REAL,
            targets_json TEXT
        );
        """
    )
    # Ensure columns exist even if table pre-existed
    def ensure_column(table: str, col: str, col_type: str) -> None:
        try:
            cur.execute(f"PRAGMA table_info({table});")
            cols = [r[1] for r in cur.fetchall()]
            if col not in cols:
                cur.execute(f"ALTER TABLE {table} ADD COLUMN {col} {col_type};")
        except Exception:
            pass
    ensure_column("metacognition", "trust_delta", "REAL")
    ensure_column("metacognition", "coherence_delta", "REAL")
    ensure_column("metacognition", "goal_accuracy_delta", "REAL")
    ensure_column("metacognition", "self_explanation_json", "TEXT")
    conn.commit()


def clamp01(x: float) -> float:
    return max(0.0, min(1.0, x))

# Fetch motivation_state coherence series to align deltas
def fetch_motivation_state(conn: sqlite3.Connection, run_id: int) -> List[Dict[str, Any]]:
    cur = conn.cursor()
    try:
        cur.execute("SELECT ts_ms, coherence FROM motivation_state WHERE run_id = ? ORDER BY ts_ms ASC;", (run_id,))
        rows = cur.fetchall()
        return [{"ts_ms": int(r[0]), "coherence": float(r[1]) if r[1] is not None else None} for r in rows]
    except Exception:
        return []


def seed_from_reward_log(conn: sqlite3.Connection, run_id: int, limit: int = 50) -> int:
    cur = conn.cursor()
    cur.execute(
        "SELECT id, run_id, ts_ms, step, reward, source, context_json FROM reward_log WHERE run_id = ? ORDER BY ts_ms ASC;",
        (run_id,),
    )
    rows = cur.fetchall()
    if not rows:
        return 0
    # Use only the last N rows to avoid over-seeding
    rows = rows[-limit:]
    inserted = 0
    # Prepare coherence alignment
    mot_rows = fetch_motivation_state(conn, run_id)
    mot_idx = 0
    last_coh = None
    last_trust = None
    last_acc = None
    for row in rows:
        _, _, ts_ms, step, reward, source, ctx_json = row
        try:
            ctx = json.loads(ctx_json) if ctx_json else {}
        except Exception:
            ctx = {}
        comp = float(ctx.get("competence_level", 0.5))
        self_trust = clamp01(comp)
        # Synthesize metrics with simple heuristics
        narrative_rmse = max(0.0, 0.5 * (1.0 - self_trust))
        goal_mae = max(0.0, 1.0 - self_trust)
        ece = 0.0
        # Advance motivation pointer to latest coherence <= ts_ms
        coh_val = last_coh
        while mot_idx < len(mot_rows) and mot_rows[mot_idx].get("ts_ms") is not None and mot_rows[mot_idx]["ts_ms"] <= int(ts_ms):
            cval = mot_rows[mot_idx].get("coherence")
            if cval is not None:
                coh_val = float(cval)
            mot_idx += 1
        # Compute deltas
        trust_delta = (float(self_trust) - float(last_trust)) if (last_trust is not None) else None
        accuracy = 1.0 - float(goal_mae)
        goal_accuracy_delta = (float(accuracy) - float(last_acc)) if (last_acc is not None) else None
        coherence_delta = (float(coh_val) - float(last_coh)) if (coh_val is not None and last_coh is not None) else None
        # Build explanation JSON summarizing causal attribution
        summary_bits = []
        if trust_delta is not None:
            summary_bits.append(f"trust {'rose' if trust_delta >= 0 else 'fell'} by {trust_delta:+.2f}")
        if coherence_delta is not None and abs(coherence_delta) > 1e-6:
            summary_bits.append(f"coherence {('up' if coherence_delta >= 0 else 'down')} {coherence_delta:+.2f}")
        if goal_accuracy_delta is not None and abs(goal_accuracy_delta) > 1e-6:
            summary_bits.append(f"goal accuracy {goal_accuracy_delta:+.2f}")
        summary = ", ".join(summary_bits) if summary_bits else "no significant change"
        explanation = {
            "summary": summary,
            "deltas": {
                "trust_delta": trust_delta,
                "coherence_delta": coherence_delta,
                "goal_accuracy_delta": goal_accuracy_delta,
            },
            "context": {
                "step": int(step),
                "source": source,
                "reward": float(reward) if reward is not None else None,
            },
        }
        notes = f"seeded_from_reward_log step={step} source={source} reward={float(reward):.3f}"
        cur.execute(
            "INSERT INTO metacognition (run_id, ts_ms, self_trust, narrative_rmse, goal_mae, ece, notes, trust_delta, coherence_delta, goal_accuracy_delta, self_explanation_json) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);",
            (
                run_id,
                int(ts_ms),
                float(self_trust),
                float(narrative_rmse),
                float(goal_mae),
                float(ece),
                notes,
                trust_delta if trust_delta is not None else None,
                coherence_delta if coherence_delta is not None else None,
                goal_accuracy_delta if goal_accuracy_delta is not None else None,
                json.dumps(explanation),
            ),
        )
        # Also seed a narrative_prediction aligned to this row
        cur.execute(
            "INSERT INTO narrative_predictions (run_id, ts_ms, reflection_id, horizon_ms, predicted_coherence_delta, confidence, targets_json) VALUES (?, ?, ?, ?, ?, ?, ?);",
            (run_id, int(ts_ms), 0, 500, float(coherence_delta or 0.0), float(self_trust), "[]"),
        )
        inserted += 1
        # Update last values
        last_trust = float(self_trust)
        last_coh = float(coh_val) if coh_val is not None else last_coh
        last_acc = float(accuracy)
    conn.commit()
    return inserted


def main() -> int:
    ap = argparse.ArgumentParser(description="Seed metacognition tables from reward_log to enable dashboard preview")
    ap.add_argument("--db", required=True, help="Path to SQLite database")
    ap.add_argument("--run_id", type=int, default=None, help="Run id to seed; default latest")
    ap.add_argument("--limit", type=int, default=50, help="Max number of rows to seed")
    ap.add_argument("--wipe_run", action="store_true", help="Delete existing rows for run_id before seeding")
    args = ap.parse_args()
    conn = sqlite3.connect(args.db)
    try:
        ensure_tables(conn)
        rid = args.run_id
        if rid is None:
            rid = latest_run_id(conn)
            if rid is None:
                print("No runs in DB; cannot seed")
                return 2
        if args.wipe_run:
            cur = conn.cursor()
            cur.execute("DELETE FROM metacognition WHERE run_id = ?;", (rid,))
            cur.execute("DELETE FROM narrative_predictions WHERE run_id = ?;", (rid,))
            conn.commit()
        count = seed_from_reward_log(conn, rid, args.limit)
        print(f"Seeded {count} metacognition rows for run {rid}")
        return 0
    finally:
        conn.close()

if __name__ == "__main__":
    raise SystemExit(main())