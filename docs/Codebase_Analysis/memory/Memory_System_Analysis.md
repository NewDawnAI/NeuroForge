# Memory System Analysis

This document analyzes the memory architecture of NeuroForge, located in `src/memory` and `include/memory`. The system uses a tiered approach inspired by human memory systems (Episodic, Semantic, Procedural), backed by a persistent SQLite database.

## 1. MemoryDB

### Location
- `include/core/MemoryDB.h`
- `src/core/MemoryDB.cpp`

### Functional Description
The persistent storage layer for the entire brain. It wraps a SQLite database and provides structured access to telemetry, learning statistics, and memory records.

### Key Features
- **Run Management**: Tracks distinct simulation runs and metadata.
- **Tables**:
  - `experiences`: Raw input/output pairs.
  - `episodes`: Temporal groupings of experiences.
  - `rewards`: Log of reward signals.
  - `hippocampal_snapshots`: Serialized states for fast plasticity.
  - `substrate_states`: Heavyweight dumps of neuron/synapse states.
- **Performance**: optimized for high-throughput logging (transaction batching inferred).

---

## 2. EpisodicMemoryManager

### Location
- `include/memory/EpisodicMemoryManager.h`
- `src/memory/EpisodicMemoryManager.cpp`

### Functional Description
Manages "autobiographical" memory. It stores events with context, sensory data, and emotional state. Provides two APIs: a **pattern-based API** (primary, for vector operations) and a **legacy API** (string-based adapters).

### Key Features
- **Dual API**: Pattern-based (`encodePattern`, `recallByCue`) and legacy (`storeEpisode`, `retrieveEpisode`).
- **Consolidation**: Moves short-term episodes to long-term storage based on `salience`.
- **Retrieval**: Supports content-based search (cosine similarity) and retrieval by ID.
- **Forgetting**: Implements decay (`decayMemoryTraces`) and pruning (`pruneWeakMemories`) of weak memories.
- **Thread Safety**: Uses `std::shared_mutex` with `shared_lock` for reads and exclusive `lock_guard` for writes. Internal `*Locked()` helpers (`encodePatternLocked`, `consolidatePatternsLocked`, etc.) prevent deadlocks in call chains.
- **Named Constants**: `kDefaultSalience`, `kMinSearchSimilarity`, `kActivationBoost`, `kConsolidationStep`, `kPruneActivationThreshold`, `kDefaultMinForgetStrength`, `kMaxRecentEpisodes`.

### Interfaces
- `encodePattern(sensory, context, emotional, salience)`: Primary pattern storage.
- `recallByCue(cue, max_results, threshold)`: Similarity-based retrieval.
- `storeEpisode(...)`: Legacy entry point for recording events.
- `consolidateMemories()`: Triggers the consolidation process.

---

## 3. Memory Integration & Sleep

### Components
- **SleepConsolidation**: (`include/memory/SleepConsolidation.h`) Handles offline processing — replays episodes, performs homeostatic synaptic scaling, and transfers between memory systems (episodic→semantic, working→procedural).
- **MemoryIntegrator**: (`include/memory/MemoryIntegrator.h`) Coordinates between Episodic, Semantic, Procedural, Working, and Developmental memory systems. Supports cross-system queries and embedded-based retrieval.
- **DreamProcessor**: (`include/memory/DreamProcessor.h`) Generates dream narratives during REM/SWS sleep — episodic replay, creative recombination, problem solving, and emotional regulation.
- **DevelopmentalConstraints**: (`include/memory/DevelopmentalConstraints.h`) Stage-gated learning via critical periods, age-based plasticity decay, and synaptic pruning.

### Thread Safety
All memory modules use `std::shared_mutex` for brain-like parallel access:
- **Read methods** use `std::shared_lock` — enabling concurrent queries across memory systems (e.g., episodic recall + semantic settling + procedural recall simultaneously).
- **Write methods** use exclusive `std::lock_guard` — serializing mutations for data integrity.

### Functional Role
These components implement the "Dreaming" or "Rest" phases where the brain optimizes its internal representation without external input. This is critical for the stability of the `LearningSystem` to prevent catastrophic forgetting.
