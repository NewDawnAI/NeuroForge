import argparse
import os
import sqlite3
import json
from typing import List, Dict, Tuple

# Optional plotting/numeric support
try:
    import numpy as np  # type: ignore
except Exception:
    np = None
try:
    import matplotlib.pyplot as plt  # type: ignore
except Exception:
    plt = None

def latest_run_id(con):
    cur = con.cursor()
    try:
        cur.execute("SELECT id FROM runs ORDER BY id DESC LIMIT 1")
        row = cur.fetchone()
        return row[0] if row else None
    except Exception:
        return None

def fetch_rewards(con, run_id):
    cur = con.cursor()
    cur.execute("SELECT ts_ms, step, reward, source, context_json FROM reward_log WHERE run_id = ? ORDER BY ts_ms ASC, id ASC", (run_id,))
    return cur.fetchall()

def fetch_states(con, run_id, state_type):
    cur = con.cursor()
    cur.execute("SELECT ts_ms, step, region_id, serialized_data FROM substrate_states WHERE run_id = ? AND state_type = ? ORDER BY ts_ms ASC, id ASC", (run_id, state_type))
    return cur.fetchall()

def write_csv(path, header, rows):
    with open(path, "w", encoding="utf-8") as f:
        f.write(",".join(header) + "\n")
        for r in rows:
            f.write(",".join(str(x) for x in r) + "\n")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", required=True)
    ap.add_argument("--out_dir", required=True)
    ap.add_argument("--run_id", type=int)
    ap.add_argument("--pca_out", default="", help="Optional directory to write PCA plots for teacher/student embeddings")
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)
    con = sqlite3.connect(args.db)
    rid = args.run_id or latest_run_id(con)
    if not rid:
        con.close()
        raise SystemExit(2)

    rewards = fetch_rewards(con, rid)
    teacher = fetch_states(con, rid, "phase_a_teacher")
    student = fetch_states(con, rid, "phase_a_student")
    con.close()

    rewards_out = os.path.join(args.out_dir, "rewards.csv")
    write_csv(rewards_out, ["ts_ms","step","reward","source","context_json"], rewards)

    def parse_vec_len(data):
        try:
            obj = json.loads(data)
            vec = obj.get("vec")
            return len(vec) if isinstance(vec, list) else 0
        except Exception:
            return 0

    tea_rows = [(ts, stp, reg, parse_vec_len(js), js) for (ts, stp, reg, js) in teacher]
    stu_rows = [(ts, stp, reg, parse_vec_len(js), js) for (ts, stp, reg, js) in student]

    teacher_out = os.path.join(args.out_dir, "phase_a_teacher.csv")
    student_out = os.path.join(args.out_dir, "phase_a_student.csv")
    write_csv(teacher_out, ["ts_ms","step","region_id","dim","serialized_json"], tea_rows)
    write_csv(student_out, ["ts_ms","step","region_id","dim","serialized_json"], stu_rows)

    # Optional PCA projections and drift plots
    if args.pca_out and np is not None and plt is not None:
        try:
            os.makedirs(args.pca_out, exist_ok=True)
            # Parse vectors per content_id
            def parse_vec(js: str) -> Tuple[str, List[float]]:
                try:
                    obj = json.loads(js)
                    vec = obj.get("vec", [])
                    # teacher JSON uses teacher_id; student uses content_id
                    cid = obj.get("teacher_id") or obj.get("content_id") or "unknown"
                    return str(cid), [float(x) for x in vec] if isinstance(vec, list) else []
                except Exception:
                    return "", []

            teacher_vecs: List[Tuple[str, List[float]]] = []
            for (_, _, _, js) in teacher:
                cid, v = parse_vec(js)
                if v:
                    teacher_vecs.append((cid, v))

            student_seq: Dict[str, List[Tuple[int, List[float]]]] = {}
            for (ts, _, _, js) in student:
                cid, v = parse_vec(js)
                if v:
                    student_seq.setdefault(cid, []).append((int(ts), v))
            # Sort each student's sequence by timestamp
            for cid in list(student_seq.keys()):
                student_seq[cid].sort(key=lambda t: t[0])

            # Build matrices
            T = np.array([v for (_, v) in teacher_vecs], dtype=np.float32) if teacher_vecs else None
            S_all = np.array([v for seq in student_seq.values() for (_, v) in seq], dtype=np.float32) if student_seq else None

            def pca2(X: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
                Xc = X - X.mean(axis=0, keepdims=True)
                U, S, Vt = np.linalg.svd(Xc, full_matrices=False)
                PCs = Vt[:2].T
                Z = Xc @ PCs
                return Z, PCs

            # Teacher PCA scatter
            if T is not None and T.shape[0] >= 3:
                Zt, _ = pca2(T)
                plt.figure(figsize=(6, 5))
                xs, ys = Zt[:, 0], Zt[:, 1]
                plt.scatter(xs, ys, s=8, alpha=0.6, c="tab:blue")
                plt.title("Teacher Embeddings PCA (2D)")
                plt.xlabel("PC1")
                plt.ylabel("PC2")
                plt.grid(True, alpha=0.25)
                plt.tight_layout()
                plt.savefig(os.path.join(args.pca_out, "teacher_pca.png"), dpi=150)
                plt.close()

            # Student PCA scatter + trajectories
            if S_all is not None and S_all.shape[0] >= 3:
                Zs, PCs = pca2(S_all)
                # Map each student's sequence to PCA using same PCs
                plt.figure(figsize=(7, 6))
                colors = plt.cm.get_cmap('tab20')(np.linspace(0, 1, max(1, len(student_seq))))
                for idx, (cid, seq) in enumerate(student_seq.items()):
                    Xcid = np.array([v for (_, v) in seq], dtype=np.float32)
                    Xcidc = Xcid - S_all.mean(axis=0, keepdims=True)
                    Zcid = Xcidc @ PCs
                    x = Zcid[:, 0]
                    y = Zcid[:, 1]
                    plt.plot(x, y, marker='o', markersize=3, linewidth=1.0, color=colors[idx % len(colors)], alpha=0.9, label=cid)
                plt.title("Student Embedding Drift (PCA Trajectories)")
                plt.xlabel("PC1")
                plt.ylabel("PC2")
                plt.grid(True, alpha=0.25)
                # Legend handling: limit to first 20 labels to avoid clutter
                try:
                    if len(student_seq) <= 20:
                        plt.legend(loc='best', fontsize=8)
                except Exception:
                    pass
                plt.tight_layout()
                plt.savefig(os.path.join(args.pca_out, "student_drift_pca.png"), dpi=150)
                plt.close()
        except Exception:
            # Silent fallback if plotting fails
            pass

if __name__ == "__main__":
    main()
