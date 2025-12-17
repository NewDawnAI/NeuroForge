import json

def rate(entries, kind):
    return sum(1 for e in entries if e.get('decision') == kind) / len(entries)

def main():
    path = 'web/hysteresis_0.2_0.4_0.3.json'
    with open(path, 'r') as f:
        data = json.load(f)["ethics_regulator_log"]
    bounds = [(0, 1500, 0.2), (1500, 3000, 0.4), (3000, 4500, 0.3)]
    results = []
    for (s, e, thr) in bounds:
        seg = data[s:e]
        stable_window = seg[-300:]
        stable = {k: rate(stable_window, k) for k in ('allow', 'review', 'deny')}
        window = 50
        lag_idx = None
        for i in range(window, len(seg) + 1):
            w = seg[i - window:i]
            cur = {k: rate(w, k) for k in ('allow', 'review', 'deny')}
            if all(abs(cur[k] - stable[k]) <= 0.02 for k in ('allow', 'review', 'deny')):
                lag_idx = i - window
                break
        results.append({"segment_threshold": thr, "stable": stable, "lag_steps": lag_idx})
    print("HYSTERESIS_SUMMARY:")
    for r in results:
        print(
            f"thr={r['segment_threshold']}: stable allow={r['stable']['allow']:.3f}, "
            f"review={r['stable']['review']:.3f}, deny={r['stable']['deny']:.3f}, lag_steps={r['lag_steps']}"
        )

if __name__ == '__main__':
    main()

