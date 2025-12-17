#!/usr/bin/env python3
import sys, os, re, time, glob, subprocess
from typing import List, Tuple

ROOT = os.path.abspath(os.path.dirname(__file__) + os.sep + "..")
INDEX_PATH = os.path.join(ROOT, 'index.html')
PLOT_SCRIPT = os.path.join(ROOT, 'plot_synapses_svg.py')

CSV_PATTERNS = [
    os.path.join(ROOT, 'live_synapses*.csv'),
    os.path.join(ROOT, 'M3_Demos_*', 'live_synapses*.csv'),
]
SVG_PATTERNS = [
    os.path.join(ROOT, 'weights_live_synapses*.svg'),
    os.path.join(ROOT, 'M3_Demos_*', 'weights_live_synapses*.svg'),
]

CARD_TMPL = (
    '<div class="card">'
    '<header><h3>{title}</h3></header>'
    '<div class="meta"><span class="chip">SVG: {svg_name}</span><span class="chip">Group: {group}</span></div>'
    '<div class="svgwrap"><object type="image/svg+xml" data="{svg_rel}"></object></div>'
    '</div>'
)

DEMO_RE = re.compile(r'^live_synapses_demo(\d+)$')
SEED_RE = re.compile(r'^live_synapses_seed_(\d+)$')
BASE_FROM_SVG_RE = re.compile(r'^weights_(live_synapses.*)\.svg$', re.IGNORECASE)


def find_files(patterns: List[str]) -> List[str]:
    files: List[str] = []
    for pat in patterns:
        files.extend(glob.glob(pat))
    # normalize, unique, existing only
    seen = set()
    out = []
    for p in files:
        ap = os.path.abspath(p)
        if ap not in seen and os.path.isfile(ap):
            seen.add(ap)
            out.append(ap)
    return out


def run_plot_for_csvs(csv_paths: List[str]) -> None:
    if not csv_paths:
        return
    # Batch invoke plotter to let it manage naming one-by-one
    for csv in sorted(csv_paths):
        try:
            subprocess.run([sys.executable, PLOT_SCRIPT, csv], cwd=ROOT, check=False,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        except Exception:
            pass


def classify_key(base: str) -> Tuple[int, Tuple]:
    m = DEMO_RE.match(base)
    if m:
        return (0, (int(m.group(1)), base))  # demos by number
    m = SEED_RE.match(base)
    if m:
        return (1, (int(m.group(1)), base))  # seeds by number
    return (2, (base,))  # others alphabetical


def build_cards(svg_paths: List[str]) -> List[str]:
    cards = []
    for svg in svg_paths:
        name = os.path.basename(svg)
        m = BASE_FROM_SVG_RE.match(name)
        if not m:
            continue
        base = m.group(1)
        # derive group
        group = 'Other'
        if DEMO_RE.match(base):
            group = 'Demos'
        elif SEED_RE.match(base):
            group = 'Seeds'
        title = base
        svg_rel = os.path.relpath(svg, ROOT).replace('\\','/')
        cards.append((classify_key(base), CARD_TMPL.format(title=title, svg_name=name, group=group, svg_rel=svg_rel)))
    # sort and drop key
    cards.sort(key=lambda x: x[0])
    return [c for _, c in cards]


def replace_synapses_grid(html: str, cards_html: str) -> str:
    start_tag = '<section class="grid" id="synapses-grid">'
    end_tag = '</section>'
    start_idx = html.find(start_tag)
    if start_idx == -1:
        raise RuntimeError('synapses-grid section not found in index.html')
    # find the closing tag after start
    end_idx = html.find(end_tag, start_idx)
    if end_idx == -1:
        raise RuntimeError('closing </section> not found for synapses-grid')
    end_idx += len(end_tag)
    new_section = start_tag + '\n' + cards_html + '\n  ' + end_tag
    return html[:start_idx] + new_section + html[end_idx:]


def update_index_once() -> int:
    # 1) find CSVs and regenerate corresponding SVGs
    csvs = find_files(CSV_PATTERNS)
    run_plot_for_csvs(csvs)
    # 2) collect all SVGs and render cards
    svgs = find_files(SVG_PATTERNS)
    # dedupe by base name (prefer latest mtime)
    best: dict[str, str] = {}
    for p in svgs:
        name = os.path.basename(p)
        m = BASE_FROM_SVG_RE.match(name)
        if not m:
            continue
        base = m.group(1).lower()
        prev = best.get(base)
        if not prev or os.path.getmtime(p) >= os.path.getmtime(prev):
            best[base] = p
    ordered_cards = build_cards(list(best.values()))
    cards_html = '\n    '.join(ordered_cards)
    # 3) patch index.html
    with open(INDEX_PATH, 'r', encoding='utf-8') as f:
        html = f.read()
    new_html = replace_synapses_grid(html, cards_html)
    if new_html != html:
        with open(INDEX_PATH, 'w', encoding='utf-8') as f:
            f.write(new_html)
        return 1
    return 0


def main(argv: List[str]) -> int:
    import argparse
    ap = argparse.ArgumentParser(description='Regenerate SVGs from live_synapses CSVs and update index.html grid.')
    ap.add_argument('--watch', action='store_true', help='Watch for changes and update continuously')
    ap.add_argument('--interval', type=float, default=5.0, help='Polling interval in seconds (watch mode)')
    args = ap.parse_args(argv)

    if not os.path.isfile(INDEX_PATH):
        print(f'index.html not found at {INDEX_PATH}')
        return 2
    if not os.path.isfile(PLOT_SCRIPT):
        print(f'plot_synapses_svg.py not found at {PLOT_SCRIPT}')
        return 2

    if not args.watch:
        changed = update_index_once()
        print(f'Updated index.html: {"yes" if changed else "no changes"}')
        return 0

    # watch loop via polling mtimes
    def snapshot() -> List[Tuple[str, float]]:
        files = find_files(CSV_PATTERNS) + find_files(SVG_PATTERNS)
        return sorted((p, os.path.getmtime(p)) for p in files)

    prev = snapshot()
    print('Watching for CSV/SVG changes… (Ctrl+C to stop)')
    while True:
        try:
            time.sleep(args.interval)
            curr = snapshot()
            if curr != prev:
                print('Change detected — updating…')
                update_index_once()
                prev = curr
            else:
                # still update occasionally in case of missed events
                pass
        except KeyboardInterrupt:
            print('Stopped.')
            break
    return 0

if __name__ == '__main__':
    raise SystemExit(main(sys.argv[1:]))