#include "core/Region.h"
#include "core/Neuron.h"
#include "core/Synapse.h"
#include "core/CUDAAccel.h"
#include <algorithm>
#include <numeric>
#include <chrono>
#include <random>
#include <functional>
#include <unordered_set>
#include <execution>

#ifdef __AVX2__
#include <immintrin.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif

namespace NeuroForge {
    namespace Core {

        // Static member initialization
        std::atomic<RegionID> RegionFactory::next_id_{1};

        // Region Implementation
        Region::Region(RegionID id, 
                      const std::string& name, 
                      Type type,
                      ActivationPattern pattern)
            : id_(id)
            , name_(name)
            , type_(type)
            , activation_pattern_(pattern) {
            
            // Reserve space for efficient memory usage
            neurons_.reserve(1000);  // Start with reasonable capacity
            internal_synapses_.reserve(5000);
        }

        bool Region::addNeuron(NeuronPtr neuron) {
            if (!neuron) {
                return false;
            }

            std::lock_guard<std::mutex> lock(region_mutex_);
            
            // Check if neuron already exists
            auto it = std::find_if(neurons_.begin(), neurons_.end(),
                [neuron](const NeuronPtr& existing) {
                    return existing && existing->getId() == neuron->getId();
                });
            
            if (it != neurons_.end()) {
                return false; // Neuron already exists
            }

            neurons_.push_back(neuron);
            mito_states_.emplace_back(); // Add default mitochondrial state
            return true;
        }

        bool Region::removeNeuron(NeuronID neuron_id) {
            std::lock_guard<std::mutex> lock(region_mutex_);
            
            auto it = std::find_if(neurons_.begin(), neurons_.end(),
                [neuron_id](const NeuronPtr& neuron) {
                    return neuron && neuron->getId() == neuron_id;
                });
            
            if (it == neurons_.end()) {
                return false;
            }

            // Remove associated synapses
            {
                std::lock_guard<std::mutex> conn_lock(connection_mutex_);
                input_connections_.erase(neuron_id);
                output_connections_.erase(neuron_id);
            }

            // Remove internal synapses connected to this neuron
            internal_synapses_.erase(
                std::remove_if(internal_synapses_.begin(), internal_synapses_.end(),
                    [neuron_id](const SynapsePtr& synapse) {
                        if (!synapse) return true;
                        auto source = synapse->getSource().lock();
                        auto target = synapse->getTarget().lock();
                        return (source && source->getId() == neuron_id) ||
                               (target && target->getId() == neuron_id);
                    }),
                internal_synapses_.end());

            // Remove mitochondrial state (must match index)
            auto index = std::distance(neurons_.begin(), it);
            if (index >= 0 && index < static_cast<long>(mito_states_.size())) {
                mito_states_.erase(mito_states_.begin() + index);
            }

            neurons_.erase(it);
            return true;
        }

        NeuronPtr Region::getNeuron(NeuronID neuron_id) const {
            std::lock_guard<std::mutex> lock(region_mutex_);
            
            auto it = std::find_if(neurons_.begin(), neurons_.end(),
                [neuron_id](const NeuronPtr& neuron) {
                    return neuron && neuron->getId() == neuron_id;
                });
            
            return (it != neurons_.end()) ? *it : nullptr;
        }

        std::vector<NeuronPtr> Region::createNeurons(std::size_t count) {
            std::vector<NeuronPtr> created_neurons;
            created_neurons.reserve(count);

            // Create all neurons first without locking
            for (std::size_t i = 0; i < count; ++i) {
                auto unique_neuron = NeuronFactory::createNeuron();
                auto neuron = std::shared_ptr<Neuron>(std::move(unique_neuron));
                created_neurons.push_back(neuron);
            }

            // Add all neurons to the region in a single lock operation
            {
                std::lock_guard<std::mutex> lock(region_mutex_);
                neurons_.insert(neurons_.end(), created_neurons.begin(), created_neurons.end());
                // Initialize mitochondrial states for new neurons
                mito_states_.insert(mito_states_.end(), count, MitochondrialState{});
            }

            return created_neurons;
        }

        std::vector<NeuronPtr> Region::spawnNeurons(std::size_t count, float energy_gate) {
            // Gate neurogenesis by average mitochondrial energy
            float avg_energy = 0.0f;
            {
                std::lock_guard<std::mutex> lock(region_mutex_);
                if (mito_states_.empty() || count == 0) {
                    return {};
                }
                for (const auto& s : mito_states_) avg_energy += s.energy;
                avg_energy /= static_cast<float>(mito_states_.size());
            }

            if (avg_energy < std::clamp(energy_gate, 0.0f, 1.0f)) {
                return {};
            }

            // Delegate creation to existing pathway
            auto created = createNeurons(count);
            // Optionally initialize slight baseline activation to integrate
            for (auto& n : created) {
                if (n) {
                    n->setActivation(0.05f);
                }
            }
            return created;
        }

        std::size_t Region::pruneWeakSynapses(float weight_threshold) {
            const float thr = std::max(0.0f, weight_threshold);

            // Snapshot candidates under connection lock to avoid concurrent mutation during scan
            std::vector<SynapsePtr> to_prune;
            {
                std::lock_guard<std::mutex> lock(connection_mutex_);
                to_prune.reserve(internal_synapses_.size());
                for (const auto& s : internal_synapses_) {
                    if (!s) continue;
                    const float w = std::abs(s->getWeight());
                    if (w < thr) {
                        to_prune.push_back(s);
                    }
                }
            }

            if (to_prune.empty()) return 0;

            // Remove from neuron-local lists first to avoid deadlocks
            for (const auto& s : to_prune) {
                if (!s) continue;
                auto src = s->getSource().lock();
                auto tgt = s->getTarget().lock();
                if (src) src->removeOutputSynapse(s);
                if (tgt) tgt->removeInputSynapse(s);
            }

            // Remove from region bookkeeping structures
            {
                std::lock_guard<std::mutex> lock(connection_mutex_);
                // internal_synapses_
                internal_synapses_.erase(
                    std::remove_if(internal_synapses_.begin(), internal_synapses_.end(),
                        [&](const SynapsePtr& s) {
                            if (!s) return true;
                            return std::find_if(to_prune.begin(), to_prune.end(),
                                [&](const SynapsePtr& x){ return x.get() == s.get(); }) != to_prune.end();
                        }),
                    internal_synapses_.end()
                );

                // input/output maps
                for (auto& kv : input_connections_) {
                    auto& vec = kv.second;
                    vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const SynapsePtr& s){
                        return s && std::find_if(to_prune.begin(), to_prune.end(),
                            [&](const SynapsePtr& x){ return x.get() == s.get(); }) != to_prune.end();
                    }), vec.end());
                }
                for (auto& kv : output_connections_) {
                    auto& vec = kv.second;
                    vec.erase(std::remove_if(vec.begin(), vec.end(), [&](const SynapsePtr& s){
                        return s && std::find_if(to_prune.begin(), to_prune.end(),
                            [&](const SynapsePtr& x){ return x.get() == s.get(); }) != to_prune.end();
                    }), vec.end());
                }
            }

            return static_cast<std::size_t>(to_prune.size());
        }

        std::size_t Region::growSynapses(std::size_t max_new,
                                         float min_activation,
                                         Weight initial_weight,
                                         SynapseType type) {
            if (max_new == 0) return 0;

            // Snapshot active neurons to avoid holding region mutex during connect
            std::vector<NeuronPtr> active;
            {
                std::lock_guard<std::mutex> lock(region_mutex_);
                active.reserve(neurons_.size());
                for (const auto& n : neurons_) {
                    if (n && n->getActivation() >= std::clamp(min_activation, 0.0f, 1.0f)) {
                        active.push_back(n);
                    }
                }
            }

            if (active.size() < 2) return 0;

            // Randomized pairing to diversify connectivity
            std::mt19937 rng(static_cast<unsigned>(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
            std::shuffle(active.begin(), active.end(), rng);

            std::size_t grown = 0;
            // Helper to check existing connection quickly
            auto exists_conn = [&](NeuronID src_id, NeuronID tgt_id) {
                std::lock_guard<std::mutex> lock(connection_mutex_);
                auto it = output_connections_.find(src_id);
                if (it == output_connections_.end()) return false;
                for (const auto& s : it->second) {
                    if (!s) continue;
                    auto t = s->getTarget().lock();
                    if (t && t->getId() == tgt_id) return true;
                }
                return false;
            };

            for (std::size_t i = 0; i < active.size() && grown < max_new; ++i) {
                for (std::size_t j = i + 1; j < active.size() && grown < max_new; ++j) {
                    auto& a = active[i];
                    auto& b = active[j];
                    if (!a || !b) continue;
                    const auto aid = a->getId();
                    const auto bid = b->getId();
                    if (aid == bid) continue;

                    // Prefer bidirectional excitatory connections among highly active neurons
                    if (!exists_conn(aid, bid)) {
                        auto s = connectNeurons(aid, bid, initial_weight, type);
                        if (s) ++grown;
                    }
                    if (grown >= max_new) break;
                    if (!exists_conn(bid, aid)) {
                        auto s = connectNeurons(bid, aid, initial_weight, type);
                        if (s) ++grown;
                    }
                }
            }

            return grown;
        }

        bool Region::addInternalSynapse(SynapsePtr synapse) {
            if (!synapse || !synapse->isValid()) {
                return false;
            }

            auto source = synapse->getSource().lock();
            auto target = synapse->getTarget().lock();
            
            if (!source || !target) {
                return false;
            }

            // Verify both neurons belong to this region
            bool source_found = false, target_found = false;
            {
                std::lock_guard<std::mutex> lock(region_mutex_);
                for (const auto& neuron : neurons_) {
                    if (neuron && neuron->getId() == source->getId()) {
                        source_found = true;
                    }
                    if (neuron && neuron->getId() == target->getId()) {
                        target_found = true;
                    }
                    if (source_found && target_found) break;
                }
            }

            if (!source_found || !target_found) {
                return false;
            }

            {
                std::lock_guard<std::mutex> lock(connection_mutex_);
                internal_synapses_.push_back(synapse);
                
                // Update connection maps
                input_connections_[target->getId()].push_back(synapse);
                output_connections_[source->getId()].push_back(synapse);
            }

            return true;
        }

        SynapsePtr Region::connectNeurons(NeuronID source_id,
                                         NeuronID target_id,
                                         Weight weight,
                                         SynapseType type) {
            
            auto source = getNeuron(source_id);
            auto target = getNeuron(target_id);
            
            if (!source || !target) {
                return nullptr;
            }

            auto synapse = SynapseFactory::createSynapse(source, target, weight, type);

            if (addInternalSynapse(synapse)) {
                // Pre-reserve per-neuron synapse capacity to minimize reallocations
                source->reserveOutputSynapses(source->getOutputSynapseCount() + 1);
                target->reserveInputSynapses(target->getInputSynapseCount() + 1);
                // Add synapse to neurons
                source->addOutputSynapse(synapse);
                target->addInputSynapse(synapse);
                return synapse;
            }

            return nullptr;
        }

        SynapsePtr Region::connectToRegion(RegionPtr target_region,
                                         NeuronID source_neuron_id,
                                         NeuronID target_neuron_id,
                                         Weight weight,
                                         SynapseType type) {
            
            if (!target_region) {
                return nullptr;
            }

            auto source = getNeuron(source_neuron_id);
            auto target = target_region->getNeuron(target_neuron_id);
            
            if (!source || !target) {
                return nullptr;
            }

            // Check if a synapse between these neurons already exists (created elsewhere)
            SynapsePtr existing = nullptr;
            {
                auto outs = source->getOutputSynapses();
                for (const auto &s : outs) {
                    auto t = s->getTarget().lock();
                    if (t && t->getId() == target->getId()) {
                        existing = s;
                        break;
                    }
                }
            }
            if (existing) {
                // Pre-reserve bookkeeping capacities to avoid reallocation during push_back
                reserveInterRegionConnections(target_region->getId(), 1);
                reserveOutputConnections(source->getId(), 1);
                target_region->reserveInputConnections(target->getId(), 1);

                // Register inter-region connection mapping without duplicating entries
                {
                    std::lock_guard<std::mutex> lock(connection_mutex_);
                    auto &vec = inter_region_connections_[target_region->getId()];
                    // Ensure at least one slot free without relying on default small reserve
                    if (vec.capacity() < vec.size() + 1) vec.reserve(vec.size() + 8);
                    bool present = false;
                    for (const auto &s : vec) {
                        if (s.get() == existing.get()) { // pointer equality is sufficient and faster
                            present = true;
                            break;
                        }
                    }
                    if (!present) {
                        vec.push_back(existing);
                    }
                    auto &outv = output_connections_[source->getId()];
                    if (outv.capacity() < outv.size() + 1) outv.reserve(outv.size() + 8);
                    bool present_out = false;
                    for (const auto &s : outv) {
                        if (s.get() == existing.get()) {
                            present_out = true;
                            break;
                        }
                    }
                    if (!present_out) {
                        outv.push_back(existing);
                    }
                }
                {
                    std::lock_guard<std::mutex> target_lock(target_region->connection_mutex_);
                    auto &invec = target_region->input_connections_[target->getId()];
                    if (invec.capacity() < invec.size() + 1) invec.reserve(invec.size() + 8);
                    bool present_in = false;
                    for (const auto &s : invec) {
                        if (s.get() == existing.get()) {
                            present_in = true;
                            break;
                        }
                    }
                    if (!present_in) {
                        invec.push_back(existing);
                    }
                }
                return existing;
            }

            auto synapse = SynapseFactory::createSynapse(source, target, weight, type);
            
            // Add synapse to neurons FIRST to avoid deadlock
            // (acquire synapse_mutex_ before connection_mutex_)
            // Pre-reserve per-neuron capacities before inserting
            source->reserveOutputSynapses(source->getOutputSynapseCount() + 1);
            target->reserveInputSynapses(target->getInputSynapseCount() + 1);
            source->addOutputSynapse(synapse);
            target->addInputSynapse(synapse);

            // Pre-reserve bookkeeping capacities to avoid reallocation during push_back
            reserveInterRegionConnections(target_region->getId(), 1);
            reserveOutputConnections(source->getId(), 1);
            target_region->reserveInputConnections(target->getId(), 1);
            
            {
                std::lock_guard<std::mutex> lock(connection_mutex_);
                auto &ireg = inter_region_connections_[target_region->getId()];
                if (ireg.capacity() < ireg.size() + 1) ireg.reserve(ireg.size() + 8);
                ireg.push_back(synapse);
                auto &outv = output_connections_[source->getId()];
                if (outv.capacity() < outv.size() + 1) outv.reserve(outv.size() + 8);
                outv.push_back(synapse);
            }

            // Add to target region's input connections
            {
                std::lock_guard<std::mutex> target_lock(target_region->connection_mutex_);
                auto &invec = target_region->input_connections_[target->getId()];
                if (invec.capacity() < invec.size() + 1) invec.reserve(invec.size() + 8);
                invec.push_back(synapse);
            }

            return synapse;
        }

        void Region::process(float delta_time) {
            if (!is_active_.load(std::memory_order_relaxed)) {
                return;
            }

            auto start_time = std::chrono::steady_clock::now();

            // Process neurons according to activation pattern
            // Create a copy of neurons to avoid holding region_mutex_ during processing
            // This prevents deadlock with learning operations that access neuron synapses
            std::vector<NeuronPtr> neurons_copy;
            {
                std::lock_guard<std::mutex> lock(region_mutex_);
                neurons_copy = neurons_; // Copy the vector
            }
            
            // Process neurons without holding region_mutex_ to avoid deadlock
            processNeuronsFromCopy(neurons_copy, delta_time);

            // R-STDP-lite: decay and accumulate eligibility traces for synapses connected to this region
            {
                // Snapshot synapse pointers under connection lock, then process outside the lock
                std::vector<SynapsePtr> synapses_snapshot;
                {
                    std::lock_guard<std::mutex> lock(connection_mutex_);
                    synapses_snapshot.reserve(internal_synapses_.size());
                    // Internal synapses (within region)
                    synapses_snapshot.insert(synapses_snapshot.end(), internal_synapses_.begin(), internal_synapses_.end());
                    // Incoming connections to neurons in this region
                    for (const auto &kv : input_connections_) {
                        synapses_snapshot.insert(synapses_snapshot.end(), kv.second.begin(), kv.second.end());
                    }
                    // Outgoing connections from neurons in this region
                    for (const auto &kv : output_connections_) {
                        synapses_snapshot.insert(synapses_snapshot.end(), kv.second.begin(), kv.second.end());
                    }
                    // Inter-region connections recorded by this region
                    for (const auto &kv : inter_region_connections_) {
                        const auto &vec = kv.second;
                        synapses_snapshot.insert(synapses_snapshot.end(), vec.begin(), vec.end());
                    }
                }
                // Remove duplicate pointers
                std::unordered_set<Synapse*> seen;
                seen.reserve(synapses_snapshot.size() * 2);
                std::vector<SynapsePtr> unique_synapses;
                unique_synapses.reserve(synapses_snapshot.size());
                for (auto &sp : synapses_snapshot) {
                    if (!sp) continue;
                    Synapse* raw = sp.get();
                    if (seen.insert(raw).second) {
                        unique_synapses.push_back(std::move(sp));
                    }
                }

                // Update eligibility: exponential decay and co-activation accumulation
                constexpr float E_DECAY_RATE = 1.0f; // per-second decay rate (tunable)
                const float dt = std::max(0.0f, delta_time);
                for (auto &s : unique_synapses) {
                    if (!s) continue;
                    // Skip modulatory synapses for plasticity dynamics
                    if (s->getType() == SynapseType::Modulatory) continue;
                    auto src = s->getSource().lock();
                    auto tgt = s->getTarget().lock();
                    if (!src || !tgt) continue;
                    const float pre = src->getActivation();
                    const float post = tgt->getActivation();
                    s->decayEligibility(E_DECAY_RATE, dt);
                    // Scale by dt so trace integrates co-activation over time
                    s->accumulateEligibility(pre, post, dt);
                }
            }

            // Update mitochondrial states and execute region-specific processing
            {
                std::lock_guard<std::mutex> lock(region_mutex_);

                // Mitochondrial update logic
                if (mito_states_.size() != neurons_.size()) {
                    mito_states_.resize(neurons_.size());
                }

                float total_energy = 0.0f;
                float total_health = 0.0f;
                float total_stress = 0.0f;

                bool use_gpu = NeuroForge::CUDAAccel::isAvailable();
                if (use_gpu && neurons_.size() > 1000) { // Threshold for GPU overhead
                    // Gather data for GPU
                    std::vector<float> energy_vec(neurons_.size());
                    std::vector<float> health_vec(neurons_.size());
                    std::vector<float> activation_vec(neurons_.size());
                    
                    for (size_t i = 0; i < neurons_.size(); ++i) {
                        energy_vec[i] = mito_states_[i].energy;
                        health_vec[i] = mito_states_[i].health;
                        activation_vec[i] = neurons_[i] ? neurons_[i]->getActivation() : 0.0f;
                    }

                    // Assume uniform parameters for now, or use the first one as representative
                    float production_rate = mito_states_.empty() ? 0.002f : mito_states_[0].production_rate;
                    float base_consumption = mito_states_.empty() ? 0.0001f : mito_states_[0].base_consumption;

                    if (NeuroForge::CUDAAccel::mitochondrialUpdate(energy_vec.data(), health_vec.data(), activation_vec.data(),
                                                                  static_cast<int>(neurons_.size()), production_rate, base_consumption)) {
                        // Scatter back results
                        for (size_t i = 0; i < neurons_.size(); ++i) {
                            mito_states_[i].energy = energy_vec[i];
                            mito_states_[i].health = health_vec[i];
                            
                            total_energy += mito_states_[i].energy;
                            total_health += mito_states_[i].health;
                            if (mito_states_[i].energy < 0.3f) {
                                total_stress += 1.0f;
                            }

                            if (neurons_[i]) {
                                neurons_[i]->setEnergy(mito_states_[i].energy);
                                neurons_[i]->setMitoHealth(mito_states_[i].health);
                            }
                        }
                    } else {
                        use_gpu = false; // Fallback if kernel failed
                    }
                }

                if (!use_gpu) {
                    for (size_t i = 0; i < neurons_.size(); ++i) {
                        auto& neuron = neurons_[i];
                        auto& mito = mito_states_[i];
                        
                        if (!neuron) continue;

                        float activity = neuron->getActivation();
                        bool spiking = (activity > 0.8f);

                        // Production: Base + Activity-dependent boost
                        float production = mito.production_rate * (0.7f + 0.3f * activity);
                        
                        // Consumption: Base + Spike cost + Maintenance
                        float consumption = mito.base_consumption;
                        if (spiking) consumption += 0.012f;
                        else if (activity > 0.1f) consumption += 0.002f;
                        
                        // Update energy
                        mito.energy = std::clamp(mito.energy + production - consumption, 0.0f, 1.0f);

                        // Health dynamics (slow variable)
                        if (mito.energy < 0.3f) {
                            // Chronic overload
                            mito.health = std::clamp(mito.health - 0.00001f, 0.1f, 1.0f);
                            total_stress += 1.0f;
                        } else if (mito.energy > 0.7f) {
                            // Recovery
                            mito.health = std::clamp(mito.health + 0.00002f, 0.1f, 1.0f);
                        }
                        
                        total_energy += mito.energy;
                        total_health += mito.health;

                        // Push to Neuron for external access (e.g. LearningSystem)
                        neuron->setEnergy(mito.energy);
                        neuron->setMitoHealth(mito.health);
                    }
                }

                if (!neurons_.empty()) {
                    stats_.avg_mitochondrial_energy = total_energy / neurons_.size();
                    stats_.avg_mitochondrial_health = total_health / neurons_.size();
                    stats_.metabolic_stress = total_stress;
                }

                processRegionSpecific(delta_time);
            }

            // Execute custom processor if set
            if (custom_processor_) {
                custom_processor_(*this, delta_time);
            }

            // Update global activation (with mutex protection)
            {
                std::lock_guard<std::mutex> lock(region_mutex_);
                global_activation_.store(calculateGlobalActivation(), std::memory_order_relaxed);
                // Update statistics
                updateStatistics();
            }

            // Record processing time
            auto end_time = std::chrono::steady_clock::now();
            stats_.processing_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            processing_cycles_.fetch_add(1, std::memory_order_relaxed);
        }

        void Region::initialize() {
            std::lock_guard<std::mutex> lock(region_mutex_);
            
            // Initialize all neurons
            for (auto& neuron : neurons_) {
                if (neuron) {
                    neuron->reset();
                }
            }

            // Reset statistics
            stats_ = Statistics{};
            processing_cycles_.store(0, std::memory_order_relaxed);
            global_activation_.store(0.0f, std::memory_order_relaxed);
            is_active_.store(true, std::memory_order_relaxed);
        }

        void Region::reset() {
            std::lock_guard<std::mutex> lock(region_mutex_);
            
            // Reset all neurons
            for (auto& neuron : neurons_) {
                if (neuron) {
                    neuron->reset();
                }
            }

            // Reset all synapses
            for (auto& synapse : internal_synapses_) {
                if (synapse) {
                    synapse->reset();
                }
            }

            // Reset state
            global_activation_.store(0.0f, std::memory_order_relaxed);
            processing_cycles_.store(0, std::memory_order_relaxed);
            stats_ = Statistics{};
        }

        Region::Statistics Region::getStatistics() const {
            std::lock_guard<std::mutex> lock(region_mutex_);
            
            Statistics current_stats = stats_;
            current_stats.neuron_count = neurons_.size();
            
            // Count all synapses: internal + input + output connections
            std::size_t total_synapses = internal_synapses_.size();
            for (const auto& [neuron_id, synapses] : input_connections_) {
                total_synapses += synapses.size();
            }
            for (const auto& [neuron_id, synapses] : output_connections_) {
                total_synapses += synapses.size();
            }
            current_stats.synapse_count = total_synapses;
            
            current_stats.memory_usage = getMemoryUsage();
            
            return current_stats;
        }

        std::size_t Region::getMemoryUsage() const {
            std::size_t total_size = sizeof(Region);
            
            // Add neuron container memory
            total_size += neurons_.capacity() * sizeof(NeuronPtr);
            
            // Add synapse container memory
            total_size += internal_synapses_.capacity() * sizeof(SynapsePtr);
            
            // Add connection map memory (approximate)
            total_size += input_connections_.size() * (sizeof(NeuronID) + sizeof(std::vector<SynapsePtr>));
            total_size += output_connections_.size() * (sizeof(NeuronID) + sizeof(std::vector<SynapsePtr>));
            
            // Add individual neuron and synapse memory
            for (const auto& neuron : neurons_) {
                if (neuron) {
                    total_size += neuron->getMemoryUsage();
                }
            }
            
            for (const auto& synapse : internal_synapses_) {
                if (synapse) {
                    total_size += synapse->getMemoryUsage();
                }
            }
            
            return total_size;
        }

        SynapsePtr Region::getSynapse(SynapseID synapse_id) const {
            std::lock_guard<std::mutex> lock(region_mutex_);
            
            // Search in internal synapses
            for (const auto& synapse : internal_synapses_) {
                if (synapse && synapse->getId() == synapse_id) {
                    return synapse;
                }
            }
            
            // Search in input connections
            for (const auto& [neuron_id, synapses] : input_connections_) {
                for (const auto& synapse : synapses) {
                    if (synapse && synapse->getId() == synapse_id) {
                        return synapse;
                    }
                }
            }
            
            // Search in output connections
            for (const auto& [neuron_id, synapses] : output_connections_) {
                for (const auto& synapse : synapses) {
                    if (synapse && synapse->getId() == synapse_id) {
                        return synapse;
                    }
                }
            }
            
            // Search in inter-region connections
            for (const auto& [region_id, synapses] : inter_region_connections_) {
                for (const auto& synapse : synapses) {
                    if (synapse && synapse->getId() == synapse_id) {
                        return synapse;
                    }
                }
            }
            
            return nullptr; // Synapse not found
        }

        std::string Region::getTypeString() const {
            switch (type_) {
                case Type::Cortical: return "Cortical";
                case Type::Subcortical: return "Subcortical";
                case Type::Brainstem: return "Brainstem";
                case Type::Special: return "Special";
                case Type::Custom: return "Custom";
                default: return "Unknown";
            }
        }

        std::string Region::getActivationPatternString() const {
            switch (activation_pattern_) {
                case ActivationPattern::Synchronous: return "Synchronous";
                case ActivationPattern::Asynchronous: return "Asynchronous";
                case ActivationPattern::Layered: return "Layered";
                case ActivationPattern::Competitive: return "Competitive";
                case ActivationPattern::Oscillatory: return "Oscillatory";
                default: return "Unknown";
            }
        }

        void Region::updateStatistics() const {
            std::size_t active_count = 0;
            float total_activation = 0.0f;
            float total_energy = 0.0f;
            const float activation_threshold = 0.2f; // Threshold for considering a neuron "active"

            // Activation stats
            for (const auto& neuron : neurons_) {
                if (neuron) {
                    float activation = neuron->getActivation();
                    total_activation += activation;
                    
                    // Count neurons as active if their activation exceeds threshold
                    if (activation > activation_threshold) {
                        active_count++;
                    }
                    
                    // Simple energy calculation (activation^2)
                    total_energy += activation * activation;
                }
            }

            // Mitochondrial stats
            float total_mito_energy = 0.0f;
            float total_mito_health = 0.0f;
            float total_metabolic_stress = 0.0f; // Derived metric

            for (const auto& state : mito_states_) {
                total_mito_energy += state.energy;
                total_mito_health += state.health;
                
                // Simple stress metric: low energy + low health contribution
                // Stress increases as energy drops below 0.3 or health drops below 0.5
                float energy_stress = (state.energy < 0.3f) ? (0.3f - state.energy) / 0.3f : 0.0f;
                float health_stress = (state.health < 0.5f) ? (0.5f - state.health) / 0.5f : 0.0f;
                total_metabolic_stress += (energy_stress + health_stress);
            }

            stats_.active_neurons = active_count;
            stats_.average_activation = neurons_.empty() ? 0.0f : total_activation / neurons_.size();
            stats_.total_energy = total_energy;
            
            size_t m_count = mito_states_.size();
            stats_.avg_mitochondrial_energy = m_count > 0 ? total_mito_energy / m_count : 0.0f;
            stats_.avg_mitochondrial_health = m_count > 0 ? total_mito_health / m_count : 0.0f;
            stats_.metabolic_stress = m_count > 0 ? total_metabolic_stress / m_count : 0.0f;
        }

        void Region::processNeurons(float delta_time) {
            if (neurons_.empty()) {
                return;
            }
            processNeuronsFromCopy(neurons_, delta_time);
        }

        void Region::processNeuronsFromCopy(const std::vector<NeuronPtr>& neurons_copy, float delta_time) {
            if (neurons_copy.empty()) {
                return;
            }

            const std::uint64_t cycle = processing_cycles_.load(std::memory_order_relaxed);
            const std::uint64_t region_salt = (static_cast<std::uint64_t>(id_) << 32) ^ (cycle + 0x9E3779B97F4A7C15ULL);

            auto mix64 = [](std::uint64_t x) {
                x += 0x9E3779B97F4A7C15ULL;
                x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
                x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
                return x ^ (x >> 31);
            };

            auto u01 = [&mix64](std::uint64_t x) {
                const std::uint64_t m = mix64(x);
                return static_cast<float>((m >> 11) * (1.0 / 9007199254740992.0));
            };

            switch (activation_pattern_) {
                case ActivationPattern::Synchronous:
                    for (const auto& neuron : neurons_copy) {
                        if (neuron) {
                            neuron->process(delta_time);
                            const std::uint64_t nid = static_cast<std::uint64_t>(neuron->getId());
                            if (u01(region_salt ^ nid) < 0.02f) {
                                float boost = 0.3f + u01(region_salt ^ (nid + 0xD1B54A32D192ED03ULL)) * 0.4f;
                                neuron->setActivation(std::min(1.0f, neuron->getActivation() + boost));
                            }
                        }
                    }
                    break;

                case ActivationPattern::Asynchronous:
                    for (const auto& neuron : neurons_copy) {
                        if (neuron) {
                            neuron->process(delta_time);
                            const std::uint64_t nid = static_cast<std::uint64_t>(neuron->getId());
                            float noise = (u01(region_salt ^ (nid + 0xA24BAED4963EE407ULL)) - 0.5f) * 0.1f;
                            float current = neuron->getActivation();
                            neuron->setActivation(std::clamp(current + noise, 0.0f, 1.0f));
                            if (u01(region_salt ^ (nid + 0x9FB21C651E98DF25ULL)) < 0.015f) {
                                neuron->setActivation(std::min(1.0f, current + 0.4f));
                            }
                        }
                    }
                    break;

                case ActivationPattern::Layered:
                    // Process in layers with wave-like activity using SIMD for wave calculations
                    processLayeredWithSIMD(neurons_copy, delta_time);
                    break;

                case ActivationPattern::Competitive: {
                    // Winner-take-all with enhanced activity
                    auto max_neuron = std::max_element(neurons_copy.begin(), neurons_copy.end(),
                        [](const NeuronPtr& a, const NeuronPtr& b) {
                            if (!a) return true;
                            if (!b) return false;
                            return a->getActivation() < b->getActivation();
                        });
                    
                    for (const auto& neuron : neurons_copy) {
                        if (neuron) {
                            neuron->process(delta_time);
                            if (neuron == *max_neuron) {
                                float boost = 0.1f * delta_time;
                                neuron->setActivation(std::min(1.0f, neuron->getActivation() + boost));
                            } else {
                                float current = neuron->getActivation();
                                neuron->setActivation(std::max(0.1f, current * 0.95f));
                                const std::uint64_t nid = static_cast<std::uint64_t>(neuron->getId());
                                if (u01(region_salt ^ (nid + 0x94D049BB133111EBULL)) < 0.01f) {
                                    neuron->setActivation(current + 0.3f);
                                }
                            }
                        }
                    }
                    break;
                }

                case ActivationPattern::Oscillatory:
                    // Enhanced oscillatory pattern with SIMD optimization
                    processOscillatoryWithSIMD(neurons_copy, delta_time);
                    break;
            }
        }

        void Region::processLayeredWithSIMD(const NeuronContainer& neurons_copy, float delta_time) {
            const size_t batch_size = 8; // Process 8 neurons at a time for SIMD
            static thread_local float global_time = 0.0f;
            global_time += delta_time;
            
            // Process neurons in batches for SIMD optimization
            for (size_t i = 0; i < neurons_copy.size(); i += batch_size) {
                const size_t end_idx = std::min(i + batch_size, neurons_copy.size());
                
                // Declare wave_values array outside conditional blocks
                alignas(32) float wave_values[8];
                
#ifdef __AVX2__
                // AVX2 implementation for 8 floats at once
                alignas(32) float indices[8];
                
                for (size_t j = 0; j < 8; ++j) {
                    indices[j] = static_cast<float>(i + j);
                }
                
                __m256 global_time_vec = _mm256_set1_ps(global_time * 5.0f);
                __m256 indices_vec = _mm256_load_ps(indices);
                __m256 offset_vec = _mm256_mul_ps(indices_vec, _mm256_set1_ps(0.1f));
                __m256 phase_vec = _mm256_add_ps(global_time_vec, offset_vec);
                
                // Calculate sin values (approximation for performance)
                __m256 sin_approx = _mm256_sub_ps(phase_vec, _mm256_mul_ps(_mm256_set1_ps(2.0f * 3.14159f), 
                                                 _mm256_floor_ps(_mm256_div_ps(phase_vec, _mm256_set1_ps(2.0f * 3.14159f)))));
                __m256 wave_vec = _mm256_add_ps(_mm256_mul_ps(sin_approx, _mm256_set1_ps(0.2f)), _mm256_set1_ps(0.2f));
                
                _mm256_store_ps(wave_values, wave_vec);
#elif defined(__SSE2__)
                // SSE2 implementation for 4 floats at once
                for (size_t batch = 0; batch < 2; ++batch) {
                    alignas(16) float indices[4];
                    for (size_t j = 0; j < 4; ++j) {
                        indices[j] = static_cast<float>(i + batch * 4 + j);
                    }
                    
                    __m128 global_time_vec = _mm_set1_ps(global_time * 5.0f);
                    __m128 indices_vec = _mm_load_ps(indices);
                    __m128 offset_vec = _mm_mul_ps(indices_vec, _mm_set1_ps(0.1f));
                    __m128 phase_vec = _mm_add_ps(global_time_vec, offset_vec);
                    
                    // Simple sin approximation for performance
                    __m128 sin_approx = _mm_sub_ps(phase_vec, _mm_mul_ps(_mm_set1_ps(2.0f * 3.14159f), 
                                                  _mm_cvtepi32_ps(_mm_cvttps_epi32(_mm_div_ps(phase_vec, _mm_set1_ps(2.0f * 3.14159f))))));
                    __m128 wave_vec = _mm_add_ps(_mm_mul_ps(sin_approx, _mm_set1_ps(0.2f)), _mm_set1_ps(0.2f));
                    
                    _mm_store_ps(&wave_values[batch * 4], wave_vec);
                }
#else
                // Fallback scalar implementation
                for (size_t j = 0; j < 8; ++j) {
                    float wave = std::sin(global_time * 5.0f + (i + j) * 0.1f) * 0.2f + 0.2f;
                    wave_values[j] = wave;
                }
#endif
                
                // Apply wave values to neurons
                for (size_t j = 0; j < end_idx - i; ++j) {
                    auto& neuron = neurons_copy[i + j];
                    if (neuron) {
                        neuron->process(delta_time);
                        float current = neuron->getActivation();
                        neuron->setActivation(std::clamp(current + wave_values[j] * delta_time, 0.0f, 1.0f));
                    }
                }
            }
        }

        void Region::processOscillatoryWithSIMD(const NeuronContainer& neurons_copy, float delta_time) {
            static thread_local float phase1 = 0.0f;
            static thread_local float phase2 = 0.0f;
            static thread_local float phase3 = 0.0f;
            phase1 += delta_time * 8.0f;  // 8 Hz alpha
            phase2 += delta_time * 15.0f; // 15 Hz beta
            phase3 += delta_time * 40.0f; // 40 Hz gamma
            
            float alpha_wave = std::sin(phase1) * 0.3f;
            float beta_wave = std::sin(phase2) * 0.2f;
            float gamma_wave = std::sin(phase3) * 0.1f;
            
            const size_t batch_size = 8;
            
            for (size_t i = 0; i < neurons_copy.size(); i += batch_size) {
                const size_t end_idx = std::min(i + batch_size, neurons_copy.size());
                
#ifdef __AVX2__
                // AVX2 implementation for modulation calculation
                alignas(32) float modulations[8];
                __m256 base_mod = _mm256_set1_ps(0.5f);
                __m256 alpha_vec = _mm256_set1_ps(alpha_wave);
                __m256 beta_vec = _mm256_set1_ps(beta_wave);
                __m256 gamma_vec = _mm256_set1_ps(gamma_wave);
                
                for (size_t j = 0; j < 8; ++j) {
                    size_t idx = i + j;
                    float mod_val = 0.5f;
                    if (idx % 3 == 0) mod_val += alpha_wave;
                    else if (idx % 3 == 1) mod_val += beta_wave;
                    else mod_val += gamma_wave;
                    modulations[j] = mod_val;
                }
#else
                // Scalar fallback
                float modulations[8];
                for (size_t j = 0; j < 8; ++j) {
                    size_t idx = i + j;
                    float mod_val = 0.5f;
                    if (idx % 3 == 0) mod_val += alpha_wave;
                    else if (idx % 3 == 1) mod_val += beta_wave;
                    else mod_val += gamma_wave;
                    modulations[j] = mod_val;
                }
#endif
                
                // Apply modulations to neurons
                for (size_t j = 0; j < end_idx - i; ++j) {
                    auto& neuron = neurons_copy[i + j];
                    if (neuron) {
                        neuron->process(delta_time);
                        float current_activation = neuron->getActivation();
                        neuron->setActivation(std::clamp(current_activation * modulations[j] + 0.1f, 0.0f, 1.0f));
                    }
                }
            }
        }

        float Region::calculateGlobalActivation() const {
            if (neurons_.empty()) {
                return 0.0f;
            }

            float total_activation = 0.0f;
            for (const auto& neuron : neurons_) {
                if (neuron) {
                    total_activation += neuron->getActivation();
                }
            }

            return total_activation / neurons_.size();
        }

        // Substrate hooks implementations
        void Region::feedExternalPattern(const std::vector<float>& pattern) {
            std::lock_guard<std::mutex> lock(region_mutex_);
            const std::size_t n = neurons_.size();
            const std::size_t m = std::min(n, pattern.size());
            for (std::size_t i = 0; i < m; ++i) {
                if (neurons_[i]) {
                    const float v = std::clamp(pattern[i], 0.0f, 1.0f);
                    neurons_[i]->setActivation(v);
                }
            }
        }
        
        void Region::readoutVector(std::vector<float>& out) const {
            std::lock_guard<std::mutex> lock(region_mutex_);
            out.resize(neurons_.size());
            for (std::size_t i = 0; i < neurons_.size(); ++i) {
                out[i] = neurons_[i] ? neurons_[i]->getActivation() : 0.0f;
            }
        }
        
        void Region::applyNeuromodulator(float level) {
            std::lock_guard<std::mutex> lock(region_mutex_);
            const float clamped = std::clamp(level, -1.0f, 1.0f);
            const float scale = 0.05f; // gentle bias factor
            for (auto& neuron : neurons_) {
                if (neuron) {
                    float a = neuron->getActivation();
                    a = std::clamp(a + clamped * scale, 0.0f, 1.0f);
                    neuron->setActivation(a);
                }
            }
            // Apply R-STDP-lite weight updates: Δw = lr * r * e_ij
            // Build a snapshot of unique synapses connected to this region
            std::vector<SynapsePtr> synapses_snapshot;
            {
                std::lock_guard<std::mutex> conn_lock(connection_mutex_);
                synapses_snapshot.reserve(internal_synapses_.size());
                synapses_snapshot.insert(synapses_snapshot.end(), internal_synapses_.begin(), internal_synapses_.end());
                for (const auto &kv : input_connections_) {
                    synapses_snapshot.insert(synapses_snapshot.end(), kv.second.begin(), kv.second.end());
                }
                for (const auto &kv : output_connections_) {
                    synapses_snapshot.insert(synapses_snapshot.end(), kv.second.begin(), kv.second.end());
                }
                for (const auto &kv : inter_region_connections_) {
                    const auto &vec = kv.second;
                    synapses_snapshot.insert(synapses_snapshot.end(), vec.begin(), vec.end());
                }
            }
            std::unordered_set<Synapse*> seen;
            seen.reserve(synapses_snapshot.size() * 2);
            for (auto &s : synapses_snapshot) {
                if (!s) continue;
                Synapse* raw = s.get();
                if (!seen.insert(raw).second) continue; // skip duplicates
                if (s->getType() == SynapseType::Modulatory) continue; // no weight update for modulatory synapses
                const float e = s->getEligibility();
                if (e == 0.0f || clamped == 0.0f) continue;
                const float lr = s->getLearningRate();
                const float delta_w = lr * clamped * e;
                const float new_w = s->getWeight() + delta_w;
                s->setWeight(new_w); // setWeight clamps to bounds and updates stats
            }
        }

        // RegionFactory Implementation
        RegionPtr RegionFactory::createRegion(const std::string& name,
                                             Region::Type type,
                                             Region::ActivationPattern pattern) {
            auto id = next_id_.fetch_add(1, std::memory_order_relaxed);
            return std::make_shared<Region>(id, name, type, pattern);
        }

        RegionPtr RegionFactory::createRegion(RegionID id,
                                             const std::string& name,
                                             Region::Type type,
                                             Region::ActivationPattern pattern) {
            // Update next_id if necessary to avoid conflicts
            auto current_next = next_id_.load(std::memory_order_relaxed);
            while (id >= current_next) {
                next_id_.compare_exchange_weak(current_next, id + 1, std::memory_order_relaxed);
                current_next = next_id_.load(std::memory_order_relaxed);
            }
            
            return std::make_shared<Region>(id, name, type, pattern);
        }

        RegionID RegionFactory::getNextId() {
            return next_id_.fetch_add(1, std::memory_order_relaxed);
        }

        void RegionFactory::resetIdCounter() {
            next_id_.store(1, std::memory_order_relaxed);
        }

        void Region::reserveInterRegionConnections(RegionID target_region_id, std::size_t additional) {
            if (additional == 0) return;
            std::lock_guard<std::mutex> lock(connection_mutex_);
            auto &vec = inter_region_connections_[target_region_id];
            const std::size_t needed = vec.size() + additional;
            if (vec.capacity() < needed) {
                vec.reserve(needed);
            }
        }

        void Region::reserveInputConnections(NeuronID target_neuron_id, std::size_t additional) {
            if (additional == 0) return;
            std::lock_guard<std::mutex> lock(connection_mutex_);
            auto &vec = input_connections_[target_neuron_id];
            const std::size_t needed = vec.size() + additional;
            if (vec.capacity() < needed) {
                vec.reserve(needed);
            }
        }

        void Region::reserveOutputConnections(NeuronID source_neuron_id, std::size_t additional) {
            if (additional == 0) return;
            std::lock_guard<std::mutex> lock(connection_mutex_);
            auto &vec = output_connections_[source_neuron_id];
            const std::size_t needed = vec.size() + additional;
            if (vec.capacity() < needed) {
                vec.reserve(needed);
            }
        }

        // Explicit-ID overloads for connectNeurons and connectToRegion
        SynapsePtr Region::connectNeurons(NeuronID source_id, 
                                         NeuronID target_id,
                                         Weight weight,
                                         SynapseType type,
                                         SynapseID explicit_id) {
            if (source_id == target_id) {
                return nullptr; // no self-connection for explicit overload as well
            }

            // Validate neurons exist and belong to this region
            auto source_neuron = getNeuron(source_id);
            auto target_neuron = getNeuron(target_id);
            if (!source_neuron || !target_neuron) {
                return nullptr;
            }

            // Create synapse with explicit ID
            auto synapse = SynapseFactory::createSynapse(explicit_id, source_neuron, target_neuron, weight, type);
            if (!synapse) {
                return nullptr;
            }

            // Add to region structures (also updates input/output connection maps)
            if (!addInternalSynapse(synapse)) {
                return nullptr;
            }

            // Update neuron local connection lists (mirror non-explicit overload behavior)
            source_neuron->reserveOutputSynapses(source_neuron->getOutputSynapseCount() + 1);
            target_neuron->reserveInputSynapses(target_neuron->getInputSynapseCount() + 1);
            source_neuron->addOutputSynapse(synapse);
            target_neuron->addInputSynapse(synapse);

            return synapse;
        }

        SynapsePtr Region::connectToRegion(RegionPtr target_region,
                                          NeuronID source_neuron_id,
                                          NeuronID target_neuron_id,
                                          Weight weight,
                                          SynapseType type,
                                          SynapseID explicit_id) {
            if (!target_region) {
                return nullptr;
            }

            if (target_region.get() == this) {
                // Delegate to internal connection if same region
                return connectNeurons(source_neuron_id, target_neuron_id, weight, type, explicit_id);
            }

            // Validate local source neuron and remote target neuron
            auto source_neuron = getNeuron(source_neuron_id);
            auto target_neuron = target_region->getNeuron(target_neuron_id);
            if (!source_neuron || !target_neuron) {
                return nullptr;
            }

            // Check if connection already exists between these neurons across regions
            {
                std::lock_guard<std::mutex> lock(connection_mutex_);
                auto& connections = inter_region_connections_[target_region->getId()];
                auto it = std::find_if(connections.begin(), connections.end(), [&](const SynapsePtr& s) {
                    auto source = s->getSource().lock();
                    auto target = s->getTarget().lock();
                    return source && target && 
                           source->getId() == source_neuron_id && 
                           target->getId() == target_neuron_id;
                });
                if (it != connections.end()) {
                    return *it; // Reuse existing
                }
            }

            // Create synapse with explicit ID
            auto synapse = SynapseFactory::createSynapse(explicit_id, source_neuron, target_neuron, weight, type);
            if (!synapse) {
                return nullptr;
            }

            // Add synapse to neurons FIRST to avoid deadlock (mirror non-explicit overload)
            source_neuron->reserveOutputSynapses(source_neuron->getOutputSynapseCount() + 1);
            target_neuron->reserveInputSynapses(target_neuron->getInputSynapseCount() + 1);
            source_neuron->addOutputSynapse(synapse);
            target_neuron->addInputSynapse(synapse);

            // Reserve capacities for bookkeeping
            reserveOutputConnections(source_neuron_id, 1);
            target_region->reserveInputConnections(target_neuron_id, 1);
            reserveInterRegionConnections(target_region->getId(), 1);

            // Thread-safe updates for both regions
            {
                std::lock_guard<std::mutex> lock_this(connection_mutex_);
                output_connections_[source_neuron_id].push_back(synapse);
                inter_region_connections_[target_region->getId()].push_back(synapse);
            }

            {
                // Update target region's input connections
                std::lock_guard<std::mutex> lock_target(target_region->connection_mutex_);
                target_region->input_connections_[target_neuron_id].push_back(synapse);
            }

            return synapse;
        }

    } // namespace Core
} // namespace NeuroForge
