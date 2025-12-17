# Substrate Integration Analysis

This document analyzes how the high-level cognitive systems interface with the low-level neural substrate (HypergraphBrain).

## 1. SubstrateLanguageManager

### Location
- `include/core/SubstrateLanguageManager.h`
- `src/core/SubstrateLanguageManager.cpp`

### Functional Description
A static manager class that decouples the `HypergraphBrain` from the `LanguageSystem`.

### Mechanism
- **Registry**: Maintains a map of `HypergraphBrain*` -> `SubstrateLanguageAdapter*`.
- **Lifecycle**:
  - `initializeForBrain()`: Creates an adapter.
  - `processSubstrateLanguage()`: Called in the brain's update loop to drive the adapter.
  - `shutdownForBrain()`: Cleanup.

### Why this exists?
To avoid circular dependencies. The Brain needs to drive language processing, but the Language System needs to read Brain state. This manager mediates that relationship.

---

## 2. SubstrateLanguageAdapter

### Location
- `include/core/SubstrateLanguageAdapter.h`
- `src/core/SubstrateLanguageAdapter.cpp`

### Functional Description
The translator between "Spikes" and "Tokens".

### Key Features
- **Encoding**: Converts `LanguageSystem` tokens into input currents for specific neuronal populations (e.g., Auditory Cortex or Broca's Area).
- **Decoding**: Monitors output regions (e.g., Motor Cortex) to infer generated tokens.
- **Binding**: Manages `NeuralLanguageBindings` which define *which* neurons correspond to *which* symbols.

---

## 3. SubstrateTaskGenerator

### Location
- `include/core/SubstrateTaskGenerator.h` (inferred)
- `src/core/SubstrateTaskGenerator.cpp`

### Functional Description
Generates training tasks for the substrate during the "Train" mode of M7 Autonomy.

### Features
- **Curriculum**: Likely defines a sequence of tasks (e.g., "Hold activation", "Sequence recall").
- **Evaluation**: Checks if the substrate performed the task correctly and issues rewards via `LearningSystem`.

