#include "biases/VoiceBias.h"
#include <iostream>
#include <vector>
#include <deque>
#include <chrono>
#include <algorithm>

using namespace NeuroForge::Biases;

const int ITERATIONS = 100000;
const int HISTORY_SIZE = 20;

void benchmarkVector() {
    std::vector<VoiceBias::VoiceFeatures> recent_features;
    recent_features.reserve(HISTORY_SIZE);

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        VoiceBias::VoiceFeatures vf;
        vf.voice_probability = static_cast<float>(i) / ITERATIONS;

        recent_features.push_back(vf);
        if (recent_features.size() > HISTORY_SIZE) {
            recent_features.erase(recent_features.begin());
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Vector (erase begin): " << elapsed.count() << " ms" << std::endl;
}

void benchmarkDeque() {
    std::deque<VoiceBias::VoiceFeatures> recent_features;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        VoiceBias::VoiceFeatures vf;
        vf.voice_probability = static_cast<float>(i) / ITERATIONS;

        recent_features.push_back(vf);
        if (recent_features.size() > HISTORY_SIZE) {
            recent_features.pop_front();
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Deque (pop_front):    " << elapsed.count() << " ms" << std::endl;
}

int main() {
    std::cout << "Benchmarking usage pattern: push_back + remove from front (size=" << HISTORY_SIZE << ")" << std::endl;
    std::cout << "Iterations: " << ITERATIONS << std::endl;

    // Warmup
    benchmarkVector();
    benchmarkDeque();

    std::cout << "\nResults:" << std::endl;
    benchmarkVector();
    benchmarkDeque();

    return 0;
}
