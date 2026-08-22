# NeuroForge M7 Autonomy – Deep Analysis Report

## Subsystem Map
- HypergraphBrain: central orchestrator; owns `LearningSystem`, `AutonomousScheduler`, `SubstrateTaskGenerator`; manages processing threads, callbacks, and substrate modes (`Off|Mirror|Train|Native`).
- LearningSystem: computes `uncertainty`, `surprise`, `prediction_error`, and `intrinsic_motivation`; exposes training/scaffolding controls and integrates with substrate pathways.
- IntrinsicMotivationSystem: provides component signals (uncertainty, prediction error, novelty, curiosity, competence, exploration, meta-learning) and composite intrinsic reward; integrates with `LearningSystem` and `HypergraphBrain`.
- AutonomousScheduler: priority-based task orchestration; lifecycle (`initialize|start|pause|resume|stop|reset`), task management (schedule/cancel/suspend/resume), execution control, metrics, and callbacks.
- AutonomousTask types: `GoalTask`, `PlanTask`, `ActionTask`, `ReflectionTask`; driven by `TaskContext` (timestamp, cycle, `delta_time`, parameters, tag).
- SubstrateTaskGenerator: turns substrate and motivation state into tasks; config thresholds, generation methods (exploration, consolidation, self-reflection, adaptive goals), statistics, and adaptive updates.
- AutonomousLearningCycle: high-level loop coordinating goal→plan→action→reflection with optional scaffold elimination.
- ConnectivityManager & Region: structural substrate wiring and region processing hooks supporting learning and task execution.
- Language Subsystems: `SubstrateLanguageIntegration`, `SubstrateLanguageManager`, `SubstrateLanguageAdapter` bridge language signals with substrate for tokenization, proto-words, prosody, and grounding.

## Data Flow & Interactions
- Sensory/Modality intake → Regions process activations → `LearningSystem` updates attention/learning and computes autonomy signals.
- Motivation loop: `IntrinsicMotivationSystem` updates signals; `LearningSystem` exposes `getIntrinsicMotivation`, `calculateUncertaintySignal`, `calculatePredictionError`, and `updateIntrinsicMotivation`.
- Task generation: `HypergraphBrain.initializeSubstrateTaskGeneration()` creates `SubstrateTaskGenerator` which reads brain/learning state (`global_activation`, `learning_rate`, `intrinsic_motivation`, `uncertainty`, `prediction_error`, region activations, performance metrics) and, per `Config`, emits tasks to `AutonomousScheduler`.
- Scheduling & execution: `AutonomousScheduler.processScheduling/Execution()` selects next tasks (considering priority aging, deadlines, dependencies), runs them with `TaskContext`, and fires pre/post callbacks.
- Task outcomes: `SubstrateTaskGenerator.evaluateTaskOutcome()` updates adaptive parameters and statistics (success rates, average performance), feeding back into thresholds.
- Autonomous loop: `HypergraphBrain.runAutonomousLoop()` cycles goal selection → planning → execution → reflection; `executeAutonomousCycle(delta_time)` integrates scheduler and generator steps.
- Metrics & controls: `HypergraphBrain` exposes autonomy knobs (`curiosity_threshold_`, `uncertainty_threshold_`, `prediction_error_threshold_`, `max_concurrent_tasks_`, `task_generation_interval_`, `autonomy_metrics_enabled_`, `autonomy_target_`) and mode switches.

## Key APIs by Subsystem
- HypergraphBrain: `initializeAutonomousScheduler()`, `setAutonomousModeEnabled(bool)`, `addAutonomousTask(TaskPtr)`, `executeAutonomousCycle(float)`, `getAutonomousStatistics()`, `initializeSubstrateTaskGeneration(Config)`, `setSubstrateTaskGenerationEnabled(bool)`, `runAutonomousLoop(...)`, `setSubstrateMode(SubstrateMode)`.
- LearningSystem: `calculateUncertaintySignal()`, `calculateSurpriseSignal(state)`, `calculatePredictionError(pred, actual)`, `getIntrinsicMotivation()`, `updateIntrinsicMotivation(state)`, substrate/scaffold controls (`setSubstrateTrainingMode`, `setScaffoldElimination`, `setMotivationDecay`).
- IntrinsicMotivationSystem: `updateMotivation(...)`, component calculators, `generateIntrinsicReward(...)`, `detectSurprise(...)`, trackers, config getters/setters, `isActive()`, `getMotivationHistory()`.
- AutonomousScheduler: `initialize|start|pause|resume|stop|reset`, `scheduleTask`, `cancelTask`, `suspendTask`, `resumeTask`, `getTask`, `getTasksByStatus/Priority`, `processScheduling/Execution`, `addPre/PostExecutionCallback`, metrics toggles, and convenience `scheduleGoal/Plan/Action/Reflection`.
- AutonomousTask & TaskContext: execution contract `execute(const TaskContext&)`, dependency management, timing/deadline helpers; `TaskContext` carries timestamp, cycle, `delta_time`, parameter map, tag.
- SubstrateTaskGenerator: `initialize|shutdown`, `generateTasks(delta_time)`, `updateSubstrateContext()`, `shouldGenerateTasks()`, generators (`generateExplorationTask`, `generateConsolidationTask`, `generateSelfReflectionTask`, `generateAdaptiveGoal`), calculators (`calculateIntrinsicMotivation`, `calculateUncertaintyLevel`, `calculatePredictionError`), `evaluateTaskOutcome`, config/statistics, `setActive|isActive`.
- AutonomousLearningCycle: `initialize|shutdown`, `executeOneCycle(delta_time)`, scaffold elimination types and config.

## Dependencies & Ownership
- Brain owns (`unique_ptr`) `LearningSystem`, `AutonomousScheduler`, `SubstrateTaskGenerator`; holds atomic autonomy parameters and mode/state flags.
- `SubstrateTaskGenerator` holds `shared_ptr` to `HypergraphBrain` and `AutonomousScheduler` to avoid cycles while coordinating generation and scheduling.
- `AutonomousScheduler` references `HypergraphBrain*` for context; manages task maps, queues, and statistics with mutexes and atomics.
- `LearningSystem` integrates `IntrinsicMotivationSystem` and provides autonomy-related methods consumed by generator and brain.

## Concurrency & Callbacks
- Brain processing threads with pre/post processing callbacks and spike observers; scheduler has pre/post execution callbacks.
- Atomics guard state (`is_running`, `is_paused`, metrics flags, thresholds); mutexes protect queues, stats, and contexts.

## Observations & Considerations
- Priority aging and deadline enforcement balance exploration vs. timely execution.
- Adaptive generation continuously tunes thresholds by outcomes; avoid resource contention via `checkResourceConstraints()`.
- Scaffold elimination toggles drive the transition toward substrate-native autonomy.
- Substrate modes allow staged operation: mirroring inputs, training, or native autonomous behavior.

## Integration Hotspots
- Brain ↔ Scheduler: `executeAutonomousCycle`, statistics, add tasks.
- Brain ↔ Generator: initialization, enablement, context updates, task emissions.
- Generator ↔ LearningSystem: pull prediction error, motivation, and uncertainty.
- Scheduler ↔ Tasks: `TaskContext` propagation and completion callbacks for outcome evaluation.