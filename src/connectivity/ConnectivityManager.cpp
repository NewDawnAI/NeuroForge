#include "connectivity/ConnectivityManager.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <chrono>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cctype>

namespace NeuroForge {
    namespace Connectivity {

        ConnectivityManager::ConnectivityManager()
            : connection_id_counter_(0)
            , total_synapses_(0)
            , is_initialized_(false)
        {
            // Initialize random number generator with current time
            auto seed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
            rng_.seed(static_cast<std::uint32_t>(seed));
            
            // Initialize default patterns
            initializeDefaultPatterns();
            
            is_initialized_ = true;
        }

        // ===== Region Registration =====

        void ConnectivityManager::registerRegion(RegionPtr region) {
            if (!region) {
                std::cerr << "ConnectivityManager: Cannot register null region" << std::endl;
                return;
            }
            
            const std::string region_id = std::to_string(region->getId());
            
            if (regions_.find(region_id) != regions_.end()) {
                std::cerr << "ConnectivityManager: Region '" << region_id << "' already registered" << std::endl;
                return;
            }
            
            regions_[region_id] = region;
            std::cout << "ConnectivityManager: Registered region '" << region_id << "'" << std::endl;
        }

        void ConnectivityManager::unregisterRegion(const std::string& region_id) {
            auto it = regions_.find(region_id);
            if (it == regions_.end()) {
                std::cerr << "ConnectivityManager: Region '" << region_id << "' not found" << std::endl;
                return;
            }
            
            // Remove all connections involving this region
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                connections_.erase(
                    std::remove_if(connections_.begin(), connections_.end(),
                        [&region_id](const RegionConnection& conn) {
                            return conn.source_region_id == region_id || conn.target_region_id == region_id;
                        }),
                    connections_.end()
                );
            }
            
            regions_.erase(it);
            std::cout << "ConnectivityManager: Unregistered region '" << region_id << "'" << std::endl;
        }

        RegionPtr ConnectivityManager::getRegion(const std::string& region_id) const {
            auto it = regions_.find(region_id);
            return (it != regions_.end()) ? it->second : nullptr;
        }

        std::vector<RegionPtr> ConnectivityManager::getAllRegions() const {
            std::vector<RegionPtr> all_regions;
            all_regions.reserve(regions_.size());
            
            for (const auto& [id, region] : regions_) {
                all_regions.push_back(region);
            }
            
            return all_regions;
        }

        // ===== Connectivity Establishment =====

        std::size_t ConnectivityManager::connectRegions(const std::string& source_id, 
                                                      const std::string& target_id,
                                                      const ConnectionParameters& params) {
            std::cout << "ConnectivityManager: Starting connectRegions '" << source_id 
                      << "' -> '" << target_id << "' with type " << static_cast<int>(params.type) << std::endl;
            
            try {
                // Validate parameters
                std::cout << "ConnectivityManager: Validating connection parameters..." << std::endl;
                if (!validateConnectionParameters(params)) {
                    std::cerr << "ConnectivityManager: ERROR - Invalid connection parameters for '" 
                              << source_id << "' -> '" << target_id << "'" << std::endl;
                    std::cerr << "  - Probability: " << params.connection_probability << std::endl;
                    std::cerr << "  - Weight mean: " << params.weight_mean << std::endl;
                    std::cerr << "  - Weight std: " << params.weight_std << std::endl;
                    return 0;
                }
                std::cout << "ConnectivityManager: Parameters validated successfully" << std::endl;
                
                // Get source and target regions
                std::cout << "ConnectivityManager: Getting source region '" << source_id << "'..." << std::endl;
                RegionPtr source_region = getRegion(source_id);
                if (!source_region) {
                    std::cerr << "ConnectivityManager: ERROR - Source region '" << source_id << "' not found" << std::endl;
                    return 0;
                }
                std::cout << "ConnectivityManager: Source region found" << std::endl;
                
                std::cout << "ConnectivityManager: Getting target region '" << target_id << "'..." << std::endl;
                RegionPtr target_region = getRegion(target_id);
                if (!target_region) {
                    std::cerr << "ConnectivityManager: ERROR - Target region '" << target_id << "' not found" << std::endl;
                    return 0;
                }
                std::cout << "ConnectivityManager: Target region found" << std::endl;
                
                // Get neurons from both regions
                std::cout << "ConnectivityManager: Getting neurons from regions..." << std::endl;
                const auto& source_neurons = source_region->getNeurons();
                const auto& target_neurons = target_region->getNeurons();
                
                std::cout << "ConnectivityManager: Source neurons: " << source_neurons.size() 
                          << ", Target neurons: " << target_neurons.size() << std::endl;
                
                if (source_neurons.empty() || target_neurons.empty()) {
                    std::cerr << "ConnectivityManager: ERROR - Source or target region has no neurons" << std::endl;
                    std::cerr << "  - Source '" << source_id << "': " << source_neurons.size() << " neurons" << std::endl;
                    std::cerr << "  - Target '" << target_id << "': " << target_neurons.size() << " neurons" << std::endl;
                    return 0;
                }
                
                std::cout << "ConnectivityManager: Starting synapse creation..." << std::endl;
            
            std::size_t synapses_created = 0;
            std::vector<SynapsePtr> new_synapses;
            
            // Calculate estimated total connections for progress tracking
            std::size_t estimated_total_connections = 0;
            switch (params.type) {
                case ConnectivityType::Sparse:
                case ConnectivityType::Dense:
                    estimated_total_connections = static_cast<std::size_t>(
                        source_neurons.size() * target_neurons.size() * params.connection_probability);
                    break;
                case ConnectivityType::Feedforward:
                case ConnectivityType::Feedback:
                    estimated_total_connections = static_cast<std::size_t>(
                        (source_neurons.size() / 4) * 5 * 3 * params.connection_probability);
                    break;
                case ConnectivityType::Lateral:
                    estimated_total_connections = static_cast<std::size_t>(
                        std::min(source_neurons.size(), target_neurons.size()) * 4 * params.connection_probability);
                    break;
                case ConnectivityType::Global:
                    estimated_total_connections = static_cast<std::size_t>(
                        100 * 100 * params.connection_probability); // Sample size is 100
                    break;
                case ConnectivityType::Reciprocal:
                    estimated_total_connections = static_cast<std::size_t>(
                        std::min(source_neurons.size(), target_neurons.size()) * 2 * params.connection_probability);
                    break;
                case ConnectivityType::Modular:
                    estimated_total_connections = static_cast<std::size_t>(
                        source_neurons.size() * target_neurons.size() * params.connection_probability * 0.5f);
                    break;
                default:
                    estimated_total_connections = 1000; // Default estimate
            }
            
            std::size_t progress_interval = std::max(estimated_total_connections / 20, std::size_t(1000));
            std::size_t next_progress_report = progress_interval;
            
            std::cout << "ConnectivityManager: Estimated " << estimated_total_connections 
                      << " connections to create" << std::endl;
            
            // Pre-reserve capacity for new synapses to minimize reallocations during batching
            if (estimated_total_connections > 0) {
                new_synapses.reserve(estimated_total_connections);
            }

            // Pre-reserve inter-region bookkeeping vectors to minimize reallocations
            if (estimated_total_connections > 0 && source_id != target_id) {
                source_region->reserveInterRegionConnections(target_region->getId(), estimated_total_connections);
                if (params.type == ConnectivityType::Reciprocal) {
                    // Reciprocal connections also populate reverse direction bookkeeping
                    target_region->reserveInterRegionConnections(source_region->getId(), estimated_total_connections);
                }
            }
            
            // Helper to register inter-region connection tracking without duplicating synapses
            auto registerInterRegion = [&](NeuronPtr s, NeuronPtr t, SynapsePtr syn,
                                           const std::string& src_id, const std::string& tgt_id) {
                if (!syn || !s || !t) return;
                // Only mirror for inter-region connections
                if (src_id == tgt_id) return;

                RegionPtr src_r = getRegion(src_id);
                RegionPtr tgt_r = getRegion(tgt_id);
                if (!src_r || !tgt_r) return;

                // Validate that the neurons indeed belong to the given regions to prevent mis-bookkeeping
                // (e.g., lateral/intra-region connections or swapped IDs)
                auto s_check = src_r->getNeuron(s->getId());
                auto t_check = tgt_r->getNeuron(t->getId());
                if (!s_check || !t_check) return; // Neuron IDs not found in expected regions
                if (s_check.get() != s.get() || t_check.get() != t.get()) return; // Mismatch: don't mirror

                // Region::connectToRegion detects existing synapses and only registers bookkeeping
                src_r->connectToRegion(tgt_r, s->getId(), t->getId(), syn->getWeight(), syn->getType());
            };
            
            // Create connections based on connectivity type
            switch (params.type) {
                case ConnectivityType::Sparse:
                case ConnectivityType::Dense: {
                    float effective_probability = params.connection_probability;
                    if (params.type == ConnectivityType::Dense) {
                        effective_probability = std::min(0.8f, params.connection_probability * 3.0f);
                    }
                    
                    // Reserve per-neuron capacities based on estimated degrees (best-effort)
                    if (effective_probability > 0.0f) {
                        const std::size_t expected_out_per_source = static_cast<std::size_t>(target_neurons.size() * effective_probability);
                        const std::size_t expected_in_per_target = static_cast<std::size_t>(source_neurons.size() * effective_probability);
                        if (expected_out_per_source > 0) {
                            for (const auto &src_n : source_neurons) {
                                auto current_out = src_n->getOutputSynapseCount();
                                src_n->reserveOutputSynapses(current_out + expected_out_per_source);
                            }
                        }
                        if (expected_in_per_target > 0) {
                            for (const auto &tgt_n : target_neurons) {
                                auto current_in = tgt_n->getInputSynapseCount();
                                tgt_n->reserveInputSynapses(current_in + expected_in_per_target);
                            }
                        }
                    }
                    
                    for (const auto& source_neuron : source_neurons) {
                        std::size_t connections_made = 0;
                        
                        for (const auto& target_neuron : target_neurons) {
                            if (params.max_connections_per_neuron > 0 && connections_made >= params.max_connections_per_neuron) break;
                            
                            // Calculate distance-based probability
                            float distance = calculateNeuronDistance(source_neuron, target_neuron);
                            float connection_prob = calculateConnectionProbability(params, distance);
                            
                            std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
                            if (prob_dist(rng_) < connection_prob * effective_probability) {
                                SynapsePtr synapse = createSynapse(source_neuron, target_neuron, params);
                                if (synapse) {
                                    new_synapses.push_back(synapse);
                                    synapses_created++;
                                    connections_made++;
                                    
                                    // Progress reporting
                                    if (synapses_created >= next_progress_report) {
                                        float progress = (static_cast<float>(synapses_created) / estimated_total_connections) * 100.0f;
                                        std::cout << "ConnectivityManager: Progress " << std::fixed << std::setprecision(1) 
                                                  << progress << "% (" << synapses_created << "/" << estimated_total_connections 
                                                  << " synapses)" << std::endl;
                                        next_progress_report += progress_interval;
                                    }
                                    
                                    // Register inter-region bookkeeping (no duplicate synapses)
                                    registerInterRegion(source_neuron, target_neuron, synapse, source_id, target_id) ;
                                }
                            }
                        }
                    }
                    break;
                }
                
                case ConnectivityType::Feedforward: {
                    // Create layered feedforward connections
                    std::vector<NeuronPtr> source_vec, target_vec;
                    source_vec.reserve(source_neurons.size());
                    target_vec.reserve(target_neurons.size());
                    for (const auto& neuron : source_neurons) source_vec.push_back(neuron);
                    for (const auto& neuron : target_neurons) target_vec.push_back(neuron);
                    
                    std::size_t source_layer_size = source_vec.size() / 4; // Assume 4 layers
                    std::size_t target_layer_size = target_vec.size() / 4;
                    
                    // Reserve per-neuron capacities: each source connects to up to 5 targets in previous layer
                    if (target_layer_size > 0) {
                        for (const auto &src_n : source_vec) {
                            auto current_out = src_n->getOutputSynapseCount();
                            src_n->reserveOutputSynapses(current_out + std::min<std::size_t>(5, target_layer_size));
                        }
                    }
                    if (source_layer_size > 0) {
                        for (const auto &tgt_n : target_vec) {
                            auto current_in = tgt_n->getInputSynapseCount();
                            // Heuristic: ~5 incoming from previous layer window
                            tgt_n->reserveInputSynapses(current_in + 5);
                        }
                    }
                    
                    for (std::size_t layer = 0; layer < 3; ++layer) {
                        for (std::size_t i = 0; i < source_layer_size; ++i) {
                            std::size_t source_idx = layer * source_layer_size + i;
                            if (source_idx >= source_vec.size()) break;
                            
                            // Connect to multiple neurons in next layer
                            for (std::size_t j = 0; j < 5 && j < target_layer_size; ++j) {
                                std::size_t target_idx = (layer + 1) * target_layer_size + 
                                                       (i + j) % target_layer_size;
                                if (target_idx >= target_vec.size()) break;
                                
                                std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
                                if (prob_dist(rng_) < params.connection_probability) {
                                    SynapsePtr synapse = createSynapse(source_vec[source_idx], 
                                                                      target_vec[target_idx], params);
                                    if (synapse) {
                                        new_synapses.push_back(synapse);
                                        synapses_created++;
                                        
                                        // Progress reporting
                                        if (synapses_created >= next_progress_report) {
                                            float progress = (static_cast<float>(synapses_created) / estimated_total_connections) * 100.0f;
                                            std::cout << "ConnectivityManager: Progress " << std::fixed << std::setprecision(1) 
                                                      << progress << "% (" << synapses_created << "/" << estimated_total_connections 
                                                      << " synapses)" << std::endl;
                                            next_progress_report += progress_interval;
                                        }
                                        
                                        // Register inter-region bookkeeping (no duplicate synapses)
                                        registerInterRegion(source_vec[source_idx], target_vec[target_idx], synapse, source_id, target_id);
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                
                case ConnectivityType::Lateral: {
                    // Create lateral connections within similar layers
                    std::vector<NeuronPtr> source_vec, target_vec;
                    source_vec.reserve(source_neurons.size());
                    target_vec.reserve(target_neurons.size());
                    for (const auto& neuron : source_neurons) source_vec.push_back(neuron);
                    for (const auto& neuron : target_neurons) target_vec.push_back(neuron);
                    
                    std::size_t min_size = std::min(source_vec.size(), target_vec.size());
                    
                    // Reserve per-neuron capacities: each pair may create up to 2 connections with probability p
                    {
                        std::size_t expected_per = (params.connection_probability > 0.0f) ? 1 : 0;
                        if (expected_per > 0) {
                            for (const auto &src_n : source_vec) {
                                auto current_out = src_n->getOutputSynapseCount();
                                src_n->reserveOutputSynapses(current_out + expected_per);
                            }
                            for (const auto &tgt_n : target_vec) {
                                auto current_in = tgt_n->getInputSynapseCount();
                                tgt_n->reserveInputSynapses(current_in + expected_per);
                            }
                        }
                    }
                    
                    for (std::size_t i = 0; i < min_size; ++i) {
                        // Connect to nearby neurons (lateral inhibition/excitation)
                        for (int offset = -2; offset <= 2; ++offset) {
                            if (offset == 0) continue; // Skip self-connection
                            
                            int target_idx = static_cast<int>(i) + offset;
                            if (target_idx >= 0 && target_idx < static_cast<int>(target_vec.size())) {
                                std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
                                if (prob_dist(rng_) < params.connection_probability) {
                                    SynapsePtr synapse = createSynapse(source_vec[i], 
                                                                      target_vec[target_idx], params);
                                    if (synapse) {
                                        new_synapses.push_back(synapse);
                                        synapses_created++;
                                        
                                        // Register inter-region bookkeeping (no duplicate synapses)
                                        registerInterRegion(source_vec[i], target_vec[target_idx], synapse, source_id, target_id);
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                
                case ConnectivityType::Global: {
                    // Create long-range global connections
                    std::vector<NeuronPtr> source_vec, target_vec;
                    source_vec.reserve(source_neurons.size());
                    target_vec.reserve(target_neurons.size());
                    for (const auto& neuron : source_neurons) source_vec.push_back(neuron);
                    for (const auto& neuron : target_neurons) target_vec.push_back(neuron);
                    
                    // Sample a subset for global connections
                    std::size_t sample_size = std::min(source_vec.size() / 10, std::size_t(100));
                    
                    std::shuffle(source_vec.begin(), source_vec.end(), rng_);
                    std::shuffle(target_vec.begin(), target_vec.end(), rng_);
                    
                    // Heuristic per-neuron capacity reservation for sampled subset
                    if (params.connection_probability > 0.0f) {
                        const std::size_t min_source = std::min(sample_size, source_vec.size());
                        const std::size_t min_target = std::min(sample_size, target_vec.size());
                        std::size_t expected_out_per = static_cast<std::size_t>(min_target * params.connection_probability);
                        std::size_t expected_in_per  = static_cast<std::size_t>(min_source * params.connection_probability);
                        if (expected_out_per == 0) expected_out_per = 1;
                        if (expected_in_per == 0) expected_in_per = 1;
                        for (std::size_t i = 0; i < min_source; ++i) {
                            auto current_out = source_vec[i]->getOutputSynapseCount();
                            source_vec[i]->reserveOutputSynapses(current_out + expected_out_per);
                        }
                        for (std::size_t j = 0; j < min_target; ++j) {
                            auto current_in = target_vec[j]->getInputSynapseCount();
                            target_vec[j]->reserveInputSynapses(current_in + expected_in_per);
                        }
                    }
                    
                    for (std::size_t i = 0; i < sample_size && i < source_vec.size(); ++i) {
                        for (std::size_t j = 0; j < sample_size && j < target_vec.size(); ++j) {
                            std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
                            if (prob_dist(rng_) < params.connection_probability) {
                                SynapsePtr synapse = createSynapse(source_vec[i], target_vec[j], params);
                                if (synapse) {
                                    new_synapses.push_back(synapse);
                                    synapses_created++;
                                    
                                    // Register inter-region bookkeeping (no duplicate synapses)
                                    registerInterRegion(source_vec[i], target_vec[j], synapse, source_id, target_id);
                                }
                            }
                        }
                    }
                    break;
                }
                
                case ConnectivityType::Feedback: {
                    // Create feedback connections (similar to feedforward but with different weights)
                    std::vector<NeuronPtr> source_vec, target_vec;
                    source_vec.reserve(source_neurons.size());
                    target_vec.reserve(target_neurons.size());
                    for (const auto& neuron : source_neurons) source_vec.push_back(neuron);
                    for (const auto& neuron : target_neurons) target_vec.push_back(neuron);
                    
                    std::size_t source_layer_size = source_vec.size() / 4; // Assume 4 layers
                    std::size_t target_layer_size = target_vec.size() / 4;
                    
                    // Feedback connections go from higher to lower layers
                    for (std::size_t layer = 1; layer < 4; ++layer) {
                        for (std::size_t i = 0; i < source_layer_size; ++i) {
                            std::size_t source_idx = layer * source_layer_size + i;
                            if (source_idx >= source_vec.size()) break;
                            
                            // Connect to multiple neurons in previous layer
                            for (std::size_t j = 0; j < 3 && j < target_layer_size; ++j) {
                                std::size_t target_idx = (layer - 1) * target_layer_size + 
                                                       (i + j) % target_layer_size;
                                if (target_idx >= target_vec.size()) break;
                                
                                std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
                                if (prob_dist(rng_) < params.connection_probability) {
                                    SynapsePtr synapse = createSynapse(source_vec[source_idx], 
                                                                      target_vec[target_idx], params);
                                    if (synapse) {
                                        new_synapses.push_back(synapse);
                                        synapses_created++;
                                        
                                        // Register inter-region bookkeeping (no duplicate synapses)
                                        registerInterRegion(source_vec[source_idx], target_vec[target_idx], synapse, source_id, target_id);
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                
                case ConnectivityType::Reciprocal: {
                    // Create reciprocal connections (bidirectional by nature)
                    std::vector<NeuronPtr> source_vec, target_vec;
                    source_vec.reserve(source_neurons.size());
                    target_vec.reserve(target_neurons.size());
                    for (const auto& neuron : source_neurons) source_vec.push_back(neuron);
                    for (const auto& neuron : target_neurons) target_vec.push_back(neuron);
                    
                    std::size_t min_size = std::min(source_vec.size(), target_vec.size());
                    
                    // Reserve per-neuron capacities: each pair may create up to 2 connections with probability p
                    {
                        std::size_t expected_per = (params.connection_probability > 0.0f) ? 1 : 0;
                        if (expected_per > 0) {
                            for (const auto &src_n : source_vec) {
                                auto current_out = src_n->getOutputSynapseCount();
                                src_n->reserveOutputSynapses(current_out + expected_per);
                            }
                            for (const auto &tgt_n : target_vec) {
                                auto current_in = tgt_n->getInputSynapseCount();
                                tgt_n->reserveInputSynapses(current_in + expected_per);
                            }
                        }
                    }
                    
                    for (std::size_t i = 0; i < min_size; ++i) {
                        std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
                        if (prob_dist(rng_) < params.connection_probability) {
                            // Create forward synapse
                            SynapsePtr forward_synapse = createSynapse(source_vec[i], target_vec[i], params);
                            if (forward_synapse) {
                                new_synapses.push_back(forward_synapse);
                                synapses_created++;
                                
                                // Register inter-region bookkeeping (no duplicate synapses)
                                registerInterRegion(source_vec[i], target_vec[i], forward_synapse, source_id, target_id);
                            }
                            
                            // Create backward synapse
                            SynapsePtr backward_synapse = createSynapse(target_vec[i], source_vec[i], params);
                            if (backward_synapse) {
                                new_synapses.push_back(backward_synapse);
                                synapses_created++;
                                
                                // Register inter-region bookkeeping for backward synapse (reverse ids)
                                registerInterRegion(target_vec[i], source_vec[i], backward_synapse, target_id, source_id);
                            }
                        }
                    }
                    break;
                }
                
                case ConnectivityType::Modular: {
                    // Create modular connections within and between modules
                    std::vector<NeuronPtr> source_vec, target_vec;
                    source_vec.reserve(source_neurons.size());
                    target_vec.reserve(target_neurons.size());
                    for (const auto& neuron : source_neurons) source_vec.push_back(neuron);
                    for (const auto& neuron : target_neurons) target_vec.push_back(neuron);
                    
                    std::size_t module_size = 50; // Neurons per module
                    std::size_t source_modules = (source_vec.size() + module_size - 1) / module_size;
                    std::size_t target_modules = (target_vec.size() + module_size - 1) / module_size;
                    
                    for (std::size_t sm = 0; sm < source_modules; ++sm) {
                        for (std::size_t tm = 0; tm < target_modules; ++tm) {
                            float inter_module_prob = (sm == tm) ? params.connection_probability : 
                                                     params.connection_probability * 0.1f; // Weaker inter-module
                            
                            std::size_t source_start = sm * module_size;
                            std::size_t source_end = std::min(source_start + module_size, source_vec.size());
                            std::size_t target_start = tm * module_size;
                            std::size_t target_end = std::min(target_start + module_size, target_vec.size());
                            
                            for (std::size_t i = source_start; i < source_end; ++i) {
                                for (std::size_t j = target_start; j < target_end; ++j) {
                                    std::uniform_real_distribution<float> prob_dist(0.0f, 1.0f);
                                    if (prob_dist(rng_) < inter_module_prob) {
                                        SynapsePtr synapse = createSynapse(source_vec[i], target_vec[j], params);
                                        if (synapse) {
                                            new_synapses.push_back(synapse);
                                            synapses_created++;
                                            
                                            // Register inter-region bookkeeping (no duplicate synapses)
                                            registerInterRegion(source_vec[i], target_vec[j], synapse, source_id, target_id);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                
                default:
                    std::cerr << "ConnectivityManager: Unsupported connectivity type: " 
                              << static_cast<int>(params.type) << std::endl;
                    return 0;
            }
            
            // Create bidirectional connections if requested
            // For Reciprocal connectivity, connections are already bidirectional within the case logic.
            // Avoid duplicating connections by skipping the automatic bidirectional pass for Reciprocal.
            if (params.bidirectional && params.type != ConnectivityType::Reciprocal && source_id != target_id) {
                 ConnectionParameters reverse_params = params;
                 reverse_params.bidirectional = false; // Prevent infinite recursion
                 synapses_created += connectRegions(target_id, source_id, reverse_params);
             }
            
            // Record the connection
            RegionConnection connection;
            connection.source_region_id = source_id;
            connection.target_region_id = target_id;
            connection.type = params.type;
            connection.synapse_count = synapses_created;
            connection.average_weight = params.weight_mean;
            connection.connection_strength = static_cast<float>(synapses_created) / 
                                           (source_neurons.size() * target_neurons.size());
            connection.is_active = true;
            connection.creation_time = std::chrono::system_clock::now();
            // Track plasticity configuration used for this connection
            connection.plasticity_rate = params.plasticity_rate;
            connection.plasticity_rule = params.plasticity_rule;
            
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                connections_.push_back(connection);
                total_synapses_ += synapses_created;
            }
            
            std::cout << "ConnectivityManager: Created " << synapses_created 
                      << " synapses between '" << source_id << "' and '" << target_id << "'" << std::endl;
            
            return synapses_created;
            
        } catch (const std::exception& e) {
            std::cerr << "ConnectivityManager: EXCEPTION in connectRegions: " << e.what() << std::endl;
            return 0;
        } catch (...) {
            std::cerr << "ConnectivityManager: UNKNOWN EXCEPTION in connectRegions" << std::endl;
            return 0;
        }
    }

        std::size_t ConnectivityManager::connectRegionsWithPattern(const std::string& source_id,
                                                                 const std::string& target_id,
                                                                 const std::string& pattern_name) {
            auto it = connectivity_patterns_.find(pattern_name);
            if (it == connectivity_patterns_.end()) {
                std::cerr << "ConnectivityManager: Pattern '" << pattern_name << "' not found" << std::endl;
                return 0;
            }
            
            return connectRegions(source_id, target_id, it->second);
        }

        void ConnectivityManager::establishCorticalHierarchy(const std::vector<std::string>& region_hierarchy,
                                                           const std::vector<ConnectionParameters>& params) {
            std::cout << "ConnectivityManager: Starting establishCorticalHierarchy with " 
                      << region_hierarchy.size() << " regions" << std::endl;
            
            try {
                if (region_hierarchy.size() < 2) {
                    std::cerr << "ConnectivityManager: ERROR - Need at least 2 regions for hierarchy, got " 
                              << region_hierarchy.size() << std::endl;
                    return;
                }
                
                // Validate all regions exist before starting connections
                for (std::size_t i = 0; i < region_hierarchy.size(); ++i) {
                    const std::string& region_id = region_hierarchy[i];
                    std::cout << "ConnectivityManager: Validating region [" << i << "]: '" << region_id << "'" << std::endl;
                    
                    if (region_id.empty()) {
                        std::cerr << "ConnectivityManager: ERROR - Empty region ID at index " << i << std::endl;
                        return;
                    }
                    
                    RegionPtr region = getRegion(region_id);
                    if (!region) {
                        std::cerr << "ConnectivityManager: ERROR - Region '" << region_id 
                                  << "' not found at index " << i << std::endl;
                        return;
                    }
                    
                    const auto& neurons = region->getNeurons();
                    if (neurons.empty()) {
                        std::cerr << "ConnectivityManager: ERROR - Region '" << region_id 
                                  << "' has no neurons" << std::endl;
                        return;
                    }
                    
                    std::cout << "ConnectivityManager: Region '" << region_id << "' validated - " 
                              << neurons.size() << " neurons" << std::endl;
                }
                
                // Check connectivity patterns availability
                if (connectivity_patterns_.find("cortical_feedforward") == connectivity_patterns_.end()) {
                    std::cerr << "ConnectivityManager: ERROR - 'cortical_feedforward' pattern not found" << std::endl;
                    return;
                }
                
                std::cout << "ConnectivityManager: All regions validated, starting connections..." << std::endl;
                
                for (std::size_t i = 0; i < region_hierarchy.size() - 1; ++i) {
                    std::cout << "ConnectivityManager: Processing connection " << (i + 1) 
                              << "/" << (region_hierarchy.size() - 1) << std::endl;
                    
                    const std::string& source_id = region_hierarchy[i];
                    const std::string& target_id = region_hierarchy[i + 1];
                    
                    std::cout << "ConnectivityManager: Connecting '" << source_id 
                              << "' -> '" << target_id << "'" << std::endl;
                    
                    ConnectionParameters conn_params;
                    if (i < params.size()) {
                        conn_params = params[i];
                        std::cout << "ConnectivityManager: Using provided parameters for connection " << i << std::endl;
                    } else {
                        conn_params = connectivity_patterns_["cortical_feedforward"];
                        std::cout << "ConnectivityManager: Using default 'cortical_feedforward' parameters" << std::endl;
                    }
                    
                    // Feedforward connections
                    std::cout << "ConnectivityManager: Creating feedforward connections..." << std::endl;
                    std::size_t feedforward_synapses = connectRegions(source_id, target_id, conn_params);
                    std::cout << "ConnectivityManager: Created " << feedforward_synapses 
                              << " feedforward synapses" << std::endl;
                    
                    // Feedback connections (weaker)
                    std::cout << "ConnectivityManager: Creating feedback connections..." << std::endl;
                    ConnectionParameters feedback_params = conn_params;
                    feedback_params.type = ConnectivityType::Feedback;
                    feedback_params.connection_probability *= 0.3f; // Weaker feedback
                    feedback_params.weight_mean *= 0.7f;
                    
                    std::size_t feedback_synapses = connectRegions(target_id, source_id, feedback_params);
                    std::cout << "ConnectivityManager: Created " << feedback_synapses 
                              << " feedback synapses" << std::endl;
                    
                    std::cout << "ConnectivityManager: Completed connection " << (i + 1) 
                              << "/" << (region_hierarchy.size() - 1) << " successfully" << std::endl;
                }
                
                std::cout << "ConnectivityManager: Successfully established cortical hierarchy with " 
                          << region_hierarchy.size() << " regions" << std::endl;
                          
            } catch (const std::exception& e) {
                std::cerr << "ConnectivityManager: EXCEPTION in establishCorticalHierarchy: " 
                          << e.what() << std::endl;
                throw;
            } catch (...) {
                std::cerr << "ConnectivityManager: UNKNOWN EXCEPTION in establishCorticalHierarchy" << std::endl;
                throw;
            }
        }

        void ConnectivityManager::establishThalamoCorticaConnections(const std::string& thalamus_id,
                                                                   const std::vector<std::string>& cortical_regions,
                                                                   const ConnectionParameters& params) {
            std::cout << "ConnectivityManager: Starting establishThalamoCorticaConnections with thalamus '" 
                      << thalamus_id << "' and " << cortical_regions.size() << " cortical regions" << std::endl;
            
            try {
                // Validate thalamus region
                if (thalamus_id.empty()) {
                    std::cerr << "ConnectivityManager: ERROR - Empty thalamus ID" << std::endl;
                    return;
                }
                
                RegionPtr thalamus_region = getRegion(thalamus_id);
                if (!thalamus_region) {
                    std::cerr << "ConnectivityManager: ERROR - Thalamus region '" << thalamus_id 
                              << "' not found" << std::endl;
                    return;
                }
                
                const auto& thalamus_neurons = thalamus_region->getNeurons();
                if (thalamus_neurons.empty()) {
                    std::cerr << "ConnectivityManager: ERROR - Thalamus region '" << thalamus_id 
                              << "' has no neurons" << std::endl;
                    return;
                }
                
                std::cout << "ConnectivityManager: Thalamus region validated - " 
                          << thalamus_neurons.size() << " neurons" << std::endl;
                
                if (cortical_regions.empty()) {
                    std::cerr << "ConnectivityManager: ERROR - No cortical regions provided" << std::endl;
                    return;
                }
                
                // Validate all cortical regions
                for (std::size_t i = 0; i < cortical_regions.size(); ++i) {
                    const std::string& cortical_id = cortical_regions[i];
                    std::cout << "ConnectivityManager: Validating cortical region [" << i << "]: '" 
                              << cortical_id << "'" << std::endl;
                    
                    if (cortical_id.empty()) {
                        std::cerr << "ConnectivityManager: ERROR - Empty cortical region ID at index " 
                                  << i << std::endl;
                        return;
                    }
                    
                    RegionPtr cortical_region = getRegion(cortical_id);
                    if (!cortical_region) {
                        std::cerr << "ConnectivityManager: ERROR - Cortical region '" << cortical_id 
                                  << "' not found at index " << i << std::endl;
                        return;
                    }
                    
                    const auto& cortical_neurons = cortical_region->getNeurons();
                    if (cortical_neurons.empty()) {
                        std::cerr << "ConnectivityManager: ERROR - Cortical region '" << cortical_id 
                                  << "' has no neurons" << std::endl;
                        return;
                    }
                    
                    std::cout << "ConnectivityManager: Cortical region '" << cortical_id 
                              << "' validated - " << cortical_neurons.size() << " neurons" << std::endl;
                }
                
                std::cout << "ConnectivityManager: All regions validated, starting thalamo-cortical connections..." << std::endl;
                
                for (std::size_t i = 0; i < cortical_regions.size(); ++i) {
                    const std::string& cortical_id = cortical_regions[i];
                    std::cout << "ConnectivityManager: Processing thalamo-cortical connection " << (i + 1) 
                              << "/" << cortical_regions.size() << " with region '" << cortical_id << "'" << std::endl;
                    
                    // Thalamus to cortex (feedforward)
                    std::cout << "ConnectivityManager: Creating thalamus->cortex connection..." << std::endl;
                    std::size_t feedforward_synapses = connectRegions(thalamus_id, cortical_id, params);
                    std::cout << "ConnectivityManager: Created " << feedforward_synapses 
                              << " thalamus->cortex synapses" << std::endl;
                    
                    // Cortex to thalamus (feedback)
                    std::cout << "ConnectivityManager: Creating cortex->thalamus feedback connection..." << std::endl;
                    ConnectionParameters feedback_params = params;
                    feedback_params.type = ConnectivityType::Feedback;
                    feedback_params.connection_probability *= 0.5f;
                    
                    std::size_t feedback_synapses = connectRegions(cortical_id, thalamus_id, feedback_params);
                    std::cout << "ConnectivityManager: Created " << feedback_synapses 
                              << " cortex->thalamus synapses" << std::endl;
                    
                    std::cout << "ConnectivityManager: Completed thalamo-cortical connection " << (i + 1) 
                              << "/" << cortical_regions.size() << " successfully" << std::endl;
                }
                
                std::cout << "ConnectivityManager: Successfully established thalamo-cortical connections" << std::endl;
                
            } catch (const std::exception& e) {
                std::cerr << "ConnectivityManager: EXCEPTION in establishThalamoCorticaConnections: " 
                          << e.what() << std::endl;
                throw;
            } catch (...) {
                std::cerr << "ConnectivityManager: UNKNOWN EXCEPTION in establishThalamoCorticaConnections" << std::endl;
                throw;
            }
        }

        void ConnectivityManager::establishLimbicConnections(const std::vector<std::string>& limbic_regions,
                                                           const ConnectionParameters& params) {
            std::cout << "ConnectivityManager: Starting establishLimbicConnections with "
                      << limbic_regions.size() << " limbic regions" << std::endl;
            
            try {
                if (limbic_regions.empty()) {
                    std::cerr << "ConnectivityManager: ERROR - No limbic regions provided" << std::endl;
                    return;
                }
                
                if (limbic_regions.size() < 2) {
                    std::cerr << "ConnectivityManager: WARNING - Only " << limbic_regions.size() 
                              << " limbic region(s) provided, need at least 2 for connections" << std::endl;
                    return;
                }
                
                // Validate all limbic regions
                for (std::size_t i = 0; i < limbic_regions.size(); ++i) {
                    const std::string& region_id = limbic_regions[i];
                    std::cout << "ConnectivityManager: Validating limbic region [" << i << "]: '" 
                              << region_id << "'" << std::endl;
                    
                    if (region_id.empty()) {
                        std::cerr << "ConnectivityManager: ERROR - Empty limbic region ID at index " 
                                  << i << std::endl;
                        return;
                    }
                    
                    RegionPtr region = getRegion(region_id);
                    if (!region) {
                        std::cerr << "ConnectivityManager: ERROR - Limbic region '" << region_id 
                                  << "' not found at index " << i << std::endl;
                        return;
                    }
                    
                    const auto& neurons = region->getNeurons();
                    if (neurons.empty()) {
                        std::cerr << "ConnectivityManager: ERROR - Limbic region '" << region_id 
                                  << "' has no neurons" << std::endl;
                        return;
                    }
                    
                    std::cout << "ConnectivityManager: Limbic region '" << region_id 
                              << "' validated - " << neurons.size() << " neurons" << std::endl;
                }
                
                std::cout << "ConnectivityManager: All limbic regions validated, starting connections..." << std::endl;
                
                std::size_t total_connections = 0;
                std::size_t expected_connections = (limbic_regions.size() * (limbic_regions.size() - 1)) / 2;
                
                // Create dense reciprocal connections within limbic system
                for (std::size_t i = 0; i < limbic_regions.size(); ++i) {
                    for (std::size_t j = i + 1; j < limbic_regions.size(); ++j) {
                        total_connections++;
                        std::cout << "ConnectivityManager: Processing limbic connection " << total_connections 
                                  << "/" << expected_connections << ": '" << limbic_regions[i] 
                                  << "' <-> '" << limbic_regions[j] << "'" << std::endl;
                        
                        ConnectionParameters limbic_params = params;
                        limbic_params.type = ConnectivityType::Reciprocal;
                        limbic_params.bidirectional = true;
                        limbic_params.connection_probability *= 1.5f; // Denser limbic connections
                        
                        std::cout << "ConnectivityManager: Creating reciprocal limbic connection..." << std::endl;
                        std::size_t synapses_created = connectRegions(limbic_regions[i], limbic_regions[j], limbic_params);
                        std::cout << "ConnectivityManager: Created " << synapses_created 
                                  << " reciprocal synapses between limbic regions" << std::endl;
                        
                        std::cout << "ConnectivityManager: Completed limbic connection " << total_connections 
                                  << "/" << expected_connections << " successfully" << std::endl;
                    }
                }
                
                std::cout << "ConnectivityManager: Successfully established " << total_connections 
                          << " limbic system connections" << std::endl;
                          
            } catch (const std::exception& e) {
                std::cerr << "ConnectivityManager: EXCEPTION in establishLimbicConnections: "
                          << e.what() << std::endl;
                throw;
            } catch (...) {
                std::cerr << "ConnectivityManager: UNKNOWN EXCEPTION in establishLimbicConnections" << std::endl;
                throw;
            }
        }

        // ===== Initialization Patterns =====

        void ConnectivityManager::initializeRegion(const std::string& region_id, 
                                                  const InitializationPattern& pattern) {
            RegionPtr region = getRegion(region_id);
            if (!region) {
                std::cerr << "ConnectivityManager: Region '" << region_id << "' not found for initialization" << std::endl;
                return;
            }
            
            // Apply custom initializer if provided
            if (pattern.custom_initializer) {
                pattern.custom_initializer(region);
            }
            
            // Apply initial activation pattern with randomization for realistic activity
            if (!pattern.initial_activation_pattern.empty()) {
                const auto& neurons = region->getNeurons();
                std::uniform_real_distribution<float> noise_dist(-0.1f, 0.1f);
                std::uniform_real_distribution<float> random_activation(0.0f, 0.8f);
                
                std::size_t pattern_idx = 0;
                
                for (const auto& neuron : neurons) {
                    if (neuron && pattern_idx < pattern.initial_activation_pattern.size()) {
                        // Base activation from pattern + random noise
                        float base_activation = pattern.initial_activation_pattern[pattern_idx];
                        float noise = noise_dist(rng_);
                        float final_activation = std::clamp(base_activation + noise, 0.0f, 1.0f);
                        
                        // Add some completely random neurons for spontaneous activity
                        if (pattern_idx % 10 == 0) {
                            final_activation = random_activation(rng_);
                        }
                        
                        if (final_activation > 0.5f) {
                            neuron->setState(Core::Neuron::State::Active);
                        } else if (final_activation > 0.1f) {
                            neuron->setState(Core::Neuron::State::Inactive);
                        } else {
                            neuron->setState(Core::Neuron::State::Inhibited);
                        }
                        pattern_idx = (pattern_idx + 1) % pattern.initial_activation_pattern.size();
                    }
                }
            }
            
            std::cout << "ConnectivityManager: Initialized region '" << region_id 
                      << "' with pattern '" << pattern.pattern_name << "' (" << region->getNeurons().size() << " neurons)" << std::endl;
        }

        void ConnectivityManager::initializeAllRegions() {
            for (const auto& [region_id, region] : regions_) {
                // Determine appropriate initialization pattern based on region type and name
                std::string pattern_name = "default";
                Region::ActivationPattern activation_pattern = Region::ActivationPattern::Asynchronous;
                
                Region::Type region_type = region->getType();
                std::string region_name = region->getName();
                
                // Map region names to appropriate patterns and activation modes
                if (region_name.find("visual") != std::string::npos || 
                    region_name.find("auditory") != std::string::npos) {
                    pattern_name = "sensory_default";
                    activation_pattern = Region::ActivationPattern::Oscillatory;  // Sensory regions use rhythmic patterns
                } else if (region_name.find("motor") != std::string::npos) {
                    pattern_name = "motor_default";
                    activation_pattern = Region::ActivationPattern::Competitive;  // Motor regions use winner-take-all
                } else if (region_name.find("prefrontal") != std::string::npos) {
                    pattern_name = "cognitive_default";
                    activation_pattern = Region::ActivationPattern::Layered;      // Cognitive regions use layered processing
                } else if (region_name.find("hippocampus") != std::string::npos || 
                           region_name.find("amygdala") != std::string::npos ||
                           region_name.find("cingulate") != std::string::npos ||
                           region_name.find("insula") != std::string::npos) {
                    pattern_name = "limbic_default";
                    activation_pattern = Region::ActivationPattern::Synchronous;  // Limbic regions use synchronized activity
                } else {
                    // Use region type as fallback
                    switch (region_type) {
                        case Region::Type::Cortical:
                            pattern_name = "cognitive_default";
                            activation_pattern = Region::ActivationPattern::Layered;
                            break;
                        case Region::Type::Subcortical:
                            pattern_name = "limbic_default";
                            activation_pattern = Region::ActivationPattern::Synchronous;
                            break;
                        default:
                            pattern_name = "default";
                            activation_pattern = Region::ActivationPattern::Asynchronous;
                            break;
                    }
                }
                
                // Set the activation pattern for the region
                region->setActivationPattern(activation_pattern);
                
                // Initialize the region (sets is_active_ to true)
                region->initialize();
                
                // Initialize with the determined pattern
                auto it = initialization_patterns_.find(pattern_name);
                if (it != initialization_patterns_.end()) {
                    initializeRegion(region_id, it->second);
                } else {
                    // Fallback to default pattern
                    auto default_it = initialization_patterns_.find("default");
                    if (default_it != initialization_patterns_.end()) {
                        initializeRegion(region_id, default_it->second);
                    }
                }
                
                std::cout << "ConnectivityManager: Initialized region '" << region_id 
                          << "' with activation pattern '" << static_cast<int>(activation_pattern) << "'" << std::endl;
            }
            
            std::cout << "ConnectivityManager: Initialized all " << regions_.size() << " regions" << std::endl;
        }

        void ConnectivityManager::addInitializationPattern(const std::string& pattern_name, 
                                                         const InitializationPattern& pattern) {
            initialization_patterns_[pattern_name] = pattern;
            std::cout << "ConnectivityManager: Added initialization pattern '" << pattern_name << "'" << std::endl;
        }

        std::vector<std::string> ConnectivityManager::getAvailablePatterns() const {
            std::vector<std::string> pattern_names;
            pattern_names.reserve(initialization_patterns_.size());
            
            for (const auto& [name, pattern] : initialization_patterns_) {
                pattern_names.push_back(name);
            }
            
            return pattern_names;
        }

        // ===== Connection Management =====

        std::size_t ConnectivityManager::disconnectRegions(const std::string& source_id, 
                                                          const std::string& target_id) {
            RegionPtr source_region = getRegion(source_id);
            RegionPtr target_region = getRegion(target_id);
            
            if (!source_region || !target_region) {
                std::cerr << "ConnectivityManager: Source or target region not found" << std::endl;
                return 0;
            }
            
            std::size_t synapses_removed = 0;
            
            // Remove synapses between the regions
            // This is a simplified implementation - in practice, you'd need to track
            // individual synapses and remove them from both neurons
            
            // Remove connection record
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                connections_.erase(
                    std::remove_if(connections_.begin(), connections_.end(),
                        [&source_id, &target_id](const RegionConnection& conn) {
                            return (conn.source_region_id == source_id && conn.target_region_id == target_id) ||
                                   (conn.source_region_id == target_id && conn.target_region_id == source_id);
                        }),
                    connections_.end()
                );
            }
            
            std::cout << "ConnectivityManager: Disconnected regions '" << source_id 
                      << "' and '" << target_id << "'" << std::endl;
            
            return synapses_removed;
        }

        void ConnectivityManager::modifyConnectionStrength(const std::string& source_id, 
                                                         const std::string& target_id,
                                                         float strength_multiplier) {
            for (auto& connection : connections_) {
                if (connection.source_region_id == source_id && connection.target_region_id == target_id) {
                    connection.connection_strength *= strength_multiplier;
                    connection.average_weight *= strength_multiplier;
                    
                    // In practice, you'd modify the actual synapse weights here
                    
                    std::cout << "ConnectivityManager: Modified connection strength between '" 
                              << source_id << "' and '" << target_id << "' by factor " 
                              << strength_multiplier << std::endl;
                    return;
                }
            }
            
            std::cerr << "ConnectivityManager: Connection not found between '" 
                      << source_id << "' and '" << target_id << "'" << std::endl;
        }

        ConnectivityManager::RegionConnection ConnectivityManager::getConnectionInfo(
            const std::string& source_id, const std::string& target_id) const {
            
            for (const auto& connection : connections_) {
                if (connection.source_region_id == source_id && connection.target_region_id == target_id) {
                    return connection;
                }
            }
            
            // Return empty connection if not found
            RegionConnection empty_connection{};
            empty_connection.is_active = false;
            return empty_connection;
        }

        std::vector<ConnectivityManager::RegionConnection> ConnectivityManager::getAllConnections() const {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            return connections_;
        }

        // ===== Connectivity Analysis =====

        std::vector<std::vector<float>> ConnectivityManager::getConnectivityMatrix() const {
            std::unordered_map<std::string, std::size_t> region_indices;
            region_indices.reserve(regions_.size());

            std::size_t idx = 0;
            for (const auto& [id, region] : regions_) {
                region_indices[id] = idx++;
            }
            
            std::size_t n = regions_.size();
            std::vector<std::vector<float>> matrix(n, std::vector<float>(n, 0.0f));
            
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                for (const auto& connection : connections_) {
                    auto source_it = region_indices.find(connection.source_region_id);
                    auto target_it = region_indices.find(connection.target_region_id);
                    
                    if (source_it != region_indices.end() && target_it != region_indices.end()) {
                        std::size_t source_idx = source_it->second;
                        std::size_t target_idx = target_it->second;
                        
                        matrix[source_idx][target_idx] = connection.connection_strength;
                    }
                }
            }
            
            return matrix;
        }

        std::unordered_map<std::string, float> ConnectivityManager::analyzeNetworkProperties() const {
            std::unordered_map<std::string, float> properties;
            
            std::lock_guard<std::mutex> lock(connections_mutex_);
            
            properties["total_regions"] = static_cast<float>(regions_.size());
            properties["total_connections"] = static_cast<float>(connections_.size());
            properties["total_synapses"] = static_cast<float>(total_synapses_);
            properties["average_connections_per_region"] = regions_.empty() ? 0.0f : 
                static_cast<float>(connections_.size()) / regions_.size();
            
            // Calculate average connection strength
            float total_strength = 0.0f;
            for (const auto& connection : connections_) {
                total_strength += connection.connection_strength;
            }
            properties["average_connection_strength"] = connections_.empty() ? 0.0f : 
                total_strength / connections_.size();
            
            return properties;
        }

        std::unordered_map<std::string, float> ConnectivityManager::getConnectivityStatistics() const {
            return analyzeNetworkProperties();
        }

        // ===== Utility Functions =====

        void ConnectivityManager::setRandomSeed(std::uint32_t seed) {
            rng_.seed(seed);
            std::cout << "ConnectivityManager: Set random seed to " << seed << std::endl;
        }

        void ConnectivityManager::reset() {
            regions_.clear();
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                connections_.clear();
                total_synapses_ = 0;
            }
            connection_id_counter_ = 0;
            
            // Reinitialize default patterns
            initialization_patterns_.clear();
            connectivity_patterns_.clear();
            initializeDefaultPatterns();
            
            std::cout << "ConnectivityManager: Reset completed" << std::endl;
        }

        std::string ConnectivityManager::exportToJson() const {
            std::ostringstream json;
            json << "{\n";
            json << "  \"regions\": [";
            
            bool first_region = true;
            for (const auto& [id, region] : regions_) {
                if (!first_region) json << ",";
                json << "\n    {\"id\": \"" << id << "\", \"type\": \"" 
                     << static_cast<int>(region->getType()) << "\"}";
                first_region = false;
            }
            
            json << "\n  ],\n";
            json << "  \"connections\": [";
            
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                bool first_connection = true;
                for (const auto& connection : connections_) {
                    if (!first_connection) json << ",";
                    json << "\n    {";
                    json << "\"source\": \"" << connection.source_region_id << "\", ";
                    json << "\"target\": \"" << connection.target_region_id << "\", ";
                    json << "\"strength\": " << connection.connection_strength << ", ";
                    json << "\"synapses\": " << connection.synapse_count << ", ";
                    json << "\"plasticity_rate\": " << connection.plasticity_rate << ", ";
                    json << "\"plasticity_rule\": " << static_cast<int>(connection.plasticity_rule);
                    json << "}";
                    first_connection = false;
                }
            }
            
            json << "\n  ]\n}";
            
            return json.str();
        }

        bool ConnectivityManager::importFromJson(const std::string& json_config) {
            // Minimal parser for the exact format produced by exportToJson()
            // Clear existing state
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                connections_.clear();
                total_synapses_ = 0;
            }
            regions_.clear();
            connection_id_counter_ = 0;

            auto trim = [](const std::string& s) -> std::string {
                size_t b = 0, e = s.size();
                while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
                while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
                return s.substr(b, e - b);
            };

            auto findArrayBounds = [&](const std::string& key) -> std::pair<std::size_t, std::size_t> {
                const std::string k = "\"" + key + "\"";
                std::size_t pos = json_config.find(k);
                if (pos == std::string::npos) return {std::string::npos, std::string::npos};
                std::size_t lb = json_config.find('[', pos);
                if (lb == std::string::npos) return {std::string::npos, std::string::npos};
                std::size_t rb = json_config.find(']', lb);
                if (rb == std::string::npos) return {std::string::npos, std::string::npos};
                return {lb + 1, rb};
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

            // Parse regions
            auto [rBegin, rEnd] = findArrayBounds("regions");
            if (rBegin != std::string::npos && rEnd != std::string::npos && rBegin <= rEnd) {
                std::size_t i = rBegin;
                while (true) {
                    std::size_t lb = json_config.find('{', i);
                    if (lb == std::string::npos || lb >= rEnd) break;
                    std::size_t rb = json_config.find('}', lb);
                    if (rb == std::string::npos || rb > rEnd) break;
                    std::string obj = json_config.substr(lb + 1, rb - lb - 1);
                    obj = trim(obj);

                    std::string id_str;
                    double type_num = 0.0;
                    bool ok_id = getStringValue(obj, "id", id_str);
                    bool ok_type = getNumberValue(obj, "type", type_num);
                    if (ok_id) {
                        auto name = std::string("Region_") + id_str;
                        auto rid = static_cast<NeuroForge::RegionID>(std::stoul(id_str));
                        auto rtype = ok_type ? static_cast<NeuroForge::Core::Region::Type>(static_cast<int>(type_num))
                                             : NeuroForge::Core::Region::Type::Custom;
                        auto region = NeuroForge::Core::RegionFactory::createRegion(rid, name, rtype, NeuroForge::Core::Region::ActivationPattern::Asynchronous);
                        registerRegion(region);
                    }
                    i = rb + 1;
                }
            }

            // Parse connections
            std::vector<RegionConnection> new_connections;
            std::size_t new_total_synapses = 0;

            auto [cBegin, cEnd] = findArrayBounds("connections");
            if (cBegin != std::string::npos && cEnd != std::string::npos && cBegin <= cEnd) {
                std::size_t i = cBegin;
                while (true) {
                    std::size_t lb = json_config.find('{', i);
                    if (lb == std::string::npos || lb >= cEnd) break;
                    std::size_t rb = json_config.find('}', lb);
                    if (rb == std::string::npos || rb > cEnd) break;
                    std::string obj = json_config.substr(lb + 1, rb - lb - 1);
                    obj = trim(obj);

                    std::string source_id, target_id;
                    double strength = 0.0, synapses = 0.0, plast_rate = 0.0, plast_rule = 0.0;
                    bool ok_src = getStringValue(obj, "source", source_id);
                    bool ok_tgt = getStringValue(obj, "target", target_id);
                    getNumberValue(obj, "strength", strength);
                    getNumberValue(obj, "synapses", synapses);
                    getNumberValue(obj, "plasticity_rate", plast_rate);
                    getNumberValue(obj, "plasticity_rule", plast_rule);

                    if (ok_src && ok_tgt) {
                        RegionConnection rc;
                        rc.source_region_id = source_id;
                        rc.target_region_id = target_id;
                        rc.type = ConnectivityType::Sparse;
                        rc.synapse_count = static_cast<std::size_t>(synapses < 0 ? 0 : synapses);
                        rc.average_weight = 0.0f;
                        rc.connection_strength = static_cast<float>(strength);
                        rc.is_active = true;
                        rc.creation_time = std::chrono::system_clock::now();
                        rc.plasticity_rate = static_cast<float>(plast_rate);
                        rc.plasticity_rule = static_cast<NeuroForge::Core::Synapse::PlasticityRule>(static_cast<int>(plast_rule));

                        new_total_synapses += rc.synapse_count;
                        new_connections.push_back(std::move(rc));
                    }
                    i = rb + 1;
                }
            }

            // Commit
            {
                std::lock_guard<std::mutex> lock(connections_mutex_);
                connections_ = std::move(new_connections);
                total_synapses_ = new_total_synapses;
            }

            return true;
        }
        
        std::size_t ConnectivityManager::getTotalSynapseCount() const {
            return total_synapses_;
        }

        // ===== Private Helper Methods =====

        float ConnectivityManager::calculateConnectionProbability(const ConnectionParameters& params, 
                                                                float distance) const {
            switch (params.distribution) {
                case ProbabilityDistribution::Uniform:
                    return 1.0f;
                    
                case ProbabilityDistribution::Gaussian:
                    return std::exp(-distance * distance / (2.0f * params.distance_decay * params.distance_decay));
                    
                case ProbabilityDistribution::Exponential:
                    return std::exp(-distance / params.distance_decay);
                    
                case ProbabilityDistribution::PowerLaw:
                    return std::pow(distance + 1.0f, -params.distance_decay);
                    
                case ProbabilityDistribution::SmallWorld:
                    // Simplified small-world: high local, some long-range
                    if (distance < 2.0f) return 0.8f;
                    else return 0.1f * std::exp(-distance / params.distance_decay);
                    
                default:
                    return 1.0f;
            }
        }

        float ConnectivityManager::generateSynapticWeight(const ConnectionParameters& params) const {
            std::normal_distribution<float> weight_dist(params.weight_mean, params.weight_std);
            float weight = weight_dist(rng_);
            return std::clamp(weight, -2.0f, 2.0f); // Clamp to reasonable range
        }

        SynapsePtr ConnectivityManager::createSynapse(NeuronPtr source, NeuronPtr target, 
                                                    const ConnectionParameters& params) const {
            if (!source || !target) return nullptr;
            
            float weight = generateSynapticWeight(params);
            
            // Use centralized SynapseFactory for synapse creation and ID management
            auto synapse = Core::SynapseFactory::createSynapse(source, target, weight, NeuroForge::SynapseType::Excitatory);
            synapse->setLearningRate(params.plasticity_rate);
            synapse->setPlasticityRule(params.plasticity_rule);
            
            // Add synapse to neurons
            source->addOutputSynapse(synapse);
            target->addInputSynapse(synapse);
            
            return synapse;
        }

        float ConnectivityManager::calculateNeuronDistance(NeuronPtr neuron1, NeuronPtr neuron2) const {
            // Simplified distance calculation based on neuron IDs
            // In practice, you'd use actual spatial coordinates
            std::uint64_t id1 = neuron1->getId();
            std::uint64_t id2 = neuron2->getId();
            
            return std::abs(static_cast<float>(id1) - static_cast<float>(id2)) / 1000.0f;
        }

        void ConnectivityManager::initializeDefaultPatterns() {
            // Default connectivity patterns with higher probabilities to ensure synapse creation
            ConnectionParameters sparse_pattern;
            sparse_pattern.type = ConnectivityType::Sparse;
            sparse_pattern.connection_probability = 0.4f;  // Increased from 0.1f
            sparse_pattern.weight_mean = 0.5f;
            sparse_pattern.weight_std = 0.1f;
            connectivity_patterns_["sparse"] = sparse_pattern;
            
            ConnectionParameters dense_pattern;
            dense_pattern.type = ConnectivityType::Dense;
            dense_pattern.connection_probability = 0.7f;  // Increased from 0.3f
            dense_pattern.weight_mean = 0.4f;
            dense_pattern.weight_std = 0.15f;
            connectivity_patterns_["dense"] = dense_pattern;
            
            ConnectionParameters feedforward_pattern;
            feedforward_pattern.type = ConnectivityType::Feedforward;
            feedforward_pattern.connection_probability = 0.6f;  // Increased from 0.2f
            feedforward_pattern.weight_mean = 0.6f;
            feedforward_pattern.weight_std = 0.1f;
            connectivity_patterns_["cortical_feedforward"] = feedforward_pattern;
            
            // Default initialization patterns - reduced sizes for stack safety
            InitializationPattern default_init;
            default_init.pattern_name = "default";
            default_init.neuron_count = 100;  // Reduced from 1000
            default_init.initial_activation_pattern = {0.3f, 0.5f, 0.4f, 0.2f};  // Reduced pattern size
            initialization_patterns_["default"] = default_init;
            
            InitializationPattern sensory_init;
            sensory_init.pattern_name = "sensory_default";
            sensory_init.neuron_count = 200;  // Reduced from 2000
            sensory_init.initial_activation_pattern = {0.6f, 0.8f, 0.5f, 0.4f};  // Reduced pattern size
            initialization_patterns_["sensory_default"] = sensory_init;
            
            InitializationPattern motor_init;
            motor_init.pattern_name = "motor_default";
            motor_init.neuron_count = 150;  // Reduced from 1500
            motor_init.initial_activation_pattern = {0.5f, 0.7f, 0.8f, 0.6f};  // Reduced pattern size
            initialization_patterns_["motor_default"] = motor_init;
            
            InitializationPattern cognitive_init;
            cognitive_init.pattern_name = "cognitive_default";
            cognitive_init.neuron_count = 300;  // Reduced from 3000
            cognitive_init.initial_activation_pattern = {0.4f, 0.3f, 0.6f, 0.5f};  // Reduced pattern size
            initialization_patterns_["cognitive_default"] = cognitive_init;
            
            InitializationPattern limbic_init;
            limbic_init.pattern_name = "limbic_default";
            limbic_init.neuron_count = 120;  // Reduced from 1200
            limbic_init.initial_activation_pattern = {0.7f, 0.6f, 0.5f, 0.8f};  // Reduced pattern size
            initialization_patterns_["limbic_default"] = limbic_init;
        }

        bool ConnectivityManager::validateConnectionParameters(const ConnectionParameters& params) const {
            if (params.connection_probability < 0.0f || params.connection_probability > 1.0f) {
                std::cerr << "ConnectivityManager: Invalid connection probability" << std::endl;
                return false;
            }
            
            if (params.weight_std < 0.0f) {
                std::cerr << "ConnectivityManager: Invalid weight standard deviation" << std::endl;
                return false;
            }
            
            // 0 means unlimited connections per neuron; treat as valid
            
            return true;
        }

    } // namespace Connectivity
} // namespace NeuroForge