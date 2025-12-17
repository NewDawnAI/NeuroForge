import argparse
import sqlite3
import json
import bisect

def fetch_data(db_path, run_id, source, align_mode):
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row

    # Pick latest run if none given
    rid = run_id
    if rid is None:
        rid = conn.execute('select max(id) from runs').fetchone()[0]

    # Fetch rewards for the chosen source
    rewards = conn.execute(
        'select step, reward from reward_log where run_id=? and source=? order by step asc',
        (rid, source)
    ).fetchall()

    # Fetch all learning stats (much denser)
    stats = conn.execute(
        'select step, avg_weight_change from learning_stats where run_id=? order by step asc',
        (rid,)
    ).fetchall()

    conn.close()

    r_steps = [int(r['step']) for r in rewards]
    r_vals  = [float(r['reward']) for r in rewards]

    s_steps = [int(s['step']) for s in stats]
    s_vals  = [float(s['avg_weight_change']) for s in stats]

    # Build mapping for stats because we will do step lookups
    step_to_w = {s_steps[i]: s_vals[i] for i in range(len(s_steps))}

    aligned_steps = []
    aligned_rewards = []
    aligned_weights = []

    for i, rstep in enumerate(r_steps):
        if align_mode == "nearest":
            # bisect finds insertion point to keep list sorted
            pos = bisect.bisect_left(s_steps, rstep)
            candidates = []
            if pos > 0:
                candidates.append(s_steps[pos - 1])
            if pos < len(s_steps):
                candidates.append(s_steps[pos])

            # choose whichever is closer
            if not candidates:
                continue
            best = min(candidates, key=lambda s: abs(s - rstep))

        elif align_mode == "floor":
            pos = bisect.bisect_right(s_steps, rstep) - 1
            if pos < 0:
                continue
            best = s_steps[pos]

        elif align_mode == "ceil":
            pos = bisect.bisect_left(s_steps, rstep)
            if pos >= len(s_steps):
                continue
            best = s_steps[pos]

        else:
            raise ValueError("Invalid align mode")

        aligned_steps.append(best)
        aligned_rewards.append(r_vals[i])
        aligned_weights.append(step_to_w[best])

    return rid, aligned_steps, aligned_rewards, aligned_weights


def try_plot(xs, yr, yw, title, out_png):
    try:
        import matplotlib.pyplot as plt
    except Exception:
        return False

    plt.figure(figsize=(10,6))
    plt.plot(xs, yr, label='reward')
    plt.plot(xs, yw, label='avg_weight_change')
    plt.xlabel('step')
    plt.ylabel('value')
    plt.title(title)
    plt.legend()
    plt.tight_layout()

    if out_png:
        plt.savefig(out_png)
    return True


def write_csv(xs, yr, yw, out_csv):
    path = out_csv or 'reward_weight_pairs.csv'
    with open(path, 'w', encoding='utf-8') as f:
        f.write('step,reward,avg_weight_change\n')
        for i in range(len(xs)):
            f.write(f"{xs[i]},{yr[i]},{yw[i]}\n")
    return path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--db', type=str, default='phasec_mem.db')
    ap.add_argument('--run', type=int)
    ap.add_argument('--source', type=str, default='shaped')
    ap.add_argument('--align', type=str, default='nearest',
                    choices=['nearest', 'floor', 'ceil'],
                    help='Step alignment mode (default: nearest)')
    ap.add_argument('--out-png', type=str)
    ap.add_argument('--out-csv', type=str)
    args = ap.parse_args()

    rid, xs, yr, yw = fetch_data(args.db, args.run, args.source, args.align)
    title = f"Run {rid} source={args.source} align={args.align}"

    if len(xs) == 0:
        print(f"WARNING: No {args.source} rewards found for run {rid}")
        print("Possible reasons:")
        print("  • Teacher embedding not used")
        print("  • No novelty/survival signals")
        print("  • Steps too low")

    if args.out_csv:
        path = write_csv(xs, yr, yw, args.out_csv)
        print("Wrote CSV to", path)

    if args.out_png:
        ok = try_plot(xs, yr, yw, title, args.out_png)
        if not ok and not args.out_csv:
            path = write_csv(xs, yr, yw, None)
            print("Matplotlib not available; wrote CSV to", path)


if __name__ == '__main__':
    main()
