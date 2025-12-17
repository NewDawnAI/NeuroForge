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
    
    struct Vec3f {
        float val[3];
        Vec3f() { val[0] = val[1] = val[2] = 0.0f; }
        Vec3f(float v0, float v1, float v2) { val[0] = v0; val[1] = v1; val[2] = v2; }
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
     * @brief ContrastEdgeBias implements center-surround receptive fields for edge detection
     * 
     * This bias module enhances visual processing by prioritizing high-contrast edges
     * and boundaries in the visual field. It uses center-surround receptive field
     * mechanisms similar to retinal ganglion cells to detect luminance and color
     * contrasts, making edges and boundaries more salient in the neural processing.
     */
    class ContrastEdgeBias {
    public:
        /**
         * @brief Configuration parameters for contrast edge detection
         */
        struct Config {
            float center_weight{1.0f};              // Weight for center region
            float surround_weight{-0.5f};           // Weight for surround region (typically negative)
            int center_radius{3};                   // Radius of center region in pixels
            int surround_radius{9};                 // Radius of surround region in pixels
            float contrast_threshold{0.1f};         // Minimum contrast for edge detection
            float edge_enhancement_factor{2.0f};    // Multiplier for edge responses
            bool enable_color_contrast{true};       // Enable color-based contrast detection
            bool enable_luminance_contrast{true};   // Enable luminance-based contrast detection
            float gaussian_sigma{1.5f};             // Sigma for Gaussian weighting
            int max_edge_responses{1000};           // Maximum number of edge responses to track
            float temporal_decay{0.95f};            // Decay factor for temporal integration
            bool normalize_responses{true};         // Whether to normalize edge responses
        };

        /**
         * @brief Edge response information
         */
        struct EdgeResponse {
            cv::Point2f location;                   // Location of edge
            float strength{0.0f};                   // Strength of edge response
            float orientation{0.0f};                // Orientation of edge (radians)
            float contrast_ratio{0.0f};             // Contrast ratio at this location
            cv::Vec3f color_gradient;               // Color gradient vector
            float temporal_persistence{0.0f};       // How long this edge has been present
        };

        /**
         * @brief Receptive field kernel for center-surround processing
         */
        struct ReceptiveField {
            cv::Mat center_kernel;                  // Center region kernel
            cv::Mat surround_kernel;                // Surround region kernel
            cv::Mat combined_kernel;                // Combined center-surround kernel
            int field_size;                         // Total size of receptive field
        };

        /**
         * @brief Constructor with configuration
         * @param config Configuration parameters
         */
        explicit ContrastEdgeBias(const Config& config);

        /**
         * @brief Destructor
         */
        ~ContrastEdgeBias() = default;

        /**
         * @brief Process visual input and detect edges/contrasts
         * @param input_image Input image for processing
         * @param feature_map Output feature map with edge enhancements
         * @param grid_size Size of the output grid
         */
        void processVisualInput(const cv::Mat& input_image, 
                              std::vector<float>& feature_map, 
                              int grid_size);

        /**
         * @brief Apply contrast edge bias to existing features
         * @param features Feature vector to modify
         * @param image_regions Corresponding image regions
         * @param grid_size Size of the feature grid
         */
        void applyContrastBias(std::vector<float>& features,
                             const cv::Mat& input_image,
                             int grid_size);

        /**
         * @brief Get detected edge responses
         * @return Vector of edge responses
         */
        std::vector<EdgeResponse> getEdgeResponses() const;

        /**
         * @brief Get contrast map for visualization
         * @param input_image Input image
         * @return Contrast map as CV_32F image
         */
        cv::Mat getContrastMap(const cv::Mat& input_image) const;

        /**
         * @brief Update temporal integration of edge responses
         * @param delta_time Time elapsed since last update
         */
        void updateTemporalIntegration(float delta_time);

        /**
         * @brief Reset edge detection state
         */
        void reset();

        /**
         * @brief Get configuration
         * @return Current configuration
         */
        const Config& getConfig() const { return config_; }

        /**
         * @brief Update configuration
         * @param config New configuration
         */
        void updateConfig(const Config& config);

    private:
        Config config_;                             // Configuration parameters
        std::vector<EdgeResponse> edge_responses_;  // Detected edge responses
        ReceptiveField receptive_field_;            // Center-surround receptive field
        cv::Mat previous_frame_;                    // Previous frame for temporal processing
        cv::Mat luminance_cache_;                   // Cached luminance image
        cv::Mat color_cache_;                       // Cached color processed image
        
        /**
         * @brief Initialize receptive field kernels
         */
        void initializeReceptiveField();

        /**
         * @brief Compute center-surround response
         * @param image Input image
         * @param x X coordinate
         * @param y Y coordinate
         * @return Center-surround response value
         */
        float computeCenterSurroundResponse(const cv::Mat& image, int x, int y) const;

        /**
         * @brief Detect edges using center-surround mechanism
         * @param image Input image
         * @return Edge response map
         */
        cv::Mat detectEdges(const cv::Mat& image) const;

        /**
         * @brief Compute luminance contrast
         * @param image Input image
         * @return Luminance contrast map
         */
        cv::Mat computeLuminanceContrast(const cv::Mat& image) const;

        /**
         * @brief Compute color contrast
         * @param image Input image
         * @return Color contrast map
         */
        cv::Mat computeColorContrast(const cv::Mat& image) const;

        /**
         * @brief Extract edge responses from contrast map
         * @param contrast_map Contrast map
         * @param orientation_map Orientation map
         */
        void extractEdgeResponses(const cv::Mat& contrast_map, const cv::Mat& orientation_map);

        /**
         * @brief Compute edge orientations
         * @param image Input image
         * @return Orientation map in radians
         */
        cv::Mat computeEdgeOrientations(const cv::Mat& image) const;

        /**
         * @brief Apply Gaussian weighting to receptive field
         * @param kernel Kernel to weight
         * @param sigma Gaussian sigma
         */
        void applyGaussianWeighting(cv::Mat& kernel, float sigma) const;

        /**
         * @brief Normalize feature map values
         * @param features Feature vector to normalize
         */
        void normalizeFeatures(std::vector<float>& features) const;
    };

} // namespace Biases
} // namespace NeuroForge