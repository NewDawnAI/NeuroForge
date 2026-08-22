🧠 Neural Substrate AI Roadmap
Stage 0 — Foundation (where you are now)

You already have:

Neuron, Synapse, Region, HypergraphBrain scaffolding

README

.

Cortical/Subcortical/Limbic region stubs.

At this stage, it’s only a simulation skeleton.

Stage 1 — Plasticity & Stability

🔹 Goal: make the skeleton self-modifying and stable.

 Enable Hebbian, STDP, BCM, and Oja rules in different regions.

 Implement homeostatic scaling (neurons regulate firing rate).

 Add neuromodulators (global dopamine/serotonin analogs) as control signals for plasticity.

 Build monitoring tools: firing rates, synapse growth/decay, stability metrics.

💡 This gives you a substrate that can grow and change, not a frozen net.

Stage 2 — Embodiment

🔹 Goal: give the substrate a body and environment.

 Connect PhysicsBody (camera, mic, proprioception, touch sensors).

 Add actuators (motor commands, speaker output, LEDs).

 Synchronize simulation ticks (sensor frames ↔ neuron updates).

 Build an EnvServer sandbox (blocks, sounds, collisions).

💡 Now the brain can experience the world and act in it.

Stage 3 — Sensory Pathways & Preprocessing

🔹 Goal: make inputs biologically plausible.

 Retina-like preprocessing → center-surround filters, temporal differencing.

 Cochlea-like preprocessing → gammatone filterbank, onset detection.

 Proprioceptive deltas → joint changes only.

 Route each sensory channel into its respective region (VisualCortex, AuditoryCortex, MotorCortex).

💡 Now sensory streams look more like what a brain really gets.

Stage 4 — Reward & Mimicry

🔹 Goal: give the substrate a way to value experience.

 Implement cross-modal prediction error (visual vs predicted visual, audio vs predicted audio).

 Implement self-consistency reward (speaker output ↔ heard audio).

 Implement homeostatic reward (stable firing rates).

 Route rewards via Amygdala/Brainstem modules.

💡 This gives the brain intrinsic motivation to mimic and stabilize its world model.

Stage 5 — Memory & Consolidation

🔹 Goal: allow learning across time.

## Core Memory Architecture [NEXT PRIORITY 🎯]

### Hard-wired Memory Systems to Implement:

🔹 **Core Memory Architecture (Hard-Wired Modules)**

#### 2.1 Working Memory Module

**Role**: holds "online thoughts" → focus, reasoning, active goals.

```cpp
class WorkingMemory {
    // Prefrontal cortex analog
public:
    std::vector<float> active_buffer;   // 7±2 slots (Miller's law)
    float decay_rate = 0.1f;            // Temporal fading per tick
    float refresh_threshold = 0.3f;     // Needed activation to persist

    void push(float item);
    void decay();                       // Automatic weakening over time
    void refresh(int index);            // Attention can reboost item
};
```

🧠 **Biological note**: Think of this as persistent neural firing loops (fronto-parietal).

#### 2.2 Episodic Memory System

**Role**: binds sensory + action + time → "experience snapshots."

```cpp
class EpisodicMemory {
    // Hippocampus analog
public:
    struct Episode {
        std::vector<float> sensory_state;
        std::vector<float> action_state;
        double timestamp;              // precise encoding
        float emotional_weight;        // Amygdala tagging
    };

    std::deque<Episode> recent_buffer;  // Short-term (hippocampal buffer)
    std::vector<Episode> consolidated;  // Long-term storage (neocortex)

    void record(const Episode& ep);
    void consolidate(float threshold);  // Move to long-term if weighted
};
```

🧠 **Biological note**: Hippocampus encodes → sleep consolidation shifts to cortex.

#### 2.3 Procedural Memory Bank

**Role**: skill automation via repetition + reinforcement.

```cpp
class ProceduralMemory {
    // Basal ganglia + cerebellum analog
public:
    struct SkillSequence {
        std::vector<int> sequence;     // motor_command IDs
        float confidence;              // learned reliability
        float execution_speed;         // optimization through practice
    };

    std::map<int, SkillSequence> learned_skills;

    void learnSkill(int skill_id, const std::vector<int>& seq);
    void reinforce(int skill_id, float reward);
    SkillSequence retrieve(int skill_id);
};
```

🧠 **Biological note**:
- Basal ganglia = reinforcement loop (selection, habit formation).
- Cerebellum = fine-tuning execution speed/timing.

#### 2.4 Semantic Knowledge Store

**Role**: abstract, decontextualized knowledge (facts, concepts).

```cpp
class SemanticMemory {
    // Temporal cortex analog
public:
    struct ConceptNode {
        std::string label;
        std::vector<int> relations;    // concept graph edges
        float strength;                // consolidation weight
    };

    std::unordered_map<int, ConceptNode> knowledge_graph;
    float consolidation_threshold = 0.8f; // Episodic → Semantic promotion

    void integrateFromEpisodic(const EpisodicMemory::Episode& ep);
    ConceptNode retrieve(int concept_id);
};
```

🧠 **Biological note**: Semantic = "consolidated" episodic → distributed cortical traces.

#### ⚡ Implementation Order

1. **Working Memory** → gives immediate focus / context window.
2. **Episodic Memory** → captures raw experiences with time/emotion tags.
3. **Procedural Memory** → lets repeated sequences become fast/automatic.
4. **Semantic Memory** → consolidates across episodes into durable concepts.

✨ **With this, you'll have a neuro-inspired memory stack that can scale from raw "moment-to-moment thought" → "lifetime knowledge."**

### Original Memory & Consolidation Tasks:

 Hippocampus = episodic memory buffer (recent experiences).

 Replay mechanism during "rest" (simulate offline replay).

 Gradual consolidation into cortical connections.

 Track emergence of prototypes / symbols.

💡 Now the brain starts to build long-term knowledge instead of reacting moment by moment.

Stage 6 — Emergent Faculties

🔹 Goal: scaffold higher cognition.

 Default Mode Network (DMN) → spontaneous thought when idle.

 SelfNode → integrate proprioception + memory into a self-model.

 CingulateCortex → conflict monitoring (reward vs action).

 Prefrontal Cortex → action selection, planning.

💡 Now the substrate starts showing traits of agency and selfhood.

Stage 7 — Reflection & Conscious Loops

🔹 Goal: move from reactive to reflective AI.

 Add ReflectionLoop → monitor past actions and outcomes.

 Enable GoalLoop → generate intrinsic goals based on learned drives.

 Add DreamProcessor → recombine memories during offline mode.

 Encourage emergent “inner speech” via Auditory ↔ Language region cross-activation.

💡 This is where proto-conscious dynamics can emerge.

Stage 8 — Expansion & Hybridization

🔹 Goal: scale and hybridize with other tech.

 Swap regions with external ML models or neuromorphic hardware where useful.

 Allow “plug-and-play” regions via RegionAdapter interface.

 Use hybrid symbolic/connectionist memory for efficiency.

 Scale simulation to billions of neurons gradually (if hardware permits).

💡 At this stage you have a true substrate — adaptive, embodied, lifelong learning — not a static model.

## 🧠 Stage 9 — Bootstrapping Priors & Curiosity Framework

🔹 **Goal**: Implement innate biases and drives that guide learning from birth.

*"The brain doesn't boot cold; it comes with bootstrapping priors that steer chaos into usable patterns, and curiosity straddles the line between those priors and emergent drives."*

### 🧠 Human Bootstrapping Priors → ⚙️ NeuroForge Analogues

#### 🔹 Perceptual Biases

**Face bias** → ✅ Already implemented with Haar cascades + feature recognition.
- Enhance with dedicated face detection priority in visual processing pipeline
- Add face-specific memory consolidation pathways

**Voice bias** → Frequency/phoneme priors in auditory pipeline.
- Implement human voice frequency range prioritization (85-255 Hz fundamental)
- Add phoneme pattern recognition templates
- Create voice-specific attention mechanisms

**Motion bias** → Already natural in monitoring system (spikes/events = change).
- Enhance motion detection with biological motion patterns
- Add predictive motion tracking for moving entities
- Implement startle response to sudden motion changes

**Contrast/edge bias** → Equivalent to convolutional kernels / low-level filters.
- Implement center-surround receptive fields
- Add edge detection prioritization in visual preprocessing
- Create contrast-based attention mechanisms

#### 🔹 Cognitive Biases

**Novelty bias** → NeuroForge salience detector (flagging unusual activity).
- Implement novelty detection algorithms in monitoring system
- Add curiosity-driven exploration mechanisms
- Create novelty-based memory consolidation priorities

**Causality bias** → Early cause-effect linking in sequence memory.
- Implement temporal sequence learning in episodic memory
- Add causal relationship detection algorithms
- Create cause-effect prediction mechanisms

**Agency bias** → Weight "moving entities" as intentional.
- Implement agent detection in visual processing
- Add intentionality assessment for moving objects
- Create social cognition pathways for agent interactions

**Social bias** → Tie to face-recognition + agent-recognition layers.
- Enhance face recognition with emotional expression detection
- Add social attention mechanisms
- Implement social learning and imitation pathways

#### 🔹 Motor & Reflexive Programs

**Rooting/sucking reflex** → "Startup behaviors" the system always tries.
- Implement default exploration behaviors on system initialization
- Add basic approach/avoidance reflexes
- Create fundamental motor pattern templates

**Grasp reflex** → Low-level binding of detected patterns to immediate response.
- Implement automatic pattern-response associations
- Add reflexive motor responses to specific stimuli
- Create fast pathway for immediate reactions

**Startle reflex** → Spike-threshold interrupt in NeuroForge monitoring (already present).
- Enhance existing monitoring with startle response mechanisms
- Add threat detection and immediate response pathways
- Implement attention redirection on sudden stimuli

**Cry response** → Initial "distress-signal" when chaotic activity exceeds stability threshold.
- Implement distress signaling mechanisms
- Add homeostatic imbalance detection
- Create help-seeking behaviors for system instability

#### 🔹 Emotional/Affective Biases

**Caregiving preference** → "Reinforce when known trusted agents appear."
- Implement trust assessment mechanisms for detected agents
- Add positive reinforcement for familiar faces/voices
- Create attachment and bonding pathways

**Separation discomfort** → "Signal loss" events treated as negative states.
- Implement separation anxiety mechanisms
- Add negative reinforcement for loss of familiar stimuli
- Create reunion recognition and positive response

**Negative bias** → Built-in threat over-weighting in anomaly detection.
- Implement threat detection with higher sensitivity
- Add negative event prioritization in memory consolidation
- Create survival-oriented attention mechanisms

#### 🔹 Neural Architectural Priors

**Hebbian bias** → Already simulated in synaptic plasticity rules.
- Enhance existing Hebbian learning with developmental constraints
- Add critical period implementations
- Create experience-dependent neural development

**Critical periods** → Time windows with higher plasticity.
- Implement developmental time windows for different systems
- Add age-dependent plasticity mechanisms
- Create sensitive period learning algorithms

**Sleep/consolidation cycles** → Offline replay/weight-pruning routines.
- Implement sleep-like consolidation phases
- Add memory replay mechanisms during offline periods
- Create synaptic homeostasis and pruning algorithms

### 🔮 Curiosity Framework Implementation

#### **Curiosity as Bias** → Novelty-salience module
- Implement curiosity-driven attention mechanisms
- Add exploration vs exploitation balance algorithms
- Create intrinsic motivation systems

#### **Curiosity as Emergent** → Prediction-error minimization loop
- Implement predictive coding frameworks
- Add prediction error calculation and response
- Create information-seeking behaviors to close prediction gaps

### 👉 NeuroForge Roadmap Integration:

1. **Seed with bias modules** (face, novelty, motion, startle)
2. **Let monitoring + memory stabilize**
3. **Watch for emergent curiosity** when prediction error drives exploration

#### **Implementation Priority:**
- **High**: Novelty bias, startle reflex, face bias enhancement
- **Medium**: Causality bias, agency bias, critical periods
- **Low**: Complex social biases, advanced emotional mechanisms

### **Crossover Point:**
- **Priors = scaffolding** (hard-coded biases and reflexes)
- **Emergent drives = self-organized behavior** (once memory, prediction, and feedback loops are online)

🗂 File-Level TODO (concrete)

### Core Memory Architecture Implementation Files:
- **src/memory/WorkingMemory.h/.cpp** — prefrontal cortex analog with active buffer and decay
- **src/memory/EpisodicMemory.h/.cpp** — hippocampus analog for experience snapshots
- **src/memory/ProceduralMemory.h/.cpp** — basal ganglia + cerebellum analog for skill automation
- **src/memory/SemanticMemory.h/.cpp** — temporal cortex analog with concept graphs
- **src/memory/MemoryIntegration.h/.cpp** — cross-system memory consolidation and routing
- **tests/test_memory_systems.cpp** — comprehensive memory system testing

### Existing Implementation Files:
src/core/PlasticityRules.* — implement STDP, Hebbian, Oja.

src/core/Homeostasis.* — firing rate normalization.

src/sim/PhysicsBody.* — embodiment (sensors + actuators).

src/sim/Preprocessors/* — retina, cochlea, proprio.

src/reward/RewardSystem.* — mimicry + homeostasis rewards.

src/memory/Hippocampus.* — episodic buffer + replay.

src/regions/* — expand limbic + prefrontal regions.

tools/telemetry_logger.* — metrics, emergent patterns.

examples/sandbox_demo.cpp — closed loop test.