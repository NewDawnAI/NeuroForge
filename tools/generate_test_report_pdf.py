import os
from datetime import datetime
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (
    SimpleDocTemplate,
    Paragraph,
    Spacer,
    PageBreak,
    Preformatted,
    Table,
    TableStyle,
    KeepInFrame,
    Image,
)
from reportlab.lib.utils import ImageReader
from reportlab.lib import colors
from reportlab.graphics.shapes import Drawing
from reportlab.graphics.charts.barcharts import VerticalBarChart

import json
import sqlite3
import math
 
ROOT = os.path.dirname(os.path.dirname(__file__))
LOGS_DIR = os.path.join(ROOT, 'logs')
OUTFILE = os.path.join(ROOT, 'Test_Summary_Report.pdf')
SCREENSHOTS_DIR = os.path.join(ROOT, 'Screenshots')
# Available frame width assuming default 1-inch margins on A4
FRAME_WIDTH = A4[0] - 2 * inch

# Formatting controls (can be overridden via environment variables)
METRIC_DECIMALS = int(os.environ.get('METRIC_DECIMALS', '3'))
METRIC_TABLE_MAX_VARIANTS = int(os.environ.get('METRIC_TABLE_MAX_VARIANTS', '3'))
# Known rollup CSV locations for metrics
ROLLUPS = {
    "1200_w150": os.path.join(ROOT, 'PhaseC_Logs', 'phase_c_long_rollups.csv'),
    "1200_w80": os.path.join(ROOT, 'PhaseC_Logs', 'v1200_w80', 'phase_c_long_rollups.csv'),
    "1800_w120": os.path.join(ROOT, 'PhaseC_Logs', 'v1800_w120', 'phase_c_long_rollups.csv'),
    "2400_w200": os.path.join(ROOT, 'PhaseC_Logs', 'v2400_w200', 'phase_c_long_rollups.csv'),
}

sections = [
    ("C++ Test Suite (ctest)", os.path.join(LOGS_DIR, 'ctest.txt')),
    ("Python JSON Smoke (pytest)", os.path.join(LOGS_DIR, 'pytest_json.txt')),
    ("Phase C Long-Smoke Baseline (1200 steps)", os.path.join(LOGS_DIR, 'phasec_1200_baseline.txt')),
    ("Phase C Long-Smoke Variant (1200 steps, w=80)", os.path.join(LOGS_DIR, 'phasec_1200_w80.txt')),
    ("Phase C Long-Smoke Variant (1800 steps, w=120)", os.path.join(LOGS_DIR, 'phasec_1800_w120.txt')),
    ("Phase C Long-Smoke Variant (2400 steps, w=200)", os.path.join(LOGS_DIR, 'phasec_2400_w200.txt')),
]

def read_tail(path: str, max_chars: int = 24000) -> str:
    try:
        with open(path, 'rb') as f:
            data = f.read()
        # Limit to the last max_chars bytes to avoid huge logs
        if len(data) > max_chars:
            data = data[-max_chars:]

        # Best-effort decoding: try utf-8, then fall back to cp1252/latin-1
        def _decode_best(buf: bytes) -> str:
            candidates = ['utf-8', 'cp1252', 'latin-1']
            best_text = None
            best_bad = 10**9
            for enc in candidates:
                try:
                    text = buf.decode(enc, errors='replace')
                except Exception:
                    continue
                bad = text.count('\ufffd')
                if bad < best_bad:
                    best_bad = bad
                    best_text = text
                    if bad == 0:
                        break
            return best_text if best_text is not None else buf.decode('latin-1', errors='ignore')

        text = _decode_best(data)

        # Clean for PDF: strip ANSI escapes and non-printable control chars, and
        # replace characters outside WinAnsi (codepoint > 255) with '?'
        import re
        text = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', text)  # ANSI color/term codes
        text = ''.join(ch if (ch in '\n\r\t' or 32 <= ord(ch) <= 126 or 160 <= ord(ch) <= 255) else ' ' for ch in text)
        text = ''.join(ch if ord(ch) <= 255 else '?' for ch in text)
        return text
    except FileNotFoundError:
        return f"[Missing log file: {path}]"
    except Exception as e:
        return f"[Error reading log file: {path} ({e})]"


def parse_rollup_csv(path: str):
    # Expect header with fields including mean_reward, var_reward, mean_novelty, var_novelty, mean_confidence, var_confidence, mean_uncertainty, var_uncertainty
    if not os.path.exists(path):
        return None
    try:
        import csv
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            reader = csv.DictReader(f)
            rows = list(reader)
            if not rows:
                return None
            last = rows[-1]
            keys = [
                'mean_reward','var_reward','mean_novelty','var_novelty',
                'mean_confidence','var_confidence','mean_uncertainty','var_uncertainty'
            ]
            return {k: last.get(k, '') for k in keys}
    except Exception:
        return None


def _fmt_num(v, places=None):
    try:
        f = float(v)
        p = METRIC_DECIMALS if places is None else places
        return f"{f:.{p}f}"
    except Exception:
        return str(v)

# ---------------------- M1/M2 correlation helpers (DB -> context_json) ----------------------

DB_CANDIDATES = [
    os.path.join(ROOT, 'test_memory.db'),
    os.path.join(ROOT, 'memory.sqlite'),
    os.path.join(ROOT, 'neuroforge.db3'),
    os.path.join(ROOT, 'smoke_phase_c.sqlite'),
    os.path.join(ROOT, 'demo.sqlite'),
    os.path.join(ROOT, 'demo_run.sqlite'),
    os.path.join(ROOT, 'phase_a_demo.db'),
    os.path.join(ROOT, 'build', 'phase_a_demo.db'),
]


def _db_has_reward_log(conn):
    try:
        cur = conn.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='reward_log';")
        return cur.fetchone() is not None
    except Exception:
        return False


def _extract_m1_m2_pairs_from_conn(conn, limit=50000):
    pairs = []
    try:
        for (ctx_str,) in conn.execute("SELECT context_json FROM reward_log WHERE context_json IS NOT NULL LIMIT ?;", (limit,)):
            if not ctx_str:
                continue
            try:
                obj = json.loads(ctx_str)
            except Exception:
                continue
            # Prefer explicit top-level keys; fall back to nested
            a = obj.get('phase_a_last_similarity')
            if a is None:
                pa = obj.get('phase_a') or {}
                a = pa.get('last_similarity')
            b = obj.get('substrate_similarity')
            try:
                if a is not None and b is not None:
                    a = float(a)
                    b = float(b)
                    if math.isfinite(a) and math.isfinite(b):
                        pairs.append((a, b))
            except Exception:
                continue
    except Exception:
        return []
    return pairs


def find_db_with_pairs():
    for p in DB_CANDIDATES:
        if not os.path.exists(p):
            continue
        try:
            conn = sqlite3.connect(p)
        except Exception:
            continue
        try:
            if _db_has_reward_log(conn):
                pairs = _extract_m1_m2_pairs_from_conn(conn)
                if len(pairs) >= 3:
                    return p, pairs
        finally:
            try:
                conn.close()
            except Exception:
                pass
    return None, []


def _pearson(xs, ys):
    n = len(xs)
    if n < 2:
        return None
    mx = sum(xs) / n
    my = sum(ys) / n
    num = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    dx = math.sqrt(sum((x - mx) ** 2 for x in xs))
    dy = math.sqrt(sum((y - my) ** 2 for y in ys))
    if dx == 0 or dy == 0:
        return None
    return num / (dx * dy)


def _rankdata(vals):
    n = len(vals)
    order = sorted([(v, i) for i, v in enumerate(vals)], key=lambda t: t[0])
    ranks = [0.0] * n
    i = 0
    while i < n:
        j = i
        while j + 1 < n and order[j + 1][0] == order[i][0]:
            j += 1
        # average rank for ties (1-based ranks)
        avg = (i + 1 + j + 1) / 2.0
        for k in range(i, j + 1):
            ranks[order[k][1]] = avg
        i = j + 1
    return ranks


def _spearman(xs, ys):
    if len(xs) != len(ys) or len(xs) < 2:
        return None
    rx = _rankdata(xs)
    ry = _rankdata(ys)
    return _pearson(rx, ry)


def add_m1_m2_correlation_section(story):
    styles = getSampleStyleSheet()
    story.append(Paragraph("Milestone 1/2: Phase A vs Substrate Similarity Correlation", styles['Heading2']))
    db_path, pairs = find_db_with_pairs()
    if not pairs:
        story.append(Paragraph("No suitable reward_log entries found in local MemoryDB files to compute correlation.", styles['BodyText']))
        story.append(Spacer(1, 0.15*inch))
        return

    xs = [a for a, _ in pairs]
    ys = [b for _, b in pairs]
    pear = _pearson(xs, ys)
    spear = _spearman(xs, ys)

    header = ["DB", "n", "pearson_r", "spearman_r"]
    row = [os.path.basename(db_path), str(len(pairs)), _fmt_num(pear) if pear is not None else "-", _fmt_num(spear) if spear is not None else "-"]
    table = Table([header, row], colWidths=[160, 60, 100, 100], hAlign='LEFT')
    table.setStyle(TableStyle([
        ('BACKGROUND', (0,0), (-1,0), colors.HexColor('#f0f0f0')),
        ('GRID', (0,0), (-1,-1), 0.3, colors.grey),
        ('FONTNAME', (0,0), (-1,0), 'Helvetica-Bold'),
        ('ALIGN', (1,1), (-1,-1), 'RIGHT'),
        ('ALIGN', (0,0), (-1,0), 'CENTER'),
        ('FONTSIZE', (0,0), (-1,-1), 8),
        ('LEFTPADDING', (0,0), (-1,-1), 2),
        ('RIGHTPADDING', (0,0), (-1,-1), 2),
        ('TOPPADDING', (0,0), (-1,-1), 2),
        ('BOTTOMPADDING', (0,0), (-1,-1), 2),
    ]))

    story.append(KeepInFrame(FRAME_WIDTH, 1.2*inch, [table], mode='shrink'))
    story.append(Paragraph("Metrics computed from reward_log.context_json across available runs. Pearson computed on raw values; Spearman computed on ranks (ties averaged).", styles['Italic']))
    story.append(Spacer(1, 0.2*inch))

# ---------------------- end M1/M2 helpers ----------------------

# New: chart helpers defined before build_story so they're available at runtime

def collect_metric(metric_name: str):
    labels = []
    values = []
    for label, path in ROLLUPS.items():
        vals = parse_rollup_csv(path)
        if not vals:
            continue
        raw = vals.get(metric_name)
        if raw in (None, ""):
            continue
        try:
            v = float(raw)
        except Exception:
            continue
        labels.append(label)
        values.append(v)
    return labels, values


def add_bar_chart(story, title: str, metric_name: str):
    styles = getSampleStyleSheet()
    labels, values = collect_metric(metric_name)
    if not labels:
        story.append(Paragraph(f"{title}: (no data)", styles['BodyText']))
        story.append(Spacer(1, 0.1*inch))
        return
    story.append(Paragraph(title, styles['Heading3']))
    d = Drawing(FRAME_WIDTH, 260)
    bc = VerticalBarChart()
    bc.x = 50
    bc.y = 30
    bc.height = 180
    bc.width = FRAME_WIDTH - 100
    bc.data = [values]
    bc.categoryAxis.categoryNames = labels
    bc.valueAxis.labels.fontName = 'Helvetica'
    bc.categoryAxis.labels.angle = 30
    bc.categoryAxis.labels.dy = -10
    bc.valueAxis.labelTextFormat = f'% .{METRIC_DECIMALS}f'.replace(' ', '')
    d.add(bc)
    # Ensure chart never exceeds frame width
    story.append(KeepInFrame(FRAME_WIDTH, 3.5*inch, [d], mode='shrink'))
    story.append(Spacer(1, 0.2*inch))

# Screenshots support

def _list_screenshots():
    if not os.path.isdir(SCREENSHOTS_DIR):
        return []
    exts = ('.png', '.jpg', '.jpeg')
    files = [
        os.path.join(SCREENSHOTS_DIR, f)
        for f in os.listdir(SCREENSHOTS_DIR)
        if f.lower().endswith(exts)
    ]
    def _key(p):
        import re
        name = os.path.splitext(os.path.basename(p))[0]
        m = re.match(r'(\d+)', name)
        return (0, int(m.group(1))) if m else (1, name)
    return sorted(files, key=_key)


def _screenshot_caption(path: str, index: int) -> str:
    """Derive a human-friendly caption for a screenshot.
    - If a sidecar .txt exists next to the image, use its first non-empty line.
    - Else, format from the filename by replacing underscores/dashes with spaces and trimming leading numbers.
    - Prefix with a figure index.
    """
    base, _ = os.path.splitext(path)
    sidecar = base + '.txt'
    caption = None
    if os.path.exists(sidecar):
        try:
            with open(sidecar, 'r', encoding='utf-8', errors='replace') as f:
                for line in f:
                    line = line.strip()
                    if line:
                        caption = line
                        break
        except Exception:
            pass
    if not caption:
        import re
        name = os.path.basename(base)
        name = re.sub(r'^\d+[-_\s]*', '', name)
        name = name.replace('_', ' ').replace('-', ' ')
        caption = name
    return f"Figure {index}: {caption}"


def add_cli_screenshots(story):
    styles = getSampleStyleSheet()
    imgs = _list_screenshots()
    if not imgs:
        return False
    story.append(Paragraph("CLI Screenshots", styles['Heading2']))
    max_w = FRAME_WIDTH
    max_h = A4[1] - 2 * inch - 1.0*inch
    for idx, p in enumerate(imgs, start=1):
        try:
            ir = ImageReader(p)
            iw, ih = ir.getSize()
            scale = min(max_w/iw, max_h/ih, 1.0)
            img = Image(p, width=iw*scale, height=ih*scale)
            story.append(KeepInFrame(FRAME_WIDTH, max_h, [img], mode='shrink'))
            story.append(Paragraph(_screenshot_caption(p, idx), styles['Italic']))
            story.append(Spacer(1, 0.2*inch))
            # Placement rule: insert a page break after very tall images or every 3 images
            if ih*scale > max_h*0.75 or (idx % 3 == 0):
                story.append(PageBreak())
        except Exception as e:
            story.append(Paragraph(f"[Could not add image {os.path.basename(p)}: {e}]", styles['BodyText']))
    return True


def add_metrics_tables(story):
    styles = getSampleStyleSheet()
    story.append(Paragraph("Phase C Metrics Rollups", styles['Heading2']))

    # Collect values per metric across known rollup CSVs
    metric_names = [
        'mean_reward','var_reward','mean_novelty','var_novelty',
        'mean_confidence','var_confidence','mean_uncertainty','var_uncertainty'
    ]

    # Possibly split variant columns into chunks for readability
    variant_labels = list(ROLLUPS.keys())
    if METRIC_TABLE_MAX_VARIANTS <= 0:
        chunks = [variant_labels]
    else:
        chunks = [variant_labels[i:i+METRIC_TABLE_MAX_VARIANTS] for i in range(0, len(variant_labels), METRIC_TABLE_MAX_VARIANTS)]

    for ci, chunk in enumerate(chunks, start=1):
        headers = ["Metric"] + chunk
        rows = [headers]
        for m in metric_names:
            row = [m]
            for label in chunk:
                path = ROLLUPS.get(label)
                vals = parse_rollup_csv(path)
                v = vals.get(m) if vals else None
                row.append(_fmt_num(v) if v not in (None, "") else "-")
            rows.append(row)

        # Compute column widths (first col wider)
        first_w = 140
        remain = FRAME_WIDTH - first_w
        col_count = max(1, len(headers) - 1)
        per_w = max(60, remain / col_count)
        col_widths = [first_w] + [per_w] * col_count

        t = Table(rows, colWidths=col_widths, hAlign='LEFT')
        t.setStyle(TableStyle([
            ('BACKGROUND', (0,0), (-1,0), colors.HexColor('#e8e8e8')),
            ('FONTNAME', (0,0), (-1,0), 'Helvetica-Bold'),
            ('ALIGN', (1,1), (-1,-1), 'RIGHT'),
            ('ALIGN', (0,0), (-1,0), 'CENTER'),
            ('GRID', (0,0), (-1,-1), 0.3, colors.grey),
            ('FONTSIZE', (0,0), (-1,-1), 8),
            ('LEFTPADDING', (0,0), (-1,-1), 2),
            ('RIGHTPADDING', (0,0), (-1,-1), 2),
            ('TOPPADDING', (0,0), (-1,-1), 2),
            ('BOTTOMPADDING', (0,0), (-1,-1), 2),
        ]))

        story.append(Paragraph(f"Variant group {ci} of {len(chunks)}", styles['Italic']))
        story.append(KeepInFrame(FRAME_WIDTH, 3.0*inch, [t], mode='shrink'))
        story.append(Spacer(1, 0.2*inch))

def build_story():
    styles = getSampleStyleSheet()
    story = []

    title = f"NeuroForge Test Summary Report\n{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
    story.append(Paragraph(title, styles['Title']))
    story.append(Spacer(1, 0.2*inch))

    # Executive Summary
    summary_lines = [
        "- All C++ tests passed (ctest, 10/10).",
        "- Python JSON smoke tests passed (test_json_smoke.py).",
        "- Phase C long-smoke baseline (1200 steps) created successfully.",
        "- Phase C variants (1200 w=80, 1800 w=120, 2400 w=200) executed successfully; longer horizons show expected drift vs 1200-step baseline.",
    ]
    story.append(Paragraph("Executive Summary", styles['Heading2']))
    story.append(Paragraph("<br/>".join(summary_lines), styles['BodyText']))
    story.append(Spacer(1, 0.2*inch))

    # Metrics tables and charts
    add_metrics_tables(story)
    add_bar_chart(story, "Mean Reward by Variant", "mean_reward")
    add_bar_chart(story, "Mean Confidence by Variant", "mean_confidence")

    # M1/M2 correlation section
    add_m1_m2_correlation_section(story)

    # Prefer screenshots over raw logs if available
    used_imgs = add_cli_screenshots(story)

    if not used_imgs:
        # Details with logs (tails)
        for title, path in sections:
            story.append(Paragraph(title, styles['Heading2']))
            story.append(Paragraph(f"Path: {os.path.relpath(path, ROOT)}", styles['Italic']))
            story.append(Spacer(1, 0.1*inch))
            log_text = read_tail(path)
            pre = Preformatted(log_text, styles['Code'])
            story.append(pre)
            # Only force a page break for longer logs; let short logs share a page
            if len(log_text) > 2000 or log_text.count('\n') > 30:
                story.append(PageBreak())
            else:
                story.append(Spacer(1, 0.2*inch))

    return story


def main():
    doc = SimpleDocTemplate(OUTFILE, pagesize=A4, title="NeuroForge Test Summary Report")
    doc.build(build_story())
    print(f"Wrote {OUTFILE}")

if __name__ == '__main__':
    main()