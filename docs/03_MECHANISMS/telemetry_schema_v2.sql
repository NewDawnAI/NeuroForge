-- NeuroForge Telemetry Schema Snapshot (v2)
-- Adds adaptive reflection counters and optional mean learning rate

-- Baseline tables retained from v1 (refer to telemetry_schema_v1.sql)
-- substrate_states, learning_stats, affective_state, rewards remain unchanged

-- New aggregated table for adaptive reflection per run
CREATE TABLE IF NOT EXISTS adaptive_reflection (
    run_id INT NOT NULL,
    step_end INT,                         -- step at which aggregation was recorded (optional)
    adaptive_low_events INT DEFAULT 0,    -- count of low-coherence corrective actions
    adaptive_high_events INT DEFAULT 0,   -- count of high-coherence exploration nudges
    mean_learning_rate REAL,              -- optional average learning rate over the aggregation window
    ts DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS vision_foveation (
    run_id INT NOT NULL,
    step INT,
    enabled INT,
    mode TEXT,
    alpha REAL,
    x INT,
    y INT,
    w INT,
    h INT,
    ts DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Optional extension: add mean_learning_rate to learning_stats
-- (If you prefer embedding LR trends with learning cadence rather than a separate table)
-- ALTER TABLE learning_stats ADD COLUMN mean_learning_rate REAL;

-- Index suggestions
-- CREATE INDEX IF NOT EXISTS idx_adaptive_reflection_run ON adaptive_reflection(run_id);
-- CREATE INDEX IF NOT EXISTS idx_adaptive_reflection_ts ON adaptive_reflection(ts);

-- Notes
-- - v2 schema is additive and backward-compatible with v1.
-- - Choose either adaptive_reflection table or learning_stats column extension depending on analysis needs.
-- - Aggregation cadence should align with unified run summaries (e.g., every ~500 steps) or end-of-run reporting.
