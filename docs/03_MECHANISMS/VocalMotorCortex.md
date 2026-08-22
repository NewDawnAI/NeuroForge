# VocalMotorCortex Documentation

Voice as a **motor modality**, not language generation.

## Design Principle

> "Language describes the world. Voice acts on the world. Only actions produce sound."

## Architecture

```
WorldModelCortex                    LanguageExpressionCortex
     ↓ (latent state)                    ↓ (weak bias only)
┌─────────────────────────────────────────────────────────────┐
│              VocalMotorCortex                               │
│  Babbling → Prosody → Syllables → Proto-words               │
└─────────────────────────────────────────────────────────────┘
                    ↓
              VocalAction (raw acoustic frames)
                    ↓
              [Sound Wave]
                    ↓
              AuditoryCortex (perception)
                    ↓
              VocalFeedbackLoop
                    ↓
              Prediction Error → Motor Refinement
```

## Developmental Stages

| Stage | Description | Exploration |
|-------|-------------|-------------|
| `BABBLING` | Random motor exploration | 80% |
| `PROSODY` | Pitch contour learning | 40% |
| `SYLLABIC` | Rhythmic motor patterns | 20% |
| `PROTO_WORD` | Stable sound attractors | 10% |
| `LANGUAGE_BIASED` | Language modulates (doesn't control) | 10% |

## Key Differences from TTS

| TTS | VocalMotorCortex |
|-----|------------------|
| Text → Sound | Motor → Sound |
| No learning | Learns from feedback |
| No babbling | Starts with babbling |
| Language controls | Language only biases |
| One-shot | Continuous refinement |

## Usage

```cpp
#include "motor/VocalMotorCortex.h"
#include "motor/VocalFeedbackLoop.h"

using namespace NeuroForge::Motor;

// Create motor cortex
VocalMotorCortex motor;

// Create feedback loop
VocalFeedbackLoop feedback(motor);

// Set audio provider from AuditoryCortex
feedback.setAudioProvider([&auditory_cortex](uint64_t t) {
    return auditory_cortex.getSpectrum(t);
});

// Motor cycle (call at 20Hz)
VocalAction action = motor.generateAction();
// [action produces sound through audio system]
feedback.processFeedback();
```

## Files

- `include/motor/VocalAction.h` - Raw acoustic motor schema
- `include/motor/VocalMotorCortex.h` - Motor controller
- `include/motor/VocalFeedbackLoop.h` - Perception-action loop
