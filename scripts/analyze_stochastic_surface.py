import json
import math
import os
import argparse
from glob import glob

OUT_SVG = os.path.join("Artifacts", "SVG", "Phase16_StabilitySurface.svg")

def percentile(vals, p):
    if not vals:
        return 0.0
    vs = sorted(vals)
    k = max(0, min(len(vs)-1, int(round(p/100.0 * (len(vs)-1)))))
    return vs[k]

def rolling_std(vals, window=100):
    out = []
    for i in range(len(vals)):
        j0 = max(0, i - window + 1)
        w = vals[j0:i+1]
        if not w:
            out.append(0.0)
            continue
        m = sum(w) / len(w)
        var = sum((x - m) ** 2 for x in w) / len(w)
        out.append(math.sqrt(var))
    return out

def estimate_tau_from_autocorr(vals, max_lag=200):
    if not vals:
        return 0.0
    m = sum(vals)/len(vals)
    x = [v - m for v in vals]
    var0 = sum(v*v for v in x)
    if var0 <= 0:
        return 0.0
    target = 1.0 / math.e
    for k in range(1, max_lag+1):
        num = sum(x[i]*x[i-k] for i in range(k, len(x)))
        den = var0
        ac_k = num/den if den != 0 else 0.0
        if ac_k <= target:
            return float(k)
    return float(max_lag)

def load_ethics_series(path):
    with open(path, "r", encoding="utf-8") as f:
        obj = json.load(f)
    logs = obj.get("ethics_regulator_log", [])
    risks = [rec.get("risk") for rec in logs if rec.get("risk") is not None]
    return risks

def parse_noise_rate(name):
    base = os.path.basename(name).replace(".json", "")
    # ethics_0.3_seed1_rate1.0_noise0.02
    tok = base.split("_")
    noise = None
    rate = None
    try:
        for t in tok:
            if t.startswith("noise"):
                noise = float(t.replace("noise", ""))
            if t.startswith("rate"):
                rate = float(t.replace("rate", ""))
    except Exception:
        pass
    return noise, rate

def color_scale(val, vmin, vmax):
    # Blue (low) -> Green (mid) -> Red (high)
    if vmax <= vmin:
        return "#cccccc"
    x = max(0.0, min(1.0, (val - vmin) / (vmax - vmin)))
    # Piecewise: 0..0.5 blue->green, 0.5..1 green->red
    if x < 0.5:
        t = x / 0.5
        r, g, b = int(0 * (1-t) + 30 * t), int(120 * (1-t) + 180 * t), int(200 * (1-t) + 80 * t)
    else:
        t = (x - 0.5) / 0.5
        r, g, b = int(30 * (1-t) + 220 * t), int(180 * (1-t) + 60 * t), int(80 * (1-t) + 50 * t)
    return f"#{r:02x}{g:02x}{b:02x}"

def main():
    parser = argparse.ArgumentParser(description="Phase 16b/16c: generate stability surface heatmaps")
    parser.add_argument("--auto-scale", action="store_true", help="Use robust percentile scaling (5–95%) for color normalization")
    args = parser.parse_args()
    os.makedirs(os.path.dirname(OUT_SVG), exist_ok=True)

    files = sorted(glob(os.path.join("web", "ethics_*_noise*.json")))
    # Aggregate per (noise, rate)
    grid = {}
    rates = set()
    noises = set()
    for p in files:
        noise, rate = parse_noise_rate(p)
        if noise is None or rate is None:
            continue
        noises.add(noise)
        rates.add(rate)
        risks = load_ethics_series(p)
        if not risks:
            continue
        sig = rolling_std(risks, window=100)
        sigma_max = max(sig) if sig else 0.0
        tau = estimate_tau_from_autocorr(risks)
        grid.setdefault((noise, rate), []).append((sigma_max, tau))

    noises = sorted(noises)
    rates = sorted(rates)

    # Average across seeds for each (noise, rate)
    sigma_max_avg = {(n, r): (sum(v for v, _ in vals)/len(vals)) if vals else 0.0
                     for (n, r), vals in grid.items()}
    tau_avg = {(n, r): (sum(t for _, t in vals)/len(vals)) if vals else 0.0
               for (n, r), vals in grid.items()}

    # Value ranges for color scale (robust if --auto-scale)
    sigma_vals = list(sigma_max_avg.values()) or [0.0]
    tau_vals = list(tau_avg.values()) or [0.0]
    if args.auto_scale:
        smin, smax = percentile(sigma_vals, 5), percentile(sigma_vals, 95)
        tmin, tmax = percentile(tau_vals, 5), percentile(tau_vals, 95)
    else:
        smin, smax = min(sigma_vals), max(sigma_vals)
        tmin, tmax = min(tau_vals), max(tau_vals)

    # SVG layout: two heatmaps side-by-side
    W, H = 1100, 600
    margin = 60
    plot_w = (W - 3*margin) // 2
    plot_h = H - 2*margin
    left_x = margin
    right_x = left_x + plot_w + margin
    top_y = margin

    svg = [
        f"<svg xmlns='http://www.w3.org/2000/svg' width='{W}' height='{H}'>",
        f"<rect x='0' y='0' width='{W}' height='{H}' fill='#fff'/>",
        f"<text x='{margin}' y='{top_y-20}' font-size='16' fill='#111'>Phase 16b: Stochastic Stability Surface (σn × rate)</text>",
        f"<text x='{left_x}' y='{top_y-4}' font-size='14' fill='#111'>σmax</text>",
        f"<rect x='{left_x}' y='{top_y}' width='{plot_w}' height='{plot_h}' fill='none' stroke='#ddd'/>",
        f"<text x='{right_x}' y='{top_y-4}' font-size='14' fill='#111'>τ</text>",
        f"<rect x='{right_x}' y='{top_y}' width='{plot_w}' height='{plot_h}' fill='none' stroke='#ddd'/>",
    ]

    # Draw axes labels
    for i, n in enumerate(noises):
        x_left = left_x + (i + 0.5) * (plot_w / max(1, len(noises)))
        x_right = right_x + (i + 0.5) * (plot_w / max(1, len(noises)))
        svg.append(f"<text x='{x_left-16:.1f}' y='{top_y-8}' font-size='12' fill='#333'>σn={n:.2f}</text>")
        svg.append(f"<text x='{x_right-16:.1f}' y='{top_y-8}' font-size='12' fill='#333'>σn={n:.2f}</text>")
    for j, r in enumerate(rates):
        y = top_y + (j + 0.5) * (plot_h / max(1, len(rates)))
        svg.append(f"<text x='{left_x-50}' y='{y+4:.1f}' font-size='12' fill='#333'>rate={r:.2f}</text>")
        svg.append(f"<text x='{right_x-50}' y='{y+4:.1f}' font-size='12' fill='#333'>rate={r:.2f}</text>")

    # Heatmap cells
    cell_w = plot_w / max(1, len(noises))
    cell_h = plot_h / max(1, len(rates))
    for j, r in enumerate(rates):
        for i, n in enumerate(noises):
            # σmax
            s_val = sigma_max_avg.get((n, r), 0.0)
            color_s = color_scale(s_val, smin, smax)
            x_s = left_x + i * cell_w
            y_s = top_y + j * cell_h
            svg.append(f"<rect x='{x_s:.1f}' y='{y_s:.1f}' width='{cell_w:.1f}' height='{cell_h:.1f}' fill='{color_s}' opacity='0.92' stroke='#eee'/>")
            # τ
            t_val = tau_avg.get((n, r), 0.0)
            color_t = color_scale(t_val, tmin, tmax)
            x_t = right_x + i * cell_w
            y_t = y_s
            svg.append(f"<rect x='{x_t:.1f}' y='{y_t:.1f}' width='{cell_w:.1f}' height='{cell_h:.1f}' fill='{color_t}' opacity='0.92' stroke='#eee'/>")

    # Legends
    # σmax legend
    lx = W - margin - 160
    ly = H - margin - 80
    svg.append(f"<text x='{lx}' y='{ly-10}' font-size='12' fill='#333'>σmax scale</text>")
    for k in range(20):
        v = smin + (k/19.0) * (smax - smin)
        cx = lx + k * 8
        svg.append(f"<rect x='{cx:.1f}' y='{ly}' width='8' height='12' fill='{color_scale(v, smin, smax)}' stroke='#ccc'/>")
    svg.append(f"<text x='{lx}' y='{ly+28}' font-size='11' fill='#555'>{smin:.4f}</text>")
    svg.append(f"<text x='{lx+160}' y='{ly+28}' font-size='11' fill='#555' text-anchor='end'>{smax:.4f}</text>")
    # τ legend
    ly2 = ly + 50
    svg.append(f"<text x='{lx}' y='{ly2-10}' font-size='12' fill='#333'>τ scale</text>")
    for k in range(20):
        v = tmin + (k/19.0) * (tmax - tmin)
        cx = lx + k * 8
        svg.append(f"<rect x='{cx:.1f}' y='{ly2}' width='8' height='12' fill='{color_scale(v, tmin, tmax)}' stroke='#ccc'/>")
    svg.append(f"<text x='{lx}' y='{ly2+28}' font-size='11' fill='#555'>{tmin:.2f}</text>")
    svg.append(f"<text x='{lx+160}' y='{ly2+28}' font-size='11' fill='#555' text-anchor='end'>{tmax:.2f}</text>")

    svg.append("</svg>")

    with open(OUT_SVG, "w", encoding="utf-8") as f:
        f.write("\n".join(svg))
    print(f"Wrote {OUT_SVG}")

if __name__ == "__main__":
    main()
