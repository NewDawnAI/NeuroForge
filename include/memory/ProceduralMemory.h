#pragma once

#include "core/Types.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <random>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace NeuroForge {
namespace Memory {

// ─────────────────────────────────────────────────────────────────
//  ProceduralTrace: PRIMARY trace storage.
//    A single context→action learned correspondence.
//    NO strings. NO text. Just weight patterns.
// ─────────────────────────────────────────────────────────────────
struct ProceduralTrace {
  std::vector<float> context_pattern; ///< PRIMARY: situation encoding
  std::vector<float> action_pattern;  ///< PRIMARY: action encoding
  float strength = 0.0f;              ///< Synapse-like: grows with practice
  uint32_t practice_count = 0;
  std::chrono::steady_clock::time_point last_practiced;
  bool automated = false; // fires without cognition when > threshold
};

// ─────────────────────────────────────────────────────────────────
//  DEPRECATED: Legacy Skill — use reinforce()/recall() instead.
//  String fields are DEBUG TAGs; primary data is in traces_/weights_.
// ─────────────────────────────────────────────────────────────────
struct [[deprecated(
    "Use reinforce()/recall() via weight matrix; Skill is a debug wrapper")]]
Skill {
  std::uint64_t id;
  std::string name; ///< DEBUG TAG — human label
  std::vector<std::string>
      action_sequence; ///< DEBUG TAG — human-readable steps
  std::vector<float> motor_pattern;
  float proficiency_level = 0.0f;
  std::uint32_t practice_count = 0;
  std::chrono::steady_clock::time_point last_practiced;
  bool automated = false;
};

// ─────────────────────────────────────────────────────────────────
//  DEPRECATED: Legacy MotorAction — use reinforce()/recall() instead.
// ─────────────────────────────────────────────────────────────────
struct [[deprecated(
    "Use reinforce()/recall(); MotorAction is a debug wrapper")]] MotorAction {
  std::string action_name; ///< DEBUG TAG — human label
  std::vector<float> motor_commands;
  float execution_time = 0.0f;
  float success_rate = 1.0f;
  std::vector<std::string> prerequisites; ///< DEBUG TAG — human-readable list
};

// ─────────────────────────────────────────────────────────────────
//  DEPRECATED: Legacy Habit — use reinforce()/recall() instead.
// ─────────────────────────────────────────────────────────────────
struct [[deprecated("Use reinforce()/recall(); Habit is a debug wrapper")]]
Habit {
  std::uint64_t id;
  std::string trigger_context; ///< DEBUG TAG — human label
  std::string habitual_action; ///< DEBUG TAG — human label
  float strength = 0.0f;
  std::uint32_t repetition_count = 0;
  std::chrono::steady_clock::time_point formation_start;
};

// ─────────────────────────────────────────────────────────────────
//  Configuration
// ─────────────────────────────────────────────────────────────────
struct ProceduralConfig {
  std::size_t max_skills = 1000;
  std::size_t max_habits = 500;
  float learning_rate = 0.1f;
  float automation_threshold = 0.8f;
  float habit_formation_threshold = 0.7f;
  std::uint32_t min_repetitions_for_habit = 21;

  // Weight-based region config (new)
  std::size_t context_dim = 64;    // dimensionality of context patterns
  std::size_t action_dim = 16;     // dimensionality of action patterns
  float weight_decay = 0.001f;     // L2 regularization per step
  float recall_temperature = 0.5f; // softmax temperature for recall
};

// ─────────────────────────────────────────────────────────────────
//  Statistics
// ─────────────────────────────────────────────────────────────────
struct ProceduralStats {
  std::size_t total_skills = 0;
  std::size_t automated_skills = 0;
  std::size_t active_habits = 0;
  float average_proficiency = 0.0f;
  std::uint32_t total_practice_sessions = 0;

  // Weight-based stats (new)
  std::size_t total_traces = 0;
  float average_trace_strength = 0.0f;
  float weight_matrix_norm = 0.0f;
  std::size_t total_reinforcements = 0;
  std::size_t total_recalls = 0;
};

// ─────────────────────────────────────────────────────────────────
//  ProceduralMemory
//
//  PRIMARY: weight-based neural region (context→action weights).
//  LEGACY (DEPRECATED): Skill/MotorAction/Habit adapters.
//  BrainPersistence serializes ONLY traces_ + weights_.
// ─────────────────────────────────────────────────────────────────
class ProceduralMemory {
public:
  explicit ProceduralMemory(
      const ProceduralConfig &config = ProceduralConfig{});
  ~ProceduralMemory() = default;

  // ═══════════════════════════════════════════════════════════
  //  WEIGHT-BASED API  (the real memory — context→action weights)
  // ═══════════════════════════════════════════════════════════

  /// Reinforce a context→action association using ULS-like learning
  /// reward > 0 strengthens, reward < 0 weakens
  void reinforce(const std::vector<float> &context,
                 const std::vector<float> &action, float reward);

  /// Recall: given context pattern, return best action pattern via weight
  /// matrix
  std::vector<float> recall(const std::vector<float> &context) const;

  /// Get the raw context→action weight matrix [context_dim × action_dim]
  const std::vector<float> &getWeightMatrix() const { return weights_; }

  /// Get all stored traces (for Cap'n Proto serialization)
  const std::vector<ProceduralTrace> &getTraces() const { return traces_; }

  /// Load traces from Cap'n Proto deserialization
  void loadTraces(const std::vector<ProceduralTrace> &traces);

  /// Load weight matrix directly (Cap'n Proto restore)
  void loadWeights(const std::vector<float> &weights, std::size_t context_dim,
                   std::size_t action_dim);

  /// Apply weight decay (L2 regularization) — call periodically
  void applyWeightDecay(float decay_rate = -1.0f);

  /// Find the trace most similar to a context pattern
  int findClosestTrace(const std::vector<float> &context,
                       float similarity_threshold = 0.5f) const;

  /// Get weight-based stats
  float getWeightNorm() const;
  std::size_t getContextDim() const { return context_dim_; }
  std::size_t getActionDim() const { return action_dim_; }

  // ═══════════════════════════════════════════════════════════
  //  LEGACY API  (adapters — translate to weight operations)
  //  These functions are preserved for backward compatibility
  //  with SkillExtractor, MemoryIntegrator, SleepConsolidation.
  // ═══════════════════════════════════════════════════════════

  // Skill management (legacy)
  std::uint64_t addSkill(const std::string &name,
                         const std::vector<std::string> &action_sequence,
                         const std::vector<float> &motor_pattern = {});
  std::shared_ptr<Skill> getSkill(std::uint64_t skill_id);
  std::shared_ptr<Skill> findSkill(const std::string &name);
  bool removeSkill(std::uint64_t skill_id);

  // Skill practice and learning (legacy → reinforce)
  void practiceSkill(std::uint64_t skill_id, float performance_score);
  void practiceSkill(const std::string &skill_name, float performance_score);
  bool executeSkill(std::uint64_t skill_id,
                    std::vector<float> &output_commands);

  // Motor action management (legacy)
  void addMotorAction(const std::string &action_name,
                      const std::vector<float> &commands,
                      float execution_time = 0.0f);
  std::shared_ptr<MotorAction> getMotorAction(const std::string &action_name);
  bool executeMotorAction(const std::string &action_name,
                          std::vector<float> &commands);

  // Habit formation and management (legacy)
  std::uint64_t startHabitFormation(const std::string &trigger_context,
                                    const std::string &action);
  void reinforceHabit(std::uint64_t habit_id);
  void reinforceHabit(const std::string &trigger_context);
  std::shared_ptr<Habit> getTriggeredHabit(const std::string &context);

  // Automation and chunking (legacy)
  void checkForAutomation();
  void chunkActionSequence(std::uint64_t skill_id,
                           const std::vector<std::size_t> &chunk_indices);
  std::vector<std::uint64_t> getAutomatedSkills() const;

  // Skill transfer and generalization (legacy → weight transfer)
  void transferSkill(std::uint64_t source_skill_id,
                     std::uint64_t target_skill_id,
                     float transfer_amount = 0.1f);
  std::vector<std::uint64_t>
  findSimilarSkills(std::uint64_t skill_id, float similarity_threshold = 0.7f);

  // Memory maintenance
  void decayUnusedSkills(float decay_rate = 0.01f);
  void strengthenFrequentlyUsed();
  void consolidateMotorMemories();

  // Retrieval and search (legacy)
  std::vector<std::shared_ptr<Skill>> getAllSkills() const;
  std::vector<std::shared_ptr<Skill>>
  getSkillsByProficiency(float min_proficiency = 0.0f) const;
  std::vector<std::shared_ptr<Habit>> getActiveHabits() const;

  // Statistics and monitoring
  const ProceduralStats &getStatistics() const { return statistics_; }
  void updateStatistics();
  float getOverallProficiency() const;

  // Configuration
  void updateConfig(const ProceduralConfig &config) { config_ = config; }
  const ProceduralConfig &getConfig() const { return config_; }

private:
  // ─── Weight-based internals ───
  float cosineSimilarity(const std::vector<float> &a,
                         const std::vector<float> &b) const;
  std::vector<float> encodeString(const std::string &s) const;
  std::vector<float>
  encodeActionSequence(const std::vector<std::string> &actions) const;

  // ─── Legacy internals ───
  float calculateSimilarity(const Skill &a, const Skill &b) const;
  void updateProficiency(Skill &skill, float performance_score);
  bool shouldAutomateSkill(const Skill &skill) const;
  void processHabitFormation();

private:
  ProceduralConfig config_;

  // ═══ PRIMARY STORAGE: weight matrix ═══
  std::vector<float> weights_; // [context_dim × action_dim]
  std::size_t context_dim_;
  std::size_t action_dim_;
  std::vector<ProceduralTrace> traces_; // recorded traces for serialization
  mutable std::shared_mutex weights_mutex_;

  // ═══ DEPRECATED LEGACY STORAGE: adapter-only (not serialized) ═══
  std::unordered_map<std::uint64_t, std::shared_ptr<Skill>>
      skills_; // DEBUG/COMPAT
  std::unordered_map<std::string, std::uint64_t>
      skill_name_lookup_; // DEBUG INDEX
  std::unordered_map<std::string, std::shared_ptr<MotorAction>>
      motor_actions_; // DEBUG/COMPAT
  std::unordered_map<std::uint64_t, std::shared_ptr<Habit>>
      habits_; // DEBUG/COMPAT
  std::unordered_map<std::string, std::vector<std::uint64_t>>
      context_habits_; // DEBUG INDEX
  mutable ProceduralStats statistics_;
  std::uint64_t next_skill_id_ = 1;
  std::uint64_t next_habit_id_ = 1;
  mutable std::mt19937 rng_;
};

} // namespace Memory
} // namespace NeuroForge