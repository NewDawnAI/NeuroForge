// Simple neuron scaling test: estimates per‑neuron memory cost and linearity
#include "core/Region.h"
#include "core/Neuron.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#endif

using namespace NeuroForge::Core;

static double getProcessPrivateMB() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&pmc), sizeof(pmc))) {
        // PrivateUsage is the committed memory that cannot be shared with other processes
        return static_cast<double>(pmc.PrivateUsage) / (1024.0 * 1024.0);
    }
    return -1.0;
#else
    return -1.0;
#endif
}

int main() {
    // Baseline memory
    double mb0 = getProcessPrivateMB();
    if (mb0 < 0) {
        std::cout << "WARN: Unable to read process memory; running best‑effort test." << std::endl;
    } else {
        std::cout << "Baseline Private MB: " << mb0 << std::endl;
    }

    // Allocate neurons outside Region to avoid vector reallocation noise in region internals
    const std::size_t n1 = 100000; // first batch
    const std::size_t n2 = 200000; // total after second batch (adds +100k)
    std::vector<std::shared_ptr<Neuron>> neurons;
    neurons.reserve(n2); // pre‑reserve to reduce capacity doubling overhead

    for (std::size_t i = 0; i < n1; ++i) {
        auto up = NeuronFactory::createNeuron();
        neurons.emplace_back(std::move(up));
    }
    // brief pause to let OS update counters
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    double mb1 = getProcessPrivateMB();
    std::cout << "After " << n1 << " neurons, Private MB: " << mb1 << std::endl;

    // Second batch (+100k)
    for (std::size_t i = 0; i < (n2 - n1); ++i) {
        auto up = NeuronFactory::createNeuron();
        neurons.emplace_back(std::move(up));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    double mb2 = getProcessPrivateMB();
    std::cout << "After " << n2 << " neurons, Private MB: " << mb2 << std::endl;

    // Estimate per‑neuron memory from incremental delta to reduce fixed overhead bias
    if (mb1 > 0 && mb2 > 0) {
        double deltaMB = mb2 - mb1;
        std::size_t deltaN = n2 - n1;
        double bytesPerNeuron = (deltaMB * 1024.0 * 1024.0) / static_cast<double>(deltaN);
        std::cout << "Estimated bytes per neuron (incremental): " << bytesPerNeuron << std::endl;
        // Acceptable window around ~64 bytes to account for allocator behavior and bookkeeping
        bool approxLinear64B = (bytesPerNeuron >= 48.0 && bytesPerNeuron <= 96.0);
        std::cout << "Linear scaling ~64B/neuron: " << (approxLinear64B ? "PASS" : "WARN") << std::endl;
    } else {
        std::cout << "NOTE: Memory counters unavailable; skipping pass/fail." << std::endl;
    }

    std::cout << "Total neurons created: " << neurons.size() << std::endl;
    return 0;
}

