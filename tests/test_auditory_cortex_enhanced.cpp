#include "regions/CorticalRegions.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cassert>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void test_pitch_detection() {
    std::cout << "Testing Pitch Detection (440Hz Sine Wave)..." << std::endl;
    
    NeuroForge::Regions::AuditoryCortex ac("AuditoryCortex", 1000);
    
    // Generate 440Hz sine wave at 16kHz sample rate
    // 1024 samples (power of 2)
    const int sample_rate = 16000;
    const int num_samples = 1024;
    std::vector<float> audio(num_samples);
    
    for (int i = 0; i < num_samples; ++i) {
        float t = (float)i / sample_rate;
        audio[i] = 0.8f * std::sin(2.0f * (float)M_PI * 440.0f * t);
    }
    
    ac.processAudioInput(audio);
    
    auto features = ac.getDetectedFeatures();
    
    bool found_pitch = false;
    for (auto f : features) {
        if (f == NeuroForge::Regions::AuditoryCortex::SoundFeature::Pitch) {
            found_pitch = true;
            break;
        }
    }
    
    if (found_pitch) {
        std::cout << "PASS: Detected Pitch." << std::endl;
    } else {
        std::cout << "FAIL: Did not detect Pitch." << std::endl;
        // Debug info could go here
    }
}

void test_phoneme_detection() {
    std::cout << "Testing Phoneme Detection (Formant-like signal)..." << std::endl;
    
    NeuroForge::Regions::AuditoryCortex ac("AuditoryCortex", 1000);
    
    // Generate signal with two formants: 500Hz and 1500Hz (like 'a' vowel)
    const int sample_rate = 16000;
    const int num_samples = 1024;
    std::vector<float> audio(num_samples);
    
    for (int i = 0; i < num_samples; ++i) {
        float t = (float)i / sample_rate;
        audio[i] = 0.5f * std::sin(2.0f * (float)M_PI * 500.0f * t) + 
                   0.4f * std::sin(2.0f * (float)M_PI * 1500.0f * t);
    }
    
    ac.processAudioInput(audio);
    
    auto features = ac.getDetectedFeatures();
    
    bool found_phoneme = false;
    for (auto f : features) {
        if (f == NeuroForge::Regions::AuditoryCortex::SoundFeature::Phoneme) {
            found_phoneme = true;
            break;
        }
    }
    
    if (found_phoneme) {
        std::cout << "PASS: Detected Phoneme." << std::endl;
    } else {
        std::cout << "FAIL: Did not detect Phoneme." << std::endl;
    }
}

int main() {
    test_pitch_detection();
    test_phoneme_detection();
    return 0;
}
