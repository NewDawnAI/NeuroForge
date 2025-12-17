#pragma once

#include <cstdint>

namespace NeuroForge {
namespace Core {

class MemoryDB;

struct AffectiveState {
    double valence{0.0};    // -1.0 (negative) to +1.0 (positive)
    double arousal{0.0};    // 0.0 (calm) to 1.0 (excited)
    double focus{0.5};      // 0.0 (scattered) to 1.0 (focused)
};

// Phase 7 Affective State Manager
// Tracks emotional state based on reward patterns and drift
class Phase7AffectiveState {
public:
    Phase7AffectiveState(MemoryDB* memdb, std::int64_t run_id);

    // Update affective state from reward and drift
    void updateFromReward(double observed_reward, double drift);

    // Get current affective state
    AffectiveState getState() const { return current_state_; }

    // Get rolling average valence for score modulation
    double getAverageValence() const { return avg_valence_; }

private:
    MemoryDB* memdb_{nullptr};
    std::int64_t run_id_{0};
    
    AffectiveState current_state_;
    
    // Rolling averages for stability
    double avg_valence_{0.0};
    double avg_arousal_{0.0};
    
    // Decay factors
    static constexpr double VALENCE_DECAY = 0.95;
    static constexpr double AROUSAL_DECAY = 0.9;
    static constexpr double FOCUS_DECAY = 0.98;
    
    // Update counters
    std::uint64_t update_count_{0};
    
    // Persist current state to database
    void persistState(std::int64_t ts_ms);
};

} // namespace Core
} // namespace NeuroForge