#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>


namespace NeuroForge {
namespace Identity {

/**
 * @brief Phase 23: Preference vector with momentum averaging
 */
struct PreferenceVector {
  float weight = 0.0f;
  int observations = 0;

  /**
   * @brief Update with momentum (early experiences matter less)
   */
  void update(float delta) {
    weight = (weight * observations + delta) / (observations + 1);
    observations++;
  }
};

/**
 * @brief Phase 23: Preference Model
 *
 * A living summary of the agent's own behavioral tendencies.
 * This is NOT personality - it's policy bias learned from experience.
 */
class PreferenceModel {
public:
  // High-level preference tendencies
  std::unordered_map<std::string, PreferenceVector> preferences;

  // Global behavioral biases
  float risk_tolerance = 0.5f;    ///< 0 = risk-averse, 1 = risk-seeking
  float action_bias = 0.5f;       ///< 0 = prefer talk, 1 = prefer act
  float verification_bias = 0.5f; ///< 0 = prefer explore, 1 = prefer confirm
  float caution_level = 0.5f;     ///< 0 = impulsive, 1 = deliberate

  /**
   * @brief Update a named preference
   */
  void updatePreference(const std::string &key, float delta) {
    preferences[key].update(delta);
  }

  /**
   * @brief Get preference weight (0 if not observed)
   */
  float getPreference(const std::string &key) const {
    auto it = preferences.find(key);
    if (it != preferences.end()) {
      return it->second.weight;
    }
    return 0.0f;
  }

  /**
   * @brief Get observation count for a preference
   */
  int getObservationCount(const std::string &key) const {
    auto it = preferences.find(key);
    if (it != preferences.end()) {
      return it->second.observations;
    }
    return 0;
  }

  /**
   * @brief Calculate total observations across all preferences
   */
  int getTotalObservations() const {
    int total = 0;
    for (const auto &[key, pref] : preferences) {
      total += pref.observations;
    }
    return total;
  }

  /**
   * @brief Clamp all biases to [0, 1]
   */
  void normalize() {
    auto clamp = [](float &v) {
      if (v < 0.0f)
        v = 0.0f;
      if (v > 1.0f)
        v = 1.0f;
    };
    clamp(risk_tolerance);
    clamp(action_bias);
    clamp(verification_bias);
    clamp(caution_level);
  }
};

// Common preference keys
namespace PreferenceKeys {
constexpr const char *PREFER_VERIFICATION =
    "prefer_verification_over_discovery";
constexpr const char *AVOID_HIGH_COST = "avoid_high_cost_domains";
constexpr const char *FAVOR_LANGUAGE = "favor_language_before_action";
constexpr const char *DEFER_UNCERTAIN = "defer_when_uncertain";
constexpr const char *PREFER_REASONING = "prefer_reasoning_region";
constexpr const char *PREFER_PERCEPTION = "prefer_perception_region";
constexpr const char *PREFER_PROCEDURAL = "prefer_procedural_shortcuts";
} // namespace PreferenceKeys

} // namespace Identity
} // namespace NeuroForge
