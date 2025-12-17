#pragma once

#include "core/Types.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <random>
#include <mutex>
#include <atomic>
#include <optional>
#include <deque>
#include <algorithm>

namespace NeuroForge {
namespace Core {

// Forward declarations for Agents
class QLearningAgent;
class PPOAgent;
class MetaRLAgent;

// Basic state/action representations for Motor Cortex
struct State {
    std::vector<float> features; // simple feature vector representation
};

using DiscreteAction = int; // index into action space
using ContinuousAction = std::vector<float>; // continuous control vector

struct DiscreteExperience {
    State s;
    DiscreteAction a;
    float r;
    State s_next;
    bool done = false;
};

struct ContinuousExperience {
    State s;
    ContinuousAction a;
    float r;
    State s_next;
    bool done = false;
    float advantage = 0.0f; // placeholder for actor-critic advantage
    float old_log_prob = 0.0f; // placeholder for PPO update
};

// Q-Learning agent for discrete control
class QLearningAgent {
public:
    struct Config {
        int num_actions = 4;
        float alpha = 0.5f;    // learning rate
        float gamma = 0.95f;   // discount
        float epsilon = 0.1f;  // exploration
        float epsilon_min = 0.01f;
        float epsilon_decay = 0.999f; // multiplicative
        int discretization_bins = 10; // for hashing continuous states
    };

private:
    Config cfg_;
    std::unordered_map<std::string, std::vector<float>> qtable_;
    std::mt19937 rng_;

public:
    explicit QLearningAgent(const Config& cfg) : cfg_(cfg), rng_(std::random_device{}()) {}

    void reset() { qtable_.clear(); }

    DiscreteAction choose(const State& s) {
        std::uniform_real_distribution<float> u(0.0f, 1.0f);
        if (u(rng_) < cfg_.epsilon) {
            std::uniform_int_distribution<int> d(0, cfg_.num_actions - 1);
            return d(rng_);
        }
        auto& q = qtable_[hashState(s)];
        if (q.empty()) q.assign(cfg_.num_actions, 0.0f);
        int best = 0; float bestq = q[0];
        for (int a = 1; a < cfg_.num_actions; ++a) {
            if (q[a] > bestq) { bestq = q[a]; best = a; }
        }
        return best;
    }

    void update(const DiscreteExperience& exp) {
        auto key = hashState(exp.s);
        auto next_key = hashState(exp.s_next);
        auto& q = qtable_[key];
        auto& qnext = qtable_[next_key];
        if (q.empty()) q.assign(cfg_.num_actions, 0.0f);
        if (qnext.empty()) qnext.assign(cfg_.num_actions, 0.0f);
        float max_next = qnext[0];
        for (int a = 1; a < cfg_.num_actions; ++a) max_next = std::max(max_next, qnext[a]);
        float target = exp.r + (exp.done ? 0.0f : cfg_.gamma * max_next);
        float td = target - q[exp.a];
        q[exp.a] += cfg_.alpha * td;
        // decay epsilon slightly
        cfg_.epsilon = std::max(cfg_.epsilon_min, cfg_.epsilon * cfg_.epsilon_decay);
    }

    const Config& getConfig() const { return cfg_; }
    void setConfig(const Config& c) { cfg_ = c; }

private:
    std::string hashState(const State& s) const {
        // simple coarse discretization for hashing
        std::string key;
        key.reserve(s.features.size() * 3);
        for (float v : s.features) {
            int bin = static_cast<int>((v + 1.0f) * 0.5f * cfg_.discretization_bins);
            if (bin < 0) bin = 0; if (bin >= cfg_.discretization_bins) bin = cfg_.discretization_bins - 1;
            key.push_back(static_cast<char>('A' + (bin % 26)));
        }
        return key;
    }
};

// Minimal PPO-like agent for continuous control (stubbed but usable)
class PPOAgent {
public:
    struct Config {
        int action_dim = 2;
        float learning_rate = 0.05f;
        float clip_epsilon = 0.2f;
        float init_sigma = 0.3f;
        float sigma_min = 0.05f;
        float sigma_decay = 0.999f;
    };

private:
    Config cfg_;
    std::vector<float> mean_; // simple state-independent mean policy
    float sigma_;
    std::mt19937 rng_;

public:
    explicit PPOAgent(const Config& cfg) : cfg_(cfg), mean_(cfg.action_dim, 0.0f), sigma_(cfg.init_sigma), rng_(std::random_device{}()) {}

    void reset() { std::fill(mean_.begin(), mean_.end(), 0.0f); sigma_ = cfg_.init_sigma; }

    ContinuousAction sample(const State& /*s*/) {
        ContinuousAction a(cfg_.action_dim);
        std::normal_distribution<float> n(0.0f, sigma_);
        for (int i = 0; i < cfg_.action_dim; ++i) a[i] = mean_[i] + n(rng_);
        return a;
    }

    float logProb(const ContinuousAction& a) const {
        // isotropic Gaussian log prob (up to constant)
        float lp = 0.0f;
        float var = sigma_ * sigma_;
        for (int i = 0; i < cfg_.action_dim; ++i) {
            float d = a[i] - mean_[i];
            lp += -0.5f * (d * d) / (var + 1e-6f);
        }
        return lp;
    }

    void update(const std::vector<ContinuousExperience>& batch) {
        if (batch.empty()) return;
        // simple heuristic: move mean towards high-reward actions; reduce sigma slowly
        std::vector<float> grad(mean_.size(), 0.0f);
        float total_w = 0.0f;
        for (const auto& e : batch) {
            float w = std::max(0.0f, e.r + e.advantage); // positive weight if good
            for (size_t i = 0; i < mean_.size() && i < e.a.size(); ++i) {
                grad[i] += w * (e.a[i] - mean_[i]);
            }
            total_w += w;
        }
        if (total_w > 0.0f) {
            for (size_t i = 0; i < mean_.size(); ++i) {
                mean_[i] += cfg_.learning_rate * (grad[i] / (total_w + 1e-6f));
            }
        }
        sigma_ = std::max(cfg_.sigma_min, sigma_ * cfg_.sigma_decay);
    }

    const std::vector<float>& mean() const { return mean_; }
    float sigma() const { return sigma_; }
    const Config& getConfig() const { return cfg_; }
    void setConfig(const Config& c) { cfg_ = c; mean_.assign(c.action_dim, 0.0f); }
};

// Meta-RL controller that adapts exploration/learning parameters
class MetaRLAgent {
public:
    struct Config {
        int performance_window = 50;
        float epsilon_boost = 0.05f;
        float epsilon_cut = 0.98f; // multiplicative decay factor when stable
        float sigma_boost = 1.05f; // multiplicative when struggling
        float sigma_cut = 0.99f;   // multiplicative when stable
        float improvement_threshold = 0.01f;
    };

private:
    Config cfg_;
    std::deque<float> recent_rewards_;

public:
    explicit MetaRLAgent(const Config& cfg) : cfg_(cfg) {}

    void reset() { recent_rewards_.clear(); }

    void recordReward(float r) {
        recent_rewards_.push_back(r);
        if (recent_rewards_.size() > static_cast<size_t>(cfg_.performance_window)) {
            recent_rewards_.pop_front();
        }
    }

    template <typename Disc, typename Cont>
    void adapt(Disc& q_agent, Cont& ppo_agent) {
        if (recent_rewards_.size() < static_cast<size_t>(cfg_.performance_window)) return;
        // compute simple slope estimate
        float n = static_cast<float>(recent_rewards_.size());
        float mean_idx = (n - 1) / 2.0f;
        float cov = 0.0f, var = 0.0f;
        float mean_r = 0.0f;
        for (float r : recent_rewards_) mean_r += r; mean_r /= n;
        int idx = 0;
        for (float r : recent_rewards_) {
            cov += (idx - mean_idx) * (r - mean_r);
            var += (idx - mean_idx) * (idx - mean_idx);
            ++idx;
        }
        float slope = (var > 0.0f) ? (cov / var) : 0.0f;
        if (slope < cfg_.improvement_threshold) {
            // struggling: increase exploration
            auto qc = q_agent.getConfig(); qc.epsilon = std::min(1.0f, qc.epsilon + cfg_.epsilon_boost); q_agent.setConfig(qc);
            auto pc = ppo_agent.getConfig(); ppo_agent.setConfig(pc); // keep action_dim same
            // inflate sigma slightly
            // not directly settable without exposing, so emulate via reset to higher init_sigma
        } else {
            // improving: gently cut exploration
            auto qc = q_agent.getConfig(); qc.epsilon = std::max(qc.epsilon_min, qc.epsilon * cfg_.epsilon_cut); q_agent.setConfig(qc);
            // shrink sigma via update decay naturally
        }
    }
};

// Motor Cortex orchestrating the pipeline and agents
class MotorCortex {
public:
    struct Config {
        // Discrete (Q-learning)
        QLearningAgent::Config q_cfg;
        // Continuous (PPO-like)
        PPOAgent::Config ppo_cfg;
        // Meta-RL
        MetaRLAgent::Config meta_cfg;
        bool enable_meta = true;
        // Pipeline toggles
        bool enable_goal_alignment = true;
        bool enable_action_planning = true;
        bool enable_translation = true;
        bool enable_execution_feedback = true;
    };

    struct Statistics {
        uint64_t steps = 0;
        uint64_t discrete_updates = 0;
        uint64_t continuous_updates = 0;
        float last_reward = 0.0f;
        float average_reward = 0.0f;
    };

private:
    Config cfg_;
    QLearningAgent q_agent_;
    PPOAgent ppo_agent_;
    MetaRLAgent meta_;
    std::mt19937 rng_;
    Statistics stats_{};
    std::mutex mtx_;

public:
    explicit MotorCortex(const Config& cfg)
        : cfg_(cfg), q_agent_(cfg.q_cfg), ppo_agent_(cfg.ppo_cfg), meta_(cfg.meta_cfg), rng_(std::random_device{}()) {}

    void reset() {
        std::lock_guard<std::mutex> lock(mtx_);
        q_agent_.reset();
        ppo_agent_.reset();
        meta_.reset();
        stats_ = {};
    }

    // Pipeline (high-level): returns either discrete or continuous action depending on flags
    DiscreteAction selectDiscreteAction(const State& s) {
        std::lock_guard<std::mutex> lock(mtx_);
        // Goal Alignment & Planning stages would normally shape the state; placeholder pass-through
        return q_agent_.choose(s);
    }

    ContinuousAction selectContinuousAction(const State& s) {
        std::lock_guard<std::mutex> lock(mtx_);
        return ppo_agent_.sample(s);
    }

    // Learning updates (Execution & Feedback stage)
    void stepDiscrete(const DiscreteExperience& exp) {
        std::lock_guard<std::mutex> lock(mtx_);
        q_agent_.update(exp);
        updateStats(exp.r);
        if (cfg_.enable_meta) { meta_.recordReward(exp.r); meta_.adapt(q_agent_, ppo_agent_); }
    }

    void stepContinuous(const std::vector<ContinuousExperience>& batch) {
        std::lock_guard<std::mutex> lock(mtx_);
        // average reward used for stats
        float rsum = 0.0f; for (auto& e : batch) rsum += e.r;
        ppo_agent_.update(batch);
        updateStats(batch.empty() ? 0.0f : (rsum / batch.size()));
        if (cfg_.enable_meta) { for (auto& e : batch) meta_.recordReward(e.r); meta_.adapt(q_agent_, ppo_agent_); }
    }

    Statistics getStatistics() const { return stats_; }

    const QLearningAgent& qAgent() const { return q_agent_; }
    QLearningAgent& qAgent() { return q_agent_; }
    const PPOAgent& ppoAgent() const { return ppo_agent_; }
    PPOAgent& ppoAgent() { return ppo_agent_; }

private:
    void updateStats(float r) {
        stats_.steps++;
        stats_.last_reward = r;
        // running average
        const float beta = 0.01f;
        stats_.average_reward = (1.0f - beta) * stats_.average_reward + beta * r;
    }
};

} // namespace Core
} // namespace NeuroForge