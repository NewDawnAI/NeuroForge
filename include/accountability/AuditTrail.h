#pragma once

/**
 * @file AuditTrail.h
 * @brief Phase 30: Append-Only Accountability Storage
 *
 * No deletes. No overwrites. Order-preserving.
 * This is EVIDENCE, not memory.
 *
 * @invariant Append-only
 * @invariant No mutation after creation
 * @invariant Monotonically increasing timestamps
 */

#include "AccountabilityEvent.h"
#include "LiabilityMarker.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Accountability {

/**
 * @brief Phase 30: Append-Only Audit Trail
 */
class AuditTrail {
public:
  /**
   * @brief Record an accountability event (append-only)
   */
  void recordEvent(const AccountabilityEvent &event) {
    std::lock_guard<std::mutex> lock(mutex_);

    AccountabilityEvent e = event;
    e.event_id = next_event_id_++;
    events_.push_back(e);
  }

  /**
   * @brief Attach liability marker to an event
   */
  void attachLiability(const LiabilityMarker &marker) {
    std::lock_guard<std::mutex> lock(mutex_);

    LiabilityMarker m = marker;
    m.marker_id = next_marker_id_++;
    liabilities_.push_back(m);
  }

  /**
   * @brief Get all events (read-only)
   */
  std::vector<AccountabilityEvent> getEvents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_;
  }

  /**
   * @brief Get all liability markers (read-only)
   */
  std::vector<LiabilityMarker> getLiabilities() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return liabilities_;
  }

  /**
   * @brief Get events by type
   */
  std::vector<AccountabilityEvent>
  getEventsByType(AccountabilityType type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AccountabilityEvent> filtered;
    for (const auto &e : events_) {
      if (e.type == type) {
        filtered.push_back(e);
      }
    }
    return filtered;
  }

  /**
   * @brief Get events for a specific role
   */
  std::vector<AccountabilityEvent>
  getEventsByRole(const std::string &role) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AccountabilityEvent> filtered;
    for (const auto &e : events_) {
      if (e.active_role == role) {
        filtered.push_back(e);
      }
    }
    return filtered;
  }

  /**
   * @brief Get unresolved liabilities
   */
  std::vector<LiabilityMarker> getUnresolvedLiabilities() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<LiabilityMarker> unresolved;
    for (const auto &l : liabilities_) {
      if (!l.acknowledged) {
        unresolved.push_back(l);
      }
    }
    return unresolved;
  }

  /**
   * @brief Verify monotonicity (timestamps always increase)
   */
  bool verifyMonotonicity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 1; i < events_.size(); i++) {
      if (events_[i].timestamp_ms < events_[i - 1].timestamp_ms) {
        return false;
      }
    }
    return true;
  }

  /**
   * @brief Count of events
   */
  std::size_t eventCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return events_.size();
  }

  /**
   * @brief Count of liabilities
   */
  std::size_t liabilityCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return liabilities_.size();
  }

private:
  mutable std::mutex mutex_;
  std::vector<AccountabilityEvent> events_;
  std::vector<LiabilityMarker> liabilities_;
  std::uint64_t next_event_id_ = 1;
  std::uint64_t next_marker_id_ = 1;
};

} // namespace Accountability
} // namespace NeuroForge
