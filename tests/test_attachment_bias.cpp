#include "biases/AttachmentBias.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

#ifdef NF_HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif

using namespace NeuroForge::Biases;

void testBasicAttachmentFunctionality() {
    std::cout << "Testing basic attachment functionality..." << std::endl;
    
    AttachmentBias::Config config;
    config.bonding_learning_rate = 0.1f;
    config.separation_distress_threshold = 5.0f; // 5 seconds for testing
    
    AttachmentBias attachment(config);
    
    // Test initial state
    auto metrics = attachment.calculateAttachmentMetrics();
    assert(metrics.caregiver_recognition_strength == 0.0f);
    assert(metrics.social_bonding_strength == 0.0f);
    assert(!attachment.isInSeparationDistress());
    
    std::cout << "✓ Basic attachment functionality test passed" << std::endl;
}

#ifdef NF_HAVE_OPENCV
void testCaregiverRegistration() {
    std::cout << "Testing caregiver registration..." << std::endl;
    
    AttachmentBias::Config config;
    AttachmentBias attachment(config);
    
    // Create mock face template and voice features
    cv::Mat face_template = cv::Mat::ones(64, 64, CV_8UC1) * 128;
    std::vector<float> voice_features = {0.5f, 0.3f, 0.8f, 0.2f, 0.6f};
    
    // Register primary caregiver
    attachment.registerCaregiver("caregiver_1", face_template, voice_features, true);
    
    // Verify registration
    const auto* profile = attachment.getCaregiverProfile("caregiver_1");
    assert(profile != nullptr);
    assert(profile->caregiver_id == "caregiver_1");
    assert(profile->is_primary_caregiver == true);
    assert(profile->bonding_strength > 0.0f);
    
    // Register secondary caregiver
    cv::Mat face_template2 = cv::Mat::ones(64, 64, CV_8UC1) * 64;
    std::vector<float> voice_features2 = {0.2f, 0.7f, 0.4f, 0.9f, 0.1f};
    
    attachment.registerCaregiver("caregiver_2", face_template2, voice_features2, false);
    
    const auto* profile2 = attachment.getCaregiverProfile("caregiver_2");
    assert(profile2 != nullptr);
    assert(profile2->is_primary_caregiver == false);
    assert(profile2->bonding_strength < profile->bonding_strength);
    
    std::cout << "✓ Caregiver registration test passed" << std::endl;
}

void testSocialInteractionProcessing() {
    std::cout << "Testing social interaction processing..." << std::endl;
    
    AttachmentBias::Config config;
    AttachmentBias attachment(config);
    
    // Register caregiver first
    cv::Mat face_template = cv::Mat::ones(64, 64, CV_8UC1) * 128;
    std::vector<float> voice_features = {0.5f, 0.3f, 0.8f, 0.2f, 0.6f};
    attachment.registerCaregiver("caregiver_1", face_template, voice_features, true);
    
    // Get initial bonding strength
    const auto* initial_profile = attachment.getCaregiverProfile("caregiver_1");
    float initial_bonding = initial_profile->bonding_strength;
    
    // Create positive interaction
    AttachmentBias::SocialInteraction interaction;
    interaction.caregiver_id = "caregiver_1";
    interaction.face_location = cv::Rect(100, 100, 50, 50);
    interaction.voice_features = voice_features;
    interaction.interaction_valence = 0.8f; // Positive interaction
    interaction.proximity_distance = 1.0f;
    interaction.interaction_duration = 30.0f;
    interaction.timestamp = std::chrono::steady_clock::now();
    interaction.interaction_type = "comfort";
    
    // Process interaction
    attachment.processSocialInteraction(interaction);
    
    // Verify bonding strength increased
    const auto* updated_profile = attachment.getCaregiverProfile("caregiver_1");
    assert(updated_profile->bonding_strength > initial_bonding);
    assert(updated_profile->interaction_frequency > 0.0f);
    
    // Test negative interaction
    interaction.interaction_valence = -0.5f;
    float pre_negative_bonding = updated_profile->bonding_strength;
    attachment.processSocialInteraction(interaction);
    
    const auto* post_negative_profile = attachment.getCaregiverProfile("caregiver_1");
    assert(post_negative_profile->bonding_strength < pre_negative_bonding);
    
    std::cout << "✓ Social interaction processing test passed" << std::endl;
}

void testAttachmentBiasApplication() {
    std::cout << "Testing attachment bias application..." << std::endl;
    
    AttachmentBias::Config config;
    AttachmentBias attachment(config);
    
    // Register caregiver
    cv::Mat face_template = cv::Mat::ones(64, 64, CV_8UC1) * 128;
    std::vector<float> voice_features = {0.5f, 0.3f, 0.8f, 0.2f, 0.6f};
    attachment.registerCaregiver("caregiver_1", face_template, voice_features, true);
    
    // Create feature vector
    int grid_size = 10;
    std::vector<float> features(grid_size * grid_size, 0.5f);
    std::vector<float> original_features = features;
    
    // Create face locations
    std::vector<cv::Rect> face_locations = {cv::Rect(320, 240, 100, 100)};
    
    // Apply attachment bias
    bool success = attachment.applyAttachmentBias(features, face_locations, voice_features, grid_size);
    assert(success);
    
    // Verify features were modified
    bool features_changed = false;
    for (size_t i = 0; i < features.size(); ++i) {
        if (std::abs(features[i] - original_features[i]) > 1e-6f) {
            features_changed = true;
            break;
        }
    }
    assert(features_changed);
    
    std::cout << "✓ Attachment bias application test passed" << std::endl;
}

void testSeparationDistress() {
    std::cout << "Testing separation distress..." << std::endl;
    
    AttachmentBias::Config config;
    config.separation_distress_threshold = 1.0f; // 1 second for testing
    config.enable_separation_distress = true;
    
    AttachmentBias attachment(config);
    
    // Register caregiver
    cv::Mat face_template = cv::Mat::ones(64, 64, CV_8UC1) * 128;
    std::vector<float> voice_features = {0.5f, 0.3f, 0.8f, 0.2f, 0.6f};
    attachment.registerCaregiver("caregiver_1", face_template, voice_features, true);
    
    // Initially no separation distress
    assert(!attachment.isInSeparationDistress());
    
    // Process interaction to reset timer
    AttachmentBias::SocialInteraction interaction;
    interaction.caregiver_id = "caregiver_1";
    interaction.interaction_valence = 0.5f;
    interaction.timestamp = std::chrono::steady_clock::now();
    attachment.processSocialInteraction(interaction);
    
    assert(!attachment.isInSeparationDistress());
    
    // Wait for separation distress threshold
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    // Apply bias to trigger distress check
    std::vector<float> features(100, 0.5f);
    std::vector<cv::Rect> face_locations;
    attachment.applyAttachmentBias(features, face_locations, voice_features, 10);
    
    // Should now be in separation distress
    assert(attachment.isInSeparationDistress());
    
    std::cout << "✓ Separation distress test passed" << std::endl;
}

void testStrangerWariness() {
    std::cout << "Testing stranger wariness..." << std::endl;
    
    AttachmentBias::Config config;
    config.enable_stranger_anxiety = true;
    config.stranger_wariness_threshold = 0.7f;
    
    AttachmentBias attachment(config);
    
    // Register known caregiver
    cv::Mat face_template = cv::Mat::ones(64, 64, CV_8UC1) * 128;
    std::vector<float> voice_features = {0.5f, 0.3f, 0.8f, 0.2f, 0.6f};
    attachment.registerCaregiver("caregiver_1", face_template, voice_features, true);
    
    // Test wariness for unknown face
    cv::Rect unknown_face(200, 150, 80, 80);
    float wariness = attachment.getStrangerWariness(unknown_face);
    assert(wariness > 0.0f);
    assert(wariness <= 1.0f);
    
    std::cout << "✓ Stranger wariness test passed" << std::endl;
}

void testAttachmentMetrics() {
    std::cout << "Testing attachment metrics calculation..." << std::endl;
    
    AttachmentBias::Config config;
    AttachmentBias attachment(config);
    
    // Register caregiver
    cv::Mat face_template = cv::Mat::ones(64, 64, CV_8UC1) * 128;
    std::vector<float> voice_features = {0.5f, 0.3f, 0.8f, 0.2f, 0.6f};
    attachment.registerCaregiver("caregiver_1", face_template, voice_features, true);
    
    // Process several positive interactions
    for (int i = 0; i < 5; ++i) {
        AttachmentBias::SocialInteraction interaction;
        interaction.caregiver_id = "caregiver_1";
        interaction.interaction_valence = 0.8f;
        interaction.proximity_distance = 1.5f;
        interaction.timestamp = std::chrono::steady_clock::now();
        interaction.interaction_type = "comfort";
        
        attachment.processSocialInteraction(interaction);
    }
    
    // Calculate metrics
    auto metrics = attachment.calculateAttachmentMetrics();
    
    // Verify metrics are reasonable
    assert(metrics.caregiver_recognition_strength > 0.0f);
    assert(metrics.caregiver_recognition_strength <= 1.0f);
    assert(metrics.social_bonding_strength > 0.0f);
    assert(metrics.social_bonding_strength <= 1.0f);
    assert(metrics.voice_familiarity >= 0.0f);
    assert(metrics.voice_familiarity <= 1.0f);
    assert(metrics.attachment_security >= 0.0f);
    assert(metrics.attachment_security <= 1.0f);
    assert(metrics.proximity_preference >= 0.0f);
    assert(metrics.proximity_preference <= 1.0f);
    
    std::cout << "✓ Attachment metrics calculation test passed" << std::endl;
}

void testAttachmentReset() {
    std::cout << "Testing attachment system reset..." << std::endl;
    
    AttachmentBias::Config config;
    AttachmentBias attachment(config);
    
    // Register caregiver and process interactions
    cv::Mat face_template = cv::Mat::ones(64, 64, CV_8UC1) * 128;
    std::vector<float> voice_features = {0.5f, 0.3f, 0.8f, 0.2f, 0.6f};
    attachment.registerCaregiver("caregiver_1", face_template, voice_features, true);
    
    AttachmentBias::SocialInteraction interaction;
    interaction.caregiver_id = "caregiver_1";
    interaction.interaction_valence = 0.8f;
    interaction.timestamp = std::chrono::steady_clock::now();
    attachment.processSocialInteraction(interaction);
    
    // Verify system has data
    assert(attachment.getCaregiverProfile("caregiver_1") != nullptr);
    assert(!attachment.getInteractionHistory().empty());
    
    // Reset system
    attachment.reset();
    
    // Verify system is cleared
    assert(attachment.getCaregiverProfile("caregiver_1") == nullptr);
    assert(attachment.getInteractionHistory().empty());
    
    auto metrics = attachment.calculateAttachmentMetrics();
    assert(metrics.caregiver_recognition_strength == 0.0f);
    assert(metrics.social_bonding_strength == 0.0f);
    
    std::cout << "✓ Attachment system reset test passed" << std::endl;
}
#endif // NF_HAVE_OPENCV

int main() {
    std::cout << "Running AttachmentBias tests..." << std::endl;
    
    try {
        testBasicAttachmentFunctionality();
#ifdef NF_HAVE_OPENCV
        testCaregiverRegistration();
        testSocialInteractionProcessing();
        testAttachmentBiasApplication();
        testSeparationDistress();
        testStrangerWariness();
        testAttachmentMetrics();
        testAttachmentReset();
#else
        std::cout << "OpenCV-dependent tests skipped (OpenCV not available)" << std::endl;
#endif
        
        std::cout << "\n✅ All AttachmentBias tests passed successfully!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "\n❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
}