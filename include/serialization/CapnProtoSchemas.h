#pragma once

/**
 * @file CapnProtoSchemas.h
 * @brief Cap'n Proto Serialization Boundaries
 *
 * Defines what CAN and CANNOT be serialized.
 * Cap'n Proto = nervous system + memory spine, NOT brain tissue.
 *
 * ALLOWED: ReplayFrame, Observation, ActionCommand, AccountabilityEvent
 * FORBIDDEN: ConceptNode internals, Preference vectors, Arbitration heuristics
 */

#include <cstdint>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Serialization {

/**
 * @brief Serializable replay frame (for persistence/IPC)
 */
struct SerializableReplayFrame {
  std::uint64_t frame_id = 0;
  std::uint64_t timestamp_ms = 0;
  std::string frame_type; // "perception", "action", "decision", etc.

  // Observation data
  std::string observation_type;
  std::vector<std::uint8_t> observation_data;

  // Action data
  std::string action_kind;
  std::string action_parameters;
  std::string action_outcome;

  // Verification data
  bool was_verified = false;
  float verification_confidence = 0.0f;
  std::string verification_source;

  // Accountability
  std::string justification_id;
  std::string active_role;
  std::string active_contract;
};

/**
 * @brief Serializable observation
 */
struct SerializableObservation {
  std::uint64_t id = 0;
  std::uint64_t timestamp_ms = 0;
  std::string source;       // "web", "camera", "video", "user"
  std::string content_type; // "text", "image", "audio"
  std::vector<std::uint8_t> content_data;
  std::string metadata_json;
};

/**
 * @brief Serializable action command
 */
struct SerializableActionCommand {
  std::uint64_t id = 0;
  std::uint64_t timestamp_ms = 0;
  std::string kind; // "browse", "speak", "verify"
  std::string parameters_json;
  std::string safety_level;
  std::string expected_outcome;
  std::uint64_t originating_frame_id = 0;
};

/**
 * @brief Serializable accountability event
 */
struct SerializableAccountabilityEvent {
  std::uint64_t event_id = 0;
  std::uint64_t timestamp_ms = 0;
  std::string event_type; // "action_executed", "action_blocked", etc.
  std::string action_id;
  std::string justification_trace_id;
  std::string active_role;
  std::string active_contract;
  std::string outcome;
  bool permitted = false;
  std::string explanation;
};

/**
 * @brief Serialization helper for boundary objects
 */
class BoundarySerializer {
public:
  /**
   * @brief Serialize replay frame to JSON
   */
  static std::string toJson(const SerializableReplayFrame &frame) {
    std::string json = "{";
    json += "\"frame_id\":" + std::to_string(frame.frame_id) + ",";
    json += "\"timestamp_ms\":" + std::to_string(frame.timestamp_ms) + ",";
    json += "\"frame_type\":\"" + frame.frame_type + "\",";
    json += "\"action_kind\":\"" + frame.action_kind + "\",";
    json += "\"was_verified\":" +
            std::string(frame.was_verified ? "true" : "false") + ",";
    json += "\"verification_confidence\":" +
            std::to_string(frame.verification_confidence);
    json += "}";
    return json;
  }

  /**
   * @brief Serialize accountability event to JSON
   */
  static std::string toJson(const SerializableAccountabilityEvent &event) {
    std::string json = "{";
    json += "\"event_id\":" + std::to_string(event.event_id) + ",";
    json += "\"timestamp_ms\":" + std::to_string(event.timestamp_ms) + ",";
    json += "\"event_type\":\"" + event.event_type + "\",";
    json += "\"action_id\":\"" + event.action_id + "\",";
    json += "\"permitted\":" + std::string(event.permitted ? "true" : "false") +
            ",";
    json += "\"explanation\":\"" + event.explanation + "\"";
    json += "}";
    return json;
  }

  // NOTE: Full Cap'n Proto integration would use .capnp schema files
  // and the capnp compiler. This provides the boundary abstraction
  // that Cap'n Proto would serialize.
};

/**
 * @brief What is ALLOWED to cross serialization boundaries
 */
struct SerializationPolicy {
  // ALLOWED (nervous system + memory spine)
  static constexpr bool ALLOW_REPLAY_FRAME = true;
  static constexpr bool ALLOW_OBSERVATION = true;
  static constexpr bool ALLOW_ACTION_COMMAND = true;
  static constexpr bool ALLOW_ACCOUNTABILITY_EVENT = true;
  static constexpr bool ALLOW_CONTRACT_RECORD = true;
  static constexpr bool ALLOW_AUDIT_EXPORT = true;

  // FORBIDDEN (brain tissue)
  static constexpr bool ALLOW_CONCEPT_NODE_INTERNALS = false;
  static constexpr bool ALLOW_PREFERENCE_VECTORS = false;
  static constexpr bool ALLOW_ARBITRATION_HEURISTICS = false;
  static constexpr bool ALLOW_NORM_INDUCTION_INTERNALS = false;
};

} // namespace Serialization
} // namespace NeuroForge
