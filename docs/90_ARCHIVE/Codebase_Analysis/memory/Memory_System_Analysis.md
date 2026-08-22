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
Manages "autobiographical" memory. It stores events with context, sensory data, and emotional state.

### Key Features
- **Episode Structure**: Contains `context`, `narrative`, `sensory_data`, and `emotional_state`.
- **Consolidation**: Moves short-term episodes to long-term storage based on `salience`.
- **Retrieval**: Supports content-based search (similarity) and retrieval by ID.
- **Forgetting**: Implements decay and pruning of weak memories.

### Interfaces
- `storeEpisode(...)`: Main entry point for recording events.
- `consolidateMemories()`: Triggers the consolidation process.

---

## 3. Memory Integration & Sleep

### Components
- **SleepConsolidation**: (`include/memory/SleepConsolidation.h`) Handles offline processing, likely replaying episodes to train cortical regions (transfer from Hippocampus to Cortex).
- **MemoryIntegrator**: (`include/memory/MemoryIntegrator.h`) Likely coordinates between Episodic, Semantic, and Procedural systems to ensure consistency.

### Functional Role
These components implement the "Dreaming" or "Rest" phases where the brain optimizes its internal representation without external input. This is critical for the stability of the `LearningSystem` to prevent catastrophic forgetting.

