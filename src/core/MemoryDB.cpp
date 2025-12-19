#include "core/MemoryDB.h"
#include <chrono>
#include <sstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>

#ifdef NF_HAVE_SQLITE3
#include <sqlite3.h>
#endif

namespace NeuroForge {
namespace Core {

static inline std::int64_t now_ms() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

MemoryDB::MemoryDB(const std::string& path) : path_(path) {}
MemoryDB::~MemoryDB() { close(); }

void MemoryDB::setDebug(bool enabled) noexcept { debug_ = enabled; }
bool MemoryDB::getDebug() const noexcept { return debug_; }

bool MemoryDB::open() {
#ifdef NF_HAVE_SQLITE3
    std::lock_guard<std::mutex> lg(m_);
    if (db_) return true;
    sqlite3* pdb = nullptr;
    if (sqlite3_open(path_.c_str(), &pdb) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] sqlite3_open failed for '" << path_ << "': " << (pdb ? sqlite3_errmsg(pdb) : "unknown") << std::endl;
        return false;
    }
    db_ = pdb;
    return ensureSchema();
#else
    return false;
#endif
}

void MemoryDB::close() {
#ifdef NF_HAVE_SQLITE3
    std::lock_guard<std::mutex> lg(m_);
    if (db_) {
        sqlite3_close(static_cast<sqlite3*>(db_));
        db_ = nullptr;
    }
#endif
}

bool MemoryDB::isOpen() const noexcept {
#ifdef NF_HAVE_SQLITE3
    return db_ != nullptr;
#else
    return false;
#endif
}

bool MemoryDB::exec(const char* sql) {
#ifdef NF_HAVE_SQLITE3
    char* errmsg = nullptr;
    int rc = sqlite3_exec(static_cast<sqlite3*>(db_), sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] exec failed rc=" << rc << " sql=" << sql << " err=" << (errmsg ? errmsg : "") << std::endl;
        if (errmsg) sqlite3_free(errmsg);
        return false;
    }
    if (debug_) std::cerr << "[MemoryDB] exec ok sql=" << sql << std::endl;
    return true;
#else
    (void)sql;
    return false;
#endif
}

bool MemoryDB::ensureSchema() {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    const char* schema_sql =
        "PRAGMA journal_mode=WAL;"
        "CREATE TABLE IF NOT EXISTS runs ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  started_ms INTEGER NOT NULL,"
        "  metadata_json TEXT"
        ");"
        "CREATE TABLE IF NOT EXISTS run_events ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  type TEXT NOT NULL,"
        "  message TEXT,"
        "  exit_code INTEGER,"
        "  rss_mb REAL,"
        "  gpu_mem_mb REAL,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_run_events_run ON run_events(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_run_events_ts ON run_events(ts_ms);"
        "CREATE TABLE IF NOT EXISTS learning_stats ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  processing_hz REAL NOT NULL,"
        "  total_updates INTEGER,"
        "  hebbian_updates INTEGER,"
        "  stdp_updates INTEGER,"
        "  reward_updates INTEGER,"
        "  avg_weight_change REAL,"
        "  consolidation_rate REAL,"
        "  active_synapses INTEGER,"
        "  potentiated_synapses INTEGER,"
        "  depressed_synapses INTEGER,"
        "  avg_energy REAL,"
        "  metabolic_hazard REAL,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS experiences ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  tag TEXT,"
        "  input_json TEXT,"
        "  output_json TEXT,"
        "  significant INTEGER,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS episodes ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  name TEXT NOT NULL,"
        "  start_ms INTEGER NOT NULL,"
        "  end_ms INTEGER,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS episode_experiences ("
        "  episode_id INTEGER NOT NULL,"
        "  experience_id INTEGER NOT NULL,"
        "  PRIMARY KEY(episode_id, experience_id),"
        "  FOREIGN KEY(episode_id) REFERENCES episodes(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(experience_id) REFERENCES experiences(id) ON DELETE CASCADE"
        ");"
        // Episode-level metrics table
        "CREATE TABLE IF NOT EXISTS episode_stats ("
        "  episode_id INTEGER PRIMARY KEY,"
        "  steps INTEGER NOT NULL DEFAULT 0,"
        "  success INTEGER NOT NULL DEFAULT 0,"
        "  episode_return REAL NOT NULL DEFAULT 0.0,"
        "  FOREIGN KEY(episode_id) REFERENCES episodes(id) ON DELETE CASCADE"
        ");"
        // New Phase 3 tables
        "CREATE TABLE IF NOT EXISTS reward_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  reward REAL NOT NULL,"
        "  source TEXT,"
        "  context_json TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS self_model ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  state_json TEXT,"
        "  confidence REAL,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        // Unified Self System tables
        "CREATE TABLE IF NOT EXISTS self_concept ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  identity_vector_json TEXT NOT NULL,"
        "  confidence REAL,"
        "  notes TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_self_concept_run ON self_concept(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_self_concept_ts ON self_concept(ts_ms);"
        "CREATE TABLE IF NOT EXISTS personality_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  trait_json TEXT NOT NULL,"
        "  proposal INTEGER NOT NULL DEFAULT 1,"
        "  approved INTEGER NOT NULL DEFAULT 0,"
        "  source_phase INTEGER,"
        "  revision_id INTEGER,"
        "  notes TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(revision_id) REFERENCES self_revision_log(id) ON DELETE SET NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_personality_history_run ON personality_history(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_personality_history_ts ON personality_history(ts_ms);"
        "CREATE INDEX IF NOT EXISTS idx_personality_history_proposal ON personality_history(proposal);"
        "CREATE INDEX IF NOT EXISTS idx_personality_history_approved ON personality_history(approved);"
        "CREATE TABLE IF NOT EXISTS social_self ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  role TEXT,"
        "  norm_json TEXT,"
        "  reputation REAL,"
        "  confidence REAL,"
        "  notes TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_social_self_run ON social_self(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_social_self_ts ON social_self(ts_ms);"
        "CREATE INDEX IF NOT EXISTS idx_social_self_role ON social_self(role);"
        // M6: Substrate state serialization tables
        "CREATE TABLE IF NOT EXISTS substrate_states ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  state_type TEXT NOT NULL,"  // "synapse_weights", "neuron_states", "hippocampal_snapshot"
        "  region_id TEXT NOT NULL,"
        "  serialized_data TEXT NOT NULL,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS hippocampal_snapshots ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  priority REAL NOT NULL,"
        "  significance REAL NOT NULL,"
        "  snapshot_data TEXT NOT NULL,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_substrate_states_type ON substrate_states(state_type);"
        "CREATE INDEX IF NOT EXISTS idx_substrate_states_region ON substrate_states(region_id);"
        "CREATE INDEX IF NOT EXISTS idx_substrate_states_ts ON substrate_states(ts_ms);"
        "CREATE INDEX IF NOT EXISTS idx_hippocampal_priority ON hippocampal_snapshots(priority DESC);"
        "CREATE INDEX IF NOT EXISTS idx_hippocampal_ts ON hippocampal_snapshots(ts_ms);"
        "CREATE TABLE IF NOT EXISTS embeddings ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  content_id TEXT,"
        "  state_type TEXT,"
        "  dim INTEGER,"
        "  vec_json TEXT,"
        "  meta_json TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_embeddings_run_step ON embeddings(run_id, step);"
        "CREATE INDEX IF NOT EXISTS idx_embeddings_content ON embeddings(content_id);"
        // Phase 6: Hybrid Reasoning Engine tables
        "CREATE TABLE IF NOT EXISTS options ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  source TEXT,"
        "  option_json TEXT NOT NULL,"
        "  confidence REAL,"
        "  selected INTEGER DEFAULT 0,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_options_run ON options(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_options_ts ON options(ts_ms);"
        "CREATE INDEX IF NOT EXISTS idx_options_selected ON options(selected);"
        "CREATE TABLE IF NOT EXISTS option_stats ("
        "  option_id INTEGER PRIMARY KEY,"
        "  evaluations INTEGER NOT NULL DEFAULT 0,"
        "  average_score REAL NOT NULL DEFAULT 0.0,"
        "  last_evaluated_ms INTEGER,"
        "  FOREIGN KEY(option_id) REFERENCES options(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS inferred_facts ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  fact_json TEXT NOT NULL,"
        "  confidence REAL,"
        "  derived_option_id INTEGER,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(derived_option_id) REFERENCES options(id) ON DELETE SET NULL"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_facts_run ON inferred_facts(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_facts_ts ON inferred_facts(ts_ms);"
        "CREATE TABLE IF NOT EXISTS verifications ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  fact_id INTEGER NOT NULL,"
        "  result TEXT,"
        "  contradiction INTEGER NOT NULL DEFAULT 0,"
        "  details_json TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(fact_id) REFERENCES inferred_facts(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_verifications_run ON verifications(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_verifications_fact ON verifications(fact_id);"
        // Context sampling log
        "CREATE TABLE IF NOT EXISTS context_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  sample REAL NOT NULL,"
        "  gain REAL NOT NULL,"
        "  update_ms INTEGER NOT NULL,"
        "  window INTEGER NOT NULL,"
        "  label TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_context_ts ON context_log(ts_ms);"
        "CREATE INDEX IF NOT EXISTS idx_context_run ON context_log(run_id);"
        // Peer context sampling log (Phase 17b extended)
        "CREATE TABLE IF NOT EXISTS context_peer_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  peer TEXT NOT NULL,"
        "  sample REAL NOT NULL,"
        "  gain REAL NOT NULL,"
        "  update_ms INTEGER NOT NULL,"
        "  window INTEGER NOT NULL,"
        "  label TEXT,"
        "  mode TEXT,"
        "  lambda REAL,"
        "  kappa REAL,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_context_peer_ts ON context_peer_log(ts_ms);"
        "CREATE INDEX IF NOT EXISTS idx_context_peer_run ON context_peer_log(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_context_peer_name ON context_peer_log(peer);"
        // Phase 7: Intent graph, affective state, reflections
        "CREATE TABLE IF NOT EXISTS intent_nodes ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  node_type TEXT NOT NULL,"
        "  state_json TEXT NOT NULL,"
        "  confidence REAL,"
        "  source TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS intent_edges ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  from_node_id INTEGER NOT NULL,"
        "  to_node_id INTEGER NOT NULL,"
        "  cause TEXT,"
        "  weight REAL,"
        "  details_json TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(from_node_id) REFERENCES intent_nodes(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(to_node_id) REFERENCES intent_nodes(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS affective_state ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  valence REAL,"
        "  arousal REAL,"
        "  focus REAL,"
        "  notes TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS reflections ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  title TEXT,"
        "  rationale_json TEXT NOT NULL,"
        "  impact REAL,"
        "  episode INTEGER,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_intent_nodes_run ON intent_nodes(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_intent_nodes_ts ON intent_nodes(ts_ms);"
        "CREATE INDEX IF NOT EXISTS idx_intent_edges_run ON intent_edges(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_intent_edges_ts ON intent_edges(ts_ms);"
        "CREATE INDEX IF NOT EXISTS idx_affective_state_run ON affective_state(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_affective_state_ts ON affective_state(ts_ms);"
        "CREATE INDEX IF NOT EXISTS idx_reflections_run ON reflections(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_reflections_ts ON reflections(ts_ms);"
        // Phase 8: Goal hierarchy and motivation state
        "CREATE TABLE IF NOT EXISTS goal_nodes ("
        "  goal_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  description TEXT NOT NULL,"
        "  priority REAL NOT NULL DEFAULT 0.5,"
        "  stability REAL NOT NULL DEFAULT 0.5,"
        "  origin_reflection_id INTEGER,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(origin_reflection_id) REFERENCES reflections(id) ON DELETE SET NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS goal_edges ("
        "  goal_id INTEGER NOT NULL,"
        "  subgoal_id INTEGER NOT NULL,"
        "  weight REAL NOT NULL DEFAULT 1.0,"
        "  PRIMARY KEY(goal_id, subgoal_id),"
        "  FOREIGN KEY(goal_id) REFERENCES goal_nodes(goal_id) ON DELETE CASCADE,"
        "  FOREIGN KEY(subgoal_id) REFERENCES goal_nodes(goal_id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS motivation_state ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  motivation REAL NOT NULL DEFAULT 0.5,"
        "  coherence REAL NOT NULL DEFAULT 0.5,"
        "  notes TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_goal_nodes_run ON goal_nodes(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_goal_nodes_desc ON goal_nodes(description);"
        "CREATE INDEX IF NOT EXISTS idx_motivation_state_run ON motivation_state(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_motivation_state_ts ON motivation_state(ts_ms);"
        // Phase 9: Metacognition tables
        "CREATE TABLE IF NOT EXISTS metacognition ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  self_trust REAL NOT NULL,"
        "  narrative_rmse REAL,"
        "  goal_mae REAL,"
        "  ece REAL,"
        "  notes TEXT,"
        "  trust_delta REAL,"
        "  coherence_delta REAL,"
        "  goal_accuracy_delta REAL,"
        "  self_explanation_json TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_metacognition_run ON metacognition(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_metacognition_ts ON metacognition(ts_ms);"
        "CREATE TABLE IF NOT EXISTS narrative_predictions ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  reflection_id INTEGER NOT NULL,"
        "  horizon_ms INTEGER NOT NULL,"
        "  predicted_coherence_delta REAL,"
        "  confidence REAL,"
        "  targets_json TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(reflection_id) REFERENCES reflections(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_narrative_pred_run ON narrative_predictions(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_narrative_pred_ts ON narrative_predictions(ts_ms);"
        "CREATE TABLE IF NOT EXISTS prediction_resolutions ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER,"
        "  ts_ms INTEGER,"
        "  prediction_id INTEGER,"
        "  observed_delta REAL,"
        "  result_json TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(prediction_id) REFERENCES narrative_predictions(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_pred_res_run ON prediction_resolutions(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_pred_res_ts ON prediction_resolutions(ts_ms);"
        "CREATE INDEX IF NOT EXISTS idx_pred_res_pred ON prediction_resolutions(prediction_id);"
        "CREATE INDEX IF NOT EXISTS idx_pred_res_run_pred ON prediction_resolutions(run_id, prediction_id);"
        // Phase 11: Self-Revision tables
        "CREATE TABLE IF NOT EXISTS self_revision_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  revision_json TEXT NOT NULL,"
        "  driver_explanation TEXT,"
        "  trust_before REAL,"
        "  trust_after REAL,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_self_revision_run ON self_revision_log(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_self_revision_ts ON self_revision_log(ts_ms);"
        "CREATE TABLE IF NOT EXISTS self_revision_outcomes ("
        "  revision_id INTEGER PRIMARY KEY,"
        "  run_id INTEGER NOT NULL,"
        "  eval_ts_ms INTEGER NOT NULL,"
        "  outcome_class TEXT NOT NULL,"
        "  trust_pre REAL,"
        "  trust_post REAL,"
        "  prediction_error_pre REAL,"
        "  prediction_error_post REAL,"
        "  coherence_pre REAL,"
        "  coherence_post REAL,"
        "  reward_slope_pre REAL,"
        "  reward_slope_post REAL,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(revision_id) REFERENCES self_revision_log(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_self_revision_outcomes_run ON self_revision_outcomes(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_self_revision_outcomes_evalts ON self_revision_outcomes(eval_ts_ms);"
        // Phase 12: Self-Consistency tables
        "CREATE TABLE IF NOT EXISTS self_consistency_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  consistency_score REAL,"
        "  notes TEXT,"
        "  window_json TEXT,"
        "  driver_explanation TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_self_consistency_run ON self_consistency_log(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_self_consistency_ts ON self_consistency_log(ts_ms);"
        // Phase 13: Autonomy Envelope decision log
        "CREATE TABLE IF NOT EXISTS autonomy_envelope_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  decision TEXT NOT NULL,"
        "  driver_json TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_autonomy_envelope_run ON autonomy_envelope_log(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_autonomy_envelope_ts ON autonomy_envelope_log(ts_ms);"
        // Phase 14: Meta-Reason log
        "CREATE TABLE IF NOT EXISTS meta_reason_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  verdict TEXT NOT NULL,"
        "  reasoning_json TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_meta_reason_run ON meta_reason_log(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_meta_reason_ts ON meta_reason_log(ts_ms);"
        // Phase 15: Ethics Regulator log
        "CREATE TABLE IF NOT EXISTS ethics_regulator_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  decision TEXT NOT NULL,"
        "  driver_json TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_ethics_regulator_run ON ethics_regulator_log(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_ethics_regulator_ts ON ethics_regulator_log(ts_ms);"
        // Phase 11: Parameter history table
        "CREATE TABLE IF NOT EXISTS parameter_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER,"
        "  ts_ms INTEGER,"
        "  phase INTEGER,"
        "  parameter TEXT,"
        "  value REAL,"
        "  revision_id INTEGER,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE,"
        "  FOREIGN KEY(revision_id) REFERENCES self_revision_log(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_paramhist_run ON parameter_history(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_paramhist_ts ON parameter_history(ts_ms);"
        "CREATE TABLE IF NOT EXISTS autonomy_modulation_log ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  autonomy_score REAL,"
        "  autonomy_tier TEXT,"
        "  autonomy_gain REAL,"
        "  ethics_hard_block INTEGER,"
        "  ethics_soft_risk REAL,"
        "  pre_rank_entropy REAL,"
        "  post_rank_entropy REAL,"
        "  exploration_bias REAL,"
        "  options_considered INTEGER,"
        "  option_rank_shift_mean REAL,"
        "  option_rank_shift_max REAL,"
        "  selected_option_id INTEGER,"
        "  decision_confidence REAL,"
        "  autonomy_applied INTEGER,"
        "  veto_reason TEXT,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_autonomy_modulation_run ON autonomy_modulation_log(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_autonomy_modulation_ts ON autonomy_modulation_log(ts_ms);"
        "CREATE TABLE IF NOT EXISTS actions ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  run_id INTEGER NOT NULL,"
        "  ts_ms INTEGER NOT NULL,"
        "  step INTEGER NOT NULL,"
        "  type TEXT NOT NULL,"
        "  payload_json TEXT,"
        "  success INTEGER NOT NULL DEFAULT 1,"
        "  FOREIGN KEY(run_id) REFERENCES runs(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_actions_run ON actions(run_id);"
        "CREATE INDEX IF NOT EXISTS idx_actions_ts ON actions(ts_ms);";
    if (!exec(schema_sql)) return false;

    // Backfill migration for Phase 17b: add mode/lambda/kappa to context_peer_log if missing
    bool has_mode = false;
    bool has_lambda = false;
    bool has_kappa = false;
    {
        sqlite3_stmt* stmt = nullptr;
        const char* pragma = "PRAGMA table_info(context_peer_log);";
        if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), pragma, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* name = sqlite3_column_text(stmt, 1);
                if (!name) continue;
                std::string col(reinterpret_cast<const char*>(name));
                if (col == "mode") has_mode = true;
                else if (col == "lambda") has_lambda = true;
                else if (col == "kappa") has_kappa = true;
            }
        }
        sqlite3_finalize(stmt);
    }
    if (!has_mode) {
        (void)exec("ALTER TABLE context_peer_log ADD COLUMN mode TEXT DEFAULT 'coop';");
    }
    if (!has_lambda) {
        (void)exec("ALTER TABLE context_peer_log ADD COLUMN lambda REAL DEFAULT 0.0;");
    }
    if (!has_kappa) {
        (void)exec("ALTER TABLE context_peer_log ADD COLUMN kappa REAL DEFAULT 0.0;");
    }

    // Backfill migration: ensure reward_updates column exists for existing DBs
    bool has_reward_updates = false;
    {
        sqlite3_stmt* stmt = nullptr;
        const char* pragma = "PRAGMA table_info(learning_stats);";
        if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), pragma, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* name = sqlite3_column_text(stmt, 1);
                if (name && std::string(reinterpret_cast<const char*>(name)) == "reward_updates") {
                    has_reward_updates = true;
                    break;
                }
            }
        }
        sqlite3_finalize(stmt);
    }
    if (!has_reward_updates) {
        (void)exec("ALTER TABLE learning_stats ADD COLUMN reward_updates INTEGER;");
    }

    bool has_avg_energy = false;
    bool has_metabolic_hazard = false;
    {
        sqlite3_stmt* stmt = nullptr;
        const char* pragma2 = "PRAGMA table_info(learning_stats);";
        if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), pragma2, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* name = sqlite3_column_text(stmt, 1);
                if (!name) continue;
                std::string col(reinterpret_cast<const char*>(name));
                if (col == "avg_energy") has_avg_energy = true;
                if (col == "metabolic_hazard") has_metabolic_hazard = true;
            }
        }
        sqlite3_finalize(stmt);
    }
    if (!has_avg_energy) {
        (void)exec("ALTER TABLE learning_stats ADD COLUMN avg_energy REAL;");
    }
    if (!has_metabolic_hazard) {
        (void)exec("ALTER TABLE learning_stats ADD COLUMN metabolic_hazard REAL;");
    }

    // Backfill migration: add self_explanation_json to metacognition if missing
    bool has_self_explanation = false;
    {
        sqlite3_stmt* stmt = nullptr;
        const char* pragma = "PRAGMA table_info(metacognition);";
        if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), pragma, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* name = sqlite3_column_text(stmt, 1);
                if (name && std::string(reinterpret_cast<const char*>(name)) == "self_explanation_json") {
                    has_self_explanation = true;
                    break;
                }
            }
        }
        sqlite3_finalize(stmt);
    }
    if (!has_self_explanation) {
        (void)exec("ALTER TABLE metacognition ADD COLUMN self_explanation_json TEXT;");
    }

    // Backfill migration: add delta columns to metacognition if missing
    bool has_trust_delta = false;
    bool has_coherence_delta = false;
    bool has_goal_accuracy_delta = false;
    {
        sqlite3_stmt* stmt = nullptr;
        const char* pragma = "PRAGMA table_info(metacognition);";
        if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), pragma, -1, &stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* name = sqlite3_column_text(stmt, 1);
                if (!name) continue;
                std::string col(reinterpret_cast<const char*>(name));
                if (col == "trust_delta") has_trust_delta = true;
                else if (col == "coherence_delta") has_coherence_delta = true;
                else if (col == "goal_accuracy_delta") has_goal_accuracy_delta = true;
            }
        }
        sqlite3_finalize(stmt);
    }
    if (!has_trust_delta) {
        (void)exec("ALTER TABLE metacognition ADD COLUMN trust_delta REAL;");
    }
    if (!has_coherence_delta) {
        (void)exec("ALTER TABLE metacognition ADD COLUMN coherence_delta REAL;");
    }
    if (!has_goal_accuracy_delta) {
        (void)exec("ALTER TABLE metacognition ADD COLUMN goal_accuracy_delta REAL;");
    }

    return true;
#else
    return false;
#endif
}


bool MemoryDB::beginRun(const std::string& metadata_json, std::int64_t& out_run_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "INSERT INTO runs (started_ms, metadata_json) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << " sql=" << sql << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, now_ms());
    sqlite3_bind_text(stmt, 2, metadata_json.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for beginRun: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_run_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] began run id=" << out_run_id << std::endl;
    return true;
#else
    (void)metadata_json; (void)out_run_id;
    return false;
#endif
}

bool MemoryDB::insertLearningStats(std::int64_t ts_ms,
                                   std::uint64_t step,
                                   double processing_hz,
                                   const LearningSystem::Statistics& s,
                                   std::int64_t run_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO learning_stats (run_id, ts_ms, step, processing_hz, total_updates, hebbian_updates, stdp_updates, reward_updates, avg_weight_change, consolidation_rate, active_synapses, potentiated_synapses, depressed_synapses, avg_energy, metabolic_hazard)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertLearningStats: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(step));
    sqlite3_bind_double(stmt, 4, processing_hz);
    sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(s.total_updates));
    sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(s.hebbian_updates));
    sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(s.stdp_updates));
    // BUGFIX: persist the actual reward_updates counter, not reward_events
    sqlite3_bind_int64(stmt, 8, static_cast<sqlite3_int64>(s.reward_updates));
    sqlite3_bind_double(stmt, 9, s.average_weight_change);
    sqlite3_bind_double(stmt, 10, s.memory_consolidation_rate);
    sqlite3_bind_int64(stmt, 11, static_cast<sqlite3_int64>(s.active_synapses));
    sqlite3_bind_int64(stmt, 12, static_cast<sqlite3_int64>(s.potentiated_synapses));
    sqlite3_bind_int64(stmt, 13, static_cast<sqlite3_int64>(s.depressed_synapses));
    sqlite3_bind_double(stmt, 14, s.avg_energy);
    sqlite3_bind_double(stmt, 15, s.metabolic_hazard);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertLearningStats: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)ts_ms; (void)step; (void)processing_hz; (void)s; (void)run_id;
    return false;
#endif
}

bool MemoryDB::insertExperience(std::int64_t ts_ms,
                          std::uint64_t step,
                          const std::string& tag,
                          const std::string& input_json,
                          const std::string& output_json,
                          bool significant,
                          std::int64_t run_id,
                          std::int64_t& out_experience_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO experiences (run_id, ts_ms, step, tag, input_json, output_json, significant)"
        " VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(step));
    sqlite3_bind_text(stmt, 4, tag.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, input_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, output_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 7, significant ? 1 : 0);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertExperience: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (ok) out_experience_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    return ok;
#else
    (void)ts_ms; (void)step; (void)tag; (void)input_json; (void)output_json; (void)significant; (void)run_id; (void)out_experience_id;
    return false;
#endif
}

bool MemoryDB::insertRewardLog(std::int64_t ts_ms,
                         std::uint64_t step,
                         double reward,
                         const std::string& source,
                         const std::string& context_json,
                         std::int64_t run_id,
                         std::int64_t& out_reward_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO reward_log (run_id, ts_ms, step, reward, source, context_json)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertRewardLog: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(step));
    sqlite3_bind_double(stmt, 4, reward);
    sqlite3_bind_text(stmt, 5, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, context_json.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertRewardLog: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_reward_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted reward id=" << out_reward_id << " reward=" << reward << " step=" << step << std::endl;
    return true;
#else
    (void)ts_ms; (void)step; (void)reward; (void)source; (void)context_json; (void)run_id; (void)out_reward_id;
    return false;
#endif
}

bool MemoryDB::insertSelfModel(std::int64_t ts_ms,
                         std::uint64_t step,
                         const std::string& state_json,
                         double confidence,
                         std::int64_t run_id,
                         std::int64_t& out_self_model_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO self_model (run_id, ts_ms, step, state_json, confidence)"
        " VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertSelfModel: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(step));
    sqlite3_bind_text(stmt, 4, state_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, confidence);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertSelfModel: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_self_model_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted self_model id=" << out_self_model_id << std::endl;
    return true;
#else
    (void)ts_ms; (void)step; (void)state_json; (void)confidence; (void)run_id; (void)out_self_model_id;
    return false;
#endif
}

bool MemoryDB::insertEpisode(const std::string& name,
                       std::int64_t start_ms,
                       std::int64_t run_id,
                       std::int64_t& out_episode_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO episodes (run_id, name, start_ms) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertEpisode: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, start_ms);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertEpisode: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_episode_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted episode id=" << out_episode_id << " name='" << name << "'" << std::endl;
    return true;
#else
    (void)name; (void)start_ms; (void)run_id; (void)out_episode_id;
    return false;
#endif
}

bool MemoryDB::updateEpisodeEnd(std::int64_t episode_id,
                          std::int64_t end_ms) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "UPDATE episodes SET end_ms = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for updateEpisodeEnd: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, end_ms);
    sqlite3_bind_int64(stmt, 2, episode_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for updateEpisodeEnd: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)episode_id; (void)end_ms;
    return false;
#endif
}

bool MemoryDB::linkExperienceToEpisode(std::int64_t experience_id,
                                 std::int64_t episode_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "INSERT OR IGNORE INTO episode_experiences (episode_id, experience_id) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for linkExperienceToEpisode: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, episode_id);
    sqlite3_bind_int64(stmt, 2, experience_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for linkExperienceToEpisode: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)experience_id; (void)episode_id;
    return false;
#endif
}

std::vector<MemoryDB::RewardEntry> MemoryDB::getRecentRewards(std::int64_t run_id, int limit) {
#ifdef NF_HAVE_SQLITE3
    std::vector<RewardEntry> out;
    if (!db_ || run_id <= 0 || limit <= 0) return out;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT id, ts_ms, step, reward, source, context_json FROM reward_log WHERE run_id = ? ORDER BY ts_ms DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRecentRewards: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return out;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int(stmt, 2, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RewardEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        e.ts_ms = sqlite3_column_int64(stmt, 1);
        e.step = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 2));
        e.reward = sqlite3_column_double(stmt, 3);
        const unsigned char* src = sqlite3_column_text(stmt, 4);
        const unsigned char* ctx = sqlite3_column_text(stmt, 5);
        e.source = src ? reinterpret_cast<const char*>(src) : "";
        e.context_json = ctx ? reinterpret_cast<const char*>(ctx) : "";
        out.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return out;
#else
    (void)run_id; (void)limit; std::vector<RewardEntry> out; return out;
#endif
}

std::vector<MemoryDB::RewardEntry> MemoryDB::getRewardsBetween(std::int64_t run_id,
                                                               std::int64_t start_ts_ms,
                                                               std::int64_t end_ts_ms,
                                                               int limit) {
#ifdef NF_HAVE_SQLITE3
    std::vector<RewardEntry> out;
    if (!db_ || run_id <= 0 || limit <= 0) return out;
    if (start_ts_ms > end_ts_ms) std::swap(start_ts_ms, end_ts_ms);
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "SELECT id, ts_ms, step, reward, source, context_json"
        " FROM reward_log WHERE run_id = ? AND ts_ms >= ? AND ts_ms <= ?"
        " ORDER BY ts_ms ASC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRewardsBetween: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return out;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, start_ts_ms);
    sqlite3_bind_int64(stmt, 3, end_ts_ms);
    sqlite3_bind_int(stmt, 4, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RewardEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        e.ts_ms = sqlite3_column_int64(stmt, 1);
        e.step = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 2));
        e.reward = sqlite3_column_double(stmt, 3);
        const unsigned char* src = sqlite3_column_text(stmt, 4);
        const unsigned char* ctx = sqlite3_column_text(stmt, 5);
        e.source = src ? reinterpret_cast<const char*>(src) : "";
        e.context_json = ctx ? reinterpret_cast<const char*>(ctx) : "";
        out.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return out;
#else
    (void)run_id; (void)start_ts_ms; (void)end_ts_ms; (void)limit; return {};
#endif
}

std::vector<MemoryDB::EpisodeEntry> MemoryDB::getEpisodes(std::int64_t run_id) {
#ifdef NF_HAVE_SQLITE3
    std::vector<EpisodeEntry> out;
    if (!db_ || run_id <= 0) return out;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT id, name, start_ms, COALESCE(end_ms, 0) FROM episodes WHERE run_id = ? ORDER BY start_ms ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getEpisodes: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return out;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        EpisodeEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        const unsigned char* name = sqlite3_column_text(stmt, 1);
        e.name = name ? reinterpret_cast<const char*>(name) : "";
        e.start_ms = sqlite3_column_int64(stmt, 2);
        e.end_ms = sqlite3_column_int64(stmt, 3);
        out.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return out;
#else
    (void)run_id; std::vector<EpisodeEntry> out; return out;
#endif
}

std::vector<MemoryDB::RunEntry> MemoryDB::getRuns() {
#ifdef NF_HAVE_SQLITE3
    std::vector<RunEntry> out;
    if (!db_) return out;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT id, started_ms, COALESCE(metadata_json, '') FROM runs ORDER BY id ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRuns: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return out;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RunEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        e.started_ms = sqlite3_column_int64(stmt, 1);
        const unsigned char* meta = sqlite3_column_text(stmt, 2);
        e.metadata_json = meta ? reinterpret_cast<const char*>(meta) : "";
        out.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return out;
#else
    std::vector<RunEntry> out; return out;
#endif
}

bool MemoryDB::upsertEpisodeStats(std::int64_t episode_id,
                            std::uint64_t steps,
                            bool success,
                            double episode_return) {
#ifdef NF_HAVE_SQLITE3
    if (!db_ || episode_id <= 0) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT OR REPLACE INTO episode_stats (episode_id, steps, success, episode_return) "
        "VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for upsertEpisodeStats: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, episode_id);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(steps));
    sqlite3_bind_int(stmt, 3, success ? 1 : 0);
    sqlite3_bind_double(stmt, 4, episode_return);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for upsertEpisodeStats: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)episode_id; (void)steps; (void)success; (void)episode_return; return false;
#endif
}

bool MemoryDB::getLatestRewardUpdates(std::int64_t run_id, std::uint64_t& out_reward_updates) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT reward_updates FROM learning_stats WHERE run_id = ? AND reward_updates IS NOT NULL ORDER BY ts_ms DESC, id DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    bool ok = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out_reward_updates = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0));
        ok = true;
    } else {
        ok = false;
    }
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)run_id; (void)out_reward_updates;
    return false;
#endif
}

// M6: Substrate state serialization methods
bool MemoryDB::insertSubstrateState(std::int64_t ts_ms,
                                    std::uint64_t step,
                                    const std::string& state_type,
                                    const std::string& region_id,
                                    const std::string& serialized_data,
                                    std::int64_t run_id,
                                    std::int64_t& out_state_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO substrate_states (run_id, ts_ms, step, state_type, region_id, serialized_data)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertSubstrateState: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(step));
    sqlite3_bind_text(stmt, 4, state_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, region_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, serialized_data.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertSubstrateState: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_state_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted substrate_state id=" << out_state_id << " type=" << state_type << " region=" << region_id << std::endl;
    return true;
#else
    (void)ts_ms; (void)step; (void)state_type; (void)region_id; (void)serialized_data; (void)run_id; (void)out_state_id;
    return false;
#endif
}

bool MemoryDB::insertHippocampalSnapshot(std::int64_t ts_ms,
                                          std::uint64_t step,
                                          double priority,
                                          double significance,
                                          const std::string& snapshot_data,
                                          std::int64_t run_id,
                                          std::int64_t& out_snapshot_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO hippocampal_snapshots (run_id, ts_ms, step, priority, significance, snapshot_data)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertHippocampalSnapshot: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(step));
    sqlite3_bind_double(stmt, 4, priority);
    sqlite3_bind_double(stmt, 5, significance);
    sqlite3_bind_text(stmt, 6, snapshot_data.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertHippocampalSnapshot: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_snapshot_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted hippocampal_snapshot id=" << out_snapshot_id << " priority=" << priority << std::endl;
    return true;
#else
    (void)ts_ms; (void)step; (void)priority; (void)significance; (void)snapshot_data; (void)run_id; (void)out_snapshot_id;
    return false;
#endif
}

std::vector<MemoryDB::SubstrateStateEntry> MemoryDB::getSubstrateStates(std::int64_t run_id,
                                                                         const std::string& state_type,
                                                                         std::int64_t start_ms,
                                                                         std::int64_t end_ms,
                                                                         int limit) {
    std::vector<SubstrateStateEntry> result;
#ifdef NF_HAVE_SQLITE3
    if (!db_) return result;
    std::lock_guard<std::mutex> lg(m_);
    
    std::ostringstream sql_stream;
    sql_stream << "SELECT id, ts_ms, step, state_type, region_id, serialized_data FROM substrate_states WHERE run_id = ?";
    
    if (!state_type.empty()) {
        sql_stream << " AND state_type = ?";
    }
    if (start_ms > 0) {
        sql_stream << " AND ts_ms >= ?";
    }
    if (end_ms > 0) {
        sql_stream << " AND ts_ms <= ?";
    }
    sql_stream << " ORDER BY ts_ms DESC";
    if (limit > 0) {
        sql_stream << " LIMIT ?";
    }
    
    std::string sql = sql_stream.str();
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getSubstrateStates: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return result;
    }
    
    int param_idx = 1;
    sqlite3_bind_int64(stmt, param_idx++, run_id);
    if (!state_type.empty()) {
        sqlite3_bind_text(stmt, param_idx++, state_type.c_str(), -1, SQLITE_STATIC);
    }
    if (start_ms > 0) {
        sqlite3_bind_int64(stmt, param_idx++, start_ms);
    }
    if (end_ms > 0) {
        sqlite3_bind_int64(stmt, param_idx++, end_ms);
    }
    if (limit > 0) {
        sqlite3_bind_int(stmt, param_idx++, limit);
    }
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SubstrateStateEntry entry;
        entry.id = sqlite3_column_int64(stmt, 0);
        entry.ts_ms = sqlite3_column_int64(stmt, 1);
        entry.step = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 2));
        entry.state_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        entry.region_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        entry.serialized_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        result.push_back(entry);
    }
    sqlite3_finalize(stmt);
#else
    (void)run_id; (void)state_type; (void)start_ms; (void)end_ms; (void)limit;
#endif
    return result;
}

std::vector<MemoryDB::HippocampalSnapshotEntry> MemoryDB::getHippocampalSnapshots(std::int64_t run_id,
                                                                                   double min_priority,
                                                                                   int limit) {
    std::vector<HippocampalSnapshotEntry> result;
#ifdef NF_HAVE_SQLITE3
    if (!db_) return result;
    std::lock_guard<std::mutex> lg(m_);
    
    const char* sql = "SELECT id, ts_ms, step, priority, significance, snapshot_data FROM hippocampal_snapshots "
                      "WHERE run_id = ? AND priority >= ? ORDER BY priority DESC, ts_ms DESC LIMIT ?";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getHippocampalSnapshots: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return result;
    }
    
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_double(stmt, 2, min_priority);
    sqlite3_bind_int(stmt, 3, limit);
    
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        HippocampalSnapshotEntry entry;
        entry.id = sqlite3_column_int64(stmt, 0);
        entry.ts_ms = sqlite3_column_int64(stmt, 1);
        entry.step = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 2));
        entry.priority = sqlite3_column_double(stmt, 3);
        entry.significance = sqlite3_column_double(stmt, 4);
        entry.snapshot_data = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        result.push_back(entry);
    }
    sqlite3_finalize(stmt);
#else
    (void)run_id; (void)min_priority; (void)limit;
#endif
    return result;
}

// Phase 6: Hybrid Reasoning Engine methods
bool MemoryDB::insertOption(std::int64_t ts_ms,
                            std::uint64_t step,
                            const std::string& source,
                            const std::string& option_json,
                            double confidence,
                            bool selected,
                            std::int64_t run_id,
                            std::int64_t& out_option_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO options (run_id, ts_ms, step, source, option_json, confidence, selected)"
        " VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertOption: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(step));
    sqlite3_bind_text(stmt, 4, source.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, option_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, confidence);
    sqlite3_bind_int(stmt, 7, selected ? 1 : 0);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertOption: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_option_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted option id=" << out_option_id << " selected=" << selected << std::endl;
    return true;
#else
    (void)ts_ms; (void)step; (void)source; (void)option_json; (void)confidence; (void)selected; (void)run_id; (void)out_option_id; return false;
#endif
}

bool MemoryDB::upsertOptionStats(std::int64_t option_id,
                                 std::uint64_t evaluations,
                                 double average_score,
                                 std::int64_t last_evaluated_ms) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT OR REPLACE INTO option_stats (option_id, evaluations, average_score, last_evaluated_ms)"
        " VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for upsertOptionStats: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, option_id);
    sqlite3_bind_int64(stmt, 2, static_cast<sqlite3_int64>(evaluations));
    sqlite3_bind_double(stmt, 3, average_score);
    if (last_evaluated_ms > 0) {
        sqlite3_bind_int64(stmt, 4, last_evaluated_ms);
    } else {
        sqlite3_bind_null(stmt, 4);
    }
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for upsertOptionStats: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)option_id; (void)evaluations; (void)average_score; (void)last_evaluated_ms; return false;
#endif
}

bool MemoryDB::insertInferredFact(std::int64_t ts_ms,
                                  const std::string& fact_json,
                                  double confidence,
                                  std::int64_t run_id,
                                  std::optional<std::int64_t> derived_option_id,
                                  std::int64_t& out_fact_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO inferred_facts (run_id, ts_ms, fact_json, confidence, derived_option_id)"
        " VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertInferredFact: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_text(stmt, 3, fact_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, confidence);
    if (derived_option_id.has_value()) {
        sqlite3_bind_int64(stmt, 5, derived_option_id.value());
    } else {
        sqlite3_bind_null(stmt, 5);
    }
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertInferredFact: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_fact_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted inferred_fact id=" << out_fact_id << std::endl;
    return true;
#else
    (void)ts_ms; (void)fact_json; (void)confidence; (void)run_id; (void)derived_option_id; (void)out_fact_id; return false;
#endif
}

bool MemoryDB::insertVerification(std::int64_t ts_ms,
                                  std::int64_t fact_id,
                                  const std::string& result,
                                  bool contradiction,
                                  const std::string& details_json,
                                  std::int64_t run_id,
                                  std::int64_t& out_verification_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO verifications (run_id, ts_ms, fact_id, result, contradiction, details_json)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertVerification: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, fact_id);
    sqlite3_bind_text(stmt, 4, result.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, contradiction ? 1 : 0);
    sqlite3_bind_text(stmt, 6, details_json.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertVerification: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_verification_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted verification id=" << out_verification_id << " contradiction=" << contradiction << std::endl;
    return true;
#else
    (void)ts_ms; (void)fact_id; (void)result; (void)contradiction; (void)details_json; (void)run_id; (void)out_verification_id; return false;
#endif
}

// Phase 7: Intent Graph, Affective State, and Reflections
bool MemoryDB::insertIntentNode(std::int64_t ts_ms,
                                const std::string& node_type,
                                const std::string& state_json,
                                double confidence,
                                const std::string& source,
                                std::int64_t run_id,
                                std::int64_t& out_node_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO intent_nodes (run_id, ts_ms, node_type, state_json, confidence, source)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertIntentNode: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_text(stmt, 3, node_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, state_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, confidence);
    sqlite3_bind_text(stmt, 6, source.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertIntentNode: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_node_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted intent_node id=" << out_node_id << std::endl;
    return true;
#else
    (void)ts_ms; (void)node_type; (void)state_json; (void)confidence; (void)source; (void)run_id; (void)out_node_id; return false;
#endif
}

bool MemoryDB::insertIntentEdge(std::int64_t ts_ms,
                                std::int64_t from_node_id,
                                std::int64_t to_node_id,
                                const std::string& cause,
                                double weight,
                                const std::string& details_json,
                                std::int64_t run_id,
                                std::int64_t& out_edge_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO intent_edges (run_id, ts_ms, from_node_id, to_node_id, cause, weight, details_json)"
        " VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertIntentEdge: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, from_node_id);
    sqlite3_bind_int64(stmt, 4, to_node_id);
    sqlite3_bind_text(stmt, 5, cause.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 6, weight);
    sqlite3_bind_text(stmt, 7, details_json.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertIntentEdge: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_edge_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted intent_edge id=" << out_edge_id << std::endl;
    return true;
#else
    (void)ts_ms; (void)from_node_id; (void)to_node_id; (void)cause; (void)weight; (void)details_json; (void)run_id; (void)out_edge_id; return false;
#endif
}

bool MemoryDB::upsertAffectiveState(std::int64_t ts_ms,
                                    double valence,
                                    double arousal,
                                    double focus,
                                    const std::string& notes,
                                    std::int64_t run_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO affective_state (run_id, ts_ms, valence, arousal, focus, notes)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for upsertAffectiveState: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_double(stmt, 3, valence);
    sqlite3_bind_double(stmt, 4, arousal);
    sqlite3_bind_double(stmt, 5, focus);
    sqlite3_bind_text(stmt, 6, notes.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for upsertAffectiveState: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)ts_ms; (void)valence; (void)arousal; (void)focus; (void)notes; (void)run_id; return false;
#endif
}

bool MemoryDB::insertReflection(std::int64_t ts_ms,
                                const std::string& title,
                                const std::string& rationale_json,
                                double impact,
                                std::optional<std::int64_t> episode,
                                std::int64_t run_id,
                                std::int64_t& out_reflection_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO reflections (run_id, ts_ms, title, rationale_json, impact, episode)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertReflection: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_text(stmt, 3, title.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, rationale_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, impact);
    if (episode.has_value()) {
        sqlite3_bind_int64(stmt, 6, episode.value());
    } else {
        sqlite3_bind_null(stmt, 6);
    }
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertReflection: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_reflection_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted reflection id=" << out_reflection_id << std::endl;
    return true;
#else
    (void)ts_ms; (void)title; (void)rationale_json; (void)impact; (void)episode; (void)run_id; (void)out_reflection_id; return false;
#endif
}

double MemoryDB::getEpisodeContradictionRate(std::int64_t run_id, std::int64_t episode_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return 0.0;
    
    // Query to get contradiction rate for verifications linked to experiences in this episode
    const char* sql = R"(
        SELECT 
            COUNT(CASE WHEN v.contradiction = 1 THEN 1 END) as contradictions,
            COUNT(*) as total_verifications
        FROM verifications v
        JOIN inferred_facts f ON v.fact_id = f.id
        JOIN episode_experiences ee ON f.derived_option_id IS NOT NULL
        WHERE v.run_id = ? AND ee.episode_id = ?
    )";
    
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] Failed to prepare getEpisodeContradictionRate query: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return 0.0;
    }
    
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, episode_id);
    
    double contradiction_rate = 0.0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int contradictions = sqlite3_column_int(stmt, 0);
        int total = sqlite3_column_int(stmt, 1);
        
        if (total > 0) {
            contradiction_rate = static_cast<double>(contradictions) / static_cast<double>(total);
        }
        
        if (debug_) {
            std::cerr << "[MemoryDB] Episode " << episode_id << " contradiction rate: " 
                      << contradictions << "/" << total << " = " << contradiction_rate << std::endl;
        }
    }
    
    sqlite3_finalize(stmt);
    return contradiction_rate;
#else
    (void)run_id; (void)episode_id; return 0.0;
#endif
}

bool MemoryDB::insertAction(std::int64_t ts_ms,
                            std::uint64_t step,
                            const std::string& type,
                            const std::string& payload_json,
                            bool success,
                            std::int64_t run_id,
                            std::int64_t& out_action_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO actions (run_id, ts_ms, step, type, payload_json, success)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertAction: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(step));
    sqlite3_bind_text(stmt, 4, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, payload_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, success ? 1 : 0);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertAction: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_action_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted action id=" << out_action_id << " type=" << type << std::endl;
    return true;
#else
    (void)ts_ms; (void)step; (void)type; (void)payload_json; (void)success; (void)run_id; (void)out_action_id; return false;
#endif
}

// Phase 8: Goal hierarchy and motivation methods
bool MemoryDB::insertGoalNode(const std::string& description,
                              double priority,
                              double stability,
                              std::int64_t run_id,
                              std::optional<std::int64_t> origin_reflection_id,
                              std::int64_t& out_goal_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO goal_nodes (run_id, description, priority, stability, origin_reflection_id)"
        " VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertGoalNode: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, priority);
    sqlite3_bind_double(stmt, 4, stability);
    if (origin_reflection_id.has_value()) {
        sqlite3_bind_int64(stmt, 5, origin_reflection_id.value());
    } else {
        sqlite3_bind_null(stmt, 5);
    }
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertGoalNode: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_goal_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted goal_node id=" << out_goal_id << std::endl;
    return true;
#else
    (void)description; (void)priority; (void)stability; (void)run_id; (void)origin_reflection_id; (void)out_goal_id; return false;
#endif
}

std::optional<std::int64_t> MemoryDB::findGoalByDescription(const std::string& description, std::int64_t run_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return std::nullopt;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT goal_id FROM goal_nodes WHERE run_id = ? AND description = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for findGoalByDescription: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_text(stmt, 2, description.c_str(), -1, SQLITE_TRANSIENT);
    std::optional<std::int64_t> result = std::nullopt;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return result;
#else
    (void)description; (void)run_id; return std::nullopt;
#endif
}

bool MemoryDB::updateGoalStability(std::int64_t goal_id, double stability) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "UPDATE goal_nodes SET stability = ? WHERE goal_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for updateGoalStability: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_double(stmt, 1, stability);
    sqlite3_bind_int64(stmt, 2, goal_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for updateGoalStability: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)goal_id; (void)stability; return false;
#endif
}

bool MemoryDB::insertGoalEdge(std::int64_t goal_id,
                              std::int64_t subgoal_id,
                              double weight) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT OR REPLACE INTO goal_edges (goal_id, subgoal_id, weight)"
        " VALUES (?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertGoalEdge: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, goal_id);
    sqlite3_bind_int64(stmt, 2, subgoal_id);
    sqlite3_bind_double(stmt, 3, weight);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertGoalEdge: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)goal_id; (void)subgoal_id; (void)weight; return false;
#endif
}

bool MemoryDB::insertMotivationState(std::int64_t ts_ms,
                                     double motivation,
                                     double coherence,
                                     const std::string& notes,
                                     std::int64_t run_id,
                                     std::int64_t& out_motivation_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO motivation_state (run_id, ts_ms, motivation, coherence, notes)"
        " VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertMotivationState: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_double(stmt, 3, motivation);
    sqlite3_bind_double(stmt, 4, coherence);
    sqlite3_bind_text(stmt, 5, notes.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertMotivationState: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_motivation_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted motivation_state id=" << out_motivation_id << std::endl;
    return true;
#else
    (void)ts_ms; (void)motivation; (void)coherence; (void)notes; (void)run_id; (void)out_motivation_id; return false;
#endif
}

std::vector<MemoryDB::MotivationStateEntry> MemoryDB::getMotivationStatesBetween(std::int64_t run_id,
                                                                                std::int64_t start_ts_ms,
                                                                                std::int64_t end_ts_ms,
                                                                                int limit) {
#ifdef NF_HAVE_SQLITE3
    std::vector<MotivationStateEntry> out;
    if (!db_ || run_id <= 0 || limit <= 0) return out;
    if (start_ts_ms > end_ts_ms) std::swap(start_ts_ms, end_ts_ms);
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "SELECT id, ts_ms, motivation, coherence, COALESCE(notes, '')"
        " FROM motivation_state WHERE run_id = ? AND ts_ms >= ? AND ts_ms <= ?"
        " ORDER BY ts_ms ASC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getMotivationStatesBetween: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return out;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, start_ts_ms);
    sqlite3_bind_int64(stmt, 3, end_ts_ms);
    sqlite3_bind_int(stmt, 4, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MotivationStateEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        e.ts_ms = sqlite3_column_int64(stmt, 1);
        e.motivation = sqlite3_column_double(stmt, 2);
        e.coherence = sqlite3_column_double(stmt, 3);
        const char* notes = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        e.notes = notes ? notes : "";
        out.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return out;
#else
    (void)run_id; (void)start_ts_ms; (void)end_ts_ms; (void)limit; return {};
#endif
}

bool MemoryDB::insertMetacognition(std::int64_t ts_ms,
                                   double self_trust,
                                   double narrative_rmse,
                                   double goal_mae,
                                   double ece,
                                   const std::string& notes,
                                   std::optional<double> trust_delta,
                                   std::optional<double> coherence_delta,
                                   std::optional<double> goal_accuracy_delta,
                                   std::int64_t run_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO metacognition (run_id, ts_ms, self_trust, narrative_rmse, goal_mae, ece, notes, trust_delta, coherence_delta, goal_accuracy_delta)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertMetacognition: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_double(stmt, 3, self_trust);
    if (std::isnan(narrative_rmse)) sqlite3_bind_null(stmt, 4); else sqlite3_bind_double(stmt, 4, narrative_rmse);
    if (std::isnan(goal_mae)) sqlite3_bind_null(stmt, 5); else sqlite3_bind_double(stmt, 5, goal_mae);
    if (std::isnan(ece)) sqlite3_bind_null(stmt, 6); else sqlite3_bind_double(stmt, 6, ece);
    sqlite3_bind_text(stmt, 7, notes.c_str(), -1, SQLITE_TRANSIENT);
    if (!trust_delta.has_value() || std::isnan(*trust_delta)) sqlite3_bind_null(stmt, 8); else sqlite3_bind_double(stmt, 8, *trust_delta);
    if (!coherence_delta.has_value() || std::isnan(*coherence_delta)) sqlite3_bind_null(stmt, 9); else sqlite3_bind_double(stmt, 9, *coherence_delta);
    if (!goal_accuracy_delta.has_value() || std::isnan(*goal_accuracy_delta)) sqlite3_bind_null(stmt, 10); else sqlite3_bind_double(stmt, 10, *goal_accuracy_delta);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertMetacognition: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)ts_ms; (void)self_trust; (void)narrative_rmse; (void)goal_mae; (void)ece; (void)notes; (void)trust_delta; (void)coherence_delta; (void)goal_accuracy_delta; (void)run_id; return false;
#endif
}

bool MemoryDB::insertNarrativePrediction(std::int64_t ts_ms,
                                         std::int64_t reflection_id,
                                         std::int64_t horizon_ms,
                                         double predicted_coherence_delta,
                                         double confidence,
                                         const std::string& targets_json,
                                         std::int64_t run_id,
                                         std::int64_t& out_prediction_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO narrative_predictions (run_id, ts_ms, reflection_id, horizon_ms, predicted_coherence_delta, confidence, targets_json)"
        " VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertNarrativePrediction: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, reflection_id);
    sqlite3_bind_int64(stmt, 4, horizon_ms);
    if (std::isnan(predicted_coherence_delta)) sqlite3_bind_null(stmt, 5); else sqlite3_bind_double(stmt, 5, predicted_coherence_delta);
    if (std::isnan(confidence)) sqlite3_bind_null(stmt, 6); else sqlite3_bind_double(stmt, 6, confidence);
    sqlite3_bind_text(stmt, 7, targets_json.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertNarrativePrediction: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_prediction_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted narrative_prediction id=" << out_prediction_id << std::endl;
    return true;
#else
    (void)ts_ms; (void)reflection_id; (void)horizon_ms; (void)predicted_coherence_delta; (void)confidence; (void)targets_json; (void)run_id; (void)out_prediction_id; return false;
#endif
}

bool MemoryDB::insertPredictionResolution(std::int64_t run_id,
                                            std::int64_t prediction_id,
                                            std::int64_t ts_ms,
                                            double observed_delta,
                                            const std::string& result_json) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO prediction_resolutions (run_id, ts_ms, prediction_id, observed_delta, result_json)"
        " VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertPredictionResolution: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, prediction_id);
    if (std::isnan(observed_delta)) sqlite3_bind_null(stmt, 4); else sqlite3_bind_double(stmt, 4, observed_delta);
    sqlite3_bind_text(stmt, 5, result_json.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertPredictionResolution: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)run_id; (void)prediction_id; (void)ts_ms; (void)observed_delta; (void)result_json; return false;
#endif
}

bool MemoryDB::updateMetacognitionExplanation(std::int64_t metacog_id,
                                              const std::string& self_explanation_json) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "UPDATE metacognition SET self_explanation_json = ? WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for updateMetacognitionExplanation: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_text(stmt, 1, self_explanation_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, metacog_id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for updateMetacognitionExplanation: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)metacog_id; (void)self_explanation_json; return false;
#endif
}

std::optional<std::int64_t> MemoryDB::getLatestMetacognitionId(std::int64_t run_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return std::nullopt;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT id FROM metacognition WHERE run_id = ? ORDER BY ts_ms DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getLatestMetacognitionId: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    std::optional<std::int64_t> result;
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        result = sqlite3_column_int64(stmt, 0);
    } else if (rc != SQLITE_DONE) {
        if (debug_) std::cerr << "[MemoryDB] step failed for getLatestMetacognitionId: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    }
    sqlite3_finalize(stmt);
    return result;
#else
    (void)run_id; return std::nullopt;
#endif
}

bool MemoryDB::insertRunEvent(std::int64_t run_id,
                              std::int64_t ts_ms,
                              std::uint64_t step,
                              const std::string& type,
                              const std::string& message,
                              int exit_code,
                              double rss_mb,
                              double gpu_mem_mb,
                              std::int64_t& out_event_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO run_events (run_id, ts_ms, step, type, message, exit_code, rss_mb, gpu_mem_mb) VALUES (?,?,?,?,?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertRunEvent: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(step));
    sqlite3_bind_text(stmt, 4, type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, message.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, exit_code);
    sqlite3_bind_double(stmt, 7, rss_mb);
    sqlite3_bind_double(stmt, 8, gpu_mem_mb);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertRunEvent: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_event_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted run_events id=" << out_event_id << std::endl;
    return true;
#else
    (void)run_id; (void)ts_ms; (void)step; (void)type; (void)message; (void)exit_code; (void)rss_mb; (void)gpu_mem_mb; (void)out_event_id; return false;
#endif
}

std::vector<MemoryDB::RunEventEntry> MemoryDB::getRecentRunEvents(std::int64_t run_id, int n) {
#ifdef NF_HAVE_SQLITE3
    std::vector<RunEventEntry> entries;
    if (!db_) return entries;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT id, ts_ms, step, type, message, exit_code, rss_mb, gpu_mem_mb FROM run_events WHERE run_id = ? ORDER BY ts_ms DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRecentRunEvents: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return entries;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int(stmt, 2, n);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        RunEventEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        e.ts_ms = sqlite3_column_int64(stmt, 1);
        e.step = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 2));
        const char* t = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (t) e.type.assign(t);
        const char* m = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (m) e.message.assign(m);
        e.exit_code = sqlite3_column_type(stmt, 5) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 5);
        e.rss_mb = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? 0.0 : sqlite3_column_double(stmt, 6);
        e.gpu_mem_mb = sqlite3_column_type(stmt, 7) == SQLITE_NULL ? 0.0 : sqlite3_column_double(stmt, 7);
        entries.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return entries;
#else
    (void)run_id; (void)n; return {};
#endif
}

// Phase 14: Meta-Reason methods
bool MemoryDB::insertMetaReason(std::int64_t run_id,
                                std::int64_t ts_ms,
                                const std::string& verdict,
                                const std::string& reasoning_json,
                                std::int64_t& out_reason_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO meta_reason_log (run_id, ts_ms, verdict, reasoning_json) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertMetaReason: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_text(stmt, 3, verdict.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, reasoning_json.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertMetaReason: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_reason_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted meta_reason_log id=" << out_reason_id << std::endl;
    return true;
#else
    (void)run_id; (void)ts_ms; (void)verdict; (void)reasoning_json; (void)out_reason_id; return false;
#endif
}

std::vector<MemoryDB::MetaReasonRecord> MemoryDB::getRecentMetaReasons(std::int64_t run_id, int n) {
#ifdef NF_HAVE_SQLITE3
    std::vector<MetaReasonRecord> entries;
    if (!db_) return entries;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT id, ts_ms, verdict, reasoning_json FROM meta_reason_log WHERE run_id = ? ORDER BY ts_ms DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRecentMetaReasons: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return entries;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int(stmt, 2, n);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MetaReasonRecord rec;
        rec.id = sqlite3_column_int64(stmt, 0);
        rec.ts_ms = sqlite3_column_int64(stmt, 1);
        const char* verdict = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (verdict) rec.verdict.assign(verdict);
        const char* reasoning = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (reasoning) rec.reasoning_json.assign(reasoning);
        entries.push_back(std::move(rec));
    }
    sqlite3_finalize(stmt);
    return entries;
#else
    (void)run_id; (void)n; return {};
#endif
}

// Phase 15: Ethics Regulator methods
bool MemoryDB::insertEthicsRegulator(std::int64_t run_id,
                                     std::int64_t ts_ms,
                                     const std::string& decision,
                                     const std::string& driver_json,
                                     std::int64_t& out_regulator_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO ethics_regulator_log (run_id, ts_ms, decision, driver_json) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertEthicsRegulator: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_text(stmt, 3, decision.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, driver_json.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertEthicsRegulator: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_regulator_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted ethics_regulator_log id=" << out_regulator_id << std::endl;
    return true;
#else
    (void)run_id; (void)ts_ms; (void)decision; (void)driver_json; (void)out_regulator_id; return false;
#endif
}

std::vector<MemoryDB::EthicsRegulatorEntry> MemoryDB::getRecentEthicsRegulator(std::int64_t run_id, int n) {
#ifdef NF_HAVE_SQLITE3
    std::vector<EthicsRegulatorEntry> entries;
    if (!db_) return entries;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT id, ts_ms, decision, driver_json FROM ethics_regulator_log WHERE run_id = ? ORDER BY ts_ms DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRecentEthicsRegulator: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return entries;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int(stmt, 2, n);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        EthicsRegulatorEntry rec;
        rec.id = sqlite3_column_int64(stmt, 0);
        rec.ts_ms = sqlite3_column_int64(stmt, 1);
        const char* decision = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (decision) rec.decision.assign(decision);
        const char* driver = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (driver) rec.driver_json.assign(driver);
        entries.push_back(std::move(rec));
    }
    sqlite3_finalize(stmt);
    return entries;
#else
    (void)run_id; (void)n; return {};
#endif
}

// Context log methods
bool MemoryDB::insertContextLog(std::int64_t run_id,
                                std::int64_t ts_ms,
                                double sample,
                                double gain,
                                int update_ms,
                                int window,
                                const std::string& label,
                                std::int64_t& out_context_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO context_log (run_id, ts_ms, sample, gain, update_ms, window, label) VALUES (?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertContextLog: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_double(stmt, 3, sample);
    sqlite3_bind_double(stmt, 4, gain);
    sqlite3_bind_int(stmt, 5, update_ms);
    sqlite3_bind_int(stmt, 6, window);
    sqlite3_bind_text(stmt, 7, label.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertContextLog: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_context_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted context_log id=" << out_context_id << " sample=" << sample << std::endl;
    return true;
#else
    (void)run_id; (void)ts_ms; (void)sample; (void)gain; (void)update_ms; (void)window; (void)label; (void)out_context_id; return false;
#endif
}

std::vector<MemoryDB::ContextLogEntry> MemoryDB::getRecentContextLog(std::int64_t run_id, int n) {
#ifdef NF_HAVE_SQLITE3
    std::vector<ContextLogEntry> entries;
    if (!db_) return entries;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT id, ts_ms, sample, gain, update_ms, window, label FROM context_log WHERE run_id = ? ORDER BY ts_ms DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRecentContextLog: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return entries;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int(stmt, 2, n);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ContextLogEntry rec;
        rec.id = sqlite3_column_int64(stmt, 0);
        rec.ts_ms = sqlite3_column_int64(stmt, 1);
        rec.sample = sqlite3_column_double(stmt, 2);
        rec.gain = sqlite3_column_double(stmt, 3);
        rec.update_ms = sqlite3_column_int(stmt, 4);
        rec.window = sqlite3_column_int(stmt, 5);
        const char* lbl = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
        if (lbl) rec.label.assign(lbl);
        entries.push_back(std::move(rec));
    }
    sqlite3_finalize(stmt);
    return entries;
#else
    (void)run_id; (void)n; return {};
#endif
}

// Context peer log methods
bool MemoryDB::insertContextPeerLog(std::int64_t run_id,
                                    std::int64_t ts_ms,
                                    const std::string& peer,
                                    double sample,
                                    double gain,
                                    int update_ms,
                                    int window,
                                    const std::string& label,
                                    const std::string& mode,
                                    double lambda,
                                    double kappa,
                                    std::int64_t& out_context_peer_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO context_peer_log (run_id, ts_ms, peer, sample, gain, update_ms, window, label, mode, lambda, kappa) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertContextPeerLog: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_text(stmt, 3, peer.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, sample);
    sqlite3_bind_double(stmt, 5, gain);
    sqlite3_bind_int(stmt, 6, update_ms);
    sqlite3_bind_int(stmt, 7, window);
    sqlite3_bind_text(stmt, 8, label.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, mode.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 10, lambda);
    sqlite3_bind_double(stmt, 11, kappa);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertContextPeerLog: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_context_peer_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted context_peer_log id=" << out_context_peer_id << " peer=" << peer << " sample=" << sample << std::endl;
    return true;
#else
    (void)run_id; (void)ts_ms; (void)peer; (void)sample; (void)gain; (void)update_ms; (void)window; (void)label; (void)mode; (void)lambda; (void)kappa; (void)out_context_peer_id; return false;
#endif
}

std::vector<MemoryDB::ContextPeerLogEntry> MemoryDB::getRecentContextPeerLog(std::int64_t run_id, const std::string& peer, int n) {
#ifdef NF_HAVE_SQLITE3
    std::vector<ContextPeerLogEntry> entries;
    if (!db_) return entries;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT id, ts_ms, peer, sample, gain, update_ms, window, label FROM context_peer_log WHERE run_id = ? AND peer = ? ORDER BY ts_ms DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRecentContextPeerLog: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return entries;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_text(stmt, 2, peer.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, n);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ContextPeerLogEntry rec;
        rec.id = sqlite3_column_int64(stmt, 0);
        rec.ts_ms = sqlite3_column_int64(stmt, 1);
        const char* pname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        rec.peer = pname ? pname : "";
        rec.sample = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? 0.0 : sqlite3_column_double(stmt, 3);
        rec.gain = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? 1.0 : sqlite3_column_double(stmt, 4);
        rec.update_ms = sqlite3_column_type(stmt, 5) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 5);
        rec.window = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? 0 : sqlite3_column_int(stmt, 6);
        const char* lab = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
        rec.label = lab ? lab : "";
        entries.push_back(std::move(rec));
    }
    sqlite3_finalize(stmt);
    return entries;
#else
    (void)run_id; (void)peer; (void)n; return {};
#endif
}

// Phase 11: Self-Revision methods
bool MemoryDB::insertSelfRevision(std::int64_t run_id,
                                  std::int64_t ts_ms,
                                  const std::string& revision_json,
                                  const std::string& driver_explanation,
                                  double trust_before,
                                  double trust_after,
                                  std::int64_t& out_revision_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO self_revision_log (run_id, ts_ms, revision_json, driver_explanation, trust_before, trust_after)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertSelfRevision: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_text(stmt, 3, revision_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, driver_explanation.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, trust_before);
    sqlite3_bind_double(stmt, 6, trust_after);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertSelfRevision: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_revision_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted self_revision_log id=" << out_revision_id << std::endl;
    return true;
#else
    (void)run_id; (void)ts_ms; (void)revision_json; (void)driver_explanation; (void)trust_before; (void)trust_after; (void)out_revision_id; return false;
#endif
}

bool MemoryDB::insertSelfRevisionOutcome(std::int64_t revision_id,
                                        std::int64_t eval_ts_ms,
                                        const std::string& outcome_class,
                                        double trust_pre,
                                        double trust_post,
                                        double prediction_error_pre,
                                        double prediction_error_post,
                                        double coherence_pre,
                                        double coherence_post,
                                        double reward_slope_pre,
                                        double reward_slope_post) {
#ifdef NF_HAVE_SQLITE3
    if (!db_ || revision_id <= 0) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT OR REPLACE INTO self_revision_outcomes (revision_id, run_id, eval_ts_ms, outcome_class, trust_pre, trust_post, prediction_error_pre, prediction_error_post, coherence_pre, coherence_post, reward_slope_pre, reward_slope_post)"
        " VALUES (?, (SELECT run_id FROM self_revision_log WHERE id = ?), ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertSelfRevisionOutcome: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, revision_id);
    sqlite3_bind_int64(stmt, 2, revision_id);
    sqlite3_bind_int64(stmt, 3, eval_ts_ms);
    sqlite3_bind_text(stmt, 4, outcome_class.c_str(), -1, SQLITE_TRANSIENT);
    if (std::isnan(trust_pre)) sqlite3_bind_null(stmt, 5); else sqlite3_bind_double(stmt, 5, trust_pre);
    if (std::isnan(trust_post)) sqlite3_bind_null(stmt, 6); else sqlite3_bind_double(stmt, 6, trust_post);
    if (std::isnan(prediction_error_pre)) sqlite3_bind_null(stmt, 7); else sqlite3_bind_double(stmt, 7, prediction_error_pre);
    if (std::isnan(prediction_error_post)) sqlite3_bind_null(stmt, 8); else sqlite3_bind_double(stmt, 8, prediction_error_post);
    if (std::isnan(coherence_pre)) sqlite3_bind_null(stmt, 9); else sqlite3_bind_double(stmt, 9, coherence_pre);
    if (std::isnan(coherence_post)) sqlite3_bind_null(stmt, 10); else sqlite3_bind_double(stmt, 10, coherence_post);
    if (std::isnan(reward_slope_pre)) sqlite3_bind_null(stmt, 11); else sqlite3_bind_double(stmt, 11, reward_slope_pre);
    if (std::isnan(reward_slope_post)) sqlite3_bind_null(stmt, 12); else sqlite3_bind_double(stmt, 12, reward_slope_post);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertSelfRevisionOutcome: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)revision_id; (void)eval_ts_ms; (void)outcome_class; (void)trust_pre; (void)trust_post; (void)prediction_error_pre; (void)prediction_error_post; (void)coherence_pre; (void)coherence_post; (void)reward_slope_pre; (void)reward_slope_post;
    return false;
#endif
}

std::optional<MemoryDB::SelfRevisionOutcomeEntry> MemoryDB::getLatestSelfRevisionOutcome(std::int64_t run_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return std::nullopt;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "SELECT revision_id, eval_ts_ms, outcome_class, trust_pre, trust_post, prediction_error_pre, prediction_error_post, coherence_pre, coherence_post, reward_slope_pre, reward_slope_post"
        " FROM self_revision_outcomes WHERE run_id = ? ORDER BY eval_ts_ms DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getLatestSelfRevisionOutcome: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    std::optional<SelfRevisionOutcomeEntry> out = std::nullopt;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        SelfRevisionOutcomeEntry e;
        e.revision_id = sqlite3_column_int64(stmt, 0);
        e.eval_ts_ms = sqlite3_column_int64(stmt, 1);
        const char* cls = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        e.outcome_class = cls ? cls : "";
        e.trust_pre = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 3));
        e.trust_post = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 4));
        e.prediction_error_pre = sqlite3_column_type(stmt, 5) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 5));
        e.prediction_error_post = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 6));
        e.coherence_pre = sqlite3_column_type(stmt, 7) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 7));
        e.coherence_post = sqlite3_column_type(stmt, 8) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 8));
        e.reward_slope_pre = sqlite3_column_type(stmt, 9) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 9));
        e.reward_slope_post = sqlite3_column_type(stmt, 10) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 10));
        out = e;
    }
    sqlite3_finalize(stmt);
    return out;
#else
    (void)run_id; return std::nullopt;
#endif
}

std::vector<MemoryDB::SelfRevisionOutcomeEntry> MemoryDB::getRecentSelfRevisionOutcomes(std::int64_t run_id, std::size_t n) {
#ifdef NF_HAVE_SQLITE3
    std::vector<SelfRevisionOutcomeEntry> entries;
    if (!db_ || run_id <= 0 || n == 0) return entries;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "SELECT revision_id, eval_ts_ms, outcome_class, trust_pre, trust_post, prediction_error_pre, prediction_error_post, coherence_pre, coherence_post, reward_slope_pre, reward_slope_post"
        " FROM self_revision_outcomes WHERE run_id = ? ORDER BY eval_ts_ms DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRecentSelfRevisionOutcomes: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return entries;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int(stmt, 2, static_cast<int>(n));
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SelfRevisionOutcomeEntry e;
        e.revision_id = sqlite3_column_int64(stmt, 0);
        e.eval_ts_ms = sqlite3_column_int64(stmt, 1);
        const char* cls = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        e.outcome_class = cls ? cls : "";
        e.trust_pre = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 3));
        e.trust_post = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 4));
        e.prediction_error_pre = sqlite3_column_type(stmt, 5) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 5));
        e.prediction_error_post = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 6));
        e.coherence_pre = sqlite3_column_type(stmt, 7) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 7));
        e.coherence_post = sqlite3_column_type(stmt, 8) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 8));
        e.reward_slope_pre = sqlite3_column_type(stmt, 9) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 9));
        e.reward_slope_post = sqlite3_column_type(stmt, 10) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 10));
        entries.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return entries;
#else
    (void)run_id; (void)n; return {};
#endif
}

std::optional<std::int64_t> MemoryDB::getLatestUnevaluatedSelfRevisionId(std::int64_t run_id, std::int64_t max_ts_ms) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return std::nullopt;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "SELECT s.id"
        " FROM self_revision_log s"
        " LEFT JOIN self_revision_outcomes o ON o.revision_id = s.id"
        " WHERE s.run_id = ? AND o.revision_id IS NULL AND s.ts_ms <= ?"
        " ORDER BY s.ts_ms DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getLatestUnevaluatedSelfRevisionId: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, max_ts_ms);
    std::optional<std::int64_t> out = std::nullopt;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return out;
#else
    (void)run_id; (void)max_ts_ms; return std::nullopt;
#endif
}

std::optional<std::int64_t> MemoryDB::getSelfRevisionTimestamp(std::int64_t revision_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_ || revision_id <= 0) return std::nullopt;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT ts_ms FROM self_revision_log WHERE id = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getSelfRevisionTimestamp: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, revision_id);
    std::optional<std::int64_t> out = std::nullopt;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        out = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return out;
#else
    (void)revision_id; return std::nullopt;
#endif
}

// Phase 12: Self-Consistency logging
bool MemoryDB::insertSelfConsistency(std::int64_t run_id,
                                     std::int64_t ts_ms,
                                     double consistency_score,
                                     const std::string& notes,
                                     const std::string& window_json,
                                     const std::string& driver_explanation,
                                     std::int64_t& out_consistency_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO self_consistency_log (run_id, ts_ms, consistency_score, notes, window_json, driver_explanation)"
        " VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertSelfConsistency: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_double(stmt, 3, consistency_score);
    sqlite3_bind_text(stmt, 4, notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, window_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, driver_explanation.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertSelfConsistency: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_consistency_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted self_consistency_log id=" << out_consistency_id << std::endl;
    return true;
#else
    (void)run_id; (void)ts_ms; (void)consistency_score; (void)notes; (void)window_json; (void)driver_explanation; (void)out_consistency_id; return false;
#endif
}

std::vector<MemoryDB::SelfConsistencyEntry> MemoryDB::getRecentConsistency(std::int64_t run_id, int n) {
#ifdef NF_HAVE_SQLITE3
    std::vector<SelfConsistencyEntry> entries;
    if (!db_) return entries;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT id, ts_ms, consistency_score, notes, window_json, driver_explanation FROM self_consistency_log WHERE run_id = ? ORDER BY ts_ms DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRecentConsistency: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return entries;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int(stmt, 2, n);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SelfConsistencyEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        e.ts_ms = sqlite3_column_int64(stmt, 1);
        e.consistency_score = sqlite3_column_type(stmt, 2) == SQLITE_NULL ? 0.0 : sqlite3_column_double(stmt, 2);
        const char* notes = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (notes) e.notes.assign(notes);
        const char* window = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        if (window) e.window_json.assign(window);
        const char* driver = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        if (driver) e.driver_explanation.assign(driver);
        entries.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return entries;
#else
    (void)run_id; (void)n; return {};
#endif
}

// Phase 13: Autonomy Envelope logging
bool MemoryDB::insertAutonomyDecision(std::int64_t run_id,
                                      std::int64_t ts_ms,
                                      const std::string& decision,
                                      const std::string& driver_json,
                                      std::int64_t& out_decision_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO autonomy_envelope_log (run_id, ts_ms, decision, driver_json) VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertAutonomyDecision: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_text(stmt, 3, decision.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, driver_json.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertAutonomyDecision: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_decision_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted autonomy_envelope_log id=" << out_decision_id << std::endl;
    return true;
#else
    (void)run_id; (void)ts_ms; (void)decision; (void)driver_json; (void)out_decision_id; return false;
#endif
}

std::vector<MemoryDB::AutonomyDecisionEntry> MemoryDB::getRecentAutonomyDecisions(std::int64_t run_id, int n) {
#ifdef NF_HAVE_SQLITE3
    std::vector<AutonomyDecisionEntry> entries;
    if (!db_) return entries;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT id, ts_ms, decision, driver_json FROM autonomy_envelope_log WHERE run_id = ? ORDER BY ts_ms DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRecentAutonomyDecisions: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return entries;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int(stmt, 2, n);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        AutonomyDecisionEntry e;
        e.id = sqlite3_column_int64(stmt, 0);
        e.ts_ms = sqlite3_column_int64(stmt, 1);
        const char* dec = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (dec) e.decision.assign(dec);
        const char* drv = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        if (drv) e.driver_json.assign(drv);
        entries.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return entries;
#else
    (void)run_id; (void)n; return {};
#endif
}

std::vector<std::string> MemoryDB::getRecentExplanations(std::int64_t run_id, int n) {
#ifdef NF_HAVE_SQLITE3
    std::vector<std::string> explanations;
    if (!db_) return explanations;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT self_explanation_json FROM metacognition WHERE run_id = ? AND self_explanation_json IS NOT NULL ORDER BY ts_ms DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRecentExplanations: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return explanations;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int(stmt, 2, n);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* explanation = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (explanation) {
            explanations.emplace_back(explanation);
        }
    }
    sqlite3_finalize(stmt);
    return explanations;
#else
    (void)run_id; (void)n; return {};
#endif
}

std::vector<MemoryDB::MetacognitionEntry> MemoryDB::getRecentMetacognition(std::int64_t run_id, int n) {
#ifdef NF_HAVE_SQLITE3
    std::vector<MetacognitionEntry> entries;
    if (!db_) return entries;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT id, ts_ms, self_trust, narrative_rmse, goal_mae, ece, trust_delta, coherence_delta, goal_accuracy_delta FROM metacognition WHERE run_id = ? ORDER BY ts_ms DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRecentMetacognition: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return entries;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int(stmt, 2, n);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MetacognitionEntry entry;
        entry.id = sqlite3_column_int64(stmt, 0);
        entry.ts_ms = sqlite3_column_int64(stmt, 1);
        entry.self_trust = sqlite3_column_double(stmt, 2);
        entry.narrative_rmse = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 3));
        entry.goal_mae = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 4));
        entry.ece = sqlite3_column_type(stmt, 5) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 5));
        entry.trust_delta = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 6));
        entry.coherence_delta = sqlite3_column_type(stmt, 7) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 7));
        entry.goal_accuracy_delta = sqlite3_column_type(stmt, 8) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 8));
        entries.push_back(entry);
    }
    sqlite3_finalize(stmt);
    return entries;
#else
    (void)run_id; (void)n; return {};
#endif
}

std::vector<MemoryDB::MetacognitionEntry> MemoryDB::getMetacognitionBetween(std::int64_t run_id,
                                                                            std::int64_t start_ts_ms,
                                                                            std::int64_t end_ts_ms,
                                                                            int limit) {
#ifdef NF_HAVE_SQLITE3
    std::vector<MetacognitionEntry> entries;
    if (!db_ || run_id <= 0 || limit <= 0) return entries;
    if (start_ts_ms > end_ts_ms) std::swap(start_ts_ms, end_ts_ms);
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "SELECT id, ts_ms, self_trust, narrative_rmse, goal_mae, ece, trust_delta, coherence_delta, goal_accuracy_delta"
        " FROM metacognition WHERE run_id = ? AND ts_ms >= ? AND ts_ms <= ?"
        " ORDER BY ts_ms ASC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getMetacognitionBetween: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return entries;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, start_ts_ms);
    sqlite3_bind_int64(stmt, 3, end_ts_ms);
    sqlite3_bind_int(stmt, 4, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        MetacognitionEntry entry;
        entry.id = sqlite3_column_int64(stmt, 0);
        entry.ts_ms = sqlite3_column_int64(stmt, 1);
        entry.self_trust = sqlite3_column_double(stmt, 2);
        entry.narrative_rmse = sqlite3_column_type(stmt, 3) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 3));
        entry.goal_mae = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 4));
        entry.ece = sqlite3_column_type(stmt, 5) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 5));
        entry.trust_delta = sqlite3_column_type(stmt, 6) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 6));
        entry.coherence_delta = sqlite3_column_type(stmt, 7) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 7));
        entry.goal_accuracy_delta = sqlite3_column_type(stmt, 8) == SQLITE_NULL ? std::nullopt : std::make_optional(sqlite3_column_double(stmt, 8));
        entries.push_back(entry);
    }
    sqlite3_finalize(stmt);
    return entries;
#else
    (void)run_id; (void)start_ts_ms; (void)end_ts_ms; (void)limit; return {};
#endif
}

bool MemoryDB::insertAutonomyModulation(std::int64_t run_id,
                                        std::int64_t ts_ms,
                                        double autonomy_score,
                                        const std::string& autonomy_tier,
                                        double autonomy_gain,
                                        int ethics_hard_block,
                                        double ethics_soft_risk,
                                        double pre_rank_entropy,
                                        double post_rank_entropy,
                                        double exploration_bias,
                                        int options_considered,
                                        double option_rank_shift_mean,
                                        double option_rank_shift_max,
                                        std::int64_t selected_option_id,
                                        double decision_confidence,
                                        int autonomy_applied,
                                        const std::string& veto_reason,
                                        std::int64_t& out_modulation_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO autonomy_modulation_log (run_id, ts_ms, autonomy_score, autonomy_tier, autonomy_gain, ethics_hard_block, ethics_soft_risk, pre_rank_entropy, post_rank_entropy, exploration_bias, options_considered, option_rank_shift_mean, option_rank_shift_max, selected_option_id, decision_confidence, autonomy_applied, veto_reason)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertAutonomyModulation: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_double(stmt, 3, autonomy_score);
    sqlite3_bind_text(stmt, 4, autonomy_tier.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, autonomy_gain);
    sqlite3_bind_int(stmt, 6, ethics_hard_block);
    sqlite3_bind_double(stmt, 7, ethics_soft_risk);
    sqlite3_bind_double(stmt, 8, pre_rank_entropy);
    sqlite3_bind_double(stmt, 9, post_rank_entropy);
    sqlite3_bind_double(stmt, 10, exploration_bias);
    sqlite3_bind_int(stmt, 11, options_considered);
    sqlite3_bind_double(stmt, 12, option_rank_shift_mean);
    sqlite3_bind_double(stmt, 13, option_rank_shift_max);
    sqlite3_bind_int64(stmt, 14, selected_option_id);
    sqlite3_bind_double(stmt, 15, decision_confidence);
    sqlite3_bind_int(stmt, 16, autonomy_applied);
    sqlite3_bind_text(stmt, 17, veto_reason.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertAutonomyModulation: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_modulation_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted autonomy_modulation_log id=" << out_modulation_id << std::endl;
    return true;
#else
    (void)run_id;
    (void)ts_ms;
    (void)autonomy_score;
    (void)autonomy_tier;
    (void)autonomy_gain;
    (void)ethics_hard_block;
    (void)ethics_soft_risk;
    (void)pre_rank_entropy;
    (void)post_rank_entropy;
    (void)exploration_bias;
    (void)options_considered;
    (void)option_rank_shift_mean;
    (void)option_rank_shift_max;
    (void)selected_option_id;
    (void)decision_confidence;
    (void)autonomy_applied;
    (void)veto_reason;
    (void)out_modulation_id;
    return false;
#endif
}

bool MemoryDB::insertParameterHistory(std::int64_t run_id,
                                      std::int64_t revision_id,
                                      int phase,
                                      const std::string& param,
                                      double value,
                                      std::int64_t ts_ms) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "INSERT INTO parameter_history (run_id, ts_ms, phase, parameter, value, revision_id) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertParameterHistory: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int(stmt, 3, phase);
    sqlite3_bind_text(stmt, 4, param.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, value);
    if (revision_id > 0) sqlite3_bind_int64(stmt, 6, revision_id); else sqlite3_bind_null(stmt, 6);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertParameterHistory: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    return ok;
#else
    (void)run_id; (void)revision_id; (void)phase; (void)param; (void)value; (void)ts_ms; return false;
#endif
}

std::vector<MemoryDB::ParameterRecord> MemoryDB::getRecentParamHistory(std::int64_t run_id, std::size_t n) {
#ifdef NF_HAVE_SQLITE3
    std::vector<ParameterRecord> records;
    if (!db_) return records;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT ts_ms, phase, parameter, value, revision_id FROM parameter_history WHERE run_id = ? ORDER BY ts_ms DESC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getRecentParamHistory: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return records;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int(stmt, 2, static_cast<int>(n));
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ParameterRecord rec;
        rec.ts_ms = sqlite3_column_int64(stmt, 0);
        rec.phase = sqlite3_column_int(stmt, 1);
        const char* p = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (p) rec.parameter.assign(p);
        rec.value = sqlite3_column_double(stmt, 3);
        rec.revision_id = sqlite3_column_type(stmt, 4) == SQLITE_NULL ? 0 : sqlite3_column_int64(stmt, 4);
        records.push_back(std::move(rec));
    }
    sqlite3_finalize(stmt);
    return records;
#else
    (void)run_id; (void)n; return {};
#endif
}
// keep namespace open for helper and method definitions
static std::vector<float> parse_vec_json(const char* js) {
#ifdef NF_HAVE_SQLITE3
    std::vector<float> v;
    if (!js) return v;
    const char* p = std::strstr(js, "[");
    const char* q = p ? std::strrchr(js, ']') : nullptr;
    if (!p || !q || q <= p) return v;
    std::string s(p+1, q - p - 1);
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == ',')) ++i;
        std::size_t j = i;
        while (j < s.size() && s[j] != ',') ++j;
        if (j > i) {
            try { v.push_back(static_cast<float>(std::stod(s.substr(i, j - i)))); } catch (...) {}
        }
        i = j + 1;
    }
    return v;
#else
    std::vector<float> v; return v;
#endif
}

std::vector<NeuroForge::Core::MemoryDB::EmbeddingEntry> NeuroForge::Core::MemoryDB::getEmbeddings(std::int64_t run_id,
                                              const std::string& state_type,
                                              int limit) {
#ifdef NF_HAVE_SQLITE3
    std::vector<EmbeddingEntry> out;
    if (!db_ || run_id <= 0 || limit <= 0) return out;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql = "SELECT ts_ms, step, content_id, state_type, vec_json, COALESCE(meta_json,'') FROM embeddings WHERE run_id = ? AND state_type = ? ORDER BY step ASC LIMIT ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return out;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_text(stmt, 2, state_type.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        EmbeddingEntry e;
        e.ts_ms = sqlite3_column_int64(stmt, 0);
        e.step = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 1));
        const unsigned char* cid = sqlite3_column_text(stmt, 2);
        const unsigned char* st = sqlite3_column_text(stmt, 3);
        const unsigned char* vj = sqlite3_column_text(stmt, 4);
        const unsigned char* mj = sqlite3_column_text(stmt, 5);
        e.content_id = cid ? reinterpret_cast<const char*>(cid) : "";
        e.state_type = st ? reinterpret_cast<const char*>(st) : "";
        e.vec = parse_vec_json(vj ? reinterpret_cast<const char*>(vj) : "");
        e.meta_json = mj ? reinterpret_cast<const char*>(mj) : "";
        out.push_back(std::move(e));
    }
    sqlite3_finalize(stmt);
    return out;
#else
    (void)run_id; (void)state_type; (void)limit; std::vector<EmbeddingEntry> out; return out;
#endif
}
// Unified Self System: minimal read APIs for SelfModel (Step 2)
bool MemoryDB::insertPersonalityHistory(std::int64_t run_id,
                                        std::int64_t ts_ms,
                                        std::uint64_t step,
                                        const std::string& trait_json,
                                        int proposal,
                                        int approved,
                                        std::optional<int> source_phase,
                                        std::optional<std::int64_t> revision_id,
                                        const std::string& notes,
                                        std::int64_t& out_personality_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "INSERT INTO personality_history (run_id, ts_ms, step, trait_json, proposal, approved, source_phase, revision_id, notes) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for insertPersonalityHistory: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    sqlite3_bind_int64(stmt, 2, ts_ms);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(step));
    sqlite3_bind_text(stmt, 4, trait_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, proposal);
    sqlite3_bind_int(stmt, 6, approved);
    if (source_phase.has_value()) {
        sqlite3_bind_int(stmt, 7, source_phase.value());
    } else {
        sqlite3_bind_null(stmt, 7);
    }
    if (revision_id.has_value() && revision_id.value() > 0) {
        sqlite3_bind_int64(stmt, 8, revision_id.value());
    } else {
        sqlite3_bind_null(stmt, 8);
    }
    sqlite3_bind_text(stmt, 9, notes.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for insertPersonalityHistory: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(stmt);
    if (!ok) return false;
    out_personality_id = sqlite3_last_insert_rowid(static_cast<sqlite3*>(db_));
    if (debug_) std::cerr << "[MemoryDB] inserted personality_history id=" << out_personality_id << std::endl;
    return true;
#else
    (void)run_id;
    (void)ts_ms;
    (void)step;
    (void)trait_json;
    (void)proposal;
    (void)approved;
    (void)source_phase;
    (void)revision_id;
    (void)notes;
    (void)out_personality_id;
    return false;
#endif
}

bool MemoryDB::approvePersonalityProposal(std::int64_t personality_id,
                                          const std::string& approver,
                                          const std::string& rationale) {
#ifdef NF_HAVE_SQLITE3
    if (!db_) return false;
    std::lock_guard<std::mutex> lg(m_);

    const char* select_sql =
        "SELECT notes FROM personality_history WHERE id = ? AND proposal = 1 AND approved = 0;";
    sqlite3_stmt* select_stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), select_sql, -1, &select_stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for approvePersonalityProposal select: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_int64(select_stmt, 1, personality_id);
    std::string existing_notes;
    int rc = sqlite3_step(select_stmt);
    if (rc == SQLITE_ROW) {
        const unsigned char* notes_text = sqlite3_column_text(select_stmt, 0);
        if (notes_text) {
            existing_notes.assign(reinterpret_cast<const char*>(notes_text));
        }
    } else {
        sqlite3_finalize(select_stmt);
        return false;
    }
    sqlite3_finalize(select_stmt);

    std::string appended_notes;
    std::string approval_entry;
    approval_entry.reserve(64 + approver.size() + rationale.size());
    approval_entry += "[approval] approver=";
    approval_entry += approver;
    approval_entry += " rationale=";
    approval_entry += rationale;
    if (existing_notes.empty()) {
        appended_notes = approval_entry;
    } else {
        appended_notes = existing_notes;
        appended_notes += "\n";
        appended_notes += approval_entry;
    }

    const char* update_sql =
        "UPDATE personality_history SET approved = 1, notes = ? WHERE id = ? AND proposal = 1 AND approved = 0;";
    sqlite3_stmt* update_stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), update_sql, -1, &update_stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for approvePersonalityProposal update: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return false;
    }
    sqlite3_bind_text(update_stmt, 1, appended_notes.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(update_stmt, 2, personality_id);
    bool ok = (sqlite3_step(update_stmt) == SQLITE_DONE);
    if (!ok && debug_) std::cerr << "[MemoryDB] step failed for approvePersonalityProposal update: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
    sqlite3_finalize(update_stmt);
    return ok;
#else
    (void)personality_id;
    (void)approver;
    (void)rationale;
    return false;
#endif
}

std::optional<MemoryDB::SelfConceptRow> MemoryDB::getLatestSelfConcept(std::int64_t run_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_ || run_id <= 0) return std::nullopt;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "SELECT id, ts_ms, step, identity_vector_json, confidence, notes "
        "FROM self_concept WHERE run_id = ? "
        "ORDER BY ts_ms DESC, step DESC, id DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getLatestSelfConcept: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    std::optional<SelfConceptRow> out;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        SelfConceptRow row;
        row.id = sqlite3_column_int64(stmt, 0);
        row.ts_ms = sqlite3_column_int64(stmt, 1);
        row.step = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 2));
        const unsigned char* identity_json = sqlite3_column_text(stmt, 3);
        row.identity_vector_json = identity_json ? reinterpret_cast<const char*>(identity_json) : std::string();
        if (sqlite3_column_type(stmt, 4) != SQLITE_NULL) {
            row.confidence = sqlite3_column_double(stmt, 4);
        }
        const unsigned char* notes = sqlite3_column_text(stmt, 5);
        row.notes = notes ? reinterpret_cast<const char*>(notes) : std::string();
        out = row;
    }
    sqlite3_finalize(stmt);
    return out;
#else
    (void)run_id;
    return std::nullopt;
#endif
}

std::optional<MemoryDB::PersonalityRow> MemoryDB::getLatestApprovedPersonality(std::int64_t run_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_ || run_id <= 0) return std::nullopt;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "SELECT id, ts_ms, step, trait_json, proposal, approved, source_phase, revision_id, notes "
        "FROM personality_history WHERE run_id = ? AND approved = 1 "
        "ORDER BY ts_ms DESC, step DESC, id DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getLatestApprovedPersonality: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    std::optional<PersonalityRow> out;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        PersonalityRow row;
        row.id = sqlite3_column_int64(stmt, 0);
        row.ts_ms = sqlite3_column_int64(stmt, 1);
        row.step = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 2));
        const unsigned char* trait_json = sqlite3_column_text(stmt, 3);
        row.trait_json = trait_json ? reinterpret_cast<const char*>(trait_json) : std::string();
        row.proposal = sqlite3_column_int(stmt, 4);
        row.approved = sqlite3_column_int(stmt, 5);
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            row.source_phase = sqlite3_column_int(stmt, 6);
        }
        if (sqlite3_column_type(stmt, 7) != SQLITE_NULL) {
            row.revision_id = sqlite3_column_int64(stmt, 7);
        }
        const unsigned char* notes = sqlite3_column_text(stmt, 8);
        row.notes = notes ? reinterpret_cast<const char*>(notes) : std::string();
        out = row;
    }
    sqlite3_finalize(stmt);
    return out;
#else
    (void)run_id;
    return std::nullopt;
#endif
}

std::optional<MemoryDB::SocialSelfRow> MemoryDB::getLatestSocialSelf(std::int64_t run_id) {
#ifdef NF_HAVE_SQLITE3
    if (!db_ || run_id <= 0) return std::nullopt;
    std::lock_guard<std::mutex> lg(m_);
    const char* sql =
        "SELECT id, ts_ms, step, role, norm_json, reputation, confidence, notes "
        "FROM social_self WHERE run_id = ? "
        "ORDER BY ts_ms DESC, step DESC, id DESC LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        if (debug_) std::cerr << "[MemoryDB] prepare failed for getLatestSocialSelf: " << sqlite3_errmsg(static_cast<sqlite3*>(db_)) << std::endl;
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, run_id);
    std::optional<SocialSelfRow> out;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        SocialSelfRow row;
        row.id = sqlite3_column_int64(stmt, 0);
        row.ts_ms = sqlite3_column_int64(stmt, 1);
        row.step = static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 2));
        const unsigned char* role = sqlite3_column_text(stmt, 3);
        row.role = role ? reinterpret_cast<const char*>(role) : std::string();
        const unsigned char* norm_json = sqlite3_column_text(stmt, 4);
        row.norm_json = norm_json ? reinterpret_cast<const char*>(norm_json) : std::string();
        if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
            row.reputation = sqlite3_column_double(stmt, 5);
        }
        if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
            row.confidence = sqlite3_column_double(stmt, 6);
        }
        const unsigned char* notes = sqlite3_column_text(stmt, 7);
        row.notes = notes ? reinterpret_cast<const char*>(notes) : std::string();
        out = row;
    }
    sqlite3_finalize(stmt);
    return out;
#else
    (void)run_id;
    return std::nullopt;
#endif
}

} // namespace Core
} // namespace NeuroForge
