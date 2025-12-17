# NeuroForge Enhanced API Reference

## Overview

This document provides a comprehensive API reference for the enhanced NeuroForge system, including the breakthrough acoustic-first language learning capabilities, developmental tracking, and visualization tools.

## Core Language System API

### Class: `LanguageSystem`

#### Enhanced Constructor
```cpp
LanguageSystem(const Config& config = Config{});
```

**Parameters**:
- `config`: Enhanced configuration with prosodic sensitivity and cohesion parameters

**New Configuration Fields**:
```cpp
struct Config {
    // Enhanced acoustic processing
    float prosody_attention_weight = 0.4f;    // Increased sensitivity
    float intonation_threshold = 0.1f;        // Lowered threshold
    float motherese_boost = 0.4f;
    
    // Enhanced cohesion parameters
    float cross_modal_decay = 0.002f;         // Reduced decay
    float token_similarity_threshold = 0.3f;  // Easier clustering
    float cohesion_boost_factor = 2.0f;       // Stronger associations
    float co_occurrence_bonus = 0.02f;        // New parameter
};

## Phase A Mimicry API

### Class: `PhaseAMimicry`

#### Configuration
```cpp
struct PhaseAMimicry::Config {
    float similarity_weight = 0.7f;
    float novelty_weight = 0.3f;
    float similarity_threshold = 0.6f;
    float novelty_threshold = 0.1f;
    bool  enable_student_table = true;
    float student_learning_rate = 0.05f;
    bool  enable_ema_stabilizer = true;
    float ema_alpha_min = 0.02f;
    float ema_alpha_max = 0.2f;
    std::size_t replay_interval_steps = 100;
    std::size_t replay_top_k = 5;
};
```

References: `include/core/PhaseAMimicry.h:112–152`, `include/core/PhaseAMimicry.h:138–140`.

#### Runtime Integration
```cpp
void setBrain(NeuroForge::Core::HypergraphBrain* brain);
void setSubstrateMode(SubstrateMode mode);
void setRewardScale(float scale);
```

#### SubstrateMode
```cpp
enum class SubstrateMode { Off, Mirror, Train, Native };
```

#### Usage Example
```cpp
phase_a_system->setBrain(&brain);
NeuroForge::Core::PhaseAMimicry::SubstrateMode p_mode = NeuroForge::Core::PhaseAMimicry::SubstrateMode::Off;
if (substrate_mode == "mirror") p_mode = NeuroForge::Core::PhaseAMimicry::SubstrateMode::Mirror;
else if (substrate_mode == "train") p_mode = NeuroForge::Core::PhaseAMimicry::SubstrateMode::Train;
else if (substrate_mode == "native") p_mode = NeuroForge::Core::PhaseAMimicry::SubstrateMode::Native;
phase_a_system->setSubstrateMode(p_mode);
phase_a_system->setRewardScale(static_cast<float>(reward_scale));
```

#### Mimicry Methods
```cpp
MimicryAttempt attemptMimicry(const std::vector<float>& student_emb,
                              const std::string& teacher_id,
                              const std::string& context);

void applyMimicryReward(const MimicryAttempt& attempt);

std::string addTeacherEmbedding(const std::vector<float>& emb,
                                TeacherType teacher_type,
                                Modality modality,
                                const std::string& content_id,
                                const std::string& raw_content = "",
                                float confidence = 1.0f);

TeacherEmbedding* getTeacherEmbedding(const std::string& id);
```

**Notes**:
- When `mimicry_internal` is disabled, rewards are delivered via the unified brain and logged to MemoryDB (`reward_log`) with `source='phase_a'`.
- `context_json` includes Phase A fields: `modality`, `teacher_id`, `similarity`, `novelty`, `total_reward`, `shaped`, `success`.

References: `include/core/PhaseAMimicry.h:223–233`, `src/core/PhaseAMimicry.cpp:202–482`.

#### Replay Buffer Usage
```cpp
PhaseAMimicry::Config cfg = PhaseAMimicryFactory::createDefaultConfig();
cfg.enable_student_table = true;
cfg.enable_ema_stabilizer = true;
cfg.replay_interval_steps = 50;  // run replay every 50 attempts
cfg.replay_top_k = 10;           // reinforce top-10 attempts

auto language = std::make_shared<LanguageSystem>();
auto memdb = std::make_shared<MemoryDB>("phase_a.db");
auto phase_a = PhaseAMimicryFactory::create(language, memdb, cfg);
phase_a->initialize();

// Normal mimicry attempts; replay triggers automatically
auto attempt = phase_a->attemptMimicry(student_vec, teacher_id, "curriculum_step");
```

References: `src/core/PhaseAMimicry.cpp:460–465` (trigger), `src/core/PhaseAMimicry.cpp:1323` (implementation).

## Unified Reward Pipeline

### Overview
The system computes a shaped reward by combining teacher similarity, novelty, and survival contributions. The shaped reward influences substrate learning and is logged to MemoryDB alongside merged and survival sources.

### CLI Flags
```
--wt-teacher=FLOAT                  Weight for teacher component
--wt-novelty=FLOAT                  Weight for novelty component
--wt-survival=FLOAT                 Weight for survival component
--log-shaped-zero[=on|off]          Log shaped even when components are zero
```

### Shaped Reward Formula
```
shaped = (teacher * wt_T) + (novelty * wt_N) + (survival * wt_S)
```

### MemoryDB Logging
- `reward_log` includes rows with `source='shaped'`, `source='merged'`, and `source='survival'` when enabled.
- Learning metrics (e.g., `avg_weight_change`, `reward_updates`) appear in `learning_stats` at the telemetry cadence.

### Example Shaping Run (Windows PowerShell)
```powershell
& .\build\neuroforge.exe ^
  --vision-demo=on ^
  --phase-a=on ^
  --mimicry=on ^
  --mimicry-internal=off ^
  --mirror-mode=vision ^
  --teacher-embed=teacher_embed_256.txt ^
  --substrate-mode=native ^
  --phase-c-survival-bias=on ^
  --phase-c-survival-scale=0.8 ^
  --phase-c-hazard-weight=0.2 ^
  --hazard-density=0.5 ^
  --enable-learning ^
  --memdb-interval=20 ^
  --wt-teacher=0.6 ^
  --wt-novelty=0.2 ^
  --wt-survival=0.2 ^
  --log-shaped-zero=off ^
  --memory-db=phasec_mem.db ^
  --steps=2000
```

### Verify and Plot
```powershell
python .\scripts\inspect_phasec_memdb.py --db .\phasec_mem.db
python .\scripts\plot_reward_vs_weight_change.py ^
  --db .\phasec_mem.db ^
  --source shaped ^
  --align nearest ^
  --out-csv shaped_pairs.csv ^
  --out-png shaped_vs_weight.png
```

### Implementation References
- CLI flag parsing: `src/main.cpp:2422`
- Shaped computation: `src/main.cpp:6469`

### Version History
- 2025-12-09: Added Phase A replay buffer config and API notes; updated method signatures; verified against `include/core/PhaseAMimicry.h` and `src/core/PhaseAMimicry.cpp`.

## Action Gating Layer (New)

- All actions (`type_text`, `key_press`, `scroll`, `click`, `cursor_move`) flow through a centralized gate.
- Reasons: `ok`, `no_web_actions`, `phase15_deny`, `phase13_freeze`.
- Telemetry and DB:
  - `actions.payload_json.filter.reason` and `filter.allow` reflect decisions.
  - `reward_log.context_json.blocked_actions.*` aggregates per-step counts.
- Code references: include `core/ActionFilter.h` at `src/main.cpp:91`; calls at `src/main.cpp:6215, 6218, 6275, 6308, 6341`; implementation `src/core/ActionFilter.cpp:6`.

## Sandbox Init Readiness (New)

- Startup waits for WebView2 controller creation, first navigation starting, and one bounds update before entering the run loop.
- Ensures stable sandbox startup and prevents hangs in action-disabled runs.

## Dataset Ingestion CLI

### Triplets Mode
```text
--dataset-triplets=PATH                Root of triplet dataset (audio/text/images)
--dataset-mode=triplets               Enable triplet ingestion mode
--dataset-limit=N                     Limit number of triplets loaded
--dataset-shuffle[=on|off]            Shuffle loaded triplets (default: off)
--reward-scale=F                      Scale delivered reward (default: 1.0)
```

### Example Invocation (Windows PowerShell)
```powershell
& ".\neuroforge.exe" --phase7=on --phase9=on --substrate-mode=native \
  --dataset-mode=triplets --dataset-triplets="C:\Data\flickr30k_triplets" \
  --dataset-limit=1000 --dataset-shuffle=on --reward-scale=1.0 \
  --memory-db=phasec_mem.db --steps=2000 --log-json=on
```
```

### Acoustic Processing Methods

#### `extractAcousticFeatures()`
```cpp
AcousticFeatures extractAcousticFeatures(
    const std::vector<float>& audio_samples,
    float sample_rate = 16000.0f
) const;
```

**Enhanced Features**:
- **Improved pitch detection**: Realistic frequency filtering (50-500Hz)
- **Enhanced intonation slope**: Proper ∆pitch/∆time calculation
- **Debug logging**: Pitch trajectory analysis

**Returns**: `AcousticFeatures` with enhanced prosodic analysis

#### `calculateSoundSalience()`
```cpp
float calculateSoundSalience(const AcousticFeatures& features) const;
```

**Enhancements**:
- **Rising intonation boost**: Extra reward for prosodic patterns
- **Slope-based calculation**: Proper pitch change analysis
- **Debug logging**: Salience detection tracking

**Returns**: Enhanced salience score (0.0-1.0)

#### `processAcousticTeacherSignal()`
```cpp
void processAcousticTeacherSignal(
    const std::vector<float>& audio_data,
    const std::string& expected_token,
    float confidence = 1.0f
);
```

**Enhanced Processing**:
- Improved prosodic feature extraction
- Better caregiver signal detection
- Enhanced mimicry learning

### Developmental Tracking Methods

#### `enableTrajectoryTracking()`
```cpp
void enableTrajectoryTracking(const std::string& log_directory);
```

**Purpose**: Enable real-time developmental tracking
**Parameters**:
- `log_directory`: Directory for trajectory logs

**Features**:
- Automatic directory creation
- Real-time snapshot capture
- Milestone detection

#### `captureTrajectorySnapshot()`
```cpp
void captureTrajectorySnapshot();
```

**Purpose**: Capture current developmental state
**Features**:
- Token activation recording
- Stage progression tracking
- Association strength measurement

#### `generateDevelopmentalReport()`
```cpp
void generateDevelopmentalReport();
```

**Purpose**: Generate comprehensive developmental analysis
**Output**:
- Current stage assessment
- Milestone achievements
- Next developmental steps
- Progress metrics

### Enhanced Statistics

#### `getStatistics()`
```cpp
Statistics getStatistics() const;
```

**Enhanced Statistics Structure**:
```cpp
struct Statistics {
    // Core metrics
    std::size_t active_vocabulary_size;
    std::size_t total_tokens_generated;
    std::size_t successful_mimicry_attempts;
    std::size_t grounding_associations_formed;
    float average_token_activation;
    float vocabulary_diversity;
    
    // Enhanced metrics
    float prosodic_sensitivity_score;
    float cohesion_improvement_rate;
    float cross_modal_binding_strength;
    std::size_t proto_words_detected;
    float developmental_progress_percentage;
};
```

### Token Management API

#### Enhanced Token Structure
```cpp
struct SymbolicToken {
    std::string symbol;
    std::vector<float> embedding;
    float activation_strength;
    std::size_t usage_count;
    
    // Enhanced fields
    float cluster_stability;
    float cross_modal_strength;
    std::vector<std::string> associated_tokens;
    AcousticFeatures acoustic_profile;
    std::chrono::milliseconds last_activation;
};
```

#### `getActiveVocabulary()`
```cpp
std::vector<SymbolicToken> getActiveVocabulary(float threshold = 0.1f) const;
```

**Enhancements**:
- Lower default threshold for better token capture
- Enhanced token metadata
- Improved filtering algorithms

#### `findSimilarTokens()`
```cpp
std::vector<std::size_t> findSimilarTokens(
    const std::vector<float>& embedding,
    float threshold = 0.3f  // Lowered from 0.5f
) const;
```

**Enhancements**:
- Improved similarity calculation
- Better clustering algorithms
- Enhanced threshold management

## Developmental Simulation API

### Class: `DevelopmentalTrackingDemo`

#### Constructor
```cpp
DevelopmentalTrackingDemo(const std::string& log_directory = "developmental_demo_logs");
```

**Features**:
- Optimized configuration for faster development
- Enhanced trajectory tracking
- Comprehensive logging

#### `runDevelopmentalSimulation()`
```cpp
void runDevelopmentalSimulation(int steps = 150);
```

**Parameters**:
- `steps`: Number of developmental steps to simulate

**Simulation Stages**:
1. **Chaos Stage**: Random acoustic exploration
2. **Babbling Stage**: Proto-phoneme formation  
3. **Mimicry Stage**: Caregiver response patterns
4. **Grounding Stage**: Cross-modal associations

**Features**:
- Real-time progress reporting
- Automatic snapshot capture
- Stage-appropriate activities

#### Stage-Specific Methods

##### `simulateChaosStage()`
```cpp
void simulateChaosStage(int step);
```

**Activities**:
- Random acoustic babbling
- Occasional teacher signals
- Exploration of sound space

##### `simulateBabblingStage()`
```cpp
void simulateBabblingStage(int step);
```

**Activities**:
- Structured proto-phoneme generation
- Caregiver-like interactions
- Motherese pattern recognition

##### `simulateMimicryStage()`
```cpp
void simulateMimicryStage(int step);
```

**Activities**:
- Consistent caregiver mimicry
- Proto-word formation
- Social interaction patterns

##### `simulateGroundingStage()`
```cpp
void simulateGroundingStage(int step);
```

**Activities**:
- Visual-linguistic integration
- Joint attention events
- Object-word associations

## Visualization API

### Class: `SimpleVisualizer` (Python)

#### Constructor
```python
def __init__(self, log_dir='trajectory_logs'):
```

**Parameters**:
- `log_dir`: Directory containing trajectory data

#### `load_trajectory_data()`
```python
def load_trajectory_data(log_dir):
    """Load trajectory data from CSV files."""
    return trajectory_data, cluster_data
```

**Returns**:
- `trajectory_data`: List of token development records
- `cluster_data`: List of cluster formation records

#### `generate_text_report()`
```python
def generate_text_report(trajectory_data, cluster_data, output_file):
    """Generate comprehensive text-based developmental report."""
```

**Features**:
- Token development analysis
- Cluster formation tracking
- Milestone achievement detection
- Research-ready summaries

#### `generate_simple_charts()`
```python
def generate_simple_charts(trajectory_data, cluster_data, output_dir):
    """Generate ASCII-based developmental charts."""
```

**Chart Types**:
- Token activation timelines
- Cluster formation progression
- Developmental milestone markers
- Association strength visualization

## Test Suite API

### Class: `AcousticLanguageTestSuite`

#### Enhanced Test Methods

##### `testProsodicSalienceDetection()`
```cpp
bool testProsodicSalienceDetection();
```

**Enhancements**:
- Improved pitch generation algorithms
- Better salience comparison logic
- Debug logging for analysis

**Expected Results**:
- Rising intonation salience > Flat audio salience
- Proper pitch slope detection
- Realistic frequency analysis

##### `testCohesionImprovement()`
```cpp
bool testCohesionImprovement();
```

**Enhancements**:
- Multi-factor cohesion calculation
- Co-occurrence bonus tracking
- Enhanced debug reporting

**Cohesion Factors**:
- Base cohesion (vocabulary diversity × usage efficiency)
- Co-occurrence bonuses
- Grounding association bonuses
- Activation strength bonuses

## Data Structures

### Enhanced AcousticFeatures
```cpp
struct AcousticFeatures {
    float pitch_contour;
    float energy_envelope;
    float spectral_centroid;
    float formant_f1;
    float formant_f2;
    float voicing_strength;
    float rhythm_pattern;
    
    // Enhanced fields
    float intonation_slope;        // Proper ∆pitch/∆time calculation
    float motherese_score;         // Caregiver speech detection
    float attention_score;         // Salience-based attention
    float novelty_score;          // Novelty detection
    
    // Debug information
    std::vector<float> pitch_trajectory;
    float duration;
    std::chrono::milliseconds timestamp;
};
```

### TokenAssociationSnapshot
```cpp
struct TokenAssociationSnapshot {
    std::chrono::milliseconds timestamp;
    std::size_t token_id;
    std::string symbol;
    float activation_strength;
    std::size_t usage_count;
    float cluster_stability;
    float cross_modal_strength;
    DevelopmentalStage stage;
    std::vector<std::string> associated_tokens;
};
```

### ClusterEvolutionData
```cpp
struct ClusterEvolutionData {
    std::size_t formation_step;
    std::string cluster_name;
    std::size_t member_count;
    float cohesion_score;
    bool is_proto_word;
    std::vector<std::string> member_tokens;
    std::chrono::milliseconds formation_time;
};
```

## Usage Examples

### Basic Acoustic Processing
```cpp
// Initialize with enhanced configuration
LanguageSystem::Config config;
config.prosody_attention_weight = 0.4f;
config.intonation_threshold = 0.1f;
config.cross_modal_decay = 0.002f;

LanguageSystem system(config);

// Process audio with enhanced features
std::vector<float> audio = loadAudioFile("caregiver_speech.wav");
auto features = system.extractAcousticFeatures(audio);
float salience = system.calculateSoundSalience(features);

// Enhanced salience should be higher for prosodic speech
if (salience > 0.5f) {
    std::cout << "High prosodic salience detected: " << salience << std::endl;
}
```

### Developmental Tracking
```cpp
// Enable trajectory tracking
system.enableTrajectoryTracking("experiment_logs");

// Run developmental simulation
for (int step = 0; step < 150; ++step) {
    // Simulate developmental activities
    system.performAcousticBabbling(3);
    
    if (step % 10 == 0) {
        system.captureTrajectorySnapshot();
    }
    
    if (step % 20 == 0) {
        auto stats = system.getStatistics();
        std::cout << "Step " << step << ": " << stats.active_vocabulary_size 
                  << " tokens, " << stats.average_token_activation 
                  << " avg activation" << std::endl;
    }
}

// Generate final report
system.generateDevelopmentalReport();
```

### Visualization
```python
# Load and analyze trajectory data
visualizer = SimpleVisualizer('experiment_logs')
trajectory_data, cluster_data = visualizer.load_trajectory_data('experiment_logs')

# Generate comprehensive analysis
visualizer.generate_text_report(trajectory_data, cluster_data, 'analysis.txt')
visualizer.generate_simple_charts(trajectory_data, cluster_data, 'experiment_logs')

print(f"Analyzed {len(trajectory_data)} trajectory records")
print(f"Detected {len(cluster_data)} cluster formations")
```

## Error Handling

### Common Exceptions
```cpp
class AcousticProcessingException : public std::runtime_error {
public:
    AcousticProcessingException(const std::string& message);
};

class TrajectoryTrackingException : public std::runtime_error {
public:
    TrajectoryTrackingException(const std::string& message);
};

class DevelopmentalStageException : public std::runtime_error {
public:
    DevelopmentalStageException(const std::string& message);
};
```

### Error Handling Examples
```cpp
try {
    auto features = system.extractAcousticFeatures(audio_data);
    float salience = system.calculateSoundSalience(features);
} catch (const AcousticProcessingException& e) {
    std::cerr << "Acoustic processing failed: " << e.what() << std::endl;
    // Handle gracefully
}

try {
    system.enableTrajectoryTracking("logs");
    system.captureTrajectorySnapshot();
} catch (const TrajectoryTrackingException& e) {
    std::cerr << "Trajectory tracking failed: " << e.what() << std::endl;
    // Continue without tracking
}
```

## Performance Considerations

### Memory Usage
- **Token embeddings**: O(vocabulary_size × embedding_dimension)
- **Trajectory data**: O(simulation_steps × active_tokens)
- **Audio processing**: O(audio_length × analysis_window)

### Processing Speed
- **Pitch detection**: O(audio_length × log(audio_length))
- **Token similarity**: O(vocabulary_size²)
- **Trajectory capture**: O(active_tokens)

### Optimization Tips
1. **Adjust vocabulary size** based on available memory
2. **Use appropriate audio window sizes** for real-time processing
3. **Enable trajectory tracking selectively** for research scenarios
4. **Batch process audio** when possible for better performance

## Version Compatibility

### API Changes in v2.0
- Enhanced `AcousticFeatures` structure
- New trajectory tracking methods
- Improved configuration parameters
- Additional statistics fields

### Migration from v1.x
```cpp
// v1.x code
LanguageSystem system;
system.processAudio(audio);

// v2.0 equivalent
LanguageSystem::Config config;
config.prosody_attention_weight = 0.4f;  // Enhanced sensitivity
LanguageSystem system(config);
system.processAcousticTeacherSignal(audio, "expected_token");
```

## Future API Extensions

### Planned Additions
- **Semantic grounding API**: Word-meaning associations
- **Social attention methods**: Joint attention mechanisms
- **Compositional learning**: Phrase and sentence formation
- **Real-time streaming**: Live audio processing
- **Multi-agent interaction**: Social learning scenarios

---

**Document Version**: 2.1  
**Last Updated**: November 29, 2025  
**Status**: Production Ready  
**Compatibility**: NeuroForge v2.1+
