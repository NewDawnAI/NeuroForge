#include "serialization/neuroforge.capnp.h"
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <chrono>
#include <iostream>
#include <vector>


/**
 * @file test_capnproto.cpp
 * @brief Test Cap'n Proto serialization for boundary objects
 *
 * ALLOWED: ReplayFrame, Observation, ActionCommand, AccountabilityEvent
 * BLOCKED: ConceptNode internals, Preference vectors, Arbitration heuristics
 */

int main() {
  std::cout << "\n==========================================" << std::endl;
  std::cout << " NeuroForge Cap'n Proto Serialization Test" << std::endl;
  std::cout << "==========================================" << std::endl;

  int passed = 0;
  int failed = 0;

  // Test 1: ReplayFrame construction and serialization
  {
    ::capnp::MallocMessageBuilder message;
    auto frame = message.initRoot<ReplayFrame>();

    frame.setFrameId(1);
    frame.setTimestampMs(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    frame.setFrameType("perception");
    frame.setObservationType("web");
    frame.setActionKind("browse");
    frame.setWasVerified(true);
    frame.setVerificationConfidence(0.95f);
    frame.setActiveRole("learner");

    // Serialize to words
    auto words = capnp::messageToFlatArray(message);

    // Verify
    bool ok = frame.getFrameId() == 1 && frame.getWasVerified() == true &&
              frame.getVerificationConfidence() > 0.9f && words.size() > 0;

    std::cout << "  [" << (ok ? "PASS" : "FAIL")
              << "] ReplayFrame serialization (" << words.size() << " words)"
              << std::endl;
    ok ? passed++ : failed++;
  }

  // Test 2: AccountabilityEvent construction
  {
    ::capnp::MallocMessageBuilder message;
    auto event = message.initRoot<AccountabilityEvent>();

    event.setEventId(100);
    event.setTimestampMs(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count());
    event.setEventType("action_executed");
    event.setActionId("browse_001");
    event.setActiveRole("explorer");
    event.setActiveContract("developmental_session");
    event.setPermitted(true);
    event.setExplanation("Within safety bounds");

    auto words = capnp::messageToFlatArray(message);

    bool ok = event.getEventId() == 100 && event.getPermitted() == true &&
              words.size() > 0;

    std::cout << "  [" << (ok ? "PASS" : "FAIL")
              << "] AccountabilityEvent serialization (" << words.size()
              << " words)" << std::endl;
    ok ? passed++ : failed++;
  }

  // Test 3: SessionSummary construction
  {
    ::capnp::MallocMessageBuilder message;
    auto summary = message.initRoot<SessionSummary>();

    summary.setSessionId(1);
    summary.setDurationSeconds(1800);
    summary.setPagesVisited(30);
    summary.setCharactersRead(2297944);
    summary.setVocabularySize(1532);
    summary.setRelationsFormed(35);
    summary.setExplorationEntropy(4.5f);
    summary.setMode("curiosity_first");

    auto words = capnp::messageToFlatArray(message);

    bool ok = summary.getPagesVisited() == 30 && words.size() > 0;
    std::cout << "  [" << (ok ? "PASS" : "FAIL")
              << "] SessionSummary serialization (" << words.size() << " words)"
              << std::endl;
    ok ? passed++ : failed++;
  }

  // Test 4: ExpressionAuditRecord construction
  {
    ::capnp::MallocMessageBuilder message;
    auto record = message.initRoot<ExpressionAuditRecord>();

    record.setRecordId(50);
    record.setOutputText("This is exploration");
    record.setExpressionType("DESCRIBE");
    record.setStatus("PERMITTED");
    record.setOverallConfidence(0.85f);
    record.setGroundedTokens(3);
    record.setTotalTokens(4);

    auto words = capnp::messageToFlatArray(message);

    bool ok = record.getRecordId() == 50 &&
              record.getOverallConfidence() > 0.8f && words.size() > 0;
    std::cout << "  [" << (ok ? "PASS" : "FAIL")
              << "] ExpressionAuditRecord serialization (" << words.size()
              << " words)" << std::endl;
    ok ? passed++ : failed++;
  }

  // Test 5: Observation construction
  {
    ::capnp::MallocMessageBuilder message;
    auto obs = message.initRoot<Observation>();

    obs.setId(1);
    obs.setTimestampMs(std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count());
    obs.setSource("web");
    obs.setContentType("text");
    obs.setMetadataJson("{\"url\": \"https://wikipedia.org\"}");

    auto words = capnp::messageToFlatArray(message);

    bool ok = obs.getId() == 1 && words.size() > 0;
    std::cout << "  [" << (ok ? "PASS" : "FAIL")
              << "] Observation serialization (" << words.size() << " words)"
              << std::endl;
    ok ? passed++ : failed++;
  }

  // Test 6: ActionCommand construction
  {
    ::capnp::MallocMessageBuilder message;
    auto cmd = message.initRoot<ActionCommand>();

    cmd.setId(1);
    cmd.setKind("browse");
    cmd.setSafetyLevel("safe");
    cmd.setOriginatingFrameId(0);

    auto words = capnp::messageToFlatArray(message);

    bool ok = cmd.getId() == 1 && words.size() > 0;
    std::cout << "  [" << (ok ? "PASS" : "FAIL")
              << "] ActionCommand serialization (" << words.size() << " words)"
              << std::endl;
    ok ? passed++ : failed++;
  }

  std::cout << "\n==========================================" << std::endl;
  std::cout << "  RESULTS: " << passed << " passed, " << failed << " failed"
            << std::endl;
  std::cout << "==========================================" << std::endl;

  std::cout << "\nSerialization Policy:" << std::endl;
  std::cout << "  [ALLOWED] ReplayFrame, Observation, ActionCommand"
            << std::endl;
  std::cout << "  [ALLOWED] AccountabilityEvent, SessionSummary, "
               "ExpressionAuditRecord"
            << std::endl;
  std::cout << "  [BLOCKED] ConceptNode internals, Preference vectors"
            << std::endl;
  std::cout << "  [BLOCKED] Arbitration heuristics, Norm evaluation logic"
            << std::endl;

  std::cout << "\nCap'n Proto Integration: LIVE" << std::endl;

  return failed == 0 ? 0 : 1;
}
