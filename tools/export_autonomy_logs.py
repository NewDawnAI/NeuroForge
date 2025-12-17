#!/usr/bin/env python3
import os, shutil, json

HERE = os.path.abspath(os.path.dirname(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
PAGES = os.path.join(ROOT, 'pages')
DEST = os.path.join(PAGES, 'autonomy')

CANDIDATE_DIRS = [
    ROOT,
    os.path.join(ROOT, 'build'),
    os.path.join(ROOT, 'build-vcpkg-vs', 'Release'),
]

FILE_NAMES = [
    'live_synapses.csv',
    'live_spikes.csv',
    'autonomy_synapses.csv',
    'autonomy_spikes.csv',
    'stats.csv',
    'stats.json',
]

os.makedirs(DEST, exist_ok=True)

copied = []
for name in FILE_NAMES:
    src = None
    for base in CANDIDATE_DIRS:
        candidate = os.path.join(base, name)
        if os.path.exists(candidate):
            src = candidate
            break
    if src:
        dst = os.path.join(DEST, name)
        shutil.copy2(src, dst)
        copied.append(name)
        print('Copied:', src, '->', dst)
    else:
        print('Missing (skip):', name)

index_path = os.path.join(DEST, 'autonomy_index.json')
with open(index_path, 'w', encoding='utf-8') as f:
    json.dump({'files': copied}, f, indent=2)
print('Wrote index:', index_path)