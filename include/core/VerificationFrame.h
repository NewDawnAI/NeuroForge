#pragma once

#include <string>
#include <vector>

namespace NeuroForge {
namespace Core {

/**
 * @brief Phase 20: Call stack of cognition
 *
 * Represents a single verification attempt in the recursive epistemic loop.
 * Tracks parent/child lineage for debuggable cognition.
 */
struct VerificationFrame {
  int frame_id = 0;
  int parent_frame_id = -1; ///< -1 = root frame

  std::string subject;
  std::string predicate;
  std::string object;

  int depth = 0;
  bool resolved = false;
  bool abandoned = false;

  std::vector<std::string> attempted_sources;

  /// Check if this frame can spawn children
  bool canSpawnChild(int max_depth) const {
    return !abandoned && depth < max_depth;
  }

  /// Mark as successfully resolved
  void markResolved() {
    resolved = true;
    abandoned = false;
  }

  /// Mark as abandoned (no valid source found)
  void markAbandoned() {
    abandoned = true;
    resolved = false;
  }

  /// Record an attempted source URL
  void recordAttempt(const std::string &url) {
    attempted_sources.push_back(url);
  }
};

} // namespace Core
} // namespace NeuroForge
