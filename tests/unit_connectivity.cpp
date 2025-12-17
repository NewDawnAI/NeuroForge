#include "core/Region.h"
#include "core/Neuron.h"
#include "core/Synapse.h"
#include "connectivity/ConnectivityManager.h"
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include "core/HypergraphBrain.h"

// Added for Cap'n Proto-based brain state tests
#ifdef NF_HAVE_CAPNP
#include <capnp/message.h>
#include <capnp/serialize.h>
#endif // NF_HAVE_CAPNP
#ifdef NF_HAVE_CAPNP
#include <kj/array.h>
#endif // NF_HAVE_CAPNP
#include <cstring>
#include <cmath>
#include <unordered_set>
#ifdef NF_HAVE_CAPNP
#include "brainstate.capnp.h"
#endif // NF_HAVE_CAPNP

using namespace NeuroForge;
using namespace NeuroForge::Core;
using namespace NeuroForge::Connectivity;

namespace {
    bool check(bool condition, const std::string& msg) {
        if (!condition) {
            std::cerr << "[FAIL] " << msg << std::endl;
            return false;
        }
        return true;
    }

    bool testCapnpRoundTrip() {
#ifdef NF_HAVE_CAPNP
        std::cout << "Running testCapnpRoundTrip..." << std::endl;
        bool ok = true;

        auto connMgr = std::make_shared<ConnectivityManager>();
        HypergraphBrain brain(connMgr);
        auto r1 = brain.createRegion("R1", Region::Type::Custom, Region::ActivationPattern::Asynchronous);
        auto r2 = brain.createRegion("R2", Region::Type::Custom, Region::ActivationPattern::Asynchronous);

        r1->createNeurons(3);
        r2->createNeurons(2);

        auto n10 = r1->getNeurons()[0];
        auto n11 = r1->getNeurons()[1];
        auto m0 = r2->getNeurons()[0];
        auto m1 = r2->getNeurons()[1];

        auto s1 = r1->connectToRegion(r2, n10->getId(), m0->getId(), 0.33f, SynapseType::Excitatory);
        auto s2 = r1->connectToRegion(r2, n11->getId(), m1->getId(), 0.66f, SynapseType::Inhibitory);
        ok &= check(s1 != nullptr && s2 != nullptr, "connectToRegion should create synapses");

        std::vector<uint8_t> buf;
        ok &= check(brain.exportToCapnp(buf), "exportToCapnp should succeed");
        ok &= check(!buf.empty(), "export buffer should not be empty");

        auto connMgr2 = std::make_shared<ConnectivityManager>();
        HypergraphBrain brain2(connMgr2);
        ok &= check(brain2.importFromCapnp(buf.data(), buf.size()), "importFromCapnp should succeed");

        ok &= check(brain.getRegions().size() == brain2.getRegions().size(), "Region count should match after import");

        auto findByName = [](const HypergraphBrain& b, const std::string& name) -> RegionPtr {
            for (const auto& kv : b.getRegions()) {
                if (kv.second && kv.second->getName() == name) return kv.second;
            }
            return nullptr;
        };
        auto r1_b = findByName(brain2, "R1");
        auto r2_b = findByName(brain2, "R2");
        ok &= check(r1_b != nullptr && r2_b != nullptr, "Imported brain should contain regions R1 and R2");

        if (r1_b && r2_b) {
            ok &= check(r1->getNeuronCount() == r1_b->getNeuronCount(), "R1 neuron count should match after import");
            ok &= check(r2->getNeuronCount() == r2_b->getNeuronCount(), "R2 neuron count should match after import");

            auto interA = r1->getInterRegionConnections();
            auto itA = interA.find(r2->getId());
            std::size_t expected_edges = (itA != interA.end()) ? itA->second.size() : 0;

            auto interAb = r1_b->getInterRegionConnections();
            auto itAb = interAb.find(r2_b->getId());
            std::size_t imported_edges = (itAb != interAb.end()) ? itAb->second.size() : 0;
            ok &= check(expected_edges == imported_edges, "Inter-region edge count (R1->R2) should match after import");

            // Extended invariants: region names and IDs preserved
            ok &= check(r1_b->getName() == std::string("R1") && r2_b->getName() == std::string("R2"), "Region names should be preserved");
            ok &= check(r1_b->getId() == r1->getId() && r2_b->getId() == r2->getId(), "Region IDs should be preserved");

            // Extended invariants: neuron ID sets preserved for each region
            auto collectIds = [](const RegionPtr& r){
                std::unordered_set<uint64_t> ids;
                for (const auto& n : r->getNeurons()) ids.insert(n->getId());
                return ids;
            };
            auto ids_r1 = collectIds(r1);
            auto ids_r1b = collectIds(r1_b);
            auto ids_r2 = collectIds(r2);
            auto ids_r2b = collectIds(r2_b);
            ok &= check(ids_r1.size() == ids_r1b.size(), "R1 neuron ID set size should match");
            ok &= check(ids_r2.size() == ids_r2b.size(), "R2 neuron ID set size should match");
            for (auto id : ids_r1) ok &= check(ids_r1b.count(id) == 1, "R1 neuron IDs should be preserved");
            for (auto id : ids_r2) ok &= check(ids_r2b.count(id) == 1, "R2 neuron IDs should be preserved");

            // Extended invariants: specific synapse properties preserved (IDs, weights, types)
            auto findSyn = [](const std::vector<SynapsePtr>& vec, uint64_t srcId, uint64_t dstId) -> SynapsePtr {
                for (const auto& s : vec) {
                    if (!s) continue;
                    auto srcPtr = s->getSource().lock();
                    auto dstPtr = s->getTarget().lock();
                    if (srcPtr && dstPtr && srcPtr->getId() == srcId && dstPtr->getId() == dstId) return s;
                }
                return nullptr;
            };

            auto s1_src_ptr = s1->getSource().lock();
            auto s1_dst_ptr = s1->getTarget().lock();
            ok &= check(s1_src_ptr != nullptr && s1_dst_ptr != nullptr, "Original synapse 1 endpoints should be valid");
            uint64_t s1_src = s1_src_ptr ? s1_src_ptr->getId() : 0;
            uint64_t s1_dst = s1_dst_ptr ? s1_dst_ptr->getId() : 0;
            const auto s1_id = s1->getId();
            const auto s1_type = s1->getType();
            const float s1_w = s1->getWeight();

            auto s2_src_ptr = s2->getSource().lock();
            auto s2_dst_ptr = s2->getTarget().lock();
            ok &= check(s2_src_ptr != nullptr && s2_dst_ptr != nullptr, "Original synapse 2 endpoints should be valid");
            uint64_t s2_src = s2_src_ptr ? s2_src_ptr->getId() : 0;
            uint64_t s2_dst = s2_dst_ptr ? s2_dst_ptr->getId() : 0;
            const auto s2_id = s2->getId();
            const auto s2_type = s2->getType();
            const float s2_w = s2->getWeight();

            SynapsePtr s1_b_syn = nullptr;
            SynapsePtr s2_b_syn = nullptr;
            {
                const auto& interMapB = r1_b->getInterRegionConnections();
                auto it = interMapB.find(r2_b->getId());
                if (it != interMapB.end()) {
                    s1_b_syn = findSyn(it->second, s1_src, s1_dst);
                    s2_b_syn = findSyn(it->second, s2_src, s2_dst);
                }
            }
            ok &= check(s1_b_syn != nullptr && s2_b_syn != nullptr, "Imported brain should contain both specific synapses");
            if (s1_b_syn) {
                ok &= check(s1_b_syn->getId() == s1_id, "Synapse 1 ID should be preserved");
                ok &= check(s1_b_syn->getType() == s1_type, "Synapse 1 type should be preserved");
                ok &= check(std::fabs(s1_b_syn->getWeight() - s1_w) < 1e-5f, "Synapse 1 weight should be preserved");
            }
            if (s2_b_syn) {
                ok &= check(s2_b_syn->getId() == s2_id, "Synapse 2 ID should be preserved");
                ok &= check(s2_b_syn->getType() == s2_type, "Synapse 2 type should be preserved");
                ok &= check(std::fabs(s2_b_syn->getWeight() - s2_w) < 1e-5f, "Synapse 2 weight should be preserved");
            }
        }

        std::cout << (ok ? "[PASS] " : "[FAIL] ") << "testCapnpRoundTrip" << std::endl;
        return ok;
#else
        std::cout << "[SKIP] testCapnpRoundTrip (Cap'n Proto not available)" << std::endl;
        return true;
#endif // NF_HAVE_CAPNP
    }

    bool testDuplicateBookkeeping() {
        std::cout << "Running testDuplicateBookkeeping..." << std::endl;

        auto regionA = RegionFactory::createRegion("A", Region::Type::Custom, Region::ActivationPattern::Asynchronous);
        auto regionB = RegionFactory::createRegion("B", Region::Type::Custom, Region::ActivationPattern::Asynchronous);

        // Create one neuron in each region
        auto neuronsA = regionA->createNeurons(1);
        auto neuronsB = regionB->createNeurons(1);
        auto src = neuronsA.front();
        auto dst = neuronsB.front();

        // First connect
        auto s1 = regionA->connectToRegion(regionB, src->getId(), dst->getId(), 0.42f, SynapseType::Excitatory);
        bool ok = check(s1 != nullptr, "First connectToRegion should create a synapse");

        // Second connect with same pair should not create duplicates
        auto s2 = regionA->connectToRegion(regionB, src->getId(), dst->getId(), 0.42f, SynapseType::Excitatory);
        ok &= check(s2 != nullptr, "Second connectToRegion should return existing synapse");
        ok &= check(s1.get() == s2.get(), "Second connectToRegion must return the same synapse instance");

        // Verify neuron-level synapse lists have no duplicates
        auto srcOut = src->getOutputSynapses();
        auto dstIn = dst->getInputSynapses();
        ok &= check(srcOut.size() == 1, "Source neuron should have exactly 1 output synapse after duplicate connect");
        ok &= check(dstIn.size() == 1, "Target neuron should have exactly 1 input synapse after duplicate connect");

        // Verify Region bookkeeping maps
        const auto& outMap = regionA->getOutputConnections();
        const auto itOut = outMap.find(src->getId());
        ok &= check(itOut != outMap.end(), "Output map should contain entry for source neuron");
        if (itOut != outMap.end()) {
            ok &= check(itOut->second.size() == 1, "Output connections vector must have size 1 (no duplicates)");
        }

        const auto& inMap = regionB->getInputConnections();
        const auto itIn = inMap.find(dst->getId());
        ok &= check(itIn != inMap.end(), "Input map should contain entry for target neuron");
        if (itIn != inMap.end()) {
            ok &= check(itIn->second.size() == 1, "Input connections vector must have size 1 (no duplicates)");
        }

        const auto& interMap = regionA->getInterRegionConnections();
        const auto itInter = interMap.find(regionB->getId());
        ok &= check(itInter != interMap.end(), "Inter-region map should contain entry for target region");
        if (itInter != interMap.end()) {
            ok &= check(itInter->second.size() == 1, "Inter-region connections vector must have size 1 (no duplicates)");
        }

        std::cout << (ok ? "[PASS] " : "[FAIL] ") << "testDuplicateBookkeeping" << std::endl;
        return ok;
    }

    bool testReservationHelpers() {
        std::cout << "Running testReservationHelpers..." << std::endl;
        bool ok = true;

        auto regionA = RegionFactory::createRegion("A", Region::Type::Custom, Region::ActivationPattern::Asynchronous);
        auto regionB = RegionFactory::createRegion("B", Region::Type::Custom, Region::ActivationPattern::Asynchronous);

        // Create neurons: 1 source in A, 4 targets in B
        auto srcs = regionA->createNeurons(1);
        auto tgts = regionB->createNeurons(4);
        auto src = srcs.front();

        // Reserve capacities before any connections
        regionA->reserveOutputConnections(src->getId(), 3);
        regionB->reserveInputConnections(tgts[0]->getId(), 1);
        regionB->reserveInputConnections(tgts[1]->getId(), 1);
        regionB->reserveInputConnections(tgts[2]->getId(), 1);
        regionB->reserveInputConnections(tgts[3]->getId(), 1);
        regionA->reserveInterRegionConnections(regionB->getId(), 4);

        // Capture capacities after reserve
        auto capOutReserved = regionA->getOutputConnections().at(src->getId()).capacity();
        auto capIn0Reserved = regionB->getInputConnections().at(tgts[0]->getId()).capacity();
        auto capInterReserved = regionA->getInterRegionConnections().at(regionB->getId()).capacity();

        ok &= check(capOutReserved >= 3, "Output reserve should allocate at least requested capacity");
        ok &= check(capIn0Reserved >= 1, "Input reserve should allocate at least requested capacity");
        ok &= check(capInterReserved >= 4, "Inter-region reserve should allocate at least requested capacity");

        // Now add connections: 3 outputs from src to first 3 targets, then one more to 4th
        for (int i = 0; i < 4; ++i) {
            auto syn = regionA->connectToRegion(regionB, src->getId(), tgts[i]->getId(), 0.5f, SynapseType::Excitatory);
            ok &= check(syn != nullptr, "connectToRegion should succeed after reserve");
        }

        // Validate sizes and that capacities did not shrink or grow unnecessarily
        const auto& outMap = regionA->getOutputConnections();
        const auto& interMap = regionA->getInterRegionConnections();

        const auto outVecSize = outMap.at(src->getId()).size();
        const auto outVecCap = outMap.at(src->getId()).capacity();
        ok &= check(outVecSize == 4, "Output connections size should match number of connections added");
        ok &= check(outVecCap >= capOutReserved, "Output connections capacity should be at least the reserved capacity (no unexpected shrink)");

        const auto interVecSize = interMap.at(regionB->getId()).size();
        const auto interVecCap = interMap.at(regionB->getId()).capacity();
        ok &= check(interVecSize == 4, "Inter-region connections size should match number of connections added");
        ok &= check(interVecCap >= capInterReserved, "Inter-region capacity should be at least the reserved capacity");

        // Spot-check input capacities and sizes for a couple of targets
        const auto& inMap = regionB->getInputConnections();
        for (int i = 0; i < 2; ++i) {
            auto it = inMap.find(tgts[i]->getId());
            ok &= check(it != inMap.end(), "Input map should contain entry for target neuron after connection");
            if (it != inMap.end()) {
                ok &= check(it->second.size() == 1, "Each target neuron should have exactly one input after single connection");
                ok &= check(it->second.capacity() >= 1, "Input vector capacity should be at least 1 as reserved");
            }
        }

        std::cout << (ok ? "[PASS] " : "[FAIL] ") << "testReservationHelpers" << std::endl;
        return ok;
    }

    void runConnectivityBenchmark(std::size_t nA, std::size_t nB, float p, ConnectivityManager::ConnectivityType type) {
        std::cout << "\nRunning connectivity benchmark: type="
                  << (type == ConnectivityManager::ConnectivityType::Reciprocal ? "Reciprocal" : "Global")
                  << ", nA=" << nA << ", nB=" << nB << ", p=" << p << std::endl;

        ConnectivityManager mgr;

        auto regionA = RegionFactory::createRegion("BenchA", Region::Type::Custom, Region::ActivationPattern::Asynchronous);
        auto regionB = RegionFactory::createRegion("BenchB", Region::Type::Custom, Region::ActivationPattern::Asynchronous);

        regionA->createNeurons(nA);
        regionB->createNeurons(nB);

        mgr.registerRegion(regionA);
        mgr.registerRegion(regionB);

        ConnectivityManager::ConnectionParameters params;
        params.type = type;
        params.distribution = ConnectivityManager::ProbabilityDistribution::Uniform;
        params.connection_probability = p;
        params.weight_mean = 0.5f;
        params.weight_std = 0.1f;
        params.distance_decay = 0.0f;
        params.bidirectional = (type == ConnectivityManager::ConnectivityType::Reciprocal);
        params.max_connections_per_neuron = 0; // unlimited
        params.plasticity_rate = 0.0f;

        auto src_id = std::to_string(regionA->getId());
        auto dst_id = std::to_string(regionB->getId());

        const auto t0 = std::chrono::steady_clock::now();
        std::size_t created = mgr.connectRegions(src_id, dst_id, params);
        const auto t1 = std::chrono::steady_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

        // Aggregate some metrics
        std::size_t total_out_edges = 0;
        for (const auto& kv : regionA->getOutputConnections()) total_out_edges += kv.second.size();
        std::size_t total_in_edges_B = 0;
        for (const auto& kv : regionB->getInputConnections()) total_in_edges_B += kv.second.size();

        std::size_t inter_size = 0, inter_cap = 0;
        auto interIt = regionA->getInterRegionConnections().find(regionB->getId());
        if (interIt != regionA->getInterRegionConnections().end()) {
            inter_size = interIt->second.size();
            inter_cap = interIt->second.capacity();
        }

        std::cout << "Created connections: " << created
                  << ", elapsed: " << ms << " ms"
                  << ", total_out(A): " << total_out_edges
                  << ", total_in(B): " << total_in_edges_B
                  << ", inter[A->B] size/cap: " << inter_size << "/" << inter_cap
                  << std::endl;
    }

    bool testBrainStateRoundTripCapnp() {
#ifdef NF_HAVE_CAPNP
        std::cout << "Running testBrainStateRoundTripCapnp..." << std::endl;
        bool ok = true;

        auto connMgr = std::make_shared<ConnectivityManager>();
        HypergraphBrain brain(connMgr);
        auto r1 = brain.createRegion("A", Region::Type::Custom, Region::ActivationPattern::Asynchronous);
        auto r2 = brain.createRegion("B", Region::Type::Custom, Region::ActivationPattern::Asynchronous);
        r1->createNeurons(2);
        r2->createNeurons(2);
        auto n0 = r1->getNeurons()[0];
        auto m0 = r2->getNeurons()[0];
        ok &= check(r1->connectToRegion(r2, n0->getId(), m0->getId(), 0.5f, SynapseType::Excitatory) != nullptr, "connectToRegion should work");

        const std::string path = "BrainState/roundtrip.capnp";
        ok &= check(brain.saveCheckpoint(path), "saveCheckpoint(.capnp) should succeed");

        auto connMgr2 = std::make_shared<ConnectivityManager>();
        HypergraphBrain brain2(connMgr2);
        ok &= check(brain2.loadCheckpoint(path), "loadCheckpoint(.capnp) should succeed");

        ok &= check(brain.getRegions().size() == brain2.getRegions().size(), "Region count should match after load");
        ok &= check(brain.getGlobalStatistics().total_synapses == brain2.getGlobalStatistics().total_synapses, "Total synapses should match after load");

        std::cout << (ok ? "[PASS] " : "[FAIL] ") << "testBrainStateRoundTripCapnp" << std::endl;
        return ok;
#else
        std::cout << "[SKIP] testBrainStateRoundTripCapnp (Cap'n Proto not available)" << std::endl;
        return true;
#endif // NF_HAVE_CAPNP
    }

    bool testBrainStateVersionGuard() {
#ifdef NF_HAVE_CAPNP
        std::cout << "Running testBrainStateVersionGuard..." << std::endl;
        bool ok = true;

        // Build a minimal valid BrainStateFile, then tamper the version field
        auto connMgr = std::make_shared<ConnectivityManager>();
        HypergraphBrain brain(connMgr);
        auto r = brain.createRegion("V", Region::Type::Custom, Region::ActivationPattern::Asynchronous);
        r->createNeurons(1);

        std::vector<uint8_t> buf;
        ok &= check(brain.exportToBrainStateCapnp(buf), "exportToBrainStateCapnp should succeed");
        if (!ok) {
            std::cout << "[FAIL] Unable to export BrainState for version guard test" << std::endl;
            return false;
        }

        // Parse and flip version to an incompatible one, then re-serialize
        try {
            const size_t wordCount = (buf.size() + sizeof(capnp::word) - 1) / sizeof(capnp::word);
#ifdef NF_HAVE_CAPNP
            kj::Array<capnp::word> words = kj::heapArray<capnp::word>(wordCount);
#endif
            auto bytesView = words.asBytes();
            std::memset(bytesView.begin(), 0, bytesView.size());
            std::memcpy(bytesView.begin(), buf.data(), buf.size());
            capnp::FlatArrayMessageReader reader(words.asPtr());

            auto file_reader = reader.getRoot<NeuroForge::BrainState::BrainStateFile>();
            // Rebuild a new message with different version
            capnp::MallocMessageBuilder builder;
            auto file_builder = builder.initRoot<NeuroForge::BrainState::BrainStateFile>();
            file_builder.setFileFormatVersion(9999); // incompatible
            file_builder.setCreatedTimestamp(file_reader.getCreatedTimestamp());
            file_builder.setCreatedBy(file_reader.getCreatedBy());
            file_builder.setDescription(file_reader.getDescription());
            file_builder.setBrain(file_reader.getBrain());
            file_builder.setMetadata(file_reader.getMetadata());

            auto flatArray = capnp::messageToFlatArray(builder);
            auto words2 = flatArray.asPtr();
            auto bytes2 = words2.asBytes();
            std::vector<uint8_t> tampered(bytes2.begin(), bytes2.end());

            auto connMgr2 = std::make_shared<ConnectivityManager>();
            HypergraphBrain brain2(connMgr2);
            bool imported = brain2.importFromBrainStateCapnp(tampered.data(), tampered.size());
            ok &= check(!imported, "importFromBrainStateCapnp should fail on version mismatch");
        } catch (...) {
            ok &= check(false, "Exception during version guard test");
        }

        std::cout << (ok ? "[PASS] " : "[FAIL] ") << "testBrainStateVersionGuard" << std::endl;
        return ok;
#else
        std::cout << "[SKIP] testBrainStateVersionGuard (Cap'n Proto not available)" << std::endl;
        return true;
#endif // NF_HAVE_CAPNP
    }

    bool testCorruptedBufferHandling() {
#ifdef NF_HAVE_CAPNP
        std::cout << "Running testCorruptedBufferHandling..." << std::endl;
        bool ok = true;

        auto connMgr = std::make_shared<ConnectivityManager>();
        HypergraphBrain brain(connMgr);
        auto r = brain.createRegion("X", Region::Type::Custom, Region::ActivationPattern::Asynchronous);
        r->createNeurons(1);

        std::vector<uint8_t> bufBrain;
        std::vector<uint8_t> bufState;
        ok &= check(brain.exportToCapnp(bufBrain), "exportToCapnp should succeed for corruption test");
        ok &= check(brain.exportToBrainStateCapnp(bufState), "exportToBrainStateCapnp should succeed for corruption test");

        // Prepare various corrupted/incomplete buffers
        std::vector<uint8_t> empty;
        std::vector<uint8_t> tiny(7, 0xAA);

        std::vector<uint8_t> truncBrain;
        if (bufBrain.size() > 4) truncBrain.assign(bufBrain.begin(), bufBrain.begin() + bufBrain.size() / 2);
        else truncBrain = tiny; // ensure non-empty malformed data

        std::vector<uint8_t> truncState;
        if (bufState.size() > 4) truncState.assign(bufState.begin(), bufState.begin() + bufState.size() / 2);
        else truncState = tiny;

        auto corruptState = bufState;
        if (!corruptState.empty()) corruptState[corruptState.size() / 2] ^= 0xFF; // flip some bits in the middle

        // Validate that imports fail gracefully
        auto connMgrA = std::make_shared<ConnectivityManager>();
        HypergraphBrain bA(connMgrA);
        ok &= check(!bA.importFromCapnp(empty.data(), 0), "importFromCapnp should fail on empty buffer");
        ok &= check(!bA.importFromCapnp(tiny.data(), tiny.size()), "importFromCapnp should fail on random tiny buffer");
        ok &= check(!bA.importFromCapnp(truncBrain.data(), truncBrain.size()), "importFromCapnp should fail on truncated buffer");

        auto connMgrB = std::make_shared<ConnectivityManager>();
        HypergraphBrain bB(connMgrB);
        ok &= check(!bB.importFromBrainStateCapnp(empty.data(), 0), "importFromBrainStateCapnp should fail on empty buffer");
        ok &= check(!bB.importFromBrainStateCapnp(tiny.data(), tiny.size()), "importFromBrainStateCapnp should fail on random tiny buffer");
        ok &= check(!bB.importFromBrainStateCapnp(truncState.data(), truncState.size()), "importFromBrainStateCapnp should fail on truncated buffer");
        ok &= check(!bB.importFromBrainStateCapnp(corruptState.data(), corruptState.size()), "importFromBrainStateCapnp should fail on corrupted buffer");

        std::cout << (ok ? "[PASS] " : "[FAIL] ") << "testCorruptedBufferHandling" << std::endl;
        return ok;
#else
        std::cout << "[SKIP] testCorruptedBufferHandling (Cap'n Proto not available)" << std::endl;
        return true;
#endif // NF_HAVE_CAPNP
    }
}

int main() {
    bool ok_all = true;

    ok_all &= testCapnpRoundTrip();
    ok_all &= testBrainStateRoundTripCapnp();
    ok_all &= testBrainStateVersionGuard();
    ok_all &= testDuplicateBookkeeping();
    ok_all &= testReservationHelpers();

    // Quick synthetic benchmarks with moderate sizes to keep CI runtime reasonable
    runConnectivityBenchmark(1000, 1000, 0.05f, ConnectivityManager::ConnectivityType::Global);
    runConnectivityBenchmark(1000, 1000, 0.05f, ConnectivityManager::ConnectivityType::Reciprocal);

    if (!ok_all) {
        std::cerr << "Some unit tests failed." << std::endl;
        return 1;
    }
    std::cout << "All unit tests passed." << std::endl;
    return 0;
}