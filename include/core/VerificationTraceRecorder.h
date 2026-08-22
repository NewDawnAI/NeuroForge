#pragma once

#include "core/VerificationTrace.h"
#include <unordered_map>

namespace NeuroForge {
namespace Core {

/**
 * @brief Phase 20a: Passive observer that records verification traces
 *
 * Read-only instrumentation - never influences verification decisions.
 */
class VerificationTraceRecorder {
public:
  /**
   * @brief Record a verification node
   */
  void recordNode(const VerificationTraceNode &node) {
    trace_[node.frame_id] = node;
  }

  /**
   * @brief Update the status of an existing node
   */
  void updateStatus(int frame_id, TraceStatus status) {
    auto it = trace_.find(frame_id);
    if (it != trace_.end()) {
      it->second.status = status;
    }
  }

  /**
   * @brief Link a child frame to its parent
   */
  void linkChild(int parent_id, int child_id) {
    auto it = trace_.find(parent_id);
    if (it != trace_.end()) {
      it->second.children.push_back(child_id);
    }
  }

  /**
   * @brief Get the full trace map
   */
  const std::unordered_map<int, VerificationTraceNode> &getTrace() const {
    return trace_;
  }

  /**
   * @brief Get count of recorded nodes
   */
  std::size_t size() const { return trace_.size(); }

  /**
   * @brief Check if trace is empty
   */
  bool empty() const { return trace_.empty(); }

  /**
   * @brief Clear all recorded traces
   */
  void clear() { trace_.clear(); }

private:
  std::unordered_map<int, VerificationTraceNode> trace_;
};

} // namespace Core
} // namespace NeuroForge
