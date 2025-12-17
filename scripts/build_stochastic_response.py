import json
import math
import os
from glob import glob

OUT_SVG = "Artifacts/SVG/Phase15_StochasticResponse.svg"

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

def load_ethics_series(path):
    with open(path, "r", encoding="utf-8") as f:
        obj = json.load(f)
    logs = obj.get("ethics_regulator_log", [])
    ts = [rec.get("ts_ms") for rec in logs]
    risks = [rec.get("risk") for rec in logs]
    thr = None
    if logs:
        thr = logs[0].get("context", {}).get("threshold")
    return ts, risks, thr

def load_syslog(path):
    cpu = []
    ram = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            try:
                rec = json.loads(line)
                cpu.append((rec.get("elapsed"), rec.get("cpu_percent")))
                ram.append((rec.get("elapsed"), rec.get("mem_percent")))
            except Exception:
                pass
    return cpu, ram

def svg_line(points, color, width=2, opacity=1.0):
    d = []
    for i, (x, y) in enumerate(points):
        cmd = "M" if i == 0 else "L"
        d.append(f"{cmd}{x:.1f},{y:.1f}")
    return f"<path d='{' '.join(d)}' stroke='{color}' stroke-width='{width}' fill='none' opacity='{opacity}'/>"

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

def main():
    os.makedirs(os.path.dirname(OUT_SVG), exist_ok=True)

    # Gather noise-injected ethics files for threshold 0.3
    ethics_files = sorted(glob("web/ethics_0.3*_noise0.02.json"))
    series = []
    for p in ethics_files:
        ts, risks, thr = load_ethics_series(p)
        if not risks:
            continue
        series.append({"path": p, "thr": thr, "ts": ts, "risk": risks})

    # Telemetry
    cpu, ram = load_syslog("web/syslog_stoch.json")

    # Prepare canvas
    W, H = 1000, 600
    margin = 50
    plot_w = W - 2 * margin
    plot_h = (H - 3 * margin) // 2

    # Panel E: sigma(t) overlay
    # Normalize time to index spacing and sigma to pixel
    colors = ["#2b8a3e", "#3b5bdb", "#e8590c", "#ae3ec9"]
    sigma_panel_y0 = margin
    sigma_panel_y1 = sigma_panel_y0 + plot_h

    # Build sigma lines
    sigma_paths = []
    legend = []
    for idx, s in enumerate(series):
        sigma = rolling_std(s["risk"], window=100)
        if not sigma:
            continue
        max_sigma = max(sigma) or 1.0
        pts = []
        for i, val in enumerate(sigma):
            x = margin + (i / max(1, len(sigma)-1)) * plot_w
            # map sigma 0..max_sigma to panel vertical (invert Y)
            y = sigma_panel_y1 - (val / max_sigma) * plot_h
            pts.append((x, y))
        sigma_paths.append(svg_line(pts, colors[idx % len(colors)], width=2))
        legend.append(f"<text x='{margin+10}' y='{sigma_panel_y0+20+idx*16}' fill='{colors[idx % len(colors)]}' font-size='12'>σ(t) {os.path.basename(s['path'])}</text>")

    # CPU overlay scaled to panel
    cpu_path = ""
    ram_path = ""
    cpu_vals = []
    ram_vals = []
    if cpu:
        cpu_vals = [c[1] or 0.0 for c in cpu]
        max_cpu = max(cpu_vals) or 1.0
        pts = []
        for i, val in enumerate(cpu_vals):
            x = margin + (i / max(1, len(cpu_vals)-1)) * plot_w
            y = sigma_panel_y1 - (val / max_cpu) * plot_h
            pts.append((x, y))
        cpu_path = svg_line(pts, "#666", width=1, opacity=0.6)
    if ram:
        ram_vals = [r[1] or 0.0 for r in ram]
        max_ram = max(ram_vals) or 1.0
        pts_r = []
        for i, val in enumerate(ram_vals):
            x = margin + (i / max(1, len(ram_vals)-1)) * plot_w
            y = sigma_panel_y1 - (val / max_ram) * plot_h
            pts_r.append((x, y))
        ram_path = svg_line(pts_r, "#e67700", width=1, opacity=0.5)

    # Panel F: tau bars (read CSV if present)
    tau_bars = []
    tau_labels = []
    try:
        with open("Artifacts/CSV/Phase15_HysteresisTau.csv", "r", encoding="utf-8") as f:
            lines = [ln.strip() for ln in f if ln.strip()]
        header = lines[0].split(',')
        rows = [ln.split(',') for ln in lines[1:]]
        # Expect columns: threshold,tau,method
        tau_panel_y0 = sigma_panel_y1 + margin
        tau_panel_y1 = tau_panel_y0 + plot_h
        n = len(rows)
        for i, r in enumerate(rows):
            thr = r[0]
            tau = float(r[1]) if r[1] else 0.0
            x = margin + (i + 0.5) * (plot_w / max(1, n))
            bar_h = (tau / max(1e-6, max(float(xr[1]) if xr[1] else 0.0 for xr in rows))) * plot_h
            y = tau_panel_y1 - bar_h
            tau_bars.append(f"<rect x='{x-12:.1f}' y='{y:.1f}' width='24' height='{bar_h:.1f}' fill='{colors[i % len(colors)]}' opacity='0.8'/>")
            tau_labels.append(f"<text x='{x-16:.1f}' y='{tau_panel_y1+16:.1f}' font-size='12' fill='#333'>thr={thr}</text>")
    except Exception:
        tau_panel_y0 = sigma_panel_y1 + margin
        tau_panel_y1 = tau_panel_y0 + plot_h
        tau_labels.append(f"<text x='{margin}' y='{tau_panel_y0+20}' font-size='12' fill='#c00'>No tau CSV found</text>")

    svg = [
        f"<svg xmlns='http://www.w3.org/2000/svg' width='{W}' height='{H}'>",
        f"<rect x='0' y='0' width='{W}' height='{H}' fill='#fff'/>",
        f"<text x='{margin}' y='{sigma_panel_y0-10}' font-size='14' fill='#111'>Panel E: σmax(t) vs CPU% (noise σn=0.02)</text>",
        f"<rect x='{margin}' y='{sigma_panel_y0}' width='{plot_w}' height='{plot_h}' fill='none' stroke='#ddd'/>",
    ]
    svg += sigma_paths
    if cpu_path:
        svg.append(cpu_path)
        svg.append(f"<text x='{W-margin-320}' y='{sigma_panel_y0+18}' font-size='12' fill='#666'>CPU overlay</text>")
    if ram_path:
        svg.append(ram_path)
        svg.append(f"<text x='{W-margin-220}' y='{sigma_panel_y0+18}' font-size='12' fill='#e67700'>RAM overlay</text>")
    svg += legend

    # Compute and annotate correlation r(σ(t), CPU%) using first sigma series if available
    try:
        if series:
            sigma_first = rolling_std(series[0]["risk"], window=100)
            sigma_max = max(sigma_first) if sigma_first else 0.0
            cpu_r = pearson_r(sigma_first, cpu_vals) if cpu_vals else 0.0
            ram_r = pearson_r(sigma_first, ram_vals) if ram_vals else 0.0
            # Upper-right corner annotations
            svg.append(f"<text x='{W-margin-280}' y='{sigma_panel_y0+36}' font-size='12' fill='#111'>r(σ, CPU%)={cpu_r:.3f}</text>")
            svg.append(f"<text x='{W-margin-140}' y='{sigma_panel_y0+36}' font-size='12' fill='#111'>r(σ, RAM%)={ram_r:.3f}</text>")
            svg.append(f"<text x='{W-margin-140}' y='{sigma_panel_y0+54}' font-size='12' fill='#555'>σmax={sigma_max:.4f}</text>")
    except Exception:
        pass
    svg += [
        f"<text x='{margin}' y='{tau_panel_y0-10}' font-size='14' fill='#111'>Panel F: τ vs noise (current τ from hysteresis fit)</text>",
        f"<rect x='{margin}' y='{tau_panel_y0}' width='{plot_w}' height='{plot_h}' fill='none' stroke='#ddd'/>",
    ]
    svg += tau_bars
    svg += tau_labels
    svg.append("</svg>")

    with open(OUT_SVG, "w", encoding="utf-8") as f:
        f.write("\n".join(svg))
    print(f"Wrote {OUT_SVG}")

if __name__ == "__main__":
    main()
