#include "biases/VoiceBias.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

using namespace NeuroForge::Biases;

void testBasicFunctionality() {
    std::cout << "Testing basic functionality..." << std::endl;

    VoiceBias::Config config;
    config.phoneme_recognition_threshold = -1.0f; // Force voice detection
    VoiceBias bias(config);

    std::vector<float> features(10, 1.0f);
    std::vector<float> audio_data(1024, 0.1f);
    float sample_rate = 44100.0f;
    int grid_size = 8;

    // First call - should store features
    bool result = bias.applyVoiceBias(features, audio_data, sample_rate, grid_size);
    assert(result == true);

    auto recent = bias.getRecentVoiceFeatures(10);
    assert(recent.size() == 1);

    // Fill up the history
    for (int i = 0; i < 25; ++i) {
        bias.applyVoiceBias(features, audio_data, sample_rate, grid_size);
    }

    recent = bias.getRecentVoiceFeatures(30);
    // It should be limited to 20 as per the implementation
    std::cout << "Recent features size: " << recent.size() << " (expected 20)" << std::endl;
    assert(recent.size() == 20);

    // Check max_history parameter
    auto recent_limited = bias.getRecentVoiceFeatures(5);
    assert(recent_limited.size() == 5);

    std::cout << "✓ Basic functionality test passed" << std::endl;
}

void testReset() {
    std::cout << "Testing reset..." << std::endl;

    VoiceBias::Config config;
    config.phoneme_recognition_threshold = -1.0f; // Force voice detection
    VoiceBias bias(config);
    std::vector<float> features(10, 1.0f);
    std::vector<float> audio_data(1024, 0.1f);

    bias.applyVoiceBias(features, audio_data, 44100.0f, 8);
    assert(bias.getRecentVoiceFeatures(10).size() > 0);

    bias.reset();
    assert(bias.getRecentVoiceFeatures(10).size() == 0);

    std::cout << "✓ Reset test passed" << std::endl;
}

int main() {
    try {
        testBasicFunctionality();
        testReset();
        std::cout << "All tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
