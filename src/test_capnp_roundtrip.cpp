// Cap'n Proto Save/Load Round-Trip Test
// Tests both .nfbin (raw Brain) and .capnp (BrainStateFile wrapper) formats
#include "core/HypergraphBrain.h"
#include "connectivity/ConnectivityManager.h"
#include "core/LearningSystem.h"
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <cmath>
#include <cassert>

using namespace NeuroForge::Core;

struct BrainSnapshot {
    std::string brain_state;
    std::string processing_mode;
    float target_frequency;
    uint64_t processing_cycles;
    size_t region_count;
    size_t total_neurons;
    size_t total_synapses;
    float global_activation;
    // Per-region data
    struct RegionInfo {
        uint32_t id;
        std::string name;
        size_t neuron_count;
    };
    std::vector<RegionInfo> regions;
};

BrainSnapshot takeBrainSnapshot(const HypergraphBrain& brain) {
    BrainSnapshot snap;
    snap.brain_state = brain.getBrainStateString();
    snap.processing_mode = brain.getProcessingModeString();
    snap.target_frequency = brain.getTargetFrequency();
    snap.processing_cycles = brain.getProcessingCycles();
    
    auto stats = brain.getGlobalStatistics();
    snap.region_count = stats.total_regions;
    snap.total_neurons = stats.total_neurons;
    snap.total_synapses = stats.total_synapses;
    snap.global_activation = stats.global_activation;
    
    for (const auto& [rid, region] : brain.getRegions()) {
        if (region) {
            snap.regions.push_back({rid, region->getName(), region->getNeuronCount()});
        }
    }
    // Sort by ID for stable comparison
    std::sort(snap.regions.begin(), snap.regions.end(),
              [](const BrainSnapshot::RegionInfo& a, const BrainSnapshot::RegionInfo& b) {
                  return a.id < b.id;
              });
    return snap;
}

bool compareSnapshots(const BrainSnapshot& before, const BrainSnapshot& after, const std::string& label) {
    bool ok = true;
    auto check = [&](const std::string& field, auto a, auto b) {
        if (a != b) {
            std::cerr << "  MISMATCH [" << label << "] " << field << ": " << a << " vs " << b << std::endl;
            ok = false;
        }
    };
    auto checkFloat = [&](const std::string& field, float a, float b) {
        if (std::abs(a - b) > 1e-4f) {
            std::cerr << "  MISMATCH [" << label << "] " << field << ": " << a << " vs " << b << std::endl;
            ok = false;
        }
    };

    check("region_count", before.region_count, after.region_count);
    check("total_neurons", before.total_neurons, after.total_neurons);
    check("total_synapses", before.total_synapses, after.total_synapses);
    check("processing_cycles", before.processing_cycles, after.processing_cycles);
    checkFloat("target_frequency", before.target_frequency, after.target_frequency);

    if (before.regions.size() == after.regions.size()) {
        for (size_t i = 0; i < before.regions.size(); ++i) {
            check("region[" + std::to_string(i) + "].id", before.regions[i].id, after.regions[i].id);
            check("region[" + std::to_string(i) + "].name", before.regions[i].name, after.regions[i].name);
            check("region[" + std::to_string(i) + "].neuron_count", before.regions[i].neuron_count, after.regions[i].neuron_count);
        }
    }

    return ok;
}

int main() {
    namespace fs = std::filesystem;
    int failures = 0;

    std::cout << "=== Cap'n Proto Round-Trip Test ===" << std::endl;
    std::cout << std::endl;

    // 1. Create and initialize a brain with some structure
    std::cout << "[1] Creating brain with regions, neurons, synapses, and learning..." << std::endl;
    auto conn_mgr = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
    auto brain = std::make_shared<HypergraphBrain>(conn_mgr, 50.0f);
    brain->setProcessingMode(HypergraphBrain::ProcessingMode::Sequential);
    brain->setRandomSeed(12345);
    
    if (!brain->initialize()) {
        std::cerr << "FATAL: Brain initialize failed" << std::endl;
        return 1;
    }

    // Create some regions with neurons
    auto sensory = brain->createRegion("Sensory", NeuroForge::Core::Region::Type::Cortical);
    auto motor = brain->createRegion("Motor", NeuroForge::Core::Region::Type::Cortical);
    auto cognitive = brain->createRegion("Cognitive", NeuroForge::Core::Region::Type::Custom);

    // Add neurons to each region
    if (sensory) sensory->createNeurons(16);
    if (motor) motor->createNeurons(16);
    if (cognitive) cognitive->createNeurons(16);

    // Connect regions
    if (sensory && cognitive) {
        brain->connectRegions(sensory->getId(), cognitive->getId(), 0.3f);
    }
    if (cognitive && motor) {
        brain->connectRegions(cognitive->getId(), motor->getId(), 0.3f);
    }

    // Initialize learning
    LearningSystem::Config lconf{};
    lconf.global_learning_rate = 0.01f;
    lconf.hebbian_rate = 0.001f;
    lconf.stdp_rate = 0.001f;
    brain->initializeLearning(lconf);
    brain->setLearningEnabled(true);

    // Run some processing steps to build up state
    std::cout << "[2] Running 50 processing steps..." << std::endl;
    for (int step = 0; step < 50; ++step) {
        brain->processStep(0.02f); // 50 Hz
    }

    // Take snapshot of pre-save state
    auto before_snap = takeBrainSnapshot(*brain);
    std::cout << "  Brain state before save:" << std::endl;
    std::cout << "    Regions: " << before_snap.region_count << std::endl;
    std::cout << "    Neurons: " << before_snap.total_neurons << std::endl;
    std::cout << "    Synapses: " << before_snap.total_synapses << std::endl;
    std::cout << "    Cycles: " << before_snap.processing_cycles << std::endl;
    std::cout << "    Frequency: " << before_snap.target_frequency << " Hz" << std::endl;
    std::cout << std::endl;

    // ======= TEST A: .nfbin (raw Brain Cap'n Proto) =======
    std::cout << "[3] TEST A: Save brain as .nfbin (raw Cap'n Proto)..." << std::endl;
    const std::string nfbin_path = "test_roundtrip.nfbin";
    bool save_ok = brain->saveCheckpoint(nfbin_path);
    if (!save_ok) {
        std::cerr << "  FAIL: saveCheckpoint(.nfbin) returned false" << std::endl;
        ++failures;
    } else {
        auto actual_path = fs::path("BrainState") / nfbin_path;
        if (fs::exists(actual_path)) {
            auto fsize = fs::file_size(actual_path);
            std::cout << "  Saved: " << actual_path.string() << " (" << fsize << " bytes)" << std::endl;
        } else {
            std::cout << "  Saved (path may differ)" << std::endl;
        }

        // Load into a fresh brain
        std::cout << "[4] Loading .nfbin into a fresh brain..." << std::endl;
        auto brain2 = std::make_shared<HypergraphBrain>(conn_mgr, 100.0f);
        brain2->setProcessingMode(HypergraphBrain::ProcessingMode::Parallel);
        brain2->initialize();

        bool load_ok = brain2->loadCheckpoint(nfbin_path);
        if (!load_ok) {
            std::cerr << "  FAIL: loadCheckpoint(.nfbin) returned false" << std::endl;
            ++failures;
        } else {
            auto after_snap = takeBrainSnapshot(*brain2);
            std::cout << "  Brain state after load:" << std::endl;
            std::cout << "    Regions: " << after_snap.region_count << std::endl;
            std::cout << "    Neurons: " << after_snap.total_neurons << std::endl;
            std::cout << "    Synapses: " << after_snap.total_synapses << std::endl;
            std::cout << "    Cycles: " << after_snap.processing_cycles << std::endl;
            std::cout << "    Frequency: " << after_snap.target_frequency << " Hz" << std::endl;

            if (compareSnapshots(before_snap, after_snap, "nfbin")) {
                std::cout << "  PASS: .nfbin round-trip verified!" << std::endl;
            } else {
                std::cerr << "  FAIL: .nfbin round-trip data mismatch" << std::endl;
                ++failures;
            }
        }
    }
    std::cout << std::endl;

    // ======= TEST B: .capnp (BrainStateFile wrapper) =======
    std::cout << "[5] TEST B: Save brain as .capnp (BrainStateFile wrapper)..." << std::endl;
    const std::string capnp_path = "test_roundtrip.capnp";
    save_ok = brain->saveCheckpoint(capnp_path);
    if (!save_ok) {
        std::cerr << "  FAIL: saveCheckpoint(.capnp) returned false" << std::endl;
        ++failures;
    } else {
        auto actual_path = fs::path("BrainState") / capnp_path;
        if (fs::exists(actual_path)) {
            auto fsize = fs::file_size(actual_path);
            std::cout << "  Saved: " << actual_path.string() << " (" << fsize << " bytes)" << std::endl;
        } else {
            std::cout << "  Saved (path may differ)" << std::endl;
        }

        std::cout << "[6] Loading .capnp into a fresh brain..." << std::endl;
        auto brain3 = std::make_shared<HypergraphBrain>(conn_mgr, 100.0f);
        brain3->initialize();

        bool load_ok = brain3->loadCheckpoint(capnp_path);
        if (!load_ok) {
            std::cerr << "  FAIL: loadCheckpoint(.capnp) returned false" << std::endl;
            ++failures;
        } else {
            auto after_snap = takeBrainSnapshot(*brain3);
            std::cout << "  Brain state after load:" << std::endl;
            std::cout << "    Regions: " << after_snap.region_count << std::endl;
            std::cout << "    Neurons: " << after_snap.total_neurons << std::endl;
            std::cout << "    Synapses: " << after_snap.total_synapses << std::endl;
            std::cout << "    Cycles: " << after_snap.processing_cycles << std::endl;

            if (compareSnapshots(before_snap, after_snap, "capnp")) {
                std::cout << "  PASS: .capnp round-trip verified!" << std::endl;
            } else {
                std::cerr << "  FAIL: .capnp round-trip data mismatch" << std::endl;
                ++failures;
            }
        }
    }
    std::cout << std::endl;

    // ======= TEST C: JSON baseline comparison =======
    std::cout << "[7] TEST C: Save brain as .json (baseline comparison)..." << std::endl;
    const std::string json_path = "test_roundtrip.json";
    save_ok = brain->saveCheckpoint(json_path);
    if (!save_ok) {
        std::cerr << "  FAIL: saveCheckpoint(.json) returned false" << std::endl;
        ++failures;
    } else {
        auto actual_path = fs::path("BrainState") / json_path;
        if (fs::exists(actual_path)) {
            auto fsize = fs::file_size(actual_path);
            std::cout << "  Saved: " << actual_path.string() << " (" << fsize << " bytes)" << std::endl;
            
            // Compare sizes
            auto nfbin_actual = fs::path("BrainState") / nfbin_path;
            auto capnp_actual = fs::path("BrainState") / capnp_path;
            if (fs::exists(nfbin_actual) && fs::exists(capnp_actual)) {
                auto json_sz = fs::file_size(actual_path);
                auto nfbin_sz = fs::file_size(nfbin_actual);
                auto capnp_sz = fs::file_size(capnp_actual);
                std::cout << std::endl;
                std::cout << "  Size comparison:" << std::endl;
                std::cout << "    JSON:    " << json_sz  << " bytes" << std::endl;
                std::cout << "    .nfbin:  " << nfbin_sz << " bytes (" << std::fixed << std::setprecision(1)
                          << (100.0 * nfbin_sz / json_sz) << "% of JSON)" << std::endl;
                std::cout << "    .capnp:  " << capnp_sz << " bytes (" << std::fixed << std::setprecision(1)
                          << (100.0 * capnp_sz / json_sz) << "% of JSON)" << std::endl;
            }
        }
        std::cout << "  PASS: JSON save succeeded" << std::endl;
    }
    std::cout << std::endl;

    // Cleanup
    brain->shutdown();

    // Summary
    std::cout << "=== Round-Trip Test Summary ===" << std::endl;
    if (failures == 0) {
        std::cout << "ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << failures << " TEST(S) FAILED" << std::endl;
    }

    // Cleanup test files
    for (const auto& f : {"test_roundtrip.nfbin", "test_roundtrip.capnp", "test_roundtrip.json"}) {
        auto p = fs::path("BrainState") / f;
        std::error_code ec;
        fs::remove(p, ec);
    }

    return failures > 0 ? 1 : 0;
}
