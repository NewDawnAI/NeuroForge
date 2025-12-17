#include "SpatialNavigationBias.h"
#include <cmath>
#include <algorithm>
#include <random>
#include <numeric>
#include <queue>
#include <set>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NeuroForge {
namespace Biases {

SpatialNavigationBias::SpatialNavigationBias(const Config& config)
    : config_(config), current_heading_(0.0f), is_navigating_(false) {
    
    // Initialize grid cells with different scales and orientations
    cognitive_map_.grid_cells.reserve(config_.grid_orientation_count * 4); // 4 scales
    for (int scale_idx = 0; scale_idx < 4; ++scale_idx) {
        float scale = config_.grid_scale * std::pow(config_.grid_spacing_ratio, scale_idx);
        for (int orient_idx = 0; orient_idx < config_.grid_orientation_count; ++orient_idx) {
            GridCell cell;
            cell.scale = scale;
            cell.orientation = (orient_idx * M_PI) / config_.grid_orientation_count;
            cell.phase_x = 0.0f;
            cell.phase_y = 0.0f;
            cell.activation = 0.0f;
            
            // Initialize firing map (simplified 2D grid)
            int map_size = static_cast<int>(500.0f / scale) + 1;
            cell.firing_map.resize(map_size, std::vector<float>(map_size, 0.0f));
            
            cognitive_map_.grid_cells.push_back(cell);
        }
    }
    
    // Initialize head direction cells
    cognitive_map_.hd_cells.reserve(config_.head_direction_cells);
    for (int i = 0; i < config_.head_direction_cells; ++i) {
        HeadDirectionCell cell;
        cell.preferred_direction = (i * 2.0f * M_PI) / config_.head_direction_cells;
        cell.tuning_width = config_.hd_cell_width * M_PI / 180.0f;
        cell.activation = 0.0f;
        cell.angular_velocity_input = 0.0f;
        cognitive_map_.hd_cells.push_back(cell);
    }
    
    // Initialize boundary cells
    cognitive_map_.boundary_cells.reserve(config_.boundary_cell_count);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist_distance(10.0f, config_.boundary_detection_range);
    std::uniform_real_distribution<float> dist_direction(0.0f, 2.0f * M_PI);
    
    for (int i = 0; i < config_.boundary_cell_count; ++i) {
        BoundaryCell cell;
        cell.preferred_distance = dist_distance(gen);
        cell.preferred_direction = dist_direction(gen);
        cell.activation = 0.0f;
        cell.boundary_type = "wall";
        cognitive_map_.boundary_cells.push_back(cell);
    }
    
    // Initialize activity vectors
    grid_cell_activities_.resize(cognitive_map_.grid_cells.size(), 0.0f);
    hd_cell_activities_.resize(cognitive_map_.hd_cells.size(), 0.0f);
    
    cognitive_map_.map_coherence = 0.0f;
}

void SpatialNavigationBias::updateLocation(const SpatialLocation& location, float heading) {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    current_location_ = location;
    current_heading_ = heading;
    
    // Add to location history
    location_history_.push_back(location);
    if (location_history_.size() > 1000) {
        location_history_.pop_front();
    }
    
    // Update spatial cell populations
    updateGridCells(location);
    updatePlaceCells(location);
    
    // Calculate angular velocity from location history
    float angular_velocity = 0.0f;
    if (location_history_.size() >= 2) {
        auto prev_loc = location_history_[location_history_.size() - 2];
        float dt = (location.timestamp_ms - prev_loc.timestamp_ms) / 1000.0f;
        if (dt > 0.001f) {
            // Simplified angular velocity calculation
            float dx = location.x - prev_loc.x;
            float dy = location.y - prev_loc.y;
            if (std::sqrt(dx*dx + dy*dy) > 1.0f) {
                float new_heading = std::atan2(dy, dx);
                angular_velocity = (new_heading - current_heading_) / dt;
                // Normalize angular velocity
                while (angular_velocity > M_PI) angular_velocity -= 2.0f * M_PI;
                while (angular_velocity < -M_PI) angular_velocity += 2.0f * M_PI;
            }
        }
    }
    
    updateHeadDirectionCells(heading, angular_velocity);
    
    // Update cognitive map coherence
    formCognitiveMap();
}

void SpatialNavigationBias::updateGridCells(const SpatialLocation& location) {
    for (size_t i = 0; i < cognitive_map_.grid_cells.size(); ++i) {
        auto& cell = cognitive_map_.grid_cells[i];
        cell.activation = calculateGridCellActivation(cell, location);
        grid_cell_activities_[i] = cell.activation;
        
        // Update firing map
        int map_x = static_cast<int>((location.x + 250.0f) / cell.scale);
        int map_y = static_cast<int>((location.y + 250.0f) / cell.scale);
        if (map_x >= 0 && map_x < static_cast<int>(cell.firing_map.size()) &&
            map_y >= 0 && map_y < static_cast<int>(cell.firing_map[0].size())) {
            cell.firing_map[map_x][map_y] = std::max(cell.firing_map[map_x][map_y], 
                                                   cell.activation * config_.grid_cell_adaptation);
        }
    }
}

void SpatialNavigationBias::updatePlaceCells(const SpatialLocation& location) {
    // Check if we need to create new place cells
    bool found_active_cell = false;
    for (auto& cell : cognitive_map_.place_cells) {
        cell.activation = calculatePlaceCellActivation(cell, location);
        if (cell.activation > config_.place_cell_threshold) {
            found_active_cell = true;
            cell.is_active = true;
        } else {
            cell.is_active = false;
        }
    }
    
    // Create new place cell if needed and under limit
    if (!found_active_cell && cognitive_map_.place_cells.size() < static_cast<size_t>(config_.max_place_cells)) {
        PlaceCell new_cell;
        new_cell.center = location;
        new_cell.field_size = config_.place_field_size;
        new_cell.peak_rate = 1.0f;
        new_cell.activation = 1.0f;
        new_cell.is_active = true;
        cognitive_map_.place_cells.push_back(new_cell);
    }
}

void SpatialNavigationBias::updateHeadDirectionCells(float heading, float angular_velocity) {
    for (size_t i = 0; i < cognitive_map_.hd_cells.size(); ++i) {
        auto& cell = cognitive_map_.hd_cells[i];
        cell.activation = calculateHeadDirectionActivation(cell, heading);
        cell.angular_velocity_input = angular_velocity * config_.angular_velocity_gain;
        hd_cell_activities_[i] = cell.activation;
    }
}

void SpatialNavigationBias::updateBoundaryCells(const std::vector<float>& boundary_distances) {
    // Simplified boundary cell update - in real implementation would use actual sensor data
    for (auto& cell : cognitive_map_.boundary_cells) {
        float min_distance = std::numeric_limits<float>::max();
        for (float distance : boundary_distances) {
            min_distance = std::min(min_distance, distance);
        }
        
        if (min_distance < config_.boundary_detection_range) {
            float distance_diff = std::abs(min_distance - cell.preferred_distance);
            cell.activation = std::exp(-distance_diff * distance_diff / (2.0f * 50.0f * 50.0f));
        } else {
            cell.activation = 0.0f;
        }
    }
}

float SpatialNavigationBias::calculateGridCellActivation(const GridCell& cell, const SpatialLocation& location) {
    // Simplified grid cell model - hexagonal grid pattern
    float cos_orient = std::cos(cell.orientation);
    float sin_orient = std::sin(cell.orientation);
    
    // Transform coordinates to grid orientation
    float x_rot = location.x * cos_orient + location.y * sin_orient;
    float y_rot = -location.x * sin_orient + location.y * cos_orient;
    
    // Calculate grid activation using cosine gratings
    float grid_x = std::cos(2.0f * M_PI * x_rot / cell.scale + cell.phase_x);
    float grid_y = std::cos(2.0f * M_PI * y_rot / cell.scale + cell.phase_y);
    float grid_xy = std::cos(2.0f * M_PI * (x_rot + y_rot) / cell.scale);
    
    return std::max(0.0f, (grid_x + grid_y + grid_xy) / 3.0f);
}

float SpatialNavigationBias::calculatePlaceCellActivation(const PlaceCell& cell, const SpatialLocation& location) const {
    float dx = location.x - cell.center.x;
    float dy = location.y - cell.center.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    if (distance > cell.field_size) {
        return 0.0f;
    }
    
    // Gaussian place field
    float sigma = cell.field_size / 3.0f; // 3-sigma rule
    return cell.peak_rate * std::exp(-(distance * distance) / (2.0f * sigma * sigma));
}

float SpatialNavigationBias::calculateHeadDirectionActivation(const HeadDirectionCell& cell, float current_heading) {
    float angle_diff = current_heading - cell.preferred_direction;
    
    // Normalize angle difference
    while (angle_diff > M_PI) angle_diff -= 2.0f * M_PI;
    while (angle_diff < -M_PI) angle_diff += 2.0f * M_PI;
    
    // Von Mises distribution (circular normal)
    float kappa = 1.0f / (cell.tuning_width * cell.tuning_width);
    return std::exp(kappa * std::cos(angle_diff));
}

void SpatialNavigationBias::setGoal(const SpatialLocation& goal) {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    goal_location_ = goal;
    is_navigating_ = true;
    current_path_ = planPath(current_location_, goal_location_);
}

SpatialNavigationBias::SpatialPath SpatialNavigationBias::planPath(const SpatialLocation& start, const SpatialLocation& goal) {
    SpatialPath path;
    
    // Simple A* pathfinding implementation
    struct Node {
        SpatialLocation location;
        float g_cost; // Distance from start
        float h_cost; // Heuristic distance to goal
        float f_cost() const { return g_cost + h_cost; }
        Node* parent;
        
        Node(const SpatialLocation& loc, float g, float h, Node* p = nullptr)
            : location(loc), g_cost(g), h_cost(h), parent(p) {}
    };
    
    auto heuristic = [](const SpatialLocation& a, const SpatialLocation& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    };
    
    auto compare = [](const std::unique_ptr<Node>& a, const std::unique_ptr<Node>& b) {
        return a->f_cost() > b->f_cost();
    };
    
    std::priority_queue<std::unique_ptr<Node>, std::vector<std::unique_ptr<Node>>, decltype(compare)> open_set(compare);
    std::set<std::pair<int, int>> closed_set;
    
    open_set.push(std::make_unique<Node>(start, 0.0f, heuristic(start, goal)));
    
    int iterations = 0;
    const int max_iterations = config_.max_path_length;
    
    while (!open_set.empty() && iterations < max_iterations) {
        iterations++;
        auto current = std::move(const_cast<std::unique_ptr<Node>&>(open_set.top()));
        open_set.pop();
        
        int grid_x = static_cast<int>(current->location.x / config_.path_planning_resolution);
        int grid_y = static_cast<int>(current->location.y / config_.path_planning_resolution);
        
        if (closed_set.count({grid_x, grid_y})) {
            continue;
        }
        closed_set.insert({grid_x, grid_y});
        
        // Check if we reached the goal
        if (heuristic(current->location, goal) < config_.path_planning_resolution) {
            // Reconstruct path
            Node* node = current.get();
            while (node != nullptr) {
                path.waypoints.insert(path.waypoints.begin(), node->location);
                node = node->parent;
            }
            break;
        }
        
        // Generate neighbors (8-connected grid)
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                
                SpatialLocation neighbor;
                neighbor.x = current->location.x + dx * config_.path_planning_resolution;
                neighbor.y = current->location.y + dy * config_.path_planning_resolution;
                neighbor.confidence = 1.0f;
                neighbor.timestamp_ms = current->location.timestamp_ms;
                
                int neighbor_grid_x = static_cast<int>(neighbor.x / config_.path_planning_resolution);
                int neighbor_grid_y = static_cast<int>(neighbor.y / config_.path_planning_resolution);
                
                if (closed_set.count({neighbor_grid_x, neighbor_grid_y})) {
                    continue;
                }
                
                float move_cost = (dx != 0 && dy != 0) ? 1.414f : 1.0f; // Diagonal vs straight
                float tentative_g = current->g_cost + move_cost * config_.path_planning_resolution;
                
                auto neighbor_node = std::make_unique<Node>(neighbor, tentative_g, heuristic(neighbor, goal), current.get());
                open_set.push(std::move(neighbor_node));
            }
        }
    }
    
    // Calculate path properties
    path.total_distance = 0.0f;
    for (size_t i = 1; i < path.waypoints.size(); ++i) {
        path.total_distance += heuristic(path.waypoints[i-1], path.waypoints[i]);
    }
    
    path.estimated_time = path.total_distance / 100.0f; // Assume 1 m/s speed
    path.path_confidence = path.waypoints.empty() ? 0.0f : 0.8f;
    path.is_optimal = true; // A* finds optimal path
    
    return path;
}

void SpatialNavigationBias::formCognitiveMap() {
    // Calculate map coherence based on place cell and grid cell consistency
    float place_coherence = 0.0f;
    float grid_coherence = 0.0f;
    
    // Place cell coherence
    int active_place_cells = 0;
    for (const auto& cell : cognitive_map_.place_cells) {
        if (cell.is_active) {
            active_place_cells++;
            place_coherence += cell.activation;
        }
    }
    if (active_place_cells > 0) {
        place_coherence /= active_place_cells;
    }
    
    // Grid cell coherence
    float total_grid_activation = std::accumulate(grid_cell_activities_.begin(), grid_cell_activities_.end(), 0.0f);
    if (!grid_cell_activities_.empty()) {
        grid_coherence = total_grid_activation / grid_cell_activities_.size();
    }
    
    cognitive_map_.map_coherence = (place_coherence + grid_coherence) / 2.0f;
}

SpatialNavigationBias::SpatialPath SpatialNavigationBias::getNavigationPath() {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    return current_path_;
}

float SpatialNavigationBias::getDistanceToGoal() const {
    if (!is_navigating_) return 0.0f;
    
    float dx = goal_location_.x - current_location_.x;
    float dy = goal_location_.y - current_location_.y;
    return std::sqrt(dx * dx + dy * dy);
}

float SpatialNavigationBias::estimateDistance(const SpatialLocation& from, const SpatialLocation& to) const {
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float euclidean_distance = std::sqrt(dx * dx + dy * dy);
    
    // Add some noise to simulate biological estimation error
    float error_factor = 1.0f + (1.0f - config_.distance_estimation_accuracy) * 
                        (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 0.4f;
    
    return euclidean_distance * error_factor;
}

float SpatialNavigationBias::estimateDirection(const SpatialLocation& from, const SpatialLocation& to) const {
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    return std::atan2(dy, dx);
}

bool SpatialNavigationBias::isLocationFamiliar(const SpatialLocation& location) const {
    // Check if any place cell is active at this location
    for (const auto& cell : cognitive_map_.place_cells) {
        float activation = calculatePlaceCellActivation(cell, location);
        if (activation > config_.place_cell_threshold) {
            return true;
        }
    }
    return false;
}

std::vector<SpatialNavigationBias::Landmark> SpatialNavigationBias::getNearbyLandmarks(const SpatialLocation& location, float radius) const {
    std::vector<Landmark> nearby;
    
    for (const auto& landmark : cognitive_map_.landmarks) {
        float distance = estimateDistance(location, landmark.location);
        if (distance <= radius) {
            nearby.push_back(landmark);
        }
    }
    
    return nearby;
}

std::vector<float> SpatialNavigationBias::getPlaceCellActivations() const {
    std::vector<float> activations;
    activations.reserve(cognitive_map_.place_cells.size());
    
    for (const auto& cell : cognitive_map_.place_cells) {
        activations.push_back(cell.activation);
    }
    
    return activations;
}

std::vector<float> SpatialNavigationBias::getGridCellActivations() const {
    return grid_cell_activities_;
}

std::vector<float> SpatialNavigationBias::getHeadDirectionActivations() const {
    return hd_cell_activities_;
}

std::vector<float> SpatialNavigationBias::getBoundaryCellActivations() const {
    std::vector<float> activations;
    activations.reserve(cognitive_map_.boundary_cells.size());
    
    for (const auto& cell : cognitive_map_.boundary_cells) {
        activations.push_back(cell.activation);
    }
    
    return activations;
}

void SpatialNavigationBias::addLandmark(const Landmark& landmark) {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    
    if (cognitive_map_.landmarks.size() < static_cast<size_t>(config_.max_landmarks)) {
        cognitive_map_.landmarks.push_back(landmark);
    }
}

SpatialNavigationBias::SpatialLocation SpatialNavigationBias::getNextNavigationStep() const {
    if (!is_navigating_ || current_path_.waypoints.empty()) {
        return current_location_;
    }
    
    // Find the next waypoint that's not too close
    for (const auto& waypoint : current_path_.waypoints) {
        float distance = estimateDistance(current_location_, waypoint);
        if (distance > config_.path_following_precision) {
            return waypoint;
        }
    }
    
    // If all waypoints are close, return the goal
    return goal_location_;
}

bool SpatialNavigationBias::hasReachedGoal(float tolerance) const {
    if (!is_navigating_) return false;
    return getDistanceToGoal() <= tolerance;
}

void SpatialNavigationBias::recalculatePath() {
    if (is_navigating_) {
        current_path_ = planPath(current_location_, goal_location_);
    }
}

void SpatialNavigationBias::reinforceLocation(const SpatialLocation& location, float reward) {
    // Strengthen place cells at this location
    for (auto& cell : cognitive_map_.place_cells) {
        float activation = calculatePlaceCellActivation(cell, location);
        if (activation > 0.1f) {
            cell.peak_rate += reward * config_.spatial_learning_rate * activation;
            cell.peak_rate = std::min(cell.peak_rate, 2.0f); // Cap maximum rate
        }
    }
}

void SpatialNavigationBias::adaptPlaceCells(const SpatialLocation& location) {
    // Update place cell activations based on current location
    for (auto& cell : cognitive_map_.place_cells) {
        float activation = calculatePlaceCellActivation(cell, location);
        cell.activation = activation;
        
        // Adapt place field properties based on experience
        if (activation > config_.place_cell_threshold) {
            // Strengthen the place field at this location
            float learning_rate = 0.01f;
            float distance = std::sqrt(std::pow(cell.center.x - location.x, 2) + 
                                     std::pow(cell.center.y - location.y, 2));
            
            if (distance < cell.field_size) {
                // Move center slightly towards current location
                cell.center.x += learning_rate * (location.x - cell.center.x);
                cell.center.y += learning_rate * (location.y - cell.center.y);
                
                // Adjust field size based on activation strength
                if (activation > 0.8f) {
                    cell.field_size *= 0.99f; // Slightly reduce field size for strong activation
                } else if (activation < 0.3f) {
                    cell.field_size *= 1.01f; // Slightly increase field size for weak activation
                }
                
                // Ensure field size stays within reasonable bounds
                cell.field_size = std::max(5.0f, std::min(cell.field_size, 50.0f));
            }
        }
    }
}

void SpatialNavigationBias::updateConfig(const Config& new_config) {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    config_ = new_config;
}

SpatialNavigationBias::Config SpatialNavigationBias::getConfig() const {
    return config_;
}

SpatialNavigationBias::CognitiveMap SpatialNavigationBias::getCognitiveMap() const {
    return cognitive_map_;
}

SpatialNavigationBias::SpatialLocation SpatialNavigationBias::getCurrentLocation() const {
    return current_location_;
}

float SpatialNavigationBias::getCurrentHeading() const {
    return current_heading_;
}

float SpatialNavigationBias::getSpatialCoherence() const {
    return cognitive_map_.map_coherence;
}

float SpatialNavigationBias::getNavigationConfidence() const {
    if (!is_navigating_) return 0.0f;
    return current_path_.path_confidence;
}

void SpatialNavigationBias::resetNavigation() {
    std::lock_guard<std::mutex> lock(bias_mutex_);
    is_navigating_ = false;
    current_path_ = SpatialPath{};
    goal_location_ = SpatialLocation{};
}

} // namespace Biases
} // namespace NeuroForge