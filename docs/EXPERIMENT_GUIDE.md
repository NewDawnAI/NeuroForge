# Cross-Model Benchmarking & Analysis Guide

This guide describes how to compare NeuroForge substrate telemetry to Transformer models (LLMs/VLMs), compute RSA/CKA, and produce publication-ready artifacts.

## Tools
- Run substrate: `build\neuroforge.exe --unified-substrate=on ...`
- Export series: `tools\db_export.py` → `web\substrate_states.json`
- Benchmark harness: `scripts\benchmark_unified.py` (adaptive/fixed/scale/ablation)
- Analyzer: `tools\analyze.py` (plots, CSV summaries, RSA/CKA)
- Transformer extractor: `tools\extract_transformer.py` (text and CLIP vision)

## Quick Start
```powershell
# 1) Generate substrate series (or use harness outputs)
python tools\db_export.py --db build\phasec_mem.db --run latest --table substrate_states --out web\substrate_states.json

# 2) Extract Transformer embeddings
# Text (single layer)
python tools\extract_transformer.py --model bert-base-uncased --inputs data\text.txt --out emb_text.json --layer -1
# Text (all layers)
python tools\extract_transformer.py --model gpt2 --inputs data\text.txt --out emb_all_layers.json --all-layers
# Vision (CLIP)
python tools\extract_transformer.py --model openai/clip-vit-base-patch32 --inputs data\images.txt --out emb_clip.json --modality vision

# 3) Analyze and compare
python tools\analyze.py --series web\substrate_states.json --out-dir Artifacts --rsa --cka --transformer-json emb_all_layers.json
```

## Outputs
- Substrate plots: `Artifacts\PNG\analysis\<series>\{coherence,growth_velocity,assemblies}.png`
- RSA heatmaps: `Artifacts\PNG\analysis\rsa_layers\<series>\rsa_layers_heatmap.png`
- CKA curves: `Artifacts\PNG\analysis\cka_layers\<series>\cka_vs_layers.png`
- CSV summaries:
  - `Artifacts\CSV\analysis\analysis_summary.csv`
  - `Artifacts\CSV\analysis\rsa_layers_<series>.csv`
  - `Artifacts\CSV\analysis\cka_layers_<series>.csv`

## Notes
- Ensure learning is enabled (`--enable-learning`) with explicit rates for non-flat telemetry.
- Align sample counts between substrate and Transformer vectors; the analyzer trims to the minimum length.
- For CLIP, vision embeddings are pooled (`get_image_features`).
- For LLM/encoders, text embeddings are mean-pooled across tokens; `--all-layers` exports every hidden state index.

## Recommended Figures
- Coherence vs Step, Growth Velocity vs Step, Assemblies vs Step.
- RSA layer heatmap (substrate vs Transformer layers).
- CKA vs layers line chart.
- Probe accuracy vs layer (future extension). 

## Troubleshooting
- Flat curves → increase `--steps`, check `--hebbian-rate`/`--stdp-rate`, re-export series.
- Missing RSA/CKA → verify `--transformer-json` produced either `vectors` or `layers`.
- Large models → use CPU/GPU appropriately; start with small/medium models for quick turnaround.

## Phase A Exports (New)
```powershell
# Export Phase A rewards and embeddings after an ADS‑1/Phase A run
python tools\export_embeddings_rewards.py --db .\experiments\ADS1.db --out_dir .\exports\ADS1

# Optional PCA visualizations for teacher/student embeddings
python tools\export_embeddings_rewards.py --db .\experiments\ADS1.db --out_dir .\exports\ADS1 --pca_out .\exports\ADS1\PCA
```

Notes
- CSV files are written post‑run; they may be empty during an ongoing session.

## ADS‑2 Triplet‑Grounding Run & Evaluation (New)
```powershell
# 1) Kick off ADS‑2 training with triplets and snapshots
PowerShell -File .\scripts\ads2_run.ps1 -RunName ADS2_run01 -Dataset c:\Users\ashis\Desktop\NeuroForge\flickr30k_triplets -Limit 800

# 2) Evaluate triplet grounding and per‑teacher statistics
python tools\eval_triplet_grounding.py --db .\experiments\ADS2.db --cmd analyze --limit 800 --out .\exports\ADS2\grounding_metrics.json
```
- Ensure `--hippocampal-snapshots=on` during the run to emit `snapshot:phase_a` entries.
- Triplet ingestion events: `experiences.event='triplet_ingestion'` with per‑item payload.
- Script metrics: `recall@1`, `recall@5`, `similarity_mean`, `token_activation_stability`, `inter_sample_generalization`, and per‑teacher accuracy.
