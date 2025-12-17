#pragma once

#include <vector>
#include <memory>
#include <cmath>

// Forward declarations for OpenCV types when not available
#ifndef NF_HAVE_OPENCV
namespace cv {
    struct Point2f {
        float x, y;
        Point2f() : x(0), y(0) {}
        Point2f(float x_, float y_) : x(x_), y(y_) {}
    };
    
    struct Rect {
        int x, y, width, height;
        Rect() : x(0), y(0), width(0), height(0) {}
        Rect(int x_, int y_, int w_, int h_) : x(x_), y(y_), width(w_), height(h_) {}
    };
    
    class Mat {
    public:
        Mat() = default;
        Mat(int rows, int cols, int type) {}
        bool empty() const { return true; }
        int rows = 0, cols = 0;
    };
}
#else
#include <opencv2/opencv.hpp>
#endif

namespace NeuroForge {
namespace Biases {

    /**
     * @brief Face Detection Bias - Implements innate preference for face-like patterns
     * 
     * This bias system provides the neural substrate with an innate tendency to
     * attend to and process face-like visual patterns, mimicking the biological
     * face detection mechanisms present from birth.
     */
    class FaceDetectionBias {
    public:
        /**
         * @brief Configuration parameters for face detection bias
         */
        struct Config {
            float face_template_weight{2.0f};       // Weight for face template matching
            float eye_region_weight{1.5f};          // Additional weight for eye regions
            float mouth_region_weight{1.2f};        // Additional weight for mouth region
            float symmetry_weight{1.3f};            // Weight for facial symmetry
            int min_face_size{24};                  // Minimum face size in pixels
            int max_face_size{300};                 // Maximum face size in pixels
            float detection_threshold{0.6f};        // Threshold for face detection
            bool enable_eye_detection{true};        // Enable specific eye detection
            bool enable_mouth_detection{true};      // Enable specific mouth detection
            float temporal_persistence{0.8f};       // Temporal integration factor
            int max_faces{10};                      // Maximum number of faces to track
            bool enable_face_tracking{true};        // Enable face tracking across frames
            bool enable_attention_boost{true};      // Enable attention boost for face regions
            float background_suppression{0.5f};     // Background suppression factor
        };

        /**
         * @brief Detected face information
         */
        struct FaceDetection {
            cv::Rect bounding_box;                  // Face bounding box
            cv::Point2f center;                     // Face center point
            float confidence{0.0f};                 // Detection confidence
            float symmetry_score{0.0f};             // Facial symmetry score
            std::vector<cv::Point2f> eye_positions; // Detected eye positions
            cv::Point2f mouth_position;             // Detected mouth position
            float temporal_stability{0.0f};         // Temporal stability score
            int tracking_id{-1};                    // Unique tracking identifier
        };

        /**
         * @brief Constructor
         * @param config Configuration parameters
         */
        explicit FaceDetectionBias(const Config& config = Config{});

        /**
         * @brief Destructor
         */
        ~FaceDetectionBias() = default;

        /**
         * @brief Process visual input for face detection
         * @param input_image Input image to process
         * @param feature_map Output feature map with face bias applied
         * @param grid_size Size of the feature grid
         */
        void processVisualInput(const cv::Mat& input_image, 
                              std::vector<float>& feature_map, 
                              int grid_size);

        /**
         * @brief Apply face detection bias to existing features
         * @param features Feature vector to modify
         * @param input_image Source image
         * @param grid_size Size of the feature grid
         */
        void applyFaceBias(std::vector<float>& features,
                          const cv::Mat& input_image,
                          int grid_size);

        /**
         * @brief Get detected faces
         * @return Vector of detected faces
         */
        std::vector<FaceDetection> getDetectedFaces() const;

        /**
         * @brief Get face saliency map
         * @param input_image Input image
         * @return Saliency map highlighting face regions
         */
        cv::Mat getFaceSaliencyMap(const cv::Mat& input_image) const;

        /**
         * @brief Update temporal integration
         * @param delta_time Time step for integration
         */
        void updateTemporalIntegration(float delta_time);

        /**
         * @brief Reset the bias system
         */
        void reset();

        /**
         * @brief Update configuration
         * @param config New configuration parameters
         */
        void updateConfig(const Config& config);

    private:
        Config config_;                             // Configuration parameters
        std::vector<FaceDetection> detected_faces_; // Currently detected faces
        cv::Mat face_template_;                     // Face template for matching
        cv::Mat previous_frame_;                    // Previous frame for temporal processing
        int next_tracking_id_;                      // Next available tracking ID

        /**
         * @brief Initialize face templates
         */
        void initializeFaceTemplates();

        /**
         * @brief Detect faces in image
         * @param image Input image
         * @return Vector of face detections
         */
        std::vector<FaceDetection> detectFaces(const cv::Mat& image) const;

        /**
         * @brief Compute face template response
         * @param image Input image
         * @param x X coordinate
         * @param y Y coordinate
         * @param scale Scale factor
         * @return Template response strength
         */
        float computeFaceTemplateResponse(const cv::Mat& image, int x, int y, float scale) const;

        /**
         * @brief Detect eye regions
         * @param face_region Face region to search
         * @return Vector of eye positions
         */
        std::vector<cv::Point2f> detectEyes(const cv::Mat& face_region) const;

        /**
         * @brief Detect mouth region
         * @param face_region Face region to search
         * @return Mouth position
         */
        cv::Point2f detectMouth(const cv::Mat& face_region) const;

        /**
         * @brief Compute facial symmetry score
         * @param face_region Face region to analyze
         * @return Symmetry score (0-1)
         */
        float computeSymmetryScore(const cv::Mat& face_region) const;

        /**
         * @brief Update face tracking
         * @param new_detections New face detections
         */
        void updateFaceTracking(const std::vector<FaceDetection>& new_detections);
    };

} // namespace Biases
} // namespace NeuroForge