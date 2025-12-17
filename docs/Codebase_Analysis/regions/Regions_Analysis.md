# Regions Analysis

This document analyzes the specific brain region implementations in `src/regions` and `include/regions`. These classes extend the base `Region` class with domain-specific logic.

## 1. VisualCortex

### Location
- `include/regions/CorticalRegions.h`
- `src/regions/CorticalRegions.cpp`

### Functional Description
Handles visual processing with a hierarchical layer structure (V1, V2, V4, IT).

### Key Features
- **Layers**:
  - **V1**: Edge orientation and simple features.
  - **V2**: Complex shapes and patterns.
  - **V4**: Color and intermediate forms.
  - **IT**: Object recognition (Inferotemporal).
- **Attention**: Supports "Spotlight" attention (`visual_attention_focus_`).
- **Feature Detection**: Extracts features like Edges, Corners, Motion.

---

## 2. AuditoryCortex

### Location
- `include/regions/CorticalRegions.h`
- `src/regions/CorticalRegions.cpp`

### Functional Description
Handles sound processing using a tonotopic map (frequency-based organization).

### Key Features
- **Tonotopic Map**: Neurons are arranged by frequency sensitivity.
- **Areas**: A1 (Primary), A2 (Secondary), Planum Temporale (Language).
- **Analysis**: Extracts Pitch, Timbre, Rhythm.

---

## 3. MotorCortex

### Location
- `include/regions/CorticalRegions.h`
- `src/regions/CorticalRegions.cpp`

### Functional Description
Controls motor output. Organized somatotopically (Body Map / Homunculus).

### Key Features
- **Areas**: M1 (Primary), PMC (Premotor), SMA (Supplementary).
- **Somatotopy**: Maps neurons to specific body parts (Hands, Face, Legs).

---

## 4. LimbicRegions

### Location
- `include/regions/LimbicRegions.h`
- `src/regions/LimbicRegions.cpp`

### Functional Description
Manages emotion, memory, and homeostatic regulation.

### Components (Inferred)
- **Amygdala**: Threat detection, fear processing (Interacts with `SurvivalBias`).
- **Hippocampus**: Memory formation, spatial navigation, and replay (Interacts with `EpisodicMemoryManager`).
- **Thalamus**: Sensory relay station.

