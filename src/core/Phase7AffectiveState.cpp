#include "core/Phase7AffectiveState.h"
#include "core/MemoryDB.h"
#include <chrono>
#include <algorithm>
#include <cmath>

namespace NeuroForge {
namespace Core {

static std::int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

Phase7AffectiveState::Phase7AffectiveState(MemoryDB* memdb, std::int64_t run_id)
    : memdb_(memdb), run_id_(run_id) {
    // Initialize with neutral state
    current_state_.valence = 0.0;
    current_state_.arousal = 0.0;
    current_state_.focus = 0.5;
}

void Phase7AffectiveState::updateFromReward(double observed_reward, double drift) {
    update_count_++;
    
    // Update valence based on reward
    // Positive rewards increase valence, negative decrease it
    double reward_impact = std::tanh(observed_reward * 2.0); // Bounded to [-1, 1]
    current_state_.valence = current_state_.valence * VALENCE_DECAY + reward_impact * (1.0 - VALENCE_DECAY);
    
    // Update arousal based on absolute reward magnitude and drift
    double excitement = std::min(1.0, std::abs(observed_reward) + std::abs(drift) * 0.5);
    current_state_.arousal = current_state_.arousal * AROUSAL_DECAY + excitement * (1.0 - AROUSAL_DECAY);
    
    // Update focus based on consistency (low drift = high focus)
    double consistency = std::exp(-std::abs(drift) * 2.0); // High consistency when drift is low
    current_state_.focus = current_state_.focus * FOCUS_DECAY + consistency * (1.0 - FOCUS_DECAY);
    
    // Update rolling averages
    double alpha = 1.0 / std::min(100.0, static_cast<double>(update_count_));
    avg_valence_ = avg_valence_ * (1.0 - alpha) + current_state_.valence * alpha;
    avg_arousal_ = avg_arousal_ * (1.0 - alpha) + current_state_.arousal * alpha;
    
    // Clamp values to valid ranges
    current_state_.valence = std::max(-1.0, std::min(1.0, current_state_.valence));
    current_state_.arousal = std::max(0.0, std::min(1.0, current_state_.arousal));
    current_state_.focus = std::max(0.0, std::min(1.0, current_state_.focus));
    
    // Persist to database
    persistState(now_ms());
}

void Phase7AffectiveState::persistState(std::int64_t ts_ms) {
    if (!memdb_) return;
    
    // Upsert affective state record (notes left empty for now)
    (void)memdb_->upsertAffectiveState(ts_ms,
                                       current_state_.valence,
                                       current_state_.arousal,
                                       current_state_.focus,
                                       std::string{},
                                       run_id_);
}

} // namespace Core
} // namespace NeuroForge