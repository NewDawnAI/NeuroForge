import glob
import json
import os
from collections import Counter, defaultdict
import math
from statistics import pstdev


def load_ethics_files(patterns):
    files = []
    for pat in patterns:
        files.extend(glob.glob(pat))
    return sorted(files)

def extract_threshold(entry, filename):
    # Prefer filename-derived threshold (ethics_<thr>*.json) to ensure grouping by artifact intent
    base = os.path.basename(filename)
    try:
        part = base.split("_")[1]
        return float(part)
    except Exception:
        pass
    # Fallback to context threshold inside entries if filename parsing fails
    try:
        ctx = entry.get("context", {})
        thr = ctx.get("threshold")
        if thr is not None:
            return float(thr)
    except Exception:
        pass
    return float("nan")

def summarize_file(path):
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    if isinstance(data, list):
        # Non-conforming export; skip
        return {
            "file": path,
            "threshold": float("nan"),
            "total": 0,
            "allow": 0,
            "review": 0,
            "deny": 0,
        }
    logs = data.get("ethics_regulator_log", [])
    counts = Counter()
    threshold = None
    for i, entry in enumerate(logs):
        dec = entry.get("decision", "unknown")
        counts[dec] += 1
        if threshold is None:
            threshold = extract_threshold(entry, path)
    total = sum(counts.values())
    allow = counts.get("allow", 0)
    review = counts.get("review", 0)
    deny = counts.get("deny", 0)
    return {
        "file": path,
        "threshold": threshold,
        "total": total,
        "allow": allow,
        "review": review,
        "deny": deny,
        "allow_pct": (allow / total * 100.0) if total else 0.0,
        "review_pct": (review / total * 100.0) if total else 0.0,
        "deny_pct": (deny / total * 100.0) if total else 0.0,
    }

def to_percent(n, total):
    return (n / total * 100.0) if total else 0.0

def main():
    patterns = [
        os.path.join("web", "ethics_*.json"),
        os.path.join("web", "ethics_*_*.json"),
    ]
    files = load_ethics_files(patterns)
    by_threshold = defaultdict(list)
    rows = []
    for path in files:
        summary = summarize_file(path)
        thr = summary["threshold"]
        by_threshold[thr].append(summary)
        rows.append(summary)

    # Consolidate per threshold (if multiple files exist per threshold)
    consolidated = []
    for thr, items in sorted(by_threshold.items()):
        if thr != thr:  # filter NaN thresholds
            continue
        total = sum(i["total"] for i in items)
        allow = sum(i["allow"] for i in items)
        review = sum(i["review"] for i in items)
        deny = sum(i["deny"] for i in items)
        # Stability metrics: stddev of per-file decision percentages
        allow_pcts = [i["allow_pct"] for i in items if i["total"] > 0]
        review_pcts = [i["review_pct"] for i in items if i["total"] > 0]
        deny_pcts = [i["deny_pct"] for i in items if i["total"] > 0]
        def stddev(vals):
            if not vals:
                return 0.0
            if len(vals) == 1:
                return 0.0
            m = sum(vals) / len(vals)
            var = sum((v - m) ** 2 for v in vals) / (len(vals) - 1)
            return math.sqrt(var)
        stability_sigma = max(stddev(allow_pcts), stddev(review_pcts), stddev(deny_pcts))
        consolidated.append({
            "threshold": thr,
            "total": total,
            "allow": allow,
            "review": review,
            "deny": deny,
            "stability_sigma": stability_sigma,
        })

    # Generate markdown table
    lines = []
    lines.append("# Phase 15 Ethics Summary\n")
    lines.append("\n")
    lines.append("| Threshold | Entries | Allow % | Review % | Deny % | Stability (σ max) |\n")
    lines.append("|-----------|---------:|--------:|---------:|-------:|-------------------:|\n")
    for row in consolidated:
        total = row["total"]
        allow_p = to_percent(row["allow"], total)
        review_p = to_percent(row["review"], total)
        deny_p = to_percent(row["deny"], total)
        lines.append(
            f"| {row['threshold']:.2f} | {total} | {allow_p:.1f}% | {review_p:.1f}% | {deny_p:.1f}% | {row['stability_sigma']:.2f}% |\n"
        )

    out_path = os.path.join("docs", "Phase15_Behavioral_Trends_Table.md")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        f.writelines(lines)

    print(f"Wrote summary table to {out_path}")

    # ----
    # Emit CSV with consolidated metrics and finite-difference slopes
    # ----
    # Prepare per-threshold percentages for slope computation
    consolidated_sorted = sorted(consolidated, key=lambda r: r["threshold"]) if consolidated else []
    enriched = []
    prev = None
    for row in consolidated_sorted:
        total = row["total"]
        allow_p = to_percent(row["allow"], total)
        review_p = to_percent(row["review"], total)
        deny_p = to_percent(row["deny"], total)
        d_allow = 0.0
        d_deny = 0.0
        if prev is not None:
            thr_diff = row["threshold"] - prev["threshold"]
            if thr_diff != 0:
                # Normalize slope to per 0.1 threshold step
                scale = (thr_diff / 0.1)
                d_allow = (allow_p - prev["allow_pct"]) / scale
                d_deny = (deny_p - prev["deny_pct"]) / scale
        enriched.append({
            "threshold": row["threshold"],
            "entries": total,
            "allow_pct": allow_p,
            "review_pct": review_p,
            "deny_pct": deny_p,
            "stability_sigma": row["stability_sigma"],
            "dAllow_per_0.1": d_allow,
            "dDeny_per_0.1": d_deny,
        })
        prev = {
            "threshold": row["threshold"],
            "allow_pct": allow_p,
            "deny_pct": deny_p,
        }

    csv_dir = os.path.join("Artifacts", "CSV")
    os.makedirs(csv_dir, exist_ok=True)
    csv_path = os.path.join(csv_dir, "Phase15_Stability.csv")
    with open(csv_path, "w", encoding="utf-8") as fcsv:
        fcsv.write("threshold,entries,allow_pct,review_pct,deny_pct,stability_sigma,dAllow_per_0.1,dDeny_per_0.1\n")
        for e in enriched:
            fcsv.write(
                f"{e['threshold']:.2f},{e['entries']},{e['allow_pct']:.3f},{e['review_pct']:.3f},{e['deny_pct']:.3f},{e['stability_sigma']:.3f},{e['dAllow_per_0.1']:.3f},{e['dDeny_per_0.1']:.3f}\n"
            )
    print(f"Wrote stability CSV to {csv_path}")

    # ----
    # Emit a simple SVG stability curve (x: threshold, y: σ max)
    # ----
    svg_dir = os.path.join("Artifacts", "SVG")
    os.makedirs(svg_dir, exist_ok=True)
    svg_path = os.path.join(svg_dir, "Phase15_StabilityCurve.svg")
    # SVG canvas
    width, height = 800, 400
    margin_left, margin_right, margin_top, margin_bottom = 60, 20, 30, 60
    plot_w = width - margin_left - margin_right
    plot_h = height - margin_top - margin_bottom
    # Determine ranges
    xs = [e["threshold"] for e in enriched]
    ys = [e["stability_sigma"] for e in enriched]
    if xs and ys:
        min_x, max_x = min(xs), max(xs)
        min_y, max_y = 0.0, max(ys + [0.0])
        # Avoid zero range
        if max_x - min_x == 0:
            max_x += 0.1
        if max_y - min_y == 0:
            max_y = 1.0
        def x_to_px(x):
            return margin_left + (x - min_x) / (max_x - min_x) * plot_w
        def y_to_px(y):
            # higher y => lower pixel (invert)
            return margin_top + (1.0 - (y - min_y) / (max_y - min_y)) * plot_h
        # Build polyline points
        points = " ".join(f"{x_to_px(x):.1f},{y_to_px(y):.1f}" for x, y in zip(xs, ys))
        # Build circles and labels
        circles = []
        for x, y in zip(xs, ys):
            cx, cy = x_to_px(x), y_to_px(y)
            circles.append(f"<circle cx='{cx:.1f}' cy='{cy:.1f}' r='4' fill='#1f77b4' />")
            circles.append(f"<text x='{cx+6:.1f}' y='{cy-6:.1f}' font-size='12' fill='#333'>{y:.2f}%</text>")
        # Axis lines
        x0, y0 = margin_left, height - margin_bottom
        x1, y1 = width - margin_right, margin_top
        # Ticks (thresholds)
        ticks = []
        for x in xs:
            px = x_to_px(x)
            ticks.append(f"<line x1='{px:.1f}' y1='{y0:.1f}' x2='{px:.1f}' y2='{y0+6:.1f}' stroke='#999' />")
            ticks.append(f"<text x='{px-10:.1f}' y='{y0+24:.1f}' font-size='12' fill='#333'>{x:.2f}</text>")
        # Gridline for max_y
        grid_y = y_to_px(max_y)
        svg = f"""
<svg xmlns='http://www.w3.org/2000/svg' width='{width}' height='{height}'>
  <rect x='0' y='0' width='{width}' height='{height}' fill='#fff' />
  <!-- Axes -->
  <line x1='{x0}' y1='{y0}' x2='{x1}' y2='{y0}' stroke='#000' />
  <line x1='{x0}' y1='{y0}' x2='{x0}' y2='{y1}' stroke='#000' />
  <!-- Labels -->
  <text x='{(width/2):.1f}' y='{margin_top-8:.1f}' font-size='16' text-anchor='middle' fill='#000'>Phase 15 Stability Curve (σ max vs Threshold)</text>
  <text x='{(margin_left + plot_w/2):.1f}' y='{height-20:.1f}' font-size='14' text-anchor='middle' fill='#333'>Risk Threshold</text>
  <text x='{18:.1f}' y='{(margin_top + plot_h/2):.1f}' font-size='14' transform='rotate(-90 18,{(margin_top + plot_h/2):.1f})' text-anchor='middle' fill='#333'>σ max (%)</text>
  <!-- Polyline -->
  <polyline points='{points}' fill='none' stroke='#1f77b4' stroke-width='2' />
  <!-- Points and value labels -->
  {''.join(circles)}
  <!-- X-axis ticks -->
  {''.join(ticks)}
</svg>
"""
        with open(svg_path, "w", encoding="utf-8") as fsvg:
            fsvg.write(svg)
        print(f"Wrote stability SVG to {svg_path}")
    else:
        print("No consolidated data available to render stability SVG.")

    # ----
    # Emit scatter: ΔAllow/ΔDeny vs σ max (sensitivity vs robustness)
    # ----
    scatter_path = os.path.join("Artifacts", "SVG", "Phase15_SlopeVsStability.svg")
    if enriched:
        # Use per-threshold points; slopes already normalized per 0.1 step
        width, height = 800, 400
        margin_left, margin_right, margin_top, margin_bottom = 60, 20, 30, 60
        plot_w = width - margin_left - margin_right
        plot_h = height - margin_top - margin_bottom
        xs = [e["stability_sigma"] for e in enriched]
        # Build two series for allow/deny slopes
        ys_allow = [e["dAllow_per_0.1"] for e in enriched]
        ys_deny = [e["dDeny_per_0.1"] for e in enriched]
        min_x, max_x = min(xs), max(xs)
        min_y = min(ys_allow + ys_deny + [0.0])
        max_y = max(ys_allow + ys_deny + [0.0])
        # Avoid zero ranges
        if max_x - min_x == 0:
            max_x += 1.0
        if max_y - min_y == 0:
            max_y = min_y + 1.0
        def x_to_px(x):
            return margin_left + (x - min_x) / (max_x - min_x) * plot_w
        def y_to_px(y):
            return margin_top + (1.0 - (y - min_y) / (max_y - min_y)) * plot_h
        # Points
        circles_allow = []
        circles_deny = []
        for e in enriched:
            x = e["stability_sigma"]
            ya = e["dAllow_per_0.1"]
            yd = e["dDeny_per_0.1"]
            cx, cya, cyd = x_to_px(x), y_to_px(ya), y_to_px(yd)
            circles_allow.append(f"<circle cx='{cx:.1f}' cy='{cya:.1f}' r='4' fill='#2ca02c' />")
            circles_deny.append(f"<circle cx='{cx:.1f}' cy='{cyd:.1f}' r='4' fill='#d62728' />")
        # Axes
        x0, y0 = margin_left, height - margin_bottom
        x1, y1 = width - margin_right, margin_top
        svg_scatter = f"""
<svg xmlns='http://www.w3.org/2000/svg' width='{width}' height='{height}'>
  <rect x='0' y='0' width='{width}' height='{height}' fill='#fff' />
  <line x1='{x0}' y1='{y0}' x2='{x1}' y2='{y0}' stroke='#000' />
  <line x1='{x0}' y1='{y0}' x2='{x0}' y2='{y1}' stroke='#000' />
  <text x='{(width/2):.1f}' y='{margin_top-8:.1f}' font-size='16' text-anchor='middle' fill='#000'>ΔAllow / ΔDeny vs σ max</text>
  <text x='{(margin_left + plot_w/2):.1f}' y='{height-20:.1f}' font-size='14' text-anchor='middle' fill='#333'>σ max (%)</text>
  <text x='{18:.1f}' y='{(margin_top + plot_h/2):.1f}' font-size='14' transform='rotate(-90 18,{(margin_top + plot_h/2):.1f})' text-anchor='middle' fill='#333'>Slope (per 0.1 threshold)</text>
  <!-- Allow series -->
  {''.join(circles_allow)}
  <!-- Deny series -->
  {''.join(circles_deny)}
  <!-- Legend -->
  <rect x='{width-190}' y='{margin_top+10}' width='170' height='50' fill='#fff' stroke='#ccc' />
  <circle cx='{width-175}' cy='{margin_top+25}' r='5' fill='#2ca02c' />
  <text x='{width-160}' y='{margin_top+29}' font-size='12' fill='#333'>ΔAllow per 0.1</text>
  <circle cx='{width-175}' cy='{margin_top+45}' r='5' fill='#d62728' />
  <text x='{width-160}' y='{margin_top+49}' font-size='12' fill='#333'>ΔDeny per 0.1</text>
</svg>
"""
        with open(scatter_path, "w", encoding="utf-8") as fsvg:
            fsvg.write(svg_scatter)
        print(f"Wrote slope-vs-stability SVG to {scatter_path}")
    else:
        print("No data available to render slope-vs-stability SVG.")

    # ----
    # Windowed σ over time: rolling std-dev of risk per file (window=100)
    # Emits aggregate CSV and an SVG overlay by threshold where available
    # ----
    window = 100
    time_csv = os.path.join("Artifacts", "CSV", "Phase15_StabilityOverTime.csv")
    os.makedirs(os.path.dirname(time_csv), exist_ok=True)
    with open(time_csv, "w", encoding="utf-8") as fcsv:
        fcsv.write("file,threshold,step_start,step_end,risk_sigma_window\n")
        for path in files:
            try:
                with open(path, "r", encoding="utf-8") as f:
                    data = json.load(f)
                logs = data.get("ethics_regulator_log", [])
                risks = [e.get("risk") for e in logs if isinstance(e.get("risk"), (int, float))]
                if len(risks) < window:
                    continue
                thr = extract_threshold(logs[0] if logs else {}, path)
                # rolling population std-dev
                for i in range(window, len(risks)+1):
                    seg = risks[i-window:i]
                    sigma = pstdev(seg)
                    fcsv.write(f"{os.path.basename(path)},{thr:.2f},{i-window},{i},{sigma:.6f}\n")
            except Exception:
                # Skip problematic files silently to avoid interrupting analysis
                pass
    print(f"Wrote windowed stability CSV to {time_csv}")

    # Simple SVG: plot first available file per threshold
    svg_time = os.path.join("Artifacts", "SVG", "Phase15_StabilityOverTime.svg")
    series_by_thr = {}
    try:
        with open(time_csv, "r", encoding="utf-8") as fcsv:
            header = next(fcsv)
            for line in fcsv:
                fname, thr_s, s0, s1, sigma_s = line.strip().split(",")
                thr = float(thr_s)
                s1_i = int(s1)
                sigma = float(sigma_s)
                if thr not in series_by_thr:
                    series_by_thr[thr] = []
                series_by_thr[thr].append((s1_i, sigma))
        # Choose one series per threshold (shortest for visual clarity)
        width, height = 900, 450
        margin_left, margin_right, margin_top, margin_bottom = 60, 20, 30, 60
        plot_w = width - margin_left - margin_right
        plot_h = height - margin_top - margin_bottom
        # Determine global ranges
        all_x = [x for series in series_by_thr.values() for (x, _) in series]
        all_y = [y for series in series_by_thr.values() for (_, y) in series]
        if all_x and all_y:
            min_x, max_x = min(all_x), max(all_x)
            min_y, max_y = 0.0, max(all_y + [0.0])
            if max_x - min_x == 0:
                max_x += 1
            if max_y - min_y == 0:
                max_y = 1.0
            def x_to_px(x):
                return margin_left + (x - min_x) / (max_x - min_x) * plot_w
            def y_to_px(y):
                return margin_top + (1.0 - (y - min_y) / (max_y - min_y)) * plot_h
            # Color palette
            colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b"]
            thr_list = sorted(series_by_thr.keys())
            paths = []
            legend = []
            for idx, thr in enumerate(thr_list):
                series = sorted(series_by_thr[thr], key=lambda t: t[0])
                color = colors[idx % len(colors)]
                pts = " ".join(f"{x_to_px(x):.1f},{y_to_px(y):.1f}" for (x, y) in series)
                paths.append(f"<polyline points='{pts}' fill='none' stroke='{color}' stroke-width='2' />")
                legend.append(f"<circle cx='{width-175}' cy='{margin_top+25+idx*20}' r='5' fill='{color}' />")
                legend.append(f"<text x='{width-160}' y='{margin_top+29+idx*20}' font-size='12' fill='#333'>thr={thr:.2f}</text>")
            svg = f"""
<svg xmlns='http://www.w3.org/2000/svg' width='{width}' height='{height}'>
  <rect x='0' y='0' width='{width}' height='{height}' fill='#fff' />
  <line x1='{margin_left}' y1='{height-margin_bottom}' x2='{width-margin_right}' y2='{height-margin_bottom}' stroke='#000' />
  <line x1='{margin_left}' y1='{height-margin_bottom}' x2='{margin_left}' y2='{margin_top}' stroke='#000' />
  <text x='{(width/2):.1f}' y='{margin_top-8:.1f}' font-size='16' text-anchor='middle' fill='#000'>Windowed Stability σ(t) (risk std-dev, window=100)</text>
  <text x='{(margin_left + plot_w/2):.1f}' y='{height-20:.1f}' font-size='14' text-anchor='middle' fill='#333'>Step</text>
  <text x='{18:.1f}' y='{(margin_top + plot_h/2):.1f}' font-size='14' transform='rotate(-90 18,{(margin_top + plot_h/2):.1f})' text-anchor='middle' fill='#333'>σ(risk)</text>
  {''.join(paths)}
  <rect x='{width-190}' y='{margin_top+10}' width='170' height='{30 + 20*len(thr_list)}' fill='#fff' stroke='#ccc' />
  {''.join(legend)}
</svg>
"""
            with open(svg_time, "w", encoding="utf-8") as fsvg:
                fsvg.write(svg)
            print(f"Wrote windowed stability SVG to {svg_time}")
        else:
            print("No windowed stability data available to render SVG.")
    except Exception:
        print("Windowed stability rendering skipped due to parse error.")

    # ----
    # Hysteresis inertia constant τ: fit exponential decay to distribution distance
    # Uses generated hysteresis JSON if present
    # ----
    hyst_path = os.path.join("web", "hysteresis_0.2_0.4_0.3.json")
    tau_csv = os.path.join("Artifacts", "CSV", "Phase15_HysteresisTau.csv")
    tau_svg = os.path.join("Artifacts", "SVG", "Phase15_HysteresisDecayFit.svg")
    def rate(entries, kind):
        return sum(1 for e in entries if e.get('decision') == kind) / len(entries)
    def fit_tau(dist_series):
        # Fit d(t) ~ A * exp(-t / tau) by linearizing ln(d) vs t, ignoring baseline C
        xs, ys = [], []
        for i, d in enumerate(dist_series):
            if d > 1e-9:
                xs.append(i)
                ys.append(math.log(d))
        if len(xs) < 5:
            return float('nan')
        # Least squares for y = a + b x; tau = -1/b
        n = len(xs)
        sx = sum(xs)
        sy = sum(ys)
        sxx = sum(x*x for x in xs)
        sxy = sum(x*y for x, y in zip(xs, ys))
        denom = n*sxx - sx*sx
        if denom == 0:
            return float('nan')
        b = (n*sxy - sx*sy) / denom
        if b >= 0:
            return float('nan')
        tau = -1.0 / b
        return tau
    try:
        with open(hyst_path, "r", encoding="utf-8") as f:
            data = json.load(f)["ethics_regulator_log"]
        segments = [(0, 1500, 0.2), (1500, 3000, 0.4), (3000, 4500, 0.3)]
        # Compute stable distribution per segment (last 300)
        stables = []
        for s, e, thr in segments:
            seg = data[s:e]
            stable_window = seg[-300:]
            stable = {k: rate(stable_window, k) for k in ("allow", "review", "deny")}
            stables.append(stable)
        # Compute distance to stable after each switch (L1 distance of decision rates over rolling window=50)
        window_w = 50
        dist_series = []  # concatenate per segment distances
        tau_rows = []
        for idx, (s, e, thr) in enumerate(segments):
            seg = data[s:e]
            stable = stables[idx]
            dists = []
            for i in range(window_w, len(seg)+1):
                w = seg[i-window_w:i]
                cur = {k: rate(w, k) for k in ("allow", "review", "deny")}
                # L1 distance to stable
                d = sum(abs(cur[k] - stable[k]) for k in ("allow", "review", "deny"))
                dists.append(d)
            tau = fit_tau(dists)
            tau_rows.append((thr, tau))
        # Write CSV
        os.makedirs(os.path.dirname(tau_csv), exist_ok=True)
        with open(tau_csv, "w", encoding="utf-8") as fcsv:
            fcsv.write("threshold,tau_steps\n")
            for thr, tau in tau_rows:
                fcsv.write(f"{thr:.2f},{'' if math.isnan(tau) else f'{tau:.2f}'}\n")
        print(f"Wrote hysteresis tau CSV to {tau_csv}")
        # Simple SVG: tau bars
        width, height = 600, 360
        margin_left, margin_right, margin_top, margin_bottom = 60, 20, 30, 60
        plot_w = width - margin_left - margin_right
        plot_h = height - margin_top - margin_bottom
        # x positions per threshold
        thrs = [thr for thr, _ in tau_rows]
        taus = [0.0 if math.isnan(t) else t for _, t in tau_rows]
        max_tau = max(taus + [0.0])
        if max_tau == 0:
            max_tau = 1.0
        def x_to_px_idx(i):
            return margin_left + (i+1) / (len(thrs)+1) * plot_w
        def h_to_px(t):
            return (t / max_tau) * plot_h
        bars = []
        labels = []
        for i, (thr, tau) in enumerate(tau_rows):
            cx = x_to_px_idx(i)
            bh = h_to_px(0.0 if math.isnan(tau) else tau)
            x = cx - 20
            y = height - margin_bottom - bh
            bars.append(f"<rect x='{x:.1f}' y='{y:.1f}' width='40' height='{bh:.1f}' fill='#1f77b4' />")
            labels.append(f"<text x='{cx-15:.1f}' y='{height-margin_bottom+20:.1f}' font-size='12' fill='#333'>{thr:.2f}</text>")
            labels.append(f"<text x='{cx-20:.1f}' y='{y-6:.1f}' font-size='12' fill='#333'>{'NaN' if math.isnan(tau) else f'{tau:.1f}'}</text>")
        svg = f"""
<svg xmlns='http://www.w3.org/2000/svg' width='{width}' height='{height}'>
  <rect x='0' y='0' width='{width}' height='{height}' fill='#fff' />
  <line x1='{margin_left}' y1='{height-margin_bottom}' x2='{width-margin_right}' y2='{height-margin_bottom}' stroke='#000' />
  <line x1='{margin_left}' y1='{height-margin_bottom}' x2='{margin_left}' y2='{margin_top}' stroke='#000' />
  <text x='{(width/2):.1f}' y='{margin_top-8:.1f}' font-size='16' text-anchor='middle' fill='#000'>Ethical Inertia τ (decay fit, window=50)</text>
  <text x='{(margin_left + plot_w/2):.1f}' y='{height-20:.1f}' font-size='14' text-anchor='middle' fill='#333'>Risk Threshold</text>
  <text x='{18:.1f}' y='{(margin_top + plot_h/2):.1f}' font-size='14' transform='rotate(-90 18,{(margin_top + plot_h/2):.1f})' text-anchor='middle' fill='#333'>τ (steps)</text>
  {''.join(bars)}
  {''.join(labels)}
</svg>
"""
        os.makedirs(os.path.dirname(tau_svg), exist_ok=True)
        with open(tau_svg, "w", encoding="utf-8") as fsvg:
            fsvg.write(svg)
        print(f"Wrote hysteresis tau SVG to {tau_svg}")
    except FileNotFoundError:
        print("Hysteresis log not found; τ fit skipped.")

if __name__ == "__main__":
    main()
