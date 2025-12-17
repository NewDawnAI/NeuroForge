#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <string>
#include <chrono>

#ifdef NF_HAVE_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#endif

namespace NeuroForge {
namespace Biases {

/**
 * @brief Face Detection Priority Enhancement
 * 
 * Implements biologically plausible perceptual bias for face detection,
 * enhancing visual processing with face-specific attention mechanisms.
 * Integrates with existing VisionEncoder for seamless operation.
 */
class FaceDetectionBias {
public:
    /**
     * @brief Face detection configuration
     */
    struct Config {
        float face_priority_multiplier{2.0f};      // Priority boost for face regions
        float face_detection_threshold{0.3f};      // Minimum confidence for face detection
        float attention_radius_scale{1.5f};        // Attention radius around detected faces
        float background_suppression{0.7f};        // Suppression factor for non-face regions
        bool enable_face_tracking{true};           // Enable temporal face tracking
        bool enable_attention_boost{true};         // Enable attention-based feature boosting
        std::string cascade_path{"haarcascade_frontalface_alt.xml"}; // Haar cascade file path
        int min_face_size{30};                     // Minimum face size in pixels
        int max_face_size{300};                    // Maximum face size in pixels
        double scale_factor{1.1};                  // Scale factor for multi-scale detection
        int min_neighbors{3};                      // Minimum neighbors for detection
    };
    
    /**
     * @brief Detected face information
     */
    struct FaceInfo {
        int x, y, width, height;                   // Face bounding box
        float confidence{0.0f};                    // Detection confidence
        float attention_weight{1.0f};              // Attention weight for this face
        std::uint64_t detection_time_ms{0};        // Time of detection
        int tracking_id{-1};                       // Tracking ID for temporal consistency
    };
    
    /**
     * @brief Face detection statistics
     */
    struct Statistics {
        std::uint64_t total_frames_processed{0};
        std::uint64_t frames_with_faces{0};
        std::uint64_t total_faces_detected{0};
        float face_detection_rate{0.0f};
        float average_faces_per_frame{0.0f};
        float average_face_confidence{0.0f};
        bool opencv_available{false};
        bool cascade_loaded{false};
        std::uint64_t processing_time_ms{0};
    };
    
private:
    Config config_;
    mutable std::mutex detection_mutex_;
    
#ifdef NF_HAVE_OPENCV
    cv::CascadeClassifier face_cascade_;
    bool cascade_loaded_{false};
#endif
    
    // Face tracking
    std::vector<FaceInfo> current_faces_;
    std::vector<FaceInfo> previous_faces_;
    int next_tracking_id_{1};
    
    // Statistics
    std::atomic<std::uint64_t> total_frames_processed_{0};
    std::atomic<std::uint64_t> frames_with_faces_{0};
    std::atomic<std::uint64_t> total_faces_detected_{0};
    std::atomic<std::uint64_t> total_processing_time_ms_{0};
    
public:
    /**
     * @brief Constructor with configuration
     * @param config Face detection configuration
     */
    explicit FaceDetectionBias(const Config& config);
    
    /**
     * @brief Destructor
     */
    ~FaceDetectionBias() = default;
    
    // Core Face Detection
    
    /**
     * @brief Apply face bias to vision features
     * @param features Vision feature vector to enhance
     * @param frame Original image frame (if available)
     * @param grid_size Size of the vision grid (for feature mapping)
     * @return True if face bias was applied successfully
     */
    bool applyFaceBias(std::vector<float>& features, 
                      const std::vector<float>& gray_image = {},
                      int grid_size = 16);
    
#ifdef NF_HAVE_OPENCV
    /**
     * @brief Apply face bias using OpenCV Mat frame
     * @param features Vision feature vector to enhance
     * @param frame OpenCV Mat frame for face detection
     * @param grid_size Size of the vision grid
     * @return True if face bias was applied successfully
     */
    bool applyFaceBias(std::vector<float>& features, 
                      const cv::Mat& frame,
                      int grid_size = 16);
    
    /**
     * @brief Detect faces in OpenCV Mat frame
     * @param frame Input frame for face detection
     * @param faces Output vector of detected face rectangles
     * @return True if detection was performed (regardless of faces found)
     */
    bool detectFaces(const cv::Mat& frame, std::vector<cv::Rect>& faces);
    
    /**
     * @brief Detect faces with detailed information
     * @param frame Input frame for face detection
     * @param face_info Output vector of detailed face information
     * @return True if detection was performed
     */
    bool detectFacesDetailed(const cv::Mat& frame, std::vector<FaceInfo>& face_info);
#endif
    
    /**
     * @brief Detect faces in grayscale image vector
     * @param gray_image Grayscale image as vector (row-major order)
     * @param width Image width in pixels
     * @param height Image height in pixels
     * @param face_info Output vector of detected faces
     * @return True if detection was performed
     */
    bool detectFacesFromGray(const std::vector<float>& gray_image,
                            int width, int height,
                            std::vector<FaceInfo>& face_info);
    
    // Feature Enhancement
    
    /**
     * @brief Apply attention boost to face regions
     * @param features Feature vector to enhance
     * @param faces Detected faces to boost
     * @param grid_size Size of the vision grid
     */
    void applyAttentionBoost(std::vector<float>& features,
                           const std::vector<FaceInfo>& faces,
                           int grid_size);
    
    /**
     * @brief Apply background suppression to non-face regions
     * @param features Feature vector to modify
     * @param faces Detected faces (regions to preserve)
     * @param grid_size Size of the vision grid
     */
    void applyBackgroundSuppression(std::vector<float>& features,
                                  const std::vector<FaceInfo>& faces,
                                  int grid_size);
    
    /**
     * @brief Calculate attention weight for grid position
     * @param grid_x Grid X coordinate
     * @param grid_y Grid Y coordinate
     * @param faces Detected faces
     * @param grid_size Size of the vision grid
     * @return Attention weight [0,1+]
     */
    float calculateAttentionWeight(int grid_x, int grid_y,
                                 const std::vector<FaceInfo>& faces,
                                 int grid_size) const;
    
    // Face Tracking
    
    /**
     * @brief Update face tracking with new detections
     * @param new_faces Newly detected faces
     */
    void updateFaceTracking(std::vector<FaceInfo>& new_faces);
    
    /**
     * @brief Match faces between frames for tracking
     * @param current_faces Current frame faces
     * @param previous_faces Previous frame faces
     * @return Vector of matched pairs (current_idx, previous_idx)
     */
    std::vector<std::pair<int, int>> matchFaces(const std::vector<FaceInfo>& current_faces,
                                               const std::vector<FaceInfo>& previous_faces) const;
    
    /**
     * @brief Calculate overlap between two face rectangles
     * @param face1 First face
     * @param face2 Second face
     * @return Overlap ratio [0,1]
     */
    float calculateFaceOverlap(const FaceInfo& face1, const FaceInfo& face2) const;
    
    // Configuration and Statistics
    
    /**
     * @brief Set face priority multiplier
     * @param multiplier New priority multiplier (>= 1.0)
     */
    void setFacePriorityMultiplier(float multiplier);
    
    /**
     * @brief Get current face priority multiplier
     * @return Current multiplier value
     */
    float getFacePriorityMultiplier() const { return config_.face_priority_multiplier; }
    
    /**
     * @brief Update configuration
     * @param new_config New configuration parameters
     */
    void setConfig(const Config& new_config);
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    Config getConfig() const { return config_; }
    
    /**
     * @brief Get comprehensive statistics
     * @return Current face detection statistics
     */
    Statistics getStatistics() const;
    
    /**
     * @brief Get currently detected faces
     * @return Vector of current face detections
     */
    std::vector<FaceInfo> getCurrentFaces() const;
    
    /**
     * @brief Check if face detection is operational
     * @return True if system can detect faces
     */
    bool isOperational() const;
    
    /**
     * @brief Check if OpenCV is available
     * @return True if OpenCV support is compiled in
     */
    bool isOpenCVAvailable() const;
    
    /**
     * @brief Clear face tracking history
     */
    void clearTracking();
    
private:
    /**
     * @brief Initialize face detection cascade
     * @return True if cascade was loaded successfully
     */
    bool initializeCascade();
    
    /**
     * @brief Convert grid coordinates to image coordinates
     * @param grid_x Grid X coordinate
     * @param grid_y Grid Y coordinate
     * @param grid_size Size of the vision grid
     * @param image_width Image width in pixels
     * @param image_height Image height in pixels
     * @param img_x Output image X coordinate
     * @param img_y Output image Y coordinate
     */
    void gridToImageCoords(int grid_x, int grid_y, int grid_size,
                          int image_width, int image_height,
                          int& img_x, int& img_y) const;
    
    /**
     * @brief Convert image coordinates to grid coordinates
     * @param img_x Image X coordinate
     * @param img_y Image Y coordinate
     * @param image_width Image width in pixels
     * @param image_height Image height in pixels
     * @param grid_size Size of the vision grid
     * @param grid_x Output grid X coordinate
     * @param grid_y Output grid Y coordinate
     */
    void imageToGridCoords(int img_x, int img_y, int image_width, int image_height,
                          int grid_size, int& grid_x, int& grid_y) const;
    
    /**
     * @brief Calculate distance between two points
     * @param x1 First point X
     * @param y1 First point Y
     * @param x2 Second point X
     * @param y2 Second point Y
     * @return Euclidean distance
     */
    float calculateDistance(float x1, float y1, float x2, float y2) const;
    
    /**
     * @brief Update processing statistics
     * @param processing_time_ms Time taken for processing
     * @param faces_detected Number of faces detected
     */
    void updateStatistics(std::uint64_t processing_time_ms, size_t faces_detected);
    
    /**
     * @brief Get current time in milliseconds
     * @return Current time in milliseconds
     */
    std::uint64_t getCurrentTimeMs() const;
    
    /**
     * @brief Validate face detection parameters
     * @param grid_size Vision grid size
     * @return True if parameters are valid
     */
    bool validateParameters(int grid_size) const;
};

} // namespace Biases
} // namespace NeuroForge