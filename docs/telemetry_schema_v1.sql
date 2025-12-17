-- NeuroForge Telemetry Schema Snapshot (v1)
-- Frozen for Level-4 self-observation baselining

-- Substrate states (Phase C / core substrate)
CREATE TABLE IF NOT EXISTS substrate_states (
    run_id INT NOT NULL,
    step INT NOT NULL,
    avg_coherence REAL,
    assemblies INT,
    bindings INT,
    ts DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Learning statistics (LearningSystem aggregate cadence)
CREATE TABLE IF NOT EXISTS learning_stats (
    run_id INT NOT NULL,
    step INT NOT NULL,
    total_updates INT,
    avg_weight_change REAL
);

-- Affective state (SurvivalBias / hazard modulation)
CREATE TABLE IF NOT EXISTS affective_state (
    run_id INT NOT NULL,
    step INT NOT NULL,
    hazard_alpha REAL,
    hazard_beta REAL,
    modulation REAL
);

-- Rewards (decoupled reward cadence)
CREATE TABLE IF NOT EXISTS rewards (
    run_id INT NOT NULL,
    step INT NOT NULL,
    reward REAL
);

-- Optional: hippocampal snapshots for episodic traceability
-- CREATE TABLE IF NOT EXISTS hippocampal_snapshots (
--     run_id INT NOT NULL,
--     step INT NOT NULL,
--     snapshot_data TEXT,
--     ts DATETIME DEFAULT CURRENT_TIMESTAMP
-- );

-- Optional: episodes table for run segmentation
-- CREATE TABLE IF NOT EXISTS episodes (
--     run_id INT NOT NULL,
--     episode_id INT NOT NULL,
--     start_ts DATETIME,
--     end_ts DATETIME,
--     notes TEXT
-- );

-- Index suggestions (optional)
-- CREATE INDEX IF NOT EXISTS idx_substrate_states_run_step ON substrate_states(run_id, step);
-- CREATE INDEX IF NOT EXISTS idx_learning_stats_run_step ON learning_stats(run_id, step);
-- CREATE INDEX IF NOT EXISTS idx_affective_state_run_step ON affective_state(run_id, step);
-- CREATE INDEX IF NOT EXISTS idx_rewards_run_step ON rewards(run_id, step);
