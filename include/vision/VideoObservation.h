#pragma once

/**
 * @file VideoObservation.h
 * @brief Phase E3: YouTube passive vision observations
 *
 * Vision is an observation stream, not a control surface.
 *
 * @invariant Vision never creates goals
 * @invariant Vision never triggers actions
 * @invariant Vision never bypasses verification
 */

#include <cstdint>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Vision {

/**
 * @brief Detected object in video frame
 */
struct DetectedObject {
  std::string label; ///< Object class label
  float confidence = 0.0f;
  float x = 0.0f; ///< Bounding box
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
};

/**
 * @brief Text fragment from video (captions, OCR)
 */
struct TextFragment {
  std::string text;
  float confidence = 0.0f;
  std::uint64_t start_ms = 0; ///< Timestamp
  std::uint64_t end_ms = 0;
  std::string source; ///< "caption", "ocr", "title"
};

/**
 * @brief Video observation (single frame/segment)
 */
struct VideoObservation {
  std::string video_id; ///< YouTube video ID
  std::string video_title;
  std::uint64_t timestamp_ms = 0;

  std::vector<DetectedObject> objects;
  std::vector<TextFragment> captions;

  float frame_confidence = 0.0f;
  bool is_safe = true; ///< Passed safety filters
  std::string safety_warning;

  /**
   * @brief Create from caption text
   */
  static VideoObservation fromCaption(const std::string &video_id,
                                      const std::string &caption,
                                      std::uint64_t ts = 0) {
    VideoObservation obs;
    obs.video_id = video_id;
    obs.timestamp_ms = ts;

    TextFragment tf;
    tf.text = caption;
    tf.confidence = 1.0f;
    tf.source = "caption";
    tf.start_ms = ts;
    obs.captions.push_back(tf);

    return obs;
  }
};

} // namespace Vision
} // namespace NeuroForge
