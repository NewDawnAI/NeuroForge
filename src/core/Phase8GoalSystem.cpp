#include "core/Phase8GoalSystem.h"
#include "core/MemoryDB.h"
#include "core/Phase7Reflection.h"
#include "core/Phase9Metacognition.h"
#include "core/SelfModel.h"
#include "core/AutonomyEnvelope.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace NeuroForge {
namespace Core {

Phase8GoalSystem::Phase8GoalSystem(std::shared_ptr<MemoryDB> memory_db, std::int64_t run_id)
    : memory_db_(std::move(memory_db)), run_id_(run_id) {}

bool Phase8GoalSystem::ingestReflection(const Phase7Reflection& /*reflection*/, std::int64_t reflection_id) {
    // Placeholder: Phase7Reflection doesn't expose title/rationale/impact getters yet
    const std::string title = std::string("Reflection ") + std::to_string(reflection_id);
    const std::string rationale_json = "{}";
    const double impact = 0.5;
    return ingestReflection(reflection_id, title, rationale_json, impact);
}

bool Phase8GoalSystem::ingestReflection(std::int64_t reflection_id, const std::string& title, const std::string& rationale_json, double impact) {
    auto existing_goal_id = findGoalByDescription(title);
    double identity_conf = 0.5;
    double social_reputation = 0.5;
    if (self_model_ && self_model_->isLoaded()) {
        const auto& id = self_model_->identity();
        if (id.confidence.has_value()) {
            identity_conf = std::min(1.0, std::max(0.0, id.confidence.value()));
        }
        const auto& soc = self_model_->social();
        if (soc.reputation.has_value()) {
            social_reputation = std::min(1.0, std::max(0.0, soc.reputation.value()));
        }
    }
    double stability_bias = 1.0 + 0.1 * (identity_conf - 0.5);
    double priority_bias = 1.0 + 0.1 * (social_reputation - 0.5);
    if (existing_goal_id.has_value()) {
        double new_stability = std::clamp((0.5 + impact * 0.5) * stability_bias, 0.0, 1.0);
        bool ok = updateGoalStability(existing_goal_id.value(), new_stability);
        return ok;
    } else {
        double priority = std::clamp(impact * priority_bias, 0.0, 1.0);
        double stability = std::clamp((0.4 + impact * 0.4) * stability_bias, 0.0, 1.0);
        return createGoal(title, priority, stability, reflection_id);
    }
}

bool Phase8GoalSystem::updateMotivationState(double motivation, double coherence, const std::string& notes) {
    last_coherence_ = coherence;
    std::int64_t motivation_id = 0;
    bool ok = memory_db_->insertMotivationState(now_ms(), motivation, coherence, notes, run_id_, motivation_id);
    
    // Apply slow background decay proportional to negative affect drift (1 - coherence)
    double decay_rate_per_sec = 0.01 + (1.0 - coherence) * 0.02; // base 1%/s + up to 2%/s when incoherent
    // Trust-weighted scaling: slow decay when trust is high, faster when trust is low
    if (metacog_) {
        double trust = std::min(1.0, std::max(0.0, metacog_->getSelfTrust()));
        const double trust_scale = 0.8 + 0.4 * (1.0 - trust); // trust=1 -> 0.8x, trust=0 -> 1.2x
        decay_rate_per_sec *= trust_scale;
    }

    static std::int64_t last_update_ms = 0;
    const std::int64_t now = now_ms();
    if (last_update_ms == 0) {
        last_update_ms = now;
    }
    double dt = (now - last_update_ms) / 1000.0;
    last_update_ms = now;
    // Decay cached goals; if nothing cached yet, this is a no-op
    if (dt > 0.0) {
        // scale by per-second rate
        decayStability(decay_rate_per_sec * dt);
    }

    // Phase 9: resolve actuals using coherence and approximate goal shift from decay scalar
    if (metacog_) {
        const double goal_shift_scalar = decay_rate_per_sec * std::max(0.0, dt);
        metacog_->resolveActuals(coherence, goal_shift_scalar, notes);
    }

    return ok;
}

bool Phase8GoalSystem::createGoal(const std::string& description, double priority, double stability,
                                  std::optional<std::int64_t> origin_reflection_id) {
    std::int64_t goal_id = 0;
    bool ok = memory_db_->insertGoalNode(description, priority, stability, run_id_, origin_reflection_id, goal_id);
    if (ok) {
        goal_stability_cache_[goal_id] = stability;
        if (autonomy_env_ && autonomy_env_->valid) {
            last_goal_context_["autonomy_tier"] = static_cast<int>(autonomy_env_->tier);
        }
    }
    return ok;
}

bool Phase8GoalSystem::linkGoals(std::int64_t parent_goal_id, std::int64_t child_goal_id, double weight) {
    return memory_db_->insertGoalEdge(parent_goal_id, child_goal_id, weight);
}

bool Phase8GoalSystem::updateGoalStability(std::int64_t goal_id, double stability) {
    bool ok = memory_db_->updateGoalStability(goal_id, stability);
    if (ok) {
        goal_stability_cache_[goal_id] = stability;
        if (autonomy_env_ && autonomy_env_->valid) {
            last_goal_context_["autonomy_tier"] = static_cast<int>(autonomy_env_->tier);
        }
    }
    return ok;
}

std::vector<std::pair<std::int64_t, double>> Phase8GoalSystem::getSubGoals(std::int64_t goal_id) {
    if (!memory_db_) return {};
    return memory_db_->getChildGoals(goal_id);
}

std::vector<std::pair<std::string, double>> Phase8GoalSystem::getSubGoalsWithDescriptions(std::int64_t goal_id) {
    if (!memory_db_) return {};
    return memory_db_->getChildGoalsWithDescriptions(goal_id);
}

void Phase8GoalSystem::decayStability(double dt_seconds) {
    // Uniform decay across cached goals; clamp to [0,1]
    if (goal_stability_cache_.empty()) return;

    for (auto& kv : goal_stability_cache_) {
        const std::int64_t goal_id = kv.first;
        double s = kv.second;
        s = std::clamp(s - dt_seconds, 0.0, 1.0);
        // Persist and update cache
        memory_db_->updateGoalStability(goal_id, s);
        kv.second = s;
    }
    last_decay_ms_ = now_ms();
}

std::optional<std::int64_t> Phase8GoalSystem::findGoalByDescription(const std::string& description) {
    return memory_db_->findGoalByDescription(description, run_id_);
}

std::optional<std::string> Phase8GoalSystem::getGoalDescription(std::int64_t goal_id) {
    return memory_db_->getGoalDescription(goal_id);
}

bool Phase8GoalSystem::extractGoalsFromReflection(const std::string& reflection_text, std::int64_t reflection_id) {
    // Placeholder: parse reflection_text to identify potential goals
    // Future: NLP extraction of intents and mapping to goal descriptions
    return true;
}

double Phase8GoalSystem::calculateGoalCoherence() {
    // Compute coherence as a function of goal stability distribution and autonomy tier
    if (goal_stability_cache_.empty()) {
        // Fall back to last recorded coherence when no goals are cached
        return last_coherence_;
    }

    // Mean stability across tracked goals
    double sum_stability = 0.0;
    for (const auto& kv : goal_stability_cache_) {
        const double s = std::clamp(kv.second, 0.0, 1.0);
        sum_stability += s;
    }
    const double count = static_cast<double>(goal_stability_cache_.size());
    const double mean_stability = sum_stability / std::max(1.0, count);

    // Average absolute deviation captures how spread-out the goals are
    double sum_abs_dev = 0.0;
    for (const auto& kv : goal_stability_cache_) {
        const double s = std::clamp(kv.second, 0.0, 1.0);
        sum_abs_dev += std::abs(s - mean_stability);
    }
    const double mean_abs_dev = sum_abs_dev / std::max(1.0, count);

    // High stability and low spread increase coherence
    double coherence = mean_stability * (1.0 - 0.5 * mean_abs_dev);
    coherence = std::clamp(coherence, 0.0, 1.0);

    // Autonomy tier gently boosts coherence when operating at higher autonomy levels
    if (autonomy_env_ && autonomy_env_->valid) {
        const int tier = static_cast<int>(autonomy_env_->tier);
        const double tier_boost = 0.02 * static_cast<double>(tier);
        coherence = std::clamp(coherence + tier_boost, 0.0, 1.0);
    }

    // Persist last coherence for Phase 7 access
    last_coherence_ = coherence;
    return coherence;
}

std::int64_t Phase8GoalSystem::now_ms() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

} // namespace Core
} // namespace NeuroForge
