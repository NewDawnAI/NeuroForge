# Viewer User and Developer Guide

Version: v0.15.0-rc1-12-gdf2999b5
Timestamp: 2025-12-08

## Features
- Load CSV snapshots and spikes; render neurons and synapses with weight/activation coloring
- Camera controls; layout modes; refresh/streaming

## Usage
- Build with `NEUROFORGE_WITH_VIEWER=ON` and GLFW/GLAD available
- Run: `neuroforge_viewer --snapshots path --spikes path`

## Data Feeds
- snapshots: neurons.csv, synapses.csv; spikes: spikes.csv

## Embedding
- Link `neuroforge_core`; use spike observer hooks to stream into viewer

## Troubleshooting
- Missing OpenGL libs; ensure `opengl32` on Windows
- No data: verify CSV paths and formats

