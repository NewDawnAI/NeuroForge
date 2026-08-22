#pragma once

#include "actuation/ActionCommand.h"
#include "actuation/Observation.h"

namespace NeuroForge {
namespace Actuation {

/**
 * @brief Phase 21: Action + Observation pair for autobiographical memory
 *
 * Every action and its result get logged for:
 * - Deterministic replay
 * - Skill extraction
 * - Safety audits
 */
struct ReplayFrame {
  std::uint64_t frame_id = 0;
  std::uint64_t timestamp_ms = 0;

  ActionCommand action;
  Observation observation;

  bool success = false;
  std::string notes; ///< Human-readable summary

  // Epistemic outcome
  std::string fact_signature;  ///< What fact was being verified
  bool fact_confirmed = false; ///< Did this action confirm the fact?
  bool contradiction_found = false;
};

} // namespace Actuation
} // namespace NeuroForge
