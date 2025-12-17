import argparse
import json
import os
import re
import sys

try:
    import matplotlib.pyplot as plt
except Exception:
    plt = None

def parse_lr_rs_from_cli(cli):
    lr = None
    rs = None
    if isinstance(cli, str):
        m_lr = re.search(r"--student-learning-rate=([0-9]*\.?[0-9]+)", cli)
        m_rs = re.search(r"--reward-scale=([0-9]*\.?[0-9]+)", cli)
        if m_lr:
            try:
                lr = float(m_lr.group(1))
            except Exception:
                lr = None
        if m_rs:
            try:
                rs = float(m_rs.group(1))
            except Exception:
                rs = None
    return lr, rs

def parse_lr_rs_from_dir(dirname):
    lr = None
    rs = None
    try:
        m = re.match(r"lr_([0-9_]+)__rs_([0-9_]+)$", dirname)
        if m:
            s_lr = m.group(1).replace("_", ".")
            s_rs = m.group(2).replace("_", ".")
            lr = float(s_lr)
            rs = float(s_rs)
    except Exception:
        lr = None
        rs = None
    return lr, rs

def collect_runs(base_dir):
    rows = []
    for name in os.listdir(base_dir):
        run_dir = os.path.join(base_dir, name)
        if not os.path.isdir(run_dir):
            continue
        meta_path = os.path.join(run_dir, "run_meta.json")
        if not os.path.exists(meta_path):
            continue
        try:
            with open(meta_path, "r", encoding="utf-8") as f:
                meta = json.load(f)
        except Exception:
            continue
        metrics = meta.get("eval_triplet_grounding") or {}
        cli = meta.get("cli", "")
        lr, rs = parse_lr_rs_from_cli(cli)
        if lr is None or rs is None:
            dlr, drs = parse_lr_rs_from_dir(name)
            lr = lr if lr is not None else dlr
            rs = rs if rs is not None else drs
        row = {
            "reward_scale": rs if rs is not None else 0.0,
            "student_lr": lr if lr is not None else 0.0,
            "recall@1": metrics.get("recall@1", 0.0),
            "recall@5": metrics.get("recall@5", 0.0),
            "grounding_accuracy": metrics.get("grounding_accuracy", 0.0),
            "token_activation_stability": metrics.get("token_activation_stability", 0.0),
            "inter_sample_generalization": metrics.get("inter_sample_generalization", 0.0),
            "similarity_mean": metrics.get("similarity_mean", 0.0),
            "similarity_count": metrics.get("similarity_count", 0),
            "run_dir": run_dir,
        }
        rows.append(row)
    return rows

def write_csv(rows, out_path):
    cols = [
        "reward_scale",
        "student_lr",
        "recall@1",
        "recall@5",
        "grounding_accuracy",
        "token_activation_stability",
        "inter_sample_generalization",
        "similarity_mean",
        "similarity_count",
        "run_dir",
    ]
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(",".join(cols) + "\n")
        for r in rows:
            vals = [
                r.get("reward_scale", 0.0),
                r.get("student_lr", 0.0),
                r.get("recall@1", 0.0),
                r.get("recall@5", 0.0),
                r.get("grounding_accuracy", 0.0),
                r.get("token_activation_stability", 0.0),
                r.get("inter_sample_generalization", 0.0),
                r.get("similarity_mean", 0.0),
                r.get("similarity_count", 0),
                r.get("run_dir", ""),
            ]
            f.write(",".join(str(v) for v in vals) + "\n")

def plot_scatter(rows, out_path):
    if plt is None:
        return
    xs = [r.get("student_lr", 0.0) for r in rows]
    ys = [r.get("recall@1", 0.0) for r in rows]
    cs = [r.get("reward_scale", 0.0) for r in rows]
    fig = plt.figure(figsize=(6, 4))
    sc = plt.scatter(xs, ys, c=cs, cmap="viridis")
    plt.colorbar(sc, label="reward_scale")
    plt.title("Recall@1 vs Learning Rate")
    plt.xlabel("student_lr")
    plt.ylabel("recall@1")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(out_path)
    plt.close()

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base_dir", default=os.path.join("experiments", "m1_triplet_grounding_sweep"))
    args = ap.parse_args()
    base = args.base_dir
    rows = collect_runs(base)
    out_csv = os.path.join(base, "sweep_results_summary.csv")
    write_csv(rows, out_csv)
    out_png = os.path.join(base, "combined_scatter_recall1_vs_lr.png")
    plot_scatter(rows, out_png)
    print(out_csv)
    print(out_png)

if __name__ == "__main__":
    sys.exit(main())
