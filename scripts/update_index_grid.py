#!/usr/bin/env python3
import os, re

EXCLUDE_DIRS = {'.git', '__pycache__', 'build'}
INCLUDE_PREFIX = 'weights_live_synapses'


def collect_svgs(root='.'):
    svgs = []
    for dirpath, dirnames, filenames in os.walk(root):
        # prune excluded dirs in-place for efficiency
        dirnames[:] = [d for d in dirnames if d not in EXCLUDE_DIRS]
        for f in filenames:
            if f.startswith(INCLUDE_PREFIX) and f.endswith('.svg'):
                rel = os.path.relpath(os.path.join(dirpath, f), root)
                # use forward slashes for browser
                rel = rel.replace('\\', '/')
                svgs.append(rel)
    # stable sort by lowercase path
    return sorted(set(svgs), key=lambda p: p.lower())


def make_card(rel):
    name = os.path.basename(rel)
    title = os.path.splitext(name)[0]
    if title.startswith('weights_'):
        title = title[len('weights_'):]
    return (
        f"    <div class=\"card\">"
        f"<header><h3>{title}</h3></header>"
        f"<div class=\"meta\"><span class=\"chip\">SVG: {name}</span></div>"
        f"<div class=\"svgwrap\"><object type=\"image/svg+xml\" data=\"{rel}\"></object></div>"
        f"</div>"
    )


esscaped_grid_re = re.compile(r'(<section class=\"grid\">).*?(</section>)', re.S)

def update_index(index_path, svgs):
    with open(index_path, 'r', encoding='utf-8') as f:
        html = f.read()
    cards = [make_card(rel) for rel in svgs]
    new_grid = '<section class=\"grid\">\n' + '\n'.join(cards) + '\n  </section>'
    if esscaped_grid_re.search(html):
        updated = esscaped_grid_re.sub(new_grid, html, count=1)
    else:
        # fallback: append before </body>
        updated = html.replace('</body>', new_grid + '\n</body>')
    with open(index_path, 'w', encoding='utf-8') as f:
        f.write(updated)


def main():
    root = '.'
    index_path = os.path.join(root, 'index.html')
    if not os.path.exists(index_path):
        print('index.html not found next to script')
        return 1
    svgs = collect_svgs(root)
    update_index(index_path, svgs)
    print(f'Updated index.html with {len(svgs)} SVG entries')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())