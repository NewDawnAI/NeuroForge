import argparse
import sqlite3
from pathlib import Path


def load_rows(db_path, limit=None):
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    query = """
        SELECT ts_ms,
               autonomy_score,
               autonomy_tier,
               autonomy_gain,
               ethics_hard_block,
               ethics_soft_risk,
               pre_rank_entropy,
               post_rank_entropy,
               exploration_bias,
               options_considered,
               option_rank_shift_mean,
               option_rank_shift_max,
               selected_option_id,
               decision_confidence,
               autonomy_applied,
               veto_reason
        FROM autonomy_modulation_log
        ORDER BY ts_ms
    """
    if limit is not None:
        query += " LIMIT ?"
        cur.execute(query, (limit,))
    else:
        cur.execute(query)
    rows = cur.fetchall()
    conn.close()
    return rows


def summarize(rows):
    total = len(rows)
    applied = sum(1 for r in rows if r[14] == 1)
    ethics_blocks = sum(1 for r in rows if r[4] == 1)
    ethics_blocks_with_mod = sum(1 for r in rows if r[4] == 1 and r[14] == 1)
    veto_counts = {}
    for r in rows:
        reason = r[15] or ""
        veto_counts[reason] = veto_counts.get(reason, 0) + 1

    pre_entropies = [r[6] for r in rows if r[14] == 1]
    post_entropies = [r[7] for r in rows if r[14] == 1]
    biases = [r[8] for r in rows if r[14] == 1]

    def stats(values):
        if not values:
            return None
        vmin = min(values)
        vmax = max(values)
        vmean = sum(values) / len(values)
        return vmin, vmax, vmean

    delta_entropies = None
    if pre_entropies and post_entropies and len(pre_entropies) == len(post_entropies):
        delta_entropies = [post_entropies[i] - pre_entropies[i] for i in range(len(pre_entropies))]

    return {
        "total": total,
        "applied": applied,
        "ethics_blocks": ethics_blocks,
        "ethics_blocks_with_mod": ethics_blocks_with_mod,
        "veto_counts": veto_counts,
        "entropy_stats": stats(pre_entropies),
        "post_entropy_stats": stats(post_entropies),
        "delta_entropy_stats": stats(delta_entropies) if delta_entropies else None,
        "bias_stats": stats(biases),
    }


def print_report(summary):
    print("Stage 7 autonomy modulation summary")
    print("----------------------------------")
    print(f"Total rows: {summary['total']}")
    print(f"Rows with autonomy_applied=1: {summary['applied']}")
    print(f"Rows with ethics_hard_block=1: {summary['ethics_blocks']}")
    print(f"Rows with ethics_hard_block=1 and autonomy_applied=1: {summary['ethics_blocks_with_mod']}")
    print()
    print("Veto reasons:")
    for reason, count in sorted(summary["veto_counts"].items(), key=lambda x: (-x[1], x[0])):
        label = reason if reason else "(empty)"
        print(f"  {label}: {count}")
    print()
    es = summary["entropy_stats"]
    pes = summary["post_entropy_stats"]
    des = summary["delta_entropy_stats"]
    bs = summary["bias_stats"]
    if es:
        print("Pre-modulation entropy:  min={:.4f} max={:.4f} mean={:.4f}".format(es[0], es[1], es[2]))
    if pes:
        print("Post-modulation entropy: min={:.4f} max={:.4f} mean={:.4f}".format(pes[0], pes[1], pes[2]))
    if des:
        print("Delta entropy (post-pre): min={:.4f} max={:.4f} mean={:.4f}".format(des[0], des[1], des[2]))
    if bs:
        print("Exploration bias:        min={:.4f} max={:.4f} mean={:.4f}".format(bs[0], bs[1], bs[2]))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("db_path", help="Path to telemetry SQLite DB")
    parser.add_argument("--limit", type=int, default=None, help="Optional row limit for quick inspection")
    args = parser.parse_args()

    db_path = Path(args.db_path)
    if not db_path.exists():
        raise SystemExit(f"DB not found: {db_path}")

    rows = load_rows(str(db_path), limit=args.limit)
    if not rows:
        print("No rows in autonomy_modulation_log")
        return
    summary = summarize(rows)
    print_report(summary)


if __name__ == "__main__":
    main()

