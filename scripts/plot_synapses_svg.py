#!/usr/bin/env python3
import sys, os, csv, glob, math

def read_weights(path):
    weights = []
    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        for line in f:
            line=line.strip()
            if not line:
                continue
            # Skip header-like lines
            if line.lower().startswith('pre') or 'weight' in line.lower():
                # best-effort parse as CSV header, skip
                continue
            # split by comma first, fallback to whitespace
            parts = [p for p in line.replace('\t',',').replace('  ',',').split(',') if p!='']
            if len(parts) < 3:
                parts = line.split()
            if len(parts) >= 3:
                try:
                    w = float(parts[2])
                    weights.append(w)
                except Exception:
                    # ignore unparsable lines
                    pass
    return weights

def generate_svg(weights, out_path, title):
    # Basic size
    W, H = 800, 400
    margin = 40
    plot_w = W - 2*margin
    plot_h = H - 2*margin
    if not weights:
        svg = f"""<svg xmlns='http://www.w3.org/2000/svg' width='{W}' height='{H}' viewBox='0 0 {W} {H}'>
  <rect x='0' y='0' width='{W}' height='{H}' fill='white'/>
  <text x='{W/2}' y='{H/2}' text-anchor='middle' fill='#444' font-family='Segoe UI,Arial' font-size='16'>No weights to display</text>
</svg>"""
        with open(out_path, 'w', encoding='utf-8') as f:
            f.write(svg)
        return
    ws = sorted(weights)
    wmin, wmax = min(ws), max(ws)
    # Avoid division by zero
    span = wmax - wmin if (wmax - wmin) != 0 else 1.0
    # Build polyline points for sorted weights (rank vs value)
    pts = []
    n = len(ws)
    for i, w in enumerate(ws):
        # x in [margin, margin+plot_w]
        x = margin if n==1 else margin + (i/(n-1))*plot_w
        # y: higher weight -> higher y visually? We'll invert to have higher weight up
        # SVG y grows downward, so y = margin + (1 - norm)*plot_h
        norm = (w - wmin)/span
        y = margin + (1.0 - norm)*plot_h
        pts.append(f"{x:.2f},{y:.2f}")
    poly = ' '.join(pts)
    # Axes
    x0, y0 = margin, margin + plot_h
    x1, y1 = margin + plot_w, margin
    # Ticks and labels (min/max)
    svg = [
        f"<svg xmlns='http://www.w3.org/2000/svg' width='{W}' height='{H}' viewBox='0 0 {W} {H}'>",
        f"  <rect x='0' y='0' width='{W}' height='{H}' fill='white'/>",
        f"  <g stroke='#333' stroke-width='1'>",
        f"    <line x1='{x0}' y1='{y0}' x2='{x1}' y2='{y0}'/>",  # x-axis
        f"    <line x1='{x0}' y1='{y0}' x2='{x0}' y2='{y1}'/>",  # y-axis
        f"  </g>",
        f"  <polyline fill='none' stroke='#007acc' stroke-width='1.5' points='{poly}'/>",
        f"  <text x='{W/2}' y='22' text-anchor='middle' fill='#111' font-family='Segoe UI,Arial' font-size='18'>{title}</text>",
        f"  <text x='{x0-6}' y='{y0+18}' text-anchor='end' fill='#444' font-family='Segoe UI,Arial' font-size='12'>0</text>",
        f"  <text x='{x1}' y='{y0+18}' text-anchor='middle' fill='#444' font-family='Segoe UI,Arial' font-size='12'>rank n={n}</text>",
        f"  <text x='{x0-8}' y='{y0}' text-anchor='end' fill='#444' font-family='Segoe UI,Arial' font-size='12'>{wmin:.3f}</text>",
        f"  <text x='{x0-8}' y='{y1+4}' text-anchor='end' fill='#444' font-family='Segoe UI,Arial' font-size='12'>{wmax:.3f}</text>",
        "</svg>"
    ]
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(svg))


def main(args):
    if args:
        files = []
        for a in args:
            files.extend(glob.glob(a))
    else:
        files = glob.glob('live_synapses_*.csv')
    if not files:
        print('No CSV files matched.')
        return 0
    for path in sorted(set(files)):
        try:
            ws = read_weights(path)
            base = os.path.basename(path)
            out = os.path.join(os.path.dirname(path), f"weights_{os.path.splitext(base)[0]}.svg")
            generate_svg(ws, out, f"Weight spectrum — {base}")
            print(f"Wrote {out} with {len(ws)} weights")
        except Exception as e:
            print(f"Error processing {path}: {e}")
    return 0

if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))