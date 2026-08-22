#pragma once

/**
 * @file SpeechSynthesizer.h
 * @brief Phase E2: Speech Synthesis (Offline TTS)
 *
 * Converts utterance plans to speech output.
 * Currently supports text output; TTS integration is modular.
 *
 * @invariant Speech produces ExpressionReplayFrame for audit
 * @invariant No speech-to-thought loop (speech is output only)
 */

#include "../expression/LanguageExpressionCortex.h"
#include "../expression/UtterancePlan.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Speech {

/**
 * @brief Speech output mode
 */
enum class SpeechMode {
  TEXT_ONLY,   ///< Console/text output only
  TTS_OFFLINE, ///< Offline TTS (file output)
  TTS_REALTIME ///< Real-time TTS (future)
};

/**
 * @brief Speech output result
 */
struct SpeechResult {
  bool success = false;
  std::string text;
  std::string audio_file; ///< Path to audio file (if TTS)
  std::uint64_t duration_ms = 0;
  std::uint64_t timestamp_ms = 0;
};

/**
 * @brief Speech synthesis log entry
 */
struct SpeechLogEntry {
  std::uint64_t id = 0;
  std::uint64_t timestamp_ms = 0;
  std::string text;
  Expression::ExpressionType type = Expression::ExpressionType::DESCRIBE;
  std::vector<std::uint64_t> concept_ids;
  Expression::NormativeStatus status = Expression::NormativeStatus::PERMITTED;
  bool was_spoken = false;
};

/**
 * @brief Speech Synthesizer
 *
 * Integrates with LanguageExpressionCortex for speech output.
 */
class SpeechSynthesizer {
public:
  using TTSCallback = std::function<SpeechResult(const std::string &)>;

  SpeechSynthesizer() = default;

  /**
   * @brief Set speech mode
   */
  void setMode(SpeechMode mode) { mode_ = mode; }

  /**
   * @brief Set custom TTS callback
   */
  void setTTSCallback(TTSCallback callback) { tts_callback_ = callback; }

  /**
   * @brief Speak an utterance plan
   */
  SpeechResult speak(const Expression::UtterancePlan &plan) {
    SpeechResult result;
    result.timestamp_ms = getCurrentTimeMs();

    // Check if utterance can be expressed
    if (!plan.canExpress()) {
      result.success = false;
      logSpeech(plan, false);
      return result;
    }

    result.text = plan.render();

    switch (mode_) {
    case SpeechMode::TEXT_ONLY:
      result = outputText(result.text);
      break;

    case SpeechMode::TTS_OFFLINE:
      result = outputTTS(result.text);
      break;

    case SpeechMode::TTS_REALTIME:
      result = outputTTS(result.text);
      break;
    }

    logSpeech(plan, result.success);
    return result;
  }

  /**
   * @brief Speak a simple text (bypasses LEC)
   */
  SpeechResult speakText(const std::string &text) {
    SpeechResult result;
    result.timestamp_ms = getCurrentTimeMs();
    result.text = text;

    switch (mode_) {
    case SpeechMode::TEXT_ONLY:
      result = outputText(text);
      break;
    default:
      result = outputTTS(text);
      break;
    }

    return result;
  }

  /**
   * @brief Get speech log (for audit)
   */
  const std::vector<SpeechLogEntry> &getLog() const { return speech_log_; }

  /**
   * @brief Get speech count
   */
  std::size_t speechCount() const { return speech_log_.size(); }

private:
  SpeechResult outputText(const std::string &text) {
    SpeechResult result;
    result.text = text;
    result.timestamp_ms = getCurrentTimeMs();

    // Output to console
    std::cout << "[Speech] " << text << std::endl;

    result.success = true;
    return result;
  }

  SpeechResult outputTTS(const std::string &text) {
    SpeechResult result;
    result.text = text;
    result.timestamp_ms = getCurrentTimeMs();

    if (tts_callback_) {
      result = tts_callback_(text);
    } else {
      // Fallback: just output text and log
      std::cout << "[TTS] " << text << std::endl;
      result.success = true;
      result.audio_file = ""; // No audio generated
    }

    return result;
  }

  void logSpeech(const Expression::UtterancePlan &plan, bool was_spoken) {
    SpeechLogEntry entry;
    entry.id = next_id_++;
    entry.timestamp_ms = getCurrentTimeMs();
    entry.text = plan.render();
    entry.concept_ids = plan.concept_ids;
    entry.status = plan.status;
    entry.was_spoken = was_spoken;

    speech_log_.push_back(entry);
  }

  std::uint64_t getCurrentTimeMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  SpeechMode mode_ = SpeechMode::TEXT_ONLY;
  TTSCallback tts_callback_;
  std::vector<SpeechLogEntry> speech_log_;
  std::uint64_t next_id_ = 1;
};

} // namespace Speech
} // namespace NeuroForge
