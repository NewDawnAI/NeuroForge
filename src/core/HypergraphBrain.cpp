#include "core/HypergraphBrain.h"
#include "core/SelfModel.h" // Step 2: read-only SelfModel mirror
#include "core/Region.h"
#include "core/Neuron.h"
#include "core/Synapse.h"
#include "core/LearningSystem.h"
#include "core/SubstrateLanguageManager.h"
#include "core/Logger.h"
#include "regions/LimbicRegions.h"
#include "regions/CorticalRegions.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <future>
#include <numeric>
#include <optional>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdio>
#include <unordered_map>
#include <unordered_set>
#include <cstring>
#include <iostream>
#include <execution>
#include <thread>

#ifdef __AVX2__
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif

#ifdef NF_HAVE_OPENCV_CUDA
#include <opencv2/core/cuda.hpp>
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

#ifdef NF_HAVE_CAPNP
#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/array.h>
#include "neuroforge.capnp.h"
#include "brainstate.capnp.h"
#endif // NF_HAVE_CAPNP

// Helpers for optional ANSI color on supported terminals
namespace {
    inline bool is_stderr_tty() {
    #ifdef _WIN32
        DWORD mode = 0; HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return false;
        return GetConsoleMode(h, &mode) != 0;
    #else
        return isatty(fileno(stderr));
    #endif
    }

    #ifdef _WIN32
    inline void enable_vt_mode() {
        HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
        if (h == INVALID_HANDLE_VALUE || h == nullptr) return;
        DWORD mode = 0; if (!GetConsoleMode(h, &mode)) return;
        mode |= 0x0004; // ENABLE_VIRTUAL_TERMINAL_PROCESSING
        SetConsoleMode(h, mode);
    }
    #endif

    inline const char* ansi_green() { return "\x1b[32m"; }
    inline const char* ansi_yellow() { return "\x1b[33m"; }
    inline const char* ansi_cyan() { return "\x1b[36m"; }
    inline const char* ansi_dim() { return "\x1b[2m"; }
    inline const char* ansi_reset() { return "\x1b[0m"; }
}

namespace NeuroForge {
    namespace Core {

        HypergraphBrain::HypergraphBrain(std::shared_ptr<NeuroForge::Connectivity::ConnectivityManager> connectivity_manager,
                                         float target_frequency, ProcessingMode processing_mode)
            : connectivity_manager_(connectivity_manager)
            , learning_system_(nullptr)
            , learning_enabled_(false)
            , processing_mode_(processing_mode)
            , brain_state_(BrainState::Uninitialized)
            , is_processing_(false)
            , target_frequency_(target_frequency)
            , actual_frequency_(0.0f)
            , active_thread_count_(0)
            , processing_cycles_(0)
            , hardware_monitoring_enabled_(true)
            , memdb_colorize_(false)
            , main_thread_id_(std::this_thread::get_id())
        {
            // Reserve space for regions and synapses to improve performance
            regions_.reserve(32);
            global_synapses_.reserve(1024);
        }

        HypergraphBrain::~HypergraphBrain() {
            shutdown();
        }

        void HypergraphBrain::setRandomSeed(std::uint32_t seed) {
            {
                std::lock_guard<std::mutex> lock(rng_mutex_);
                rng_.seed(seed);
            }
            if (learning_system_) {
                learning_system_->setRandomSeed(seed);
            }
        }

        bool HypergraphBrain::initialize() {
            if (brain_state_ != BrainState::Uninitialized && brain_state_ != BrainState::Shutdown) {
                return false;
            }

            brain_state_.store(BrainState::Initializing);
            is_processing_.store(false);
            processing_cycles_.store(0);
            actual_frequency_.store(0.0f);

            if (hardware_monitoring_enabled_.load()) {
                updateHardwareInfo();
            }

            // Initialize regions if needed
            {
                std::lock_guard<std::mutex> lock(region_mutex_);
                for (auto &kv : regions_) {
                    if (kv.second) {
                        kv.second->initialize();
                    }
                }
            }

            brain_state_.store(BrainState::Paused);
            return true;
        }

        bool HypergraphBrain::start() {
            if (brain_state_ == BrainState::Running) {
                return true;
            }
            if (brain_state_ == BrainState::Uninitialized || brain_state_ == BrainState::Shutdown) {
                if (!initialize()) return false;
            }
            is_processing_.store(true);
            brain_state_.store(BrainState::Running);
            return true;
        }

        void HypergraphBrain::pause() {
            if (brain_state_ == BrainState::Running) {
                is_processing_.store(false);
                brain_state_.store(BrainState::Paused);
            }
        }

        void HypergraphBrain::resume() {
            if (brain_state_ == BrainState::Paused) {
                is_processing_.store(true);
                brain_state_.store(BrainState::Running);
            }
        }

        void HypergraphBrain::stop() {
            is_processing_.store(false);
            if (brain_state_ == BrainState::Running || brain_state_ == BrainState::Paused) {
                std::lock_guard<std::mutex> lock(region_mutex_);
                for (auto &kv : regions_) {
                    if (kv.second) {
                        kv.second->reset();
                    }
                }
                brain_state_.store(BrainState::Paused);
            }
        }

        void HypergraphBrain::reset() {
            brain_state_.store(BrainState::Resetting);
            stop();

            {
                std::lock_guard<std::mutex> lock(brain_mutex_);

                // Clear all regions and connections safely
                {
                    std::lock_guard<std::mutex> rlock(region_mutex_);
                    regions_.clear();
                    region_names_.clear();
                }

                // Clear global synapses
                global_synapses_.clear();
                global_synapse_ptrs_.clear();

                // Reset statistics
                processing_cycles_.store(0);
                actual_frequency_.store(0.0f);

                {
                    std::lock_guard<std::mutex> lock(statistics_mutex_);
                    stats_dirty_ = true;
                }
                
                // Ensure processing is stopped and allow a fresh initialize() after reset
                is_processing_.store(false);
                brain_state_.store(BrainState::Uninitialized);
            }

            // Clear callbacks
            clearCallbacks();
        }

        void HypergraphBrain::shutdown() {
            // Stop processing first; we'll finalize the state after cleanup
            stop();

            std::lock_guard<std::mutex> lock(brain_mutex_);

            // Clear global neuron spike callback to avoid dangling captures
            Neuron::setSpikeCallback(nullptr);

            // Release learning system if active (no explicit shutdown API)
            learning_system_.reset();
            learning_enabled_.store(false, std::memory_order_relaxed);

            // Clear all regions
            regions_.clear();
            region_names_.clear();

            // Clear global synapses (no nested lock; brain_mutex_ already held)
            global_synapses_.clear();
            global_synapse_ptrs_.clear();

            // Clear callbacks
            clearCallbacks();

            // Ensure not processing and set final state to Shutdown
            is_processing_.store(false);
            brain_state_.store(BrainState::Shutdown);
        }

        bool HypergraphBrain::addRegion(RegionPtr region) {
            if (!region) {
                return false;
            }

            std::lock_guard<std::mutex> lock(region_mutex_);

            RegionID region_id = region->getId();

            // Check if region already exists
            if (regions_.find(region_id) != regions_.end()) {
                return false;
            }

            // Add region
            regions_[region_id] = region;
            region_names_[region->getName()] = region_id;

            // If MemoryDB already set on brain, propagate to this new region
            if (memory_db_) {
                try {
                    region->setMemoryDB(memory_db_, memory_db_run_id_);
                    if (memdb_propagation_debug_) {
                        if (memdb_colorize_ && is_stderr_tty()) {
                        #ifdef _WIN32
                            enable_vt_mode();
                        #endif
                            std::cerr << ansi_cyan() << "[MemoryDB Debug][" << region->getName() << ":id=" << region_id << "] "
                                      << ansi_green() << "Propagated to new region, run_id=" << memory_db_run_id_ << ansi_reset() << "\n";
                        } else {
                            std::cerr << "[MemoryDB Debug][" << region->getName() << ":id=" << region_id << "] "
                                      << "Propagated to new region, run_id=" << memory_db_run_id_ << "\n";
                        }
                    }
                } catch (...) {
                    // Ignore per-region exceptions during best-effort propagation
                }
            }

            // If brain is already initialized or running, initialize the new region to ensure it's active
            auto state = brain_state_.load(std::memory_order_relaxed);
            if (state != BrainState::Uninitialized && state != BrainState::Shutdown) {
                region->initialize();
            }
            region->setBrain(this);

            // Update statistics
            {
                std::lock_guard<std::mutex> lock(statistics_mutex_);
                stats_dirty_ = true;
            }

            return true;
        }

        bool HypergraphBrain::removeRegion(RegionID region_id) {
            // Keep the region alive while we clean up associated synapses
            RegionPtr region_to_remove;
            std::string region_name;
            {
                std::lock_guard<std::mutex> lock(region_mutex_);
                auto it = regions_.find(region_id);
                if (it == regions_.end()) {
                    return false;
                }
                region_to_remove = it->second;
                region_name = region_to_remove->getName();
            }

            // Build a set of neuron IDs belonging to this region
            std::unordered_set<NeuroForge::NeuronID> neuron_ids;
            const auto& neurons = region_to_remove->getNeurons();
            neuron_ids.reserve(neurons.size());
            for (const auto& n : neurons) {
                if (n) {
                    neuron_ids.insert(n->getId());
                }
            }

            // Remove any global synapses associated with neurons of this region
            {
                std::lock_guard<std::mutex> lock(brain_mutex_);
                global_synapses_.erase(std::remove_if(global_synapses_.begin(), global_synapses_.end(),
                    [&neuron_ids](const SynapsePtr& synapse) {
                        if (!synapse) return false;
                        auto src = synapse->getSource().lock();
                        if (src && neuron_ids.find(src->getId()) != neuron_ids.end()) return true;
                        auto tgt = synapse->getTarget().lock();
                        if (tgt && neuron_ids.find(tgt->getId()) != neuron_ids.end()) return true;
                        return false;
                    }),
                    global_synapses_.end());
            }

            // Now remove the region from containers
            {
                std::lock_guard<std::mutex> lock(region_mutex_);
                region_names_.erase(region_name);
                regions_.erase(region_id);
            }

            {
                std::lock_guard<std::mutex> lock(statistics_mutex_);
                stats_dirty_ = true;
            }
            return true;
        }

        RegionPtr HypergraphBrain::getRegion(RegionID region_id) const {
            std::lock_guard<std::mutex> lock(region_mutex_);

            auto it = regions_.find(region_id);
            return (it != regions_.end()) ? it->second : nullptr;
        }

        RegionPtr HypergraphBrain::getRegion(const std::string& name) const {
            std::lock_guard<std::mutex> lock(region_mutex_);

            auto it = region_names_.find(name);
            if (it != region_names_.end()) {
                auto rit = regions_.find(it->second);
                return (rit != regions_.end()) ? rit->second : nullptr;
            }
            return nullptr;
        }

        RegionPtr HypergraphBrain::createRegion(const std::string& name,
                                               Region::Type type,
                                               Region::ActivationPattern pattern) {
            auto region = RegionFactory::createRegion(name, type, pattern);
            if (region && addRegion(region)) {
                return region;
            }
            return nullptr;
        }

        // ===== Neural substrate API: modality routing and I/O =====
        void HypergraphBrain::mapModality(NeuroForge::Modality modality, NeuroForge::RegionID region_id) {
            std::lock_guard<std::mutex> lock(region_mutex_);
            // Only map if the region currently exists
            auto it = regions_.find(region_id);
            if (it != regions_.end()) {
                modality_region_map_[modality] = region_id;
            } else {
                // If region does not exist, remove mapping if present
                modality_region_map_.erase(modality);
            }
        }

        RegionPtr HypergraphBrain::getModalityRegion(NeuroForge::Modality modality) const {
            std::lock_guard<std::mutex> lock(region_mutex_);
            auto it = modality_region_map_.find(modality);
            if (it == modality_region_map_.end()) {
                return nullptr;
            }
            auto rit = regions_.find(it->second);
            return (rit != regions_.end()) ? rit->second : nullptr;
        }

        void HypergraphBrain::feedExternalPattern(NeuroForge::Modality modality, const std::vector<float>& pattern) {
            auto region = getModalityRegion(modality);
            if (!region) { return; }
            region->feedExternalPattern(pattern);
        }

        std::vector<float> HypergraphBrain::readoutVector(NeuroForge::Modality modality) const {
            std::vector<float> out;
            auto region = getModalityRegion(modality);
            if (region) {
                region->readoutVector(out);
            }
            return out;
        }

        void HypergraphBrain::applyNeuromodulator(NeuroForge::Modality modality, float level) {
            auto region = getModalityRegion(modality);
            if (!region) { return; }
            region->applyNeuromodulator(level);
        }

        std::size_t HypergraphBrain::connectRegions(RegionID source_region_id,
                                                     RegionID target_region_id,
                                                     float connection_density,
                                                     std::pair<float, float> weight_range) {
            auto source_region = getRegion(source_region_id);
            auto target_region = getRegion(target_region_id);
            if (!source_region || !target_region) {
                return 0;
            }

            const auto& source_neurons = source_region->getNeurons();
            const auto& target_neurons = target_region->getNeurons();
            if (source_neurons.empty() || target_neurons.empty()) {
                return 0;
            }

            // Early-out if zero density requested
            if (connection_density <= 0.0f) {
                return 0;
            }

            // Sampling-based strategy for connectivity
            // Cap per-source fan-out to keep synapse counts and runtime reasonable
            const std::size_t target_count = target_neurons.size();

            // Compute expected fan-out per source and cap it
            const float expected_k_f = connection_density * static_cast<float>(target_count);
            const std::size_t base_k = static_cast<std::size_t>(expected_k_f);
            const float fractional = expected_k_f - static_cast<float>(base_k);

            // Hard cap per source to avoid explosive synapse counts with huge regions
            // This retains sparsity while keeping runtime bounded.
            const std::size_t max_per_source_cap = 64; // tuned for demo stability

            // If procedural mode is enabled (e.g. for massive scale), skip physical synapse creation
            // We only log the connection count but do not instantiate objects.
            if (procedural_connectivity_enabled_) {
                std::size_t total_virtual_synapses = 0;
                for (const auto& source : source_neurons) {
                    if (!source) continue;
                    // Statistical estimate of connections
                    std::size_t k = base_k + (0.5f < fractional ? 1 : 0); 
                    if (k > max_per_source_cap) k = max_per_source_cap;
                    total_virtual_synapses += k;
                }
                return total_virtual_synapses;
            }

            std::lock_guard<std::mutex> rng_lock(rng_mutex_);
            std::uniform_real_distribution<float> prob01(0.0f, 1.0f);
            std::uniform_int_distribution<std::size_t> target_index_dist(0, target_count - 1);
            std::uniform_real_distribution<float> weight_dist(weight_range.first, weight_range.second);

            std::size_t established_connections = 0;
            const bool self_connection = (source_region_id == target_region_id);

            for (const auto& source : source_neurons) {
                if (!source) continue;

                // Determine k for this source neuron based on expected value and fractional part
                std::size_t k = base_k + ((prob01(rng_) < fractional) ? 1 : 0);
                if (k > max_per_source_cap) k = max_per_source_cap;
                if (k == 0) continue; // nothing to connect for this source

                // Sample k distinct targets without replacement
                std::unordered_set<std::size_t> chosen;
                chosen.reserve(k * 2);

                // Limit attempts to avoid pathological loops when k ~ N
                std::size_t attempts = 0;
                const std::size_t max_attempts = k * 8 + 16;

                while (chosen.size() < k && attempts < max_attempts) {
                    std::size_t idx = target_index_dist(rng_);
                    // Avoid self-loop when connecting within the same region
                    if (self_connection && target_neurons[idx] && (target_neurons[idx]->getId() == source->getId())) {
                        ++attempts;
                        continue;
                    }
                    chosen.insert(idx);
                    ++attempts;
                }

                for (std::size_t idx : chosen) {
                    auto& target = target_neurons[idx];
                    if (!target) continue;
                    auto synapse = connectNeurons(source_region_id, target_region_id,
                                                  source->getId(), target->getId(),
                                                  weight_dist(rng_), SynapseType::Excitatory);
                    if (synapse) {
                        ++established_connections;
                    }
                }
            }

            return established_connections;
        }

        SynapsePtr HypergraphBrain::connectNeurons(RegionID source_region_id,
                                                  RegionID target_region_id,
                                                  NeuronID source_neuron_id,
                                                  NeuronID target_neuron_id,
                                                  Weight weight,
                                                  SynapseType type) {
            auto source_region = getRegion(source_region_id);
            auto target_region = getRegion(target_region_id);

            if (!source_region || !target_region) {
                return nullptr;
            }

            SynapsePtr synapse = nullptr;

            if (source_region_id == target_region_id) {
                // Intra-region connection: let Region manage neuron lists and internal bookkeeping
                synapse = source_region->connectNeurons(source_neuron_id, target_neuron_id, weight, type);
            } else {
                // Inter-region connection: delegate to Region::connectToRegion to populate
                // inter-region, input/output maps and neuron synapse lists without duplication
                synapse = source_region->connectToRegion(target_region, source_neuron_id, target_neuron_id, weight, type);
            }

            if (synapse && synapse->isValid()) {
                // Track globally, avoid duplicates
                std::lock_guard<std::mutex> lock(brain_mutex_);
                if (global_synapse_ptrs_.insert(synapse.get()).second) {
                    global_synapses_.push_back(synapse);
                }
                return synapse;
            }

            return nullptr;
        }

        SynapsePtr HypergraphBrain::connectNeurons(RegionID source_region_id,
                                                  RegionID target_region_id,
                                                  NeuronID source_neuron_id,
                                                  NeuronID target_neuron_id,
                                                  Weight weight,
                                                  SynapseType type,
                                                  SynapseID explicit_id) {
            auto source_region = getRegion(source_region_id);
            auto target_region = getRegion(target_region_id);

            if (!source_region || !target_region) {
                return nullptr;
            }

            SynapsePtr synapse = nullptr;

            if (source_region_id == target_region_id) {
                // Intra-region connection with explicit synapse ID
                synapse = source_region->connectNeurons(source_neuron_id, target_neuron_id, weight, type, explicit_id);
            } else {
                // Inter-region connection with explicit synapse ID
                synapse = source_region->connectToRegion(target_region, source_neuron_id, target_neuron_id, weight, type, explicit_id);
            }

            if (synapse && synapse->isValid()) {
                // Track globally, avoid duplicates
                std::lock_guard<std::mutex> lock(brain_mutex_);
                if (global_synapse_ptrs_.insert(synapse.get()).second) {
                    global_synapses_.push_back(synapse);
                }
                return synapse;
            }

            return nullptr;
        }

        void HypergraphBrain::processStep(float delta_time) {
            // Allow manual stepping regardless of is_processing_ state to support tests and single-step execution
            // Previously: if (!is_processing_.load()) return;

            // Execute pre-processing callbacks
            executePreProcessingCallbacks(delta_time);

            switch (processing_mode_) {
                case ProcessingMode::Sequential:
                    processSequential(delta_time);
                    break;
                case ProcessingMode::Parallel:
                    processParallel(delta_time);
                    break;
                case ProcessingMode::Hierarchical:
                    processHierarchical(delta_time);
                    break;
                case ProcessingMode::Custom:
                    processCustomOrder(delta_time);
                    break;
            }

            // Execute post-processing callbacks
            executePostProcessingCallbacks(delta_time);

            // Update learning system (if enabled)
            if (learning_enabled_.load(std::memory_order_relaxed) && learning_system_) {
                learning_system_->updateLearning(delta_time);
                
                // M7: Update intrinsic motivation if enabled
                if (learning_system_->getConfig().enable_intrinsic_motivation) {
                    // Collect current brain state for intrinsic motivation
                    std::vector<float> current_state;
                    
                    // Collect neuron activations from all regions - use separate scope to avoid deadlock
                    {
                        std::lock_guard<std::mutex> lock(region_mutex_);
                        for (const auto& region_pair : regions_) {
                            const auto& region = region_pair.second;
                            if (region) {
                                const auto& neurons = region->getNeurons();
                                for (const auto& neuron : neurons) {
                                    if (neuron) {
                                        current_state.push_back(neuron->getActivation());
                                    }
                                }
                            }
                        }
                    }
                    
                    // Update intrinsic motivation with current state
                    learning_system_->updateIntrinsicMotivation(current_state);
                }
            }

            // Process substrate language adaptation (Milestone 5)
            SubstrateLanguageManager::processSubstrateLanguage(this, delta_time);

            // Update statistics
            processing_cycles_.fetch_add(1, std::memory_order_relaxed);
            updateFrequencyCalculation(delta_time);

            if (hardware_monitoring_enabled_.load()) {
                updateHardwareInfo();
            }

            // M6: Optional database operations - learning continues regardless of DB state
            // These operations are for monitoring/persistence only and do not affect core learning
            if (memory_db_) {
                try {
                    // M6: Hippocampal snapshotting - check if we should take a snapshot
                    if (hippocampal_enabled_.load()) {
                        std::string context_tag = "processing_cycle_" + std::to_string(processing_cycles_.load());
                        takeHippocampalSnapshot(context_tag, false);
                    }

                    // M6: Periodic substrate state serialization (every 100 cycles)
                    if (processing_cycles_.load() % 100 == 0) {
                        // Serialize current synapse weights
                        std::ostringstream synapse_data;
                        synapse_data << "{\"synapses\":[";
                        bool first_synapse = true;
                        
                        for (const auto& synapse : global_synapses_) {
                            if (!first_synapse) synapse_data << ",";
                            auto source = synapse->getSource().lock();
                            auto target = synapse->getTarget().lock();
                            if (source && target) {
                                synapse_data << "{\"id\":" << synapse->getId() 
                                           << ",\"weight\":" << synapse->getWeight()
                                           << ",\"source_neuron\":" << source->getId()
                                           << ",\"target_neuron\":" << target->getId() << "}";
                                first_synapse = false;
                            }
                        }
                        synapse_data << "]}";

                        // Serialize current neuron states
                        std::ostringstream neuron_data;
                        neuron_data << "{\"neurons\":[";
                        bool first_neuron = true;
                        
                        for (const auto& [region_id, region] : regions_) {
                            for (const auto& neuron : region->getNeurons()) {
                                if (neuron) {
                                    if (!first_neuron) neuron_data << ",";
                                    neuron_data << "{\"region_id\":" << region_id
                                              << ",\"neuron_id\":" << neuron->getId()
                                              << ",\"activation\":" << neuron->getActivation()
                                              << ",\"threshold\":" << neuron->getThreshold()
                                              << ",\"decay_rate\":" << neuron->getDecayRate() << "}";
                                    first_neuron = false;
                                }
                            }
                        }
                        neuron_data << "]}";

                        // Store substrate states in database - failure does not affect learning
                        std::int64_t state_id = -1;
                        memory_db_->insertSubstrateState(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now().time_since_epoch()).count(),
                            processing_cycles_.load(),
                            "synapse_weights",
                            "global",
                            synapse_data.str(),
                            memory_db_run_id_,
                            state_id
                        );

                        memory_db_->insertSubstrateState(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now().time_since_epoch()).count(),
                            processing_cycles_.load(),
                            "neuron_states",
                            "global",
                            neuron_data.str(),
                            memory_db_run_id_,
                            state_id
                        );
                    }
                } catch (const std::exception& e) {
                    // Database operations are optional - log error but continue learning
                    std::cerr << "Warning: Database operation failed (learning continues): " << e.what() << std::endl;
                }
            }
            // Note: Learning system operates independently of database state
        }

        void HypergraphBrain::processSequential(float delta_time) {
            static const bool debug_regions = []() {
                const char* v = std::getenv("NF_DEBUG_REGIONS");
                return v && std::string(v) == "1";
            }();

            std::vector<RegionPtr> region_ptrs;
            {
                std::lock_guard<std::mutex> lock(region_mutex_);
                region_ptrs.reserve(regions_.size());
                for (auto& kv : regions_) {
                    if (kv.second) {
                        region_ptrs.push_back(kv.second);
                    }
                }
            }

            for (auto& region : region_ptrs) {
                if (region) {
                    if (debug_regions) {
                        std::cout << "[Region Begin] name=" << region->getName() << std::endl;
                    }
                    const auto start = std::chrono::steady_clock::now();
                    region->process(delta_time);
                    const auto end = std::chrono::steady_clock::now();
                    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                    if (ms >= 1000) {
                        std::cout << "[Slow Region] name=" << region->getName() << " ms=" << ms << std::endl;
                    }
                    if (debug_regions) {
                        std::cout << "[Region End] name=" << region->getName() << std::endl;
                    }
                }
            }
        }

        void HypergraphBrain::processParallel(float delta_time) {
            // Enhanced parallel processing with better thread management
            const size_t num_regions = regions_.size();
            if (num_regions == 0) return;
            
            const size_t hardware_threads = std::thread::hardware_concurrency();
            const size_t optimal_threads = std::min(hardware_threads, num_regions);
            
            std::vector<std::future<void>> futures;
            std::vector<RegionPtr> region_ptrs;
            
            {
                std::lock_guard<std::mutex> lock(region_mutex_);
                region_ptrs.reserve(num_regions);
                for (auto &kv : regions_) {
                    if (kv.second) {
                        region_ptrs.push_back(kv.second);
                    }
                }
            }
            
            // Use std::execution::par_unseq for vectorized parallel processing
            std::for_each(std::execution::par_unseq, 
                         region_ptrs.begin(), 
                         region_ptrs.end(),
                         [delta_time](RegionPtr& region) {
                             if (region) {
                                 region->process(delta_time);
                             }
                         });
        }

        void HypergraphBrain::processHierarchical(float delta_time) {
            // Placeholder for hierarchical processing implementation
            processSequential(delta_time);
        }

        void HypergraphBrain::processCustomOrder(float delta_time) {
            // Placeholder for custom order processing
            processSequential(delta_time);
        }

        void HypergraphBrain::executePreProcessingCallbacks(float delta_time) {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            for (const auto &cb : pre_processing_callbacks_) {
                if (cb) cb(*this, delta_time);
            }
        }

        void HypergraphBrain::executePostProcessingCallbacks(float delta_time) {
            std::lock_guard<std::mutex> lock(callback_mutex_);
            for (const auto &cb : post_processing_callbacks_) {
                if (cb) cb(*this, delta_time);
            }
        }

        void HypergraphBrain::updateFrequencyCalculation(float delta_time) {
            if (delta_time > 0.0f) {
                actual_frequency_.store(1.0f / delta_time, std::memory_order_relaxed);
            }
        }

        bool HypergraphBrain::initializeLearning(const NeuroForge::Core::LearningSystem::Config& config) {
            if (learning_system_) {
                // Reconfigure existing learning system instead of failing, so callers that
                // switch competence modes or learning parameters can proceed without full teardown.
                learning_system_->updateConfig(config);
                learning_enabled_.store(true, std::memory_order_relaxed);
                return true;
            }
            learning_system_ = std::make_unique<LearningSystem>(this, config);
            const bool ok = learning_system_->initialize();
            learning_enabled_.store(ok, std::memory_order_relaxed);
            if (ok) {
                // Wire neuron spike events to the learning system with timestamp for STDP
                Neuron::setSpikeCallback([this](NeuroForge::NeuronID neuron_id) {
                    auto* ls = learning_system_.get();
                    if (ls && learning_enabled_.load(std::memory_order_relaxed)) {
                        auto tnow = std::chrono::steady_clock::now();
                        ls->onNeuronSpike(neuron_id, tnow);
                        if (spike_observer_) {
                            spike_observer_(neuron_id, tnow);
                        }
                    }
                });

                // Accumulate eligibility traces after each processing step using current neuron activations
                addPostProcessingCallback([this](const HypergraphBrain& /*brain*/, float /*delta_time*/) {
                    if (!learning_enabled_.load(std::memory_order_relaxed)) return;
                    auto* ls = learning_system_.get();
                    if (!ls) return;

                    // Respect LearningSystem toggle for automatic accumulation
                    if (!ls->isAutoEligibilityAccumulationEnabled()) return;

                    // Take a snapshot of global synapses under lock to avoid holding the lock during iteration
                    std::vector<SynapsePtr> syns_snapshot;
                    {
                        std::lock_guard<std::mutex> lock(brain_mutex_);
                        syns_snapshot = global_synapses_;
                    }

                    for (const auto& s : syns_snapshot) {
                        if (!s || !s->isValid()) continue;
                        auto src = s->getSource().lock();
                        auto tgt = s->getTarget().lock();
                        if (!src || !tgt) continue;

                        const float pre = src->getActivation();
                        const float post = tgt->getActivation();

                        // Only record if there is any activity to reduce overhead
                        if (pre != 0.0f || post != 0.0f) {
                            ls->notePrePost(s->getId(), pre, post);
                        }
                    }
                });
            }
            return ok;
        }

        void HypergraphBrain::applyHebbianLearning(NeuroForge::RegionID region_id, float learning_rate) {
            auto region = getRegion(region_id);
            if (region && learning_system_) {
                learning_system_->applyHebbianLearning(region_id, learning_rate);
            }
        }

        void HypergraphBrain::consolidateMemories(const std::vector<NeuroForge::RegionID>& regions) {
            if (learning_system_) {
                learning_system_->consolidateMemories(regions);
            }
        }

        void HypergraphBrain::applyAttentionModulation(
            const std::unordered_map<NeuroForge::NeuronID, float>& attention_map,
            float learning_boost) {
            if (learning_system_) {
                learning_system_->applyAttentionModulation(attention_map, learning_boost);
            }
        }

        void HypergraphBrain::biasNeuronActivation(NeuroForge::NeuronID neuron_id, float influence_strength) {
            // Minimal language-to-neuron influence implemented via attention modulation
            if (!learning_system_ || influence_strength == 0.0f) {
                return;
            }

            std::unordered_map<NeuroForge::NeuronID, float> attention_map;
            attention_map.emplace(neuron_id, influence_strength);
            learning_system_->applyAttentionModulation(attention_map, 0.0f);
        }

        std::optional<NeuroForge::Core::LearningSystem::Statistics> HypergraphBrain::getLearningStatistics() const {
            if (learning_system_) {
                return learning_system_->getStatistics();
            }
            return std::nullopt;
        }

        // ===== Mimicry shaping bridge (forward to LearningSystem) =====
        void HypergraphBrain::setMimicryEnabled(bool enabled) {
            auto* ls = learning_system_.get();
            if (ls) {
                ls->setMimicryEnabled(enabled);
            }
        }

        void HypergraphBrain::setMimicryWeight(float mu) {
            auto* ls = learning_system_.get();
            if (ls) {
                ls->setMimicryWeight(mu);
            }
        }

        void HypergraphBrain::setTeacherVector(const std::vector<float>& teacher) {
            auto* ls = learning_system_.get();
            if (ls) {
                ls->setTeacherVector(teacher);
            }
        }

        void HypergraphBrain::setStudentEmbedding(const std::vector<float>& student) {
            auto* ls = learning_system_.get();
            if (ls) {
                ls->setStudentEmbedding(student);
            }
        }

        void HypergraphBrain::setMimicryInternal(bool enabled) {
            auto* ls = learning_system_.get();
            if (ls) {
                ls->setMimicryInternal(enabled);
            }
        }

        void HypergraphBrain::setMimicryAttemptScores(float similarity, float novelty, float total_reward, bool success) {
            auto* ls = learning_system_.get();
            if (ls) {
                ls->setMimicryAttemptScores(similarity, novelty, total_reward, success);
            }
        }

        float HypergraphBrain::getLastMimicrySimilarity() const {
            auto* ls = learning_system_.get();
            if (ls) {
                return ls->getLastMimicrySim();
            }
            return 0.0f;
        }

        bool HypergraphBrain::saveCheckpoint(const std::string& filepath, bool /*pretty*/) const {
            namespace fs = std::filesystem;
            try {
                // If filepath contains path separators, use as-is; otherwise, place in BrainState directory
                fs::path target;
                if (filepath.find('/') != std::string::npos || filepath.find('\\') != std::string::npos) {
                    target = fs::path(filepath);
                } else {
                    target = fs::path("BrainState") / filepath;
                }
                
                // Ensure BrainState directory exists
                if (target.has_parent_path()) {
                    fs::create_directories(target.parent_path());
                }
                
                std::string ext = target.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            
                // Write Cap'n Proto BrainStateFile format
                if (ext == ".capnp") {
#ifdef NF_HAVE_CAPNP
                    std::vector<uint8_t> bin;
                    if (!exportToBrainStateCapnp(bin)) {
                        return false;
                    }
            
                    fs::path tmp = target;
                    tmp += ".tmp";
                    {
                        std::ofstream ofs(tmp, std::ios::out | std::ios::trunc | std::ios::binary);
                        if (!ofs) {
                            return false;
                        }
                        if (!bin.empty()) {
                            ofs.write(reinterpret_cast<const char*>(bin.data()), static_cast<std::streamsize>(bin.size()));
                        }
                        ofs.flush();
                        if (!ofs.good()) {
                            return false;
                        }
                    }
            
                    std::error_code ec;
                    std::filesystem::remove(target, ec); // ignore if not exists
                    ec.clear();
                    std::filesystem::rename(tmp, target, ec);
                    if (ec) {
                        ec.clear();
                        std::filesystem::copy_file(tmp, target, std::filesystem::copy_options::overwrite_existing, ec);
                        if (ec) {
                            return false;
                        }
                        std::filesystem::remove(tmp, ec);
                    }
                    return true;
#else
                    return false; // Cap'n Proto not available
#endif // NF_HAVE_CAPNP
                }
                // Write Cap'n Proto binary if extension indicates NeuroForge binary snapshot
                if (ext == ".nfbin") {
#ifdef NF_HAVE_CAPNP
                    std::vector<uint8_t> bin;
                    if (!exportToCapnp(bin)) {
                        return false;
                    }
            
                    fs::path tmp = target;
                    tmp += ".tmp";
                    {
                        std::ofstream ofs(tmp, std::ios::out | std::ios::trunc | std::ios::binary);
                        if (!ofs) {
                            return false;
                        }
                        if (!bin.empty()) {
                            ofs.write(reinterpret_cast<const char*>(bin.data()), static_cast<std::streamsize>(bin.size()));
                        }
                        ofs.flush();
                        if (!ofs.good()) {
                            return false;
                        }
                    }
            
                    std::error_code ec;
                    std::filesystem::remove(target, ec); // ignore if not exists
                    ec.clear();
                    std::filesystem::rename(tmp, target, ec);
                    if (ec) {
                        ec.clear();
                        std::filesystem::copy_file(tmp, target, std::filesystem::copy_options::overwrite_existing, ec);
                        if (ec) {
                            return false;
                        }
                        std::filesystem::remove(tmp, ec);
                    }
                    return true;
#else
                    return false; // Cap'n Proto not available
#endif // NF_HAVE_CAPNP
                }
                // Default: write JSON text snapshot
                std::string json_data = exportToJson();
            
                fs::path tmp = target;
                tmp += ".tmp";
                {
                    std::ofstream ofs(tmp, std::ios::out | std::ios::trunc | std::ios::binary);
                    if (!ofs) {
                        return false;
                    }
                    if (!json_data.empty()) {
                        ofs.write(json_data.data(), static_cast<std::streamsize>(json_data.size()));
                    }
                    ofs.flush();
                    if (!ofs.good()) {
                        return false;
                    }
                }
            
                std::error_code ec;
                std::filesystem::remove(target, ec); // ignore errors if file doesn't exist
                ec.clear();
                std::filesystem::rename(tmp, target, ec);
                if (ec) {
                    // Fallback: copy + remove
                    ec.clear();
                    std::filesystem::copy_file(tmp, target, std::filesystem::copy_options::overwrite_existing, ec);
                    if (ec) {
                        return false;
                    }
                    std::filesystem::remove(tmp, ec);
                }
                return true;
            } catch (...) {
                return false;
            }
        }
        
        bool HypergraphBrain::loadCheckpoint(const std::string& filepath) {
            namespace fs = std::filesystem;
            try {
                // If filepath contains path separators, use as-is; otherwise, look in BrainState directory
                fs::path target;
                if (filepath.find('/') != std::string::npos || filepath.find('\\') != std::string::npos) {
                    target = fs::path(filepath);
                } else {
                    target = fs::path("BrainState") / filepath;
                }
                
                std::string ext = target.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
            
                // Load Cap'n Proto BrainStateFile wrapper
                if (ext == ".capnp") {
#ifdef NF_HAVE_CAPNP
                    std::ifstream ifs(target, std::ios::in | std::ios::binary);
                    if (!ifs) {
                        return false;
                    }
                    ifs.seekg(0, std::ios::end);
                    std::streampos len = ifs.tellg();
                    if (len < 0) {
                        return false;
                    }
                    ifs.seekg(0, std::ios::beg);
                    std::vector<uint8_t> buf(static_cast<std::size_t>(len));
                    if (!buf.empty()) {
                        ifs.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
                    }
                    if (!ifs.good() && !ifs.eof()) {
                        return false;
                    }
                    return importFromBrainStateCapnp(buf.data(), buf.size());
#else
                    return false; // Cap'n Proto not available
#endif // NF_HAVE_CAPNP
                }
                
                if (ext == ".nfbin") {
#ifdef NF_HAVE_CAPNP
                    std::ifstream ifs(target, std::ios::in | std::ios::binary);
                    if (!ifs) {
                        return false;
                    }
                    ifs.seekg(0, std::ios::end);
                    std::streampos len = ifs.tellg();
                    if (len < 0) {
                        return false;
                    }
                    ifs.seekg(0, std::ios::beg);
                    std::vector<uint8_t> buf(static_cast<std::size_t>(len));
                    if (!buf.empty()) {
                        ifs.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
                    }
                    if (!ifs.good() && !ifs.eof()) {
                        return false;
                    }
                    return importFromCapnp(buf.data(), buf.size());
#else
                    return false; // Cap'n Proto not available
#endif // NF_HAVE_CAPNP
                }
                // Default: load JSON snapshot
                std::ifstream ifs(target, std::ios::in | std::ios::binary);
                if (!ifs) {
                    return false;
                }
                ifs.seekg(0, std::ios::end);
                std::streampos len = ifs.tellg();
                if (len < 0) {
                    return false;
                }
                ifs.seekg(0, std::ios::beg);
                std::string json_data;
                json_data.resize(static_cast<std::size_t>(len));
                if (!json_data.empty()) {
                    ifs.read(&json_data[0], static_cast<std::streamsize>(json_data.size()));
                }
                if (!ifs.good() && !ifs.eof()) {
                    return false;
                }
#ifdef NF_HAVE_CAPNP
                return importFromJson(json_data);
#else
                return false; // importFromJson not available without Cap'n Proto
#endif // NF_HAVE_CAPNP
            } catch (...) {
                return false;
            }
        }
        
        // JSON export/import helpers and implementations
        std::string HypergraphBrain::getBrainStateString() const {
            switch (brain_state_.load()) {
                case BrainState::Uninitialized: return "Uninitialized";
                case BrainState::Initializing:  return "Initializing";
                case BrainState::Running:       return "Running";
                case BrainState::Paused:        return "Paused";
                case BrainState::Resetting:     return "Resetting";
                case BrainState::Shutdown:      return "Shutdown";
                default:                        return "Unknown";
            }
        }
        
        std::string HypergraphBrain::getProcessingModeString() const {
            switch (processing_mode_) {
                case ProcessingMode::Sequential:    return "Sequential";
                case ProcessingMode::Parallel:      return "Parallel";
                case ProcessingMode::Hierarchical:  return "Hierarchical";
                case ProcessingMode::Custom:        return "Custom";
                default:                            return "Unknown";
            }
        }
        
        HypergraphBrain::HardwareInfo HypergraphBrain::getHardwareInfo() const {
            return hardware_info_;
        }
        
        std::size_t HypergraphBrain::getTotalMemoryUsage() const {
            std::size_t total_memory = 0;
            // Sum memory from all regions
            for (const auto& [region_id, region] : regions_) {
                if (region) {
                    total_memory += region->getMemoryUsage();
                }
            }
            // Add global synapse container overhead (pointer storage)
            total_memory += global_synapses_.size() * sizeof(SynapsePtr);
            return total_memory;
        }
        
        std::string HypergraphBrain::exportToJson() const {
            std::ostringstream json;
            json << "{\n";
            json << "  \"brain_state\": \"" << getBrainStateString() << "\",\n";
            json << "  \"processing_mode\": \"" << getProcessingModeString() << "\",\n";
            json << "  \"target_frequency\": " << target_frequency_.load() << ",\n";
            json << "  \"actual_frequency\": " << actual_frequency_.load() << ",\n";
            json << "  \"processing_cycles\": " << processing_cycles_.load() << ",\n";
            json << "  \"regions\": [\n";
        
            bool first_region = true;
            for (const auto& [region_id, region] : regions_) {
                if (!region) continue;
                if (!first_region) json << ",\n";
                json << "    {\n";
                json << "      \"id\": " << region_id << ",\n";
                json << "      \"name\": \"" << region->getName() << "\",\n";
                json << "      \"neuron_count\": " << region->getNeuronCount() << "\n";
                json << "    }";
                first_region = false;
            }
        
            json << "\n  ],\n";
        
            // Serialize synapses with region-local neuron indices
            json << "  \"synapses\": [\n";
            bool first_syn = true;
            auto findRegionAndIndex = [&](NeuronID nid, RegionID& out_rid, std::size_t& out_index) -> bool {
                for (const auto& [rid, region] : regions_) {
                    if (!region) continue;
                    auto n = region->getNeuron(nid);
                    if (n) {
                        const auto& neurons = region->getNeurons();
                        for (std::size_t i = 0; i < neurons.size(); ++i) {
                            if (neurons[i] && neurons[i]->getId() == nid) {
                                out_rid = rid;
                                out_index = i;
                                return true;
                            }
                        }
                    }
                }
                return false;
            };
        
            for (const auto& s : global_synapses_) {
                if (!s || !s->isValid()) continue;
                auto src = s->getSource().lock();
                auto tgt = s->getTarget().lock();
                if (!src || !tgt) continue;
        
                RegionID src_rid = 0, tgt_rid = 0;
                std::size_t src_idx = 0, tgt_idx = 0;
                if (!findRegionAndIndex(src->getId(), src_rid, src_idx)) continue;
                if (!findRegionAndIndex(tgt->getId(), tgt_rid, tgt_idx)) continue;
        
                if (!first_syn) json << ",\n";
                json << "    {\n";
                json << "      \"id\": " << s->getId() << ",\n";
                json << "      \"source_region\": " << src_rid << ",\n";
                json << "      \"target_region\": " << tgt_rid << ",\n";
                json << "      \"source_neuron_index\": " << static_cast<unsigned long long>(src_idx) << ",\n";
                json << "      \"target_neuron_index\": " << static_cast<unsigned long long>(tgt_idx) << ",\n";
                json << "      \"weight\": " << s->getWeight() << ",\n";
                json << "      \"type\": " << static_cast<int>(s->getType()) << "\n";
                json << "    }";
                first_syn = false;
            }
            json << "\n  ]\n";
            json << "}";
        
            return json.str();
        }

#ifdef NF_HAVE_CAPNP
        bool HypergraphBrain::importFromJson(const std::string& json_data) {
            // Minimal parser for the exact format produced by exportToJson()
            // 1) Stop processing and clear current state
            stop();
            {
                std::lock_guard<std::mutex> lock(brain_mutex_);
                {
                    std::lock_guard<std::mutex> rlock(region_mutex_);
                    regions_.clear();
                    region_names_.clear();
                }
                global_synapses_.clear();
                processing_cycles_.store(0, std::memory_order_relaxed);
                actual_frequency_.store(0.0f, std::memory_order_relaxed);
            }
        
            auto trim = [](const std::string& s) -> std::string {
                size_t b = 0, e = s.size();
                while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
                while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
                return s.substr(b, e - b);
            };
        
            auto getStringValue = [](const std::string& obj, const std::string& key, std::string& out) -> bool {
                std::string k = "\"" + key + "\"";
                std::size_t p = obj.find(k);
                if (p == std::string::npos) return false;
                p = obj.find(':', p);
                if (p == std::string::npos) return false;
                p = obj.find('"', p);
                if (p == std::string::npos) return false;
                std::size_t q = obj.find('"', p + 1);
                if (q == std::string::npos) return false;
                out = obj.substr(p + 1, q - (p + 1));
                return true;
            };
        
            auto getNumberValue = [](const std::string& obj, const std::string& key, double& out) -> bool {
                std::string k = "\"" + key + "\"";
                std::size_t p = obj.find(k);
                if (p == std::string::npos) return false;
                p = obj.find(':', p);
                if (p == std::string::npos) return false;
                ++p;
                while (p < obj.size() && std::isspace(static_cast<unsigned char>(obj[p]))) ++p;
                std::size_t q = p;
                while (q < obj.size()) {
                    char c = obj[q];
                    if (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E') {
                        ++q;
                    } else {
                        break;
                    }
                }
                if (q == p) return false;
                try {
                    out = std::stod(obj.substr(p, q - p));
                    return true;
                } catch (...) {
                    return false;
                }
            };
        
            auto findArrayBounds = [&](const std::string& key) -> std::pair<std::size_t, std::size_t> {
                const std::string k = "\"" + key + "\"";
                std::size_t pos = json_data.find(k);
                if (pos == std::string::npos) return {std::string::npos, std::string::npos};
                std::size_t lb = json_data.find('[', pos);
                if (lb == std::string::npos) return {std::string::npos, std::string::npos};
                std::size_t rb = json_data.find(']', lb);
                if (rb == std::string::npos) return {std::string::npos, std::string::npos};
                return {lb + 1, rb};
            };
        
            // 2) Parse top-level properties
            {
                std::string state_str;
                if (getStringValue(json_data, "brain_state", state_str)) {
                    if (state_str == "Uninitialized") brain_state_.store(BrainState::Uninitialized);
                    else if (state_str == "Initializing") brain_state_.store(BrainState::Initializing);
                    else if (state_str == "Running") brain_state_.store(BrainState::Running);
                    else if (state_str == "Paused") brain_state_.store(BrainState::Paused);
                    else if (state_str == "Resetting") brain_state_.store(BrainState::Resetting);
                    else if (state_str == "Shutdown") brain_state_.store(BrainState::Shutdown);
                }
        
                std::string mode_str;
                if (getStringValue(json_data, "processing_mode", mode_str)) {
                    if (mode_str == "Sequential") processing_mode_ = ProcessingMode::Sequential;
                    else if (mode_str == "Parallel") processing_mode_ = ProcessingMode::Parallel;
                    else if (mode_str == "Hierarchical") processing_mode_ = ProcessingMode::Hierarchical;
                    else if (mode_str == "Custom") processing_mode_ = ProcessingMode::Custom;
                }
        
                double tf = 0.0, af = 0.0, pc = 0.0;
                if (getNumberValue(json_data, "target_frequency", tf)) {
                    target_frequency_.store(static_cast<float>(tf));
                }
                if (getNumberValue(json_data, "actual_frequency", af)) {
                    actual_frequency_.store(static_cast<float>(af));
                }
                if (getNumberValue(json_data, "processing_cycles", pc)) {
                    processing_cycles_.store(static_cast<std::uint64_t>(pc));
                }
            }
        
            // 3) Parse regions array
            auto [rBegin, rEnd] = findArrayBounds("regions");
            if (rBegin != std::string::npos && rEnd != std::string::npos && rBegin <= rEnd) {
                std::size_t i = rBegin;
                while (true) {
                    std::size_t lb = json_data.find('{', i);
                    if (lb == std::string::npos || lb >= rEnd) break;
                    std::size_t rb = json_data.find('}', lb);
                    if (rb == std::string::npos || rb > rEnd) break;
                    std::string obj = json_data.substr(lb + 1, rb - lb - 1);
                    obj = trim(obj);
        
                    double id_num = 0.0, neuron_cnt = 0.0;
                    std::string name_str;
                    bool ok_id = getNumberValue(obj, "id", id_num);
                    bool ok_name = getStringValue(obj, "name", name_str);
                    bool ok_neurons = getNumberValue(obj, "neuron_count", neuron_cnt);
        
                    if (ok_id && ok_name) {
                        RegionID rid = static_cast<RegionID>(static_cast<std::uint64_t>(id_num));
                        auto region = RegionFactory::createRegion(rid, name_str, Region::Type::Custom, Region::ActivationPattern::Asynchronous);
                        if (region) {
                            addRegion(region);
                            if (ok_neurons && neuron_cnt > 0) {
                                region->createNeurons(static_cast<std::size_t>(neuron_cnt));
                            }
                        }
                    }
                    i = rb + 1;
                }
            }
        
            // 4) Parse synapses array and reconstruct connections
            auto [sBegin, sEnd] = findArrayBounds("synapses");
            if (sBegin != std::string::npos && sEnd != std::string::npos && sBegin <= sEnd) {
                std::size_t i = sBegin;
                while (true) {
                    std::size_t lb = json_data.find('{', i);
                    if (lb == std::string::npos || lb >= sEnd) break;
                    std::size_t rb = json_data.find('}', lb);
                    if (rb == std::string::npos || rb > sEnd) break;
                    std::string obj = json_data.substr(lb + 1, rb - lb - 1);
                    obj = trim(obj);
        
                    double src_region_num = 0.0, tgt_region_num = 0.0;
                    double src_index_num = 0.0, tgt_index_num = 0.0;
                    double weight_num = 0.0, type_num = 0.0;
                    double id_num = 0.0;
        
                    bool ok_sr = getNumberValue(obj, "source_region", src_region_num);
                    bool ok_tr = getNumberValue(obj, "target_region", tgt_region_num);
                    bool ok_si = getNumberValue(obj, "source_neuron_index", src_index_num);
                    bool ok_ti = getNumberValue(obj, "target_neuron_index", tgt_index_num);
                    bool ok_w = getNumberValue(obj, "weight", weight_num);
                    bool ok_ty = getNumberValue(obj, "type", type_num);
                    bool ok_id = getNumberValue(obj, "id", id_num);
        
                    if (ok_sr && ok_tr && ok_si && ok_ti && ok_w && ok_ty) {
                        RegionID srid = static_cast<RegionID>(static_cast<std::uint64_t>(src_region_num));
                        RegionID trid = static_cast<RegionID>(static_cast<std::uint64_t>(tgt_region_num));
                        std::size_t sidx = static_cast<std::size_t>(src_index_num);
                        std::size_t tidx = static_cast<std::size_t>(tgt_index_num);
                        auto sreg = getRegion(srid);
                        auto treg = getRegion(trid);
                        if (sreg && treg) {
                            const auto& neurons = sreg->getNeurons();
                            const auto& tneurons = treg->getNeurons();
                            if (sidx < neurons.size() && tidx < tneurons.size() && neurons[sidx] && tneurons[tidx]) {
                                NeuronID source_nid = neurons[sidx]->getId();
                                NeuronID target_nid = tneurons[tidx]->getId();
                                auto stype = static_cast<SynapseType>(static_cast<int>(type_num));
                                if (ok_id) {
                                    SynapseID syn_id = static_cast<SynapseID>(static_cast<std::uint64_t>(id_num));
                                    this->connectNeurons(srid, trid, source_nid, target_nid, static_cast<float>(weight_num), stype, syn_id);
                                } else {
                                    this->connectNeurons(srid, trid, source_nid, target_nid, static_cast<float>(weight_num), stype);
                                }
                            }
                        }
                    }
        
                    i = rb + 1;
                }
            }
        
            updateGlobalStatistics();
            return true;
        }
#endif // NF_HAVE_CAPNP
        
        void HypergraphBrain::addExperience(const std::string& tag,
                                                              const std::vector<float>& input,
                                                              const std::vector<float>& output,
                                                              bool significant) {
            ExperienceRecord rec;
            using namespace std::chrono;
            rec.timestamp_ms = static_cast<std::uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
            rec.step = processing_cycles_.load(std::memory_order_relaxed);
            rec.tag = tag;
            rec.input = input;
            rec.output = output;
            rec.significant = significant;
        
            experience_buffer_.push_back(std::move(rec));
        
            // Enforce capacity (ring semantics by dropping oldest)
            if (experience_capacity_ == 0) {
                experience_buffer_.clear();
                episodes_.clear();
                return;
            }
            if (experience_buffer_.size() > experience_capacity_) {
                // Remove oldest entry
                experience_buffer_.erase(experience_buffer_.begin());
                // Note: Episode indices may become stale; for now, clear episodes to avoid invalid references
                episodes_.clear();
            }
        
            // Log to MemoryDB if available
            if (memory_db_ && memory_db_run_id_ > 0) {
                // Serialize input/output as simple JSON arrays without adding a new dependency
                auto vec_to_json = [](const std::vector<float>& v) -> std::string {
                    std::string s;
                    s.reserve(v.size() * 8 + 2);
                    s.push_back('[');
                    for (size_t i = 0; i < v.size(); ++i) {
                        if (i) s.push_back(',');
                        char buf[64];
                        int n = std::snprintf(buf, sizeof(buf), "%.6f", static_cast<double>(v[i]));
                        if (n > 0) s.append(buf, static_cast<size_t>(n));
                    }
                    s.push_back(']');
                    return s;
                };
                const auto& lastExp = experience_buffer_.back();
                std::int64_t exp_id = -1;
                try {
                    (void)memory_db_->insertExperience(static_cast<std::int64_t>(lastExp.timestamp_ms),
                                                       static_cast<std::uint64_t>(lastExp.step),
                                                       lastExp.tag,
                                                       vec_to_json(lastExp.input),
                                                       vec_to_json(lastExp.output),
                                                       lastExp.significant,
                                                       memory_db_run_id_,
                                                       exp_id);
                    if (current_episode_id_ > 0 && exp_id > 0) {
                        (void)memory_db_->linkExperienceToEpisode(exp_id, current_episode_id_);
                    }
                } catch (...) {
                    // Swallow logging errors to avoid impacting core processing
                }
            }
        }
        
        void HypergraphBrain::setExperienceCapacity(std::size_t capacity) {
            experience_capacity_ = capacity;
            if (experience_capacity_ == 0) {
                experience_buffer_.clear();
                episodes_.clear();
                return;
            }
            if (experience_buffer_.size() > experience_capacity_) {
                // Keep the most recent entries up to capacity
                std::size_t to_remove = experience_buffer_.size() - experience_capacity_;
                experience_buffer_.erase(experience_buffer_.begin(), experience_buffer_.begin() + static_cast<std::ptrdiff_t>(to_remove));
                episodes_.clear();
            }
        }
        
        std::size_t HypergraphBrain::getExperienceCount() const noexcept {
            return experience_buffer_.size();
        }
        
        void HypergraphBrain::clearExperiences() {
            experience_buffer_.clear();
            episodes_.clear();
        }

#ifdef NF_HAVE_CAPNP
        // Cap'n Proto mapping functions
        NeuroForge::Schema::ProcessingMode HypergraphBrain::mapProcessingMode(HypergraphBrain::ProcessingMode mode) const {
            switch (mode) {
                case ProcessingMode::Sequential: return NeuroForge::Schema::ProcessingMode::SEQUENTIAL;
                case ProcessingMode::Parallel: return NeuroForge::Schema::ProcessingMode::PARALLEL;
                case ProcessingMode::Hierarchical: return NeuroForge::Schema::ProcessingMode::HIERARCHICAL;
                case ProcessingMode::Custom: return NeuroForge::Schema::ProcessingMode::CUSTOM;
                default: return NeuroForge::Schema::ProcessingMode::PARALLEL;
            }
        }

        HypergraphBrain::ProcessingMode HypergraphBrain::unmapProcessingMode(NeuroForge::Schema::ProcessingMode mode) const {
            switch (mode) {
                case NeuroForge::Schema::ProcessingMode::SEQUENTIAL: return ProcessingMode::Sequential;
                case NeuroForge::Schema::ProcessingMode::PARALLEL: return ProcessingMode::Parallel;
                case NeuroForge::Schema::ProcessingMode::HIERARCHICAL: return ProcessingMode::Hierarchical;
                case NeuroForge::Schema::ProcessingMode::CUSTOM: return ProcessingMode::Custom;
                default: return ProcessingMode::Parallel;
            }
        }

        NeuroForge::Schema::BrainState HypergraphBrain::mapBrainState(BrainState state) const {
            switch (state) {
                case BrainState::Uninitialized: return NeuroForge::Schema::BrainState::UNINITIALIZED;
                case BrainState::Initializing: return NeuroForge::Schema::BrainState::INITIALIZING;
                case BrainState::Running: return NeuroForge::Schema::BrainState::RUNNING;
                case BrainState::Paused: return NeuroForge::Schema::BrainState::PAUSED;
                case BrainState::Resetting: return NeuroForge::Schema::BrainState::RESETTING;
                case BrainState::Shutdown: return NeuroForge::Schema::BrainState::SHUTDOWN;
                default: return NeuroForge::Schema::BrainState::UNINITIALIZED;
            }
        }

        HypergraphBrain::BrainState HypergraphBrain::unmapBrainState(NeuroForge::Schema::BrainState state) const {
            switch (state) {
                case NeuroForge::Schema::BrainState::UNINITIALIZED: return BrainState::Uninitialized;
                case NeuroForge::Schema::BrainState::INITIALIZING: return BrainState::Initializing;
                case NeuroForge::Schema::BrainState::RUNNING: return BrainState::Running;
                case NeuroForge::Schema::BrainState::PAUSED: return BrainState::Paused;
                case NeuroForge::Schema::BrainState::RESETTING: return BrainState::Resetting;
                case NeuroForge::Schema::BrainState::SHUTDOWN: return BrainState::Shutdown;
                default: return BrainState::Uninitialized;
            }
        }

        NeuroForge::Schema::RegionType HypergraphBrain::mapRegionType(Region::Type type) const {
            switch (type) {
                case Region::Type::Cortical: return NeuroForge::Schema::RegionType::CORTICAL;
                case Region::Type::Subcortical: return NeuroForge::Schema::RegionType::SUBCORTICAL;
                case Region::Type::Brainstem: return NeuroForge::Schema::RegionType::BRAINSTEM;
                case Region::Type::Special: return NeuroForge::Schema::RegionType::SPECIAL;
                case Region::Type::Custom: return NeuroForge::Schema::RegionType::CUSTOM;
                default: return NeuroForge::Schema::RegionType::CUSTOM;
            }
        }

        Region::Type HypergraphBrain::unmapRegionType(NeuroForge::Schema::RegionType type) const {
            switch (type) {
                case NeuroForge::Schema::RegionType::CORTICAL: return Region::Type::Cortical;
                case NeuroForge::Schema::RegionType::SUBCORTICAL: return Region::Type::Subcortical;
                case NeuroForge::Schema::RegionType::BRAINSTEM: return Region::Type::Brainstem;
                case NeuroForge::Schema::RegionType::SPECIAL: return Region::Type::Special;
                case NeuroForge::Schema::RegionType::CUSTOM: return Region::Type::Custom;
                default: return Region::Type::Custom;
            }
        }

        NeuroForge::Schema::ActivationPattern HypergraphBrain::mapActivationPattern(Region::ActivationPattern pattern) const {
            switch (pattern) {
                case Region::ActivationPattern::Synchronous: return NeuroForge::Schema::ActivationPattern::SYNCHRONOUS;
                case Region::ActivationPattern::Asynchronous: return NeuroForge::Schema::ActivationPattern::ASYNCHRONOUS;
                case Region::ActivationPattern::Layered: return NeuroForge::Schema::ActivationPattern::LAYERED;
                case Region::ActivationPattern::Competitive: return NeuroForge::Schema::ActivationPattern::COMPETITIVE;
                case Region::ActivationPattern::Oscillatory: return NeuroForge::Schema::ActivationPattern::OSCILLATORY;
                default: return NeuroForge::Schema::ActivationPattern::ASYNCHRONOUS;
            }
        }

        Region::ActivationPattern HypergraphBrain::unmapActivationPattern(NeuroForge::Schema::ActivationPattern pattern) const {
            switch (pattern) {
                case NeuroForge::Schema::ActivationPattern::SYNCHRONOUS: return Region::ActivationPattern::Synchronous;
                case NeuroForge::Schema::ActivationPattern::ASYNCHRONOUS: return Region::ActivationPattern::Asynchronous;
                case NeuroForge::Schema::ActivationPattern::LAYERED: return Region::ActivationPattern::Layered;
                case NeuroForge::Schema::ActivationPattern::COMPETITIVE: return Region::ActivationPattern::Competitive;
                case NeuroForge::Schema::ActivationPattern::OSCILLATORY: return Region::ActivationPattern::Oscillatory;
                default: return Region::ActivationPattern::Asynchronous;
            }
        }

        NeuroForge::Schema::NeuronState HypergraphBrain::mapNeuronState(Neuron::State state) const {
            switch (state) {
                case Neuron::State::Inactive: return NeuroForge::Schema::NeuronState::INACTIVE;
                case Neuron::State::Active: return NeuroForge::Schema::NeuronState::ACTIVE;
                case Neuron::State::Refractory: return NeuroForge::Schema::NeuronState::REFRACTORY;
                case Neuron::State::Inhibited: return NeuroForge::Schema::NeuronState::INHIBITED;
                default: return NeuroForge::Schema::NeuronState::INACTIVE;
            }
        }

        Neuron::State HypergraphBrain::unmapNeuronState(NeuroForge::Schema::NeuronState state) const {
            switch (state) {
                case NeuroForge::Schema::NeuronState::INACTIVE: return Neuron::State::Inactive;
                case NeuroForge::Schema::NeuronState::ACTIVE: return Neuron::State::Active;
                case NeuroForge::Schema::NeuronState::REFRACTORY: return Neuron::State::Refractory;
                case NeuroForge::Schema::NeuronState::INHIBITED: return Neuron::State::Inhibited;
                default: return Neuron::State::Inactive;
            }
        }

        NeuroForge::Schema::SynapseType HypergraphBrain::mapSynapseType(NeuroForge::SynapseType type) const {
            switch (type) {
                case NeuroForge::SynapseType::Excitatory: return NeuroForge::Schema::SynapseType::EXCITATORY;
                case NeuroForge::SynapseType::Inhibitory: return NeuroForge::Schema::SynapseType::INHIBITORY;
                case NeuroForge::SynapseType::Modulatory: return NeuroForge::Schema::SynapseType::MODULATORY;
                default: return NeuroForge::Schema::SynapseType::EXCITATORY;
            }
        }

        NeuroForge::SynapseType HypergraphBrain::unmapSynapseType(NeuroForge::Schema::SynapseType type) const {
            switch (type) {
                case NeuroForge::Schema::SynapseType::EXCITATORY: return NeuroForge::SynapseType::Excitatory;
                case NeuroForge::Schema::SynapseType::INHIBITORY: return NeuroForge::SynapseType::Inhibitory;
                case NeuroForge::Schema::SynapseType::MODULATORY: return NeuroForge::SynapseType::Modulatory;
                default: return NeuroForge::SynapseType::Excitatory;
            }
        }

        NeuroForge::Schema::PlasticityRule HypergraphBrain::mapPlasticityRule(Synapse::PlasticityRule rule) const {

            switch (rule) {
                case Synapse::PlasticityRule::None:     return NeuroForge::Schema::PlasticityRule::NONE;
                case Synapse::PlasticityRule::Hebbian:  return NeuroForge::Schema::PlasticityRule::HEBBIAN;
                case Synapse::PlasticityRule::STDP:     return NeuroForge::Schema::PlasticityRule::STDP;
                case Synapse::PlasticityRule::BCM:      return NeuroForge::Schema::PlasticityRule::BCM;
                case Synapse::PlasticityRule::Oja:      return NeuroForge::Schema::PlasticityRule::OJA;
                default:                                return NeuroForge::Schema::PlasticityRule::NONE;
            }
        }

        Synapse::PlasticityRule HypergraphBrain::unmapPlasticityRule(NeuroForge::Schema::PlasticityRule rule) const {
            switch (rule) {
                case NeuroForge::Schema::PlasticityRule::NONE:     return Synapse::PlasticityRule::None;
                case NeuroForge::Schema::PlasticityRule::HEBBIAN:  return Synapse::PlasticityRule::Hebbian;
                case NeuroForge::Schema::PlasticityRule::STDP:     return Synapse::PlasticityRule::STDP;
                case NeuroForge::Schema::PlasticityRule::BCM:      return Synapse::PlasticityRule::BCM;
                case NeuroForge::Schema::PlasticityRule::OJA:      return Synapse::PlasticityRule::Oja;
                default:                                          return Synapse::PlasticityRule::None;
            }
        }

        void HypergraphBrain::serializeSynapse(NeuroForge::Schema::Synapse::Builder& builder, const SynapsePtr& synapse) const {
            if (!synapse) return;
            
            builder.setId(synapse->getId());
            
            // Determine actual source/target region IDs by checking neuron membership in regions
            uint32_t sourceRegionId = 0;
            uint32_t targetRegionId = 0;
            
            auto source = synapse->getSource().lock();
            auto target = synapse->getTarget().lock();
            
            if (source) {
                const auto srcId = source->getId();
                for (const auto& region_pair : regions_) {
                    if (region_pair.second->getNeuron(srcId)) {
                        sourceRegionId = region_pair.first;
                        break;
                    }
                }
            }
            if (target) {
                const auto tgtId = target->getId();
                for (const auto& region_pair : regions_) {
                    if (region_pair.second->getNeuron(tgtId)) {
                        targetRegionId = region_pair.first;
                        break;
                    }
                }
            }

            builder.setSourceRegionId(sourceRegionId);
            builder.setTargetRegionId(targetRegionId);
            
            // Set actual neuron IDs from weak pointers
            builder.setSourceNeuronId(source ? source->getId() : 0);
            builder.setTargetNeuronId(target ? target->getId() : 0);
            builder.setWeight(synapse->getWeight());
            builder.setInitialWeight(synapse->getWeight()); // Use current weight as initial
            builder.setType(mapSynapseType(synapse->getType()));
            builder.setPlasticityRule(mapPlasticityRule(synapse->getPlasticityRule()));
            builder.setLearningRate(synapse->getLearningRate());
            builder.setDelayMs(synapse->getDelay());
            builder.setMinWeight(synapse->getMinWeight());
            builder.setMaxWeight(synapse->getMaxWeight());
            // Statistics are optional for now
            builder.setSignalCount(0);
            builder.setUpdateCount(0);
        }

        SynapsePtr HypergraphBrain::deserializeSynapse(
            const NeuroForge::Schema::Synapse::Reader& reader,
            const std::unordered_map<uint32_t, std::unordered_map<uint64_t, NeuronPtr>>& regionNeuronMap) const {
            
            auto synapse_id = reader.getId();
            auto source_neuron_id = reader.getSourceNeuronId();
            auto target_neuron_id = reader.getTargetNeuronId();
            auto weight = reader.getWeight();
            auto type = unmapSynapseType(reader.getType());
            auto plasticity_rule = unmapPlasticityRule(reader.getPlasticityRule());
            auto learning_rate = reader.getLearningRate();
            auto delay_ms = reader.getDelayMs();
            auto min_weight = reader.getMinWeight();
            auto max_weight = reader.getMaxWeight();
            
            // Find source and target neurons
            NeuronPtr source_neuron = nullptr;
            NeuronPtr target_neuron = nullptr;
            
            for (const auto& region_pair : regionNeuronMap) {
                const auto& neuron_map = region_pair.second;
                if (!source_neuron) {
                    auto src_it = neuron_map.find(source_neuron_id);
                    if (src_it != neuron_map.end()) {
                        source_neuron = src_it->second;
                    }
                }
                if (!target_neuron) {
                    auto tgt_it = neuron_map.find(target_neuron_id);
                    if (tgt_it != neuron_map.end()) {
                        target_neuron = tgt_it->second;
                    }
                }
                if (source_neuron && target_neuron) break;
            }
            
            if (!source_neuron || !target_neuron) {
                return nullptr; // Cannot create synapse without valid neurons
            }
            
            // Create the synapse
            auto synapse = SynapseFactory::createSynapse(synapse_id, source_neuron, target_neuron, weight, type);
            synapse->setPlasticityRule(plasticity_rule);
            synapse->setLearningRate(learning_rate);
            synapse->setDelay(delay_ms);
            synapse->setWeightBounds(min_weight, max_weight);
            
            return synapse;
        }

        bool HypergraphBrain::exportToCapnp(std::vector<uint8_t>& outBuffer) const {
            try {
                std::lock_guard<std::mutex> lock(brain_mutex_);
                
                capnp::MallocMessageBuilder message;
                auto brain_builder = message.initRoot<NeuroForge::Schema::Brain>();
                
                // Set brain properties from available schema fields
                brain_builder.setFormatVersion(1);
                brain_builder.setProcessingMode(mapProcessingMode(processing_mode_));
                brain_builder.setBrainState(mapBrainState(brain_state_.load()));
                brain_builder.setTargetFrequency(target_frequency_.load());
                brain_builder.setProcessingCycles(processing_cycles_.load());
                
                // Serialize regions
                auto regions_list = brain_builder.initRegions(static_cast<capnp::uint>(regions_.size()));
                capnp::uint region_idx = 0;
                for (const auto& region_pair : regions_) {
                    auto region_builder = regions_list[region_idx++];
                    const auto& region = region_pair.second;
                    
                    region_builder.setId(region->getId());
                    region_builder.setName(region->getName());
                    region_builder.setType(mapRegionType(region->getType()));
                    region_builder.setActivationPattern(mapActivationPattern(region->getActivationPattern()));
                    
                    // Serialize neurons in this region
                    const auto& neurons = region->getNeurons();
                    auto neurons_list = region_builder.initNeurons(static_cast<capnp::uint>(neurons.size()));
                    capnp::uint neuron_idx = 0;
                    for (const auto& neuron : neurons) {
                        auto neuron_builder = neurons_list[neuron_idx++];
                        neuron_builder.setId(neuron->getId());
                        neuron_builder.setState(mapNeuronState(neuron->getState()));
                        neuron_builder.setThreshold(neuron->getThreshold());
                        neuron_builder.setActivation(neuron->getActivation());
                        neuron_builder.setDecayRate(neuron->getDecayRate());
                        neuron_builder.setRefractoryTimer(neuron->getRefractoryTimer());
                        neuron_builder.setRefractoryPeriod(neuron->getRefractoryPeriod());
                    }
                    
                    // Serialize internal synapses for this region
                    const auto& internal_synapses = region->getInternalSynapses();
                    auto internal_synapses_list = region_builder.initInternalSynapses(static_cast<capnp::uint>(internal_synapses.size()));
                    capnp::uint internal_syn_idx = 0;
                    for (const auto& synapse : internal_synapses) {
                        auto synapse_builder = internal_synapses_list[internal_syn_idx++];
                        serializeSynapse(synapse_builder, synapse);
                    }
                }
                
                // Serialize inter-region synapses by scanning regions' inter-region maps to ensure
                // we capture connections even if callers created them directly on Region.
                std::vector<SynapsePtr> inter_synapses;
                inter_synapses.reserve(global_synapses_.size()); // best-effort initial reserve
                std::vector<const Synapse*> seen_ptrs;
                for (const auto& kv : regions_) {
                    const auto& region = kv.second;
                    const auto& inter_map = region->getInterRegionConnections();
                    for (const auto& it : inter_map) {
                        const auto& vec = it.second;
                        for (const auto& s : vec) {
                            const Synapse* raw = s.get();
                            if (!raw) continue;
                            bool present = false;
                            for (const auto* r : seen_ptrs) {
                                if (r == raw) { present = true; break; }
                            }
                            if (!present) {
                                seen_ptrs.push_back(raw);
                                inter_synapses.push_back(s);
                            }
                        }
                    }
                }

                auto global_synapses_list = brain_builder.initGlobalSynapses(static_cast<capnp::uint>(inter_synapses.size()));
                capnp::uint global_syn_idx = 0;
                for (const auto& synapse : inter_synapses) {
                    auto synapse_builder = global_synapses_list[global_syn_idx++];
                    serializeSynapse(synapse_builder, synapse);
                }
                
                // Serialize to a flat array of words to ensure compatibility with FlatArrayMessageReader
                auto flatArray = capnp::messageToFlatArray(message);
                auto words = flatArray.asPtr();
                auto bytes = words.asBytes();
                outBuffer.assign(bytes.begin(), bytes.end());
                return true;
                
            } catch (const std::exception&) {
                // Handle serialization errors
                return false;
            }
        }

        bool HypergraphBrain::importFromCapnp(const uint8_t* data, size_t size) {
            try {
                std::cerr << "[capnp] import start, size=" << size << std::endl;
                
                // Ensure word alignment by copying bytes into a word-aligned buffer
                const size_t wordCount = (size + sizeof(capnp::word) - 1) / sizeof(capnp::word);
                std::cerr << "[capnp] wordCount=" << wordCount << std::endl;
                kj::Array<capnp::word> wordBuffer = kj::heapArray<capnp::word>(wordCount);
                auto bytesView = wordBuffer.asBytes();
                std::memset(bytesView.begin(), 0, bytesView.size());
                std::memcpy(bytesView.begin(), data, size);
                capnp::FlatArrayMessageReader message(wordBuffer.asPtr());
                auto brain_reader = message.getRoot<NeuroForge::Schema::Brain>();
                std::cerr << "[capnp] root read ok" << std::endl;
                
                // Clear current state with fine-grained locking to avoid deadlocks
                {
                    std::lock_guard<std::mutex> rg_lock(region_mutex_);
                    regions_.clear();
                    region_names_.clear();
                }
                {
                    std::lock_guard<std::mutex> br_lock(brain_mutex_);
                    global_synapses_.clear();
                }
                std::cerr << "[capnp] cleared state" << std::endl;
                
                // Restore brain properties from available schema fields
                processing_mode_ = this->unmapProcessingMode(brain_reader.getProcessingMode());
                brain_state_.store(this->unmapBrainState(brain_reader.getBrainState()));
                target_frequency_.store(brain_reader.getTargetFrequency());
                processing_cycles_.store(brain_reader.getProcessingCycles());
                std::cerr << "[capnp] restored properties" << std::endl;
                
                // Build neuron lookup map for synapse reconstruction
                std::unordered_map<uint32_t, std::unordered_map<uint64_t, NeuronPtr>> regionNeuronMap;
                
                // Restore regions and neurons
                auto regions_list = brain_reader.getRegions();
                std::cerr << "[capnp] regions count=" << regions_list.size() << std::endl;
                for (const auto& region_reader : regions_list) {
                    uint32_t region_id = region_reader.getId();
                    std::string region_name = region_reader.getName().cStr();
                    Region::Type region_type = unmapRegionType(region_reader.getType());
                    Region::ActivationPattern activation_pattern = unmapActivationPattern(region_reader.getActivationPattern());
                    
                    auto region = std::make_shared<Region>(region_id, region_name, region_type, activation_pattern);
                    
                    // Restore neurons
                    auto neurons_list = region_reader.getNeurons();
                    std::cerr << "[capnp] region id=" << region_id << " name=" << region_name << " neurons=" << neurons_list.size() << std::endl;
                    auto& neuron_map = regionNeuronMap[region_id];
                    for (const auto& neuron_reader : neurons_list) {
                        uint64_t neuron_id = neuron_reader.getId();
                        Neuron::State neuron_state = unmapNeuronState(neuron_reader.getState());
                        float threshold = neuron_reader.getThreshold();
                        float activation = neuron_reader.getActivation();
                        float refractory_period = neuron_reader.getRefractoryPeriod();
                        float decay_rate = neuron_reader.getDecayRate();
                        float refractory_timer = neuron_reader.getRefractoryTimer();
                        
                        auto neuron = std::make_shared<Neuron>(neuron_id, threshold);
                        neuron->setRefractoryPeriod(refractory_period);
                        neuron->setDecayRate(decay_rate);
                        neuron->setRefractoryTimer(refractory_timer);
                        neuron->setState(neuron_state);
                        neuron->setActivation(activation);
                        
                        region->addNeuron(neuron);
                        neuron_map[neuron_id] = neuron;
                    }
                    
                    // Add region directly to avoid deadlock - we already cleared the maps above
                    {
                        std::lock_guard<std::mutex> lock(region_mutex_);
                        regions_[region_id] = region;
                        region_names_[region_name] = region_id;
                        
                        // If MemoryDB already set on brain, propagate to this new region
                        if (memory_db_) {
                            try {
                                region->setMemoryDB(memory_db_, memory_db_run_id_);
                                if (memdb_propagation_debug_) {
                                    if (memdb_colorize_ && is_stderr_tty()) {
                                    #ifdef _WIN32
                                        enable_vt_mode();
                                    #endif
                                        std::cerr << ansi_cyan() << "[MemoryDB Debug][" << region->getName() << ":id=" << region_id << "] "
                                                  << ansi_green() << "Propagated to new region, run_id=" << memory_db_run_id_ << ansi_reset() << "\n";
                                    } else {
                                        std::cerr << "[MemoryDB Debug][" << region->getName() << ":id=" << region_id << "] "
                                                  << "Propagated to new region, run_id=" << memory_db_run_id_ << "\n";
                                    }
                                }
                            } catch (...) {
                                // Ignore per-region exceptions during best-effort propagation
                            }
                        }
                    }
                    std::cerr << "[capnp] addRegion(" << region_name << ") => ok" << std::endl;

                    // Restore internal synapses for this region
                    auto internal_list = region_reader.getInternalSynapses();
                    std::size_t internal_count = 0, internal_fail = 0;
                    for (const auto& synapse_reader : internal_list) {
                        auto synapse_id = synapse_reader.getId();
                        auto source_neuron_id = synapse_reader.getSourceNeuronId();
                        auto target_neuron_id = synapse_reader.getTargetNeuronId();
                        auto weight = synapse_reader.getWeight();
                        auto type = unmapSynapseType(synapse_reader.getType());
                        auto plasticity_rule = unmapPlasticityRule(synapse_reader.getPlasticityRule());
                        auto learning_rate = synapse_reader.getLearningRate();
                        auto delay_ms = synapse_reader.getDelayMs();
                        auto min_weight = synapse_reader.getMinWeight();
                        auto max_weight = synapse_reader.getMaxWeight();

                        SynapsePtr s = connectNeurons(region_id, region_id,
                                                      source_neuron_id, target_neuron_id,
                                                      weight, type, synapse_id);
                        if (s) {
                            s->setPlasticityRule(plasticity_rule);
                            s->setLearningRate(learning_rate);
                            s->setDelay(delay_ms);
                            s->setWeightBounds(min_weight, max_weight);
                        } else {
                            ++internal_fail;
                        }
                        ++internal_count;
                    }
                    std::cerr << "[capnp] internal synapses processed=" << internal_count << " fail=" << internal_fail << std::endl;
                }
                
                // Rebuild neuronId -> regionId mapping
                std::unordered_map<uint64_t, uint32_t> neuronToRegion;
                for (const auto& pr : regionNeuronMap) {
                    uint32_t rid = pr.first;
                    for (const auto& nr : pr.second) {
                        neuronToRegion[nr.first] = rid;
                    }
                }
                std::cerr << "[capnp] neuronToRegion map size=" << neuronToRegion.size() << std::endl;

                // Restore all synapses via connectNeurons to rebuild full bookkeeping
                auto global_synapses_list = brain_reader.getGlobalSynapses();
                std::size_t global_count = 0, global_fail = 0;
                std::cerr << "[capnp] global synapses count=" << global_synapses_list.size() << std::endl;
                for (const auto& synapse_reader : global_synapses_list) {
                    auto synapse_id = synapse_reader.getId();
                    auto source_neuron_id = synapse_reader.getSourceNeuronId();
                    auto target_neuron_id = synapse_reader.getTargetNeuronId();
                    auto weight = synapse_reader.getWeight();
                    auto type = unmapSynapseType(synapse_reader.getType());
                    auto plasticity_rule = unmapPlasticityRule(synapse_reader.getPlasticityRule());
                    auto learning_rate = synapse_reader.getLearningRate();
                    auto delay_ms = synapse_reader.getDelayMs();
                    auto min_weight = synapse_reader.getMinWeight();
                    auto max_weight = synapse_reader.getMaxWeight();

                    uint32_t source_region_id = 0;
                    uint32_t target_region_id = 0;
                    auto itSrcReg = neuronToRegion.find(source_neuron_id);
                    if (itSrcReg != neuronToRegion.end()) source_region_id = itSrcReg->second;
                    auto itTgtReg = neuronToRegion.find(target_neuron_id);
                    if (itTgtReg != neuronToRegion.end()) target_region_id = itTgtReg->second;

                    SynapsePtr s = connectNeurons(source_region_id, target_region_id,
                                                  source_neuron_id, target_neuron_id,
                                                  weight, type, synapse_id);
                    if (s) {
                        s->setPlasticityRule(plasticity_rule);
                        s->setLearningRate(learning_rate);
                        s->setDelay(delay_ms);
                        s->setWeightBounds(min_weight, max_weight);
                    } else {
                        ++global_fail;
                    }
                    ++global_count;
                }
                std::cerr << "[capnp] global synapses processed=" << global_count << " fail=" << global_fail << std::endl;
                
                // Internal synapses are already handled via global synapse reconstruction above
                
                {
                    std::lock_guard<std::mutex> lock(statistics_mutex_);
                    stats_dirty_ = true;
                }
                std::cerr << "[capnp] import done" << std::endl;
                return true;
                
            } catch (const std::exception& e) {
                std::cerr << "[capnp] import exception: " << e.what() << std::endl;
                // Handle deserialization errors
                return false;
            }
        }

        bool HypergraphBrain::exportToBrainStateCapnp(std::vector<uint8_t>& outBuffer) const {
            try {
                std::lock_guard<std::mutex> lock(brain_mutex_);

                capnp::MallocMessageBuilder message;
                auto file_builder = message.initRoot<NeuroForge::BrainState::BrainStateFile>();

                // File metadata
                file_builder.setFileFormatVersion(kCheckpointFormatVersion);
                using namespace std::chrono;
                std::uint64_t ts_ms = static_cast<std::uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
                file_builder.setCreatedTimestamp(ts_ms);
                file_builder.setCreatedBy("NeuroForge");
                file_builder.setDescription("");

                // Build the embedded Brain using the same logic as exportToCapnp
                auto brain_builder = file_builder.initBrain();

                // Brain core properties
                brain_builder.setFormatVersion(1);
                brain_builder.setProcessingMode(mapProcessingMode(processing_mode_));
                brain_builder.setBrainState(mapBrainState(brain_state_.load()));
                brain_builder.setTargetFrequency(target_frequency_.load());
                brain_builder.setProcessingCycles(processing_cycles_.load());

                // Regions and internal synapses
                auto regions_list = brain_builder.initRegions(static_cast<capnp::uint>(regions_.size()));
                capnp::uint region_idx = 0;

                // For metadata computations
                double sumNeuronActivation = 0.0;
                std::size_t neuronCount = 0;
                double sumSynapseWeight = 0.0;
                std::size_t synapseCount = 0;

                for (const auto& region_pair : regions_) {
                    auto region_builder = regions_list[region_idx++];
                    const auto& region = region_pair.second;

                    region_builder.setId(region->getId());
                    region_builder.setName(region->getName());
                    region_builder.setType(mapRegionType(region->getType()));
                    region_builder.setActivationPattern(mapActivationPattern(region->getActivationPattern()));

                    // Neurons
                    const auto& neurons = region->getNeurons();
                    auto neurons_list = region_builder.initNeurons(static_cast<capnp::uint>(neurons.size()));
                    capnp::uint neuron_idx = 0;
                    for (const auto& neuron : neurons) {
                        auto neuron_builder = neurons_list[neuron_idx++];
                        neuron_builder.setId(neuron->getId());
                        neuron_builder.setState(mapNeuronState(neuron->getState()));
                        neuron_builder.setThreshold(neuron->getThreshold());
                        neuron_builder.setActivation(neuron->getActivation());
                        neuron_builder.setDecayRate(neuron->getDecayRate());
                        neuron_builder.setRefractoryTimer(neuron->getRefractoryTimer());
                        neuron_builder.setRefractoryPeriod(neuron->getRefractoryPeriod());

                        sumNeuronActivation += neuron->getActivation();
                        ++neuronCount;
                    }

                    // Internal synapses (also used for avg weight)
                    const auto& internal_synapses = region->getInternalSynapses();
                    auto internal_synapses_list = region_builder.initInternalSynapses(static_cast<capnp::uint>(internal_synapses.size()));
                    capnp::uint internal_syn_idx = 0;
                    for (const auto& synapse : internal_synapses) {
                        auto synapse_builder = internal_synapses_list[internal_syn_idx++];
                        serializeSynapse(synapse_builder, synapse);

                        sumSynapseWeight += synapse->getWeight();
                        ++synapseCount;
                    }
                }

                // Inter-region synapses (global) - also used for avg weight
                std::vector<SynapsePtr> inter_synapses;
                inter_synapses.reserve(global_synapses_.size());
                std::vector<const Synapse*> seen_ptrs;
                for (const auto& kv : regions_) {
                    const auto& region = kv.second;
                    const auto& inter_map = region->getInterRegionConnections();
                    for (const auto& it : inter_map) {
                        const auto& vec = it.second;
                        for (const auto& s : vec) {
                            const Synapse* raw = s.get();
                            if (!raw) continue;
                            bool present = false;
                            for (const auto* r : seen_ptrs) { if (r == raw) { present = true; break; } }
                            if (!present) { seen_ptrs.push_back(raw); inter_synapses.push_back(s); }
                        }
                    }
                }

                auto global_synapses_list = brain_builder.initGlobalSynapses(static_cast<capnp::uint>(inter_synapses.size()));
                capnp::uint global_syn_idx = 0;
                for (const auto& synapse : inter_synapses) {
                    auto synapse_builder = global_synapses_list[global_syn_idx++];
                    serializeSynapse(synapse_builder, synapse);

                    sumSynapseWeight += synapse->getWeight();
                    ++synapseCount;
                }

                // Fill persistence metadata
                auto stats = getGlobalStatistics();
                auto meta = file_builder.initMetadata();
                meta.setTotalNeurons(static_cast<std::uint64_t>(stats.total_neurons));
                meta.setTotalSynapses(static_cast<std::uint64_t>(stats.total_synapses));
                meta.setTotalRegions(static_cast<std::uint32_t>(stats.total_regions));
                float avgAct = (neuronCount > 0) ? static_cast<float>(sumNeuronActivation / neuronCount) : 0.0f;
                float avgW = (synapseCount > 0) ? static_cast<float>(sumSynapseWeight / synapseCount) : 0.0f;
                meta.setAvgNeuronActivation(avgAct);
                meta.setAvgSynapseWeight(avgW);
                meta.setProcessingFrequencyHz(stats.processing_frequency);
                meta.setLastUpdateDurationMs(0.0f);
                float memMB = static_cast<float>(getTotalMemoryUsage() / (1024.0 * 1024.0));
                meta.setMemoryUsageMB(memMB);

                std::uint64_t totalPlastic = 0;
                if (auto ls = getLearningStatistics()) {
                    totalPlastic = static_cast<std::uint64_t>(ls->total_updates);
                }
                meta.setTotalPlasticUpdates(totalPlastic);
                meta.setLearningEnabled(learning_enabled_);
                meta.setNeuroforgeVersion("unknown");
                meta.setSchemaVersion("1");
                meta.setDataChecksum("");

                // Serialize to bytes
                auto flatArray = capnp::messageToFlatArray(message);
                auto words = flatArray.asPtr();
                auto bytes = words.asBytes();
                outBuffer.assign(bytes.begin(), bytes.end());
                return true;

            } catch (...) {
                return false;
            }
        }

        bool HypergraphBrain::importFromBrainStateCapnp(const uint8_t* data, size_t size) {
            try {
                // Ensure word alignment by copying bytes into a word-aligned buffer
                const size_t wordCount = (size + sizeof(capnp::word) - 1) / sizeof(capnp::word);
                kj::Array<capnp::word> wordBuffer = kj::heapArray<capnp::word>(wordCount);
                auto bytesView = wordBuffer.asBytes();
                std::memset(bytesView.begin(), 0, bytesView.size());
                std::memcpy(bytesView.begin(), data, size);
                capnp::FlatArrayMessageReader message(wordBuffer.asPtr());

                auto file_reader = message.getRoot<NeuroForge::BrainState::BrainStateFile>();

                // Version compatibility guard
                try {
                    const auto fmtVer = file_reader.getFileFormatVersion();
                    if (fmtVer != kCheckpointFormatVersion) {
                        return false;
                    }
                } catch (...) {
                    return false; // unable to read version
                }

                // Optionally apply metadata flags
                try {
                    auto md = file_reader.getMetadata();
                    learning_enabled_ = md.getLearningEnabled();
                } catch (...) {
                    // ignore metadata issues
                }

                // Extract embedded brain and delegate to existing importer for reconstruction
                auto brain_reader = file_reader.getBrain();
                capnp::MallocMessageBuilder inner;
                inner.setRoot(brain_reader);

                auto flatArray = capnp::messageToFlatArray(inner);
                auto words = flatArray.asPtr();
                auto bytes = words.asBytes();
                return importFromCapnp(reinterpret_cast<const uint8_t*>(bytes.begin()), bytes.size());
            } catch (...) {
                return false;
            }
        }
#endif // NF_HAVE_CAPNP

        void HypergraphBrain::setMemoryDB(std::shared_ptr<NeuroForge::Core::MemoryDB> db, std::int64_t run_id) {
            auto prev_db = memory_db_;
            auto prev_run_id = memory_db_run_id_;

            memory_db_ = std::move(db);
            memory_db_run_id_ = run_id;

            // Initialize SelfModel snapshot cache once per run (no behavioral influence in Step 2)
            try {
                if (memory_db_ && memory_db_run_id_ > 0) {
                    self_model_ = std::make_unique<SelfModel>(*memory_db_);
                    self_model_->loadForRun(memory_db_run_id_);
                }
            } catch (...) {
                // SelfModel is optional and read-only; ignore initialization errors
            }

            if (memdb_propagation_debug_) {
                if (prev_db && memory_db_ && prev_db == memory_db_ && prev_run_id != memory_db_run_id_) {
                    if (memdb_colorize_ && is_stderr_tty()) {
                    #ifdef _WIN32
                        enable_vt_mode();
                    #endif
                        std::cerr << ansi_cyan() << "[MemoryDB Debug] " << ansi_yellow()
                                  << "run_id changed: " << prev_run_id << " -> " << memory_db_run_id_
                                  << " (MemoryDB unchanged)" << ansi_reset() << "\n";
                    } else {
                        std::cerr << "[MemoryDB Debug] run_id changed: " << prev_run_id << " -> " << memory_db_run_id_
                                  << " (MemoryDB unchanged)\n";
                    }
                }
            }

            // Propagate to regions via virtual hook (no-op by default)
            try {
                std::lock_guard<std::mutex> lock(region_mutex_);
                for (auto& kv : regions_) {
                    auto& r = kv.second;
                    if (r) {
                        r->setMemoryDB(memory_db_, memory_db_run_id_);
                        if (memdb_propagation_debug_) {
                            if (memdb_colorize_ && is_stderr_tty()) {
                            #ifdef _WIN32
                                enable_vt_mode();
                            #endif
                                std::cerr << ansi_cyan() << "[MemoryDB Debug][" << r->getName() << ":id=" << kv.first << "] "
                                          << ansi_green() << "Propagated to region, run_id=" << memory_db_run_id_ << ansi_reset() << "\n";
                            } else {
                                std::cerr << "[MemoryDB Debug][" << r->getName() << ":id=" << kv.first << "] "
                                          << "Propagated to region, run_id=" << memory_db_run_id_ << "\n";
                            }
                        }
                    }
                }
            } catch (...) {
                // Best-effort propagation; ignore exceptions from individual regions
            }
        }

        std::int64_t HypergraphBrain::startEpisode(const std::string& name) {
            using namespace std::chrono;
            std::int64_t eid = -1;
            if (memory_db_ && memory_db_run_id_ > 0) {
                try {
                    std::int64_t start_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                    if (memory_db_->insertEpisode(name, start_ms, memory_db_run_id_, eid)) {
                        current_episode_id_ = eid;
                    }
                } catch (...) {
                    // ignore
                }
            }
            // Also record in-memory episode list
            HypergraphBrain::EpisodeRecord ep;
            ep.name = name;
            ep.start_ms = static_cast<std::uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
            episodes_.push_back(std::move(ep));
            return eid;
        }

        bool HypergraphBrain::endEpisode(std::int64_t episode_id) {
            using namespace std::chrono;
            bool ok = true;
            if (memory_db_ && episode_id > 0) {
                try {
                    std::int64_t end_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                    ok = memory_db_->updateEpisodeEnd(episode_id, end_ms);
                } catch (...) {
                    ok = false;
                }
            }
            current_episode_id_ = -1;
            if (!episodes_.empty()) {
                episodes_.back().end_ms = static_cast<std::uint64_t>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
            }
            return ok;
        }

        void HypergraphBrain::deliverReward(double reward, const std::string& source, const std::string& context_json) {
            // Forward reward to the learning system if available
            auto* ls = learning_system_.get();
            bool skip_apply = false;
            if (ls && source == "phase_a") {
                // When Phase A internalization is enabled, ignore external reward application
                if (ls->isMimicryInternalEnabled()) {
                    skip_apply = true;
                }
            }
            if (ls && !skip_apply) {
                ls->applyExternalReward(static_cast<float>(reward));
            }
            // Log to MemoryDB (the logger swallows any DB errors internally)
            logReward(reward, source, context_json);
        }

        void HypergraphBrain::logReward(double reward, const std::string& source, const std::string& context_json) {
            using namespace std::chrono;
            if (!memory_db_ || memory_db_run_id_ <= 0) return;
            try {
                std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                long long offset = static_cast<long long>(reward_lag_align_offset_.load(std::memory_order_relaxed));
                long long step_signed = static_cast<long long>(processing_cycles_.load(std::memory_order_relaxed)) + offset;
                std::uint64_t step = step_signed < 0 ? 0ULL : static_cast<std::uint64_t>(step_signed);
                std::int64_t reward_id = -1;
                (void)memory_db_->insertRewardLog(ts_ms, step, reward, source, context_json, memory_db_run_id_, reward_id);
            } catch (...) {
                // Swallow logging errors to avoid impacting core processing
            }
        }

        void HypergraphBrain::logSelfModel(const std::string& state_json, double confidence) {
            using namespace std::chrono;
            if (!memory_db_ || memory_db_run_id_ <= 0) return;
            try {
                std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                std::uint64_t step = static_cast<std::uint64_t>(processing_cycles_.load(std::memory_order_relaxed));
                std::int64_t self_model_id = -1;
                (void)memory_db_->insertSelfModel(ts_ms, step, state_json, confidence, memory_db_run_id_, self_model_id);
            } catch (...) {
                // Swallow logging errors to avoid impacting core processing
            }
        }

        void HypergraphBrain::logSubstrateState(const std::string& state_type,
                                                const std::string& region_id,
                                                const std::string& serialized_data) {
            using namespace std::chrono;
            if (!memory_db_ || memory_db_run_id_ <= 0) return;
            try {
                std::int64_t ts_ms = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
                std::uint64_t step = static_cast<std::uint64_t>(processing_cycles_.load(std::memory_order_relaxed));
                std::int64_t state_id = -1;
                (void)memory_db_->insertSubstrateState(ts_ms, step, state_type, region_id, serialized_data, memory_db_run_id_, state_id);
            } catch (...) {
            }
        }

        HypergraphBrain::GlobalStatistics HypergraphBrain::getGlobalStatistics() const {
            std::lock_guard<std::mutex> lock(statistics_mutex_);
            if (stats_dirty_) {
                const_cast<HypergraphBrain*>(this)->recalculateGlobalStatistics();
            }

            GlobalStatistics stats = global_stats_;
            stats.processing_cycles = processing_cycles_.load();
            stats.processing_frequency = actual_frequency_.load();

            return stats;
        }

        void HypergraphBrain::updateGlobalStatistics(std::int64_t active_neuron_delta,
                                                       std::int64_t active_region_delta,
                                                       double energy_delta) {
            std::lock_guard<std::mutex> lock(statistics_mutex_);
            global_stats_.active_neurons += active_neuron_delta;
            global_stats_.active_regions += active_region_delta;
            global_stats_.total_energy += energy_delta;

            if (global_stats_.total_neurons > 0) {
                global_stats_.global_activation =
                    static_cast<float>(global_stats_.active_neurons) / global_stats_.total_neurons;
            } else {
                global_stats_.global_activation = 0.0f;
            }
        }

        void HypergraphBrain::recalculateGlobalStatistics() {
            statistics_mutex_.unlock();
            auto total_memory_usage = getTotalMemoryUsage();
            statistics_mutex_.lock();

            global_stats_.total_regions = regions_.size();
            global_stats_.total_neurons = 0;
            global_stats_.total_synapses = 0;
            global_stats_.active_regions = 0;
            global_stats_.active_neurons = 0;
            global_stats_.global_activation = 0.0f;
            global_stats_.total_energy = 0.0f;
            global_stats_.total_memory_usage = total_memory_usage;

            std::size_t cm_syn_count = connectivity_manager_ ? connectivity_manager_->getTotalSynapseCount() : 0;

            for (const auto& [region_id, region] : regions_) {
                if (region) {
                    auto region_stats = region->getStatistics();
                    global_stats_.total_neurons += region_stats.neuron_count;
                    global_stats_.active_neurons += region_stats.active_neurons;
                    global_stats_.total_energy += region_stats.total_energy;

                    if (cm_syn_count == 0) {
                        const auto& neurons = region->getNeurons();
                        for (const auto& neuron : neurons) {
                            if (neuron) {
                                global_stats_.total_synapses += neuron->getOutputSynapseCount();
                            }
                        }
                    }

                    if (region_stats.active_neurons > 0) {
                        global_stats_.active_regions++;
                    }
                }
            }

            if (cm_syn_count > 0) {
                global_stats_.total_synapses = cm_syn_count;
            }

            if (global_stats_.total_neurons > 0) {
                global_stats_.global_activation =
                    static_cast<float>(global_stats_.active_neurons) / global_stats_.total_neurons;
            }

            stats_dirty_ = false;
        }

        void HypergraphBrain::updateHardwareInfo() {
#ifdef _WIN32
            // Windows-specific hardware monitoring
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            GlobalMemoryStatusEx(&memInfo);

            hardware_info_.available_memory = memInfo.ullTotalPhys;
            hardware_info_.used_memory = memInfo.ullTotalPhys - memInfo.ullAvailPhys;
            hardware_info_.memory_usage = static_cast<float>(hardware_info_.used_memory) / hardware_info_.available_memory;

            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            hardware_info_.cpu_cores = sysInfo.dwNumberOfProcessors;
#else
            // Unix-like systems
            hardware_info_.cpu_cores = std::thread::hardware_concurrency();

            struct rusage usage;
            getrusage(RUSAGE_SELF, &usage);
            hardware_info_.used_memory = usage.ru_maxrss * 1024; // Convert KB to bytes
#endif
        }

        // M6: Hippocampal-like snapshotting implementation for fast plasticity memory path
        void HypergraphBrain::configureHippocampalSnapshotting(const HippocampalConfig& config) {
            std::lock_guard<std::mutex> lock(hippocampal_mutex_);
            hippocampal_config_ = config;
            
            // Resize snapshot buffer if needed
            if (hippocampal_snapshots_.size() > config.max_snapshots) {
                hippocampal_snapshots_.resize(config.max_snapshots);
            }
        }

        bool HypergraphBrain::takeHippocampalSnapshot(const std::string& context_tag, bool force_snapshot) {
            if (!hippocampal_enabled_.load()) {
                return false;
            }

            float current_activation = calculateGlobalActivation();
            
            if (!force_snapshot && !shouldTakeSnapshot(current_activation)) {
                return false;
            }

            std::lock_guard<std::mutex> lock(hippocampal_mutex_);
            
            // Create new snapshot
            HippocampalSnapshot snapshot;
            captureCurrentState(snapshot, context_tag);
            
            // Add to buffer (circular buffer behavior)
            if (hippocampal_snapshots_.size() >= hippocampal_config_.max_snapshots) {
                // Remove oldest snapshot
                hippocampal_snapshots_.erase(hippocampal_snapshots_.begin());
            }
            
            hippocampal_snapshots_.push_back(std::move(snapshot));
            
            // M6: Serialize snapshot to MemoryDB if available
            if (memory_db_ && memory_db_run_id_ >= 0) {
                try {
                    // Serialize snapshot data to JSON
                    std::ostringstream snapshot_json;
                    snapshot_json << "{";
                    snapshot_json << "\"context_tag\":\"" << context_tag << "\",";
                    snapshot_json << "\"timestamp_ms\":" << hippocampal_snapshots_.back().timestamp_ms << ",";
                    snapshot_json << "\"global_activation\":" << hippocampal_snapshots_.back().global_activation << ",";
                    snapshot_json << "\"priority\":" << hippocampal_snapshots_.back().priority << ",";
                    snapshot_json << "\"significant\":" << hippocampal_snapshots_.back().significant << ",";
                    snapshot_json << "\"synapse_weights\":[";
                    for (size_t i = 0; i < hippocampal_snapshots_.back().synapse_weights.size(); ++i) {
                        if (i > 0) snapshot_json << ",";
                        snapshot_json << hippocampal_snapshots_.back().synapse_weights[i];
                    }
                    snapshot_json << "],";
                    snapshot_json << "\"neuron_activations\":[";
                    for (size_t i = 0; i < hippocampal_snapshots_.back().neuron_activations.size(); ++i) {
                        if (i > 0) snapshot_json << ",";
                        snapshot_json << hippocampal_snapshots_.back().neuron_activations[i];
                    }
                    snapshot_json << "]}";
                    
                    std::int64_t snapshot_id = -1;
                    memory_db_->insertHippocampalSnapshot(
                        static_cast<std::int64_t>(hippocampal_snapshots_.back().timestamp_ms),
                        static_cast<std::uint64_t>(processing_cycles_),
                        static_cast<double>(hippocampal_snapshots_.back().priority),
                        static_cast<double>(hippocampal_snapshots_.back().significant ? 1.0 : 0.0),
                        snapshot_json.str(),
                        memory_db_run_id_,
                        snapshot_id
                    );
                } catch (const std::exception& e) {
                    // Log error but don't fail the snapshot operation
                    std::cerr << "[HypergraphBrain] Failed to serialize hippocampal snapshot: " << e.what() << std::endl;
                }
            }
            
            // Update tracking variables
            last_snapshot_time_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            last_global_activation_ = current_activation;
            
            return true;
        }

        std::size_t HypergraphBrain::consolidateHippocampalSnapshots(bool force_all) {
            if (!hippocampal_enabled_.load()) {
                return 0;
            }

            std::lock_guard<std::mutex> lock(hippocampal_mutex_);
            
            if (hippocampal_snapshots_.empty()) {
                return 0;
            }

            // Update priorities before consolidation
            updateSnapshotPriorities();
            
            // Select snapshots for consolidation
            auto indices_to_consolidate = selectSnapshotsForConsolidation(force_all);
            
            if (indices_to_consolidate.empty()) {
                return 0;
            }

            std::size_t consolidated_count = 0;
            
            // Consolidate selected snapshots to long-term memory
            for (auto it = indices_to_consolidate.rbegin(); it != indices_to_consolidate.rend(); ++it) {
                std::size_t idx = *it;
                if (idx < hippocampal_snapshots_.size()) {
                    const auto& snapshot = hippocampal_snapshots_[idx];
                    
                    // Apply snapshot weights to current synapses (consolidation)
                    for (const auto& [synapse_id, weight] : snapshot.synapse_weights) {
                        // Find synapse in global container
                        auto synapse_it = std::find_if(global_synapses_.begin(), global_synapses_.end(),
                            [synapse_id](const NeuroForge::SynapsePtr& synapse) {
                                return synapse && synapse->getId() == synapse_id;
                            });
                        if (synapse_it != global_synapses_.end()) {
                            auto synapse = *synapse_it;
                            if (synapse) {
                                // Blend snapshot weight with current weight
                                float current_weight = synapse->getWeight();
                                float blended_weight = current_weight * (1.0f - hippocampal_config_.consolidation_threshold) +
                                                     weight * hippocampal_config_.consolidation_threshold;
                                synapse->setWeight(blended_weight);
                            }
                        }
                    }
                    
                    // Remove consolidated snapshot
                    hippocampal_snapshots_.erase(hippocampal_snapshots_.begin() + idx);
                    consolidated_count++;
                }
            }
            
            return consolidated_count;
        }

        HypergraphBrain::HippocampalStats HypergraphBrain::getHippocampalStats() const {
            std::lock_guard<std::mutex> lock(hippocampal_mutex_);
            
            HippocampalStats stats;
            stats.total_snapshots = hippocampal_snapshots_.size();
            stats.last_snapshot_time_ms = last_snapshot_time_ms_;
            
            float total_priority = 0.0f;
            for (const auto& snapshot : hippocampal_snapshots_) {
                total_priority += snapshot.priority;
                if (snapshot.significant) {
                    stats.significant_snapshots++;
                }
            }
            
            if (!hippocampal_snapshots_.empty()) {
                stats.average_priority = total_priority / hippocampal_snapshots_.size();
            }
            
            // Calculate memory usage
            for (const auto& snapshot : hippocampal_snapshots_) {
                stats.memory_usage_bytes += sizeof(HippocampalSnapshot);
                stats.memory_usage_bytes += snapshot.synapse_weights.size() * (sizeof(SynapseID) + sizeof(Weight));
                stats.memory_usage_bytes += snapshot.neuron_activations.size() * (sizeof(NeuronID) + sizeof(float));
                stats.memory_usage_bytes += snapshot.context_tag.size();
            }
            
            return stats;
        }

        void HypergraphBrain::setHippocampalEnabled(bool enabled) {
            hippocampal_enabled_.store(enabled);
        }

        bool HypergraphBrain::isHippocampalEnabled() const {
            return hippocampal_enabled_.load();
        }

        // M6: Private helper methods for hippocampal snapshotting
        float HypergraphBrain::calculateGlobalActivation() const {
            std::lock_guard<std::mutex> lock(region_mutex_);
            
            float total_activation = 0.0f;
            std::size_t total_neurons = 0;
            
            for (const auto& [region_id, region] : regions_) {
                if (region) {
                    auto region_stats = region->getStatistics();
                    total_activation += region_stats.average_activation * region_stats.neuron_count;
                    total_neurons += region_stats.neuron_count;
                }
            }
            
            return total_neurons > 0 ? total_activation / total_neurons : 0.0f;
        }

        bool HypergraphBrain::shouldTakeSnapshot(float current_activation) const {
            // Check time threshold
            auto current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            
            if (current_time_ms - last_snapshot_time_ms_ < hippocampal_config_.snapshot_interval_ms) {
                return false;
            }
            
            // Check activation threshold
            float activation_change = std::abs(current_activation - last_global_activation_);
            if (activation_change < hippocampal_config_.snapshot_threshold) {
                return false;
            }
            
            // Check if buffer is full and current activation is significant enough
            if (hippocampal_snapshots_.size() >= hippocampal_config_.max_snapshots) {
                // Only take snapshot if current activation is higher than lowest priority snapshot
                auto min_priority_it = std::min_element(hippocampal_snapshots_.begin(), hippocampal_snapshots_.end(),
                    [](const HippocampalSnapshot& a, const HippocampalSnapshot& b) {
                        return a.priority < b.priority;
                    });
                
                if (min_priority_it != hippocampal_snapshots_.end() && 
                    current_activation <= min_priority_it->priority) {
                    return false;
                }
            }
            
            return true;
        }

        void HypergraphBrain::captureCurrentState(HippocampalSnapshot& snapshot, const std::string& context_tag) const {
            snapshot.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            snapshot.context_tag = context_tag;
            snapshot.global_activation = calculateGlobalActivation();
            snapshot.priority = snapshot.global_activation; // Initial priority based on activation
            snapshot.significant = snapshot.global_activation > hippocampal_config_.consolidation_threshold;
            
            // Capture synapse weights
            std::lock_guard<std::mutex> lock(brain_mutex_);
            for (const auto& synapse : global_synapses_) {
                if (synapse) {
                    snapshot.synapse_weights[synapse->getId()] = synapse->getWeight();
                }
            }
            
            // Capture neuron activations
            std::lock_guard<std::mutex> region_lock(region_mutex_);
            for (const auto& [region_id, region] : regions_) {
                if (region) {
                    auto neurons = region->getNeurons();
                    for (const auto& neuron : neurons) {
                        if (neuron) {
                            snapshot.neuron_activations[neuron->getId()] = neuron->getActivation();
                        }
                    }
                }
            }
        }

        void HypergraphBrain::updateSnapshotPriorities() {
            auto current_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            
            for (auto& snapshot : hippocampal_snapshots_) {
                // Decay priority over time
                float age_factor = 1.0f - (current_time_ms - snapshot.timestamp_ms) / 
                                  (hippocampal_config_.max_age_ms * 1000.0f);
                age_factor = std::max(0.0f, age_factor);
                
                // Update priority with decay and significance boost
                snapshot.priority = snapshot.global_activation * age_factor;
                if (snapshot.significant) {
                    snapshot.priority *= hippocampal_config_.significance_boost;
                }
            }
        }

        std::vector<std::size_t> HypergraphBrain::selectSnapshotsForConsolidation(bool force_all) const {
            std::vector<std::size_t> indices;
            
            if (force_all) {
                for (std::size_t i = 0; i < hippocampal_snapshots_.size(); ++i) {
                    indices.push_back(i);
                }
                return indices;
            }
            
            // Select snapshots based on priority threshold
            for (std::size_t i = 0; i < hippocampal_snapshots_.size(); ++i) {
                const auto& snapshot = hippocampal_snapshots_[i];
                if (snapshot.priority >= hippocampal_config_.consolidation_threshold ||
                    snapshot.significant) {
                    indices.push_back(i);
                }
            }
            
            // Limit number of consolidations per call
            if (indices.size() > hippocampal_config_.max_consolidations_per_call) {
                // Sort by priority and take top N
                std::sort(indices.begin(), indices.end(), 
                    [this](std::size_t a, std::size_t b) {
                        return hippocampal_snapshots_[a].priority > hippocampal_snapshots_[b].priority;
                    });
                indices.resize(hippocampal_config_.max_consolidations_per_call);
            }
            
            return indices;
        }

        // Autonomous task scheduling system implementations
        bool HypergraphBrain::initializeAutonomousScheduler(const AutonomousScheduler::Config& config) {
            // Note: scheduler_mutex_ should already be held by caller
            
            try {
                autonomous_scheduler_ = std::make_unique<AutonomousScheduler>(this);
                if (!autonomous_scheduler_->initialize(config)) {
                    return false;
                }
                autonomous_scheduler_->start();
                return true;
            } catch (const std::exception& e) {
                // Log error if logging system is available
                return false;
            }
        }

        void HypergraphBrain::setAutonomousModeEnabled(bool enabled) {
            autonomous_mode_enabled_.store(enabled, std::memory_order_relaxed);
        }

        bool HypergraphBrain::addAutonomousTask(std::shared_ptr<AutonomousTask> task) {
            // Conditionally acquire scheduler_mutex_ to prevent deadlock in autonomous loop
            std::unique_ptr<std::lock_guard<std::mutex>> lock;
            if (std::this_thread::get_id() != autonomous_thread_id_) {
                lock = std::make_unique<std::lock_guard<std::mutex>>(scheduler_mutex_);
            }
            
            if (!autonomous_scheduler_) {
                return false;
            }
            
            return autonomous_scheduler_->scheduleTask(std::move(task)) != 0;
        }

        bool HypergraphBrain::executeAutonomousCycle(float delta_time) {
            if (!autonomous_mode_enabled_.load(std::memory_order_relaxed)) {
                return false;
            }
            
            // Check if we're being called from the autonomous loop thread
            // In that case, don't lock the mutex to avoid deadlock
            std::thread::id current_thread = std::this_thread::get_id();
            bool is_autonomous_thread = (current_thread == autonomous_thread_id_);
            
            std::unique_ptr<std::lock_guard<std::mutex>> lock;
            if (!is_autonomous_thread) {
                lock = std::make_unique<std::lock_guard<std::mutex>>(scheduler_mutex_);
            }
            
            if (!autonomous_scheduler_) {
                return false;
            }
            
            // Process scheduling and execution
            TaskContext context;
            context.delta_time = delta_time;
            context.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            if (substrate_task_generator_ && substrate_task_generator_->isActive()) {
                substrate_task_generator_->generateTasks(delta_time);
            }
            
            autonomous_scheduler_->processScheduling(context);
            autonomous_scheduler_->processExecution(context);
            
            return autonomous_scheduler_->hasWork();
        }

        std::optional<AutonomousScheduler::Statistics> HypergraphBrain::getAutonomousStatistics() const {
            std::lock_guard<std::mutex> lock(scheduler_mutex_);
            
            if (!autonomous_scheduler_) {
                return std::nullopt;
            }
            
            return autonomous_scheduler_->getStatistics();
        }

        void HypergraphBrain::setSelfNodeIntegrationEnabled(bool enabled) {
            selfnode_integration_enabled_.store(enabled, std::memory_order_relaxed);
        }

        void HypergraphBrain::setPrefrontalCortexIntegrationEnabled(bool enabled) {
            pfc_integration_enabled_.store(enabled, std::memory_order_relaxed);
        }

        void HypergraphBrain::setMotorCortexIntegrationEnabled(bool enabled) {
            motor_cortex_integration_enabled_.store(enabled, std::memory_order_relaxed);
        }

        void HypergraphBrain::runAutonomousLoop(std::size_t max_iterations, float target_frequency) {
            std::cout << "[DEBUG] Entering runAutonomousLoop with max_iterations=" << max_iterations << std::endl;
            // Store the autonomous thread ID for deadlock prevention
            autonomous_thread_id_ = std::this_thread::get_id();
            
            if (!autonomous_scheduler_) {
                std::cout << "[DEBUG] Initializing AutonomousScheduler..." << std::endl;
                // Initialize with default config if not already initialized
                if (!initializeAutonomousScheduler()) {
                    std::cerr << "[ERROR] Failed to initialize AutonomousScheduler" << std::endl;
                    return;
                }
            }
            
            setAutonomousModeEnabled(true);
            std::cout << "[DEBUG] Autonomous mode enabled. Starting loop..." << std::endl;
            
            const auto target_period = std::chrono::duration<float>(1.0f / target_frequency);
            std::size_t iteration = 0;
            const std::size_t introspection_interval = static_cast<std::size_t>(target_frequency * 10); // Every 10 seconds
            
            while (autonomous_mode_enabled_.load(std::memory_order_relaxed) && 
                   (max_iterations == 0 || iteration < max_iterations)) {
                
                // Debug print every 100 iterations
                 if (iteration % 100 == 0) {
                      std::cout << "[DEBUG] Autonomous iteration " << iteration << std::endl;
                 }
                
                auto cycle_start = std::chrono::steady_clock::now();
                float delta_time = 1.0f / target_frequency;
                
                if (selfnode_integration_enabled_.load(std::memory_order_relaxed)) {
                    auto self_node = getRegion("SelfNode");
                    if (self_node) {
                        auto self_node_limbic = std::dynamic_pointer_cast<NeuroForge::Regions::SelfNode>(self_node);
                        if (self_node_limbic) {
                            self_node_limbic->initiateReflection("autonomous_goal_selection",
                                {NeuroForge::Regions::SelfNode::SelfAspect::Cognitive, NeuroForge::Regions::SelfNode::SelfAspect::Temporal});
                        }
                    }
                }
                
                if (pfc_integration_enabled_.load(std::memory_order_relaxed)) {
                    auto prefrontal_cortex = getRegion("PrefrontalCortex");
                    if (prefrontal_cortex) {
                        auto pfc = std::dynamic_pointer_cast<NeuroForge::Regions::PrefrontalCortex>(prefrontal_cortex);
                        if (pfc) {
                            std::vector<float> options = {0.2f, 0.5f, 0.8f, 0.3f};
                            std::vector<float> values = {0.6f, 0.9f, 0.4f, 0.7f};
                            auto decision = pfc->makeDecision(options, values);
                            std::vector<float> planning_context = {
                                static_cast<float>(iteration % 100) / 100.0f,
                                calculateGlobalActivation(),
                                static_cast<float>(decision.selected_option),
                                decision.confidence
                            };
                            pfc->storeInWorkingMemory(planning_context);
                        }
                    }
                }
                
                if (motor_cortex_integration_enabled_.load(std::memory_order_relaxed)) {
                    auto motor_cortex = getRegion("MotorCortex");
                    if (motor_cortex) {
                        auto mc = std::dynamic_pointer_cast<NeuroForge::Regions::MotorCortex>(motor_cortex);
                        if (mc) {
                            std::vector<float> movement_vector = {0.1f, 0.0f, 0.2f};
                            mc->planMovement(NeuroForge::Regions::MotorCortex::BodyPart::Arms, movement_vector, 0.5f);
                            mc->executeMotorCommands();
                        }
                    }
                }
                
                // 4. Execute autonomous scheduler cycle
                executeAutonomousCycle(delta_time);
                
                // 5. Execute regular brain processing if enabled
                if (brain_state_.load(std::memory_order_relaxed) == BrainState::Running) {
                    processStep(delta_time);
                }
                
                if (selfnode_integration_enabled_.load(std::memory_order_relaxed)) {
                    if (iteration % introspection_interval == 0) {
                        auto self_node = getRegion("SelfNode");
                        if (self_node) {
                            auto self_node_limbic = std::dynamic_pointer_cast<NeuroForge::Regions::SelfNode>(self_node);
                            if (self_node_limbic) {
                                self_node_limbic->initiateReflection("periodic_introspection", {
                                    NeuroForge::Regions::SelfNode::SelfAspect::Cognitive,
                                    NeuroForge::Regions::SelfNode::SelfAspect::Emotional,
                                    NeuroForge::Regions::SelfNode::SelfAspect::Temporal,
                                    NeuroForge::Regions::SelfNode::SelfAspect::Narrative
                                });
                                auto reflection_task = std::make_shared<ReflectionTask>(
                                    iteration + 1000000,
                                    "periodic_self_reflection",
                                    "comprehensive"
                                );
                                reflection_task->setPriority(TaskPriority::Medium);
                                addAutonomousTask(reflection_task);
                            }
                        }
                    }
                }
                
                // 7. Adaptive goal generation based on performance
                /*
                if (iteration % (introspection_interval / 2) == 0) {
                    // Create new goal task based on current state
                    auto goal_task = std::make_shared<GoalTask>(
                        iteration + 2000000, // Unique ID
                        "adaptive_exploration_goal",
                        "exploration"
                    );
                    goal_task->setPriority(TaskPriority::High);
                    goal_task->setGoalParameters({
                        calculateGlobalActivation(),
                        static_cast<float>(iteration) / static_cast<float>(max_iterations > 0 ? max_iterations : 1000),
                        0.5f // Exploration factor
                    });
                    goal_task->setSuccessThreshold(0.7f);
                    addAutonomousTask(goal_task);
                }
                */
                
                // Sleep to maintain target frequency
                auto cycle_end = std::chrono::steady_clock::now();
                auto cycle_duration = cycle_end - cycle_start;
                
                if (cycle_duration < target_period) {
                    std::this_thread::sleep_for(target_period - cycle_duration);
                }
                
                ++iteration;
            }
        }

        // M7: Substrate mode and autonomous operation method implementations
        void HypergraphBrain::setSubstrateMode(SubstrateMode mode) {
            substrate_mode_.store(mode, std::memory_order_relaxed);
            
            // Configure substrate behavior based on mode
            switch (mode) {
                case SubstrateMode::Off:
                    // Disable substrate processing
                    setSubstrateTaskGenerationEnabled(false);
                    break;
                    
                case SubstrateMode::Mirror:
                    // Enable mirroring of external inputs
                    setSubstrateTaskGenerationEnabled(false);
                    setAutonomousModeEnabled(false);
                    if (learning_system_) {
                        learning_system_->setMimicryEnabled(true);
                        learning_system_->setSubstrateTrainingMode(false);
                    }
                    break;
                    
                case SubstrateMode::Train:
                    // Enable training mode with substrate learning
                    setSubstrateTaskGenerationEnabled(true);
                    if (learning_system_) {
                        learning_system_->setSubstrateTrainingMode(true);
                    }
                    break;
                    
                case SubstrateMode::Native:
                    // Enable full native substrate operation
                    setSubstrateTaskGenerationEnabled(true);
                    setAutonomousModeEnabled(true);
                    if (learning_system_) {
                        learning_system_->setSubstrateTrainingMode(false);
                    }
                    break;
            }
        }

        void HypergraphBrain::setCuriosityThreshold(float threshold) {
            if (threshold >= 0.0f && threshold <= 1.0f) {
                curiosity_threshold_.store(threshold, std::memory_order_relaxed);
                
                // Update substrate task generator configuration
                if (substrate_task_generator_) {
                    auto config = substrate_task_generator_->getConfig();
                    config.curiosity_threshold = threshold;
                    substrate_task_generator_->setConfig(config);
                }
            }
        }

        void HypergraphBrain::setUncertaintyThreshold(float threshold) {
            if (threshold >= 0.0f && threshold <= 1.0f) {
                uncertainty_threshold_.store(threshold, std::memory_order_relaxed);
                
                // Update substrate task generator configuration
                if (substrate_task_generator_) {
                    auto config = substrate_task_generator_->getConfig();
                    config.uncertainty_threshold = threshold;
                    substrate_task_generator_->setConfig(config);
                }
            }
        }

        void HypergraphBrain::setPredictionErrorThreshold(float threshold) {
            if (threshold >= 0.0f && threshold <= 1.0f) {
                prediction_error_threshold_.store(threshold, std::memory_order_relaxed);
                
                // Update substrate task generator configuration
                if (substrate_task_generator_) {
                    auto config = substrate_task_generator_->getConfig();
                    config.prediction_error_threshold = threshold;
                    substrate_task_generator_->setConfig(config);
                }
            }
        }

        void HypergraphBrain::setMaxConcurrentTasks(int max_tasks) {
            if (max_tasks > 0) {
                max_concurrent_tasks_.store(max_tasks, std::memory_order_relaxed);
                
                // Update substrate task generator configuration
                if (substrate_task_generator_) {
                    auto config = substrate_task_generator_->getConfig();
                    config.max_concurrent_tasks = static_cast<std::uint32_t>(max_tasks);
                    substrate_task_generator_->setConfig(config);
                }
            }
        }

        void HypergraphBrain::setTaskGenerationInterval(int interval_ms) {
            if (interval_ms >= 0) {
                task_generation_interval_.store(interval_ms, std::memory_order_relaxed);
                
                // Update substrate task generator configuration
                if (substrate_task_generator_) {
                    auto config = substrate_task_generator_->getConfig();
                    config.task_generation_interval_ms = static_cast<std::uint64_t>(interval_ms);
                    substrate_task_generator_->setConfig(config);
                }
            }
        }

        void HypergraphBrain::setEliminateScaffolds(bool enabled) {
            eliminate_scaffolds_.store(enabled, std::memory_order_relaxed);
            
            // Configure scaffold elimination in learning system
            if (learning_system_) {
                learning_system_->setScaffoldElimination(enabled);
            }
        }

        void HypergraphBrain::setAutonomyMetrics(bool enabled) {
            autonomy_metrics_enabled_.store(enabled, std::memory_order_relaxed);
            
            // Configure autonomy metrics collection
            if (autonomous_scheduler_) {
                autonomous_scheduler_->setMetricsEnabled(enabled);
            }
        }

        void HypergraphBrain::setAutonomyTarget(float target) {
            if (target >= 0.0f && target <= 1.0f) {
                autonomy_target_.store(target, std::memory_order_relaxed);
            }
        }

        void HypergraphBrain::setMotivationDecay(float decay) {
            if (decay >= 0.0f && decay <= 1.0f) {
                motivation_decay_.store(decay, std::memory_order_relaxed);
                
                // Update intrinsic motivation system
                if (learning_system_) {
                    learning_system_->setMotivationDecay(decay);
                }
            }
        }

        void HypergraphBrain::setExplorationBonus(float bonus) {
            if (bonus >= 0.0f) {
                exploration_bonus_.store(bonus, std::memory_order_relaxed);
                
                // Update exploration parameters in learning system
                if (learning_system_) {
                    learning_system_->setExplorationBonus(bonus);
                }
            }
        }

        void HypergraphBrain::setNoveltyMemorySize(int size) {
            if (size > 0) {
                novelty_memory_size_.store(size, std::memory_order_relaxed);
                
                // Update novelty detection system
                if (learning_system_) {
                    learning_system_->setNoveltyMemorySize(static_cast<std::size_t>(size));
                }
            }
        }

        // M7: Substrate task generation methods
        void HypergraphBrain::setSubstrateTaskGenerationEnabled(bool enabled) {
            // Guarded initialization with safe fallback to no-op when dependencies are missing
            // This avoids introducing hard dependencies (e.g., Cap'n Proto) and keeps
            // autonomous scheduling available even if substrate generation cannot be initialized.
            if (enabled) {
                // Initialize substrate task generator if not already present
                if (!substrate_task_generator_) {
                    SubstrateTaskGenerator::Config cfg;
                    // Bind thresholds and limits from brain-level autonomy knobs
                    cfg.curiosity_threshold           = curiosity_threshold_.load(std::memory_order_relaxed);
                    cfg.uncertainty_threshold         = uncertainty_threshold_.load(std::memory_order_relaxed);
                    cfg.prediction_error_threshold    = prediction_error_threshold_.load(std::memory_order_relaxed);
                    cfg.max_concurrent_tasks          = static_cast<std::uint32_t>(max_concurrent_tasks_.load(std::memory_order_relaxed));
                    cfg.task_generation_interval_ms   = static_cast<std::uint64_t>(task_generation_interval_.load(std::memory_order_relaxed));

                    if (!initializeSubstrateTaskGeneration(cfg)) {
                        // Fallback: keep autonomous scheduler available, but skip substrate tasks
                        substrate_task_generator_.reset();
                        NF_LOG_WARN("HypergraphBrain", "substrate task generator failed to initialize — continuing without substrate tasks");
                        return;
                    }
                }

                // Activate generator if successfully initialized
                if (substrate_task_generator_) {
                    substrate_task_generator_->setActive(true);
                    NF_LOG_INFO("HypergraphBrain", "substrate task generation enabled");
                }
            } else {
                // Deactivate generator if present
                if (substrate_task_generator_) {
                    substrate_task_generator_->setActive(false);
                    NF_LOG_INFO("HypergraphBrain", "substrate task generation disabled");
                }
            }
        }

        bool HypergraphBrain::isSubstrateTaskGenerationEnabled() const {
            // Reflect actual generator activation state when available
            return substrate_task_generator_ && substrate_task_generator_->isActive();
        }

        bool HypergraphBrain::initializeSubstrateTaskGeneration(const SubstrateTaskGenerator::Config& config) {
            // Ensure thread-safe interaction with the autonomous scheduler and generator
            std::lock_guard<std::mutex> lock(scheduler_mutex_);

            // If already initialized, update configuration and succeed
            if (substrate_task_generator_) {
                substrate_task_generator_->setConfig(config);
                NF_LOG_DEBUG("HypergraphBrain", "substrate task generator configuration updated");
                return true;
            }

            // Ensure autonomous scheduler exists before binding the generator
            if (!autonomous_scheduler_) {
                if (!initializeAutonomousScheduler()) {
                    NF_LOG_WARN("HypergraphBrain", "autonomous scheduler unavailable — cannot initialize substrate task generator");
                    return false;
                }
            }

            // Create a non-owning shared_ptr wrapper around this brain and the scheduler
            // so the generator can reference them without affecting lifetime management.
            auto brain_ref = std::shared_ptr<HypergraphBrain>(this, [](HypergraphBrain*){});
            auto sched_ref = std::shared_ptr<AutonomousScheduler>(autonomous_scheduler_.get(), [](AutonomousScheduler*){});

            // Construct and initialize the substrate task generator
            substrate_task_generator_ = std::make_unique<SubstrateTaskGenerator>(brain_ref, sched_ref, config);
            if (!substrate_task_generator_->initialize()) {
                NF_LOG_WARN("HypergraphBrain", "failed to initialize substrate task generator");
                substrate_task_generator_.reset();
                return false;
            }

            NF_LOG_INFO("HypergraphBrain", "substrate task generator initialized");
            return true;
        }

    } // namespace Core
} // namespace NeuroForge
