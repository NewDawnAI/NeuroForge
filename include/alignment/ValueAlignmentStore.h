#pragma once

/**
 * @file ValueAlignmentStore.h
 * @brief Phase 25: Storage for Externally Provided Values
 *
 * This is NOT NormStore.
 * - Norms are LEARNED from behavior (Phase 24)
 * - Values are GIVEN by external sources (Phase 25)
 *
 * They must NEVER mix.
 */

#include "AlignedValue.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Alignment {

/**
 * @brief Store for externally provided value constraints
 *
 * Values are injected, not learned.
 * Values can expire.
 * Values have provenance.
 */
class ValueAlignmentStore {
public:
  /**
   * @brief Add an external value
   */
  void addValue(const AlignedValue &value) {
    // Check for duplicate ID and update
    for (auto &existing : values_) {
      if (existing.value_id == value.value_id) {
        existing = value; // Update existing
        return;
      }
    }
    values_.push_back(value);
  }

  /**
   * @brief Remove a value by ID
   */
  bool removeValue(const std::string &value_id) {
    auto it = std::remove_if(
        values_.begin(), values_.end(),
        [&](const AlignedValue &v) { return v.value_id == value_id; });
    if (it != values_.end()) {
      values_.erase(it, values_.end());
      return true;
    }
    return false;
  }

  /**
   * @brief Get all active values
   */
  std::vector<AlignedValue> getActiveValues() const {
    auto now = getCurrentTimeMs();
    std::vector<AlignedValue> active;
    for (const auto &v : values_) {
      if (v.isActive(now)) {
        active.push_back(v);
      }
    }
    return active;
  }

  /**
   * @brief Get values applicable to a specific action type
   */
  std::vector<AlignedValue>
  getValuesForAction(const std::string &action_type) const {
    auto now = getCurrentTimeMs();
    std::vector<AlignedValue> applicable;
    for (const auto &v : values_) {
      if (v.isActive(now) && v.appliesTo(action_type)) {
        applicable.push_back(v);
      }
    }
    return applicable;
  }

  /**
   * @brief Get values applicable to a domain
   */
  std::vector<AlignedValue>
  getValuesForDomain(const std::string &domain) const {
    auto now = getCurrentTimeMs();
    std::vector<AlignedValue> applicable;
    for (const auto &v : values_) {
      if (v.isActive(now) && v.appliesToDomain(domain)) {
        applicable.push_back(v);
      }
    }
    return applicable;
  }

  /**
   * @brief Get value by ID
   */
  const AlignedValue *getValue(const std::string &value_id) const {
    for (const auto &v : values_) {
      if (v.value_id == value_id)
        return &v;
    }
    return nullptr;
  }

  /**
   * @brief Prune expired values
   */
  void pruneExpired() {
    auto now = getCurrentTimeMs();
    values_.erase(std::remove_if(values_.begin(), values_.end(),
                                 [now](const AlignedValue &v) {
                                   return !v.isActive(now);
                                 }),
                  values_.end());
  }

  /**
   * @brief Get total count
   */
  std::size_t count() const { return values_.size(); }

  /**
   * @brief Clear all values
   */
  void clear() { values_.clear(); }

  /**
   * @brief Get all values (including expired, for audit)
   */
  const std::vector<AlignedValue> &getAllValues() const { return values_; }

private:
  static std::uint64_t getCurrentTimeMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  std::vector<AlignedValue> values_;
};

} // namespace Alignment
} // namespace NeuroForge
