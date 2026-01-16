#include "core/Region.h"
#include "core/Neuron.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <thread>

using namespace NeuroForge::Core;

int main() {
    // 1. Create Region
    auto region = RegionFactory::createRegion("BenchRegion", Region::Type::Cortical, Region::ActivationPattern::Asynchronous);

    // 2. Populate with neurons
    const size_t NEURON_COUNT = 20000;
    std::cout << "Creating " << NEURON_COUNT << " neurons..." << std::endl;
    region->createNeurons(NEURON_COUNT);

    // Activate the region!
    region->initialize();

    // Warmup
    std::cout << "Warming up..." << std::endl;
    for(int i=0; i<10; ++i) region->process(0.016f);

    const int ITERATIONS = 1000;
    std::cout << "Benchmarking process() over " << ITERATIONS << " iterations..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        // Simulate 60fps frame time
        region->process(0.016f);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    double total_ms = elapsed.count();
    double avg_ms = total_ms / ITERATIONS;

    std::cout << "Total time: " << total_ms << " ms" << std::endl;
    std::cout << "Average time per call: " << avg_ms << " ms" << std::endl;

    return 0;
}
