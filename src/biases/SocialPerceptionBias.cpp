#include "SocialPerceptionBias.h"
#include "core/HypergraphBrain.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <iostream>
#include <numeric>

#ifdef NF_HAVE_OPENCV

namespace NeuroForge {
namespace Biases {

SocialPerceptionBias::SocialPerceptionBias(const Config& config) 
    : config_(config) {
    recent_events_.clear();
    lip_motion_history_.reserve(LIP_MOTION_HISTORY_SIZE);
    tracked_faces_.clear();
}

bool SocialPerceptionBias::initialize() {
    bool success = true;
    
    if (config_.enable_face_detection) {
        if (!face_cascade_.load(config_.face_cascade_path)) {
            // Try alternative paths
            std::vector<std::string> fallback_paths = {
                "c:/Users/ashis/Desktop/NeuroForge/external/vcpkg/buildtrees/opencv4/src/4.11.0-0357908e41.clean/data/haarcascades/" + config_.face_cascade_path,
                "data/haarcascades/" + config_.face_cascade_path,
                "/usr/share/opencv/haarcascades/" + config_.face_cascade_path,
                "/usr/local/share/opencv/haarcascades/" + config_.face_cascade_path
            };
            
            bool loaded = false;
            for (const auto& path : fallback_paths) {
                if (face_cascade_.load(path)) {
                    loaded = true;
                    break;
                }
            }
            
            if (!loaded) {
                std::cerr << "Warning: Could not load face cascade. Face detection will be disabled." << std::endl;
                config_.enable_face_detection = false;
                success = false;
            }
        }
    }
    
    if (config_.enable_gaze_tracking) {
        if (!eye_cascade_.load(config_.eye_cascade_path)) {
            std::vector<std::string> fallback_paths = {
                "c:/Users/ashis/Desktop/NeuroForge/external/vcpkg/buildtrees/opencv4/src/4.11.0-0357908e41.clean/data/haarcascades/" + config_.eye_cascade_path,
                "data/haarcascades/" + config_.eye_cascade_path,
                "/usr/share/opencv/haarcascades/" + config_.eye_cascade_path,
                "/usr/local/share/opencv/haarcascades/" + config_.eye_cascade_path
            };
            
            bool loaded = false;
            for (const auto& path : fallback_paths) {
                if (eye_cascade_.load(path)) {
                    loaded = true;
                    break;
                }
            }
            
            if (!loaded) {
                std::cerr << "Warning: Could not load eye cascade. Gaze tracking will be disabled." << std::endl;
                config_.enable_gaze_tracking = false;
            }
        }
    }
    
    if (config_.enable_lip_sync) {
        if (!mouth_cascade_.load(config_.mouth_cascade_path)) {
            std::vector<std::string> fallback_paths = {
                "c:/Users/ashis/Desktop/NeuroForge/external/vcpkg/buildtrees/opencv4/src/4.11.0-0357908e41.clean/data/haarcascades/" + config_.mouth_cascade_path,
                "data/haarcascades/" + config_.mouth_cascade_path,
                "/usr/share/opencv/haarcascades/" + config_.mouth_cascade_path,
                "/usr/local/share/opencv/haarcascades/" + config_.mouth_cascade_path
            };
            
            bool loaded = false;
            for (const auto& path : fallback_paths) {
                if (mouth_cascade_.load(path)) {
                    loaded = true;
                    break;
                }
            }
            
            if (!loaded) {
                std::cerr << "Warning: Could not load mouth cascade. Lip-sync detection will be disabled." << std::endl;
                config_.enable_lip_sync = false;
            }
        }
    }
    
    return success;
}

std::vector<SocialPerceptionBias::SocialEvent> SocialPerceptionBias::processSocialFrame(
    const cv::Mat& frame, const AudioBuffer& audio) {
    
    std::vector<SocialEvent> events;
    
    if (frame.empty()) {
        return events;
    }
    
    // Update statistics
    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_frames_processed++;
        stats_.last_update_time = getCurrentTimeMs();
    }
    
    // Step 1: Enhanced face detection with masks and contours
    std::vector<cv::Rect> faces;
    std::vector<cv::Mat> face_masks;
    std::vector<std::vector<cv::Point>> face_contours;
    
    if (config_.enable_face_detection && detectFacesWithMasks(frame, faces, face_masks, face_contours)) {
        updateFaceTracking(faces);
        
        // Process each detected face with enhanced features
        for (size_t i = 0; i < faces.size(); ++i) {
            const auto& face = faces[i];
            SocialEvent event;
            
            // Legacy face box (for compatibility)
            event.face_box = face;
            
            // NEW: Enhanced face features
            if (i < face_masks.size()) event.face_mask = face_masks[i];
            if (i < face_contours.size()) event.face_contour = face_contours[i];
            
            event.timestamp_ms = getCurrentTimeMs();
            event.tracking_id = getTrackingId(face);
            event.total_salience_boost = config_.face_priority_multiplier;
            
            // Extract face region
            cv::Mat face_roi = frame(face);
            
            // Step 2: Enhanced eye detection with pupil tracking and gaze vectors
            if (config_.enable_gaze_tracking) {
                std::vector<cv::Rect> eyes;
                std::vector<cv::Point2f> pupil_positions;
                std::vector<std::vector<cv::Point>> eye_contours;
                
                if (detectEyesWithPupils(face_roi, eyes, pupil_positions, eye_contours)) {
                    // Store pupil positions and eye contours
                    for (size_t j = 0; j < std::min(pupil_positions.size(), size_t(2)); ++j) {
                        event.pupil_positions[j] = pupil_positions[j];
                        // Convert to global coordinates
                        event.pupil_positions[j].x += face.x;
                        event.pupil_positions[j].y += face.y;
                    }
                    
                    for (size_t j = 0; j < std::min(eye_contours.size(), size_t(2)); ++j) {
                        event.eye_contours[j] = eye_contours[j];
                        // Convert contour points to global coordinates
                        for (auto& point : event.eye_contours[j]) {
                            point.x += face.x;
                            point.y += face.y;
                        }
                    }
                    
                    // Compute vectorized gaze direction
                    event.gaze_confidence = computeGazeVector(face, pupil_positions, frame.size(),
                                                            event.gaze_vector, event.gaze_angle, 
                                                            event.gaze_target_box);
                    
                    // Calculate dynamic attention strength based on gaze confidence
                    event.attention_strength = 1.0f + (event.gaze_confidence * config_.gaze_attention_multiplier);
                    
                    if (event.gaze_confidence > 0.5f) {
                        event.total_salience_boost += config_.gaze_attention_multiplier;
                        
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        stats_.gaze_events_detected++;
                    }
                }
            }
            
            // Step 3: Enhanced mouth detection with precise masking and lip-sync
            if (config_.enable_lip_sync && !audio.audio_envelope.empty()) {
                cv::Rect mouth;
                cv::Mat mouth_mask;
                
                if (detectMouthWithMask(face_roi, mouth, mouth_mask)) {
                    event.mouth_region = mouth;
                    event.mouth_region.x += face.x;  // Convert to global coordinates
                    event.mouth_region.y += face.y;
                    event.mouth_mask = mouth_mask;
                    
                    // Enhanced lip-sync detection using precise mouth mask
                    event.lip_sync_confidence = detectLipSyncWithMask(mouth_mask, audio);
                    
                    if (event.lip_sync_confidence > config_.lip_sync_threshold) {
                        event.is_speaking = true;
                        event.total_salience_boost += config_.lip_sync_boost;
                        
                        std::lock_guard<std::mutex> lock(stats_mutex_);
                        stats_.lip_sync_events_detected++;
                    }
                }
            }
            
            events.push_back(event);
        }
    }
    
    // Store events in history
    {
        std::lock_guard<std::mutex> lock(events_mutex_);
        for (const auto& event : events) {
            recent_events_.push_back(event);
        }
        
        // Maintain history size limit
        while (recent_events_.size() > config_.event_history_size) {
            recent_events_.pop_front();
        }
    }
    
    // Update statistics
    updateStatistics(events);
    
    return events;
}

// Overload without audio: forwards to main implementation with a default-constructed buffer
std::vector<SocialPerceptionBias::SocialEvent> SocialPerceptionBias::processSocialFrame(
    const cv::Mat& frame) {
    return processSocialFrame(frame, AudioBuffer{});
}

void SocialPerceptionBias::applySocialBias(std::vector<float>& features, 
                                          const std::vector<SocialEvent>& events,
                                          int grid_size) {
    if (features.empty() || events.empty() || grid_size <= 0) {
        return;
    }
    
    const int total_features = grid_size * grid_size;
    if (static_cast<int>(features.size()) != total_features) {
        return;
    }
    
    float fatigue_scale = 1.0f;
    if (brain_ != nullptr) {
        auto stats_opt = brain_->getLearningStatistics();
        if (stats_opt.has_value()) {
            float h = stats_opt->metabolic_hazard;
            float scale = 1.0f - 0.5f * std::max(0.0f, std::min(1.0f, h));
            fatigue_scale = std::max(0.5f, scale);
        }
    }

    // Enhanced substrate integration: encode masks and vectors into grid
    encodeMasksToGrid(features, events, grid_size);
    
    // Apply attention boost for each social event
    for (const auto& event : events) {
        // Face region boost
        if (!event.face_box.empty()) {
            applyRegionBoost(features, event.face_box, event.total_salience_boost * fatigue_scale, grid_size);
        }
        
        // Gaze target boost
        if (!event.gaze_target_box.empty() && event.gaze_confidence > 0.3f) {
            float gaze_boost = config_.gaze_attention_multiplier * event.gaze_confidence * fatigue_scale;
            applyRegionBoost(features, event.gaze_target_box, gaze_boost, grid_size);
        }
        
        // Mouth region boost for speaking
        if (event.is_speaking && !event.mouth_region.empty()) {
            float lip_boost = config_.lip_sync_boost * event.lip_sync_confidence * fatigue_scale;
            applyRegionBoost(features, event.mouth_region, lip_boost, grid_size);
        }
    }
    
    // Feed social feature vector to brain substrate if available
    if (brain_ != nullptr && !features.empty()) {
        brain_->feedExternalPattern(NeuroForge::Modality::Social, features);
    }
}

void SocialPerceptionBias::applyRegionBoost(std::vector<float>& features,
                                           const cv::Rect& region,
                                           float boost_factor,
                                           int grid_size) {
    // Convert region to grid coordinates (assuming features represent a grid)
    // This is a simplified mapping - in practice, you'd need the actual frame dimensions
    const int region_start_x = std::max(0, region.x * grid_size / 640);  // Assuming 640px width
    const int region_end_x = std::min(grid_size, (region.x + region.width) * grid_size / 640);
    const int region_start_y = std::max(0, region.y * grid_size / 480);  // Assuming 480px height
    const int region_end_y = std::min(grid_size, (region.y + region.height) * grid_size / 480);
    
    for (int y = region_start_y; y < region_end_y; ++y) {
        for (int x = region_start_x; x < region_end_x; ++x) {
            int idx = y * grid_size + x;
            if (idx >= 0 && idx < static_cast<int>(features.size())) {
                features[idx] *= boost_factor;
            }
        }
    }
}

bool SocialPerceptionBias::detectFaces(const cv::Mat& frame, std::vector<cv::Rect>& faces) {
    if (!config_.enable_face_detection || frame.empty()) {
        return false;
    }
    
    try {
        cv::Mat gray;
        if (frame.channels() == 3) {
            cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = frame.clone();
        }
        
        cv::equalizeHist(gray, gray);
        
        face_cascade_.detectMultiScale(gray, faces, 1.1, 3, 
                                      cv::CASCADE_SCALE_IMAGE, 
                                      cv::Size(30, 30));
        
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.faces_detected += static_cast<std::uint32_t>(faces.size());
        }
        
        return !faces.empty();
        
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error in face detection: " << e.what() << std::endl;
        return false;
    }
}

bool SocialPerceptionBias::detectEyes(const cv::Mat& face_roi, std::vector<cv::Rect>& eyes) {
    if (!config_.enable_gaze_tracking || face_roi.empty()) {
        return false;
    }
    
    try {
        cv::Mat gray;
        if (face_roi.channels() == 3) {
            cv::cvtColor(face_roi, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = face_roi.clone();
        }
        
        eye_cascade_.detectMultiScale(gray, eyes, 1.1, 2, 
                                     cv::CASCADE_SCALE_IMAGE,
                                     cv::Size(10, 10));
        
        return eyes.size() >= 2;
        
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error in eye detection: " << e.what() << std::endl;
        return false;
    }
}

bool SocialPerceptionBias::detectMouth(const cv::Mat& face_roi, cv::Rect& mouth) {
    if (!config_.enable_lip_sync || face_roi.empty()) {
        return false;
    }
    
    try {
        cv::Mat gray;
        if (face_roi.channels() == 3) {
            cv::cvtColor(face_roi, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = face_roi.clone();
        }
        
        std::vector<cv::Rect> mouths;
        mouth_cascade_.detectMultiScale(gray, mouths, 1.1, 2,
                                       cv::CASCADE_SCALE_IMAGE,
                                       cv::Size(10, 10));
        
        if (!mouths.empty()) {
            mouth = mouths[0];  // Take the first detected mouth
            return true;
        }
        
        return false;
        
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error in mouth detection: " << e.what() << std::endl;
        return false;
    }
}

cv::Rect SocialPerceptionBias::estimateGazeTarget(const cv::Rect& face, 
                                                 const std::vector<cv::Rect>& eyes,
                                                 const cv::Size& frame_size) {
    if (eyes.size() < 2 || frame_size.width <= 0 || frame_size.height <= 0) {
        return cv::Rect();
    }
    
    // Validate face rectangle is within frame bounds
    if (face.x < 0 || face.y < 0 || 
        face.x + face.width > frame_size.width || 
        face.y + face.height > frame_size.height) {
        return cv::Rect();
    }
    
    // Calculate eye centers (relative to face) with bounds checking
    cv::Point2f left_eye_center(eyes[0].x + eyes[0].width / 2.0f, 
                               eyes[0].y + eyes[0].height / 2.0f);
    cv::Point2f right_eye_center(eyes[1].x + eyes[1].width / 2.0f, 
                                eyes[1].y + eyes[1].height / 2.0f);
    
    // Validate eye positions are within face bounds
    if (left_eye_center.x < 0 || left_eye_center.y < 0 ||
        left_eye_center.x >= face.width || left_eye_center.y >= face.height ||
        right_eye_center.x < 0 || right_eye_center.y < 0 ||
        right_eye_center.x >= face.width || right_eye_center.y >= face.height) {
        return cv::Rect();
    }
    
    // Ensure left eye is actually on the left
    if (left_eye_center.x > right_eye_center.x) {
        std::swap(left_eye_center, right_eye_center);
    }
    
    // Calculate gaze direction (simplified - assumes forward gaze)
    cv::Point2f eye_midpoint = (left_eye_center + right_eye_center) * 0.5f;
    
    // Convert to global coordinates with validation
    eye_midpoint.x += face.x;
    eye_midpoint.y += face.y;
    
    // Validate global eye position
    if (eye_midpoint.x < 0 || eye_midpoint.y < 0 ||
        eye_midpoint.x >= frame_size.width || eye_midpoint.y >= frame_size.height) {
        return cv::Rect();
    }
    
    // Project gaze forward (simplified projection)
    cv::Point2f gaze_target = eye_midpoint;
    gaze_target.y += config_.gaze_projection_distance;
    
    // Create target region with size validation
    int target_size = std::max(10, std::min(100, 50));  // Clamp target size between 10-100 pixels
    cv::Rect target_rect(
        static_cast<int>(gaze_target.x - target_size / 2),
        static_cast<int>(gaze_target.y - target_size / 2),
        target_size, target_size
    );
    
    // Enhanced clamping to frame boundaries with size adjustment
    if (target_rect.width > frame_size.width) target_rect.width = frame_size.width;
    if (target_rect.height > frame_size.height) target_rect.height = frame_size.height;
    
    target_rect.x = std::max(0, std::min(target_rect.x, frame_size.width - target_rect.width));
    target_rect.y = std::max(0, std::min(target_rect.y, frame_size.height - target_rect.height));
    
    // Final validation - ensure target rect is valid
    if (target_rect.width <= 0 || target_rect.height <= 0 ||
        target_rect.x < 0 || target_rect.y < 0 ||
        target_rect.x + target_rect.width > frame_size.width ||
        target_rect.y + target_rect.height > frame_size.height) {
        return cv::Rect();
    }
    
    return target_rect;
}

// NOTE: Removed duplicate detectLipSync implementation.
// The robust fixed-size patch version is defined later in this file.

std::vector<float> SocialPerceptionBias::extractLipMotion(const cv::Mat& mouth_roi) {
    std::vector<float> features;
    
    try {
        // Input validation
        if (mouth_roi.empty() || mouth_roi.rows <= 0 || mouth_roi.cols <= 0) {
            features.assign({0.0f, 0.0f, 0.0f});
            return features;
        }
        
        // Type assertion - ensure valid OpenCV matrix type
        if (mouth_roi.type() != CV_8UC1 && mouth_roi.type() != CV_8UC3 && 
            mouth_roi.type() != CV_8UC4 && mouth_roi.type() != CV_32FC1) {
            std::cerr << "Warning: Unexpected mouth ROI type: " << mouth_roi.type() << std::endl;
            features.assign({0.0f, 0.0f, 0.0f});
            return features;
        }
        
        // Enforce single-channel grayscale with robust conversion
        cv::Mat gray;
        if (mouth_roi.channels() == 1) {
            if (mouth_roi.type() == CV_32FC1) {
                mouth_roi.convertTo(gray, CV_8UC1, 255.0);
            } else {
                gray = mouth_roi.clone();
            }
        } else if (mouth_roi.channels() == 3) {
            cv::cvtColor(mouth_roi, gray, cv::COLOR_BGR2GRAY);
        } else if (mouth_roi.channels() == 4) {
            cv::cvtColor(mouth_roi, gray, cv::COLOR_BGRA2GRAY);
        } else {
            std::cerr << "Warning: Unsupported channel count: " << mouth_roi.channels() << std::endl;
            features.assign({0.0f, 0.0f, 0.0f});
            return features;
        }
        
        // Size assertion - ensure reasonable dimensions
        if (gray.rows < 4 || gray.cols < 4 || gray.rows > 1000 || gray.cols > 1000) {
            std::cerr << "Warning: Invalid mouth ROI dimensions: " << gray.cols << "x" << gray.rows << std::endl;
            features.assign({0.0f, 0.0f, 0.0f});
            return features;
        }
        
        // Resize to fixed patch to avoid size mismatch across frames
        cv::Mat patch;
        cv::resize(gray, patch, cv::Size(LIP_PATCH_WIDTH, LIP_PATCH_HEIGHT), 0, 0, cv::INTER_AREA);
        
        // Validate patch after resize
        if (patch.empty() || patch.type() != CV_8UC1) {
            std::cerr << "Warning: Invalid patch after resize" << std::endl;
            features.assign({0.0f, 0.0f, 0.0f});
            return features;
        }
        
        // Normalize intensity to [0,255] with robust handling
        cv::Mat patch_u8;
        double minVal = 0.0, maxVal = 0.0;
        cv::minMaxLoc(patch, &minVal, &maxVal);
        
        if (maxVal - minVal > 1e-5 && std::isfinite(minVal) && std::isfinite(maxVal)) {
            cv::Mat normalized;
            patch.convertTo(normalized, CV_32F);
            normalized = (normalized - static_cast<float>(minVal)) * (255.0f / static_cast<float>(maxVal - minVal));
            normalized.convertTo(patch_u8, CV_8U);
        } else {
            // Handle degenerate case (uniform intensity)
            patch_u8 = cv::Mat(patch.size(), CV_8U, cv::Scalar(128));
        }

        // Maintain fixed-size history of patches with buffer management
        if (!lip_motion_history_.empty()) {
            const cv::Mat& prev = lip_motion_history_.back();
            
            // Validate previous patch compatibility
            if (!prev.empty() && prev.size() == patch_u8.size() && prev.type() == patch_u8.type()) {
                cv::Mat diff;
                cv::absdiff(prev, patch_u8, diff);
                
                // Validate difference matrix
                if (!diff.empty() && diff.type() == CV_8UC1) {
                    // Downsample difference to a compact feature vector
                    cv::Mat diff_float;
                    diff.convertTo(diff_float, CV_32F, 1.0 / 255.0);
                    
                    // Compute simple statistics with validation
                    cv::Scalar mean_val, stddev_val;
                    cv::meanStdDev(diff_float, mean_val, stddev_val);
                    
                    // Validate computed statistics
                    if (std::isfinite(mean_val[0]) && std::isfinite(stddev_val[0])) {
                        features.push_back(static_cast<float>(mean_val[0]));
                        features.push_back(static_cast<float>(stddev_val[0]));
                        
                        // Add temporal energy (sum of diffs) with validation
                        cv::Scalar sum_val = cv::sum(diff_float);
                        if (std::isfinite(sum_val[0])) {
                            float energy = static_cast<float>(sum_val[0] / (LIP_PATCH_WIDTH * LIP_PATCH_HEIGHT));
                            features.push_back(energy);
                        } else {
                            features.push_back(0.0f);
                        }
                    } else {
                        // Statistics computation failed
                        features.assign({0.0f, 0.0f, 0.0f});
                    }
                } else {
                    // Difference computation failed
                    features.assign({0.0f, 0.0f, 0.0f});
                }
            } else {
                // History type/size mismatch: reset history to current patch
                lip_motion_history_.clear();
                std::cerr << "Warning: Lip motion history reset due to incompatible patch" << std::endl;
            }
        }

        // Push current patch into history with size management
        lip_motion_history_.push_back(patch_u8.clone());
        
        // Maintain history size limit with efficient removal
        while (lip_motion_history_.size() > LIP_MOTION_HISTORY_SIZE) {
            lip_motion_history_.erase(lip_motion_history_.begin());
        }

        // If no features yet (first frame or error), seed with zeros for stability
        if (features.empty()) {
            features.assign({0.0f, 0.0f, 0.0f});
        }
        
        // Final validation of feature vector
        for (auto& feature : features) {
            if (!std::isfinite(feature)) {
                feature = 0.0f;
            }
        }
        
    } catch (const cv::Exception& e) {
        // On OpenCV errors, return a safe default and reset history
        std::cerr << "OpenCV error in extractLipMotion: " << e.what() << std::endl;
        lip_motion_history_.clear();
        features.assign({0.0f, 0.0f, 0.0f});
    } catch (const std::exception& e) {
        std::cerr << "Standard error in extractLipMotion: " << e.what() << std::endl;
        lip_motion_history_.clear();
        features.assign({0.0f, 0.0f, 0.0f});
    } catch (...) {
        std::cerr << "Unknown error in extractLipMotion" << std::endl;
        lip_motion_history_.clear();
        features.assign({0.0f, 0.0f, 0.0f});
    }

   return features;
}

float SocialPerceptionBias::detectLipSync(const cv::Mat& mouth_roi, const AudioBuffer& audio) {
    try {
        // Extract motion features from fixed-size patch
        std::vector<float> motion = extractLipMotion(mouth_roi);
        // Build a simple envelope from motion features
        // Here we use mean and energy as proxies for mouth activity
        std::vector<float> lip_envelope;
        lip_envelope.push_back(motion.size() > 0 ? motion[0] : 0.0f);
        lip_envelope.push_back(motion.size() > 2 ? motion[2] : 0.0f);

        // If audio envelope empty, cannot correlate
        if (audio.audio_envelope.empty()) {
            return 0.0f;
        }
        // Normalize both signals to zero-mean unit-variance for robust correlation
        auto normalize = [](const std::vector<float>& x) {
            std::vector<float> y = x;
            if (y.empty()) return y;
            double mean = 0.0;
            for (float v : y) mean += v;
            mean /= static_cast<double>(y.size());
            double var = 0.0;
            for (float& v : y) { v = static_cast<float>(v - mean); var += v * v; }
            var /= std::max(1.0, static_cast<double>(y.size()));
            double stdv = std::sqrt(var);
            if (stdv < 1e-8) stdv = 1.0; // avoid divide-by-zero
            for (float& v : y) v = static_cast<float>(v / stdv);
            return y;
        };

        std::vector<float> lip_norm = normalize(lip_envelope);
        std::vector<float> aud_norm = normalize(audio.audio_envelope);

        // Simple cross-correlation at zero-lag (signals may be short)
        size_t n = std::min(lip_norm.size(), aud_norm.size());
        if (n == 0) return 0.0f;
        double dot = 0.0;
        for (size_t i = 0; i < n; ++i) dot += static_cast<double>(lip_norm[i]) * static_cast<double>(aud_norm[i]);
        double corr = dot / static_cast<double>(n);
        // Map to [0,1]
        corr = std::max(-1.0, std::min(1.0, corr));
        return static_cast<float>((corr + 1.0) * 0.5);
    } catch (...) {
        return 0.0f;
    }
}

float SocialPerceptionBias::crossCorrelate(const std::vector<float>& lip_motion,
                                          const std::vector<float>& audio_envelope) {
    if (lip_motion.empty() || audio_envelope.empty()) {
        return 0.0f;
    }
    
    // Normalize both signals
    auto normalize = [](const std::vector<float>& signal) {
        float mean = std::accumulate(signal.begin(), signal.end(), 0.0f) / signal.size();
        float variance = 0.0f;
        for (float val : signal) {
            variance += (val - mean) * (val - mean);
        }
        variance /= signal.size();
        float std_dev = std::sqrt(variance);
        
        std::vector<float> normalized;
        for (float val : signal) {
            normalized.push_back(std_dev > 0 ? (val - mean) / std_dev : 0.0f);
        }
        return normalized;
    };
    
    auto norm_lip = normalize(lip_motion);
    auto norm_audio = normalize(audio_envelope);
    
    // Calculate cross-correlation at zero lag (simplified)
    size_t min_size = std::min(norm_lip.size(), norm_audio.size());
    float correlation = 0.0f;
    
    for (size_t i = 0; i < min_size; ++i) {
        correlation += norm_lip[i] * norm_audio[i];
    }
    
    correlation /= min_size;
    
    return std::clamp(correlation, -1.0f, 1.0f);
}

bool SocialPerceptionBias::detectRhythmicPattern(const std::vector<float>& signal) {
    if (signal.size() < 4) {
        return false;
    }
    
    // Simple rhythmic pattern detection based on variance
    float mean = std::accumulate(signal.begin(), signal.end(), 0.0f) / signal.size();
    float variance = 0.0f;
    
    for (float val : signal) {
        variance += (val - mean) * (val - mean);
    }
    variance /= signal.size();
    
    // Consider rhythmic if variance is above threshold
    return variance > 10.0f;  // Threshold for rhythmic motion
}

float SocialPerceptionBias::calculateJointAttention(const SocialEvent& event) {
    if (event.gaze_target_box.empty()) {
        return 0.0f;
    }
    
    // Simple confidence based on gaze target size and position
    float confidence = 0.5f;  // Base confidence
    
    // Boost confidence if gaze target is reasonable size
    int target_area = event.gaze_target_box.width * event.gaze_target_box.height;
    if (target_area > 100 && target_area < 10000) {
        confidence += 0.3f;
    }
    
    // Boost confidence if face and gaze target don't overlap too much
    float overlap = calculateOverlap(event.face_box, event.gaze_target_box);
    if (overlap < 0.3f) {
        confidence += 0.2f;
    }
    
    return std::clamp(confidence, 0.0f, 1.0f);
}

void SocialPerceptionBias::updateFaceTracking(const std::vector<cv::Rect>& faces) {
    // Simple tracking based on overlap with previous faces
    std::vector<std::pair<int, cv::Rect>> new_tracked_faces;
    
    for (const auto& face : faces) {
        int best_id = -1;
        float best_overlap = 0.0f;
        
        // Find best matching tracked face
        for (const auto& tracked : tracked_faces_) {
            float overlap = calculateOverlap(face, tracked.second);
            if (overlap > best_overlap && overlap > 0.3f) {
                best_overlap = overlap;
                best_id = tracked.first;
            }
        }
        
        if (best_id == -1) {
            // New face
            best_id = next_tracking_id_++;
        }
        
        new_tracked_faces.emplace_back(best_id, face);
    }
    
    tracked_faces_ = std::move(new_tracked_faces);
}

int SocialPerceptionBias::getTrackingId(const cv::Rect& face) {
    for (const auto& tracked : tracked_faces_) {
        if (calculateOverlap(face, tracked.second) > 0.5f) {
            return tracked.first;
        }
    }
    return -1;
}

void SocialPerceptionBias::updateStatistics(const std::vector<SocialEvent>& events) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    stats_.social_events_created += static_cast<std::uint32_t>(events.size());
    
    if (!events.empty()) {
        float total_face_conf = 0.0f;
        float total_gaze_conf = 0.0f;
        float total_lip_conf = 0.0f;
        int gaze_count = 0;
        int lip_count = 0;
        
        for (const auto& event : events) {
            total_face_conf += 1.0f;  // Face confidence (simplified)
            
            if (event.gaze_confidence > 0.0f) {
                total_gaze_conf += event.gaze_confidence;
                gaze_count++;
            }
            
            if (event.lip_sync_confidence > 0.0f) {
                total_lip_conf += event.lip_sync_confidence;
                lip_count++;
            }
        }
        
        stats_.average_face_confidence = total_face_conf / events.size();
        stats_.average_gaze_confidence = gaze_count > 0 ? total_gaze_conf / gaze_count : 0.0f;
        stats_.average_lip_sync_confidence = lip_count > 0 ? total_lip_conf / lip_count : 0.0f;
    }
}

std::vector<SocialPerceptionBias::SocialEvent> SocialPerceptionBias::getRecentSocialEvents(size_t max_events) const {
    std::lock_guard<std::mutex> lock(events_mutex_);
    
    std::vector<SocialEvent> result;
    size_t start_idx = recent_events_.size() > max_events ? 
                      recent_events_.size() - max_events : 0;
    
    for (size_t i = start_idx; i < recent_events_.size(); ++i) {
        result.push_back(recent_events_[i]);
    }
    
    return result;
}

SocialPerceptionBias::Statistics SocialPerceptionBias::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void SocialPerceptionBias::updateConfig(const Config& new_config) {
    config_ = new_config;
}

void SocialPerceptionBias::clear() {
    std::lock_guard<std::mutex> events_lock(events_mutex_);
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    recent_events_.clear();
    lip_motion_history_.clear();
    tracked_faces_.clear();
    next_tracking_id_ = 1;
    stats_ = Statistics{};
}

bool SocialPerceptionBias::isOperational() const {
    return (config_.enable_face_detection ? !face_cascade_.empty() : true) &&
           (config_.enable_gaze_tracking ? !eye_cascade_.empty() : true) &&
           (config_.enable_lip_sync ? !mouth_cascade_.empty() : true);
}

float SocialPerceptionBias::calculateOverlap(const cv::Rect& rect1, const cv::Rect& rect2) const {
    cv::Rect intersection = rect1 & rect2;
    if (intersection.area() == 0) {
        return 0.0f;
    }
    
    cv::Rect union_rect = rect1 | rect2;
    return static_cast<float>(intersection.area()) / static_cast<float>(union_rect.area());
}

std::uint64_t SocialPerceptionBias::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

void SocialPerceptionBias::setBrain(NeuroForge::Core::HypergraphBrain* brain) {
    brain_ = brain;
}

void SocialPerceptionBias::setOutputGridSize(int grid_size) {
    if (grid_size > 0) {
        output_grid_size_ = grid_size;
    }
}

} // namespace Biases
} // namespace NeuroForge

// NEW: Enhanced detection methods for biological realism

namespace NeuroForge {
namespace Biases {

bool SocialPerceptionBias::detectFacesWithMasks(const cv::Mat& frame, 
                                               std::vector<cv::Rect>& faces,
                                               std::vector<cv::Mat>& face_masks,
                                               std::vector<std::vector<cv::Point>>& face_contours) {
    // First, use legacy face detection
    if (!detectFaces(frame, faces)) {
        return false;
    }
    
    // Generate masks and contours for each detected face
    face_masks.clear();
    face_contours.clear();
    
    for (const auto& face : faces) {
        cv::Mat face_roi = frame(face);
        cv::Mat face_mask;
        std::vector<cv::Point> face_contour;
        
        if (generateFaceMask(face_roi, face_mask, face_contour)) {
            face_masks.push_back(face_mask);
            
            // Convert contour points to global coordinates
            for (auto& point : face_contour) {
                point.x += face.x;
                point.y += face.y;
            }
            face_contours.push_back(face_contour);
        } else {
            // Fallback: create simple rectangular mask
            cv::Mat fallback_mask = cv::Mat::zeros(face_roi.size(), CV_8UC1);
            cv::rectangle(fallback_mask, cv::Rect(0, 0, face_roi.cols, face_roi.rows), cv::Scalar(255), -1);
            face_masks.push_back(fallback_mask);
            
            // Fallback: rectangular contour
            std::vector<cv::Point> rect_contour = {
                cv::Point(face.x, face.y),
                cv::Point(face.x + face.width, face.y),
                cv::Point(face.x + face.width, face.y + face.height),
                cv::Point(face.x, face.y + face.height)
            };
            face_contours.push_back(rect_contour);
        }
    }
    
    return true;
}

bool SocialPerceptionBias::detectEyesWithPupils(const cv::Mat& face_roi, 
                                               std::vector<cv::Rect>& eyes,
                                               std::vector<cv::Point2f>& pupil_positions,
                                               std::vector<std::vector<cv::Point>>& eye_contours) {
    // First, use legacy eye detection
    if (!detectEyes(face_roi, eyes)) {
        return false;
    }
    
    pupil_positions.clear();
    eye_contours.clear();
    
    // Extract pupil positions and contours for each eye
    for (const auto& eye : eyes) {
        cv::Mat eye_roi = face_roi(eye);
        cv::Point2f pupil_pos;
        std::vector<cv::Point> eye_contour;
        
        if (extractPupilPosition(eye_roi, pupil_pos, eye_contour)) {
            // Convert to face-relative coordinates
            pupil_pos.x += eye.x;
            pupil_pos.y += eye.y;
            pupil_positions.push_back(pupil_pos);
            
            // Convert contour to face-relative coordinates
            for (auto& point : eye_contour) {
                point.x += eye.x;
                point.y += eye.y;
            }
            eye_contours.push_back(eye_contour);
        } else {
            // Fallback: use eye center
            cv::Point2f eye_center(eye.x + eye.width / 2.0f, eye.y + eye.height / 2.0f);
            pupil_positions.push_back(eye_center);
            
            // Fallback: circular contour
            std::vector<cv::Point> circle_contour;
            int radius = std::min(eye.width, eye.height) / 2;
            for (int angle = 0; angle < 360; angle += 10) {
                float rad = angle * CV_PI / 180.0f;
                cv::Point pt(eye_center.x + radius * cos(rad), eye_center.y + radius * sin(rad));
                circle_contour.push_back(pt);
            }
            eye_contours.push_back(circle_contour);
        }
    }
    
    return !pupil_positions.empty();
}

bool SocialPerceptionBias::detectMouthWithMask(const cv::Mat& face_roi, 
                                              cv::Rect& mouth,
                                              cv::Mat& mouth_mask) {
    // First, use legacy mouth detection
    if (!detectMouth(face_roi, mouth)) {
        return false;
    }
    
    // Generate precise mouth mask
    cv::Mat mouth_roi = face_roi(mouth);
    std::vector<cv::Point> mouth_contour;
    
    if (generateFaceMask(mouth_roi, mouth_mask, mouth_contour)) {
        return true;
    } else {
        // Fallback: create simple rectangular mask
        mouth_mask = cv::Mat::zeros(mouth_roi.size(), CV_8UC1);
        cv::rectangle(mouth_mask, cv::Rect(0, 0, mouth_roi.cols, mouth_roi.rows), cv::Scalar(255), -1);
        return true;
    }
}

float SocialPerceptionBias::computeGazeVector(const cv::Rect& face,
                                            const std::vector<cv::Point2f>& pupil_positions,
                                            const cv::Size& frame_size,
                                            cv::Point2f& gaze_vector,
                                            float& gaze_angle,
                                            cv::Rect& target_box) {
    if (pupil_positions.size() < 2) {
        gaze_vector = cv::Point2f(0, 0);
        gaze_angle = 0.0f;
        target_box = cv::Rect();
        return 0.0f;
    }
    
    // Calculate eye midpoint
    cv::Point2f left_pupil = pupil_positions[0];
    cv::Point2f right_pupil = pupil_positions[1];
    
    // Ensure left is actually left
    if (left_pupil.x > right_pupil.x) {
        std::swap(left_pupil, right_pupil);
    }
    
    cv::Point2f eye_midpoint = (left_pupil + right_pupil) * 0.5f;
    
    // Convert to global coordinates
    eye_midpoint.x += face.x;
    eye_midpoint.y += face.y;
    
    // Calculate gaze direction vector (simplified model)
    cv::Point2f face_center(face.x + face.width / 2.0f, face.y + face.height / 2.0f);
    cv::Point2f gaze_direction = eye_midpoint - face_center;
    
    // Normalize gaze vector
    float magnitude = sqrt(gaze_direction.x * gaze_direction.x + gaze_direction.y * gaze_direction.y);
    if (magnitude > 0.001f) {
        gaze_vector = gaze_direction / magnitude;
    } else {
        gaze_vector = cv::Point2f(0, 1); // Default forward gaze
    }
    
    // Calculate gaze angle
    gaze_angle = atan2(gaze_vector.y, gaze_vector.x);
    
    // Project gaze to target location
    cv::Point2f gaze_target = eye_midpoint + gaze_vector * config_.gaze_projection_distance;
    
    // Create target box
    int target_size = 50;
    target_box = cv::Rect(
        static_cast<int>(gaze_target.x - target_size / 2),
        static_cast<int>(gaze_target.y - target_size / 2),
        target_size, target_size
    );
    
    // Clamp to frame boundaries
    target_box.x = std::max(0, std::min(target_box.x, frame_size.width - target_box.width));
    target_box.y = std::max(0, std::min(target_box.y, frame_size.height - target_box.height));
    
    // Calculate confidence based on pupil symmetry and clarity
    float pupil_distance = sqrt(pow(right_pupil.x - left_pupil.x, 2) + pow(right_pupil.y - left_pupil.y, 2));
    float expected_distance = face.width * 0.3f; // Typical inter-pupil distance
    float distance_confidence = 1.0f - std::abs(pupil_distance - expected_distance) / expected_distance;
    
    return std::max(0.0f, std::min(1.0f, distance_confidence));
}

bool SocialPerceptionBias::generateFaceMask(const cv::Mat& face_roi, 
                                           cv::Mat& face_mask,
                                           std::vector<cv::Point>& face_contour) {
    if (face_roi.empty()) {
        return false;
    }
    
    try {
        // Convert to grayscale if needed
        cv::Mat gray;
        if (face_roi.channels() == 3) {
            cv::cvtColor(face_roi, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = face_roi.clone();
        }
        
        // Apply Gaussian blur to reduce noise
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(5, 5), 1.5);
        
        // Apply Canny edge detection
        cv::Mat edges;
        cv::Canny(blurred, edges, 50, 150);
        
        // Find contours
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        if (contours.empty()) {
            return false;
        }
        
        // Find the largest contour (likely the face outline)
        auto largest_contour = *std::max_element(contours.begin(), contours.end(),
            [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
                return cv::contourArea(a) < cv::contourArea(b);
            });
        
        // Create mask from contour
        face_mask = cv::Mat::zeros(face_roi.size(), CV_8UC1);
        cv::fillPoly(face_mask, std::vector<std::vector<cv::Point>>{largest_contour}, cv::Scalar(255));
        
        // Smooth the mask
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
        cv::morphologyEx(face_mask, face_mask, cv::MORPH_CLOSE, kernel);
        cv::morphologyEx(face_mask, face_mask, cv::MORPH_OPEN, kernel);
        
        face_contour = largest_contour;
        return true;
        
    } catch (const cv::Exception& e) {
        std::cerr << "Error in generateFaceMask: " << e.what() << std::endl;
        return false;
    }
}

bool SocialPerceptionBias::extractPupilPosition(const cv::Mat& eye_roi,
                                               cv::Point2f& pupil_position,
                                               std::vector<cv::Point>& eye_contour) {
    if (eye_roi.empty()) {
        return false;
    }
    
    try {
        // Convert to grayscale if needed
        cv::Mat gray;
        if (eye_roi.channels() == 3) {
            cv::cvtColor(eye_roi, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = eye_roi.clone();
        }
        
        // Apply Gaussian blur
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 1.0);
        
        // Find the darkest region (pupil)
        cv::Point min_loc;
        cv::minMaxLoc(blurred, nullptr, nullptr, &min_loc, nullptr);
        pupil_position = cv::Point2f(min_loc.x, min_loc.y);
        
        // Generate eye contour using edge detection
        cv::Mat edges;
        cv::Canny(blurred, edges, 30, 100);
        
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        
        if (!contours.empty()) {
            // Find contour closest to pupil
            auto closest_contour = *std::min_element(contours.begin(), contours.end(),
                [&pupil_position](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
                    float dist_a = cv::pointPolygonTest(a, pupil_position, true);
                    float dist_b = cv::pointPolygonTest(b, pupil_position, true);
                    return std::abs(dist_a) < std::abs(dist_b);
                });
            eye_contour = closest_contour;
        } else {
            // Fallback: circular contour around pupil
            int radius = std::min(eye_roi.cols, eye_roi.rows) / 4;
            for (int angle = 0; angle < 360; angle += 15) {
                float rad = angle * CV_PI / 180.0f;
                cv::Point pt(pupil_position.x + radius * cos(rad), pupil_position.y + radius * sin(rad));
                eye_contour.push_back(pt);
            }
        }
        
        return true;
        
    } catch (const cv::Exception& e) {
        std::cerr << "Error in extractPupilPosition: " << e.what() << std::endl;
        return false;
    }
}

float SocialPerceptionBias::detectLipSyncWithMask(const cv::Mat& mouth_mask, const AudioBuffer& audio) {
    if (mouth_mask.empty() || audio.audio_envelope.empty()) {
        return 0.0f;
    }
    
    try {
        // Extract motion features from mouth mask
        std::vector<float> mask_motion = extractLipMotion(mouth_mask);
        
        // Cross-correlate with audio envelope
        return crossCorrelate(mask_motion, audio.audio_envelope);
        
    } catch (const std::exception& e) {
        std::cerr << "Error in detectLipSyncWithMask: " << e.what() << std::endl;
        return 0.0f;
    }
}

void SocialPerceptionBias::encodeMasksToGrid(std::vector<float>& features,
                                           const std::vector<SocialEvent>& events,
                                           int grid_size) {
    if (features.size() != static_cast<size_t>(grid_size * grid_size)) {
        return;
    }
    
    float fatigue_scale = 1.0f;
    if (brain_ != nullptr) {
        auto stats_opt = brain_->getLearningStatistics();
        if (stats_opt.has_value()) {
            float h = stats_opt->metabolic_hazard;
            float scale = 1.0f - 0.5f * std::max(0.0f, std::min(1.0f, h));
            fatigue_scale = std::max(0.5f, scale);
        }
    }

    for (const auto& event : events) {
        // Encode face mask into grid
        if (!event.face_mask.empty()) {
            cv::Mat resized_mask;
            cv::resize(event.face_mask, resized_mask, cv::Size(grid_size, grid_size));
            
            for (int y = 0; y < grid_size; ++y) {
                for (int x = 0; x < grid_size; ++x) {
                    int idx = y * grid_size + x;
                    float mask_value = resized_mask.at<uchar>(y, x) / 255.0f;
                    features[idx] += mask_value * event.total_salience_boost * fatigue_scale;
                }
            }
        }
        
        // Apply gaze vector attention
        if (event.gaze_vector.x != 0 || event.gaze_vector.y != 0) {
            applyGazeAttention(features, event.gaze_vector, event.attention_strength, grid_size);
        }
    }
}

void SocialPerceptionBias::applyGazeAttention(std::vector<float>& features,
                                            const cv::Point2f& gaze_vector,
                                            float attention_strength,
                                            int grid_size) {
    if (features.size() != static_cast<size_t>(grid_size * grid_size)) {
        return;
    }
    
    // Create attention gradient along gaze direction
    cv::Point2f center(grid_size / 2.0f, grid_size / 2.0f);
    
    for (int y = 0; y < grid_size; ++y) {
        for (int x = 0; x < grid_size; ++x) {
            cv::Point2f pos(x, y);
            cv::Point2f dir = pos - center;
            
            // Normalize direction
            float magnitude = sqrt(dir.x * dir.x + dir.y * dir.y);
            if (magnitude > 0.001f) {
                dir /= magnitude;
            }
            
            // Calculate alignment with gaze vector
            float alignment = dir.dot(gaze_vector);
            
            // Apply attention boost based on alignment
            if (alignment > 0) {
                int idx = y * grid_size + x;
                features[idx] *= (1.0f + alignment * attention_strength * 0.5f);
            }
        }
    }
}

} // namespace Biases
} // namespace NeuroForge

#endif // NF_HAVE_OPENCV
