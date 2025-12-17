#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <deque>
#include <array>

namespace NeuroForge {
namespace Biases {

/**
 * @brief SpatialNavigationBias implements innate spatial reasoning and navigation
 * 
 * This class provides biological spatial navigation capabilities including:
 * - Place cell and grid cell simulation for spatial mapping
 * - Path planning and route optimization
 * - Landmark recognition and spatial memory
 * - Boundary detection and obstacle avoidance
 * - Spatial orientation and heading direction
 * - Distance estimation and spatial scale
 * - Cognitive mapping and spatial learning
 */
class SpatialNavigationBias {
public:
    struct Config {
        // Grid cell parameters
        float grid_scale = 50.0f;                  // Grid cell spacing (cm)
        int grid_orientation_count = 6;            // Number of grid orientations
        float grid_field_size = 30.0f;            // Grid field diameter (cm)
        float grid_spacing_ratio = 1.4f;           // Spacing ratio between scales
        
        // Place cell parameters
        float place_field_size = 40.0f;           // Place field diameter (cm)
        int max_place_cells = 1000;               // Maximum place cells
        float place_cell_threshold = 0.7f;        // Place cell activation threshold
        float place_field_overlap = 0.3f;         // Allowed field overlap
        
        // Head direction parameters
        int head_direction_cells = 60;             // Number of HD cells (6° resolution)
        float hd_cell_width = 15.0f;              // HD cell tuning width (degrees)
        float angular_velocity_gain = 1.0f;       // Angular velocity integration gain
        
        // Border/boundary detection
        float boundary_detection_range = 200.0f;  // Boundary detection range (cm)
        float wall_cell_threshold = 0.8f;         // Wall cell activation threshold
        int boundary_cell_count = 100;            // Number of boundary cells
        
        // Path planning parameters
        float path_planning_resolution = 10.0f;   // Path planning grid resolution (cm)
        float obstacle_avoidance_margin = 30.0f;  // Safety margin around obstacles
        int max_path_length = 500;                // Maximum path waypoints
        float path_smoothing_factor = 0.7f;       // Path smoothing strength
        
        // Spatial memory parameters
        float spatial_memory_decay = 0.99f;       // Memory decay rate per update
        float landmark_salience_threshold = 0.6f; // Landmark importance threshold
        int max_landmarks = 50;                    // Maximum tracked landmarks
        float landmark_recognition_radius = 100.0f; // Landmark recognition range
        
        // Navigation behavior parameters
        float exploration_bias = 0.3f;            // Tendency to explore new areas
        float goal_attraction_strength = 2.0f;    // Goal-directed navigation strength
        float path_following_precision = 15.0f;   // Path following tolerance (cm)
        float shortcut_detection_angle = 45.0f;   // Shortcut detection cone (degrees)
        
        // Distance and scale estimation
        float distance_estimation_accuracy = 0.85f; // Distance estimation accuracy
        float scale_invariance_range = 10.0f;     // Scale invariance factor
        bool enable_metric_estimation = true;     // Enable metric distance estimation
        bool enable_topological_mapping = true;   // Enable topological relationships
        
        // Learning parameters
        float spatial_learning_rate = 0.1f;       // Spatial learning rate
        float place_cell_plasticity = 0.05f;      // Place cell adaptation rate
        float grid_cell_adaptation = 0.02f;       // Grid cell adaptation rate
        int spatial_memory_consolidation_time = 300; // Memory consolidation time (s)
        
        // Advanced features
        bool enable_cognitive_mapping = true;     // Enable cognitive map formation
        bool enable_vector_navigation = true;     // Enable vector-based navigation
        bool enable_route_learning = true;        // Enable route memorization
        bool enable_spatial_inference = true;     // Enable spatial reasoning
    };
    
    struct SpatialLocation {
        float x, y;                               // Cartesian coordinates (cm)
        float confidence;                         // Location confidence
        std::uint64_t timestamp_ms;              // Timestamp
        
        SpatialLocation() : x(0), y(0), confidence(0), timestamp_ms(0) {}
        SpatialLocation(float x_, float y_, float conf, std::uint64_t ts)
            : x(x_), y(y_), confidence(conf), timestamp_ms(ts) {}
    };
    
    struct GridCell {
        float scale;                              // Grid scale
        float orientation;                        // Grid orientation (radians)
        float phase_x, phase_y;                   // Grid phase offsets
        float activation;                         // Current activation level
        std::vector<std::vector<float>> firing_map; // Spatial firing pattern
    };
    
    struct PlaceCell {
        SpatialLocation center;                   // Place field center
        float field_size;                         // Place field diameter
        float peak_rate;                          // Maximum firing rate
        float activation;                         // Current activation
        std::vector<int> connected_landmarks;     // Associated landmarks
        bool is_active;                           // Active status
    };
    
    struct HeadDirectionCell {
        float preferred_direction;                // Preferred direction (radians)
        float tuning_width;                       // Tuning curve width
        float activation;                         // Current activation
        float angular_velocity_input;             // Angular velocity integration
    };
    
    struct BoundaryCell {
        float preferred_distance;                 // Preferred distance to boundary
        float preferred_direction;                // Preferred boundary direction
        float activation;                         // Current activation
        std::string boundary_type;                // Wall, edge, corner, etc.
    };
    
    struct Landmark {
        int landmark_id;                          // Unique identifier
        SpatialLocation location;                 // Landmark position
        std::vector<float> visual_features;       // Visual feature vector
        float salience;                           // Landmark importance
        float recognition_confidence;             // Recognition confidence
        std::string landmark_type;                // Landmark category
        bool is_reliable;                         // Reliability flag
    };
    
    struct SpatialPath {
        std::vector<SpatialLocation> waypoints;   // Path waypoints
        float total_distance;                     // Total path length
        float estimated_time;                     // Estimated travel time
        float path_confidence;                    // Path reliability
        std::vector<int> landmark_sequence;       // Landmark-based route
        bool is_optimal;                          // Optimality flag
    };
    
    struct CognitiveMap {
        std::vector<PlaceCell> place_cells;       // Place cell population
        std::vector<GridCell> grid_cells;         // Grid cell population
        std::vector<HeadDirectionCell> hd_cells;  // Head direction cells
        std::vector<BoundaryCell> boundary_cells; // Boundary cells
        std::vector<Landmark> landmarks;          // Landmark database
        std::unordered_map<int, std::vector<int>> connectivity; // Spatial connectivity
        float map_coherence;                      // Map consistency measure
    };

private:
    Config config_;
    std::mutex bias_mutex_;
    
    // Core spatial representations
    CognitiveMap cognitive_map_;
    SpatialLocation current_location_;
    float current_heading_;
    
    // Navigation state
    SpatialPath current_path_;
    SpatialLocation goal_location_;
    bool is_navigating_;
    
    // Spatial memory and learning
    std::deque<SpatialLocation> location_history_;
    std::unordered_map<int, float> place_cell_activities_;
    std::vector<float> grid_cell_activities_;
    std::vector<float> hd_cell_activities_;
    
    // Internal methods
    void updateGridCells(const SpatialLocation& location);
    void updatePlaceCells(const SpatialLocation& location);
    void updateHeadDirectionCells(float heading, float angular_velocity);
    void updateBoundaryCells(const std::vector<float>& boundary_distances);
    void updateLandmarks(const std::vector<Landmark>& detected_landmarks);
    
    float calculateGridCellActivation(const GridCell& cell, const SpatialLocation& location);
    float calculatePlaceCellActivation(const PlaceCell& cell, const SpatialLocation& location) const;
    float calculateHeadDirectionActivation(const HeadDirectionCell& cell, float current_heading);
    
    void formCognitiveMap();
    void consolidateSpatialMemory();
    SpatialPath planPath(const SpatialLocation& start, const SpatialLocation& goal);
    std::vector<SpatialLocation> optimizePath(const std::vector<SpatialLocation>& raw_path);
    
public:
    explicit SpatialNavigationBias(const Config& config);
    ~SpatialNavigationBias() = default;
    
    // Core navigation functions
    void updateLocation(const SpatialLocation& location, float heading);
    void setGoal(const SpatialLocation& goal);
    SpatialPath getNavigationPath();
    float getDistanceToGoal() const;
    
    // Spatial reasoning
    float estimateDistance(const SpatialLocation& from, const SpatialLocation& to) const;
    float estimateDirection(const SpatialLocation& from, const SpatialLocation& to) const;
    bool isLocationFamiliar(const SpatialLocation& location) const;
    std::vector<Landmark> getNearbyLandmarks(const SpatialLocation& location, float radius) const;
    
    // Place and grid cell access
    std::vector<float> getPlaceCellActivations() const;
    std::vector<float> getGridCellActivations() const;
    std::vector<float> getHeadDirectionActivations() const;
    std::vector<float> getBoundaryCellActivations() const;
    
    // Spatial memory management
    void addLandmark(const Landmark& landmark);
    void updateLandmark(int landmark_id, const Landmark& updated_landmark);
    void removeLandmark(int landmark_id);
    void clearSpatialMemory();
    
    // Navigation behavior
    SpatialLocation getNextNavigationStep() const;
    bool hasReachedGoal(float tolerance = 20.0f) const;
    void recalculatePath();
    
    // Spatial learning
    void reinforceLocation(const SpatialLocation& location, float reward);
    void adaptPlaceCells(const SpatialLocation& location);
    void adaptGridCells(const SpatialLocation& location);
    
    // Configuration and state
    void updateConfig(const Config& new_config);
    Config getConfig() const;
    CognitiveMap getCognitiveMap() const;
    SpatialLocation getCurrentLocation() const;
    float getCurrentHeading() const;
    
    // Utility functions
    float getSpatialCoherence() const;
    float getNavigationConfidence() const;
    std::vector<SpatialLocation> getExploredRegions() const;
    void resetNavigation();
};

} // namespace Biases
} // namespace NeuroForge