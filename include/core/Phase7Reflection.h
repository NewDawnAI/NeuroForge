#pragma once

#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Core {

class MemoryDB;
struct AffectiveState;
class Phase8GoalSystem;
class Phase9Metacognition; // forward declaration for Phase 9 wiring
class SelfModel;

// Phase 7 Reflection Generator
// Creates textual reflections based on episode metrics and affective state
class Phase7Reflection {
public:
    Phase7Reflection(MemoryDB* memdb, std::int64_t run_id);

    // Set Phase 8 components (optional)
    void setPhase8Components(Phase8GoalSystem* goals) {
        phase8_goals_ = goals;
    }

    // Set Phase 9 metacognition (optional)
    void setPhase9Metacognition(Phase9Metacognition* meta) {
        metacog_ = meta;
    }

    void setSelfModel(SelfModel* self_model) {
        self_model_ = self_model;
    }

    // Generate reflection at episode end if conditions are met
    void maybeReflect(std::int64_t episode_index, 
                      double contradiction_rate,
                      double avg_reward,
                      double valence,
                      double arousal);

private:
    MemoryDB* memdb_{nullptr};
    std::int64_t run_id_{0};
    
    // Phase 8 components (optional)
    Phase8GoalSystem* phase8_goals_{nullptr};

    // Phase 9 metacognition (optional)
    Phase9Metacognition* metacog_{nullptr};

    SelfModel* self_model_{nullptr};
    
    // Reflection frequency control
    std::int64_t last_reflection_episode_{-1};
    static constexpr std::int64_t MIN_EPISODE_GAP = 2;
    
    // Narrative emission control
    std::uint64_t reflection_count_{0};
    static constexpr std::uint64_t NARRATIVE_PERIOD = 10;
    
    // Generate reflection text based on metrics
    std::string generateReflectionText(std::int64_t episode_index,
                                       double contradiction_rate,
                                       double avg_reward,
                                       double valence,
                                       double arousal);
};

} // namespace Core
} // namespace NeuroForge
