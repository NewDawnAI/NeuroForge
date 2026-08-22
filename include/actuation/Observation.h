#pragma once

#include <cstdint>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Actuation {

/**
 * @brief Phase 21: Sensory modality types
 */
enum class Modality {
  TEXT,          ///< Textual content (web, speech recognition)
  VISION,        ///< Visual input (future)
  AUDIO,         ///< Audio input (future)
  PROPRIOCEPTIVE ///< Body state (future embodiment)
};

/**
 * @brief Phase 21: Observation from the environment
 *
 * Every action returns an observation back into cognition.
 */
struct Observation {
  std::uint64_t id = 0;
  std::uint64_t timestamp_ms = 0;

  Modality modality = Modality::TEXT;
  std::string payload; ///< The actual content

  std::string source_url;   ///< Where this came from
  std::string domain_class; ///< Academic, Encyclopedia, etc.

  float confidence = 0.0f; ///< How reliable is this observation

  /// Convert Modality to string
  static std::string modalityToString(Modality m) {
    switch (m) {
    case Modality::TEXT:
      return "TEXT";
    case Modality::VISION:
      return "VISION";
    case Modality::AUDIO:
      return "AUDIO";
    case Modality::PROPRIOCEPTIVE:
      return "PROPRIOCEPTIVE";
    default:
      return "UNKNOWN";
    }
  }
};

} // namespace Actuation
} // namespace NeuroForge
