#pragma once

#include "actuation/ActionCommand.h"
#include "actuation/Observation.h"
#include "actuation/ReplayFrame.h"
#include "core/EpistemicGates.h"
#include "core/SourceClassifier.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <vector>


namespace NeuroForge {
namespace Actuation {

/**
 * @brief Phase 21: GateDecision record for audit trail
 */
struct GateDecision {
  std::string gate_name;
  bool passed = false;
  std::string reason;
};

/**
 * @brief Phase 21: Action Broker
 *
 * Manages action dispatch with safety gate enforcement.
 * No action crosses into the world without passing gates.
 */
class ActionBroker {
public:
  using ActionExecutor = std::function<Observation(const ActionCommand &)>;

  ActionBroker() : next_action_id_(1), next_frame_id_(1) {}

  /**
   * @brief Register an executor for a specific action kind
   */
  void registerExecutor(ActionKind kind, ActionExecutor executor) {
    executors_[static_cast<int>(kind)] = std::move(executor);
  }

  /**
   * @brief Execute an action with full gate checking
   *
   * Returns empty observation if gates fail.
   */
  ReplayFrame executeAction(ActionCommand &action,
                            const Core::ReadinessReport &readiness) {
    action.id = next_action_id_++;
    action.timestamp_ms = getCurrentTimestamp();

    ReplayFrame frame;
    frame.frame_id = next_frame_id_++;
    frame.timestamp_ms = action.timestamp_ms;
    frame.action = action;

    // Gate check
    GateDecision gate;
    gate.gate_name = "EpistemicReadiness";
    gate.passed = readiness.is_ready;
    gate.reason = readiness.denial_reason;
    gate_decisions_.push_back(gate);

    if (!gate.passed) {
      std::cout << "  [ActionBroker] BLOCKED by gate: " << gate.reason << "\n";
      frame.success = false;
      frame.notes = "Gate blocked: " + gate.reason;
      replay_log_.push_back(frame);
      return frame;
    }

    // Safety level check for physical actions
    if (action.safety_level == SafetyLevel::PHYSICAL) {
      std::cout
          << "  [ActionBroker] BLOCKED: Physical actions not yet enabled\n";
      frame.success = false;
      frame.notes = "Physical safety level not enabled";
      replay_log_.push_back(frame);
      return frame;
    }

    // Execute action
    auto it = executors_.find(static_cast<int>(action.kind));
    if (it == executors_.end()) {
      std::cout << "  [ActionBroker] No executor for action kind: "
                << ActionCommand::kindToString(action.kind) << "\n";
      frame.success = false;
      frame.notes = "No executor registered";
      replay_log_.push_back(frame);
      return frame;
    }

    std::cout << "  [ActionBroker] Executing: "
              << ActionCommand::kindToString(action.kind)
              << " (frame=" << frame.frame_id << ")\n";

    frame.observation = it->second(action);
    frame.observation.id = next_action_id_++;
    frame.observation.timestamp_ms = getCurrentTimestamp();
    frame.success = !frame.observation.payload.empty();
    frame.notes =
        frame.success ? "Action completed" : "No observation returned";

    replay_log_.push_back(frame);

    std::cout << "  [ActionBroker] Frame " << frame.frame_id
              << " complete (success=" << frame.success << ")\n";

    return frame;
  }

  /**
   * @brief Get all recorded replay frames
   */
  const std::vector<ReplayFrame> &getReplayLog() const { return replay_log_; }

  /**
   * @brief Get all gate decisions
   */
  const std::vector<GateDecision> &getGateDecisions() const {
    return gate_decisions_;
  }

  /**
   * @brief Clear the replay log
   */
  void clearReplayLog() {
    replay_log_.clear();
    gate_decisions_.clear();
  }

private:
  static std::uint64_t getCurrentTimestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  std::unordered_map<int, ActionExecutor> executors_;
  std::vector<ReplayFrame> replay_log_;
  std::vector<GateDecision> gate_decisions_;
  std::uint64_t next_action_id_;
  std::uint64_t next_frame_id_;
};

} // namespace Actuation
} // namespace NeuroForge
