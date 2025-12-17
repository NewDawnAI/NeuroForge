# NeuroForge Configuration Parameters Reference

## Overview

This document provides a comprehensive reference for all configuration parameters in the NeuroForge system, their effects on behavior, and recommended values for different use cases.

## Core Configuration Structure

### Location
**File**: `include/core/LanguageSystem.h`  
**Struct**: `Config`

### Parameter Categories
1. **Developmental Parameters**: Control learning progression
2. **Acoustic Processing**: Audio analysis and prosodic detection
3. **Visual-Linguistic Integration**: Cross-modal associations
4. **Token Management**: Vocabulary and clustering behavior
5. **Speech Production**: Output generation and quality
6. **Multimodal Integration**: Cross-sensory learning

## Developmental Parameters

### Basic Learning Controls

#### `mimicry_learning_rate`
- **Type**: `float`
- **Default**: `0.1f`
- **Range**: `0.01f - 0.5f`
- **Purpose**: Controls how quickly the system learns from caregiver interactions
- **Effect**: Higher values = faster mimicry learning, but may reduce stability

#### `grounding_strength`
- **Type**: `float`
- **Default**: `0.3f`
- **Range**: `0.1f - 0.8f`
- **Purpose**: Strength of word-object associations
- **Effect**: Higher values = stronger semantic grounding

#### `developmental_momentum`
- **Type**: `float`
- **Default**: `0.05f`
- **Range**: `0.01f - 0.2f`
- **Purpose**: Rate of developmental stage progression
- **Effect**: Higher values = faster stage transitions

### Advanced Developmental Controls

#### `chaos_exploration_rate`
- **Type**: `float`
- **Default**: `0.8f`
- **Range**: `0.3f - 1.0f`
- **Purpose**: Amount of random exploration in Chaos stage
- **Effect**: Higher values = more diverse initial token generation

#### `babbling_structure_weight`
- **Type**: `float`
- **Default**: `0.4f`
- **Range**: `0.2f - 0.7f`
- **Purpose**: Emphasis on structured patterns in Babbling stage
- **Effect**: Higher values = more organized proto-word formation

## Acoustic Processing Parameters

### Prosodic Sensitivity

#### `prosody_attention_weight`
- **Type**: `float`
- **Default**: `0.4f` (Enhanced from `0.3f`)
- **Range**: `0.1f - 0.8f`
- **Purpose**: Boost for prosodic salience detection
- **Effect**: Higher values = increased sensitivity to intonation patterns
- **Critical**: Essential for caregiver-infant interaction detection

#### `intonation_threshold`
- **Type**: `float`
- **Default**: `0.1f` (Lowered from `0.3f`)
- **Range**: `0.05f - 0.5f`
- **Purpose**: Hz change threshold for attention trigger
- **Effect**: Lower values = more sensitive to pitch changes
- **Breakthrough**: Key parameter for prosodic salience success

#### `motherese_boost`
- **Type**: `float`
- **Default**: `0.4f`
- **Range**: `0.2f - 0.8f`
- **Purpose**: Amplification for infant-directed speech
- **Effect**: Higher values = stronger response to caregiver speech patterns

### Audio Analysis

#### `formant_clustering_threshold`
- **Type**: `float`
- **Default**: `50.0f`
- **Range**: `20.0f - 100.0f`
- **Purpose**: Hz threshold for phoneme clustering
- **Effect**: Lower values = more fine-grained phoneme distinctions

#### `spectral_analysis_window`
- **Type**: `std::size_t`
- **Default**: `1024`
- **Range**: `512 - 4096`
- **Purpose**: Window size for frequency analysis
- **Effect**: Larger values = better frequency resolution, slower processing

## Visual-Linguistic Integration Parameters

### Cross-Modal Associations

#### `face_language_coupling`
- **Type**: `float`
- **Default**: `0.6f`
- **Range**: `0.3f - 0.9f`
- **Purpose**: Face-speech binding strength
- **Effect**: Higher values = stronger face-voice associations

#### `gaze_attention_weight`
- **Type**: `float`
- **Default**: `0.4f`
- **Range**: `0.2f - 0.7f`
- **Purpose**: Gaze direction influence on attention
- **Effect**: Higher values = more gaze-guided learning

#### `lip_sync_threshold`
- **Type**: `float`
- **Default**: `0.3f`
- **Range**: `0.1f - 0.6f`
- **Purpose**: Minimum lip-speech correlation
- **Effect**: Lower values = more permissive lip-sync detection

#### `visual_grounding_boost`
- **Type**: `float`
- **Default**: `0.5f`
- **Range**: `0.2f - 0.8f`
- **Purpose**: Visual modality reinforcement
- **Effect**: Higher values = stronger visual-linguistic associations

#### `cross_modal_decay`
- **Type**: `float`
- **Default**: `0.002f` (Reduced from `0.005f`)
- **Range**: `0.001f - 0.01f`
- **Purpose**: Association decay rate
- **Effect**: Lower values = longer-lasting associations
- **Critical**: Key parameter for cohesion improvement

## Vision Foveation Runtime Flags (New)

These are command-line controls for dynamic retina focusing during `--vision-demo` or `--youtube-mode` runs.

### `--foveation[=on|off]`
- Type: flag
- Default: `off`
- Purpose: Enable dynamic focusing of the capture region.

### `--fovea-size=WxH`
- Type: string (`WxH`)
- Default: `640x360`
- Purpose: Set the width and height of the fovea rectangle.

### `--fovea-mode=cursor|center|attention`
- Type: enum
- Default: `cursor`
- Purpose: Selects how the fovea center is determined.
  - `cursor`: follows the OS cursor.
  - `center`: fixed at the retina/sandbox center.
  - `attention`: follows the most salient vision tile (motor cortex output).

### `--fovea-alpha=F`
- Type: float in `[0,1]`
- Default: `0.3`
- Purpose: EMA smoothing rate for the fovea center to reduce jitter.
- Effect: Higher values track faster; lower values stabilize more.

Notes:
- When sandbox mode is enabled (`--sandbox=on`), the fovea rectangle is clamped to the sandbox bounds.
- Telemetry logs include `vision.retina` and `vision.foveation` metadata.

## Browser Sandbox Runtime Flags (New)

These enable a safe, bounded interaction window with optional embedded Edge WebView2.

### `--sandbox[=on|off]`
- Type: flag
- Default: `off`
- Purpose: Enable a dedicated sandbox window.

### `--sandbox-url=URL`
- Type: string (URL)
- Default: `https://www.youtube.com`
- Purpose: Initial navigation target for the embedded browser.

### `--sandbox-size=WxH`
- Type: string (`WxH`)
- Default: `1280x720`
- Purpose: Client-area size of the sandbox window.

Notes:
- When WebView2 is available on Windows/MSVC builds, the sandbox hosts an embedded Edge browser; otherwise a plain window is used.
- Actions (`cursor_move`, `scroll`, `click`, `type_text`, `key_press`) can be logged to `actions` in MemoryDB when enabled.

### Sandbox Init Readiness (Windows)

## Phase A Mimicry & Replay Runtime Flags (Updated)

These flags control Phase A student learning, replay behavior, and evaluation for triplet‑grounding experiments.

### Core Learning Pressure
- `--reward-scale=F`
  - Scales total reward applied to student updates. Higher values increase alignment pressure.
- `--student-learning-rate=F`
  - Base per‑entry learning rate for student embedding updates.
- `--phase-a-ema[=on|off]`, `--phase-a-ema-min=F`, `--phase-a-ema-max=F`
  - Enables EMA stabilizer and clamps update magnitude to `[min,max]` for stability.

### Similarity & Novelty Criteria
- `--phase-a-similarity-threshold=F`
  - Minimum cosine similarity for a mimicry attempt to be considered successful.
- `--phase-a-novelty-threshold=F`
  - Minimum novelty required to avoid copying degenerate patterns.

### Negative Sampling
- `--negative-sampling-k=K`
  - Number of negative examples considered per attempt.
- `--negative-weight=F`
  - Repulsion weight applied to negatives; tune low to prevent trajectory scattering.

### Replay Controls
- `--phase-a-replay-interval=N`
  - Triggers a replay cycle every `N` mimicry attempts.
- `--phase-a-replay-top-k=K`
  - Replays the top‑K past attempts by reward to reinforce alignment.
- `--phase-a-replay-boost=F`
  - Scales reward during replay updates.
- `--phase-a-replay-lr-scale=F`
  - Scales learning rate during replay updates.
- `--phase-a-replay-include-hard-negatives=on|off`
  - Enables hard‑negative replay to improve discrimination.
- `--phase-a-replay-hard-k=K`
  - Number of hard negatives processed in each replay cycle.
- `--phase-a-replay-repulsion-weight=F`
  - Repulsion strength for hard negatives during replay.

### Dataset & Telemetry
- `--dataset-mode=triplets`, `--dataset-triplets=PATH`, `--dataset-limit=N`
  - Enables Flickr30k‑style triplet ingestion and evaluation over `N` items.
- `--memory-db=PATH`
  - Enables telemetry to SQLite with run/episode linking.
- `--hippocampal-snapshots=on|off`
  - Emits `snapshot:phase_a` experiences with Phase A metrics for evaluation.

### Integration & Substrate
- `--phase5-language=on|off`
  - Activates LanguageSystem for token grounding during Phase A.
- `--mirror-mode=off|vision|audio`
  - Derives student embedding dimension from sensory features when teacher vectors are absent.
- `--substrate-mode=off|mirror|train|native`
  - Selects brain integration mode; `native` enables region‑level plasticity.
- Projection to LanguageSystem tokens (automatic)
  - Student embeddings are linearly projected to `LanguageSystem::Config.embedding_dimension` during Phase A logging and token grounding.
  - Behavior: maps 62‑dim Phase A vectors to 256‑dim LanguageSystem expectations when active.
  - Reference: `src/core/PhaseAMimicry.cpp:1423`–`1454` (`projectStudent`).
- The sandbox window waits for readiness before entering the main loop to prevent startup stalls.
- Readiness conditions: WebView2 controller created, first navigation starting fired, and at least one client-area bounds update (WM_SIZE/WM_PAINT).
- Effect: Stable startup for `--sandbox=on` runs; prevents hangs when actions are disabled.

### Action Gating & Debug Flags
- `--simulate-blocked-actions=N`
  - Type: integer (≥0)
  - Purpose: Debug-only counter that simulates N blocked actions per step for metrics.
  - Behavior: Does not gate real actions; increments blocked metrics for analysis and shaped reward context.
- `--simulate-rewards=N`
  - Type: integer (≥0)
  - Purpose: Emit N synthetic reward events per step for pipeline debugging.
  - Behavior: Tagged in `reward_log.source` and `context_json` for traceability.

### Unified Action Filter
- All action sites flow through a centralized gating layer with explicit allow/deny reasons.
- Reasons: `ok`, `no_web_actions`, `phase15_deny`, `phase13_freeze`.
- Logging: Reasons are attached to `actions.payload_json.filter.reason` and mirrored in `reward_log.context_json.blocked_actions`.

## Token Management Parameters

### Vocabulary Control

#### `max_vocabulary_size`
- **Type**: `std::size_t`
- **Default**: `1000`
- **Range**: `100 - 10000`
- **Purpose**: Maximum number of tokens to maintain
- **Effect**: Higher values = larger vocabulary capacity

#### `token_embedding_dimension`
- **Type**: `std::size_t`
- **Default**: `128`
- **Range**: `64 - 512`
- **Purpose**: Dimensionality of token representations
- **Effect**: Higher values = richer representations, more memory usage

#### `token_decay_rate`
- **Type**: `float`
- **Default**: `0.01f`
- **Range**: `0.001f - 0.05f`
- **Purpose**: Rate of unused token decay
- **Effect**: Higher values = faster vocabulary turnover

### Clustering and Similarity

#### `token_similarity_threshold`
- **Type**: `float`
- **Default**: `0.3f` (Lowered from `0.5f`)
- **Range**: `0.1f - 0.8f`
- **Purpose**: Similarity threshold for token clustering
- **Effect**: Lower values = easier clustering, more proto-words
- **Critical**: Essential for cohesion improvement

#### `cohesion_boost_factor`
- **Type**: `float`
- **Default**: `2.0f` (Increased from `1.5f`)
- **Range**: `1.0f - 3.0f`
- **Purpose**: Boost factor for co-occurring tokens
- **Effect**: Higher values = stronger token associations

#### `co_occurrence_bonus`
- **Type**: `float`
- **Default**: `0.02f` (New parameter)
- **Range**: `0.01f - 0.05f`
- **Purpose**: Bonus per repeated token pair
- **Effect**: Higher values = stronger reinforcement of repeated patterns

## Speech Production Parameters

### Output Generation

#### `speech_production_rate`
- **Type**: `float`
- **Default**: `1.0f`
- **Range**: `0.5f - 2.0f`
- **Purpose**: Default speaking rate
- **Effect**: Higher values = faster speech output

#### `articulation_precision`
- **Type**: `float`
- **Default**: `0.7f`
- **Range**: `0.3f - 1.0f`
- **Purpose**: Precision of speech articulation
- **Effect**: Higher values = clearer speech output

#### `prosodic_variation`
- **Type**: `float`
- **Default**: `0.5f`
- **Range**: `0.2f - 0.8f`
- **Purpose**: Amount of prosodic variation in output
- **Effect**: Higher values = more expressive speech

### Quality Control

#### `self_monitoring_weight`
- **Type**: `float`
- **Default**: `0.3f`
- **Range**: `0.1f - 0.6f`
- **Purpose**: Strength of self-monitoring feedback
- **Effect**: Higher values = more self-correction

#### `output_quality_threshold`
- **Type**: `float`
- **Default**: `0.6f`
- **Range**: `0.3f - 0.9f`
- **Purpose**: Minimum quality for speech output
- **Effect**: Higher values = higher quality requirements

## Multimodal Integration Parameters

### Timing and Synchronization

#### `multimodal_sync_window`
- **Type**: `float`
- **Default**: `0.2f`
- **Range**: `0.1f - 0.5f`
- **Purpose**: Time window for cross-modal synchronization
- **Effect**: Larger values = more permissive synchronization

#### `attention_decay_rate`
- **Type**: `float`
- **Default**: `0.05f`
- **Range**: `0.01f - 0.1f`
- **Purpose**: Rate of attention decay over time
- **Effect**: Higher values = shorter attention spans

### Integration Weights

#### `acoustic_weight`
- **Type**: `float`
- **Default**: `0.6f`
- **Range**: `0.3f - 0.8f`
- **Purpose**: Relative importance of acoustic input
- **Effect**: Higher values = more acoustic-driven learning

#### `visual_weight`
- **Type**: `float`
- **Default**: `0.4f`
- **Range**: `0.2f - 0.7f`
- **Purpose**: Relative importance of visual input
- **Effect**: Higher values = more visually-guided learning

## Recommended Configurations

### Research Configuration (Default)
```cpp
Config research_config = {
    // Developmental
    .mimicry_learning_rate = 0.1f,
    .grounding_strength = 0.3f,
    
    // Acoustic (Enhanced)
    .prosody_attention_weight = 0.4f,
    .intonation_threshold = 0.1f,
    .motherese_boost = 0.4f,
    
    // Cross-modal (Enhanced)
    .cross_modal_decay = 0.002f,
    
    // Token management (Enhanced)
    .token_similarity_threshold = 0.3f,
    .cohesion_boost_factor = 2.0f,
    .co_occurrence_bonus = 0.02f
};
```

### Fast Development Configuration
```cpp
Config fast_config = {
    // Accelerated learning
    .mimicry_learning_rate = 0.2f,
    .developmental_momentum = 0.1f,
    .chaos_exploration_rate = 1.0f,
    
    // Sensitive detection
    .intonation_threshold = 0.05f,
    .prosody_attention_weight = 0.6f,
    
    // Aggressive clustering
    .token_similarity_threshold = 0.2f,
    .cohesion_boost_factor = 3.0f,
    .cross_modal_decay = 0.001f
};
```

### Conservative Configuration
```cpp
Config conservative_config = {
    // Slower, more stable learning
    .mimicry_learning_rate = 0.05f,
    .developmental_momentum = 0.02f,
    .chaos_exploration_rate = 0.5f,
    
    // Less sensitive detection
    .intonation_threshold = 0.2f,
    .prosody_attention_weight = 0.3f,
    
    // Stricter clustering
    .token_similarity_threshold = 0.6f,
    .cohesion_boost_factor = 1.2f,
    .cross_modal_decay = 0.008f
};
```

## Parameter Tuning Guidelines

### For Prosodic Sensitivity
1. **Lower `intonation_threshold`** for increased sensitivity
2. **Increase `prosody_attention_weight`** for stronger responses
3. **Adjust `motherese_boost`** for caregiver interaction strength

### For Token Clustering
1. **Lower `token_similarity_threshold`** for easier clustering
2. **Increase `cohesion_boost_factor`** for stronger associations
3. **Reduce `cross_modal_decay`** for longer-lasting connections

### For Developmental Speed
1. **Increase `developmental_momentum`** for faster stage progression
2. **Adjust `mimicry_learning_rate`** for learning speed
3. **Modify `chaos_exploration_rate`** for initial diversity

### For System Stability
1. **Use moderate values** for all parameters initially
2. **Adjust one parameter at a time** to isolate effects
3. **Monitor test success rates** to validate changes

## Performance Impact

### Memory Usage
- `max_vocabulary_size`: Linear impact on memory
- `token_embedding_dimension`: Quadratic impact on memory
- `spectral_analysis_window`: Linear impact on processing memory

### Processing Speed
- `intonation_threshold`: Lower values = more processing
- `token_similarity_threshold`: Lower values = more comparisons
- `spectral_analysis_window`: Larger values = slower analysis

### Learning Quality
- `cross_modal_decay`: Lower values = better retention
- `cohesion_boost_factor`: Higher values = stronger learning
- `prosody_attention_weight`: Higher values = better attention

## Troubleshooting

### Common Issues

#### Low Test Success Rates
- **Check**: `intonation_threshold` (try lowering)
- **Check**: `prosody_attention_weight` (try increasing)
- **Check**: `token_similarity_threshold` (try lowering)

#### Poor Token Clustering
- **Check**: `token_similarity_threshold` (lower for easier clustering)
- **Check**: `cohesion_boost_factor` (increase for stronger associations)
- **Check**: `cross_modal_decay` (reduce for longer retention)

#### Slow Development
- **Check**: `developmental_momentum` (increase for faster progression)
- **Check**: `mimicry_learning_rate` (increase for faster learning)
- **Check**: `chaos_exploration_rate` (increase for more diversity)

### Debug Parameters
```cpp
// Enable debug logging
bool enable_debug_logging = true;
bool verbose_trajectory_tracking = true;
bool detailed_milestone_reporting = true;
```

## Version History

### v1.1 (Dataset & Reward Controls)
- Added runtime CLI parameters for dataset ingestion and reward scaling
- Triplets mode flags documented with precedence notes
- Aligned configuration reference with Phase A substrate controls

### v1.0 (Breakthrough Configuration)
- Enhanced prosodic sensitivity parameters
- Improved cohesion calculation factors
- Added co-occurrence bonus mechanism
- Achieved 80% test success rate

### v0.9 (Initial Configuration)
- Basic parameter set
- 60% test success rate
- Foundation for enhancement

## Future Parameters

### Planned Additions
- `semantic_grounding_weight`: For word-meaning associations
- `social_attention_factor`: For joint attention mechanisms
- `compositional_learning_rate`: For phrase formation
- `pragmatic_inference_strength`: For communication intent

---

**Runtime CLI Parameters (Dataset & Reward)**
```text
--dataset-triplets=PATH                Root of triplet dataset (audio/text/images)
--dataset-mode=triplets               Enable triplet ingestion mode
--dataset-limit=N                     Limit number of triplets loaded
--dataset-shuffle[=on|off]            Shuffle loaded triplets (default: off)
--reward-scale=F                      Scale mimicry reward delivered (default: 1.0)
```

**Runtime CLI Parameters (Mimicry & Substrate)**
```text
--mimicry[=on|off]                     Enable Phase A mimicry (default: off)
--mimicry-internal[=on|off]            Route rewards internally (default: off)
--mirror-mode=off|vision|audio         Student embedding source (default: off)
--substrate-mode=off|mirror|train|native  Connect Phase A to substrate (default: off)
--teacher-embed=PATH                   Provide teacher embedding vector file
```

Notes (Teacher Embedding Format & Export Timing)
- `--teacher-embed` accepts plain text (whitespace‑separated floats), CSV with numeric values, and JSON arrays (e.g., `[0.12, -0.34, ...]`). The loader preserves numeric characters (`0–9`, `-`, `+`, `.`, `e`, `E`) and whitespace; all other characters are treated as separators.
- When `--substrate-mode=native|mirror|train` and `--mimicry-internal=off` are set, Phase A rewards are delivered to the unified brain and logged to MemoryDB.
- Phase A CSV exports (`rewards.csv`, `phase_a_teacher.csv`, `phase_a_student.csv`) are generated post‑run. Use `python tools\export_embeddings_rewards.py --db <db> --out_dir <dir>` to export. Empty files during an ongoing run are expected.

**Runtime CLI Parameters (Unified Reward Pipeline)**
```text
--wt-teacher=FLOAT                     Weight for teacher component (default: 0.6)
--wt-novelty=FLOAT                     Weight for novelty component (default: 0.1)
--wt-survival=FLOAT                    Weight for survival component (default: 0.3)
--log-shaped-zero[=on|off]             Log shaped rows even if components are zero (default: off)
--phase-c-survival-bias[=on|off]       Enable hazard-driven survival modulation (default: off)
--phase-c-survival-scale=FLOAT         Scale survival reward contribution (e.g., 0.8)
--phase-c-hazard-weight=FLOAT          Down-modulation weight for hazard (default: 0.2)
--hazard-density=FLOAT                 Fixed external hazard density in [0,1]
--memdb-interval=MS                    MemoryDB telemetry interval (ms)
--reward-interval=MS                   Reward logging interval (ms)
```

**Notes (Telemetry & Logging)**
- `reward_log` includes `source='shaped'`, `source='merged'`, and `source='survival'` when enabled.
- `learning_stats` records `reward_updates` and `avg_weight_change` at `--memdb-interval` or `NF_MEMDB_INTERVAL_MS` cadence.
- CLI precedence: `--memdb-interval` > `NF_MEMDB_INTERVAL_MS`; `--reward-interval` > `NF_REWARD_INTERVAL_MS`.

Phase A MemoryDB schema updates (v0.16+)
- `reward_log(ts_ms, step, reward, source, context_json, run_id)` stores Phase A rewards when external routing is enabled.
- `substrate_states(ts_ms, step, state_type, region_id, serialized_data, run_id)` includes `state_type='phase_a_teacher'|'phase_a_student'` for typed embedding snapshots.

**Document Version**: 1.1  
**Last Updated**: November 29, 2025  
**Status**: Production Ready  
**Next Review**: Upon Parameter Optimization Studies
## Phase A Mimicry Parameters (Updated)

### Student Representation & Stability

#### `enable_student_table`
- **Type**: `bool`
- **Default**: `true`
- **Purpose**: Enables internal student embeddings table for learned representations.

#### `enable_ema_stabilizer`
- **Type**: `bool`
- **Default**: `true`
- **Purpose**: Clamps update coefficient to stabilize student updates.

#### `ema_alpha_min`
- **Type**: `float`
- **Default**: `0.02f`
- **Purpose**: Lower bound for EMA mixing coefficient.

#### `ema_alpha_max`
- **Type**: `float`
- **Default**: `0.2f`
- **Purpose**: Upper bound for EMA mixing coefficient.

### Replay Buffer Controls (New)

#### `replay_interval_steps`
- **Type**: `std::size_t`
- **Default**: `100`
- **Purpose**: Interval at which Phase A replays top attempts.

#### `replay_top_k`
- **Type**: `std::size_t`
- **Default**: `5`
- **Purpose**: Number of highest‑reward attempts to reinforce each replay cycle.

References: `include/core/PhaseAMimicry.h:135–140`, `src/core/PhaseAMimicry.cpp:460–465`, `src/core/PhaseAMimicry.cpp:1323`.
