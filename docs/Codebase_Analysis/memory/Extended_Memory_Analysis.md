# Extended Memory System Analysis

This document details the Semantic and Procedural memory systems, complementing the Episodic memory analysis.

## 1. Semantic Memory

### Location
- `include/memory/SemanticMemory.h`
- `src/memory/SemanticMemory.cpp`

### Functional Description
Stores generalized knowledge, concepts, and relationships, abstracted from specific episodes.

### Key Components
- **ConceptNode**: The basic unit. Contains:
  - `label`: Name (e.g., "Dog").
  - `feature_vector`: Distributed representation.
  - `type`: Object, Action, Abstract, etc.
  - `relationships`: Links to other concepts (Parent/Child/Related).
- **Consolidation**: Extracts concepts from `EpisodicMemoryManager`. If an object appears in many episodes, it becomes a stable Concept.
- **Graph Operations**: Supports querying the "Knowledge Graph" (e.g., "find path from Dog to Mammal").

### Statistics
- Tracks `active_concepts`, `relationships`, and `abstraction_level`.

---

## 2. Procedural Memory

### Location
- `include/memory/ProceduralMemory.h`
- `src/memory/ProceduralMemory.cpp`

### Functional Description
Stores "Skills" (how to do things) and "Habits" (automatic responses).

### Key Components
- **Skill**: A sequence of actions (`action_sequence`) or motor commands (`motor_pattern`).
  - Tracks `proficiency_level` and `practice_count`.
- **Habit**: A direct mapping from `trigger_context` to `habitual_action`.
  - Formed after repeated execution (`min_repetitions_for_habit`).
- **MotorAction**: Low-level motor primitives.

### Mechanisms
- **Automation**: When a Skill is practiced enough (`proficiency > threshold`), it can become "Automated" (Chunked), requiring less cognitive load.
- **Transfer**: Supports skill transfer between similar tasks.

---

## 3. Working Memory

### Location
- `include/memory/WorkingMemory.h` (inferred)
- `src/memory/WorkingMemory.cpp`

### Functional Description
A short-term buffer for active tokens, concepts, and sensory data.

### Key Features
- **Capacity Limited**: Holds a finite number of items (7 +/- 2 analog).
- **Decay**: Items fade quickly unless rehearsed.
- **Interface**: The "workbench" where the Reasoner operates.

