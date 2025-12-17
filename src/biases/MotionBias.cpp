#include "MotionBias.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
namespace NeuroForge {
namespace Biases {

MotionBias::MotionBias(const Config& config) : config_(config) {
    // Initialize motion history
    motion_history_.clear();
}

bool MotionBias::applyMotionBias(std::vector<float>& features,
                                const std::vector<std::vector<float>>& motion_data,
                                int grid_width, int grid_height,
                                std::uint64_t timestamp_ms) {
    if (motion_data.empty() || features.empty() || grid_width <= 0 || grid_height <= 0) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(motion_mutex_);
    ++total_processing_calls_;
    
    // Analyze current motion field
    MotionField current_field = analyzeMotionField(motion_data, grid_width, grid_height, timestamp_ms);
    
    // Store motion history
    motion_history_.push_back(current_field);
    if (motion_history_.size() > config_.num_motion_history_frames) {
        motion_history_.pop_front();
    }
    
    // Update object tracking
    std::vector<TrackedObject> active_objects;
    for (auto& [id, obj] : tracked_objects_) {
        if (obj.is_active && (timestamp_ms - obj.last_update_ms) < config_.temporal_window_ms * 2) {
            active_objects.push_back(obj);
        }
    }
    
    // Clean up inactive objects
    cleanupInactiveObjects(timestamp_ms);
    
    // Check for startle responses
    bool motion_detected = current_field.motion_density > config_.motion_threshold;
    if (motion_detected) {
        ++total_motion_detections_;
        
        // Check for sudden motion that could trigger startle
        for (int y = 0; y < current_field.height; ++y) {
            for (int x = 0; x < current_field.width; ++x) {
                const MotionVector& motion = current_field.grid[y][x];
                if (checkStartleResponse(motion, timestamp_ms)) {
                    break; // Only one startle per frame
                }
            }
        }
    }
    
    // Apply motion-based attention modulation
    applyMotionAttentionBoost(features, current_field, active_objects);
    
    // Apply startle response if active
    if (isInStartleState(timestamp_ms)) {
        float startle_intensity = getStartleIntensity(timestamp_ms);
        applyStartleResponse(features, startle_intensity);
    }
    
    // Apply predictive attention for tracked objects
    if (config_.enable_trajectory_prediction) {
        applyPredictiveAttention(features, active_objects, grid_width, grid_height);
    }
    
    // Suppress background motion
    suppressBackgroundMotion(features, current_field);
    
    return motion_detected;
}

MotionBias::MotionField MotionBias::analyzeMotionField(const std::vector<std::vector<float>>& motion_data,
                                                      int grid_width, int grid_height,
                                                      std::uint64_t timestamp_ms) {
    MotionField field;
    field.width = grid_width;
    field.height = grid_height;
    field.timestamp_ms = timestamp_ms;
    field.grid.resize(grid_height, std::vector<MotionVector>(grid_width));
    
    // Extract optical flow if enabled and previous frame available
    std::vector<MotionVector> flow_vectors;
    if (config_.enable_optical_flow && !previous_frame_.empty()) {
        flow_vectors = extractOpticalFlow(motion_data, previous_frame_);
    }
    
    // Populate motion grid
    float total_motion = 0.0f;
    int motion_pixels = 0;
    std::vector<float> directions;
    
    for (int y = 0; y < grid_height; ++y) {
        for (int x = 0; x < grid_width; ++x) {
            if (y < static_cast<int>(motion_data.size()) && 
                x < static_cast<int>(motion_data[y].size())) {
                
                // Use optical flow if available, otherwise use direct motion data
                if (!flow_vectors.empty() && (y * grid_width + x) < flow_vectors.size()) {
                    field.grid[y][x] = flow_vectors[y * grid_width + x];
                } else {
                    // Simple motion estimation from intensity changes
                    float motion_magnitude = motion_data[y][x];
                    field.grid[y][x] = MotionVector(motion_magnitude, 0.0f, timestamp_ms);
                }
                
                if (field.grid[y][x].magnitude > config_.background_motion_threshold) {
                    total_motion += field.grid[y][x].magnitude;
                    directions.push_back(field.grid[y][x].direction);
                    ++motion_pixels;
                }
            }
        }
    }
    
    // Calculate field statistics
    field.motion_density = motion_pixels > 0 ? total_motion / motion_pixels : 0.0f;
    
    // Calculate dominant direction
    if (!directions.empty()) {
        // Use circular mean for angles
        float sin_sum = 0.0f, cos_sum = 0.0f;
        for (float dir : directions) {
            sin_sum += std::sin(dir);
            cos_sum += std::cos(dir);
        }
        field.dominant_direction = std::atan2(sin_sum, cos_sum);
    }
    
    // Calculate motion coherence
    field.coherence_score = calculateMotionCoherence(field, config_.motion_coherence_radius);
    
    // Store current frame for next optical flow calculation
    previous_frame_ = motion_data;
    
    return field;
}

MotionBias::BiologicalMotionFeatures MotionBias::analyzeBiologicalMotion(const std::vector<MotionVector>& motion_history) {
    BiologicalMotionFeatures features;
    
    if (motion_history.empty()) {
        return features;
    }
    
    // Calculate gait score (periodic walking pattern)
    if (config_.enable_gait_analysis) {
        features.gait_score = calculateGaitScore(motion_history);
    }
    
    // Calculate gesture score (hand/arm movements)
    if (config_.enable_gesture_recognition) {
        features.gesture_score = calculateGestureScore(motion_history);
    }
    
    // Calculate periodicity
    features.periodicity = calculatePeriodicity(motion_history);
    
    // Calculate smoothness
    features.smoothness = calculateSmoothness(motion_history);
    
    // Calculate predictability
    if (motion_history.size() >= 3) {
        float prediction_error = 0.0f;
        for (size_t i = 2; i < motion_history.size(); ++i) {
            // Simple linear prediction
            float predicted_x = 2 * motion_history[i-1].x - motion_history[i-2].x;
            float predicted_y = 2 * motion_history[i-1].y - motion_history[i-2].y;
            
            float error = std::sqrt(std::pow(motion_history[i].x - predicted_x, 2) +
                                  std::pow(motion_history[i].y - predicted_y, 2));
            prediction_error += error;
        }
        features.predictability = 1.0f / (1.0f + prediction_error / (motion_history.size() - 2));
    }
    
    // Detect predatory motion
    if (config_.enable_predator_detection) {
        features.is_predatory = detectPredatoryMotion(motion_history);
    }
    
    // Detect approaching motion
    if (motion_history.size() >= 2) {
        float initial_distance = std::sqrt(motion_history[0].x * motion_history[0].x + 
                                         motion_history[0].y * motion_history[0].y);
        float final_distance = std::sqrt(motion_history.back().x * motion_history.back().x + 
                                       motion_history.back().y * motion_history.back().y);
        features.is_approaching = final_distance < initial_distance * 0.8f;
    }
    
    // Classify motion type
    if (features.gait_score > 0.7f) {
        features.motion_type = "walking";
    } else if (features.gesture_score > 0.7f) {
        features.motion_type = "gesture";
    } else if (features.is_predatory) {
        features.motion_type = "predatory";
    } else if (features.periodicity > 0.6f) {
        features.motion_type = "periodic";
    } else if (features.smoothness > 0.8f) {
        features.motion_type = "smooth";
    } else {
        features.motion_type = "random";
    }
    
    return features;
}

std::vector<std::pair<float, float>> MotionBias::predictTrajectory(const std::vector<MotionVector>& motion_history,
                                                                  float prediction_time_ms) {
    std::vector<std::pair<float, float>> trajectory;
    
    if (motion_history.size() < 3) {
        return trajectory;
    }
    
    // Extract position and time data
    std::vector<float> x_positions, y_positions, times;
    for (const auto& motion : motion_history) {
        x_positions.push_back(motion.x);
        y_positions.push_back(motion.y);
        times.push_back(static_cast<float>(motion.timestamp_ms));
    }
    
    // Fit polynomial trajectories (2nd degree for smooth motion)
    std::vector<float> x_coeffs = fitPolynomial(times, x_positions, 2);
    std::vector<float> y_coeffs = fitPolynomial(times, y_positions, 2);
    
    if (x_coeffs.empty() || y_coeffs.empty()) {
        return trajectory;
    }
    
    // Generate predicted trajectory points
    float current_time = times.back();
    float end_time = current_time + prediction_time_ms;
    float time_step = prediction_time_ms / 10.0f; // 10 prediction points
    
    for (float t = current_time; t <= end_time; t += time_step) {
        float pred_x = evaluatePolynomial(x_coeffs, t);
        float pred_y = evaluatePolynomial(y_coeffs, t);
        trajectory.emplace_back(pred_x, pred_y);
    }
    
    ++trajectory_predictions_;
    return trajectory;
}

int MotionBias::startTracking(const std::vector<MotionVector>& initial_motion,
                             float x, float y, std::uint64_t timestamp_ms) {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    
    int object_id = next_object_id_++;
    TrackedObject& obj = tracked_objects_[object_id];
    
    obj.object_id = object_id;
    obj.motion_history = initial_motion;
    obj.confidence = 1.0f;
    obj.last_update_ms = timestamp_ms;
    obj.is_active = true;
    
    // Analyze biological motion features
    obj.bio_features = analyzeBiologicalMotion(initial_motion);
    
    // Generate initial trajectory prediction
    if (config_.enable_trajectory_prediction) {
        obj.predicted_trajectory = predictTrajectory(initial_motion, config_.prediction_horizon_ms);
    }
    
    return object_id;
}

bool MotionBias::updateTracking(int object_id, const MotionVector& new_motion) {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    
    auto it = tracked_objects_.find(object_id);
    if (it == tracked_objects_.end() || !it->second.is_active) {
        return false;
    }
    
    TrackedObject& obj = it->second;
    
    // Add new motion to history
    obj.motion_history.push_back(new_motion);
    if (obj.motion_history.size() > config_.num_motion_history_frames) {
        obj.motion_history.erase(obj.motion_history.begin());
    }
    
    // Update biological motion analysis
    obj.bio_features = analyzeBiologicalMotion(obj.motion_history);
    
    // Update trajectory prediction
    if (config_.enable_trajectory_prediction) {
        obj.predicted_trajectory = predictTrajectory(obj.motion_history, config_.prediction_horizon_ms);
    }
    
    // Update confidence based on motion consistency
    float motion_consistency = obj.bio_features.predictability;
    obj.confidence = (obj.confidence * 0.9f) + (motion_consistency * 0.1f);
    
    obj.last_update_ms = new_motion.timestamp_ms;
    
    // Update average tracking confidence
    float total_confidence = 0.0f;
    int active_count = 0;
    for (const auto& [id, tracked_obj] : tracked_objects_) {
        if (tracked_obj.is_active) {
            total_confidence += tracked_obj.confidence;
            ++active_count;
        }
    }
    average_tracking_confidence_ = active_count > 0 ? total_confidence / active_count : 0.0f;
    
    return true;
}

void MotionBias::stopTracking(int object_id) {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    
    auto it = tracked_objects_.find(object_id);
    if (it != tracked_objects_.end()) {
        it->second.is_active = false;
    }
}

std::vector<MotionBias::TrackedObject> MotionBias::getActiveTrackedObjects() const {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    
    std::vector<TrackedObject> active_objects;
    for (const auto& [id, obj] : tracked_objects_) {
        if (obj.is_active) {
            active_objects.push_back(obj);
        }
    }
    
    return active_objects;
}

bool MotionBias::checkStartleResponse(const MotionVector& motion, std::uint64_t timestamp_ms) {
    // Check if motion magnitude exceeds startle threshold
    if (motion.magnitude > config_.startle_threshold && !in_startle_state_) {
        in_startle_state_ = true;
        startle_start_time_ms_ = timestamp_ms;
        current_startle_intensity_ = std::min(1.0f, motion.magnitude / config_.startle_threshold);
        ++startle_responses_;
        return true;
    }
    
    return false;
}

bool MotionBias::isInStartleState(std::uint64_t current_time_ms) const {
    if (!in_startle_state_) {
        return false;
    }
    
    return (current_time_ms - startle_start_time_ms_) < config_.startle_recovery_ms;
}

float MotionBias::getStartleIntensity(std::uint64_t current_time_ms) const {
    if (!isInStartleState(current_time_ms)) {
        return 0.0f;
    }
    
    // Exponential decay of startle intensity
    float elapsed_ms = static_cast<float>(current_time_ms - startle_start_time_ms_);
    float decay_factor = std::exp(-elapsed_ms / (config_.startle_recovery_ms * 0.3f));
    
    return current_startle_intensity_ * decay_factor;
}

bool MotionBias::detectLooming(const std::vector<MotionVector>& motion_sequence,
                              float object_size_change) {
    if (motion_sequence.size() < 3) {
        return false;
    }
    
    // Check for consistent approach pattern
    bool approaching = true;
    for (size_t i = 1; i < motion_sequence.size(); ++i) {
        float prev_distance = std::sqrt(motion_sequence[i-1].x * motion_sequence[i-1].x + 
                                      motion_sequence[i-1].y * motion_sequence[i-1].y);
        float curr_distance = std::sqrt(motion_sequence[i].x * motion_sequence[i].x + 
                                      motion_sequence[i].y * motion_sequence[i].y);
        
        if (curr_distance >= prev_distance) {
            approaching = false;
            break;
        }
    }
    
    // Check for size increase (looming cue)
    bool size_increasing = object_size_change > config_.looming_threshold;
    
    if (approaching && size_increasing) {
        ++looming_detections_;
        return true;
    }
    
    return false;
}

float MotionBias::calculateMotionCoherence(const MotionField& motion_field,
                                          float analysis_radius) {
    if (motion_field.grid.empty()) {
        return 0.0f;
    }
    
    float total_coherence = 0.0f;
    int coherence_samples = 0;
    
    for (int y = 0; y < motion_field.height; ++y) {
        for (int x = 0; x < motion_field.width; ++x) {
            const MotionVector& center_motion = motion_field.grid[y][x];
            
            if (center_motion.magnitude < config_.background_motion_threshold) {
                continue;
            }
            
            // Analyze neighborhood coherence
            float local_coherence = 0.0f;
            int neighbor_count = 0;
            
            int radius = static_cast<int>(analysis_radius);
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = x + dx;
                    int ny = y + dy;
                    
                    if (nx >= 0 && nx < motion_field.width && 
                        ny >= 0 && ny < motion_field.height) {
                        
                        const MotionVector& neighbor_motion = motion_field.grid[ny][nx];
                        
                        if (neighbor_motion.magnitude > config_.background_motion_threshold) {
                            // Calculate direction similarity
                            float angle_diff = std::abs(normalizeAngle(center_motion.direction - 
                                                                     neighbor_motion.direction));
                            float direction_similarity = std::cos(angle_diff);
                            
                            // Calculate magnitude similarity
                            float mag_ratio = std::min(center_motion.magnitude, neighbor_motion.magnitude) /
                                            std::max(center_motion.magnitude, neighbor_motion.magnitude);
                            
                            local_coherence += direction_similarity * mag_ratio;
                            ++neighbor_count;
                        }
                    }
                }
            }
            
            if (neighbor_count > 0) {
                total_coherence += local_coherence / neighbor_count;
                ++coherence_samples;
            }
        }
    }
    
    return coherence_samples > 0 ? total_coherence / coherence_samples : 0.0f;
}

std::vector<std::vector<float>> MotionBias::identifyCoherentGroups(const MotionField& motion_field) {
    std::vector<std::vector<float>> groups;
    
    // Simple clustering based on motion coherence
    // This is a simplified implementation - more sophisticated clustering could be used
    
    std::vector<std::vector<bool>> visited(motion_field.height, 
                                         std::vector<bool>(motion_field.width, false));
    
    for (int y = 0; y < motion_field.height; ++y) {
        for (int x = 0; x < motion_field.width; ++x) {
            if (!visited[y][x] && 
                motion_field.grid[y][x].magnitude > config_.motion_threshold) {
                
                std::vector<float> group;
                std::vector<std::pair<int, int>> stack;
                stack.emplace_back(x, y);
                
                while (!stack.empty()) {
                    auto [cx, cy] = stack.back();
                    stack.pop_back();
                    
                    if (visited[cy][cx]) continue;
                    visited[cy][cx] = true;
                    
                    group.push_back(static_cast<float>(cx));
                    group.push_back(static_cast<float>(cy));
                    group.push_back(motion_field.grid[cy][cx].magnitude);
                    group.push_back(motion_field.grid[cy][cx].direction);
                    
                    // Check neighbors for similar motion
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            int nx = cx + dx;
                            int ny = cy + dy;
                            
                            if (nx >= 0 && nx < motion_field.width && 
                                ny >= 0 && ny < motion_field.height && 
                                !visited[ny][nx]) {
                                
                                const MotionVector& current = motion_field.grid[cy][cx];
                                const MotionVector& neighbor = motion_field.grid[ny][nx];
                                
                                // Check if neighbor has similar motion
                                float angle_diff = std::abs(normalizeAngle(current.direction - 
                                                                         neighbor.direction));
                                float mag_ratio = std::min(current.magnitude, neighbor.magnitude) /
                                                std::max(current.magnitude, neighbor.magnitude);
                                
                                if (angle_diff < M_PI / 4 && mag_ratio > 0.5f) {
                                    stack.emplace_back(nx, ny);
                                }
                            }
                        }
                    }
                }
                
                if (group.size() >= 12) { // At least 3 pixels (4 values each)
                    groups.push_back(group);
                }
            }
        }
    }
    
    return groups;
}

void MotionBias::updateConfig(const Config& new_config) {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    config_ = new_config;
    
    // Resize motion history if needed
    if (motion_history_.size() > config_.num_motion_history_frames) {
        motion_history_.resize(config_.num_motion_history_frames);
    }
}

MotionBias::Config MotionBias::getConfig() const {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    return config_;
}

MotionBias::Statistics MotionBias::getStatistics() const {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    
    Statistics stats;
    stats.total_motion_detections = total_motion_detections_;
    stats.biological_motion_detections = biological_motion_detections_;
    stats.startle_responses = startle_responses_;
    stats.looming_detections = looming_detections_;
    stats.trajectory_predictions = trajectory_predictions_;
    stats.average_tracking_confidence = average_tracking_confidence_;
    stats.total_processing_calls = total_processing_calls_;
    
    stats.motion_detection_rate = total_processing_calls_ > 0 ? 
        static_cast<float>(total_motion_detections_) / total_processing_calls_ : 0.0f;
    
    stats.biological_motion_rate = total_motion_detections_ > 0 ? 
        static_cast<float>(biological_motion_detections_) / total_motion_detections_ : 0.0f;
    
    // Count active tracks
    for (const auto& [id, obj] : tracked_objects_) {
        if (obj.is_active) {
            ++stats.active_tracks;
        }
    }
    
    return stats;
}

void MotionBias::reset() {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    
    tracked_objects_.clear();
    motion_history_.clear();
    previous_frame_.clear();
    
    in_startle_state_ = false;
    startle_start_time_ms_ = 0;
    current_startle_intensity_ = 0.0f;
    
    total_motion_detections_ = 0;
    biological_motion_detections_ = 0;
    startle_responses_ = 0;
    looming_detections_ = 0;
    trajectory_predictions_ = 0;
    total_processing_calls_ = 0;
    average_tracking_confidence_ = 0.0f;
    
    next_object_id_ = 1;
}

std::vector<MotionBias::MotionVector> MotionBias::extractOpticalFlow(
    const std::vector<std::vector<float>>& current_frame,
    const std::vector<std::vector<float>>& previous_frame) {
    
    std::vector<MotionVector> flow_vectors;
    
    if (current_frame.size() != previous_frame.size() || current_frame.empty()) {
        return flow_vectors;
    }
    
    std::uint64_t timestamp = getCurrentTimestamp();
    
    // Simple Lucas-Kanade optical flow approximation
    for (size_t y = 1; y < current_frame.size() - 1; ++y) {
        for (size_t x = 1; x < current_frame[y].size() - 1; ++x) {
            // Calculate gradients
            float Ix = (current_frame[y][x+1] - current_frame[y][x-1]) / 2.0f;
            float Iy = (current_frame[y+1][x] - current_frame[y-1][x]) / 2.0f;
            float It = current_frame[y][x] - previous_frame[y][x];
            
            // Solve for optical flow (simplified)
            float denominator = Ix * Ix + Iy * Iy;
            if (denominator > 1e-6f) {
                float u = -(Ix * It) / denominator;
                float v = -(Iy * It) / denominator;
                
                flow_vectors.emplace_back(u, v, timestamp);
            } else {
                flow_vectors.emplace_back(0.0f, 0.0f, timestamp);
            }
        }
    }
    
    return flow_vectors;
}

bool MotionBias::detectCollisionRisk(const TrackedObject& obj1, const TrackedObject& obj2,
                                    float time_horizon_ms) {
    if (!config_.enable_collision_detection || 
        obj1.predicted_trajectory.empty() || 
        obj2.predicted_trajectory.empty()) {
        return false;
    }
    
    // Check if predicted trajectories intersect within time horizon
    size_t max_steps = std::min(obj1.predicted_trajectory.size(), 
                               obj2.predicted_trajectory.size());
    
    for (size_t i = 0; i < max_steps; ++i) {
        float distance = calculateDistance(obj1.predicted_trajectory[i].first,
                                         obj1.predicted_trajectory[i].second,
                                         obj2.predicted_trajectory[i].first,
                                         obj2.predicted_trajectory[i].second);
        
        // Collision threshold (could be based on object sizes)
        if (distance < 5.0f) {
            return true;
        }
    }
    
    return false;
}

std::vector<std::pair<int, int>> MotionBias::getPotentialCollisions(float time_horizon_ms) {
    std::lock_guard<std::mutex> lock(motion_mutex_);
    
    std::vector<std::pair<int, int>> collisions;
    std::vector<TrackedObject> active_objects;
    
    for (const auto& [id, obj] : tracked_objects_) {
        if (obj.is_active) {
            active_objects.push_back(obj);
        }
    }
    
    // Check all pairs of active objects
    for (size_t i = 0; i < active_objects.size(); ++i) {
        for (size_t j = i + 1; j < active_objects.size(); ++j) {
            if (detectCollisionRisk(active_objects[i], active_objects[j], time_horizon_ms)) {
                collisions.emplace_back(active_objects[i].object_id, 
                                      active_objects[j].object_id);
            }
        }
    }
    
    return collisions;
}

// Private helper method implementations

void MotionBias::applyMotionAttentionBoost(std::vector<float>& features,
                                          const MotionField& motion_field,
                                          const std::vector<TrackedObject>& active_objects) {
    if (features.empty()) {
        return;
    }
    
    float boost_factor = 1.0f;
    
    // Boost based on motion density
    if (motion_field.motion_density > config_.motion_threshold) {
        boost_factor *= (1.0f + motion_field.motion_density * 0.5f);
    }
    
    // Additional boost for biological motion
    for (const auto& obj : active_objects) {
        if (obj.bio_features.gait_score > config_.biological_motion_threshold ||
            obj.bio_features.gesture_score > config_.biological_motion_threshold) {
            boost_factor *= config_.biological_motion_boost;
            ++biological_motion_detections_;
            break; // Only count once per frame
        }
    }
    
    // Boost for looming objects
    for (const auto& obj : active_objects) {
        if (obj.bio_features.is_approaching) {
            boost_factor *= config_.looming_attention_boost;
            break;
        }
    }
    
    // Apply boost to features
    for (float& feature : features) {
        feature *= boost_factor;
    }
}

void MotionBias::applyStartleResponse(std::vector<float>& features,
                                     float startle_intensity) {
    if (features.empty() || startle_intensity <= 0.0f) {
        return;
    }
    
    float boost_factor = 1.0f + (startle_intensity * config_.startle_attention_boost);
    
    for (float& feature : features) {
        feature *= boost_factor;
    }
}

void MotionBias::applyPredictiveAttention(std::vector<float>& features,
                                         const std::vector<TrackedObject>& active_objects,
                                         int grid_width, int grid_height) {
    if (features.empty() || active_objects.empty()) {
        return;
    }
    
    // This is a simplified implementation - in practice, you'd map predicted positions
    // to specific feature indices based on spatial layout
    
    bool has_predictions = false;
    for (const auto& obj : active_objects) {
        if (!obj.predicted_trajectory.empty()) {
            has_predictions = true;
            break;
        }
    }
    
    if (has_predictions) {
        float boost_factor = config_.predictive_attention_boost;
        for (float& feature : features) {
            feature *= boost_factor;
        }
    }
}

void MotionBias::suppressBackgroundMotion(std::vector<float>& features,
                                         const MotionField& motion_field) {
    if (features.empty()) {
        return;
    }
    
    // Suppress features in areas with low motion
    if (motion_field.motion_density < config_.background_motion_threshold) {
        float suppression_factor = config_.noise_suppression;
        for (float& feature : features) {
            feature *= suppression_factor;
        }
    }
}

float MotionBias::calculateGaitScore(const std::vector<MotionVector>& motion_history) {
    if (motion_history.size() < 10) {
        return 0.0f;
    }
    
    // Analyze for periodic walking pattern
    std::vector<float> magnitudes;
    for (const auto& motion : motion_history) {
        magnitudes.push_back(motion.magnitude);
    }
    
    // Simple periodicity detection using autocorrelation
    float max_correlation = 0.0f;
    int best_period = 0;
    
    for (int period = 3; period < static_cast<int>(magnitudes.size()) / 2; ++period) {
        float correlation = 0.0f;
        int samples = static_cast<int>(magnitudes.size()) - period;
        
        for (int i = 0; i < samples; ++i) {
            correlation += magnitudes[i] * magnitudes[i + period];
        }
        
        correlation /= samples;
        
        if (correlation > max_correlation) {
            max_correlation = correlation;
            best_period = period;
        }
    }
    
    // Gait typically has period of 4-8 frames at typical frame rates
    if (best_period >= 4 && best_period <= 8) {
        return std::min(1.0f, max_correlation / 10.0f); // Normalize
    }
    
    return 0.0f;
}

float MotionBias::calculateGestureScore(const std::vector<MotionVector>& motion_history) {
    if (motion_history.size() < 5) {
        return 0.0f;
    }
    
    // Analyze for gesture-like patterns (smooth, directed movements)
    float smoothness = calculateSmoothness(motion_history);
    float directedness = 0.0f;
    
    // Calculate directional consistency
    if (motion_history.size() >= 2) {
        float direction_variance = 0.0f;
        float mean_direction = 0.0f;
        
        // Calculate mean direction
        float sin_sum = 0.0f, cos_sum = 0.0f;
        for (const auto& motion : motion_history) {
            sin_sum += std::sin(motion.direction);
            cos_sum += std::cos(motion.direction);
        }
        mean_direction = std::atan2(sin_sum, cos_sum);
        
        // Calculate variance
        for (const auto& motion : motion_history) {
            float angle_diff = normalizeAngle(motion.direction - mean_direction);
            direction_variance += angle_diff * angle_diff;
        }
        direction_variance /= motion_history.size();
        
        directedness = 1.0f / (1.0f + direction_variance);
    }
    
    return (smoothness + directedness) / 2.0f;
}

float MotionBias::calculatePeriodicity(const std::vector<MotionVector>& motion_history) {
    if (motion_history.size() < 6) {
        return 0.0f;
    }
    
    // Extract magnitude sequence
    std::vector<float> magnitudes;
    for (const auto& motion : motion_history) {
        magnitudes.push_back(motion.magnitude);
    }
    
    // Find dominant frequency using simple autocorrelation
    float max_autocorr = 0.0f;
    
    for (size_t lag = 1; lag < magnitudes.size() / 2; ++lag) {
        float autocorr = 0.0f;
        size_t samples = magnitudes.size() - lag;
        
        for (size_t i = 0; i < samples; ++i) {
            autocorr += magnitudes[i] * magnitudes[i + lag];
        }
        
        autocorr /= samples;
        max_autocorr = std::max(max_autocorr, autocorr);
    }
    
    return std::min(1.0f, max_autocorr / 5.0f); // Normalize
}

float MotionBias::calculateSmoothness(const std::vector<MotionVector>& motion_history) {
    if (motion_history.size() < 3) {
        return 0.0f;
    }
    
    // Calculate acceleration variance (smoothness measure)
    float acceleration_variance = 0.0f;
    
    for (size_t i = 2; i < motion_history.size(); ++i) {
        float ax = motion_history[i].x - 2 * motion_history[i-1].x + motion_history[i-2].x;
        float ay = motion_history[i].y - 2 * motion_history[i-1].y + motion_history[i-2].y;
        float acceleration_mag = std::sqrt(ax * ax + ay * ay);
        acceleration_variance += acceleration_mag * acceleration_mag;
    }
    
    acceleration_variance /= (motion_history.size() - 2);
    
    // Smoothness is inverse of acceleration variance
    return 1.0f / (1.0f + acceleration_variance);
}

bool MotionBias::detectPredatoryMotion(const std::vector<MotionVector>& motion_history) {
    if (motion_history.size() < 5) {
        return false;
    }
    
    // Predatory motion characteristics:
    // 1. Periods of stillness followed by sudden movement
    // 2. Directed approach behavior
    // 3. Variable speed (stalking then pouncing)
    
    bool has_stillness_periods = false;
    bool has_sudden_movement = false;
    bool is_approaching = false;
    
    // Check for stillness periods
    int stillness_count = 0;
    for (const auto& motion : motion_history) {
        if (motion.magnitude < config_.background_motion_threshold) {
            ++stillness_count;
        }
    }
    has_stillness_periods = stillness_count > static_cast<int>(motion_history.size()) * 0.3f;
    
    // Check for sudden movements
    for (size_t i = 1; i < motion_history.size(); ++i) {
        float speed_change = std::abs(motion_history[i].magnitude - motion_history[i-1].magnitude);
        if (speed_change > config_.startle_threshold * 0.5f) {
            has_sudden_movement = true;
            break;
        }
    }
    
    // Check for approach behavior
    if (motion_history.size() >= 2) {
        float initial_distance = std::sqrt(motion_history[0].x * motion_history[0].x + 
                                         motion_history[0].y * motion_history[0].y);
        float final_distance = std::sqrt(motion_history.back().x * motion_history.back().x + 
                                       motion_history.back().y * motion_history.back().y);
        is_approaching = final_distance < initial_distance * 0.7f;
    }
    
    return has_stillness_periods && has_sudden_movement && is_approaching;
}

std::vector<float> MotionBias::fitPolynomial(const std::vector<float>& x_data,
                                            const std::vector<float>& y_data,
                                            int degree) {
    if (x_data.size() != y_data.size() || x_data.size() < degree + 1) {
        return {};
    }
    
    // Simple least squares polynomial fitting
    // For degree 2: y = a + bx + cx^2
    
    if (degree == 2 && x_data.size() >= 3) {
        // Set up normal equations for quadratic fit
        size_t n = x_data.size();
        
        float sum_x = 0, sum_x2 = 0, sum_x3 = 0, sum_x4 = 0;
        float sum_y = 0, sum_xy = 0, sum_x2y = 0;
        
        for (size_t i = 0; i < n; ++i) {
            float x = x_data[i];
            float y = y_data[i];
            float x2 = x * x;
            float x3 = x2 * x;
            float x4 = x2 * x2;
            
            sum_x += x;
            sum_x2 += x2;
            sum_x3 += x3;
            sum_x4 += x4;
            sum_y += y;
            sum_xy += x * y;
            sum_x2y += x2 * y;
        }
        
        // Solve 3x3 system (simplified for degree 2)
        // This is a basic implementation - more robust methods exist
        
        float det = n * sum_x2 * sum_x4 + 2 * sum_x * sum_x2 * sum_x3 - 
                   sum_x2 * sum_x2 * sum_x2 - n * sum_x3 * sum_x3 - sum_x * sum_x * sum_x4;
        
        if (std::abs(det) < 1e-10f) {
            return {}; // Singular matrix
        }
        
        // Calculate coefficients using Cramer's rule (simplified)
        float a = (sum_y * sum_x2 * sum_x4 + sum_xy * sum_x2 * sum_x3 + sum_x2y * sum_x * sum_x2 -
                  sum_x2y * sum_x2 * sum_x2 - sum_y * sum_x3 * sum_x3 - sum_xy * sum_x * sum_x4) / det;
        
        float b = (n * sum_xy * sum_x4 + sum_y * sum_x2 * sum_x3 + sum_x2y * sum_x * sum_x2 -
                  sum_xy * sum_x2 * sum_x2 - n * sum_x2y * sum_x3 - sum_y * sum_x * sum_x4) / det;
        
        float c = (n * sum_x2 * sum_x2y + sum_x * sum_xy * sum_x3 + sum_y * sum_x * sum_x2 -
                  sum_y * sum_x2 * sum_x2 - n * sum_x3 * sum_xy - sum_x * sum_x * sum_x2y) / det;
        
        return {a, b, c};
    }
    
    return {}; // Only degree 2 implemented for now
}

float MotionBias::evaluatePolynomial(const std::vector<float>& coefficients, float x) {
    if (coefficients.empty()) {
        return 0.0f;
    }
    
    float result = 0.0f;
    float x_power = 1.0f;
    
    for (float coeff : coefficients) {
        result += coeff * x_power;
        x_power *= x;
    }
    
    return result;
}

std::uint64_t MotionBias::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

float MotionBias::calculateDistance(float x1, float y1, float x2, float y2) const {
    return std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

float MotionBias::normalizeAngle(float angle) const {
    while (angle > M_PI) angle -= 2 * M_PI;
    while (angle < -M_PI) angle += 2 * M_PI;
    return angle;
}

bool MotionBias::isValidMotionVector(const MotionVector& motion) const {
    return std::isfinite(motion.x) && std::isfinite(motion.y) && 
           std::isfinite(motion.magnitude) && std::isfinite(motion.direction) &&
           motion.magnitude >= 0.0f;
}

void MotionBias::cleanupInactiveObjects(std::uint64_t current_time_ms) {
    auto it = tracked_objects_.begin();
    while (it != tracked_objects_.end()) {
        if (!it->second.is_active || 
            (current_time_ms - it->second.last_update_ms) > config_.temporal_window_ms * 5) {
            it = tracked_objects_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace Biases
} // namespace NeuroForge