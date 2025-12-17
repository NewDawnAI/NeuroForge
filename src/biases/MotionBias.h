#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <deque>
#include <cmath>

namespace NeuroForge {
namespace Biases {

/**
 * @brief MotionBias implements biological motion detection and tracking
 * 
 * This class provides innate capabilities for:
 * - Biological motion pattern recognition (walking, running, gestures)
 * - Predictive motion tracking and trajectory estimation
 * - Startle response to sudden movements
 * - Looming detection for approaching objects
 * - Motion coherence analysis for group movements
 * - Predator-prey motion pattern recognition
 */
class MotionBias {
public:
    struct Config {
        // Motion detection parameters
        float motion_threshold = 0.1f;              // Minimum motion magnitude
        float biological_motion_threshold = 0.6f;   // Threshold for biological motion
        float startle_threshold = 2.0f;             // Sudden motion threshold
        float looming_threshold = 1.5f;             // Approaching object threshold
        
        // Temporal parameters
        float temporal_window_ms = 200.0f;          // Motion analysis window
        float prediction_horizon_ms = 500.0f;      // Motion prediction timeframe
        float startle_recovery_ms = 1000.0f;       // Startle response duration
        
        // Spatial parameters
        float max_tracking_distance = 100.0f;      // Maximum tracking range
        int max_tracked_objects = 10;              // Maximum simultaneous tracks
        float motion_coherence_radius = 50.0f;     // Group motion analysis radius
        
        // Biological motion features
        bool enable_gait_analysis = true;          // Analyze walking patterns
        bool enable_gesture_recognition = true;    // Recognize hand/arm gestures
        bool enable_predator_detection = true;     // Detect predatory motion patterns
        bool enable_social_motion = true;          // Analyze social interactions
        
        // Response parameters
        float biological_motion_boost = 2.5f;      // Attention boost for bio motion
        float startle_attention_boost = 4.0f;      // Attention boost during startle
        float looming_attention_boost = 3.0f;      // Attention boost for looming
        float predictive_attention_boost = 1.8f;   // Boost for predicted locations
        
        // Filtering parameters
        float noise_suppression = 0.3f;            // Non-motion suppression
        float background_motion_threshold = 0.05f; // Background motion filter
        int min_motion_duration_ms = 50;           // Minimum motion duration
        
        // Advanced features
        bool enable_optical_flow = true;           // Use optical flow analysis
        bool enable_trajectory_prediction = true;  // Predict motion trajectories
        bool enable_collision_detection = true;    // Detect potential collisions
        int num_motion_history_frames = 20;        // Motion history buffer size
    };
    
    struct MotionVector {
        float x, y;                                // Motion vector components
        float magnitude;                           // Motion magnitude
        float direction;                           // Motion direction (radians)
        std::uint64_t timestamp_ms;               // Timestamp
        
        MotionVector() : x(0), y(0), magnitude(0), direction(0), timestamp_ms(0) {}
        MotionVector(float x_, float y_, std::uint64_t ts) 
            : x(x_), y(y_), timestamp_ms(ts) {
            magnitude = std::sqrt(x*x + y*y);
            direction = std::atan2(y, x);
        }
    };
    
    struct BiologicalMotionFeatures {
        float gait_score = 0.0f;                   // Walking pattern score
        float gesture_score = 0.0f;                // Gesture pattern score
        float periodicity = 0.0f;                  // Motion periodicity
        float smoothness = 0.0f;                   // Motion smoothness
        float predictability = 0.0f;               // Motion predictability
        float social_coordination = 0.0f;          // Social motion coordination
        bool is_predatory = false;                 // Predatory motion pattern
        bool is_approaching = false;               // Approaching motion
        std::string motion_type = "unknown";       // Classified motion type
    };
    
    struct TrackedObject {
        int object_id;                             // Unique object identifier
        std::vector<MotionVector> motion_history;  // Motion history
        BiologicalMotionFeatures bio_features;     // Biological motion analysis
        std::vector<std::pair<float, float>> predicted_trajectory; // Predicted path
        float confidence = 0.0f;                   // Tracking confidence
        std::uint64_t last_update_ms = 0;         // Last update timestamp
        bool is_active = false;                    // Active tracking status
        
        // Startle response state
        bool triggered_startle = false;            // Has triggered startle
        std::uint64_t startle_time_ms = 0;        // Startle trigger time
        float startle_intensity = 0.0f;           // Startle response intensity
    };
    
    struct MotionField {
        std::vector<std::vector<MotionVector>> grid; // Spatial motion grid
        int width, height;                         // Grid dimensions
        float coherence_score = 0.0f;              // Motion coherence
        float dominant_direction = 0.0f;           // Dominant motion direction
        float motion_density = 0.0f;               // Motion density
        std::uint64_t timestamp_ms = 0;           // Field timestamp
    };
    
    struct Statistics {
        std::uint64_t total_motion_detections = 0;
        std::uint64_t biological_motion_detections = 0;
        std::uint64_t startle_responses = 0;
        std::uint64_t looming_detections = 0;
        std::uint64_t trajectory_predictions = 0;
        float average_tracking_confidence = 0.0f;
        float motion_detection_rate = 0.0f;
        float biological_motion_rate = 0.0f;
        std::uint64_t active_tracks = 0;
        std::uint64_t total_processing_calls = 0;
    };
    
    explicit MotionBias(const Config& config);
    ~MotionBias() = default;
    
    // Main processing interface
    bool applyMotionBias(std::vector<float>& features,
                        const std::vector<std::vector<float>>& motion_data,
                        int grid_width, int grid_height,
                        std::uint64_t timestamp_ms);
    
    // Motion analysis
    MotionField analyzeMotionField(const std::vector<std::vector<float>>& motion_data,
                                  int grid_width, int grid_height,
                                  std::uint64_t timestamp_ms);
    
    BiologicalMotionFeatures analyzeBiologicalMotion(const std::vector<MotionVector>& motion_history);
    
    std::vector<std::pair<float, float>> predictTrajectory(const std::vector<MotionVector>& motion_history,
                                                          float prediction_time_ms);
    
    // Object tracking
    int startTracking(const std::vector<MotionVector>& initial_motion,
                     float x, float y, std::uint64_t timestamp_ms);
    
    bool updateTracking(int object_id, const MotionVector& new_motion);
    
    void stopTracking(int object_id);
    
    std::vector<TrackedObject> getActiveTrackedObjects() const;
    
    // Startle response
    bool checkStartleResponse(const MotionVector& motion, std::uint64_t timestamp_ms);
    
    bool isInStartleState(std::uint64_t current_time_ms) const;
    
    float getStartleIntensity(std::uint64_t current_time_ms) const;
    
    // Looming detection
    bool detectLooming(const std::vector<MotionVector>& motion_sequence,
                      float object_size_change);
    
    // Motion coherence analysis
    float calculateMotionCoherence(const MotionField& motion_field,
                                  float analysis_radius);
    
    std::vector<std::vector<float>> identifyCoherentGroups(const MotionField& motion_field);
    
    // State and configuration
    void updateConfig(const Config& new_config);
    Config getConfig() const;
    Statistics getStatistics() const;
    void reset();
    
    // Advanced features
    std::vector<MotionVector> extractOpticalFlow(const std::vector<std::vector<float>>& current_frame,
                                                const std::vector<std::vector<float>>& previous_frame);
    
    bool detectCollisionRisk(const TrackedObject& obj1, const TrackedObject& obj2,
                            float time_horizon_ms);
    
    std::vector<std::pair<int, int>> getPotentialCollisions(float time_horizon_ms);
    
private:
    Config config_;
    mutable std::mutex motion_mutex_;
    
    // Tracking state
    std::unordered_map<int, TrackedObject> tracked_objects_;
    int next_object_id_ = 1;
    std::deque<MotionField> motion_history_;
    
    // Startle response state
    bool in_startle_state_ = false;
    std::uint64_t startle_start_time_ms_ = 0;
    float current_startle_intensity_ = 0.0f;
    
    // Statistics
    std::uint64_t total_motion_detections_ = 0;
    std::uint64_t biological_motion_detections_ = 0;
    std::uint64_t startle_responses_ = 0;
    std::uint64_t looming_detections_ = 0;
    std::uint64_t trajectory_predictions_ = 0;
    std::uint64_t total_processing_calls_ = 0;
    float average_tracking_confidence_ = 0.0f;
    
    // Previous frame for optical flow
    std::vector<std::vector<float>> previous_frame_;
    
    // Helper methods
    void applyMotionAttentionBoost(std::vector<float>& features,
                                  const MotionField& motion_field,
                                  const std::vector<TrackedObject>& active_objects);
    
    void applyStartleResponse(std::vector<float>& features,
                             float startle_intensity);
    
    void applyPredictiveAttention(std::vector<float>& features,
                                 const std::vector<TrackedObject>& active_objects,
                                 int grid_width, int grid_height);
    
    void suppressBackgroundMotion(std::vector<float>& features,
                                 const MotionField& motion_field);
    
    // Motion analysis helpers
    float calculateGaitScore(const std::vector<MotionVector>& motion_history);
    float calculateGestureScore(const std::vector<MotionVector>& motion_history);
    float calculatePeriodicity(const std::vector<MotionVector>& motion_history);
    float calculateSmoothness(const std::vector<MotionVector>& motion_history);
    bool detectPredatoryMotion(const std::vector<MotionVector>& motion_history);
    
    // Trajectory prediction helpers
    std::vector<float> fitPolynomial(const std::vector<float>& x_data,
                                    const std::vector<float>& y_data,
                                    int degree);
    
    float evaluatePolynomial(const std::vector<float>& coefficients, float x);
    
    // Utility methods
    std::uint64_t getCurrentTimestamp() const;
    float calculateDistance(float x1, float y1, float x2, float y2) const;
    float normalizeAngle(float angle) const;
    bool isValidMotionVector(const MotionVector& motion) const;
    void cleanupInactiveObjects(std::uint64_t current_time_ms);
};

} // namespace Biases
} // namespace NeuroForge