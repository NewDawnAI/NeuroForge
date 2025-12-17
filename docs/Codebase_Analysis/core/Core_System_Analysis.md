# Core System Analysis

This document analyzes the core components of the NeuroForge system found in `src/core` and `include/core`. These components form the fundamental substrate of the hypergraph brain simulation.

## 1. HypergraphBrain

### Location
- `include/core/HypergraphBrain.h`
- `src/core/HypergraphBrain.cpp`

### Functional Description
The `HypergraphBrain` class is the central orchestrator of the NeuroForge system. It manages the lifecycle of the simulation, coordinates processing across billions of neurons distributed in regions, and handles global system states. It supports multiple processing modes (Sequential, Parallel, Hierarchical) and integrates with the `LearningSystem` for plasticity.

### Input/Output Specifications
- **Input**: Sensory data via Encoders (injected into specific Regions), configuration parameters.
- **Output**: Motor commands (via Motor Cortex regions), system statistics, brain state snapshots.

### Dependencies
- `Region`: Manages a collection of regions.
- `ConnectivityManager`: Handles wiring between regions.
- `LearningSystem`: Delegates plasticity and memory consolidation.
- `MemoryDB`: Persists hippocampal snapshots and state.
- `AutonomousScheduler`: Schedules tasks (inferred).

### Internal Implementation Logic
- **Initialization**: Sets up regions, allocates memory, and establishes connectivity using `ConnectivityManager`.
- **Processing Loop**:
  - Updates global statistics.
  - Iterates through regions based on `ProcessingMode`.
  - Triggers `Region::process()` for each active region.
  - Invokes `LearningSystem::updateLearning()`.
  - Manages threading for parallel execution.
- **M6 Autonomy**: Implements "Hippocampal Snapshots" for fast plasticity, allowing the system to save significant states to `MemoryDB`.

### Performance Characteristics
- Designed for massive scale (billions of neurons).
- Uses `std::vector` and sparse representations for efficiency.
- Supports multi-threading (`processing_threads_`).
- Includes hardware monitoring (CPU/GPU usage).

### Usage Example
```cpp
auto connectivity = std::make_shared<ConnectivityManager>();
HypergraphBrain brain(connectivity, 60.0f, HypergraphBrain::ProcessingMode::Parallel);
brain.initialize();
brain.start();
// ... simulation loop ...
brain.stop();
```

---

## 2. Neuron

### Location
- `include/core/Neuron.h`
- `src/core/Neuron.cpp`

### Functional Description
Represents a single spiking neuron. It handles activation states, membrane potential integration, and spike generation. It is designed to be thread-safe and memory-efficient.

### Key Features
- **States**: Active, Inactive, Inhibited, Refractory.
- **Properties**: Activation threshold, decay rate, refractory period.
- **Connectivity**: Maintains lists of input and output `Synapse` pointers.

### Internal Logic
- **Process**:
  - Sums inputs from `input_synapses`.
  - Applies decay to current activation.
  - Checks against `threshold`.
  - Updates state (e.g., entering Refractory period after firing).

---

## 3. Synapse

### Location
- `include/core/Synapse.h`
- `src/core/Synapse.cpp`

### Functional Description
Represents a directed connection between two neurons. It encapsulates the weight, delay, and plasticity rules governing the connection strength.

### Key Features
- **Plasticity**: Supports Hebbian, STDP, BCM, and Oja's rules.
- **Types**: Excitatory, Inhibitory, Modulatory.
- **Delay**: Simulates signal propagation time.

### Usage Example
```cpp
auto synapse = std::make_shared<Synapse>(id, source_neuron, target_neuron, 0.5f);
synapse->setPlasticityRule(Synapse::PlasticityRule::STDP);
```

---

## 4. Region

### Location
- `include/core/Region.h`
- `src/core/Region.cpp`

### Functional Description
A logical container for neurons, representing a specific brain area (e.g., Visual Cortex, Hippocampus). It manages local connectivity and executes processing logic for its constituent neurons.

### Key Features
- **Metabolism**: Tracks mitochondrial energy states (ATP analogs), adding biological constraints to processing.
- **Activation Patterns**: Synchronous, Asynchronous, Layered, etc.
- **Statistics**: Monitors local energy usage, active neurons, and metabolic stress.

---

## 5. LearningSystem

### Location
- `include/core/LearningSystem.h`
- `src/core/LearningSystem.cpp`

### Functional Description
Manages the adaptive capabilities of the brain. It implements synaptic weight updates based on activity (Hebbian/STDP) and reinforcement (Reward-Modulated Plasticity).

### Key Features
- **STDP**: Spike-Timing Dependent Plasticity implementation.
- **Reinforcement**: Applies global reward signals to eligible synapses.
- **Intrinsic Motivation (M7)**: Computes uncertainty, surprise, and prediction error to drive autonomous learning.
- **Consolidation**: Moves short-term memory traces to long-term storage (Hippocampal replay).

### Potential Technical Debt
- The interaction between `LearningSystem` and individual `Synapse` update logic needs careful synchronization to avoid race conditions in parallel modes.
- "Phase 4" methods suggest an evolutionary naming scheme that might need refactoring for clarity.

