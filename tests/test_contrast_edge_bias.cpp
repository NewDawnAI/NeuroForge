// Build this test only when GoogleTest is available; otherwise skip under minimal framework
#ifdef MINIMAL_TEST_FRAMEWORK
#include <iostream>

int main() {
    std::cout << "[SKIP] ContrastEdgeBias tests (requires GoogleTest and OpenCV)" << std::endl;
    return 0;
}

#else

#include <gtest/gtest.h>
#include <opencv2/opencv.hpp>
#include "biases/ContrastEdgeBias.h"

using namespace NeuroForge::Biases;

class ContrastEdgeBiasTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create default configuration
        config_.center_radius = 3.0f;
        config_.surround_radius = 8.0f;
        config_.center_weight = 1.0f;
        config_.surround_weight = -0.5f;
        config_.contrast_threshold = 0.1f;
        config_.edge_enhancement_factor = 2.0f;
        config_.gaussian_sigma = 1.5f;
        config_.temporal_decay = 0.95f;
        config_.max_edge_responses = 100;
        config_.enable_luminance_contrast = true;
        config_.enable_color_contrast = true;
        config_.normalize_responses = true;
        
        bias_ = std::make_unique<ContrastEdgeBias>(config_);
        
        // Create test images
        createTestImages();
    }
    
    void createTestImages() {
        // Create a simple test image with edges
        test_image_ = cv::Mat::zeros(100, 100, CV_8UC3);
        
        // Add a vertical edge
        cv::rectangle(test_image_, cv::Rect(40, 20, 20, 60), cv::Scalar(255, 255, 255), -1);
        
        // Add a horizontal edge
        cv::rectangle(test_image_, cv::Rect(20, 40, 60, 20), cv::Scalar(128, 128, 128), -1);
        
        // Create a gradient image
        gradient_image_ = cv::Mat::zeros(100, 100, CV_8UC3);
        for (int y = 0; y < gradient_image_.rows; ++y) {
            for (int x = 0; x < gradient_image_.cols; ++x) {
                int intensity = static_cast<int>(255.0 * x / gradient_image_.cols);
                gradient_image_.at<cv::Vec3b>(y, x) = cv::Vec3b(intensity, intensity, intensity);
            }
        }
        
        // Create a uniform image (no edges)
        uniform_image_ = cv::Mat::ones(100, 100, CV_8UC3) * 128;
    }
    
    ContrastEdgeBias::Config config_;
    std::unique_ptr<ContrastEdgeBias> bias_;
    cv::Mat test_image_;
    cv::Mat gradient_image_;
    cv::Mat uniform_image_;
};

TEST_F(ContrastEdgeBiasTest, ConstructorInitialization) {
    EXPECT_NO_THROW({
        ContrastEdgeBias bias(config_);
    });
}

TEST_F(ContrastEdgeBiasTest, ProcessVisualInputBasic) {
    std::vector<float> feature_map;
    int grid_size = 10;
    
    EXPECT_NO_THROW({
        bias_->processVisualInput(test_image_, feature_map, grid_size);
    });
    
    EXPECT_EQ(feature_map.size(), static_cast<size_t>(grid_size * grid_size));
    
    // Check that some features are non-zero (edges detected)
    bool has_non_zero = std::any_of(feature_map.begin(), feature_map.end(),
                                   [](float val) { return val > 0.0f; });
    EXPECT_TRUE(has_non_zero);
}

TEST_F(ContrastEdgeBiasTest, ProcessEmptyImage) {
    std::vector<float> feature_map;
    cv::Mat empty_image;
    int grid_size = 10;
    
    EXPECT_NO_THROW({
        bias_->processVisualInput(empty_image, feature_map, grid_size);
    });
    
    EXPECT_EQ(feature_map.size(), static_cast<size_t>(grid_size * grid_size));
    
    // All features should be zero for empty image
    bool all_zero = std::all_of(feature_map.begin(), feature_map.end(),
                               [](float val) { return val == 0.0f; });
    EXPECT_TRUE(all_zero);
}

TEST_F(ContrastEdgeBiasTest, EdgeDetection) {
    cv::Mat edge_map = bias_->detectEdges(test_image_);
    
    EXPECT_FALSE(edge_map.empty());
    EXPECT_EQ(edge_map.type(), CV_32F);
    EXPECT_EQ(edge_map.size(), test_image_.size());
    
    // Check that edges are detected (some non-zero values)
    double min_val, max_val;
    cv::minMaxLoc(edge_map, &min_val, &max_val);
    EXPECT_GT(max_val, 0.0);
}

TEST_F(ContrastEdgeBiasTest, ContrastMapComputation) {
    cv::Mat contrast_map = bias_->getContrastMap(test_image_);
    
    EXPECT_FALSE(contrast_map.empty());
    EXPECT_EQ(contrast_map.type(), CV_32F);
    EXPECT_EQ(contrast_map.size(), test_image_.size());
    
    // Contrast map should have some variation
    cv::Scalar mean, stddev;
    cv::meanStdDev(contrast_map, mean, stddev);
    EXPECT_GT(stddev[0], 0.0);
}

TEST_F(ContrastEdgeBiasTest, LuminanceContrast) {
    cv::Mat luminance_contrast = bias_->computeLuminanceContrast(gradient_image_);
    
    EXPECT_FALSE(luminance_contrast.empty());
    EXPECT_EQ(luminance_contrast.type(), CV_32F);
    
    // Gradient image should produce contrast
    double min_val, max_val;
    cv::minMaxLoc(luminance_contrast, &min_val, &max_val);
    EXPECT_GT(max_val - min_val, 0.0);
}

TEST_F(ContrastEdgeBiasTest, ColorContrast) {
    // Create a color test image
    cv::Mat color_image = cv::Mat::zeros(50, 50, CV_8UC3);
    cv::rectangle(color_image, cv::Rect(10, 10, 30, 30), cv::Scalar(255, 0, 0), -1); // Red square
    
    cv::Mat color_contrast = bias_->computeColorContrast(color_image);
    
    EXPECT_FALSE(color_contrast.empty());
    EXPECT_EQ(color_contrast.type(), CV_32F);
    
    // Color boundaries should produce contrast
    double min_val, max_val;
    cv::minMaxLoc(color_contrast, &min_val, &max_val);
    EXPECT_GT(max_val, 0.0);
}

TEST_F(ContrastEdgeBiasTest, EdgeOrientationComputation) {
    cv::Mat orientation_map = bias_->computeEdgeOrientations(test_image_);
    
    EXPECT_FALSE(orientation_map.empty());
    EXPECT_EQ(orientation_map.type(), CV_32F);
    EXPECT_EQ(orientation_map.size(), test_image_.size());
    
    // Orientation values should be in valid range [0, 2π]
    double min_val, max_val;
    cv::minMaxLoc(orientation_map, &min_val, &max_val);
    EXPECT_GE(min_val, 0.0);
    EXPECT_LE(max_val, 2 * M_PI);
}

TEST_F(ContrastEdgeBiasTest, ApplyContrastBias) {
    std::vector<float> features(100, 0.5f); // Initialize with some base values
    int grid_size = 10;
    
    EXPECT_NO_THROW({
        bias_->applyContrastBias(features, test_image_, grid_size);
    });
    
    // Features should be modified (some should be enhanced)
    bool has_enhanced = std::any_of(features.begin(), features.end(),
                                   [](float val) { return val > 0.5f; });
    EXPECT_TRUE(has_enhanced);
}

TEST_F(ContrastEdgeBiasTest, EdgeResponseExtraction) {
    std::vector<float> feature_map;
    bias_->processVisualInput(test_image_, feature_map, 10);
    
    auto edge_responses = bias_->getEdgeResponses();
    
    // Should detect some edges
    EXPECT_GT(edge_responses.size(), 0);
    
    // Check edge response properties
    for (const auto& edge : edge_responses) {
        EXPECT_GE(edge.strength, 0.0f);
        EXPECT_GE(edge.orientation, 0.0f);
        EXPECT_LE(edge.orientation, 2 * M_PI);
        EXPECT_GE(edge.contrast_ratio, 0.0f);
        EXPECT_GE(edge.temporal_persistence, 0.0f);
        EXPECT_LE(edge.temporal_persistence, 1.0f);
    }
}

TEST_F(ContrastEdgeBiasTest, TemporalIntegration) {
    std::vector<float> feature_map;
    bias_->processVisualInput(test_image_, feature_map, 10);
    
    auto initial_responses = bias_->getEdgeResponses();
    size_t initial_count = initial_responses.size();
    
    // Update temporal integration (simulate time passing)
    bias_->updateTemporalIntegration(0.1f);
    
    auto updated_responses = bias_->getEdgeResponses();
    
    // Responses should still exist but with reduced persistence
    EXPECT_LE(updated_responses.size(), initial_count);
    
    for (const auto& edge : updated_responses) {
        EXPECT_LT(edge.temporal_persistence, 1.0f);
    }
}

TEST_F(ContrastEdgeBiasTest, ConfigurationUpdate) {
    ContrastEdgeBias::Config new_config = config_;
    new_config.contrast_threshold = 0.5f;
    new_config.edge_enhancement_factor = 5.0f;
    
    EXPECT_NO_THROW({
        bias_->updateConfig(new_config);
    });
    
    // Process with new configuration
    std::vector<float> feature_map;
    bias_->processVisualInput(test_image_, feature_map, 10);
    
    // Should still work with new configuration
    EXPECT_EQ(feature_map.size(), 100);
}

TEST_F(ContrastEdgeBiasTest, Reset) {
    // Process some input first
    std::vector<float> feature_map;
    bias_->processVisualInput(test_image_, feature_map, 10);
    
    EXPECT_GT(bias_->getEdgeResponses().size(), 0);
    
    // Reset the bias
    bias_->reset();
    
    // Edge responses should be cleared
    EXPECT_EQ(bias_->getEdgeResponses().size(), 0);
}

TEST_F(ContrastEdgeBiasTest, CenterSurroundResponse) {
    cv::Mat gray_image;
    cv::cvtColor(test_image_, gray_image, cv::COLOR_BGR2GRAY);
    gray_image.convertTo(gray_image, CV_32F, 1.0/255.0);
    
    // Test center-surround response at various locations
    float response1 = bias_->computeCenterSurroundResponse(gray_image, 50, 50);
    float response2 = bias_->computeCenterSurroundResponse(gray_image, 10, 10);
    
    // Responses should be finite
    EXPECT_TRUE(std::isfinite(response1));
    EXPECT_TRUE(std::isfinite(response2));
}

TEST_F(ContrastEdgeBiasTest, UniformImageProcessing) {
    std::vector<float> feature_map;
    bias_->processVisualInput(uniform_image_, feature_map, 10);
    
    EXPECT_EQ(feature_map.size(), 100);
    
    // Uniform image should produce minimal edge responses
    auto edge_responses = bias_->getEdgeResponses();
    EXPECT_LE(edge_responses.size(), 5); // Very few or no edges
}

TEST_F(ContrastEdgeBiasTest, GrayscaleImageProcessing) {
    cv::Mat gray_image;
    cv::cvtColor(test_image_, gray_image, cv::COLOR_BGR2GRAY);
    
    std::vector<float> feature_map;
    EXPECT_NO_THROW({
        bias_->processVisualInput(gray_image, feature_map, 10);
    });
    
    EXPECT_EQ(feature_map.size(), 100);
    
    // Should still detect edges in grayscale
    bool has_non_zero = std::any_of(feature_map.begin(), feature_map.end(),
                                   [](float val) { return val > 0.0f; });
    EXPECT_TRUE(has_non_zero);
}

TEST_F(ContrastEdgeBiasTest, LargeImageProcessing) {
    // Create a larger test image
    cv::Mat large_image = cv::Mat::zeros(500, 500, CV_8UC3);
    cv::rectangle(large_image, cv::Rect(200, 200, 100, 100), cv::Scalar(255, 255, 255), -1);
    
    std::vector<float> feature_map;
    EXPECT_NO_THROW({
        bias_->processVisualInput(large_image, feature_map, 20);
    });
    
    EXPECT_EQ(feature_map.size(), 400);
    
    // Should handle large images without issues
    bool has_non_zero = std::any_of(feature_map.begin(), feature_map.end(),
                                   [](float val) { return val > 0.0f; });
    EXPECT_TRUE(has_non_zero);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

#endif // MINIMAL_TEST_FRAMEWORK