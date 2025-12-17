# Sensory-to-Learning Flow Mapping (processVisualInput → setActivation() → neuron->process() → LearningSystem)

This document maps the end-to-end flow of sensory input driving learning updates in NeuroForge.

High-level sequence
- Sensory features generated and dispatched to a cortical region
- Region sets neuron activations from features
- Step processing calls neuron->process(delta_time) for each neuron
- Threshold crossings emit spike callbacks
- Brain’s LearningSystem receives spike times, then performs Hebbian and/or STDP updates
- Stats are accumulated and printed

1) Sensory input dispatch (main loop)
- Vision path: features are encoded and passed to the visual region
- Audio path: features are encoded and passed to the auditory region
- After input injection, the brain advances one processing step (processStep)

Files/locations
- src/main.cpp: within the main simulation loop
  - visual_region->processVisualInput(features);
  - auditory_region->processAudioInput(features);
  - brain.processStep(dt);

2) Region input handling → setActivation()
- VisualCortex::processVisualInput: clamps feature values, calls neuron->setActivation(v) and resets state to Inactive to allow a fresh threshold crossing during processing
- AuditoryCortex::processAudioInput: mirrors the same pattern for auditory features

Files/locations
- src/regions/CorticalRegions.cpp
  - VisualCortex::processVisualInput(...)
    - neurons[i]->setActivation(v);
    - neurons[i]->setState(Core::Neuron::State::Inactive);
  - AuditoryCortex::processAudioInput(...)
    - neurons[i]->setActivation(v);
    - neurons[i]->setState(Core::Neuron::State::Inactive);

3) Per-step region processing → neuron->process()
- Regions iterate their neurons and invoke neuron->process(delta_time)
- VisualCortex::processRegionSpecific and AuditoryCortex::processRegionSpecific do this each step

Files/locations
- src/regions/CorticalRegions.cpp
  - VisualCortex::processRegionSpecific(float)
  - AuditoryCortex::processRegionSpecific(float)

4) Neuron dynamics and spike emission
- Neuron::process(delta_time):
  - Accumulates weighted inputs
  - Applies decay and clamps activation
  - setActivation(new_activation)
  - If activation crosses threshold and previous state wasn’t Active: setState(Active), increment fire count, invoke global spike callback, propagate to outputs, then enter Refractory

Files/locations
- src/core/Neuron.cpp
  - Neuron::setActivation(ActivationValue)
  - Neuron::process(float)

5) Wiring spikes to LearningSystem and driving updates
- HypergraphBrain::initializeLearning(config): installs a global Neuron::setSpikeCallback that forwards spikes to LearningSystem::onNeuronSpike(neuron_id, now)
- HypergraphBrain::processStep(delta_time): after regions process, calls learning_system->updateLearning(delta_time) if enabled

Files/locations
- src/core/HypergraphBrain.cpp
  - HypergraphBrain::initializeLearning(...)
    - Neuron::setSpikeCallback([...]{ learning_system->onNeuronSpike(...); })
  - HypergraphBrain::processStep(float)
    - learning_system_->updateLearning(delta_time);

6) LearningSystem update loop
- LearningSystem::updateLearning(delta_time):
  - Hebbian: for each region, applyHebbianLearning(rid, hebbian_rate) if enabled
  - STDP: snapshot recent spike times and call applySTDPLearning(synapses, spike_times) if enabled
  - Periodic consolidation and stats maintenance

Files/locations
- src/core/LearningSystem.cpp
  - LearningSystem::updateLearning(float)
  - LearningSystem::applyHebbianLearning(RegionID, float)
  - LearningSystem::applySTDPLearning(const std::vector<SynapsePtr>&, const std::unordered_map<NeuronID, TimePoint>&)

7) Synaptic plasticity rules
- Hebbian: Δw = η · pre_activation · post_activation · Δt
- STDP: LTP/LTD based on temporal order; exponential decay with timing difference

Files/locations
- src/core/Synapse.cpp
  - Synapse::applyHebbianLearning(float, float, float)
  - Synapse::applySTDP(TimePoint, TimePoint)

8) Statistics reporting
- At the end of the run, main prints learning statistics from brain.getLearningStatistics()

Files/locations
- src/main.cpp: “Learning System Statistics” section

Notes
- The sensory-to-spike linkage relies on regions writing features into neuron activations (setActivation) and resetting state, followed by neuron->process to detect threshold crossings and emit spikes.
- LearningSystem consumes spike timestamps (for STDP) and current activations (for Hebbian) and updates weights accordingly.

9) Phase-4 Reward-Modulated Plasticity (three-factor rule)
- Eligibility traces: on spikes/pre-post events, LearningSystem maintains per-synapse eligibility e_ij with e_ij(t+Δt) = λ·e_ij(t) + η_elig·pre_i·post_j (configured via --lambda and --eta-elig). HypergraphBrain installs a Neuron spike callback to LearningSystem::onNeuronSpike and also calls notePrePost to wire pre/post activity for eligibility updates.
- Shaped reward: the system can compute a shaped reward R_t using α·novelty + γ·task_reward − η·uncertainty (+ optional mimicry term), and clamps R_t to [-2, 2].
- External reward: modules (e.g., the maze demo) call LearningSystem::applyExternalReward(r). Rewards accumulate into a pending buffer and are consumed during updateLearning.
- Reward-modulated update: during updateLearning, the pending reward R is distributed over eligible synapses with Δw_ij = κ·R·e_ij, after which eligibility traces decay. Periodic consolidation and statistics updates continue to run as before.

Files/locations (non-exhaustive)
- src/core/HypergraphBrain.cpp: installs Neuron::setSpikeCallback that forwards to LearningSystem::onNeuronSpike; calls LearningSystem::notePrePost; logs rewards/episodes to MemoryDB when enabled.
- src/core/LearningSystem.cpp: configurePhase4(...), onNeuronSpike(...), notePrePost(...), applyExternalReward(...), updateLearning(...), computeShapedReward(...).
- src/main.cpp: CLI flag parsing for --enable-learning, Phase-4 params (--lambda, --eta-elig, --kappa, --alpha, --gamma, --eta), maze demo flags (--maze-demo, --maze-view, --maze-view-interval, --maze-shaping, --maze-shaping-k, --maze-shaping-gamma), and MemoryDB flags (--memory-db, --memdb-debug, --list-runs, --list-episodes, --recent-rewards).

9a) Shaped reward details and computation path
- Observation proxy: the main loop constructs an observation vector from the concatenated region activations that are currently active (e.g., visual, auditory, maze observation, and maze action regions). This becomes both obs and acts inputs to LearningSystem::computeShapedReward to keep integration minimal. See implementation in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.
- Novelty term: computeShapedReward maintains an exponential moving average of recent observations and computes novelty via cosine similarity to that running mean; higher novelty increases reward. See code in <mcfile name="LearningSystem.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\core\LearningSystem.cpp"></mcfile>.
- Uncertainty term: the system estimates variability (a variance-like proxy) and subtracts η·uncertainty from reward, discouraging volatile states. See <mcfile name="LearningSystem.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\core\LearningSystem.cpp"></mcfile>.
- Mimicry term (optional): if mimicry is enabled and teacher/student embeddings are set, a cosine similarity term is computed between teacher_embed and student_embed and added as μ·similarity to the shaped reward. See <mcfile name="LearningSystem.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\core\LearningSystem.cpp"></mcfile>.
- Clamp and accumulation: the shaped reward is clamped to a bounded range before being accumulated into the pending reward buffer via applyExternalReward; updateLearning consumes this buffer to modulate synaptic updates. See <mcfile name="LearningSystem.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\core\LearningSystem.cpp"></mcfile> and the integration call site in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.

9b) Shaped reward integration in the main loop
- The loop periodically (on a wall-clock cadence) computes a task_reward from recent spikes (and maze reward when the maze demo is active), builds region_acts/obs, calls computeShapedReward(obs, acts, task_reward), and immediately forwards it to LearningSystem via applyExternalReward(shaped_reward). See the logging block that also writes shaped reward events to MemoryDB in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.
- MemoryDB logging context for shaped rewards includes the following JSON keys: spikes, window_ms, maze_reward, task, shaped, obs_dim, acts_dim. This helps post-hoc analysis correlate shaped reward with activity.

10) CSV exports and logging (formats confirmed)
- Episode CSV: header
  episode_index,steps,return,time_ms,success
  These are appended at episode end. See writer code in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.
- Synapse snapshot CSV (snapshot-csv/snapshot-live): header
  pre_neuron,post_neuron,weight
  Emitted either once at end-of-run or periodically, controlled by snapshot-interval. See generation in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.
- Recent spikes CSV (spikes-live): header
  neuron_id,t_ms
  The file is periodically overwritten with the most recent spikes within a TTL window; the viewer overlays these on the live graph. Writer in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>, reader in <mcfile name="ViewerMain.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\viewer\ViewerMain.cpp"></mcfile>.

12) Optional Phase-5 mimicry hooks (API overview)
- Enabling and weighting mimicry: call LearningSystem::setMimicryEnabled(true) and LearningSystem::setMimicryWeight(mu) to activate the mimicry term in reward shaping. See <mcfile name="LearningSystem.h" path="c:\Users\ashis\Desktop\NeuroForge\include\core\LearningSystem.h"></mcfile>.
- Providing embeddings: periodically provide a teacher embedding via setTeacherVector(...) and a student embedding via setStudentEmbedding(...). When enabled, computeShapedReward adds μ·cosine(teacher, student) to the shaped reward and records the last similarity internally for diagnostics. See <mcfile name="LearningSystem.h" path="c:\Users\ashis\Desktop\NeuroForge\include\core\LearningSystem.h"></mcfile> and <mcfile name="LearningSystem.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\core\LearningSystem.cpp"></mcfile>.
- Integration guidance: the main loop already computes obs/acts and shaped reward; to exploit mimicry, supply embeddings at the cadence appropriate to your demonstration data source before calling computeShapedReward. If mimicry is disabled or embeddings are missing/mismatched, the mimicry term gracefully contributes 0.

- Episode CSV: when enabled via --episode-csv=PATH, each finished episode appends a row with header:
  episode_index,steps,return,time_ms,success
  where success is 1 for success and 0 otherwise. See writer code in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.
- Synapse snapshot CSV: end-of-run export via --snapshot-csv=PATH, or periodic export via --snapshot-live=PATH together with --snapshot-interval=MS. The CSV header is:
  pre_neuron,post_neuron,weight
  which the 3D viewer also consumes for graph construction. See export and scheduling in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.
- Recent spikes CSV (live overlay): when --spikes-live=PATH is set, the process periodically overwrites a CSV file with header:
  neuron_id,t_ms
  where t_ms is the age of the most recent spike for each neuron in milliseconds. This format is expected by the viewer; see loader code in <mcfile name="ViewerMain.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\viewer\ViewerMain.cpp"></mcfile> and writer in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.
- MemoryDB logging (optional): when --memory-db=PATH is enabled, episodes and rewards are persisted; episode success and returns are also upserted alongside CSV logging.

11) Live 3D viewer (optional)
- The viewer can be launched alongside the simulation using --viewer. You can override the path with --viewer-exe=PATH if auto-detection fails. The viewer consumes:
  - a live synapse snapshot CSV via --snapshot-live=PATH (emitted by the main process), with optional refresh cadence set by --snapshot-interval=MS.
  - an optional spikes overlay via --spikes-live=PATH (emitted by the main process), using the neuron_id,t_ms format described above.
- Display configuration:
  - --viewer-layout=MODE selects layout (e.g., force-directed, grid) as supported by the viewer.
  - --viewer-refresh-ms=MS controls how often the viewer reloads CSVs.
  - --viewer-threshold=T filters edges by weight to declutter the graph.
- Implementation references: viewer process and loaders live in <mcfile name="ViewerMain.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\viewer\ViewerMain.cpp"></mcfile>, while CSV generation and process launching are orchestrated in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.

13) Policy selection and exploration flags (maze demo)
- --qlearning[=on|off]: when enabled, prepares a tabular Q baseline; if no --hybrid-lambda is provided, action selection uses pure Q-values; otherwise, it blends neural preferences with Q-values. Parsing and defaults live in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.
- --hybrid-lambda=F in [0,1]: computes scores[a] = F · prefs[a] + (1−F) · qvals[a] when Q-learning is active; with F=1 you get neural-only control, F=0 yields Q-only control. See unified action-selection block in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.
- --epsilon=F in [0,1]: enables epsilon-greedy exploration only when Q-learning baseline is active; with probability ε, a random action is sampled, otherwise the greedy choice from scores is used. This branch takes precedence over softmax when active. See exploration logic in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.
- --softmax-temp=F > 0: enables stochastic softmax sampling over the current scores with temperature F when epsilon-greedy is not active; uses a numerically stable softmax (subtracting max). See policy sampling in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.

Notes
- prefs come from the MazeAction region’s neuron activations; qvals come from the tabular Q-table keyed by agent state. The final policy selects greedily from blended scores, then optionally applies exploration per the flags above. See the action preference construction, blending, and sampling in <mcfile name="main.cpp" path="c:\Users\ashis\Desktop\NeuroForge\src\main.cpp"></mcfile>.