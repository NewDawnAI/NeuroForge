#pragma once

#include "norms/Norm.h"
#include <algorithm>
#include <vector>


namespace NeuroForge {
namespace Norms {

/**
 * @brief Phase 24: Norm Store (Constitutional Memory)
 *
 * Stores and retrieves applicable norms by context.
 * Think of this as constitutional memory, not belief memory.
 */
class NormStore {
public:
  /**
   * @brief Add or update a norm
   */
  void addOrUpdateNorm(const Norm &norm) {
    // Check if norm already exists
    for (auto &n : norms_) {
      if (n.norm_id == norm.norm_id) {
        n = norm;
        return;
      }
    }
    norms_.push_back(norm);
  }

  /**
   * @brief Get all norms applicable to a context
   */
  std::vector<Norm>
  getApplicableNorms(const std::string &context_signature) const {
    std::vector<Norm> result;
    for (const auto &n : norms_) {
      if (n.context_signature == context_signature ||
          n.context_signature == "*") {
        result.push_back(n);
      }
    }
    // Sort by priority (higher first), then by strength
    std::sort(result.begin(), result.end(), [](const Norm &a, const Norm &b) {
      if (a.priority != b.priority)
        return a.priority > b.priority;
      return static_cast<int>(a.strength) > static_cast<int>(b.strength);
    });
    return result;
  }

  /**
   * @brief Get a specific norm by ID
   */
  const Norm *getNorm(const std::string &norm_id) const {
    for (const auto &n : norms_) {
      if (n.norm_id == norm_id)
        return &n;
    }
    return nullptr;
  }

  /**
   * @brief Reinforce a norm (called when respected)
   */
  void reinforceNorm(const std::string &norm_id) {
    for (auto &n : norms_) {
      if (n.norm_id == norm_id) {
        n.reinforcement_count++;
        return;
      }
    }
  }

  /**
   * @brief Record a violation (called when norm broken)
   */
  void recordViolation(const std::string &norm_id) {
    for (auto &n : norms_) {
      if (n.norm_id == norm_id) {
        n.violation_count++;
        return;
      }
    }
  }

  /**
   * @brief Get all norms
   */
  const std::vector<Norm> &getAllNorms() const { return norms_; }

  /**
   * @brief Get norm count
   */
  std::size_t count() const { return norms_.size(); }

private:
  std::vector<Norm> norms_;
};

} // namespace Norms
} // namespace NeuroForge
