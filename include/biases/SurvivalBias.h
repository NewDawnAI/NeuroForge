#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <cmath>

namespace NeuroForge {
namespace Biases {

/**
 * SurvivalBias models hazard/risk-driven modulation that reduces assembly coherence
 * under perceived danger, increasing vigilance and avoidance drives.
 */
class SurvivalBias {
public:
    struct Config {
        float hazard_threshold = 0.7f;           // Activation spike threshold for hazard
        float hazard_coherence_weight = 0.2f;    // Weight to down-modulate coherence on risk
        float hazard_alpha = 0.0f;               // Sensitivity of modulation to external hazard in [0,1]
        float hazard_beta = 0.0f;                // Sensitivity of modulation to arousal in [0,1]
        float arousal_gain = 1.0f;               // Gain for arousal increase on risk
        float vigilance_decay_rate = 0.02f;      // Per-step decay of vigilance
        float min_coherence = 0.0f;              // Clamp lower bound
        float max_coherence = 1.0f;              // Clamp upper bound
        float variance_sensitivity = 1.0f;       // Scales variance contribution to risk
        float metabolic_hazard_sensitivity = 0.5f; // Scales metabolic stress contribution to risk
    };

    struct Metrics {
        float hazard_probability = 0.0f; // 0..1 probability of danger
        float risk_score = 0.0f;         // Composite risk from spikes and incoherence
        float arousal_level = 0.0f;      // Tracks vigilance/arousal state
        float avoidance_drive = 0.0f;    // Derived avoidance tendency
        float approach_drive = 0.0f;     // Derived approach tendency (inverse of avoidance)
        float coherence_modulation = 0.0f; // Suggested coherence delta (negative under risk)
        float metabolic_hazard = 0.0f;   // Current metabolic stress level
    };

    SurvivalBias() = default;
    explicit SurvivalBias(const Config& cfg) : config_(cfg) {}

    void updateConfig(const Config& cfg) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = cfg;
    }

    Config getConfig() const { return config_; }

    // Analyze an activation pattern and update internal metrics
    Metrics analyze(const std::vector<float>& activation_pattern) {
        std::lock_guard<std::mutex> lock(mutex_);

        Metrics m{};
        if (activation_pattern.empty()) {
            last_metrics_ = m;
            return m;
        }

        // Compute simple statistics
        float max_act = 0.0f;
        float sum = 0.0f;
        for (float a : activation_pattern) {
            if (a > max_act) max_act = a;
            sum += a;
        }
        float mean = sum / static_cast<float>(activation_pattern.size());
        float var = 0.0f;
        for (float a : activation_pattern) {
            float d = a - mean;
            var += d * d;
        }
        var /= static_cast<float>(activation_pattern.size());

        // Hazard probability from spike relative to threshold (internal measure)
        float hp_internal = sigmoid((max_act - config_.hazard_threshold) * 4.0f);
        // Fuse with any external hazard input (e.g., audio RMS or CLI constant)
        m.hazard_probability = clamp01(std::max(hp_internal, external_hazard_));

        // Metabolic hazard contribution
        m.metabolic_hazard = metabolic_hazard_;
        float metabolic_risk = config_.metabolic_hazard_sensitivity * m.metabolic_hazard;

        // Risk combines hazard prob, variance (incoherence proxy), and metabolic stress
        float incoherence = std::min(1.0f, config_.variance_sensitivity * var);
        m.risk_score = clamp01(0.5f * m.hazard_probability + 0.3f * incoherence + 0.2f * metabolic_risk);

        // Update arousal with decay
        arousal_ = std::max(0.0f, arousal_ - config_.vigilance_decay_rate);
        arousal_ += m.risk_score * config_.arousal_gain;
        arousal_ = clamp01(arousal_);
        m.arousal_level = arousal_;

        m.avoidance_drive = clamp01(m.risk_score * m.arousal_level);
        m.approach_drive = clamp01(1.0f - m.avoidance_drive);

        // Negative modulation proportional to risk
        m.coherence_modulation = -config_.hazard_coherence_weight * m.risk_score;

        last_metrics_ = m;
        last_pattern_ = activation_pattern;
        return m;
    }

    // Apply coherence modulation: returns new coherence clamped to configured range
    float applyCoherenceBias(float base_coherence,
                             const std::vector<float>& activation_pattern,
                             float override_weight = -1.0f) {
        Metrics m = analyze(activation_pattern);
        float base_weight = (override_weight >= 0.0f) ? override_weight : config_.hazard_coherence_weight;
        // Compute dynamic scaling factor from external hazard and arousal.
        // Default (alpha=beta=0) preserves current behavior (scale=1).
        float scale = 1.0f;
        if (config_.hazard_alpha != 0.0f || config_.hazard_beta != 0.0f) {
            scale = clamp01(config_.hazard_alpha * external_hazard_ + config_.hazard_beta * arousal_);
        }
        float applied_weight = base_weight * scale;
        last_applied_weight_ = applied_weight;
        float delta = -applied_weight * m.risk_score;
        float new_c = base_coherence + delta;
        return clamp(new_c, config_.min_coherence, config_.max_coherence);
    }

    Metrics getLastMetrics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_metrics_;
    }

    // Last applied modulation weight (after dynamic scaling and overrides)
    float getLastAppliedWeight() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_applied_weight_;
    }

    // Inject external hazard signal in [0,1] (e.g., audio RMS or explicit density)
    void setExternalHazard(float h) {
        std::lock_guard<std::mutex> lock(mutex_);
        external_hazard_ = clamp01(h);
    }

    // Inject metabolic hazard signal in [0,1] (from LearningSystem)
    void setMetabolicHazard(float h) {
        std::lock_guard<std::mutex> lock(mutex_);
        metabolic_hazard_ = clamp01(h);
    }

private:
    static float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    static float clamp(float v, float lo, float hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }
    static float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

private:
    Config config_{};
    mutable std::mutex mutex_;
    Metrics last_metrics_{};
    std::vector<float> last_pattern_{};
    float arousal_ = 0.0f;
    float external_hazard_ = 0.0f;
    float metabolic_hazard_ = 0.0f;
    float last_applied_weight_ = 0.0f;
};

} // namespace Biases
} // namespace NeuroForge