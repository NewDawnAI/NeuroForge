#include "FaceDetectionBias.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace NeuroForge {
namespace Biases {

FaceDetectionBias::FaceDetectionBias(const Config& config) : config_(config) {
#ifdef NF_HAVE_OPENCV
    try {
        cascade_loaded_ = initializeCascade();
        if (!cascade_loaded_) {
            std::cerr << "Warning: Face detection cascade could not be loaded. Face detection will be disabled." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error initializing face detection cascade: " << e.what() << std::endl;
        cascade_loaded_ = false;
    }
#endif
}

bool FaceDetectionBias::applyFaceBias(std::vector<float>& features, 
                                     const std::vector<float>& gray_image,
                                     int grid_size) {
    if (!validateParameters(grid_size) || features.empty()) {
        return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(detection_mutex_);
    
    std::vector<FaceDetectionBias::FaceInfo> detected_faces;
    bool detection_performed = false;
    
    // Attempt face detection if we have image data
    if (!gray_image.empty()) {
        // Assume square image for grid mapping
        int image_size = static_cast<int>(std::sqrt(gray_image.size()));
        if (image_size * image_size == static_cast<int>(gray_image.size())) {
            detection_performed = detectFacesFromGray(gray_image, image_size, image_size, detected_faces);
        }
    }
    
    // Apply face bias if faces were detected
    if (detection_performed && !detected_faces.empty()) {
        // Update face tracking
        if (config_.enable_face_tracking) {
            updateFaceTracking(detected_faces);
        }
        
        // Apply attention boost to face regions
        if (config_.enable_attention_boost) {
            applyAttentionBoost(features, detected_faces, grid_size);
        }
        
        // Apply background suppression
        if (config_.background_suppression < 1.0f) {
            applyBackgroundSuppression(features, detected_faces, grid_size);
        }
        
        current_faces_ = detected_faces;
    }
    
    // Update statistics
    auto end_time = std::chrono::steady_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    updateStatistics(static_cast<std::uint64_t>(processing_time), detected_faces.size());
    
    return detection_performed;
}

#ifdef NF_HAVE_OPENCV
bool FaceDetectionBias::applyFaceBias(std::vector<float>& features, 
                                     const cv::Mat& frame,
                                     int grid_size) {
    if (!validateParameters(grid_size) || features.empty() || frame.empty()) {
        return false;
    }
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(detection_mutex_);
    
    std::vector<FaceDetectionBias::FaceInfo> face_info;
    bool detection_performed = detectFacesDetailed(frame, face_info);
    
    // Apply face bias if faces were detected
    if (detection_performed && !face_info.empty()) {
        // Update face tracking
        if (config_.enable_face_tracking) {
            updateFaceTracking(face_info);
        }
        
        // Apply attention boost to face regions
        if (config_.enable_attention_boost) {
            applyAttentionBoost(features, face_info, grid_size);
        }
        
        // Apply background suppression
        if (config_.background_suppression < 1.0f) {
            applyBackgroundSuppression(features, face_info, grid_size);
        }
        
        current_faces_ = face_info;
    }
    
    // Update statistics
    auto end_time = std::chrono::steady_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    updateStatistics(static_cast<std::uint64_t>(processing_time), face_info.size());
    
    return detection_performed;
}

bool FaceDetectionBias::detectFaces(const cv::Mat& frame, std::vector<cv::Rect>& faces) {
    if (!cascade_loaded_ || frame.empty()) {
        return false;
    }
    
    try {
        cv::Mat gray_frame;
        if (frame.channels() == 3) {
            cv::cvtColor(frame, gray_frame, cv::COLOR_BGR2GRAY);
        } else if (frame.channels() == 1) {
            gray_frame = frame;
        } else {
            return false;
        }
        
        // Equalize histogram for better detection
        cv::equalizeHist(gray_frame, gray_frame);
        
        // Detect faces
        face_cascade_.detectMultiScale(
            gray_frame,
            faces,
            config_.scale_factor,
            config_.min_neighbors,
            0,
            cv::Size(config_.min_face_size, config_.min_face_size),
            cv::Size(config_.max_face_size, config_.max_face_size)
        );
        
        return true;
        
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error in face detection: " << e.what() << std::endl;
        return false;
    }
}

bool FaceDetectionBias::detectFacesDetailed(const cv::Mat& frame, std::vector<FaceDetectionBias::FaceInfo>& face_info) {
    std::vector<cv::Rect> faces;
    if (!detectFaces(frame, faces)) {
        return false;
    }
    
    face_info.clear();
    face_info.reserve(faces.size());
    
    std::uint64_t current_time = getCurrentTimeMs();
    
    for (const auto& face_rect : faces) {
        FaceDetectionBias::FaceInfo info;
        info.x = face_rect.x;
        info.y = face_rect.y;
        info.width = face_rect.width;
        info.height = face_rect.height;
        info.confidence = 1.0f; // Haar cascades don't provide confidence scores
        info.attention_weight = config_.face_priority_multiplier;
        info.detection_time_ms = current_time;
        info.tracking_id = -1; // Will be assigned during tracking update
        
        face_info.push_back(info);
    }
    
    return true;
}
#endif

bool FaceDetectionBias::detectFacesFromGray(const std::vector<float>& gray_image,
                                           int width, int height,
                                           std::vector<FaceDetectionBias::FaceInfo>& face_info) {
    // Initialize face_info for all code paths
    face_info.clear();
    
#ifdef NF_HAVE_OPENCV
    // If OpenCV is available and cascade is loaded, try OpenCV detection first
    if (cascade_loaded_ && !gray_image.empty() && width > 0 && height > 0) {
        try {
            // Convert float vector to cv::Mat
            cv::Mat gray_mat(height, width, CV_32F, const_cast<float*>(gray_image.data()));
            
            // Convert to 8-bit for cascade detection
            cv::Mat gray_8bit;
            gray_mat.convertTo(gray_8bit, CV_8U, 255.0);
            
            // Equalize histogram
            cv::equalizeHist(gray_8bit, gray_8bit);
            
            return detectFacesDetailed(gray_8bit, face_info);
            
        } catch (const cv::Exception& e) {
            std::cerr << "OpenCV error in grayscale face detection: " << e.what() << std::endl;
            // Fall through to heuristic detection
        }
    }
#endif
    
    // Fallback: simple heuristic-based face detection
    // This runs when OpenCV is not available OR when cascade loading failed
    
    if (gray_image.empty() || width <= 0 || height <= 0) {
        return false;
    }
    
    // Very basic heuristic: look for regions with moderate intensity variation
    // This is a placeholder for when OpenCV is not available or cascade fails
    const float target_intensity = 0.5f; // Typical face intensity
    const float intensity_tolerance = 0.3f;
    
    // Scan for face-like regions (simplified)
    int min_face_size = std::max(width / 10, 20);
    int max_face_size = std::min(width / 3, height / 3);
    
    // Limit the number of potential faces to avoid excessive computation
    int max_faces = 10;
    int faces_found = 0;
    
    for (int y = 0; y < height - min_face_size && faces_found < max_faces; y += min_face_size / 2) {
        for (int x = 0; x < width - min_face_size && faces_found < max_faces; x += min_face_size / 2) {
            for (int size = min_face_size; size <= max_face_size && 
                 x + size < width && y + size < height && faces_found < max_faces; size += min_face_size / 4) {
                
                // Calculate average intensity in region
                float avg_intensity = 0.0f;
                int pixel_count = 0;
                
                for (int dy = 0; dy < size; ++dy) {
                    for (int dx = 0; dx < size; ++dx) {
                        int idx = (y + dy) * width + (x + dx);
                        if (idx >= 0 && idx < static_cast<int>(gray_image.size())) {
                            avg_intensity += gray_image[idx];
                            pixel_count++;
                        }
                    }
                }
                
                if (pixel_count > 0) {
                    avg_intensity /= pixel_count;
                    
                    // Check if intensity is in face-like range
                    if (std::abs(avg_intensity - target_intensity) < intensity_tolerance) {
                        FaceInfo info;
                        info.x = x;
                        info.y = y;
                        info.width = size;
                        info.height = size;
                        info.confidence = 1.0f - std::abs(avg_intensity - target_intensity) / intensity_tolerance;
                        info.attention_weight = config_.face_priority_multiplier;
                        info.detection_time_ms = getCurrentTimeMs();
                        info.tracking_id = -1;
                        
                        face_info.push_back(info);
                        faces_found++;
                    }
                }
            }
        }
    }
    
    return true; // Always return true for heuristic detection - it attempted detection
}

void FaceDetectionBias::applyAttentionBoost(std::vector<float>& features,
                                           const std::vector<FaceDetectionBias::FaceInfo>& faces,
                                           int grid_size) {
    if (faces.empty() || features.size() != static_cast<size_t>(grid_size * grid_size)) {
        return;
    }
    
    for (int grid_y = 0; grid_y < grid_size; ++grid_y) {
        for (int grid_x = 0; grid_x < grid_size; ++grid_x) {
            int feature_idx = grid_y * grid_size + grid_x;
            
            float attention_weight = calculateAttentionWeight(grid_x, grid_y, faces, grid_size);
            
            // Apply attention boost
            features[feature_idx] *= attention_weight;
            
            // Clamp to prevent overflow
            features[feature_idx] = std::clamp(features[feature_idx], 0.0f, 2.0f);
        }
    }
}

void FaceDetectionBias::applyBackgroundSuppression(std::vector<float>& features,
                                                   const std::vector<FaceDetectionBias::FaceInfo>& faces,
                                                   int grid_size) {
    if (faces.empty() || features.size() != static_cast<size_t>(grid_size * grid_size)) {
        return;
    }
    
    for (int grid_y = 0; grid_y < grid_size; ++grid_y) {
        for (int grid_x = 0; grid_x < grid_size; ++grid_x) {
            int feature_idx = grid_y * grid_size + grid_x;
            
            float attention_weight = calculateAttentionWeight(grid_x, grid_y, faces, grid_size);
            
            // If attention weight is low (not near a face), apply suppression
            if (attention_weight < 1.5f) {
                features[feature_idx] *= config_.background_suppression;
            }
        }
    }
}

float FaceDetectionBias::calculateAttentionWeight(int grid_x, int grid_y,
                                                 const std::vector<FaceDetectionBias::FaceInfo>& faces,
                                                 int grid_size) const {
    if (faces.empty()) {
        return 1.0f; // No attention boost if no faces
    }
    
    float max_weight = 1.0f;
    
    // Convert grid coordinates to normalized coordinates [0,1]
    float norm_x = static_cast<float>(grid_x) / grid_size;
    float norm_y = static_cast<float>(grid_y) / grid_size;
    
    for (const auto& face : faces) {
        // Convert face coordinates to normalized coordinates
        float face_center_x = (face.x + face.width * 0.5f) / 100.0f; // Assume 100x100 reference
        float face_center_y = (face.y + face.height * 0.5f) / 100.0f;
        float face_radius = std::max(face.width, face.height) * 0.5f / 100.0f;
        
        // Calculate distance from grid point to face center
        float distance = calculateDistance(norm_x, norm_y, face_center_x, face_center_y);
        
        // Calculate attention radius
        float attention_radius = face_radius * config_.attention_radius_scale;
        
        // Calculate attention weight based on distance
        if (distance <= attention_radius) {
            float weight_factor = 1.0f - (distance / attention_radius);
            float attention_weight = 1.0f + (face.attention_weight - 1.0f) * weight_factor;
            max_weight = std::max(max_weight, attention_weight);
        }
    }
    
    return max_weight;
}

void FaceDetectionBias::updateFaceTracking(std::vector<FaceDetectionBias::FaceInfo>& new_faces) {
    if (!config_.enable_face_tracking) {
        return;
    }
    
    // Match new faces with previous faces
    auto matches = matchFaces(new_faces, previous_faces_);
    
    // Assign tracking IDs
    for (size_t i = 0; i < new_faces.size(); ++i) {
        bool matched = false;
        
        for (const auto& match : matches) {
            if (match.first == static_cast<int>(i)) {
                // This face matches a previous face
                new_faces[i].tracking_id = previous_faces_[match.second].tracking_id;
                matched = true;
                break;
            }
        }
        
        if (!matched) {
            // New face, assign new tracking ID
            new_faces[i].tracking_id = next_tracking_id_++;
        }
    }
    
    // Update both current and previous faces for next frame
    current_faces_ = new_faces;
    previous_faces_ = new_faces;
}

std::vector<std::pair<int, int>> FaceDetectionBias::matchFaces(const std::vector<FaceDetectionBias::FaceInfo>& current_faces,
                                                              const std::vector<FaceDetectionBias::FaceInfo>& previous_faces) const {
    std::vector<std::pair<int, int>> matches;
    
    if (current_faces.empty() || previous_faces.empty()) {
        return matches;
    }
    
    // Simple greedy matching based on overlap
    std::vector<bool> current_matched(current_faces.size(), false);
    std::vector<bool> previous_matched(previous_faces.size(), false);
    
    for (size_t i = 0; i < current_faces.size(); ++i) {
        if (current_matched[i]) continue;
        
        float best_overlap = 0.0f;
        int best_match = -1;
        
        for (size_t j = 0; j < previous_faces.size(); ++j) {
            if (previous_matched[j]) continue;
            
            float overlap = calculateFaceOverlap(current_faces[i], previous_faces[j]);
            if (overlap > best_overlap && overlap > 0.3f) { // Minimum overlap threshold
                best_overlap = overlap;
                best_match = static_cast<int>(j);
            }
        }
        
        if (best_match >= 0) {
            matches.emplace_back(static_cast<int>(i), best_match);
            current_matched[i] = true;
            previous_matched[best_match] = true;
        }
    }
    
    return matches;
}

float FaceDetectionBias::calculateFaceOverlap(const FaceDetectionBias::FaceInfo& face1, const FaceDetectionBias::FaceInfo& face2) const {
    // Calculate intersection rectangle
    int x1 = std::max(face1.x, face2.x);
    int y1 = std::max(face1.y, face2.y);
    int x2 = std::min(face1.x + face1.width, face2.x + face2.width);
    int y2 = std::min(face1.y + face1.height, face2.y + face2.height);
    
    if (x2 <= x1 || y2 <= y1) {
        return 0.0f; // No overlap
    }
    
    // Calculate areas
    int intersection_area = (x2 - x1) * (y2 - y1);
    int face1_area = face1.width * face1.height;
    int face2_area = face2.width * face2.height;
    int union_area = face1_area + face2_area - intersection_area;
    
    if (union_area <= 0) {
        return 0.0f;
    }
    
    // Return IoU (Intersection over Union)
    return static_cast<float>(intersection_area) / union_area;
}

void FaceDetectionBias::setFacePriorityMultiplier(float multiplier) {
    config_.face_priority_multiplier = std::max(multiplier, 1.0f);
}

void FaceDetectionBias::setConfig(const Config& new_config) {
    std::lock_guard<std::mutex> lock(detection_mutex_);
    config_ = new_config;
    
#ifdef NF_HAVE_OPENCV
    // Reload cascade if path changed
    if (!cascade_loaded_ || config_.cascade_path != new_config.cascade_path) {
        cascade_loaded_ = initializeCascade();
    }
#endif
}

FaceDetectionBias::Statistics FaceDetectionBias::getStatistics() const {
    Statistics stats;
    
    std::lock_guard<std::mutex> lock(detection_mutex_);
    
    stats.total_frames_processed = total_frames_processed_.load();
    stats.frames_with_faces = frames_with_faces_.load();
    stats.total_faces_detected = total_faces_detected_.load();
    stats.processing_time_ms = total_processing_time_ms_.load();
    
    if (stats.total_frames_processed > 0) {
        stats.face_detection_rate = static_cast<float>(stats.frames_with_faces) / stats.total_frames_processed;
        stats.average_faces_per_frame = static_cast<float>(stats.total_faces_detected) / stats.total_frames_processed;
    }
    
    if (stats.total_faces_detected > 0) {
        stats.average_face_confidence = 1.0f; // Placeholder - would need to track actual confidences
    }
    
#ifdef NF_HAVE_OPENCV
    stats.opencv_available = true;
    stats.cascade_loaded = cascade_loaded_;
#else
    stats.opencv_available = false;
    stats.cascade_loaded = false;
#endif
    
    return stats;
}

std::vector<FaceDetectionBias::FaceInfo> FaceDetectionBias::getCurrentFaces() const {
    std::lock_guard<std::mutex> lock(detection_mutex_);
    return current_faces_;
}

bool FaceDetectionBias::isOperational() const {
#ifdef NF_HAVE_OPENCV
    return cascade_loaded_;
#else
    return true; // Fallback detection is always available
#endif
}

bool FaceDetectionBias::isOpenCVAvailable() const {
#ifdef NF_HAVE_OPENCV
    return true;
#else
    return false;
#endif
}

void FaceDetectionBias::clearTracking() {
    std::lock_guard<std::mutex> lock(detection_mutex_);
    current_faces_.clear();
    previous_faces_.clear();
    next_tracking_id_ = 1;
}

bool FaceDetectionBias::initializeCascade() {
#ifdef NF_HAVE_OPENCV
    try {
        // Try to load the cascade file
        if (face_cascade_.load(config_.cascade_path)) {
            return true;
        }
        
        // If custom path fails, try default OpenCV data paths
        std::vector<std::string> default_paths = {
            "haarcascade_frontalface_alt.xml",
            "data/haarcascades/haarcascade_frontalface_alt.xml",
            "/usr/share/opencv/haarcascades/haarcascade_frontalface_alt.xml",
            "/usr/local/share/opencv/haarcascades/haarcascade_frontalface_alt.xml"
        };
        
        for (const auto& path : default_paths) {
            if (face_cascade_.load(path)) {
                config_.cascade_path = path;
                return true;
            }
        }
        
        std::cerr << "Could not load face detection cascade from any path" << std::endl;
        return false;
        
    } catch (const cv::Exception& e) {
        std::cerr << "OpenCV error loading cascade: " << e.what() << std::endl;
        return false;
    }
#else
    return false;
#endif
}

void FaceDetectionBias::gridToImageCoords(int grid_x, int grid_y, int grid_size,
                                         int image_width, int image_height,
                                         int& img_x, int& img_y) const {
    img_x = (grid_x * image_width) / grid_size;
    img_y = (grid_y * image_height) / grid_size;
}

void FaceDetectionBias::imageToGridCoords(int img_x, int img_y, int image_width, int image_height,
                                         int grid_size, int& grid_x, int& grid_y) const {
    grid_x = (img_x * grid_size) / image_width;
    grid_y = (img_y * grid_size) / image_height;
    
    // Clamp to valid range
    grid_x = std::clamp(grid_x, 0, grid_size - 1);
    grid_y = std::clamp(grid_y, 0, grid_size - 1);
}

float FaceDetectionBias::calculateDistance(float x1, float y1, float x2, float y2) const {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

void FaceDetectionBias::updateStatistics(std::uint64_t processing_time_ms, size_t faces_detected) {
    total_frames_processed_.fetch_add(1);
    total_processing_time_ms_.fetch_add(processing_time_ms);
    
    if (faces_detected > 0) {
        frames_with_faces_.fetch_add(1);
        total_faces_detected_.fetch_add(faces_detected);
    }
}

std::uint64_t FaceDetectionBias::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

bool FaceDetectionBias::validateParameters(int grid_size) const {
    return grid_size > 0 && grid_size <= 1000; // Reasonable limits
}

} // namespace Biases
} // namespace NeuroForge