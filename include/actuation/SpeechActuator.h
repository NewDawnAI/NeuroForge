#pragma once

#include "actuation/ActionCommand.h"
#include "actuation/Observation.h"

#include <sstream>
#include <string>


namespace NeuroForge {
namespace Actuation {

/**
 * @brief Phase 21: Speech intent types
 *
 * Speech is an epistemic probe, not free-form language generation.
 */
enum class SpeechIntent {
  ASK_CLARIFICATION, ///< "Can you clarify X?"
  REQUEST_EVIDENCE,  ///< "Give me an example of X"
  CONFIRM_FACT,      ///< "Is it true that X?"
  NEGOTIATE_GOAL     ///< "Should we focus on X?"
};

/**
 * @brief Phase 21: Speech payload
 */
struct SpeechPayload {
  std::string utterance;
  SpeechIntent intent = SpeechIntent::REQUEST_EVIDENCE;
  std::string expected_signal;

  /// Convert intent to string
  static std::string intentToString(SpeechIntent i) {
    switch (i) {
    case SpeechIntent::ASK_CLARIFICATION:
      return "ASK_CLARIFICATION";
    case SpeechIntent::REQUEST_EVIDENCE:
      return "REQUEST_EVIDENCE";
    case SpeechIntent::CONFIRM_FACT:
      return "CONFIRM_FACT";
    case SpeechIntent::NEGOTIATE_GOAL:
      return "NEGOTIATE_GOAL";
    default:
      return "UNKNOWN";
    }
  }
};

/**
 * @brief Phase 21: Speech Actuator
 *
 * Speech is a verification action: you ask a question to get evidence.
 */
class SpeechActuator {
public:
  /**
   * @brief Create a speech action for verification
   */
  static ActionCommand createVerificationSpeech(const std::string &subject,
                                                const std::string &predicate,
                                                const std::string &object,
                                                int originating_frame_id = -1) {

    ActionCommand cmd;
    cmd.kind = ActionKind::SPEAK;
    cmd.safety_level = SafetyLevel::INTERACTIVE;
    cmd.originating_frame_id = originating_frame_id;

    // Generate verification question
    std::ostringstream question;
    question << "Can you provide evidence that ";
    if (!subject.empty())
      question << subject << " ";
    if (!predicate.empty())
      question << predicate << " ";
    if (!object.empty())
      question << object;
    question << "?";

    SpeechPayload payload;
    payload.utterance = question.str();
    payload.intent = SpeechIntent::REQUEST_EVIDENCE;
    payload.expected_signal = "evidence + explanation";

    cmd.parameters = serializePayload(payload);
    cmd.expected_outcome = "Observation with supporting evidence";

    return cmd;
  }

  /**
   * @brief Create a clarification request
   */
  static ActionCommand
  createClarificationRequest(const std::string &topic,
                             int originating_frame_id = -1) {

    ActionCommand cmd;
    cmd.kind = ActionKind::SPEAK;
    cmd.safety_level = SafetyLevel::INTERACTIVE;
    cmd.originating_frame_id = originating_frame_id;

    SpeechPayload payload;
    payload.utterance = "Can you clarify what is meant by " + topic + "?";
    payload.intent = SpeechIntent::ASK_CLARIFICATION;
    payload.expected_signal = "definition + context";

    cmd.parameters = serializePayload(payload);
    cmd.expected_outcome = "Clarifying explanation";

    return cmd;
  }

private:
  static std::string serializePayload(const SpeechPayload &p) {
    std::ostringstream ss;
    ss << "{\"utterance\":\"" << p.utterance << "\","
       << "\"intent\":\"" << SpeechPayload::intentToString(p.intent) << "\","
       << "\"expected_signal\":\"" << p.expected_signal << "\"}";
    return ss.str();
  }
};

} // namespace Actuation
} // namespace NeuroForge
