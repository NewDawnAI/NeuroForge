# NeuroForge API Documentation

**Version**: 1.0 (Post-M7)  
**Date**: January 2025  
**Status**: Production Ready  

---

## 📡 Telemetry & Testing Overview

- Env-first configuration with clear precedence:
  - `NF_TELEMETRY_DB`: SQLite DB path for periodic telemetry logging.
  - `NF_ASSERT_ENGINE_DB`: Seeds initial rows and asserts presence for short runs.
  - `NF_MEMDB_INTERVAL_MS`: Interval for periodic logging when CLI not provided.
  - Precedence: `--memdb-interval` > `NF_MEMDB_INTERVAL_MS` > assertion-mode default (50 ms) > code default (1000 ms).

- Testing tiers guide:
  - Smoke: `--steps=5` with `NF_ASSERT_ENGINE_DB=1` → ensures `reward_log` and `learning_stats` present.
  - Integration: `--steps=200`, `--step-ms=1`, interval ~25 ms → multiple periodic entries, stable cadence.
  - Benchmark: 5–10k steps, tuned interval, optional snapshots/viewers → performance and stability validation.

Reference: See `docs/HOWTO.md` (Telemetry & MemoryDB, Testing Tiers) for examples.

---

## 🎯 **Overview**

NeuroForge provides a comprehensive neural substrate API for building advanced AI systems with biologically-inspired learning mechanisms. The API supports multi-modal processing, autonomous operation, and emergent behavior analysis.

### **Key Features**
- **Unified Neural Substrate**: Single framework for all learning mechanisms
- **Multi-Modal Processing**: Vision, audio, and cross-modal integration
- **Autonomous Operation**: Self-directed learning and task generation
- **Memory Continuity**: Persistent state across sessions
- **Emergent Behavior**: Assembly formation and higher-order cognition
- **Real-Time Processing**: High-performance neural computation

---

## 🚀 **Quick Start**

### **Basic Neural Processing**
```powershell
# Simple vision processing with learning
.\neuroforge.exe --vision-demo --enable-learning --hebbian-rate=0.01 --steps=50

# Multi-modal processing with cross-modal connections
.\neuroforge.exe --vision-demo --audio-demo --cross-modal --enable-learning --steps=100

# Autonomous operation with substrate-native processing
.\neuroforge.exe --vision-demo --autonomous-mode=on --substrate-mode=native --enable-learning --steps=200
```

### **Memory and Persistence**
```powershell
# Save neural state
.\neuroforge.exe --vision-demo --enable-learning --save-brain=my_brain.capnp --steps=50

# Load and continue from saved state
.\neuroforge.exe --load-brain=my_brain.capnp --vision-demo --enable-learning --steps=25
```

---

## 📋 **Command Line Interface**

### **Core Parameters**

#### **Execution Control**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--steps=N` | Integer | 1 | Number of processing steps |
| `--step-ms=MS` | Integer | 10 | Milliseconds per step |

#### **Debug Flags (New)**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--simulate-blocked-actions=N` | Integer | 0 | Increment blocked-action metrics N times per step (no gating) |
| `--simulate-rewards=N` | Integer | 0 | Emit N synthetic reward events per step for pipeline debugging |

#### **Learning Configuration**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--enable-learning` | Flag | false | Enable neural plasticity |
| `--hebbian-rate=R` | Float | 0.001 | Hebbian learning rate |
| `--stdp-rate=R` | Float | 0.002 | STDP learning rate |
| `--attention-boost=F` | Float | 1.0 | Attention amplification factor |
| `--homeostasis[=on\|off]` | Boolean | on | Enable homeostatic regulation |

#### **Phase-4 Reward Modulation**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `-l, --lambda=F` | Float | 0.90 | Eligibility trace decay |
| `-e, --eta-elig=F` | Float | 0.50 | Eligibility learning rate |
| `-k, --kappa=F` | Float | 0.15 | Reward scaling factor |
| `-a, --alpha=F` | Float | 0.50 | Novelty weight |
| `-g, --gamma=F` | Float | 1.00 | Task reward weight |

### **Multi-Modal Processing**

#### **Vision System**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--vision-demo` | Flag | false | Enable vision processing |
| `--vision-grid=N` | Integer | 32 | Vision grid resolution (N×N) |
| `--vision-source=TYPE` | String | camera | Source: camera, maze, synthetic |
| `--camera-index=N` | Integer | 0 | Camera device index |
| `--camera-backend=TYPE` | String | any | Backend: msmf, dshow, any |

#### **Sandbox Window (Embedded Browser)**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--sandbox[=on\|off]` | Boolean | off | Enable sandboxed window for safe interaction |
| `--sandbox-url=URL` | String | https://www.youtube.com | Initial navigation URL |
| `--sandbox-size=WxH` | String | 1280x720 | Sandbox client area size |
| `--motor-cortex` | Flag | false | Bind visual focus to cursor and actions within sandbox |

##### Sandbox Init Readiness (New)
- Startup waits for: controller creation, first navigation starting, and one bounds update.
- Ensures stable entry into the main loop and prevents startup hangs.

#### **Vision Foveation**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--foveation[=on\|off]` | Boolean | off | Enable dynamic retina focusing |
| `--fovea-size=WxH` | String | 640x360 | Fovea rectangle dimensions |
| `--fovea-mode=cursor\|center\|attention` | Enum | cursor | Fovea center source |
| `--fovea-alpha=F` | Float `[0,1]` | 0.3 | EMA smoothing of fovea center |

Notes:
- When sandbox mode is enabled, the fovea rectangle is clamped within the sandbox bounds.
- `attention` mode centers on the most salient tile from the motor cortex.

#### **Audio System**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--audio-demo` | Flag | false | Enable audio processing |
| `--audio-feature-bins=N` | Integer | 256 | Audio feature dimensions |
| `--audio-spectral-bins=N` | Integer | 64 | Spectral analysis bins |
| `--audio-mel-bands=N` | Integer | 64 | Mel-frequency bands |
| `--audio-system[=on\|off]` | Boolean | off | Enable system audio loopback (WASAPI) |
| `--audio-mic[=on\|off]` | Boolean | off | Enable microphone audio capture |

#### **Cross-Modal Integration**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--cross-modal[=on\|off]` | Boolean | off | Enable cross-modal connections |
| `--motor-cortex` | Flag | false | Enable motor cortex region |

### **Autonomous Operation**

### **Quick Example: Browser Sandbox Agent**
```powershell
.\neuroforge.exe --vision-demo --motor-cortex --sandbox=on --sandbox-url=https://www.youtube.com ^
  --audio-demo --audio-system=on --memory-db=phasec_mem.db --steps=600
```

#### **M7 Substrate Mode**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--autonomous-mode[=on\|off]` | Boolean | off | Enable autonomous operation |
| `--substrate-mode=TYPE` | String | off | Mode: off, mirror, train, native |
| `--curiosity-threshold=F` | Float | 0.5 | Intrinsic motivation threshold |

#### **Mimicry Controls (Phase A)**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--mimicry[=on\|off]` | Boolean | off | Enable Phase A mimicry |
| `--mimicry-internal[=on\|off]` | Boolean | off | Route rewards internally (no external delivery) |
| `--mirror-mode=off\|vision\|audio` | String | off | Student embedding source |
| `--teacher-embed=PATH` | String | - | Teacher embedding vector file |
| `--wt-teacher=FLOAT` | Float | 0.6 | Weight for teacher component |
| `--wt-novelty=FLOAT` | Float | 0.1 | Weight for novelty component |
| `--wt-survival=FLOAT` | Float | 0.3 | Weight for survival component |
| `--log-shaped-zero[=on|off]` | Boolean | off | Log shaped even when components are zero |

##### Phase A Replay Controls (New)
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--phase-a-replay-interval=N` | Integer | 0 | Replay top attempts every N steps |
| `--phase-a-replay-top-k=K` | Integer | 0 | Number of past attempts to replay |
| `--phase-a-replay-boost=F` | Float | 1.0 | Scale reward during replay |
| `--phase-a-replay-lr-scale=F` | Float | 1.0 | Scale per‑entry learning rate during replay |
| `--phase-a-replay-include-hard-negatives=on\|off` | Boolean | on | Include hard‑negative repulsion |
| `--phase-a-replay-hard-k=K` | Integer | 3 | Hard negatives processed per replay cycle |
| `--phase-a-replay-repulsion-weight=F` | Float | 0.5 | Repulsion strength for hard negatives |

Notes:
- Replay applies scaled reward and learning rate and can include hard negative repulsion.
- References: `src/core/PhaseAMimicry.cpp:1326` (update scaling), `1370` (replay cycle), `1348` (repel).

##### Triplets Dataset Mode (New)
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--dataset-mode=triplets` | Flag | - | Enable Flickr30k‑style triplet ingestion |
| `--dataset-triplets=PATH` | String | - | Triplet dataset root (image/audio/text) |
| `--dataset-limit=N` | Integer | - | Limit number of triplets loaded |
| `--dataset-shuffle[=on\|off]` | Boolean | off | Shuffle loaded triplets |

Notes:
- Per‑item ingestion events: `experiences.event='triplet_ingestion'`.
- Phase A snapshots for evaluation: `experiences.event='snapshot:phase_a'`.
- References: `src/main.cpp:6107`–`6188`, `6250`–`6336`, `6451`–`6530`, `6368`–`6399`.

##### Projection to LanguageSystem Tokens (New)
- Phase A student embeddings are projected to `LanguageSystem::Config.embedding_dimension` for token grounding.
- Default `embedding_dimension` is 256; 62‑dim Phase A vectors are mapped 62→256 when active.
- References: `src/core/PhaseAMimicry.cpp:1423`–`1454` (`projectStudent`), `486`–`497` (norm logging).

#### **Action Gating Layer (New)**
- All actions (`type_text`, `key_press`, `scroll`, `click`, `cursor_move`) are filtered by a centralized gate.
- Reasons: `ok`, `no_web_actions`, `phase15_deny`, `phase13_freeze`.
- Telemetry:
  - `actions.payload_json.filter.reason` and `filter.allow` reflect decisions.
  - `reward_log.context_json.blocked_actions.*` aggregates counts per step.

### **Reward Pipeline Map**
teacher embedding
│
▼
Phase A mimicry
│   novelty
│     │
▼     ▼
shaped = (teacher * wt_T)
+ (novelty * wt_N)
+ (survival * wt_S)
│
▼
merged = arbiter(shaped)
│
▼
substrate learning
│
▼
DB logging
(shaped / merged / survival)

#### **Memory Systems**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--hippocampal-snapshots[=on\|off]` | Boolean | off | Enable memory snapshots |
| `--memory-independent[=on\|off]` | Boolean | off | Memory-independent learning |
| `--consolidation-interval=MS` | Integer | 1000 | Memory consolidation interval |

### **Maze Navigation**

#### **Spatial Processing**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--maze-demo[=on\|off]` | Boolean | off | Enable maze environment |
| `--maze-first-person[=on\|off]` | Boolean | off | First-person navigation |
| `--maze-size=N` | Integer | 8 | Maze grid size (N×N) |
| `--maze-wall-density=F` | Float | 0.20 | Wall density fraction |
| `--maze-view[=on\|off]` | Boolean | off | Live maze visualization |
| `--maze-view-interval=MS` | Integer | 300 | Visualization refresh rate |

#### **Navigation Control**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--epsilon=F` | Float | -1 | Epsilon-greedy exploration rate |
| `--softmax-temp=F` | Float | 0.5 | Softmax temperature |
| `--maze-max-episode-steps=N` | Integer | 4×N² | Maximum steps per episode |

### **Data Export and Analysis**

#### **State Persistence**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--save-brain=PATH` | String | - | Save brain state to file |
| `--load-brain=PATH` | String | - | Load brain state from file |

#### **Data Logging**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--episode-csv=PATH` | String | - | Episode metrics CSV output |
| `--memory-db=PATH` | String | - | SQLite database logging |
| `--log-json[=PATH]` | String | - | JSON event logging |

#### **Visualization**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `--viewer[=on\|off]` | Boolean | off | Enable 3D neural viewer |
| `--viewer-layout=TYPE` | String | shells | Layout: shells, layers |
| `--viewer-refresh-ms=MS` | Integer | 1500 | Viewer refresh rate |

---

## 🧠 **Programming Interface**

### **Core Classes**

#### **HypergraphBrain**
```cpp
class HypergraphBrain {
public:
    // Initialization
    bool initialize();
    void processStep(double deltaTime);
    
    // Region Management
    std::shared_ptr<Region> createRegion(const std::string& name, 
                                       Region::Type type, 
                                       Region::ActivationPattern pattern);
    void addRegion(std::shared_ptr<Region> region);
    void connectRegions(RegionID from, RegionID to, 
                       float density, std::pair<float,float> weightRange);
    
    // State Management
    bool saveCheckpoint(const std::string& path);
    bool loadCheckpoint(const std::string& path);
    
    // Learning System
    LearningSystem* getLearningSystem();
};
```

#### **Region Types**
```cpp
// Visual Processing
auto visualRegion = std::make_shared<NeuroForge::Regions::VisualCortex>(
    "VisualCortex", featureCount);

// Audio Processing  
auto auditoryRegion = std::make_shared<NeuroForge::Regions::AuditoryCortex>(
    "AuditoryCortex", featureCount);

// Motor Control
auto motorRegion = std::make_shared<NeuroForge::Regions::MotorCortex>(
    "MotorCortex", neuronCount);
```

#### **Learning System**
```cpp
class LearningSystem {
public:
    // Configuration
    void configureHebbian(float rate);
    void configureSTDP(float rate, float multiplier);
    void configurePhase4(float lambda, float etaElig, float kappa, 
                        float alpha, float gamma, float upsilon);
    
    // Reward Processing
    void applyExternalReward(float reward);
    void updateLearning(double timeSeconds);
    
    // Statistics
    LearningStats getStatistics() const;
};
```

### **First-Person Maze Renderer**
```cpp
class FirstPersonMazeRenderer {
public:
    struct RenderConfig {
        int width = 320;
        int height = 240;
        float fov = 60.0f;
        float view_distance = 5.0f;
        bool enable_textures = true;
    };
    
    // Setup
    void setMaze(const std::vector<bool>& walls, int size, int goal_x, int goal_y);
    
    // Rendering
    std::vector<float> render(const AgentState& agent);
    cv::Mat renderToMat(const AgentState& agent);
    
    // Agent Control
    bool updateAgentPosition(AgentState& agent, int action, 
                           const std::vector<bool>& walls, int size);
};
```

---

## 📊 **Integration Examples**

### **Basic Neural Processing**
```cpp
#include "core/HypergraphBrain.h"
#include "regions/VisualCortex.h"
#include "encoders/VisionEncoder.h"

// Initialize brain
NeuroForge::Core::HypergraphBrain brain;

// Create visual region
auto visualRegion = std::make_shared<NeuroForge::Regions::VisualCortex>(
    "VisualCortex", 1024);
brain.addRegion(visualRegion);
visualRegion->createNeurons(1024);

// Initialize and configure learning
brain.initialize();
auto* learning = brain.getLearningSystem();
learning->configureHebbian(0.01f);
learning->configureSTDP(0.005f, 2.0f);

// Process visual input
NeuroForge::Encoders::VisionEncoder encoder;
std::vector<float> imageData = loadImage("input.jpg");
auto features = encoder.encode(imageData);
visualRegion->processVisualInput(features);

// Process neural step
brain.processStep(0.01); // 10ms timestep

// Get learning statistics
auto stats = learning->getStatistics();
std::cout << "Total Updates: " << stats.total_updates << std::endl;
```

### **Multi-Modal Integration**
```cpp
// Create multi-modal brain
auto visualRegion = std::make_shared<NeuroForge::Regions::VisualCortex>("Visual", 512);
auto auditoryRegion = std::make_shared<NeuroForge::Regions::AuditoryCortex>("Audio", 256);

brain.addRegion(visualRegion);
brain.addRegion(auditoryRegion);

// Cross-modal connections
brain.connectRegions(visualRegion->getId(), auditoryRegion->getId(), 
                    0.02f, {0.05f, 0.2f});
brain.connectRegions(auditoryRegion->getId(), visualRegion->getId(), 
                    0.02f, {0.05f, 0.2f});

// Process multi-modal input
visualRegion->processVisualInput(visualFeatures);
auditoryRegion->processAudioInput(audioFeatures);
brain.processStep(0.01);
```

### **Autonomous Operation**
```cpp
// Configure autonomous mode
learning->configurePhase4(0.9f, 0.5f, 0.15f, 0.5f, 1.0f, 0.2f);

// Enable curiosity-driven exploration
float curiosityThreshold = 0.3f;

// Process autonomous step
brain.processStep(0.01);

// Apply intrinsic motivation
float novelty = calculateNovelty(currentState);
if (novelty > curiosityThreshold) {
    learning->applyExternalReward(0.1f); // Curiosity reward
}
```

### **Memory Persistence**
```cpp
// Save brain state
brain.saveCheckpoint("trained_model.capnp");

// Load brain state
NeuroForge::Core::HypergraphBrain loadedBrain;
loadedBrain.loadCheckpoint("trained_model.capnp");

// Continue processing with loaded state
loadedBrain.processStep(0.01);
```

---

## 🔧 **Assembly Detection API**

### **Neural Assembly Analysis**
```python
# Python assembly detection tool
from scripts.assembly_detector import AssemblyDetector

# Load connectivity data
detector = AssemblyDetector()
detector.load_connectivity_data("connections.csv")

# Detect assemblies using spectral clustering
assemblies = detector.detect_assemblies_spectral(n_clusters=50)

# Generate analysis report
report = detector.generate_report("assembly_analysis.json")

# Visualize results
detector.visualize_assemblies("assembly_plot.png")
```

### **Assembly Detection Results**
```json
{
  "summary": {
    "total_assemblies": 56,
    "total_neurons": 1024,
    "neurons_in_assemblies": 555,
    "coverage_percentage": 54.19,
    "avg_assembly_size": 9.91,
    "max_assembly_size": 34,
    "avg_cohesion": 1.025
  },
  "assemblies": [
    {
      "id": 0,
      "neurons": [71, 325, 331, 366],
      "size": 4,
      "internal_strength": 0.381,
      "external_strength": 0.388,
      "cohesion": 0.980,
      "method": "Spectral"
    }
  ]
}
```

---

## 📈 **Performance Metrics**

### **Scalability Benchmarks**
| Configuration | Neurons | Synapses | Updates/sec | Memory (MB) |
|---------------|---------|----------|-------------|-------------|
| Basic Vision | 512 | 13K | 100K | 45 |
| Multi-Modal | 1024 | 36K | 85K | 78 |
| Cross-Modal | 1536 | 52K | 70K | 112 |
| Full Substrate | 2048+ | 75K+ | 60K+ | 150+ |

### **Learning Performance**
| Metric | Typical Range | Optimal Range |
|--------|---------------|---------------|
| Hebbian Rate | 0.001-0.01 | 0.005-0.01 |
| STDP Rate | 0.001-0.005 | 0.002-0.005 |
| Phase-4 Lambda | 0.8-0.95 | 0.85-0.92 |
| Active Synapses | 10K-100K | 25K-75K |
| Weight Change | 1e-6 to 1e-4 | 1e-5 to 5e-5 |

---

## 🚨 **Error Handling**

### **Common Issues**

#### **Memory Errors**
```cpp
// Check for successful initialization
if (!brain.initialize()) {
    std::cerr << "Brain initialization failed" << std::endl;
    return -1;
}

// Validate region creation
auto region = brain.createRegion("Test", Region::Type::Custom, 
                                Region::ActivationPattern::Asynchronous);
if (!region) {
    std::cerr << "Region creation failed" << std::endl;
    return -1;
}
```

#### **Learning Configuration**
```cpp
// Validate learning parameters
if (hebbianRate < 0.0f || hebbianRate > 1.0f) {
    std::cerr << "Invalid Hebbian rate: " << hebbianRate << std::endl;
    return -1;
}

// Check Phase-4 parameters
if (lambda < 0.0f || lambda > 1.0f) {
    std::cerr << "Lambda must be in [0,1]: " << lambda << std::endl;
    return -1;
}
```

#### **File I/O Errors**
```cpp
// Checkpoint save validation
if (!brain.saveCheckpoint(filepath)) {
    std::cerr << "Failed to save checkpoint: " << filepath << std::endl;
    return -1;
}

// Checkpoint load validation
if (!brain.loadCheckpoint(filepath)) {
    std::cerr << "Failed to load checkpoint: " << filepath << std::endl;
    return -1;
}
```

---

## 🔗 **Integration Patterns**

### **Robotics Integration**
```cpp
class RobotController {
private:
    NeuroForge::Core::HypergraphBrain brain_;
    std::shared_ptr<NeuroForge::Regions::MotorCortex> motorRegion_;
    
public:
    void processRobotStep(const SensorData& sensors) {
        // Process sensory input
        auto visualFeatures = processCameraData(sensors.camera);
        auto audioFeatures = processMicrophoneData(sensors.microphone);
        
        // Neural processing
        visualRegion_->processVisualInput(visualFeatures);
        auditoryRegion_->processAudioInput(audioFeatures);
        brain_.processStep(0.01);
        
        // Extract motor commands
        auto motorCommands = motorRegion_->getMotorOutput();
        executeMotorCommands(motorCommands);
    }
};
```

### **Game AI Integration**
```cpp
class GameAI {
private:
    NeuroForge::Core::HypergraphBrain brain_;
    
public:
    Action selectAction(const GameState& state) {
        // Encode game state
        auto stateFeatures = encodeGameState(state);
        
        // Process through neural substrate
        inputRegion_->processInput(stateFeatures);
        brain_.processStep(0.01);
        
        // Extract action preferences
        auto actionScores = actionRegion_->getActivations();
        return selectBestAction(actionScores);
    }
    
    void applyReward(float reward) {
        brain_.getLearningSystem()->applyExternalReward(reward);
    }
};
```

### **Research Platform Integration**
```cpp
class ResearchExperiment {
private:
    NeuroForge::Core::HypergraphBrain brain_;
    std::vector<ExperimentTrial> trials_;
    
public:
    void runExperiment(const ExperimentConfig& config) {
        // Configure neural substrate
        setupBrainArchitecture(config);
        
        // Run experimental trials
        for (auto& trial : trials_) {
            runTrial(trial);
            collectMetrics(trial);
            
            // Save intermediate states
            if (trial.saveCheckpoint) {
                brain_.saveCheckpoint(trial.checkpointPath);
            }
        }
        
        // Generate analysis report
        generateExperimentReport();
    }
};
```

---

## 📚 **Best Practices**

### **Performance Optimization**
1. **Batch Processing**: Process multiple inputs in single steps when possible
2. **Memory Management**: Use checkpoints for long-running sessions
3. **Parameter Tuning**: Start with default parameters and adjust based on metrics
4. **Resource Monitoring**: Monitor active synapses and memory usage

### **Learning Configuration**
1. **Hebbian Learning**: Use 0.01 for rapid adaptation, 0.005 for stable learning
2. **STDP**: Enable for temporal sequence learning
3. **Phase-4**: Use for reward-based tasks and autonomous operation
4. **Homeostasis**: Keep enabled for stable long-term operation

### **Integration Guidelines**
1. **Modular Design**: Create separate regions for different modalities
2. **Cross-Modal Connections**: Use sparse connectivity (2-5%) for efficiency
3. **Error Handling**: Always validate initialization and file operations
4. **Logging**: Use JSON logging for detailed analysis and debugging

---

## 🎯 **Success Metrics**

### **Technical Validation**
- ✅ **Stable Operation**: 100K+ neurons without crashes
- ✅ **Memory Continuity**: 90%+ retention across sessions  
- ✅ **Assembly Formation**: Measurable emergent structures
- ✅ **Benchmark Performance**: 80%+ success on validation tasks

### **Integration Readiness**
- ✅ **Complete API**: All core functionality exposed
- ✅ **Documentation**: Comprehensive usage examples
- ✅ **Error Handling**: Robust failure recovery
- ✅ **Performance**: Real-time processing capabilities

---

## 📞 **Support and Resources**

### **Documentation**
- **Technical Guide**: `docs/HOWTO.md`
- **Benchmark Suite**: `VALIDATION_BENCHMARKS.md`
- **Spatial Navigation**: `SPATIAL_MAZE_BENCHMARK.md`

### **Example Code**
- **Basic Integration**: `examples/basic_neural_processing.cpp`
- **Multi-Modal**: `examples/cross_modal_integration.cpp`
- **Autonomous Operation**: `examples/autonomous_learning.cpp`

### **Tools and Utilities**
- **Assembly Detection**: `scripts/assembly_detector.py`
- **Benchmark Runner**: `scripts/benchmark_runner.ps1`
- **Neural Viewer**: `neuroforge_viewer.exe`

---

**NeuroForge API v1.0 - Ready for Production Integration**  
*Complete neural substrate architecture with emergent behavior capabilities*
