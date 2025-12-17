import argparse
import json
import os
import sqlite3
import statistics
from datetime import datetime, timezone

# Optional plotting support; falls back gracefully if unavailable
try:
    import matplotlib.pyplot as plt  # type: ignore
except Exception:
    plt = None


def latest_run_id(conn):
    cur = conn.cursor()
    cur.execute("SELECT id, started_ms FROM runs ORDER BY id DESC LIMIT 1;")
    row = cur.fetchone()
    return row[0] if row else None


def get_ingestions(conn, run_id, limit=None):
    cur = conn.cursor()
    sql = "SELECT id, ts_ms, step, input_json FROM experiences WHERE run_id = ? AND tag = 'triplet_ingestion' ORDER BY ts_ms ASC;"
    cur.execute(sql, (run_id,))
    rows = cur.fetchall()
    items = []
    for r in rows:
        rid, ts, step, input_json = r
        try:
            payload = json.loads(input_json)
        except Exception:
            payload = {}
        items.append({"id": rid, "ts": ts, "step": step, "payload": payload})
    if isinstance(limit, int) and limit > 0:
        items = items[:limit]
    return items


def get_snapshots(conn, run_id):
    cur = conn.cursor()
    sql = "SELECT id, ts_ms, step, tag, input_json FROM experiences WHERE run_id = ? AND tag LIKE 'snapshot:%' ORDER BY ts_ms ASC;"
    cur.execute(sql, (run_id,))
    rows = cur.fetchall()
    items = []
    for r in rows:
        rid, ts, step, tag, input_json = r
        try:
            payload = json.loads(input_json)
        except Exception:
            payload = {}
        items.append({"id": rid, "ts": ts, "step": step, "tag": tag, "payload": payload})
    return items


def match_snapshots(ingestions, snapshots, window_ms=3000, max_k=5):
    out = {}
    s_ix = 0
    n = len(snapshots)
    for ing in ingestions:
        ts0 = ing["ts"]
        matched = []
        while s_ix < n and snapshots[s_ix]["ts"] < ts0:
            s_ix += 1
        j = s_ix
        while j < n and snapshots[j]["ts"] - ts0 <= window_ms and len(matched) < max_k:
            matched.append(snapshots[j])
            j += 1
        out[ing["id"]] = matched
    return out


def compute_metrics(ingestions, snap_map):
    # Aggregate core recall metrics and per-teacher statistics
    recall1 = 0
    recall5 = 0
    total = len(ingestions)
    sim_values = []
    by_teacher = {}
    by_teacher_success = {}
    for ing in ingestions:
        tid = ing["payload"].get("teacher_id", "")
        snaps = snap_map.get(ing["id"], [])
        by_teacher_success.setdefault(tid, {"success": 0, "total": 0, "sims": []})
        if snaps:
            p = snaps[0].get("payload", {})
            ph = p.get("phase_a", {})
            sim = ph.get("last_similarity")
            suc = ph.get("last_success")
            if isinstance(sim, (int, float)):
                sim_values.append(float(sim))
                by_teacher.setdefault(tid, []).append(float(sim))
                by_teacher_success[tid]["sims"].append(float(sim))
            if isinstance(suc, bool) and suc:
                recall1 += 1
                by_teacher_success[tid]["success"] += 1
            by_teacher_success[tid]["total"] += 1
        ok5 = False
        for s in snaps:
            p = s.get("payload", {})
            ph = p.get("phase_a", {})
            suc = ph.get("last_success")
            if isinstance(suc, bool) and suc:
                ok5 = True
                break
        if ok5:
            recall5 += 1
    grounding_accuracy = (recall1 / total) if total > 0 else 0.0
    r1 = (recall1 / total) if total > 0 else 0.0
    r5 = (recall5 / total) if total > 0 else 0.0
    stability_vals = []
    for tid, sims in by_teacher.items():
        if len(sims) >= 3:
            try:
                sd = statistics.pstdev(sims)
            except Exception:
                sd = 0.0
            stability_vals.append(1.0 / (1.0 + sd))
        else:
            stability_vals.append(0.5)
    token_stability = (sum(stability_vals) / len(stability_vals)) if stability_vals else 0.0
    transitions = 0
    gen_acc = 0.0
    for i in range(1, len(ingestions)):
        t_prev = ingestions[i - 1]["payload"].get("teacher_id", "")
        t_cur = ingestions[i]["payload"].get("teacher_id", "")
        if t_prev != t_cur:
            transitions += 1
            s_prev = snap_map.get(ingestions[i - 1]["id"], [])
            s_cur = snap_map.get(ingestions[i]["id"], [])
            def first_sim(snaps):
                if not snaps:
                    return None
                ph = snaps[0].get("payload", {}).get("phase_a", {})
                v = ph.get("last_similarity")
                return float(v) if isinstance(v, (int, float)) else None
            sp = first_sim(s_prev)
            sc = first_sim(s_cur)
            if sp is not None and sc is not None:
                gen_acc += 1.0 if sc >= 0.3 * sp else 0.0
    inter_sample_generalization = (gen_acc / transitions) if transitions > 0 else 0.0
    # Build per-teacher accuracy summary (confusion-like view)
    teacher_stats = {}
    for t, st in by_teacher_success.items():
        tot = st.get("total", 0) or 0
        suc = st.get("success", 0) or 0
        acc = (suc / tot) if tot > 0 else 0.0
        ms = st.get("sims", [])
        teacher_stats[t] = {
            "success_count": suc,
            "total": tot,
            "accuracy": acc,
            "mean_similarity": (sum(ms) / len(ms)) if ms else 0.0,
        }

    return {
        "recall@1": r1,
        "recall@5": r5,
        "grounding_accuracy": grounding_accuracy,
        "token_activation_stability": token_stability,
        "inter_sample_generalization": inter_sample_generalization,
        "similarity_mean": (sum(sim_values) / len(sim_values)) if sim_values else 0.0,
        "similarity_count": len(sim_values),
        "teacher_stats": teacher_stats,
    }


def update_meta(meta_path, db_path, dataset_path, cmdline, run_id, metrics):
    meta = {}
    if os.path.exists(meta_path):
        try:
            with open(meta_path, "r", encoding="utf-8") as f:
                meta = json.load(f)
        except Exception:
            meta = {}
    meta["db"] = db_path
    meta["steps"] = meta.get("steps", 0)
    meta["timestamp_utc"] = datetime.now(timezone.utc).isoformat()
    meta["dataset_triplets_root"] = dataset_path
    meta["cli"] = cmdline
    meta["run_id"] = run_id
    meta["eval_triplet_grounding"] = metrics
    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump(meta, f, indent=4)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", required=True)
    ap.add_argument("--meta", required=True)
    ap.add_argument("--dataset", required=True)
    ap.add_argument("--cmd", required=True)
    ap.add_argument("--run_id", type=int, default=0)
    ap.add_argument("--limit", type=int, default=200)
    ap.add_argument("--window_ms", type=int, default=3000)
    # Optional outputs for confusion and plots
    ap.add_argument("--confusion_out", default="")
    ap.add_argument("--plots_out", default="")
    args = ap.parse_args()
    conn = sqlite3.connect(args.db)
    try:
        rid = args.run_id if args.run_id > 0 else latest_run_id(conn)
        if rid is None:
            print("No runs found")
            return 1
        ing = get_ingestions(conn, rid, args.limit)
        snaps = get_snapshots(conn, rid)
        snap_map = match_snapshots(ing, snaps, window_ms=args.window_ms, max_k=5)
        metrics = compute_metrics(ing, snap_map)
        update_meta(args.meta, args.db, args.dataset, args.cmd, rid, metrics)
        print(json.dumps(metrics, indent=2))
        # Optional: write per-teacher confusion-like CSV
        if args.confusion_out:
            try:
                with open(args.confusion_out, "w", encoding="utf-8") as f:
                    f.write("teacher_id,success,total,accuracy,mean_similarity\n")
                    for t, st in metrics.get("teacher_stats", {}).items():
                        f.write(f"{t},{st['success_count']},{st['total']},{st['accuracy']:.6f},{st['mean_similarity']:.6f}\n")
            except Exception:
                pass
        # Optional: generate plots if matplotlib is available
        if args.plots_out and plt is not None:
            try:
                os.makedirs(args.plots_out, exist_ok=True)
                # Plot similarity over ingestions
                xs = list(range(len(ing)))
                ys_sim = []
                ys_reward = []
                for i in range(len(ing)):
                    snaps_i = snap_map.get(ing[i]["id"], [])
                    if snaps_i:
                        ph = snaps_i[0].get("payload", {}).get("phase_a", {})
                        sim = ph.get("last_similarity")
                        rew = ph.get("last_reward")
                        ys_sim.append(float(sim) if isinstance(sim, (int, float)) else 0.0)
                        ys_reward.append(float(rew) if isinstance(rew, (int, float)) else 0.0)
                    else:
                        ys_sim.append(0.0)
                        ys_reward.append(0.0)
                # Similarity plot
                plt.figure(figsize=(8, 4))
                plt.plot(xs, ys_sim, label="Similarity", color="blue")
                plt.title("Phase A Similarity over Ingestions")
                plt.xlabel("Ingestion Index")
                plt.ylabel("Similarity")
                plt.grid(True, alpha=0.3)
                plt.legend()
                plt.tight_layout()
                plt.savefig(os.path.join(args.plots_out, "similarity.png"))
                plt.close()
                # Reward plot
                plt.figure(figsize=(8, 4))
                plt.plot(xs, ys_reward, label="Reward", color="green")
                plt.title("Phase A Reward over Ingestions")
                plt.xlabel("Ingestion Index")
                plt.ylabel("Reward")
                plt.grid(True, alpha=0.3)
                plt.legend()
                plt.tight_layout()
                plt.savefig(os.path.join(args.plots_out, "reward.png"))
                plt.close()
                # Per-teacher accuracy bar plot
                labels = list(metrics.get("teacher_stats", {}).keys())
                accs = [metrics["teacher_stats"][t]["accuracy"] for t in labels]
                if labels:
                    plt.figure(figsize=(max(8, len(labels)), 4))
                    plt.bar(labels, accs, color="purple")
                    plt.title("Per-Teacher Success Accuracy")
                    plt.xlabel("Teacher ID")
                    plt.ylabel("Accuracy")
                    plt.xticks(rotation=60, ha="right")
                    plt.tight_layout()
                    plt.savefig(os.path.join(args.plots_out, "teacher_accuracy.png"))
                    plt.close()
            except Exception:
                pass
        return 0
    finally:
        conn.close()


if __name__ == "__main__":
    raise SystemExit(main())
