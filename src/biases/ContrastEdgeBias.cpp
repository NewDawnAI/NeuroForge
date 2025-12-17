#include "ContrastEdgeBias.h"
#include <algorithm>
#include <cmath>
#include <numeric>

#ifdef NF_HAVE_OPENCV

namespace NeuroForge {
namespace Biases {

ContrastEdgeBias::ContrastEdgeBias(const Config& config) : config_(config) {
    initializeReceptiveField();
    edge_responses_.reserve(config_.max_edge_responses);
}

void ContrastEdgeBias::initializeReceptiveField() {
    int field_size = config_.surround_radius * 2 + 1;
    receptive_field_.field_size = field_size;
    
    // Create center kernel
    receptive_field_.center_kernel = cv::Mat::zeros(field_size, field_size, CV_32F);
    cv::Point2f center(config_.surround_radius, config_.surround_radius);
    
    for (int y = 0; y < field_size; ++y) {
        for (int x = 0; x < field_size; ++x) {
            float distance = cv::norm(cv::Point2f(x, y) - center);
            if (distance <= config_.center_radius) {
                receptive_field_.center_kernel.at<float>(y, x) = config_.center_weight;
            }
        }
    }
    
    // Create surround kernel
    receptive_field_.surround_kernel = cv::Mat::zeros(field_size, field_size, CV_32F);
    for (int y = 0; y < field_size; ++y) {
        for (int x = 0; x < field_size; ++x) {
            float distance = cv::norm(cv::Point2f(x, y) - center);
            if (distance > config_.center_radius && distance <= config_.surround_radius) {
                receptive_field_.surround_kernel.at<float>(y, x) = config_.surround_weight;
            }
        }
    }
    
    // Create combined kernel
    receptive_field_.combined_kernel = receptive_field_.center_kernel + receptive_field_.surround_kernel;
    
    // Apply Gaussian weighting if enabled
    if (config_.gaussian_sigma > 0.0f) {
        applyGaussianWeighting(receptive_field_.combined_kernel, config_.gaussian_sigma);
    }
}

void ContrastEdgeBias::applyGaussianWeighting(cv::Mat& kernel, float sigma) const {
    cv::Point2f center(kernel.cols / 2.0f, kernel.rows / 2.0f);
    
    for (int y = 0; y < kernel.rows; ++y) {
        for (int x = 0; x < kernel.cols; ++x) {
            float distance_sq = std::pow(x - center.x, 2) + std::pow(y - center.y, 2);
            float gaussian_weight = std::exp(-distance_sq / (2.0f * sigma * sigma));
            kernel.at<float>(y, x) *= gaussian_weight;
        }
    }
}

void ContrastEdgeBias::processVisualInput(const cv::Mat& input_image, 
                                        std::vector<float>& feature_map, 
                                        int grid_size) {
    if (input_image.empty()) {
        return;
    }
    
    // Ensure feature map is properly sized
    feature_map.resize(grid_size * grid_size, 0.0f);
    
    // Detect edges using center-surround mechanism
    cv::Mat edge_map = detectEdges(input_image);
    cv::Mat orientation_map = computeEdgeOrientations(input_image);
    
    // Extract edge responses
    extractEdgeResponses(edge_map, orientation_map);
    
    // Map edge responses to feature grid
    float grid_scale_x = static_cast<float>(input_image.cols) / grid_size;
    float grid_scale_y = static_cast<float>(input_image.rows) / grid_size;
    
    for (const auto& edge : edge_responses_) {
        int grid_x = static_cast<int>(edge.location.x / grid_scale_x);
        int grid_y = static_cast<int>(edge.location.y / grid_scale_y);
        
        grid_x = std::max(0, std::min(grid_size - 1, grid_x));
        grid_y = std::max(0, std::min(grid_size - 1, grid_y));
        
        int idx = grid_y * grid_size + grid_x;
        feature_map[idx] += edge.strength * config_.edge_enhancement_factor;
    }
    
    // Normalize if requested
    if (config_.normalize_responses) {
        normalizeFeatures(feature_map);
    }
    
    // Store current frame for temporal processing
    input_image.copyTo(previous_frame_);
}

void ContrastEdgeBias::applyContrastBias(std::vector<float>& features,
                                       const cv::Mat& input_image,
                                       int grid_size) {
    if (input_image.empty() || features.size() != static_cast<size_t>(grid_size * grid_size)) {
        return;
    }
    
    // Get contrast map
    cv::Mat contrast_map = getContrastMap(input_image);
    
    // Resize contrast map to match grid size
    cv::Mat resized_contrast;
    cv::resize(contrast_map, resized_contrast, cv::Size(grid_size, grid_size));
    
    // Apply contrast enhancement to features
    for (int y = 0; y < grid_size; ++y) {
        for (int x = 0; x < grid_size; ++x) {
            int idx = y * grid_size + x;
            float contrast_value = resized_contrast.at<float>(y, x);
            
            // Enhance features based on local contrast
            if (contrast_value > config_.contrast_threshold) {
                features[idx] += contrast_value * config_.edge_enhancement_factor;
            }
        }
    }
}

cv::Mat ContrastEdgeBias::detectEdges(const cv::Mat& image) const {
    cv::Mat gray_image;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray_image, cv::COLOR_BGR2GRAY);
    } else {
        gray_image = image.clone();
    }
    
    // Convert to float for processing
    cv::Mat float_image;
    gray_image.convertTo(float_image, CV_32F, 1.0/255.0);
    
    // Apply center-surround filter
    cv::Mat edge_response;
    cv::filter2D(float_image, edge_response, CV_32F, receptive_field_.combined_kernel);
    
    // Take absolute value to get edge strength
    cv::Mat abs_response;
    cv::absdiff(edge_response, cv::Scalar(0), abs_response);
    
    return abs_response;
}

cv::Mat ContrastEdgeBias::computeLuminanceContrast(const cv::Mat& image) const {
    cv::Mat gray_image;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray_image, cv::COLOR_BGR2GRAY);
    } else {
        gray_image = image.clone();
    }
    
    cv::Mat float_image;
    gray_image.convertTo(float_image, CV_32F, 1.0/255.0);
    
    // Compute local mean using box filter
    cv::Mat local_mean;
    cv::boxFilter(float_image, local_mean, CV_32F, 
                  cv::Size(config_.surround_radius * 2 + 1, config_.surround_radius * 2 + 1));
    
    // Compute contrast as (I - mean) / (I + mean + epsilon)
    cv::Mat contrast;
    cv::Mat denominator = float_image + local_mean + 1e-6f;
    cv::Mat numerator = float_image - local_mean;
    cv::divide(numerator, denominator, contrast);
    
    return contrast;
}

cv::Mat ContrastEdgeBias::computeColorContrast(const cv::Mat& image) const {
    if (image.channels() != 3) {
        return cv::Mat::zeros(image.size(), CV_32F);
    }
    
    // Convert to Lab color space for perceptual uniformity
    cv::Mat lab_image;
    cv::cvtColor(image, lab_image, cv::COLOR_BGR2Lab);
    
    std::vector<cv::Mat> lab_channels;
    cv::split(lab_image, lab_channels);
    
    cv::Mat color_contrast = cv::Mat::zeros(image.size(), CV_32F);
    
    // Compute contrast for each channel
    for (int c = 0; c < 3; ++c) {
        cv::Mat float_channel;
        lab_channels[c].convertTo(float_channel, CV_32F, 1.0/255.0);
        
        cv::Mat local_mean;
        cv::boxFilter(float_channel, local_mean, CV_32F,
                      cv::Size(config_.surround_radius * 2 + 1, config_.surround_radius * 2 + 1));
        
        cv::Mat channel_contrast;
        cv::Mat denominator = float_channel + local_mean + 1e-6f;
        cv::Mat numerator = float_channel - local_mean;
        cv::divide(numerator, denominator, channel_contrast);
        
        // Accumulate contrast across channels
        color_contrast += cv::abs(channel_contrast);
    }
    
    return color_contrast / 3.0f; // Average across channels
}

cv::Mat ContrastEdgeBias::getContrastMap(const cv::Mat& input_image) const {
    cv::Mat luminance_contrast, color_contrast;
    
    if (config_.enable_luminance_contrast) {
        luminance_contrast = computeLuminanceContrast(input_image);
    } else {
        luminance_contrast = cv::Mat::zeros(input_image.size(), CV_32F);
    }
    
    if (config_.enable_color_contrast) {
        color_contrast = computeColorContrast(input_image);
    } else {
        color_contrast = cv::Mat::zeros(input_image.size(), CV_32F);
    }
    
    // Combine luminance and color contrast
    cv::Mat combined_contrast = luminance_contrast + color_contrast;
    
    return combined_contrast;
}

cv::Mat ContrastEdgeBias::computeEdgeOrientations(const cv::Mat& image) const {
    cv::Mat gray_image;
    if (image.channels() == 3) {
        cv::cvtColor(image, gray_image, cv::COLOR_BGR2GRAY);
    } else {
        gray_image = image.clone();
    }
    
    // Compute gradients
    cv::Mat grad_x, grad_y;
    cv::Sobel(gray_image, grad_x, CV_32F, 1, 0, 3);
    cv::Sobel(gray_image, grad_y, CV_32F, 0, 1, 3);
    
    // Compute orientation
    cv::Mat orientation;
    cv::phase(grad_x, grad_y, orientation, false); // Result in radians
    
    return orientation;
}

void ContrastEdgeBias::extractEdgeResponses(const cv::Mat& contrast_map, const cv::Mat& orientation_map) {
    edge_responses_.clear();
    
    // Find local maxima in contrast map
    for (int y = 1; y < contrast_map.rows - 1; ++y) {
        for (int x = 1; x < contrast_map.cols - 1; ++x) {
            float center_value = contrast_map.at<float>(y, x);
            
            if (center_value > config_.contrast_threshold) {
                // Check if it's a local maximum
                bool is_maximum = true;
                for (int dy = -1; dy <= 1 && is_maximum; ++dy) {
                    for (int dx = -1; dx <= 1 && is_maximum; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        if (contrast_map.at<float>(y + dy, x + dx) >= center_value) {
                            is_maximum = false;
                        }
                    }
                }
                
                if (is_maximum && edge_responses_.size() < static_cast<size_t>(config_.max_edge_responses)) {
                    EdgeResponse response;
                    response.location = cv::Point2f(x, y);
                    response.strength = center_value;
                    response.orientation = orientation_map.at<float>(y, x);
                    response.contrast_ratio = center_value;
                    response.temporal_persistence = 1.0f;
                    
                    edge_responses_.push_back(response);
                }
            }
        }
    }
    
    // Sort by strength (strongest first)
    std::sort(edge_responses_.begin(), edge_responses_.end(),
              [](const EdgeResponse& a, const EdgeResponse& b) {
                  return a.strength > b.strength;
              });
}

void ContrastEdgeBias::updateTemporalIntegration(float delta_time) {
    // Decay temporal persistence of existing edge responses
    for (auto& edge : edge_responses_) {
        edge.temporal_persistence *= config_.temporal_decay;
    }
    
    // Remove edges with very low persistence
    edge_responses_.erase(
        std::remove_if(edge_responses_.begin(), edge_responses_.end(),
                      [](const EdgeResponse& edge) {
                          return edge.temporal_persistence < 0.1f;
                      }),
        edge_responses_.end());
}

std::vector<ContrastEdgeBias::EdgeResponse> ContrastEdgeBias::getEdgeResponses() const {
    return edge_responses_;
}

void ContrastEdgeBias::reset() {
    edge_responses_.clear();
    previous_frame_ = cv::Mat();
    luminance_cache_ = cv::Mat();
    color_cache_ = cv::Mat();
}

void ContrastEdgeBias::updateConfig(const Config& config) {
    config_ = config;
    initializeReceptiveField();
    edge_responses_.reserve(config_.max_edge_responses);
}

void ContrastEdgeBias::normalizeFeatures(std::vector<float>& features) const {
    if (features.empty()) return;
    
    // Find min and max values
    auto minmax = std::minmax_element(features.begin(), features.end());
    float min_val = *minmax.first;
    float max_val = *minmax.second;
    
    // Normalize to [0, 1] range
    if (max_val > min_val) {
        float range = max_val - min_val;
        for (float& feature : features) {
            feature = (feature - min_val) / range;
        }
    }
}

float ContrastEdgeBias::computeCenterSurroundResponse(const cv::Mat& image, int x, int y) const {
    if (x < receptive_field_.field_size / 2 || y < receptive_field_.field_size / 2 ||
        x >= image.cols - receptive_field_.field_size / 2 || 
        y >= image.rows - receptive_field_.field_size / 2) {
        return 0.0f;
    }
    
    // Extract region of interest
    cv::Rect roi(x - receptive_field_.field_size / 2, y - receptive_field_.field_size / 2,
                 receptive_field_.field_size, receptive_field_.field_size);
    cv::Mat region = image(roi);
    
    // Convert to float if necessary
    cv::Mat float_region;
    if (region.type() != CV_32F) {
        region.convertTo(float_region, CV_32F, 1.0/255.0);
    } else {
        float_region = region;
    }
    
    // Compute weighted sum
    cv::Mat response;
    cv::multiply(float_region, receptive_field_.combined_kernel, response);
    
    return cv::sum(response)[0];
}

} // namespace Biases
} // namespace NeuroForge

#endif // NF_HAVE_OPENCV