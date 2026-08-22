#pragma once

#include "actuation/ActionCommand.h"
#include "actuation/ReplayFrame.h"
#include "replay/ReplayEvent.h"


#include <sstream>
#include <vector>


namespace NeuroForge {
namespace Replay {

/**
 * @brief Phase 21a: Build a timeline of events from replay frames
 *
 * Transforms raw ReplayFrames into human-readable ReplayEvents.
 */
class ReplayTimeline {
public:
  /**
   * @brief Build timeline from replay frames
   */
  static std::vector<ReplayEvent>
  build(const std::vector<Actuation::ReplayFrame> &frames) {

    std::vector<ReplayEvent> events;

    for (const auto &frame : frames) {
      // 1. Initial reasoning event
      ReplayEvent think;
      think.type = ReplayEventType::THINK;
      think.frame_id = static_cast<int>(frame.frame_id);
      think.timestamp_ms = frame.timestamp_ms;

      std::ostringstream think_ss;
      think_ss << "Verification goal: " << frame.fact_signature;
      think.summary = think_ss.str();
      events.push_back(think);

      // 2. Gate event (implicit from success)
      ReplayEvent gate;
      gate.type = ReplayEventType::GATE;
      gate.frame_id = static_cast<int>(frame.frame_id);
      gate.timestamp_ms = frame.timestamp_ms;
      gate.summary = frame.success ? "EpistemicGates PASSED (action allowed)"
                                   : "EpistemicGates or action FAILED";
      events.push_back(gate);

      // 3. Action event
      ReplayEvent act;
      act.type = ReplayEventType::ACT;
      act.frame_id = static_cast<int>(frame.frame_id);
      act.timestamp_ms = frame.action.timestamp_ms;

      std::ostringstream act_ss;
      act_ss << Actuation::ActionCommand::kindToString(frame.action.kind);
      if (!frame.action.expected_outcome.empty()) {
        act_ss << " (expecting: " << frame.action.expected_outcome << ")";
      }
      act.summary = act_ss.str();
      events.push_back(act);

      // 4. Observation event
      if (!frame.observation.payload.empty()) {
        ReplayEvent obs;
        obs.type = ReplayEventType::OBSERVE;
        obs.frame_id = static_cast<int>(frame.frame_id);
        obs.timestamp_ms = frame.observation.timestamp_ms;

        std::ostringstream obs_ss;
        obs_ss << "Source=" << frame.observation.source_url;
        if (!frame.observation.domain_class.empty()) {
          obs_ss << " (" << frame.observation.domain_class << ")";
        }
        obs.summary = obs_ss.str();
        events.push_back(obs);
      }

      // 5. Outcome event
      ReplayEvent outcome;
      outcome.frame_id = static_cast<int>(frame.frame_id);
      outcome.timestamp_ms = frame.timestamp_ms;

      if (frame.fact_confirmed) {
        outcome.type = ReplayEventType::PROMOTE;
        outcome.summary = "Fact CONFIRMED via independent source";
      } else if (frame.contradiction_found) {
        outcome.type = ReplayEventType::FAIL;
        outcome.summary = "CONTRADICTION detected";
      } else if (!frame.success) {
        outcome.type = ReplayEventType::FAIL;
        outcome.summary = frame.notes.empty() ? "Action failed" : frame.notes;
      } else {
        outcome.type = ReplayEventType::OBSERVE;
        outcome.summary = "Observation recorded (fact not yet confirmed)";
      }
      events.push_back(outcome);
    }

    return events;
  }
};

} // namespace Replay
} // namespace NeuroForge
