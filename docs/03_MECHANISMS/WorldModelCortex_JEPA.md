# WorldModelCortex (JEPA-Style) Documentation

NeuroForge's predictive world model for learning **how the world changes** without language dependency.

## Overview

The WorldModelCortex implements a JEPA-style architecture that:
- Encodes multi-channel observation into a unified latent space
- Predicts future states via transition matrix
- Drives curiosity through prediction errors
- Enables reasoning without language
- Supports closed-loop attention (N5) and bounded regulation (N6) without actions

## Architecture

```
        ┌─────────────────────────────────────────────────────┐
        │               PERCEPTUAL BIASES                     │
        │  Visual  Motion  Temporal  Social  Spatial  Audio   │
        │                      └──┬──┘                        │
        │                 Linguistic  Semantic                │
        └──────────────────────┬──────────────────────────────┘
                               ↓
        ┌─────────────────────────────────────────────────────┐
        │              WorldEncoder                           │
        │  Fuses channels into latent z_t                      │
        └──────────────────────┬──────────────────────────────┘
                               ↓
        ┌─────────────────────────────────────────────────────┐
        │              WorldPredictor                         │
        │  Predicts z_{t+1} = W * z_t                         │
        └──────────────────────┬──────────────────────────────┘
                               ↓
        ┌─────────────────────────────────────────────────────┐
        │              PredictionError                        │
        │  L2 distance → NoveltyBias → Curiosity              │
        └─────────────────────────────────────────────────────┘
```

## Channels

WorldModelCortex consumes observation as channels:
- Visual
- Motion
- Temporal
- Social
- Spatial
- Auditory
- Linguistic
- Semantic (concept-projection channel used by N-series harnesses)

Channel weighting is implemented in the encoder configuration. Documentation should be treated as descriptive unless it is sourced directly from config defaults.

## Attention (N5)

WorldModelCortex maintains a transient per-cycle attention state that scales channel contributions as multiplicative gains before encoding.

Properties:
- weights are clamped to `[0.5, 1.5]`
- baseline is `1.0`
- weights decay back toward baseline quickly (short half-life)
- attention affects encoding only (never action, never memory writes)

The attention state is observable via `getAttentionState()`.

## Cognitive Regulation (N6)

WorldModelCortex optionally enables bounded internal regulation via `WorldModelCortexConfig::enable_cognitive_regulation`.

Regulation produces a per-cycle `RegulationState` that exposes:
- `attention_alpha` (bounded)
- `attention_lambda` (bounded)
- `learning_rate_multiplier` (bounded)

These are intended to modulate internal processing or gated learning loops in harnesses without introducing policy or goals.

## Usage

```cpp
#include "perception/world/WorldModelCortex.h"

using namespace NeuroForge::Perception;

// Create cortex with default config
WorldModelCortex cortex;

// Process a cycle with 7-modality input
WorldState z_t = cortex.processCycle(
    visual_features,
    motion_features,
    temporal_context,
    social_features,
    spatial_features,
    auditory_features,
    linguistic_embeddings, // optional, default = {}
    semantic_embeddings    // optional, default = {}
);

// Connect to NoveltyBias for curiosity
cortex.setSurpriseCallback([&novelty_bias](float surprise, float error) {
    novelty_bias.updateFromWorldModel(error, surprise);
});
```

## Integration Points

| Component | Integration |
|-----------|-------------|
| `NoveltyBias` | `updateFromWorldModel(error, uncertainty)` |
| `IntrinsicMotivationSystem` | Prediction error drives curiosity |
| `ConceptNode` | `predictive_power` updated from prediction accuracy |
| `LanguageSystem` | Token embeddings feed as linguistic modality |
| `SemanticProjection` | Concept activation projected into semantic channel (harnesses) |

## Key Design Decisions

1. **No gradient learning** — uses symbolic-hybrid transition matrix
2. **Language as percept** — treated like sound with structure, not logic
3. **Latent prediction** — JEPA-compliant, no pixel reconstruction
4. **Infant-like ordering** — perception precedes language binding
5. **Jurisdictional safety** — attention/regulation remain internal; action selection is not part of this module

## Files

- `include/perception/world/WorldState.h`
- `include/perception/world/WorldEncoder.h`
- `include/perception/world/WorldPredictor.h`
- `include/perception/world/WorldModelCortex.h`
