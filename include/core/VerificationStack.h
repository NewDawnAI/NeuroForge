#pragma once

#include "core/VerificationFrame.h"
#include <iostream>
#include <stack>


namespace NeuroForge {
namespace Core {

/**
 * @brief Phase 20: Bounded Verification Stack
 *
 * Manages the recursive verification loop with hard depth limits.
 * Prevents open-ended recursion while allowing multi-step verification.
 */
class VerificationStack {
public:
  /// Configuration constants
  static constexpr int MAX_VERIFICATION_DEPTH = 2;
  static constexpr int MAX_BRANCHES_PER_FACT = 2;
  static constexpr int MAX_RETRIES_PER_FRAME = 2;

  explicit VerificationStack(int max_depth = MAX_VERIFICATION_DEPTH)
      : max_depth_(max_depth), next_frame_id_(0) {}

  /// Check if we can push a new frame at given depth
  bool canPush(int depth) const { return depth < max_depth_; }

  /// Push a new verification frame
  void push(const VerificationFrame &frame) {
    stack_.push(frame);
    std::cout << "  [Phase20] Pushed frame " << frame.frame_id
              << " depth=" << frame.depth << " (" << frame.subject << " "
              << frame.predicate << " " << frame.object << ")\n";
  }

  /// Check if stack is empty
  bool empty() const { return stack_.empty(); }

  /// Pop the next frame to process
  VerificationFrame pop() {
    auto f = stack_.top();
    stack_.pop();
    return f;
  }

  /// Get current stack depth (number of pending frames)
  std::size_t size() const { return stack_.size(); }

  /// Allocate a new unique frame ID
  int allocateFrameId() { return next_frame_id_++; }

  /// Create a child frame from a parent
  VerificationFrame createChildFrame(const VerificationFrame &parent,
                                     const std::string &subject,
                                     const std::string &predicate,
                                     const std::string &object) {

    VerificationFrame child;
    child.frame_id = allocateFrameId();
    child.parent_frame_id = parent.frame_id;
    child.subject = subject;
    child.predicate = predicate;
    child.object = object;
    child.depth = parent.depth + 1;
    return child;
  }

  /// Create a root frame (depth 0)
  VerificationFrame createRootFrame(const std::string &subject,
                                    const std::string &predicate,
                                    const std::string &object) {

    VerificationFrame root;
    root.frame_id = allocateFrameId();
    root.parent_frame_id = -1;
    root.subject = subject;
    root.predicate = predicate;
    root.object = object;
    root.depth = 0;
    return root;
  }

  int getMaxDepth() const { return max_depth_; }

private:
  int max_depth_;
  int next_frame_id_;
  std::stack<VerificationFrame> stack_;
};

} // namespace Core
} // namespace NeuroForge
