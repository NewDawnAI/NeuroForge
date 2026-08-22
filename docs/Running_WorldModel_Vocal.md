# Running NeuroForge with WorldModelCortex + VocalMotorCortex

This guide shows how to run the complete NeuroForge system with:
- **WorldModelCortex** (JEPA-style predictive world model)
- **VocalMotorCortex** (voice as motor modality)
- All existing perceptual biases and learning systems

---

## Quick Start

### 1. Build the Project

```powershell
cd C:\path\to\NeuroForge

# Configure with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build all targets
cmake --build build --config Release --parallel
```

### 2. Run Autonomous Learning (Existing)

```powershell
# Internet learning with all phases enabled
.\build\neuroforge_learn.exe --db=phasec_mem.db --max-pages=10 --max-seconds=300
```

### 3. Run Main Simulation

```powershell
# Full substrate with learning and WorldModelCortex
.\build\neuroforge.exe --steps=2000 --step-ms=10 ^
  --substrate-mode=native --enable-learning ^
  --hebbian-rate=0.01 --stdp-rate=0.01 ^
  --phase7=on --phase8=on --phase9=on --phase10=on --phase11=on --phase15=on ^
  --memory-db=phasec_mem.db --log-json=on
```

---

## Integration Code Example

To use the new components in your own code:

```cpp
#include "core/HypergraphBrain.h"
#include "perception/world/WorldModelCortex.h"
#include "motor/VocalMotorCortex.h"
#include "motor/VocalFeedbackLoop.h"
#include "biases/NoveltyBias.h"

using namespace NeuroForge;

int main() {
    // ===== 1. Core Brain =====
    Core::HypergraphBrain brain;
    brain.initialize();

    // ===== 2. WorldModelCortex (JEPA) =====
    Perception::WorldModelCortex world_model;
    
    // Connect to NoveltyBias for curiosity
    Biases::NoveltyBias novelty_bias;
    world_model.setSurpriseCallback([&](float surprise, float error) {
        novelty_bias.updateFromWorldModel(error, surprise);
    });

    // ===== 3. VocalMotorCortex =====
    Motor::VocalMotorCortex vocal_motor;
    Motor::VocalFeedbackLoop vocal_feedback(vocal_motor);

    // Connect audio provider (from AuditoryCortex)
    vocal_feedback.setAudioProvider([](std::uint64_t t) {
        // Return current audio spectrum from microphone/auditory cortex
        return std::vector<float>(13, 0.0f);
    });

    // ===== 4. Main Processing Loop =====
    for (int step = 0; step < 2000; ++step) {
        // Get perceptual inputs
        std::vector<float> visual = getVisualFeatures();
        std::vector<float> motion = getMotionFeatures();
        std::vector<float> temporal = getTemporalContext();
        std::vector<float> social = getSocialFeatures();
        std::vector<float> spatial = getSpatialFeatures();
        std::vector<float> auditory = getAuditoryFeatures();
        std::vector<float> linguistic = getLinguisticEmbeddings();

        // WorldModelCortex: predict and learn
        auto world_state = world_model.processCycle(
            visual, motion, temporal, social, spatial, auditory, linguistic
        );

        // VocalMotorCortex: generate action and process feedback
        auto vocal_action = vocal_motor.generateAction();
        // [vocal_action → sound system → produces audio]
        vocal_feedback.processFeedback();

        // Brain step
        brain.processStep(0.01f);

        // Log prediction error (drives curiosity)
        float prediction_error = world_model.getAveragePredictionError();
        std::cout << "Step " << step 
                  << " | World Error: " << prediction_error
                  << " | Vocal Stage: " << static_cast<int>(vocal_motor.getStage())
                  << std::endl;
    }

    return 0;
}
```

---

## Architecture Flow

```
┌──────────────────────────────────────────────────────────────────────────┐
│                           PERCEPTUAL INPUT                               │
│  Camera → VisualCortex → ContrastEdgeBias/MotionBias/FaceDetectionBias   │
│  Microphone → AuditoryCortex → VoiceBias                                 │
│  Text → LanguageSystem → Token Embeddings                                │
└────────────────────────────────┬─────────────────────────────────────────┘
                                 ↓
┌──────────────────────────────────────────────────────────────────────────┐
│                         WorldModelCortex (JEPA)                          │
│  7 modalities → WorldEncoder → latent z_t → WorldPredictor → z_{t+1}     │
│  Prediction error → NoveltyBias → Curiosity                              │
└────────────────────────────────┬─────────────────────────────────────────┘
                                 ↓
┌──────────────────────────────────────────────────────────────────────────┐
│                          ACTION OUTPUT                                   │
│  WorldState → VocalMotorCortex → VocalAction → Sound                     │
│  Sound → AuditoryCortex → VocalFeedbackLoop → Motor Refinement           │
│                                                                          │
│  WorldState → LanguageExpressionCortex → Text (debug only)               │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## CLI Flags Reference

| Flag | Description |
|------|-------------|
| `--substrate-mode=native` | Enable neural substrate |
| `--enable-learning` | Enable Hebbian/STDP learning |
| `--hebbian-rate=0.01` | Hebbian learning rate |
| `--curiosity-threshold=0.3` | Curiosity trigger threshold |
| `--prediction-error-threshold=0.5` | Prediction error threshold |
| `--memory-db=<path>` | SQLite persistence |
| `--log-json=on` | JSON telemetry output |

---

## Developmental Stages

### WorldModelCortex
Learns world dynamics through prediction. High prediction error = surprise = curiosity.

### VocalMotorCortex
Developmental stages:
1. **BABBLING** - Random motor exploration (80% exploration)
2. **PROSODY** - Pitch contour learning
3. **SYLLABIC** - Rhythmic patterns
4. **PROTO_WORD** - Stable sound attractors
5. **LANGUAGE_BIASED** - Language modulates (≤30%)

Stage transitions happen automatically based on prediction error reduction.

---

## Monitoring

```powershell
# View live synapse weights
.\build\Release\neuroforge_viewer.exe --snapshot-file live_synapses.csv --refresh-ms 500

# Query memory database
sqlite3 phasec_mem.db "SELECT * FROM telemetry ORDER BY timestamp DESC LIMIT 10;"
```
