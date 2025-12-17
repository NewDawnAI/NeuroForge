# Biases Analysis

This document analyzes the "Biases" of the NeuroForge system—innate predispositions and hardcoded instincts that modulate brain activity. These components are located in `src/biases` and `include/biases`.

## 1. SurvivalBias

### Location
- `include/biases/SurvivalBias.h`
- `src/biases/SurvivalBias.cpp`

### Functional Description
A system that models risk, arousal, and avoidance. It monitors "Hazard" levels from external sensors or internal metabolic stress.

### Mechanism
- **Hazard Analysis**: Computes a risk score based on activation variance (incoherence) and external signals.
- **Modulation**: 
  - High Risk -> Increases "Arousal" and "Avoidance Drive".
  - Modulates global coherence, typically reducing it to break fixation on dangerous stimuli.

### Usage Example
```cpp
SurvivalBias::Config cfg;
cfg.hazard_threshold = 0.8f;
SurvivalBias bias(cfg);
auto metrics = bias.analyze(neuron_activations);
```

---

## 2. FaceDetectionBias

### Location
- `include/biases/FaceDetectionBias.h`
- `src/biases/FaceDetectionBias.cpp`

### Functional Description
A predisposition to attend to face-like patterns. This serves as a "social bootstrapping" mechanism, ensuring the agent pays attention to caregivers or humans early in development.

### Mechanism
- **Pattern Matching**: Uses a kernel or heuristic to detect basic facial geometry (two eyes, mouth).
- **Signal Injection**: Boosts activation in the Visual Cortex when a match is found.

---

## 3. NoveltyBias

### Location
- `include/biases/NoveltyBias.h`
- `src/biases/NoveltyBias.cpp`

### Functional Description
Encourages exploration by rewarding the detection of unseen states or patterns.

### Mechanism
- **Prediction Error**: Compares current sensory input with predicted input.
- **Reward**: High error (surprise) triggers a positive modulatory signal (simulated Dopamine), reinforcing the behavior that led to the novel state.

---

## 4. Other Biases
- **AttachmentBias**: Drives proximity-seeking behavior towards trusted entities.
- **SpatialNavigationBias**: Encourages mapping and movement.
- **SocialPerceptionBias**: Enhances sensitivity to social cues (voice intonation, gestures).
