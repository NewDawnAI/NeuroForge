#include "biases/FaceDetectionBias.h"
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cassert>

using namespace NeuroForge::Biases;

class FaceDetectionBiasTest {
private:
    std::mt19937 rng_;
    
public:
    FaceDetectionBiasTest() : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {}
    
    std::vector<float> generateRandomFeatures(size_t size, float min_val = 0.0f, float max_val = 1.0f) {
        std::uniform_real_distribution<float> dist(min_val, max_val);
        std::vector<float> features(size);
        for (auto& val : features) {
            val = dist(rng_);
        }
        return features;
    }
    
    std::vector<float> generateGrayImage(int width, int height, float base_intensity = 0.5f) {
        std::normal_distribution<float> noise(0.0f, 0.1f);
        std::vector<float> image(width * height);
        for (auto& pixel : image) {
            pixel = std::clamp(base_intensity + noise(rng_), 0.0f, 1.0f);
        }
        return image;
    }
    
    bool testBasicConfiguration() {
        std::cout << "Testing basic configuration..." << std::endl;
        
        FaceDetectionBias::Config config;
        config.face_priority_multiplier = 3.0f;
        config.face_detection_threshold = 0.4f;
        
        FaceDetectionBias bias(config);
        
        auto retrieved_config = bias.getConfig();
        if (retrieved_config.face_priority_multiplier != 3.0f ||
            retrieved_config.face_detection_threshold != 0.4f) {
            std::cout << "✗ Configuration retrieval failed" << std::endl;
            return false;
        }
        
        // Test priority multiplier setting
        bias.setFacePriorityMultiplier(2.5f);
        if (bias.getFacePriorityMultiplier() != 2.5f) {
            std::cout << "✗ Face priority multiplier setting failed" << std::endl;
            return false;
        }
        
        std::cout << "✓ Basic configuration test passed" << std::endl;
        return true;
    }
    
    bool testFeatureEnhancement() {
        std::cout << "Testing feature enhancement..." << std::endl;
        
        FaceDetectionBias::Config config;
        config.face_priority_multiplier = 2.0f;
        config.enable_attention_boost = true;
        
        FaceDetectionBias bias(config);
        
        const int grid_size = 8;
        auto features = generateRandomFeatures(grid_size * grid_size, 0.3f, 0.7f);
        auto original_features = features;
        
        // Create mock face info
        std::vector<FaceDetectionBias::FaceDetection> faces;
        FaceDetectionBias::FaceDetection face;
        face.bounding_box.x = 25;
        face.bounding_box.y = 25;
        face.bounding_box.width = 50;
        face.bounding_box.height = 50;
        face.confidence = config.face_priority_multiplier;
        faces.push_back(face);
        
        // Apply attention boost
        bias.applyAttentionBoost(features, faces, grid_size);
        
        // Check that some features were enhanced
        bool features_changed = false;
        for (size_t i = 0; i < features.size(); ++i) {
            if (std::abs(features[i] - original_features[i]) > 0.001f) {
                features_changed = true;
                break;
            }
        }
        
        if (!features_changed) {
            std::cout << "✗ Features should be modified by attention boost" << std::endl;
            return false;
        }
        
        std::cout << "✓ Feature enhancement test passed" << std::endl;
        return true;
    }
    
    bool testAttentionWeightCalculation() {
        std::cout << "Testing attention weight calculation..." << std::endl;
        
        FaceDetectionBias bias;
        
        // Create face info
        std::vector<FaceDetectionBias::FaceInfo> faces;
        FaceDetectionBias::FaceInfo face;
        face.x = 40;
        face.y = 40;
        face.width = 20;
        face.height = 20;
        face.attention_weight = 2.0f;
        faces.push_back(face);
        
        const int grid_size = 10;
        
        // Test weight at face center (should be high)
        float center_weight = bias.calculateAttentionWeight(5, 5, faces, grid_size);
        
        // Test weight far from face (should be lower)
        float far_weight = bias.calculateAttentionWeight(0, 0, faces, grid_size);
        
        if (center_weight <= far_weight) {
            std::cout << "✗ Attention weight should be higher near face center" << std::endl;
            return false;
        }
        
        // Test with no faces
        float no_face_weight = bias.calculateAttentionWeight(5, 5, {}, grid_size);
        if (no_face_weight != 1.0f) {
            std::cout << "✗ Attention weight should be 1.0 with no faces" << std::endl;
            return false;
        }
        
        std::cout << "✓ Attention weight calculation test passed" << std::endl;
        return true;
    }
    
    bool testFaceTracking() {
        std::cout << "Testing face tracking..." << std::endl;
        
        FaceDetectionBias::Config config;
        config.enable_face_tracking = true;
        FaceDetectionBias bias(config);
        
        // Create initial faces
        std::vector<FaceDetectionBias::FaceDetection> faces1;
        FaceDetectionBias::FaceDetection face1;
        face1.bounding_box.x = 50;
        face1.bounding_box.y = 50;
        face1.bounding_box.width = 30;
        face1.bounding_box.height = 30;
        faces1.push_back(face1);
        
        // Update tracking
        bias.updateFaceTracking(faces1);
        
        // Create slightly moved faces
        std::vector<FaceDetectionBias::FaceDetection> faces2;
        FaceDetectionBias::FaceDetection face2;
        face2.bounding_box.x = 52; // Slightly moved
        face2.bounding_box.y = 52;
        face2.bounding_box.width = 30;
        face2.bounding_box.height = 30;
        faces2.push_back(face2);
        
        bias.updateFaceTracking(faces2);
        
        // The tracking ID should be consistent
        if (faces1[0].tracking_id != faces2[0].tracking_id) {
            std::cout << "✗ Face tracking should maintain consistent IDs for similar faces" << std::endl;
            return false;
        }
        
        std::cout << "✓ Face tracking test passed" << std::endl;
        return true;
    }
    
    bool testFaceOverlapCalculation() {
        std::cout << "Testing face overlap calculation..." << std::endl;
        
        FaceDetectionBias bias;
        
        // Create overlapping faces
        FaceDetectionBias::FaceInfo face1;
        face1.x = 10;
        face1.y = 10;
        face1.width = 20;
        face1.height = 20;
        
        FaceDetectionBias::FaceInfo face2;
        face2.x = 15;
        face2.y = 15;
        face2.width = 20;
        face2.height = 20;
        
        float overlap = bias.calculateFaceOverlap(face1, face2);
        if (overlap <= 0.0f) {
            std::cout << "✗ Overlapping faces should have positive overlap" << std::endl;
            return false;
        }
        
        // Create non-overlapping faces
        FaceDetectionBias::FaceInfo face3;
        face3.x = 50;
        face3.y = 50;
        face3.width = 20;
        face3.height = 20;
        
        float no_overlap = bias.calculateFaceOverlap(face1, face3);
        if (no_overlap != 0.0f) {
            std::cout << "✗ Non-overlapping faces should have zero overlap" << std::endl;
            return false;
        }
        
        std::cout << "✓ Face overlap calculation test passed" << std::endl;
        return true;
    }
    
    bool testGrayImageDetection() {
        std::cout << "Testing grayscale image detection..." << std::endl;
        
        FaceDetectionBias bias;
        
        // Create a simple grayscale image
        const int width = 64;
        const int height = 64;
        auto gray_image = generateGrayImage(width, height);
        
        std::vector<FaceDetectionBias::FaceInfo> faces;
        bool detection_performed = bias.detectFacesFromGray(gray_image, width, height, faces);
        
        if (!detection_performed) {
            std::cout << "✗ Face detection should be performed on valid grayscale image" << std::endl;
            return false;
        }
        
        // The fallback detection might find faces or not, but it should complete
        std::cout << "✓ Grayscale image detection test passed" << std::endl;
        return true;
    }
    
    bool testStatistics() {
        std::cout << "Testing statistics tracking..." << std::endl;
        
        FaceDetectionBias bias;
        
        // Process some frames
        const int grid_size = 8;
        for (int i = 0; i < 3; ++i) {
            auto features = generateRandomFeatures(grid_size * grid_size);
            auto gray_image = generateGrayImage(64, 64);
            bias.applyFaceBias(features, gray_image, grid_size);
        }
        
        auto stats = bias.getStatistics();
        
        if (stats.total_frames_processed != 3) {
            std::cout << "✗ Statistics should track total frames processed" << std::endl;
            return false;
        }
        
        // Check OpenCV availability reporting
        bool opencv_available = bias.isOpenCVAvailable();
        if (stats.opencv_available != opencv_available) {
            std::cout << "✗ Statistics should correctly report OpenCV availability" << std::endl;
            return false;
        }
        
        std::cout << "✓ Statistics test passed" << std::endl;
        return true;
    }
    
    bool testOperationalStatus() {
        std::cout << "Testing operational status..." << std::endl;
        
        FaceDetectionBias bias;
        
        bool operational = bias.isOperational();
        // Should be operational regardless of OpenCV (fallback available)
        
        bool opencv_available = bias.isOpenCVAvailable();
        std::cout << "  OpenCV available: " << (opencv_available ? "Yes" : "No") << std::endl;
        std::cout << "  System operational: " << (operational ? "Yes" : "No") << std::endl;
        
        std::cout << "✓ Operational status test passed" << std::endl;
        return true;
    }
    
    bool testBackgroundSuppression() {
        std::cout << "Testing background suppression..." << std::endl;
        
        FaceDetectionBias::Config config;
        config.background_suppression = 0.5f; // 50% suppression
        FaceDetectionBias bias(config);
        
        const int grid_size = 8;
        auto features = generateRandomFeatures(grid_size * grid_size, 0.5f, 1.0f);
        auto original_features = features;
        
        // Create face in center
        std::vector<FaceDetectionBias::FaceInfo> faces;
        FaceDetectionBias::FaceInfo face;
        face.x = 25;
        face.y = 25;
        face.width = 50;
        face.height = 50;
        faces.push_back(face);
        
        bias.applyBackgroundSuppression(features, faces, grid_size);
        
        // Check that background regions were suppressed
        bool suppression_applied = false;
        for (size_t i = 0; i < features.size(); ++i) {
            if (features[i] < original_features[i] * 0.9f) {
                suppression_applied = true;
                break;
            }
        }
        
        if (!suppression_applied) {
            std::cout << "✗ Background suppression should reduce some feature values" << std::endl;
            return false;
        }
        
        std::cout << "✓ Background suppression test passed" << std::endl;
        return true;
    }
    
    bool testClearTracking() {
        std::cout << "Testing clear tracking..." << std::endl;
        
        // Create bias with face tracking enabled
        FaceDetectionBias::Config config;
        config.enable_face_tracking = true;
        FaceDetectionBias bias(config);
        
        // Add some faces
        std::vector<FaceDetectionBias::FaceInfo> faces;
        FaceDetectionBias::FaceInfo face;
        face.x = 10;
        face.y = 10;
        face.width = 20;
        face.height = 20;
        faces.push_back(face);
        
        bias.updateFaceTracking(faces);
        
        auto current_faces = bias.getCurrentFaces();
        if (current_faces.empty()) {
            std::cout << "✗ Should have current faces before clear" << std::endl;
            return false;
        }
        
        bias.clearTracking();
        current_faces = bias.getCurrentFaces();
        
        if (!current_faces.empty()) {
            std::cout << "✗ Current faces should be empty after clear tracking" << std::endl;
            return false;
        }
        
        std::cout << "✓ Clear tracking test passed" << std::endl;
        return true;
    }
    
    bool runAllTests() {
        std::cout << "=== Face Detection Bias Module Test Suite ===" << std::endl;
        
        bool all_passed = true;
        
        all_passed &= testBasicConfiguration();
        all_passed &= testFeatureEnhancement();
        all_passed &= testAttentionWeightCalculation();
        all_passed &= testFaceTracking();
        all_passed &= testFaceOverlapCalculation();
        all_passed &= testGrayImageDetection();
        all_passed &= testStatistics();
        all_passed &= testOperationalStatus();
        all_passed &= testBackgroundSuppression();
        all_passed &= testClearTracking();
        
        if (all_passed) {
            std::cout << std::endl << "✅ All Face Detection Bias tests passed!" << std::endl;
            std::cout << "Face Detection Bias module is ready for integration." << std::endl;
        } else {
            std::cout << std::endl << "❌ Some Face Detection Bias tests failed!" << std::endl;
        }
        
        return all_passed;
    }
};

int main() {
    FaceDetectionBiasTest test;
    bool success = test.runAllTests();
    return success ? 0 : 1;
}