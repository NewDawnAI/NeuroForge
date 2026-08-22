#pragma once

/**
 * @file CameraObservation.h
 * @brief Phase E4: Camera (real-world) observations
 *
 * Strict safety sandbox for real-world vision.
 *
 * @invariant No identity inference (faces → anonymized)
 * @invariant No tracking (no persistent person IDs)
 * @invariant No action (vision ≠ motor)
 * @invariant Always auditable (ReplayFrame mandatory)
 */

#include "VideoObservation.h" // For TextFragment

#include <cstdint>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Vision {

/**
 * @brief Privacy level for camera observation
 */
enum class PrivacyLevel {
  SAFE,       ///< No personal data detected
  ANONYMIZED, ///< Personal data detected and anonymized
  RESTRICTED, ///< Contains sensitive context (medical, etc.)
  BLOCKED     ///< Cannot be processed (children, explicit)
};

/**
 * @brief Object label from camera
 */
struct ObjectLabel {
  std::string category;    ///< e.g., "person", "car", "book"
  std::string subcategory; ///< e.g., "sedan", "textbook"
  float confidence = 0.0f;
  bool is_anonymized = false; ///< True if original was a face/person
};

/**
 * @brief Camera observation (single frame)
 */
struct CameraObservation {
  std::uint64_t frame_id = 0;
  std::uint64_t timestamp_ms = 0;

  std::vector<ObjectLabel> objects;
  std::vector<TextFragment> text; ///< OCR from scene

  float overall_confidence = 0.0f;
  PrivacyLevel privacy_level = PrivacyLevel::SAFE;
  std::string privacy_action; ///< What was done for privacy

  bool is_provisional = true; ///< Not committed to long-term memory

  /**
   * @brief Check if observation can be processed
   */
  bool canProcess() const {
    return privacy_level == PrivacyLevel::SAFE ||
           privacy_level == PrivacyLevel::ANONYMIZED;
  }

  /**
   * @brief Check if observation can be stored
   */
  bool canStore() const {
    return canProcess() && privacy_level != PrivacyLevel::RESTRICTED;
  }
};

} // namespace Vision
} // namespace NeuroForge
