# Language System Analysis

This document analyzes the Language System of NeuroForge, located in `src/core` and `include/core`. This system implements developmental language acquisition, moving from chaos to communication.

## 1. LanguageSystem

### Location
- `include/core/LanguageSystem.h`
- `src/core/LanguageSystem.cpp`

### Functional Description
Implements a developmental path for language acquisition:
1.  **Chaos**: Random activation.
2.  **Babbling**: Generation of proto-phonemes.
3.  **Mimicry**: Copying teacher patterns (Phase A integration).
4.  **Grounding**: Associating symbols with multimodal experiences.
5.  **Communication**: Goal-directed language usage.

### Key Components
- **SymbolicToken**: Represents a unit of meaning (Phoneme, Word, Action, Meta). Tracks usage count, activation, and multimodal embeddings.
- **PhonemeCluster**: Groups acoustic features into stable phonemes using clustering logic.
- **ProtoWord**: Tracks emerging word patterns (e.g., "ma-ma") and their stability/crystallization.
- **GroundingAssociation**: Links tokens to sensory data (Visual, Tactile, Auditory).
- **MultimodalAttentionState**: Manages attention between faces, speech, and objects (Joint Attention).

### Development Stages
The system explicitly models development stages. It doesn't just "learn" text; it simulates the *process* of learning to speak:
- **Acoustic Analysis**: Extracts pitch, energy, formants, and "motherese" scores.
- **Crystallization**: Repeated patterns become "crystallized" into stable tokens.

---

## 2. Token Tracking

### Components
- `LanguageSystem_TokenTracker.cpp`
- `LanguageSystem_TokenTracker_Simple.cpp`

### Functionality
These components likely implement the logic for:
- **Frequency Counting**: How often a token is used.
- **Decay**: Unused tokens fade away.
- **Activation**: Managing the "working memory" of active tokens during a conversation or thought process.

---

## 3. Substrate Integration

### SubstrateLanguageManager
- **Location**: `include/core/SubstrateLanguageManager.h`
- **Role**: Bridges the high-level `LanguageSystem` with the low-level `HypergraphBrain`.
- **Mechanism**: Uses a static map to associate `HypergraphBrain` instances with `SubstrateLanguageAdapter`s, avoiding circular dependencies.

### SubstrateLanguageAdapter
- **Role**: Translates neural activity (spikes) into language tokens and vice versa.
- **Input**: Neural patterns -> Token Activation.
- **Output**: Token Selection -> Neural Stimulation (Motor Cortex for speech).

