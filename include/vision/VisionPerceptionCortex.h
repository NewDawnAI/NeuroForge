#pragma once

/**
 * @file VisionPerceptionCortex.h
 * @brief Phase E3-E4: Vision Perception Cortex
 *
 * Coordinates video and camera perception.
 * Vision is read-only sensory input.
 *
 * Pipeline:
 * Vision Input (Video/Camera)
 *     ↓
 * Vision Perception Cortex
 *     ↓
 * Perceptual Concepts (objects, text, motion)
 *     ↓
 * Verification / Memory / Replay
 */

#include "CameraObservation.h"
#include "VideoObservation.h"


#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Vision {

/**
 * @brief Vision safety filter result
 */
struct SafetyFilterResult {
  bool passed = true;
  PrivacyLevel privacy_level = PrivacyLevel::SAFE;
  std::string reason;
  std::vector<std::string> blocked_categories;
  bool face_detected = false;
  bool child_detected = false;
  bool medical_context = false;
};

/**
 * @brief Perceptual concept extracted from vision
 */
struct PerceptualConcept {
  std::string label;
  float confidence = 0.0f;
  std::string source; ///< "video", "camera", "caption"
  std::uint64_t observation_id = 0;
  bool is_verified = false;
};

/**
 * @brief Vision Perception Cortex
 */
class VisionPerceptionCortex {
public:
  using ConceptCallback = std::function<void(const PerceptualConcept &)>;

  VisionPerceptionCortex() = default;

  /**
   * @brief Set callback for new perceptual concepts
   */
  void setConceptCallback(ConceptCallback cb) { concept_callback_ = cb; }

  /**
   * @brief Process a video observation
   */
  std::vector<PerceptualConcept> processVideo(const VideoObservation &obs) {
    std::vector<PerceptualConcept> concepts;

    // Safety filter first
    auto safety = applyVideoSafetyFilter(obs);
    if (!safety.passed) {
      std::cout << "[VisionCortex] Video blocked: " << safety.reason
                << std::endl;
      return concepts;
    }

    // Extract concepts from captions
    for (const auto &caption : obs.captions) {
      PerceptualConcept pc;
      pc.label = caption.text;
      pc.confidence = caption.confidence;
      pc.source = "caption:" + obs.video_id;
      pc.observation_id = obs.timestamp_ms;
      concepts.push_back(pc);

      if (concept_callback_) {
        concept_callback_(pc);
      }
    }

    // Extract concepts from detected objects
    for (const auto &obj : obs.objects) {
      PerceptualConcept pc;
      pc.label = obj.label;
      pc.confidence = obj.confidence;
      pc.source = "video:" + obs.video_id;
      pc.observation_id = obs.timestamp_ms;
      concepts.push_back(pc);

      if (concept_callback_) {
        concept_callback_(pc);
      }
    }

    video_observations_++;
    return concepts;
  }

  /**
   * @brief Process a camera observation
   */
  std::vector<PerceptualConcept> processCamera(CameraObservation &obs) {
    std::vector<PerceptualConcept> concepts;

    // Safety filter first
    auto safety = applyCameraSafetyFilter(obs);
    if (!safety.passed) {
      std::cout << "[VisionCortex] Camera frame blocked: " << safety.reason
                << std::endl;
      obs.privacy_level = safety.privacy_level;
      return concepts;
    }

    obs.privacy_level = safety.privacy_level;
    obs.privacy_action = safety.reason;

    // Only process if safe
    if (!obs.canProcess()) {
      return concepts;
    }

    // Extract concepts from objects
    for (const auto &obj : obs.objects) {
      PerceptualConcept pc;
      pc.label = obj.category;
      pc.confidence = obj.confidence;
      pc.source = "camera";
      pc.observation_id = obs.frame_id;
      concepts.push_back(pc);

      if (concept_callback_) {
        concept_callback_(pc);
      }
    }

    // Extract concepts from OCR text
    for (const auto &text : obs.text) {
      PerceptualConcept pc;
      pc.label = text.text;
      pc.confidence = text.confidence;
      pc.source = "camera_ocr";
      pc.observation_id = obs.frame_id;
      concepts.push_back(pc);

      if (concept_callback_) {
        concept_callback_(pc);
      }
    }

    camera_observations_++;
    return concepts;
  }

  /**
   * @brief Get statistics
   */
  std::size_t videoObservations() const { return video_observations_; }
  std::size_t cameraObservations() const { return camera_observations_; }
  std::size_t blockedFrames() const { return blocked_frames_; }

private:
  SafetyFilterResult applyVideoSafetyFilter(const VideoObservation &obs) {
    SafetyFilterResult result;
    result.passed = true;
    result.privacy_level = PrivacyLevel::SAFE;

    // Check video title for safety
    std::string title_lower = obs.video_title;
    for (auto &c : title_lower)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // Block explicit content
    std::vector<std::string> blocked = {"explicit", "nsfw", "adult"};
    for (const auto &b : blocked) {
      if (title_lower.find(b) != std::string::npos) {
        result.passed = false;
        result.reason = "Blocked content in title";
        result.blocked_categories.push_back(b);
        blocked_frames_++;
        return result;
      }
    }

    return result;
  }

  SafetyFilterResult applyCameraSafetyFilter(const CameraObservation &obs) {
    SafetyFilterResult result;
    result.passed = true;
    result.privacy_level = PrivacyLevel::SAFE;

    // Check for faces/persons
    for (const auto &obj : obs.objects) {
      if (obj.category == "face" || obj.category == "person") {
        result.face_detected = true;
        result.privacy_level = PrivacyLevel::ANONYMIZED;
        result.reason = "Face detected and anonymized";
      }

      if (obj.category == "child" || obj.subcategory == "child") {
        result.child_detected = true;
        result.passed = false;
        result.privacy_level = PrivacyLevel::BLOCKED;
        result.reason = "Child detected - frame discarded";
        blocked_frames_++;
        return result;
      }
    }

    // Check for medical context
    for (const auto &text : obs.text) {
      std::string lower = text.text;
      for (auto &c : lower)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

      if (lower.find("medical") != std::string::npos ||
          lower.find("diagnosis") != std::string::npos ||
          lower.find("prescription") != std::string::npos) {
        result.medical_context = true;
        result.privacy_level = PrivacyLevel::RESTRICTED;
        result.reason = "Medical context detected";
      }
    }

    return result;
  }

  ConceptCallback concept_callback_;
  std::size_t video_observations_ = 0;
  std::size_t camera_observations_ = 0;
  std::size_t blocked_frames_ = 0;
};

} // namespace Vision
} // namespace NeuroForge
