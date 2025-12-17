import json
import math
import os
from glob import glob

OUT_SVG = os.path.join("Artifacts", "SVG", "Phase16_StochasticScaling.svg")
OUT_COUPLING_SVG = os.path.join("Artifacts", "SVG", "Phase16_TemporalCoupling.svg")

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

def pearson_r(a, b):
    n = min(len(a), len(b))
    if n < 2:
        return 0.0
    a = a[:n]
    b = b[:n]
    ma = sum(a) / n
    mb = sum(b) / n
    va = sum((x - ma) ** 2 for x in a)
    vb = sum((y - mb) ** 2 for y in b)
    if va <= 0 or vb <= 0:
        return 0.0
    cov = sum((x - ma) * (y - mb) for x, y in zip(a, b))
    return cov / math.sqrt(va * vb)

def load_ethics_series(path):
    with open(path, "r", encoding="utf-8") as f:
        obj = json.load(f)
    logs = obj.get("ethics_regulator_log", [])
    risks = [rec.get("risk") for rec in logs if rec.get("risk") is not None]
    return risks

def load_syslog(path):
    cpu = []
    mem = []
    try:
        with open(path, "r", encoding="utf-8") as f:
            for ln in f:
                try:
                    rec = json.loads(ln)
                    cpu.append(rec.get("cpu_percent") or 0.0)
                    mem.append(rec.get("mem_percent") or 0.0)
                except Exception:
                    pass
    except Exception:
        pass
    return cpu, mem

def estimate_tau_from_autocorr(vals, max_lag=200):
    # Normalize
    if not vals:
        return 0.0
    m = sum(vals)/len(vals)
    x = [v - m for v in vals]
    var0 = sum(v*v for v in x)
    if var0 <= 0:
        return 0.0
    # autocorr at lag k
    ac = []
    for k in range(1, max_lag+1):
        num = sum(x[i]*x[i-k] for i in range(k, len(x)))
        den = var0
        ac_k = num/den if den != 0 else 0.0
        ac.append(ac_k)
    # tau ~ lag where ac falls below 1/e
    target = 1.0 / math.e
    for i, v in enumerate(ac, start=1):
        if v <= target:
            return float(i)
    return float(max_lag)

def rolling_tau(vals, window=200, max_lag=100):
    out = []
    n = len(vals)
    if n == 0:
        return out
    for i in range(n):
        j0 = max(0, i - window + 1)
        w = vals[j0:i+1]
        out.append(estimate_tau_from_autocorr(w, max_lag=max_lag))
    return out

def rolling_corr(a, b, window=200):
    out = []
    n = min(len(a), len(b))
    if n == 0:
        return out
    for i in range(n):
        j0 = max(0, i - window + 1)
        sa = a[j0:i+1]
        sb = b[j0:i+1]
        out.append(pearson_r(sa, sb))
    return out

def parse_noise_from_name(name):
    # expects ..._noise0.02.json or ..._noise0.10.json
    base = os.path.basename(name)
    try:
        s = base.split("_noise")[-1].replace(".json", "")
        return float(s)
    except Exception:
        return None

def svg_scatter(points, color="#2b8a3e", radius=3, opacity=0.9):
    return [f"<circle cx='{x:.1f}' cy='{y:.1f}' r='{radius}' fill='{color}' opacity='{opacity}'/>" for x, y in points]

def svg_line(points, color="#3b5bdb", width=2):
    d = []
    for i, (x, y) in enumerate(points):
        cmd = "M" if i == 0 else "L"
        d.append(f"{cmd}{x:.1f},{y:.1f}")
    return f"<path d='{' '.join(d)}' stroke='{color}' stroke-width='{width}' fill='none'/>"

def main():
    os.makedirs(os.path.dirname(OUT_SVG), exist_ok=True)

    # Discover noise-injected ethics files
    files = sorted(glob(os.path.join("web", "ethics_*_noise*.json")))
    groups = {}
    for p in files:
        sigma = parse_noise_from_name(p)
        if sigma is None:
            continue
        groups.setdefault(sigma, []).append(p)

    # Aggregate metrics per noise level
    noise_levels = sorted(groups.keys())
    sigma_max_by_noise = []
    tau_by_noise = []
    corr_cpu_by_noise = []
    corr_mem_by_noise = []

    for sigma in noise_levels:
        sigma_max_vals = []
        tau_vals = []
        r_cpu_vals = []
        r_mem_vals = []
        for p in groups[sigma]:
            risks = load_ethics_series(p)
            if not risks:
                continue
            sig = rolling_std(risks, window=100)
            sigma_max_vals.append(max(sig) if sig else 0.0)
            tau_vals.append(estimate_tau_from_autocorr(risks))
            # Attempt per-run telemetry pairing
            # Map ethics file to telemetry by seed/rate in name
            base = os.path.basename(p).replace(".json", "")
            # ethics_0.3_seed1_rate1.0_noise0.02 → syslog_stoch_seed1_rate1.0.json
            tokens = base.split("_")
            tele_name = None
            try:
                seed_tok = [t for t in tokens if t.startswith("seed")][0]
                rate_tok = [t for t in tokens if t.startswith("rate")][0]
                tele_name = f"web/syslog_stoch_{seed_tok}_{rate_tok}.json"
            except Exception:
                tele_name = "web/syslog_stoch.json"  # fallback
            cpu, mem = load_syslog(tele_name)
            if cpu:
                r_cpu_vals.append(pearson_r(sig, cpu))
            if mem:
                r_mem_vals.append(pearson_r(sig, mem))
        # Aggregate across runs
        if sigma_max_vals:
            sigma_max_by_noise.append((sigma, sum(sigma_max_vals)/len(sigma_max_vals)))
        else:
            sigma_max_by_noise.append((sigma, 0.0))
        if tau_vals:
            tau_by_noise.append((sigma, sum(tau_vals)/len(tau_vals)))
        else:
            tau_by_noise.append((sigma, 0.0))
        corr_cpu_by_noise.append((sigma, sum(r_cpu_vals)/len(r_cpu_vals) if r_cpu_vals else 0.0))
        corr_mem_by_noise.append((sigma, sum(r_mem_vals)/len(r_mem_vals) if r_mem_vals else 0.0))

    # Prepare SVG canvas
    W, H = 1000, 700
    margin = 50
    plot_w = W - 2*margin
    plot_h = (H - 4*margin) // 3

    # Panel G1: σmax vs σn (line)
    g1_y0 = margin
    g1_y1 = g1_y0 + plot_h
    max_sigma_max = max([v for _, v in sigma_max_by_noise] or [1.0]) or 1.0
    pts1 = []
    for i, (sigma, val) in enumerate(sigma_max_by_noise):
        x = margin + (i / max(1, len(sigma_max_by_noise)-1)) * plot_w
        y = g1_y1 - (val / max_sigma_max) * plot_h
        pts1.append((x, y))
    line1 = svg_line(pts1, color="#2b8a3e")

    # Panel G2: τ vs σn (bars)
    g2_y0 = g1_y1 + margin
    g2_y1 = g2_y0 + plot_h
    max_tau = max([v for _, v in tau_by_noise] or [1.0]) or 1.0
    bars2 = []
    labels2 = []
    for i, (sigma, val) in enumerate(tau_by_noise):
        x = margin + (i + 0.5) * (plot_w / max(1, len(tau_by_noise)))
        bar_h = (val / max_tau) * plot_h
        y = g2_y1 - bar_h
        bars2.append(f"<rect x='{x-12:.1f}' y='{y:.1f}' width='24' height='{bar_h:.1f}' fill='#3b5bdb' opacity='0.85'/>")
        labels2.append(f"<text x='{x-16:.1f}' y='{g2_y1+16:.1f}' font-size='12' fill='#333'>σn={sigma:.2f}</text>")

    # Panel G3: r(σ, CPU/RAM) scatter across σn
    g3_y0 = g2_y1 + margin
    g3_y1 = g3_y0 + plot_h
    # Map corr values -1..1 to 0..plot_h
    def map_corr(val):
        return g3_y1 - ((val + 1.0) / 2.0) * plot_h
    points_cpu = []
    points_mem = []
    for i, (sigma, r_cpu) in enumerate(corr_cpu_by_noise):
        x = margin + (i / max(1, len(corr_cpu_by_noise)-1)) * plot_w
        y = map_corr(r_cpu)
        points_cpu.append((x, y))
    for i, (sigma, r_mem) in enumerate(corr_mem_by_noise):
        x = margin + (i / max(1, len(corr_mem_by_noise)-1)) * plot_w
        y = map_corr(r_mem)
        points_mem.append((x, y))

    svg = [
        f"<svg xmlns='http://www.w3.org/2000/svg' width='{W}' height='{H}'>",
        f"<rect x='0' y='0' width='{W}' height='{H}' fill='#fff'/>",
        f"<text x='{margin}' y='{g1_y0-10}' font-size='14' fill='#111'>Panel G1: σmax vs σn</text>",
        f"<rect x='{margin}' y='{g1_y0}' width='{plot_w}' height='{plot_h}' fill='none' stroke='#ddd'/>",
        line1,
        f"<text x='{W-margin-240}' y='{g1_y0+18}' font-size='12' fill='#666'>max σmax={max_sigma_max:.4f}</text>",
        f"<text x='{margin}' y='{g2_y0-10}' font-size='14' fill='#111'>Panel G2: τ vs σn</text>",
        f"<rect x='{margin}' y='{g2_y0}' width='{plot_w}' height='{plot_h}' fill='none' stroke='#ddd'/>",
    ]
    svg += bars2
    svg += labels2
    svg += [
        f"<text x='{margin}' y='{g3_y0-10}' font-size='14' fill='#111'>Panel G3: r(σ, CPU/RAM) vs σn</text>",
        f"<rect x='{margin}' y='{g3_y0}' width='{plot_w}' height='{plot_h}' fill='none' stroke='#ddd'/>",
    ]
    svg += svg_scatter(points_cpu, color="#e8590c", radius=4)
    svg += svg_scatter(points_mem, color="#ae3ec9", radius=4, opacity=0.8)
    svg += [
        f"<text x='{W-margin-280}' y='{g3_y0+18}' font-size='12' fill='#e8590c'>CPU corr</text>",
        f"<text x='{W-margin-200}' y='{g3_y0+18}' font-size='12' fill='#ae3ec9'>RAM corr</text>",
        "</svg>"
    ]

    with open(OUT_SVG, "w", encoding="utf-8") as f:
        f.write("\n".join(svg))
    print(f"Wrote {OUT_SVG}")

    # Panel I: Rolling σ–τ coupling over time (overlay per σn)
    # For each noise level, average r_window(t) across available runs by alignment
    colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf"]
    W2, H2 = 1000, 400
    margin2 = 50
    plot_w2 = W2 - 2*margin2
    plot_h2 = H2 - 2*margin2
    svg2 = [
        f"<svg xmlns='http://www.w3.org/2000/svg' width='{W2}' height='{H2}'>",
        f"<rect x='0' y='0' width='{W2}' height='{H2}' fill='#fff'/>",
        f"<text x='{margin2}' y='{margin2-12}' font-size='14' fill='#111'>Panel I: Rolling σ–τ coupling vs time (per σn)</text>",
        f"<rect x='{margin2}' y='{margin2}' width='{plot_w2}' height='{plot_h2}' fill='none' stroke='#ddd'/>",
    ]

    # Build per-noise overlay lines
    # r_window(t) = corr(σ_local(t-window..t), τ_local(t-window..t))
    for idx, sigma in enumerate(noise_levels):
        runs = groups.get(sigma, [])
        r_series = []
        max_len = 0
        for p in runs:
            risks = load_ethics_series(p)
            if not risks:
                continue
            sig_local = rolling_std(risks, window=100)
            tau_local = rolling_tau(risks, window=200, max_lag=100)
            r_win = rolling_corr(sig_local, tau_local, window=200)
            r_series.append(r_win)
            max_len = max(max_len, len(r_win))
        if not r_series:
            continue
        # Pad/truncate to align lengths and average pointwise
        avg = []
        for i in range(max_len):
            vals = [s[i] for s in r_series if i < len(s)]
            avg.append(sum(vals)/len(vals) if vals else 0.0)
        # Map -1..1 to vertical space
        pts = []
        for i, val in enumerate(avg):
            x = margin2 + (i / max(1, len(avg)-1)) * plot_w2
            y = margin2 + (1.0 - (val + 1.0)/2.0) * plot_h2
            pts.append((x, y))
        color = colors[idx % len(colors)]
        # path
        d = []
        for j, (x, y) in enumerate(pts):
            d.append(("M" if j == 0 else "L") + f"{x:.1f},{y:.1f}")
        svg2.append(f"<path d='{' '.join(d)}' stroke='{color}' stroke-width='2' fill='none' opacity='0.9'/>")
        # legend label
        lx = W2 - margin2 - 140
        ly = margin2 + 18 + 16*idx
        svg2.append(f"<text x='{lx}' y='{ly}' font-size='12' fill='{color}'>σn={sigma:.2f}</text>")

    svg2.append("</svg>")
    os.makedirs(os.path.dirname(OUT_COUPLING_SVG), exist_ok=True)
    with open(OUT_COUPLING_SVG, "w", encoding="utf-8") as f:
        f.write("\n".join(svg2))
    print(f"Wrote {OUT_COUPLING_SVG}")

if __name__ == "__main__":
    main()
