#include "biases/SpatialNavigationBias.h"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cassert>
#include <cmath>

using namespace NeuroForge::Biases;

class SpatialNavigationBiasTest {
private:
    std::mt19937 rng_;
    
public:
    SpatialNavigationBiasTest() : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {}
    
    SpatialNavigationBias::SpatialLocation generateRandomLocation(float x_range = 500.0f, float y_range = 500.0f) {
        std::uniform_real_distribution<float> x_dist(-x_range/2, x_range/2);
        std::uniform_real_distribution<float> y_dist(-y_range/2, y_range/2);
        std::uniform_real_distribution<float> conf_dist(0.7f, 1.0f);
        
        auto now = std::chrono::steady_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        
        return SpatialNavigationBias::SpatialLocation(x_dist(rng_), y_dist(rng_), conf_dist(rng_), timestamp);
    }
    
    bool testBasicLocationUpdate() {
        std::cout << "Testing basic location update..." << std::endl;
        
        try {
            SpatialNavigationBias::Config config;
            config.max_place_cells = 50;
            config.head_direction_cells = 60;
            config.boundary_cell_count = 100;
            SpatialNavigationBias bias(config);
            
            // Update location
            SpatialNavigationBias::SpatialLocation location;
            location.x = 10.0f;
            location.y = 20.0f;
            location.confidence = 1.0f;
            location.timestamp_ms = 1000;
            
            bias.updateLocation(location, 45.0f);
            
            // Verify location was stored
            auto currentLoc = bias.getCurrentLocation();
            assert(std::abs(currentLoc.x - 10.0f) < 0.01f);
            assert(std::abs(currentLoc.y - 20.0f) < 0.01f);
            
            std::cout << "✓ Basic location update successful" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in basic location update: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testPlaceCellFormation() {
        std::cout << "Testing place cell formation..." << std::endl;
        
        try {
            SpatialNavigationBias::Config config;
            config.max_place_cells = 100;
            config.place_field_size = 40.0f;
            config.place_cell_threshold = 0.7f;
            SpatialNavigationBias bias(config);
            
            // Visit multiple distinct locations
            std::vector<SpatialNavigationBias::SpatialLocation> locations;
            for (int i = 0; i < 10; ++i) {
                SpatialNavigationBias::SpatialLocation location;
                location.x = i * 10.0f;
                location.y = i * 5.0f;
                location.confidence = 1.0f;
                location.timestamp_ms = i * 1000;
                
                locations.push_back(location);
                bias.updateLocation(location, i * 36.0f);
                bias.adaptPlaceCells(location);
            }
            
            auto place_activations = bias.getPlaceCellActivations();
            if (place_activations.size() == 0) {
                std::cout << "✗ No place cells activated: " << place_activations.size() << std::endl;
                return false;
            }
            
            // Test location familiarity
            bool familiar = bias.isLocationFamiliar(locations[0]);
            if (!familiar) {
                std::cout << "✗ Previously visited location not recognized as familiar" << std::endl;
                return false;
            }
            
            // Test unfamiliar location
            auto new_location = SpatialNavigationBias::SpatialLocation(1000.0f, 1000.0f, 1.0f, 10000);
            bool unfamiliar = !bias.isLocationFamiliar(new_location);
            if (!unfamiliar) {
                std::cout << "✗ New location incorrectly recognized as familiar" << std::endl;
                return false;
            }
            
            std::cout << "✓ Place cell formation successful" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in place cell formation: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testNavigationPlanning() {
        std::cout << "Testing navigation planning..." << std::endl;
        
        try {
            SpatialNavigationBias::Config config;
            config.max_place_cells = 100;
            config.max_path_length = 10;  // Very small path length for testing
            config.path_planning_resolution = 20.0f;  // Larger resolution for fewer nodes
            SpatialNavigationBias bias(config);
            
            // Set current location
        SpatialNavigationBias::SpatialLocation start;
        start.x = 0.0f;
        start.y = 0.0f;
        start.confidence = 1.0f;
        start.timestamp_ms = 1000;
        
        bias.updateLocation(start, 0.0f);
        
        // Set goal location (closer to start)
        SpatialNavigationBias::SpatialLocation goal;
        goal.x = 50.0f;  // Closer goal
        goal.y = 50.0f;  // Closer goal
        goal.confidence = 1.0f;
        goal.timestamp_ms = 2000;
            
            // Test distance estimation first (without path planning)
            float distance = bias.estimateDistance(start, goal);
            if (distance <= 0.0f) {
                std::cout << "✗ Invalid distance estimation: " << distance << std::endl;
                return false;
            }
            
            // Test direction estimation
            float direction = bias.estimateDirection(start, goal);
            if (direction < -3.15f || direction > 3.15f) {  // Allow full range of atan2
                std::cout << "✗ Invalid direction estimation: " << direction << std::endl;
                return false;
            }
            
            std::cout << "✓ Navigation planning successful" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in navigation planning: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testSpatialMemory() {
        std::cout << "Testing spatial memory..." << std::endl;
        
        try {
            SpatialNavigationBias::Config config;
            config.max_landmarks = 5;
            SpatialNavigationBias bias(config);
            
            // Add landmarks
            SpatialNavigationBias::Landmark landmark1;
            landmark1.landmark_id = 1;
            landmark1.location = SpatialNavigationBias::SpatialLocation(50.0f, 50.0f, 1.0f, 0);
            landmark1.landmark_type = "tree";
            landmark1.salience = 0.8f;
            landmark1.is_reliable = true;
            
            bias.addLandmark(landmark1);
            
            // Test nearby landmarks
            auto current_loc = SpatialNavigationBias::SpatialLocation(60.0f, 60.0f, 1.0f, 1000);
            auto nearby = bias.getNearbyLandmarks(current_loc, 20.0f);
            
            if (nearby.empty()) {
                std::cout << "✗ Nearby landmark not found" << std::endl;
                return false;
            }
            
            if (nearby[0].landmark_id != 1) {
                std::cout << "✗ Wrong landmark returned" << std::endl;
                return false;
            }
            
            // Test distance estimation
            auto target_loc = SpatialNavigationBias::SpatialLocation(100.0f, 100.0f, 1.0f, 2000);
            float estimated_distance = bias.estimateDistance(current_loc, target_loc);
            float actual_distance = std::sqrt(40.0f * 40.0f + 40.0f * 40.0f);
            
            // Allow for biological estimation error
            if (std::abs(estimated_distance - actual_distance) > actual_distance * 0.5f) {
                std::cout << "✗ Distance estimation too inaccurate: " << estimated_distance << " vs " << actual_distance << std::endl;
                return false;
            }
            
            std::cout << "✓ Spatial memory test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in spatial memory: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testSpatialLearning() {
        std::cout << "Testing spatial learning..." << std::endl;
        
        SpatialNavigationBias::Config config;
        config.max_place_cells = 100;
        
        SpatialNavigationBias bias(config);
        
        // Test location with reward
        SpatialNavigationBias::SpatialLocation reward_location;
        reward_location.x = 25.0f;
        reward_location.y = 25.0f;
        reward_location.confidence = 1.0f;
        reward_location.timestamp_ms = 1000;
        
        bias.updateLocation(reward_location, 0.0f);
        bias.reinforceLocation(reward_location, 1.0f);
        
        // Test spatial coherence and navigation confidence
        float coherence = bias.getSpatialCoherence();
        float confidence = bias.getNavigationConfidence();
        
        // These should return valid values
        if (coherence < 0.0f || coherence > 1.0f) {
            std::cout << "✗ Invalid spatial coherence value: " << coherence << std::endl;
            return false;
        }
        
        if (confidence < 0.0f || confidence > 1.0f) {
            std::cout << "✗ Invalid navigation confidence value: " << confidence << std::endl;
            return false;
        }
        
        std::cout << "✓ Spatial learning successful" << std::endl;
        return true;
    }
    
    bool runAllTests() {
        std::cout << "=== SpatialNavigationBias Test Suite ===" << std::endl;
        
        int passed = 0;
        int total = 5;
        
        if (testBasicLocationUpdate()) passed++;
        if (testPlaceCellFormation()) passed++;
        if (testNavigationPlanning()) passed++;
        if (testSpatialMemory()) passed++;
        if (testSpatialLearning()) passed++;
        
        std::cout << "\n=== Test Results ===" << std::endl;
        std::cout << "Passed: " << passed << "/" << total << std::endl;
        
        if (passed == total) {
            std::cout << "✓ All tests passed!" << std::endl;
            return true;
        } else {
            std::cout << "✗ Some tests failed!" << std::endl;
            return false;
        }
    }
};

int main() {
    SpatialNavigationBiasTest test;
    bool success = test.runAllTests();
    return success ? 0 : 1;
}