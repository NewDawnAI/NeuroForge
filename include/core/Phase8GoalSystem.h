#pragma once

#include <string>
#include <memory>
#include <optional>
#include <cstdint>
#include <unordered_map>

namespace NeuroForge {
namespace Core {

class MemoryDB;
class Phase7Reflection;
class Phase9Metacognition; // forward declaration for Phase 9 integration
class SelfModel;
class AutonomyEnvelope;

/**
 * Phase 8: Goal System
 * 
 * Manages hierarchical goal formation and motivation tracking based on reflections.
 * Ingests goals from Phase 7 reflections and maintains goal stability and coherence.
 */
class Phase8GoalSystem {
public:
    explicit Phase8GoalSystem(std::shared_ptr<MemoryDB> memory_db, std::int64_t run_id);
    ~Phase8GoalSystem() = default;

    // Core goal ingestion from reflections
    bool ingestReflection(const Phase7Reflection& reflection, std::int64_t reflection_id);
    bool ingestReflection(std::int64_t reflection_id, const std::string& title, const std::string& rationale_json, double impact);
    
    // Motivation state tracking
    bool updateMotivationState(double motivation, double coherence, const std::string& notes = "");
    
    // Goal hierarchy management
    bool createGoal(const std::string& description, double priority = 0.5, double stability = 0.5, 
                   std::optional<std::int64_t> origin_reflection_id = std::nullopt);
    bool linkGoals(std::int64_t parent_goal_id, std::int64_t child_goal_id, double weight = 1.0);
    bool updateGoalStability(std::int64_t goal_id, double stability);

    // Background decay for unused goals (uniform slow decay)
    void decayStability(double dt_seconds);
    
    // Goal retrieval
    std::optional<std::int64_t> findGoalByDescription(const std::string& description);

    // Set Phase 9 metacognition (optional)
    void setPhase9Metacognition(Phase9Metacognition* meta) { metacog_ = meta; }

    void setSelfModel(SelfModel* self_model) { self_model_ = self_model; }

    void setAutonomyEnvelope(const AutonomyEnvelope* env) { autonomy_env_ = env; }

    // Getters
    std::int64_t getRunId() const noexcept { return run_id_; }
    double getLastCoherence() const noexcept { return last_coherence_; }

private:
    std::shared_ptr<MemoryDB> memory_db_;
    std::int64_t run_id_;
    double last_coherence_{0.5};

    // Local cache for goal stability to enable decay without DB reads
    std::unordered_map<std::int64_t, double> goal_stability_cache_;
    std::int64_t last_decay_ms_{0};
    
    // Internal helpers
    bool extractGoalsFromReflection(const std::string& reflection_text, std::int64_t reflection_id);
    double calculateGoalCoherence();
    std::int64_t now_ms() const;

    // Phase 9 metacognition bridge
    Phase9Metacognition* metacog_{nullptr};

    SelfModel* self_model_{nullptr};

    const AutonomyEnvelope* autonomy_env_{nullptr};
    std::unordered_map<std::string, int> last_goal_context_;
};

} // namespace Core
} // namespace NeuroForge
