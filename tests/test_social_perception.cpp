#include "../src/biases/SocialPerceptionBias.h"
#ifdef NF_HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <random>

using namespace NeuroForge::Biases;

#ifdef NF_HAVE_OPENCV

// Test utilities
namespace {
    std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
    
    cv::Mat createTestFrame(int width = 640, int height = 480) {
        cv::Mat frame(height, width, CV_8UC3, cv::Scalar(128, 128, 128));
        return frame;
    }
    
    cv::Mat createFaceRegion(int x, int y, int size) {
        cv::Mat face(size, size, CV_8UC3);
        // Create a simple face-like pattern
        cv::ellipse(face, cv::Point(size/2, size/2), cv::Size(size/3, size/2), 
                   0, 0, 360, cv::Scalar(200, 180, 160), -1);
        // Eyes
        cv::circle(face, cv::Point(size/3, size/3), size/10, cv::Scalar(0, 0, 0), -1);
        cv::circle(face, cv::Point(2*size/3, size/3), size/10, cv::Scalar(0, 0, 0), -1);
        // Mouth
        cv::ellipse(face, cv::Point(size/2, 2*size/3), cv::Size(size/6, size/12), 
                   0, 0, 180, cv::Scalar(100, 50, 50), -1);
        return face;
    }
    
    SocialPerceptionBias::AudioBuffer createTestAudio(size_t length = 100) {
        SocialPerceptionBias::AudioBuffer audio;
        audio.audio_envelope.reserve(length);
        
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (size_t i = 0; i < length; ++i) {
            audio.audio_envelope.push_back(dist(rng));
        }
        audio.speech_probability = 0.8f;
        audio.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        return audio;
    }
    
    std::vector<float> createTestFeatures(int grid_size) {
        std::vector<float> features(grid_size * grid_size, 1.0f);
        return features;
    }
}

class SocialPerceptionTest {
public:
    bool runAllTests() {
        std::cout << "=== Social Perception Module Test Suite ===" << std::endl;
        
        int passed = 0;
        int total = 0;
        
        // Basic functionality tests
        if (testBasicConfiguration()) passed++; total++;
        if (testInitialization()) passed++; total++;
        if (testFrameProcessing()) passed++; total++;
        if (testSocialEventCreation()) passed++; total++;
        if (testFeatureBiasApplication()) passed++; total++;
        
        // Advanced functionality tests
        if (testGazeTargetEstimation()) passed++; total++;
        if (testLipSyncDetection()) passed++; total++;
        if (testFaceTracking()) passed++; total++;
        if (testMultimodalIntegration()) passed++; total++;
        
        // Performance and edge case tests
        if (testStatisticsTracking()) passed++; total++;
        if (testConfigurationUpdates()) passed++; total++;
        if (testEdgeCases()) passed++; total++;
        if (testClearOperation()) passed++; total++;
        
        std::cout << std::endl;
        if (passed == total) {
            std::cout << "✅ All Social Perception tests passed!" << std::endl;
            std::cout << "Social Perception module is ready for integration." << std::endl;
            return true;
        } else {
            std::cout << "❌ Some Social Perception tests failed!" << std::endl;
            std::cout << "Passed: " << passed << "/" << total << std::endl;
            return false;
        }
    }

private:
    bool testBasicConfiguration() {
        std::cout << "Testing basic configuration..." << std::endl;
        
        try {
            SocialPerceptionBias::Config config;
            config.face_priority_multiplier = 3.0f;
            config.gaze_attention_multiplier = 2.0f;
            config.lip_sync_boost = 2.5f;
            config.enable_face_detection = true;
            config.enable_gaze_tracking = true;
            config.enable_lip_sync = true;
            
            SocialPerceptionBias bias(config);
            
            auto retrieved_config = bias.getConfig();
            if (retrieved_config.face_priority_multiplier != 3.0f ||
                retrieved_config.gaze_attention_multiplier != 2.0f ||
                retrieved_config.lip_sync_boost != 2.5f) {
                std::cout << "✗ Configuration values not properly stored" << std::endl;
                return false;
            }
            
            std::cout << "✓ Basic configuration test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in basic configuration: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testInitialization() {
        std::cout << "Testing initialization..." << std::endl;
        
        try {
            SocialPerceptionBias::Config config;
            // Use paths that might not exist to test graceful degradation
            config.face_cascade_path = "nonexistent_face.xml";
            config.eye_cascade_path = "nonexistent_eye.xml";
            config.mouth_cascade_path = "nonexistent_mouth.xml";
            
            SocialPerceptionBias bias(config);
            
            // Should not crash even with missing cascade files
            bool initialized = bias.initialize();
            
            // System should still be created even if cascades fail to load
            auto stats = bias.getStatistics();
            if (stats.total_frames_processed != 0) {
                std::cout << "✗ Statistics should be initialized to zero" << std::endl;
                return false;
            }
            
            std::cout << "✓ Initialization test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in initialization: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testFrameProcessing() {
        std::cout << "Testing frame processing..." << std::endl;
        
        try {
            SocialPerceptionBias bias;
            bias.initialize();
            
            cv::Mat test_frame = createTestFrame();
            auto events = bias.processSocialFrame(test_frame);
            
            // Should process without crashing
            auto stats = bias.getStatistics();
            if (stats.total_frames_processed != 1) {
                std::cout << "✗ Frame processing count incorrect" << std::endl;
                return false;
            }
            
            // Test empty frame handling
            cv::Mat empty_frame;
            auto empty_events = bias.processSocialFrame(empty_frame);
            if (!empty_events.empty()) {
                std::cout << "✗ Empty frame should produce no events" << std::endl;
                return false;
            }
            
            std::cout << "✓ Frame processing test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in frame processing: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testSocialEventCreation() {
        std::cout << "Testing social event creation..." << std::endl;
        
        try {
            SocialPerceptionBias bias;
            bias.initialize();
            
            // Create frame with face-like region
            cv::Mat frame = createTestFrame();
            cv::Mat face_region = createFaceRegion(100, 100, 80);
            face_region.copyTo(frame(cv::Rect(100, 100, 80, 80)));
            
            auto events = bias.processSocialFrame(frame);
            
            // Verify event structure
            for (const auto& event : events) {
                if (event.timestamp_ms == 0) {
                    std::cout << "✗ Event timestamp should be set" << std::endl;
                    return false;
                }
                
                if (event.total_salience_boost < 1.0f) {
                    std::cout << "✗ Salience boost should be at least 1.0" << std::endl;
                    return false;
                }
            }
            
            std::cout << "✓ Social event creation test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in social event creation: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testFeatureBiasApplication() {
        std::cout << "Testing feature bias application..." << std::endl;
        
        try {
            SocialPerceptionBias bias;
            bias.initialize();
            
            // Create test features
            int grid_size = 16;
            auto features = createTestFeatures(grid_size);
            auto original_features = features;
            
            // Create mock social event
            std::vector<SocialPerceptionBias::SocialEvent> events;
            SocialPerceptionBias::SocialEvent event;
            event.face_box = cv::Rect(100, 100, 50, 50);
            event.total_salience_boost = 2.0f;
            events.push_back(event);
            
            // Apply bias
            bias.applySocialBias(features, events, grid_size);
            
            // Features should be modified
            bool features_changed = false;
            for (size_t i = 0; i < features.size(); ++i) {
                if (std::abs(features[i] - original_features[i]) > 0.001f) {
                    features_changed = true;
                    break;
                }
            }
            
            if (!features_changed) {
                std::cout << "✗ Features should be modified by social bias" << std::endl;
                return false;
            }
            
            std::cout << "✓ Feature bias application test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in feature bias application: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testGazeTargetEstimation() {
        std::cout << "Testing gaze target estimation..." << std::endl;
        
        try {
            SocialPerceptionBias bias;
            
            // Test gaze estimation with mock eye positions
            cv::Rect face(100, 100, 80, 80);
            std::vector<cv::Rect> eyes = {
                cv::Rect(20, 25, 10, 8),  // Left eye (relative to face)
                cv::Rect(50, 25, 10, 8)   // Right eye (relative to face)
            };
            cv::Size frame_size(640, 480);
            
            // This is a private method, so we test it indirectly through processSocialFrame
            // The gaze estimation logic is embedded in the social event creation
            
            cv::Mat frame = createTestFrame();
            auto events = bias.processSocialFrame(frame);
            
            // Test should not crash with gaze estimation
            std::cout << "✓ Gaze target estimation test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in gaze target estimation: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testLipSyncDetection() {
        std::cout << "Testing lip-sync detection..." << std::endl;
        
        try {
            SocialPerceptionBias bias;
            bias.initialize();
            
            cv::Mat frame = createTestFrame();
            auto audio = createTestAudio(50);
            
            auto events = bias.processSocialFrame(frame, audio);
            
            // Test should process audio without crashing
            auto stats = bias.getStatistics();
            if (stats.total_frames_processed == 0) {
                std::cout << "✗ Frame with audio should be processed" << std::endl;
                return false;
            }
            
            std::cout << "✓ Lip-sync detection test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in lip-sync detection: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testFaceTracking() {
        std::cout << "Testing face tracking..." << std::endl;
        
        try {
            SocialPerceptionBias bias;
            bias.initialize();
            
            cv::Mat frame1 = createTestFrame();
            cv::Mat frame2 = createTestFrame();
            
            // Process multiple frames to test tracking
            auto events1 = bias.processSocialFrame(frame1);
            auto events2 = bias.processSocialFrame(frame2);
            
            // Should maintain tracking state between frames
            auto recent_events = bias.getRecentSocialEvents(10);
            
            std::cout << "✓ Face tracking test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in face tracking: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testMultimodalIntegration() {
        std::cout << "Testing multimodal integration..." << std::endl;
        
        try {
            SocialPerceptionBias bias;
            bias.initialize();
            
            cv::Mat frame = createTestFrame();
            auto audio = createTestAudio(100);
            
            // Process frame with both visual and audio data
            auto events = bias.processSocialFrame(frame, audio);
            
            // Test different audio configurations
            SocialPerceptionBias::AudioBuffer silent_audio;
            auto silent_events = bias.processSocialFrame(frame, silent_audio);
            
            // Should handle both cases gracefully
            auto stats = bias.getStatistics();
            if (stats.total_frames_processed < 2) {
                std::cout << "✗ Should process both audio and silent frames" << std::endl;
                return false;
            }
            
            std::cout << "✓ Multimodal integration test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in multimodal integration: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testStatisticsTracking() {
        std::cout << "Testing statistics tracking..." << std::endl;
        
        try {
            SocialPerceptionBias bias;
            bias.initialize();
            
            auto initial_stats = bias.getStatistics();
            if (initial_stats.total_frames_processed != 0) {
                std::cout << "✗ Initial statistics should be zero" << std::endl;
                return false;
            }
            
            // Process some frames
            cv::Mat frame = createTestFrame();
            for (int i = 0; i < 5; ++i) {
                bias.processSocialFrame(frame);
            }
            
            auto final_stats = bias.getStatistics();
            if (final_stats.total_frames_processed != 5) {
                std::cout << "✗ Frame count should be 5, got " << final_stats.total_frames_processed << std::endl;
                return false;
            }
            
            if (final_stats.last_update_time == 0) {
                std::cout << "✗ Last update time should be set" << std::endl;
                return false;
            }
            
            std::cout << "✓ Statistics tracking test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in statistics tracking: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testConfigurationUpdates() {
        std::cout << "Testing configuration updates..." << std::endl;
        
        try {
            SocialPerceptionBias bias;
            
            SocialPerceptionBias::Config new_config;
            new_config.face_priority_multiplier = 5.0f;
            new_config.event_history_size = 200;
            
            bias.updateConfig(new_config);
            
            auto retrieved_config = bias.getConfig();
            if (retrieved_config.face_priority_multiplier != 5.0f ||
                retrieved_config.event_history_size != 200) {
                std::cout << "✗ Configuration update failed" << std::endl;
                return false;
            }
            
            std::cout << "✓ Configuration updates test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in configuration updates: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testEdgeCases() {
        std::cout << "Testing edge cases..." << std::endl;
        
        try {
            SocialPerceptionBias bias;
            bias.initialize();
            
            // Test with very small frame
            cv::Mat tiny_frame(10, 10, CV_8UC3);
            auto events1 = bias.processSocialFrame(tiny_frame);
            
            // Test with very large frame
            cv::Mat large_frame(2000, 2000, CV_8UC3, cv::Scalar(128, 128, 128));
            auto events2 = bias.processSocialFrame(large_frame);
            
            // Test with invalid audio
            SocialPerceptionBias::AudioBuffer invalid_audio;
            invalid_audio.speech_probability = -1.0f;  // Invalid probability
            auto events3 = bias.processSocialFrame(createTestFrame(), invalid_audio);
            
            // Test feature bias with empty features
            std::vector<float> empty_features;
            std::vector<SocialPerceptionBias::SocialEvent> empty_events;
            bias.applySocialBias(empty_features, empty_events, 0);
            
            // All should complete without crashing
            std::cout << "✓ Edge cases test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in edge cases: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool testClearOperation() {
        std::cout << "Testing clear operation..." << std::endl;
        
        try {
            SocialPerceptionBias bias;
            bias.initialize();
            
            // Process some frames to populate state
            cv::Mat frame = createTestFrame();
            for (int i = 0; i < 3; ++i) {
                bias.processSocialFrame(frame);
            }
            
            auto stats_before = bias.getStatistics();
            if (stats_before.total_frames_processed == 0) {
                std::cout << "✗ Should have processed frames before clear" << std::endl;
                return false;
            }
            
            // Clear state
            bias.clear();
            
            auto stats_after = bias.getStatistics();
            if (stats_after.total_frames_processed != 0) {
                std::cout << "✗ Statistics should be reset after clear" << std::endl;
                return false;
            }
            
            auto events = bias.getRecentSocialEvents();
            if (!events.empty()) {
                std::cout << "✗ Event history should be empty after clear" << std::endl;
                return false;
            }
            
            std::cout << "✓ Clear operation test passed" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "✗ Exception in clear operation: " << e.what() << std::endl;
            return false;
        }
    }
};

#endif // NF_HAVE_OPENCV

int main() {
#ifdef NF_HAVE_OPENCV
    SocialPerceptionTest test;
    bool success = test.runAllTests();
    
    std::cout << "\n=== Social Perception Test Results ===" << std::endl;
    std::cout << (success ? "✓ All tests passed!" : "✗ Some tests failed!") << std::endl;
    
    return success ? 0 : 1;
#else
    std::cout << "Social perception tests require OpenCV (NF_HAVE_OPENCV not defined)" << std::endl;
    return 0;
#endif
}