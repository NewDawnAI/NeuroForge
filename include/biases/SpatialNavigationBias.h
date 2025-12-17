#pragma once

#include "core/Types.h"
#include <vector>
#include <memory>
#include <chrono>

namespace NeuroForge {
namespace Biases {

/**
 * @brief Spatial navigation bias configuration
 */
struct SpatialNavigationConfig {
    float landmark_weight = 2.0f;
    float path_integration_weight = 1.5f;
    float boundary_detection_weight = 1.8f;
    float grid_cell_spacing = 0.5f;
    bool enable_place_cells = true;
    bool enable_head_direction_cells = true;
    float spatial_memory_decay = 0.01f;
};

/**
 * @brief Spatial landmark representation
 */
struct SpatialLandmark {
    float x, y;
    float salience = 1.0f;
    std::string landmark_type;
    std::chrono::steady_clock::time_point last_seen;
};

/**
 * @brief Spatial navigation bias for enhanced spatial processing
 */
class SpatialNavigationBias {
public:
    explicit SpatialNavigationBias(const SpatialNavigationConfig& config = SpatialNavigationConfig{});
    ~SpatialNavigationBias() = default;

    // Spatial processing
    void processSpatialInput(const std::vector<float>& spatial_features,
                           std::vector<float>& enhanced_features);
    
    // Landmark management
    void addLandmark(float x, float y, const std::string& type, float salience = 1.0f);
    void updateLandmarks(const std::vector<SpatialLandmark>& detected_landmarks);
    std::vector<SpatialLandmark> getNearbyLandmarks(float x, float y, float radius) const;
    
    // Navigation assistance
    std::vector<float> computeNavigationBias(float current_x, float current_y, 
                                           float target_x, float target_y) const;
    void updatePosition(float x, float y, float heading);
    
    // Configuration
    void updateConfig(const SpatialNavigationConfig& config) { config_ = config; }
    const SpatialNavigationConfig& getConfig() const { return config_; }

private:
    SpatialNavigationConfig config_;
    std::vector<SpatialLandmark> landmarks_;
    float current_x_ = 0.0f;
    float current_y_ = 0.0f;
    float current_heading_ = 0.0f;
};

} // namespace Biases
} // namespace NeuroForge