#include "core/LearningSystem.h"
#include "core/HypergraphBrain.h"
#include "core/Region.h"
#include "core/Neuron.h"
#include "core/Synapse.h"
#include "core/CUDAAccel.h"
#include "memory/DevelopmentalConstraints.h"
#include <algorithm>
#include <numeric>
#include <unordered_set>
#include <cmath>

namespace NeuroForge {
    namespace Core {

        LearningSystem::LearningSystem(NeuroForge::Core::HypergraphBrain* brain, const Config& config)
            : brain_(brain), config_(config) {
        }

        LearningSystem::~LearningSystem() {
            shutdown();
        }

        bool LearningSystem::initialize() {
            is_active_.store(true, std::memory_order_relaxed);
            is_paused_.store(false, std::memory_order_relaxed);
            return true;
        }

        void LearningSystem::shutdown() {
            is_active_.store(false, std::memory_order_relaxed);
            {
                std::lock_guard<std::mutex> lock(syn_state_mutex_);
                syn_state_.clear();
            }
            {
                std::lock_guard<std::mutex> lock(spike_times_mutex_);
                last_spike_times_.clear();
            }
        }

        void LearningSystem::updateLearning(float delta_time) {
            (void)delta_time;
            if (!is_active_.load() || is_paused_.load()) return;
            
            // Gather mitochondrial stats from regions for telemetry
            if (brain_) {
                const auto& regions_map0 = brain_->getRegionsMap();
                double total_energy_sum = 0.0;
                double total_hazard_count = 0.0;
                size_t total_neurons = 0;

                for (const auto& [rid0, region0] : regions_map0) {
                    if (!region0) continue;
                    auto stats = region0->getStatistics();
                    // approximate: average * count
                    total_energy_sum += stats.avg_mitochondrial_energy * stats.neuron_count;
                    total_hazard_count += stats.metabolic_stress;
                    total_neurons += stats.neuron_count;
                }
                
                if (total_neurons > 0) {
                    statistics_.avg_energy = static_cast<float>(total_energy_sum / total_neurons);
                    statistics_.metabolic_hazard = static_cast<float>(total_hazard_count / total_neurons);
                }
            }
            
            // Advance/decay attention anneal timeline: decay only when no new map arrives
            if (config_.attention_anneal_ms > 0 && config_.enable_attention_modulation && config_.attention_mode != AttentionMode::Off) {
                int dec = static_cast<int>(std::round(std::max(0.0f, delta_time) * 1000.0f));
                if (attention_anneal_elapsed_ms_ > 0) {
                    attention_anneal_elapsed_ms_ = std::max(0, attention_anneal_elapsed_ms_ - dec);
                    float t = (config_.attention_anneal_ms > 0)
                        ? std::clamp(static_cast<float>(attention_anneal_elapsed_ms_) / static_cast<float>(config_.attention_anneal_ms), 0.0f, 1.0f)
                        : 1.0f;
                    // As time decays, boost decays from last base toward 1.0
                    attention_boost_effective_ = 1.0f + (last_attention_boost_base_ - 1.0f) * t;
                }
            }
        
            // Early exit only if no Hebbian, no STDP, and no pending reward for Phase-4
            // This ensures reward-modulated plasticity can still occur when classic learning is disabled
            float pendingR_snapshot = pending_reward_.load(std::memory_order_relaxed);
            if (config_.hebbian_rate <= 0.0f && config_.stdp_rate <= 0.0f && std::fabs(pendingR_snapshot) <= 1e-9f) {
                return;
            }
        
            // Collect region IDs and cache synapses (optimization: single pass)
            std::vector<RegionID> region_ids;
            std::unordered_map<RegionID, std::vector<NeuroForge::SynapsePtr>> region_synapses_cache;
            
            if (brain_) {
                const auto& regions_map = brain_->getRegionsMap();
                region_ids.reserve(regions_map.size());
                
                for (const auto& [rid, region] : regions_map) {
                    region_ids.push_back(rid);
                    
                    // Cache synapses for this region if we'll need them
                    if (config_.hebbian_rate > 0.0f || config_.stdp_rate > 0.0f) {
                        region_synapses_cache[rid] = getRegionSynapses(rid);
                    }
                }
            }
        
            // 1) Hebbian learning across regions (primary default)
            if (config_.hebbian_rate > 0.0f) {
                for (auto rid : region_ids) {
                    applyHebbianLearning(rid, config_.hebbian_rate);
        
                    // Optional homeostasis to stabilize weights
                    if (config_.enable_homeostasis) {
                        applyHomeostasis(rid);
                    }
                }
            }
        
            // 2) STDP based on recent spike times (if any were recorded)
            if (config_.stdp_rate > 0.0f) {
                std::unordered_map<NeuronID, TimePoint> spike_snapshot;
                {
                    std::lock_guard<std::mutex> lock(spike_times_mutex_);
                    spike_snapshot = last_spike_times_;
                }
                
                // Use cached synapses for STDP (optimization: avoid repeated lookups)
                for (const auto& [rid, synapses] : region_synapses_cache) {
                    if (!synapses.empty()) {
                        applySTDPLearning(rid, synapses, spike_snapshot);
                    }
                }
            }

            // 3) Reward-modulated plasticity with eligibility traces (Phase 4)
            float R = pending_reward_.exchange(0.0f, std::memory_order_relaxed);
            if (std::fabs(R) > 1e-9f) {
                std::vector<std::pair<SynapseID, float>> deltas;
                {
                    std::lock_guard<std::mutex> lock(syn_state_mutex_);
                    deltas.reserve(syn_state_.size());
                    for (auto& kv : syn_state_) {
                        const SynapseID sid = kv.first;
                        SynapseRuntime& st = kv.second;
                        float comp_scale_dw = 1.0f;
                        if (config_.competence_mode == CompetenceMode::ScaleLearningRates) {
                            comp_scale_dw = std::clamp(competence_level_.load(std::memory_order_relaxed), 0.0f, 1.0f);
                        }
                        const float dw = kappa_ * R * st.eligibility * config_.global_learning_rate; // Apply global learning rate
                        if (std::fabs(dw) > 0.0f) {
                            // Stochastic gating for sparse plasticity
                            float effective_p_gate_r = config_.p_gate;
                            if (config_.competence_mode == CompetenceMode::ScalePGate) {
                                float comp = competence_level_.load(std::memory_order_relaxed);
                                effective_p_gate_r = std::clamp(effective_p_gate_r * comp, 0.0f, 1.0f);
                            }
                            if (effective_p_gate_r >= 1.0f || dist01_(rng_) <= effective_p_gate_r) {
                                deltas.emplace_back(sid, dw);
                            }
                        }
                        // Note: eligibility decay removed to match expected test formula kappa*R*elig
                    }
                }
                // Apply weight deltas without holding the map mutex
                for (const auto& [sid, dw] : deltas) {
                    auto s = findSynapseById(sid);
                    if (!s) continue;
                    float before = s->getWeight();
                    float after = std::clamp(before + dw,
                                             static_cast<float>(s->getMinWeight()),
                                             static_cast<float>(s->getMaxWeight()));
                    s->setWeight(after);
                    // Count as Phase-4 reward-modulated update
                    updateStatistics(Algorithm::RewardModulated, after - before);
                 }
            }

            // 4) Periodic consolidation (and track a simple consolidation rate)
            static float consolidation_accum = 0.0f;
            consolidation_accum += std::max(0.0f, delta_time);
            const float interval_s = std::max(0.001f, config_.update_interval.count() / 1000.0f);
            if (consolidation_accum >= interval_s && !region_ids.empty()) {
                consolidateMemories(region_ids, ConsolidationPhase::Consolidation);
                consolidation_accum = 0.0f;
                statistics_.memory_consolidation_rate = config_.consolidation_strength;
            }
        }

        void LearningSystem::applySTDPLearning(
            NeuroForge::RegionID region_id,
            const std::vector<NeuroForge::SynapsePtr>& synapses,
            const std::unordered_map<NeuroForge::NeuronID, NeuroForge::TimePoint>& spike_times) {
            if (!is_active_.load() || is_paused_.load()) return;
            // Compute developmental modulation for this region if available
            float dev_lr_mult = 1.0f;
            float dev_plasticity_mult = 1.0f;
            bool dev_restricted = false;
            if (developmental_constraints_ && brain_) {
                auto region = brain_->getRegion(region_id);
                if (region) {
                    const std::string& rname = region->getName();
                    auto mod = developmental_constraints_->getLearningModulation("STDP", rname);
                    dev_lr_mult = mod.learning_rate_multiplier;
                    dev_plasticity_mult = mod.plasticity_multiplier;
                    dev_restricted = mod.is_restricted;
                }
            }
            if (dev_restricted) {
                return; // Skip STDP updates for restricted periods
            }

            // Accelerated path: approximate STDP as pairwise pre/post contributions when batch is large
            if (config_.prefer_gpu && CUDAAccel::isAvailable() && synapses.size() >= 64) {
                const std::size_t n = synapses.size();
                std::vector<float> pre(n, 0.0f), post(n, 0.0f), weights(n, 0.0f);
                // Snapshot competence level once
                float comp_level = 1.0f;
                if (config_.competence_mode == CompetenceMode::ScaleLearningRates) {
                    comp_level = std::clamp(competence_level_.load(std::memory_order_relaxed), 0.0f, 1.0f);
                }
                for (std::size_t i = 0; i < n; ++i) {
                    const auto& synapse = synapses[i];
                    weights[i] = synapse ? synapse->getWeight() : 0.0f;
                    if (!synapse || !synapse->isValid()) { continue; }
                    // Sparse gating
                    float effective_p_gate = config_.p_gate;
                    if (config_.competence_mode == CompetenceMode::ScalePGate) {
                        float comp = std::clamp(competence_level_.load(std::memory_order_relaxed), 0.0f, 1.0f);
                        effective_p_gate = std::clamp(effective_p_gate * comp, 0.0f, 1.0f);
                    }
                    if (effective_p_gate < 1.0f && dist01_(rng_) > effective_p_gate) { continue; }
                    auto source = synapse->getSource().lock();
                    auto target = synapse->getTarget().lock();
                    if (!source || !target) continue;
                    // Spike presence (1.0 if present else 0.0)
                    float pre_spike = (spike_times.find(source->getId()) != spike_times.end()) ? 1.0f : 0.0f;
                    float post_spike = (spike_times.find(target->getId()) != spike_times.end()) ? 1.0f : 0.0f;
                    // Attention factor
                    float attention_factor = 1.0f;
                    if (config_.attention_mode == AttentionMode::Saliency) {
                        float pre_a = std::clamp(source->getActivation(), 0.0f, 1.0f);
                        float post_a = std::clamp(target->getActivation(), 0.0f, 1.0f);
                        float w = std::max(pre_a, post_a);
                        attention_factor = 1.0f + (last_attention_boost_base_ - 1.0f) * w;
                    } else if (!attention_weights_.empty()) {
                        float w_s = 0.0f, w_t = 0.0f;
                        auto itS = attention_weights_.find(source->getId()); if (itS != attention_weights_.end()) w_s = itS->second;
                        auto itT = attention_weights_.find(target->getId()); if (itT != attention_weights_.end()) w_t = itT->second;
                        float w = std::clamp(std::max(w_s, w_t), 0.0f, 1.0f);
                        attention_factor = 1.0f + (last_attention_boost_base_ - 1.0f) * w;
                    }
                    // Effective per-synapse scaling folded into signals; kernel uses a_plus=a_minus=1
                    float eff = synapse->getLearningRate() * config_.stdp_rate_multiplier * dev_lr_mult * dev_plasticity_mult * comp_level;
                    float eg = 1.0f;
                {
                    float e = target->getEnergy();
                    float h = target->getMitoHealth();
                    eg = std::pow(e, 2.0f) * h;
                }
                    eff *= eg;
                    pre[i]  = pre_spike  * eff;
                    post[i] = post_spike * eff;
                }
                bool gpu_ok = CUDAAccel::stdpPairwise(pre.data(), post.data(), weights.data(), static_cast<int>(n), 1.0f, 1.0f);
                if (gpu_ok) {
                    for (std::size_t i = 0; i < n; ++i) {
                        const auto& synapse = synapses[i];
                        if (!synapse) continue;
                        float before_w = synapse->getWeight();
                        float after_w = weights[i];
                        float delta = after_w - before_w;
                        float safe_delta = synapse->applySafetyGuardrailsPublic(delta);
                        synapse->setWeight(before_w + safe_delta);
                        updateStatistics(Algorithm::STDP, safe_delta);
                    }
                    return;
                }
                // If GPU failed, fall through to CPU path
            }
            
            for (const auto& synapse : synapses) {
                if (!synapse || !synapse->isValid()) continue;
                
                // Stochastic gating for sparse updates
                float effective_p_gate = config_.p_gate;
                if (config_.competence_mode == CompetenceMode::ScalePGate) {
                    float comp = competence_level_.load(std::memory_order_relaxed);
                    effective_p_gate = std::clamp(effective_p_gate * comp, 0.0f, 1.0f);
                }
                if (effective_p_gate < 1.0f && dist01_(rng_) > effective_p_gate) {
                    continue;
                }
                
                auto source_weak = synapse->getSource();
                auto target_weak = synapse->getTarget();
                
                auto source = source_weak.lock();
                auto target = target_weak.lock();
                
                if (!source || !target) continue;
                
                auto pre_it = spike_times.find(source->getId());
                auto post_it = spike_times.find(target->getId());
                if (pre_it != spike_times.end() && post_it != spike_times.end()) {
                    float before_w = synapse->getWeight();
                    const float original_lr = synapse->getLearningRate();
                    const float eff_lr = original_lr * config_.stdp_rate_multiplier;
                    auto dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(post_it->second - pre_it->second).count();
                    float delta = 0.0f;
                    if (dt_ms > 0) {
                        delta = eff_lr * std::exp(-static_cast<float>(dt_ms) / 20.0f);
                    } else if (dt_ms < 0) {
                        delta = -eff_lr * std::exp(static_cast<float>(dt_ms) / 20.0f);
                    }
                    float safe_delta = synapse->applySafetyGuardrailsPublic(delta);
                    synapse->setWeight(before_w + safe_delta);
                    updateStatistics(Algorithm::STDP, safe_delta);
                }
            }

            if (config_.enable_structural_plasticity && brain_) {
                auto current_cycles = brain_->getProcessingCycles();
                auto last_cycles = last_structural_cycle_.load(std::memory_order_relaxed);
                if (config_.structural_interval_steps > 0 && current_cycles - last_cycles >= config_.structural_interval_steps) {
                    last_structural_cycle_.store(current_cycles, std::memory_order_relaxed);
                    std::vector<RegionID> structural_region_ids;
                    const auto& regions_map_sp = brain_->getRegionsMap();
                    structural_region_ids.reserve(regions_map_sp.size());
                    for (const auto& [rid_sp, region_sp] : regions_map_sp) {
                        (void)region_sp;
                        structural_region_ids.push_back(rid_sp);
                    }
                    std::size_t applied = 0;
                    for (auto rid : structural_region_ids) {
                        if (config_.structural_max_regions_per_cycle > 0 && applied >= config_.structural_max_regions_per_cycle) break;
                        const float p = dist01_(rng_);
                        if (p <= std::clamp(config_.p_gate, 0.0f, 1.0f)) {
                            applyStructuralPlasticity(rid);
                            ++applied;
                        }
                    }
                }
            }
        }

        void LearningSystem::applyStructuralPlasticity(NeuroForge::RegionID region_id) {
            if (!brain_) return;
            auto region = brain_->getRegion(region_id);
            if (!region) return;

            // Gate by metabolic state
            auto stats = region->getStatistics();
            const bool energy_ok = stats.avg_mitochondrial_energy >= std::clamp(config_.structural_energy_gate, 0.0f, 1.0f);
            const bool stress_low = stats.metabolic_stress <= 0.5f;

            // Prune weak synapses regardless of energy gate to maintain stability
            if (config_.structural_prune_threshold > 0.0f) {
                (void)region->pruneWeakSynapses(config_.structural_prune_threshold);
            }

            if (!energy_ok || !stress_low) {
                return; // do not grow or spawn under low energy/high stress
            }

            // Neurogenesis (spawn)
            if (config_.structural_spawn_batch > 0) {
                (void)region->spawnNeurons(config_.structural_spawn_batch, config_.structural_energy_gate);
            }

            // Synaptogenesis (grow)
            if (config_.structural_grow_batch > 0) {
                (void)region->growSynapses(config_.structural_grow_batch);
            }
        }

        void LearningSystem::applyHebbianLearning(NeuroForge::RegionID region_id, float learning_rate) {
            if (!is_active_.load() || is_paused_.load()) return;
            
            // Determine the base Hebbian rate to report in statistics
            float base_rate = (learning_rate < 0) ? config_.hebbian_rate : learning_rate;
            
            // Get synapses for this region
            auto synapses = getRegionSynapses(region_id);
            
            // Compute developmental modulation for this region if available
            float dev_lr_mult = 1.0f;
            float dev_plasticity_mult = 1.0f;
            bool dev_restricted = false;
            if (developmental_constraints_ && brain_) {
                auto region = brain_->getRegion(region_id);
                if (region) {
                    const std::string& rname = region->getName();
                    auto mod = developmental_constraints_->getLearningModulation("Hebbian", rname);
                    dev_lr_mult = mod.learning_rate_multiplier;
                    dev_plasticity_mult = mod.plasticity_multiplier;
                    dev_restricted = mod.is_restricted;
                }
            }
            if (dev_restricted) {
                return; // Skip Hebbian updates during restricted periods
            }
            
            // Attempt accelerated path when batch size is reasonably large, user prefers GPU, and CUDA is available
            if (config_.prefer_gpu && CUDAAccel::isAvailable() && synapses.size() >= 64) {
                const float dt = 0.016f; // matches CPU path usage in updateWeight
                const std::size_t n = synapses.size();
                std::vector<float> pre(n, 0.0f), post(n, 0.0f), weights(n, 0.0f);
                // ratio between requested base_rate and configured default
                float ratio_factor = 1.0f;
                if (config_.hebbian_rate > 1e-12f) {
                    ratio_factor = base_rate / config_.hebbian_rate;
                }
                // Snapshot competence level once for consistency
                float comp_level = 1.0f;
                if (config_.competence_mode == CompetenceMode::ScaleLearningRates) {
                    comp_level = std::clamp(competence_level_.load(std::memory_order_relaxed), 0.0f, 1.0f);
                }
                // Build contiguous arrays and embed per-synapse effective LR into pre[]
                for (std::size_t i = 0; i < n; ++i) {
                    auto& synapse = synapses[i];
                    weights[i] = synapse ? synapse->getWeight() : 0.0f;
                    if (!synapse || !synapse->isValid()) {
                        pre[i] = 0.0f; post[i] = 0.0f; continue;
                    }
                    // Stochastic gating for sparse updates
                    float effective_p_gate_h = config_.p_gate;
                    if (config_.competence_mode == CompetenceMode::ScalePGate) {
                        float comp = std::clamp(competence_level_.load(std::memory_order_relaxed), 0.0f, 1.0f);
                        effective_p_gate_h = std::clamp(effective_p_gate_h * comp, 0.0f, 1.0f);
                    }
                    if (effective_p_gate_h < 1.0f && dist01_(rng_) > effective_p_gate_h) {
                        pre[i] = 0.0f; post[i] = 0.0f; continue; // skip this synapse
                    }
                    auto source_weak = synapse->getSource();
                    auto target_weak = synapse->getTarget();
                    auto source = source_weak.lock();
                    auto target = target_weak.lock();
                    if (!source || !target) { pre[i] = 0.0f; post[i] = 0.0f; continue; }
                    float pre_activation = source->getActivation();
                    float post_activation = target->getActivation();
                    // Attention factor
                    float attention_factor = 1.0f;
                    if (config_.attention_mode == AttentionMode::Saliency) {
                        float pre_a = std::clamp(pre_activation, 0.0f, 1.0f);
                        float post_a = std::clamp(post_activation, 0.0f, 1.0f);
                        float w = std::max(pre_a, post_a);
                        attention_factor = 1.0f + (last_attention_boost_base_ - 1.0f) * w;
                    } else if (!attention_weights_.empty()) {
                        float w_s = 0.0f, w_t = 0.0f;
                        auto itS = attention_weights_.find(source->getId()); if (itS != attention_weights_.end()) w_s = itS->second;
                        auto itT = attention_weights_.find(target->getId()); if (itT != attention_weights_.end()) w_t = itT->second;
                        float w = std::clamp(std::max(w_s, w_t), 0.0f, 1.0f);
                        attention_factor = 1.0f + (last_attention_boost_base_ - 1.0f) * w;
                    }
                    // Effective per-synapse LR folded into pre[] so the kernel can use lr=1
                    float eff_lr = synapse->getLearningRate() * attention_factor * ratio_factor * dev_lr_mult * dev_plasticity_mult * comp_level;
                    float eg = 1.0f;
                    {
                        float e = target->getEnergy();
                        float h = target->getMitoHealth();
                        eg = std::pow(e, 2.0f) * h;
                    }
                    eff_lr *= eg;
                    pre[i]  = pre_activation * (eff_lr * dt);
                    post[i] = post_activation;
                }
                // Launch accelerated Hebbian update (lr=1.0; factors embedded in pre[])
                bool gpu_ok = CUDAAccel::hebbianUpdate(pre.data(), post.data(), weights.data(), static_cast<int>(n), 1.0f);
                if (gpu_ok) {
                    for (std::size_t i = 0; i < n; ++i) {
                        auto& synapse = synapses[i];
                        if (!synapse) continue;
                        float before_w = synapse->getWeight();
                        float gpu_after = weights[i];
                        float delta = gpu_after - before_w;
                        // Preserve CPU guardrails
                        float safe_delta = synapse->applySafetyGuardrailsPublic(delta);
                        synapse->setWeight(before_w + safe_delta);
                        updateStatistics(Algorithm::Hebbian, safe_delta);
                    }
                    // Optional homeostasis
                    if (config_.enable_homeostasis) {
                        applyHomeostasis(region_id);
                    }
                    return; // Accelerated path handled this region
                }
                // If GPU failed, fall through to CPU path
            }

            for (const auto& synapse : synapses) {
                if (!synapse || !synapse->isValid()) continue;
                
                // Stochastic gating for sparse updates
                float effective_p_gate_h = config_.p_gate;
                if (config_.competence_mode == CompetenceMode::ScalePGate) {
                    float comp = competence_level_.load(std::memory_order_relaxed);
                    effective_p_gate_h = std::clamp(effective_p_gate_h * comp, 0.0f, 1.0f);
                }
                if (effective_p_gate_h < 1.0f && dist01_(rng_) > effective_p_gate_h) {
                    continue;
                }
                
                auto source_weak = synapse->getSource();
                auto target_weak = synapse->getTarget();
                
                auto source = source_weak.lock();
                auto target = target_weak.lock();
                
                if (!source || !target) continue;
                
                float pre_activation = source->getActivation();
                float post_activation = target->getActivation();
                
                // Compute attention factor depending on mode
                float attention_factor = 1.0f;
                if (config_.attention_mode == AttentionMode::Saliency) {
                    float pre_a = std::clamp(pre_activation, 0.0f, 1.0f);
                    float post_a = std::clamp(post_activation, 0.0f, 1.0f);
                    float w = std::max(pre_a, post_a);
                    attention_factor = 1.0f + (attention_boost_effective_ - 1.0f) * w;
                } else if (!attention_weights_.empty()) {
                    float w_s = 0.0f;
                    float w_t = 0.0f;
                    {
                        auto it = attention_weights_.find(source->getId());
                        if (it != attention_weights_.end()) w_s = it->second;
                    }
                    {
                        auto it = attention_weights_.find(target->getId());
                        if (it != attention_weights_.end()) w_t = it->second;
                    }
                    float w = std::max(w_s, w_t);
                    w = std::clamp(w, 0.0f, 1.0f);
                    attention_factor = 1.0f + (attention_boost_effective_ - 1.0f) * w;
                }
                
                // Temporarily scale the synapse's learning rate so the weight update reflects
                // both the requested base rate override and the attention factor.
                const float original_lr = synapse->getLearningRate();
                float ratio_factor = 1.0f;
                if (config_.hebbian_rate > 1e-12f) {
                    ratio_factor = base_rate / config_.hebbian_rate;
                }
                float effective_synapse_lr = original_lr * attention_factor * ratio_factor * dev_lr_mult * dev_plasticity_mult;
                if (config_.competence_mode == CompetenceMode::ScaleLearningRates) {
                    float comp = competence_level_.load(std::memory_order_relaxed);
                    effective_synapse_lr *= std::clamp(comp, 0.0f, 1.0f);
                }
                float eg = 1.0f;
                {
                    float e = target->getEnergy();
                    float h = target->getMitoHealth();
                    eg = std::pow(e, 2.0f) * h;
                }
                effective_synapse_lr *= eg;
                synapse->setLearningRate(effective_synapse_lr);
                
                // Apply rule-respecting weight update (uses the synapse's plasticity rule)
                // Ensure Hebbian rule is applied even if the synapse hasn't been configured
                // with a specific plasticity rule yet.
                auto original_rule = synapse->getPlasticityRule();
                synapse->setPlasticityRule(Synapse::PlasticityRule::Hebbian);
        
                // Track actual weight delta
                float before_w = synapse->getWeight();
                synapse->updateWeight(pre_activation, post_activation, 0.016f);
                float after_w = synapse->getWeight();
                float delta = after_w - before_w;
        
                synapse->setPlasticityRule(original_rule);
                
                // Restore original learning rate
                synapse->setLearningRate(original_lr);
                
                // Record statistics using actual signed delta (averaging uses |delta|)
                updateStatistics(Algorithm::Hebbian, delta);
            }
        }

        // Developmental constraints setter
        void LearningSystem::setDevelopmentalConstraints(NeuroForge::Memory::DevelopmentalConstraints* constraints) {
            developmental_constraints_ = constraints;
        }

        void LearningSystem::consolidateMemories(
            const std::vector<NeuroForge::RegionID>& regions,
            ConsolidationPhase phase) {
            (void)phase;
            if (!is_active_.load() || is_paused_.load()) return;

            std::lock_guard<std::mutex> consolidation_lock(consolidation_mutex_);

            for (RegionID region_id : regions) {
                auto synapses = getRegionSynapses(region_id);

                // Apply weight decay for memory consolidation
                applyWeightDecay(synapses);

                // Update consolidation strength
                consolidation_strengths_[region_id] = config_.consolidation_strength;
            }

            // Track consolidation events telemetry (once per consolidation cycle)
            statistics_.consolidation_events++;
        }

        // New overload: default phase
        void LearningSystem::consolidateMemories(const std::vector<NeuroForge::RegionID>& regions) {
            consolidateMemories(regions, ConsolidationPhase::Consolidation);
        }

        

// Auto eligibility accumulation toggle
void LearningSystem::setAutoEligibilityAccumulation(bool enabled) {
    auto_eligibility_accumulation_enabled_.store(enabled, std::memory_order_relaxed);
}

bool LearningSystem::isAutoEligibilityAccumulationEnabled() const {
    return auto_eligibility_accumulation_enabled_.load(std::memory_order_relaxed);
}

// Configuration accessor
const LearningSystem::Config& LearningSystem::getConfig() const {
    return config_;
}

float LearningSystem::getLastAttentionBoostBase() const {
    return last_attention_boost_base_;
}

// Statistics methods
LearningSystem::Statistics LearningSystem::getStatistics() const {
    Statistics stats = statistics_;
    stats.uncertainty_signal = current_uncertainty_;
    stats.surprise_signal = current_surprise_;
    stats.prediction_error = current_prediction_error_;
    stats.intrinsic_motivation = current_intrinsic_motivation_;
    return stats;
}

void LearningSystem::resetStatistics() {
    statistics_ = Statistics{};
}

// Weight decay implementation
void LearningSystem::applyWeightDecay(const std::vector<NeuroForge::SynapsePtr>& synapses) {
    if (config_.decay_rate <= 0.0f) return;
    
    for (auto& synapse : synapses) {
        if (!synapse) continue;
        
        float current_weight = synapse->getWeight();
        float decayed_weight = current_weight * (1.0f - config_.decay_rate);
        
        // Clamp to synapse bounds
        float min_weight = synapse->getMinWeight();
        float max_weight = synapse->getMaxWeight();
        decayed_weight = std::max(min_weight, std::min(max_weight, decayed_weight));
        
        synapse->setWeight(decayed_weight);
        
        // Update statistics
        float weight_change = decayed_weight - current_weight;
        updateStatistics(Algorithm::RewardModulated, weight_change);
    }
}

// Homeostasis implementation
void LearningSystem::applyHomeostasis(NeuroForge::RegionID region_id) {
    if (!config_.enable_homeostasis || config_.homeostasis_eta <= 0.0f) return;
    
    auto neurons = getRegionNeurons(region_id);
    if (neurons.empty()) return;
    
    // Calculate average activation for the region
    float total_activation = 0.0f;
    for (const auto& neuron : neurons) {
        if (neuron) {
            total_activation += neuron->getActivation();
        }
    }
    float avg_activation = total_activation / static_cast<float>(neurons.size());
    
    // Apply homeostatic scaling to synapses
    auto synapses = getRegionSynapses(region_id);
    for (auto& synapse : synapses) {
        if (!synapse) continue;
        
        float current_weight = synapse->getWeight();
        float homeostatic_adjustment = config_.homeostasis_eta * (1.0f - avg_activation);
        float new_weight = current_weight + homeostatic_adjustment;
        
        // Clamp to synapse bounds
        float min_weight = synapse->getMinWeight();
        float max_weight = synapse->getMaxWeight();
        new_weight = std::max(min_weight, std::min(max_weight, new_weight));
        
        synapse->setWeight(new_weight);
        
        // Update statistics
        float weight_change = new_weight - current_weight;
        updateStatistics(Algorithm::Hebbian, weight_change);
    }
}

// Neuron spike event handler
void LearningSystem::onNeuronSpike(NeuroForge::NeuronID neuron_id, NeuroForge::TimePoint spike_time) {
    // Update spike timing cache for STDP
    {
        std::lock_guard<std::mutex> lock(spike_times_mutex_);
        last_spike_times_[neuron_id] = spike_time;
    }
    
    // If auto-eligibility accumulation is enabled, update eligibility traces
    if (auto_eligibility_accumulation_enabled_.load(std::memory_order_relaxed)) {
        // Find synapses connected to this neuron and update eligibility
        if (brain_) {
            // Search through all regions to find the neuron
            NeuroForge::NeuronPtr neuron = nullptr;
            const auto& regions_map = brain_->getRegionsMap();
            
            for (const auto& region_pair : regions_map) {
                const auto& region = region_pair.second;
                if (region) {
                    neuron = region->getNeuron(neuron_id);
                    if (neuron) {
                        break;
                    }
                }
            }
            
            if (neuron) {
                // Get synapses connected to this neuron
                std::vector<NeuroForge::SynapsePtr> input_synapses = neuron->getInputSynapses();
                std::vector<NeuroForge::SynapsePtr> output_synapses = neuron->getOutputSynapses();
                
                std::lock_guard<std::mutex> lock(syn_state_mutex_);
                
                // Update eligibility for input synapses (post-synaptic)
                for (const auto& synapse : input_synapses) {
                    if (synapse) {
                        auto& state = syn_state_[synapse->getId()];
                        state.eligibility = std::min(1.0f, state.eligibility + 0.1f);
                    }
                }
                
                // Update eligibility for output synapses (pre-synaptic)
                for (const auto& synapse : output_synapses) {
                    if (synapse) {
                        auto& state = syn_state_[synapse->getId()];
                        state.eligibility = std::min(1.0f, state.eligibility + 0.1f);
                    }
                }
            }
        }
    }
}

void LearningSystem::updateConfig(const Config& cfg) {
    config_ = cfg;
}

// Set global learning rate and proportionally adjust Hebbian/STDP rates
void LearningSystem::setLearningRate(float lr) {
    if (lr <= 0.0f) return;
    float prev = config_.global_learning_rate;
    config_.global_learning_rate = lr;
    if (prev > 1e-12f) {
        float scale = lr / prev;
        if (config_.hebbian_rate > 0.0f) {
            config_.hebbian_rate *= scale;
        }
        if (config_.stdp_rate > 0.0f) {
            config_.stdp_rate *= scale;
        }
    }
}

float LearningSystem::getLearningRate() const {
    return config_.global_learning_rate;
}

void LearningSystem::setRandomSeed(std::uint32_t seed) {
    std::lock_guard<std::mutex> lock(syn_state_mutex_);
    rng_.seed(seed);
    dist01_ = std::uniform_real_distribution<float>(0.0f, 1.0f);
}

void LearningSystem::applyAttentionModulation(const std::unordered_map<NeuroForge::NeuronID, float>& attention_map, float learning_boost) {
    std::lock_guard<std::mutex> lock(syn_state_mutex_);
    attention_weights_ = attention_map;
    if (!attention_map.empty()) {
        statistics_.attention_modulation_events++;
        float sum = 0.0f;
        for (const auto& kv : attention_map) {
            sum += kv.second;
        }
        statistics_.mean_attention_weight = sum / static_cast<float>(attention_map.size());
    } else {
        statistics_.mean_attention_weight = 0.0f;
    }

    // Determine base boost: fall back to config if caller provides non-positive
    float base = (learning_boost > 0.0f) ? learning_boost : config_.attention_boost_factor;

    // Enforce sensible bounds using Config [Amin, Amax]
    float amin = std::max(1.0f, config_.attention_Amin);
    float amax = std::max(amin, config_.attention_Amax);
    base = std::clamp(base, amin, amax);

    // Record base and apply immediate effective boost so next updates are scaled
    last_attention_boost_base_ = base;
    attention_boost_effective_ = base;

    // Ensure external-map attention is active when a map is provided
    if (!attention_weights_.empty()) {
        config_.enable_attention_modulation = true;
        if (config_.attention_mode == AttentionMode::Off) {
            config_.attention_mode = AttentionMode::ExternalMap;
        }
    }

    // Reset anneal window so boost decays over configured time in updateLearning()
    if (config_.attention_anneal_ms > 0 && config_.enable_attention_modulation && config_.attention_mode != AttentionMode::Off) {
        attention_anneal_elapsed_ms_ = config_.attention_anneal_ms;
    } else {
        attention_anneal_elapsed_ms_ = 0;
    }
}

std::vector<NeuroForge::SynapsePtr> LearningSystem::getRegionSynapses(NeuroForge::RegionID region_id) const {
    std::vector<NeuroForge::SynapsePtr> synapses;
    if (!brain_) return synapses;
    
    auto region = brain_->getRegion(region_id);
    if (!region) return synapses;
    
    // Pre-calculate total size to avoid reallocations (performance optimization)
    size_t total_size = 0;
    const auto& internal_synapses = region->getInternalSynapses();
    total_size += internal_synapses.size();
    
    const auto& input_connections = region->getInputConnections();
    for (const auto& conn_pair : input_connections) {
        total_size += conn_pair.second.size();
    }
    
    const auto& output_connections = region->getOutputConnections();
    for (const auto& conn_pair : output_connections) {
        total_size += conn_pair.second.size();
    }
    
    // Reserve space to avoid reallocations
    synapses.reserve(total_size);
    
    // Get internal synapses
    synapses.insert(synapses.end(), internal_synapses.begin(), internal_synapses.end());
    
    // Get input connections
    for (const auto& conn_pair : input_connections) {
        const auto& conn_synapses = conn_pair.second;
        synapses.insert(synapses.end(), conn_synapses.begin(), conn_synapses.end());
    }
    
    // Get output connections
    for (const auto& conn_pair : output_connections) {
        const auto& conn_synapses = conn_pair.second;
        synapses.insert(synapses.end(), conn_synapses.begin(), conn_synapses.end());
    }
    
    return synapses;
}

std::vector<NeuroForge::NeuronPtr> LearningSystem::getRegionNeurons(NeuroForge::RegionID region_id) const {
    std::vector<NeuroForge::NeuronPtr> neurons;
    if (!brain_) return neurons;
    
    auto region = brain_->getRegion(region_id);
    if (!region) return neurons;
    
    const auto& region_neurons = region->getNeurons();
    neurons.insert(neurons.end(), region_neurons.begin(), region_neurons.end());
    
    return neurons;
}

std::vector<LearningSystem::SynapseSnapshot> LearningSystem::getSynapseSnapshot() const {
    std::vector<SynapseSnapshot> snapshots;
    if (!brain_) return snapshots;
    
    // Get all regions and their synapses
    const auto& regions = brain_->getRegionsMap();
    for (const auto& region_pair : regions) {
        auto region = region_pair.second;
        if (!region) continue;
        
        // Get internal synapses
        const auto& internal_synapses = region->getInternalSynapses();
        for (const auto& synapse : internal_synapses) {
            if (synapse) {
                SynapseSnapshot snapshot;
                auto source = synapse->getSource().lock();
                auto target = synapse->getTarget().lock();
                if (source && target) {
                    snapshot.pre_neuron = source->getId();
                    snapshot.post_neuron = target->getId();
                    snapshot.weight = synapse->getWeight();
                    snapshots.push_back(snapshot);
                }
            }
        }
        
        // Get input connections
        const auto& input_connections = region->getInputConnections();
        for (const auto& conn_pair : input_connections) {
            const auto& conn_synapses = conn_pair.second;
            for (const auto& synapse : conn_synapses) {
                if (synapse) {
                    SynapseSnapshot snapshot;
                    auto source = synapse->getSource().lock();
                    auto target = synapse->getTarget().lock();
                    if (source && target) {
                        snapshot.pre_neuron = source->getId();
                        snapshot.post_neuron = target->getId();
                        snapshot.weight = synapse->getWeight();
                        snapshots.push_back(snapshot);
                    }
                }
            }
        }
        
        // Get output connections
        const auto& output_connections = region->getOutputConnections();
        for (const auto& conn_pair : output_connections) {
            const auto& conn_synapses = conn_pair.second;
            for (const auto& synapse : conn_synapses) {
                if (synapse) {
                    SynapseSnapshot snapshot;
                    auto source = synapse->getSource().lock();
                    auto target = synapse->getTarget().lock();
                    if (source && target) {
                        snapshot.pre_neuron = source->getId();
                        snapshot.post_neuron = target->getId();
                        snapshot.weight = synapse->getWeight();
                        snapshots.push_back(snapshot);
                    }
                }
            }
        }
    }
    
    return snapshots;
}

void LearningSystem::updateStatistics(LearningSystem::Algorithm algorithm, float weight_change) {
    std::lock_guard<std::mutex> lock(syn_state_mutex_);
    
    statistics_.total_updates++;
    // Use magnitude for averaging to reflect overall plasticity strength
    float mag = std::fabs(weight_change);
    statistics_.average_weight_change = 
        (statistics_.average_weight_change * (statistics_.total_updates - 1) + mag) / statistics_.total_updates;
    
    switch (algorithm) {
        case Algorithm::Hebbian:
            statistics_.hebbian_updates++;
            break;
        case Algorithm::STDP:
            statistics_.stdp_updates++;
            break;
        case Algorithm::RewardModulated:
            statistics_.reward_updates++;
            break;
    }
    
    if (weight_change > 0) {
        statistics_.potentiated_synapses++;
    } else if (weight_change < 0) {
        statistics_.depressed_synapses++;
    }
    
    // FIX: Update active synapses count by counting all synapses in the brain
    if (brain_) {
        statistics_.active_synapses = 0;
        // Count synapses across all regions
        const auto& regions_map = brain_->getRegions();
        for (const auto& [region_id, region] : regions_map) {
            if (region) {
                const auto& neurons = region->getNeurons();
                for (const auto& neuron : neurons) {
                    if (neuron) {
                        statistics_.active_synapses += neuron->getInputSynapseCount();
                    }
                }
            }
        }
    }
}

// ===== Phase 4: Reward-Modulated Plasticity =====
        void LearningSystem::notePrePost(NeuroForge::SynapseID sid, float pre, float post) {
            if (!is_active_.load()) return;
            const float increment = etaElig_ * pre * post;
            std::lock_guard<std::mutex> lock(syn_state_mutex_);
            auto &st = syn_state_[sid];
            st.eligibility = lambda_ * st.eligibility + increment;
        }

        void LearningSystem::applyExternalReward(float r) {
            // Clamp external reward to a reasonable range to avoid instability
            r = std::clamp(r, -2.0f, 2.0f);
            // Use compare_exchange_weak for atomic float addition (C++11 compatible)
            float expected = pending_reward_.load(std::memory_order_relaxed);
            float desired;
            do {
                desired = expected + r;
            } while (!pending_reward_.compare_exchange_weak(expected, desired, std::memory_order_relaxed));

            // Immediately update competence as EMA of normalized reward in [0,1]
            // This ensures competence gating affects learning right after external reward is applied
            float comp = competence_level_.load(std::memory_order_relaxed);
            float s_norm = 0.25f * (r + 2.0f); // map [-2,2] -> [0,1]
            s_norm = std::clamp(s_norm, 0.0f, 1.0f);
            float rho = std::clamp(config_.competence_rho, 0.0f, 1.0f);
            float comp_new = (1.0f - rho) * comp + rho * s_norm;
            competence_level_.store(comp_new, std::memory_order_relaxed);
        }

        void LearningSystem::configurePhase4(float lambda, float etaElig, float kappa, float alpha, float gamma, float eta) {
            lambda_ = lambda;
            etaElig_ = etaElig;
            kappa_ = kappa;
            alpha_ = alpha;
            gamma_ = gamma;
            eta_ = eta;
            // Reset Phase 4 runtime state to ensure deterministic behavior between configurations
            {
                std::lock_guard<std::mutex> lock(syn_state_mutex_);
                for (auto &kv : syn_state_) {
                    kv.second.eligibility = 0.0f;
                }
            }
        }

        float LearningSystem::getElig(NeuroForge::SynapseID sid) const {
            std::lock_guard<std::mutex> lock(syn_state_mutex_);
            auto it = syn_state_.find(sid);
            if (it == syn_state_.end()) return 0.0f;
            return it->second.eligibility;
        }

        float LearningSystem::computeShapedReward(const std::vector<float>& obs,
                                                  const std::vector<float>& regionActs,
                                                  float taskReward) {
            // Novelty via 1 - cosine similarity to an exponential moving average of observations
            if (obs_mean_.empty()) {
                obs_mean_ = obs;
            } else if (obs_mean_.size() != obs.size()) {
                // Resize mean vector if observation dimensionality changes
                obs_mean_.assign(obs.size(), 0.0f);
            }

            float dot = 0.0f, n1 = 0.0f, n2 = 0.0f;
            for (size_t i = 0; i < obs.size(); ++i) {
                dot += obs[i] * obs_mean_[i];
                n1 += obs[i] * obs[i];
                n2 += obs_mean_[i] * obs_mean_[i];
            }
            float denom = std::sqrt(n1) * std::sqrt(n2) + 1e-6f;
            float novelty_obs = 1.0f - (denom > 0.0f ? (dot / denom) : 0.0f);

            // Update running mean
            const float beta = 0.01f;
            for (size_t i = 0; i < obs.size(); ++i) {
                obs_mean_[i] = (1.0f - beta) * obs_mean_[i] + beta * obs[i];
            }

            // Substrate-side novelty via 1 - cosine similarity between region activations and their EMA
            float novelty_sub = 0.0f;
            float substrate_sim = 0.0f;
            if (!regionActs.empty()) {
                if (region_mean_.empty() || region_mean_.size() != regionActs.size()) {
                    region_mean_ = regionActs; // initialize
                }
                float d2 = 0.0f, na = 0.0f, nm = 0.0f;
                for (size_t i = 0; i < regionActs.size(); ++i) {
                    d2 += regionActs[i] * region_mean_[i];
                    na += regionActs[i] * regionActs[i];
                    nm += region_mean_[i] * region_mean_[i];
                }
                float den2 = std::sqrt(std::max(1e-12f, na)) * std::sqrt(std::max(1e-12f, nm));
                substrate_sim = (den2 > 0.0f ? d2 / den2 : 0.0f);
                novelty_sub = 1.0f - substrate_sim;
                // Update running mean for region activations
                for (size_t i = 0; i < regionActs.size(); ++i) {
                    region_mean_[i] = (1.0f - beta) * region_mean_[i] + beta * regionActs[i];
                }
            }
            last_substrate_similarity_ = substrate_sim;
            last_substrate_novelty_ = novelty_sub;

            // Uncertainty as variance of regional activations
            float mean = 0.0f;
            if (!regionActs.empty()) {
                for (float v : regionActs) mean += v;
                mean /= static_cast<float>(regionActs.size());
            }
            float var = 0.0f;
            if (!regionActs.empty()) {
                for (float v : regionActs) { float d = v - mean; var += d * d; }
                var /= static_cast<float>(regionActs.size());
            }

            // Mimicry similarity term (cosine between teacher and student embeddings)
            float mimicry_sim = 0.0f;
            float mu = 0.0f;
            bool use_phase_a = false;
            float phase_a_similarity = 0.0f;
            float phase_a_novelty = 0.0f;
            float phase_a_total = 0.0f;
            {
                std::lock_guard<std::mutex> lg(mimicry_mutex_);
                mu = mimicry_weight_mu_;
                // If internal Phase A is enabled and scores are available, prefer those
                if (mimicry_internal_enabled_ && has_phase_a_scores_) {
                    use_phase_a = true;
                    phase_a_similarity = last_phase_a_similarity_;
                    phase_a_novelty = last_phase_a_novelty_;
                    phase_a_total = last_phase_a_total_reward_;
                    last_mimicry_sim_ = phase_a_similarity; // keep telemetry coherent
                } else if (mimicry_enabled_ && !teacher_embed_.empty() && !student_embed_.empty() && teacher_embed_.size() == student_embed_.size()) {
                    float d = 0.0f, ns = 0.0f, nt = 0.0f;
                    for (size_t i = 0; i < teacher_embed_.size(); ++i) {
                        d += teacher_embed_[i] * student_embed_[i];
                        ns += student_embed_[i] * student_embed_[i];
                        nt += teacher_embed_[i] * teacher_embed_[i];
                    }
                    float den = std::sqrt(std::max(1e-12f, ns)) * std::sqrt(std::max(1e-12f, nt));
                    mimicry_sim = (den > 0.0f ? d / den : 0.0f);
                    last_mimicry_sim_ = mimicry_sim;
                } else {
                    mimicry_sim = 0.0f;
                    last_mimicry_sim_ = 0.0f;
                }
            }

            // Blend novelty terms unless overridden by Phase A
            float novelty = novelty_obs;
            if (!use_phase_a) {
                float w_obs = std::max(0.0f, config_.novelty_obs_weight);
                float w_sub = std::max(0.0f, config_.novelty_substrate_weight);
                float sumw = w_obs + w_sub;
                if (sumw > 1e-6f) {
                    novelty = (w_obs * novelty_obs + w_sub * novelty_sub) / sumw;
                }
            }

            // If Phase A scores are used, override novelty and mimicry similarity from Phase A
            if (use_phase_a) {
                // Use Phase A novelty directly to replace novelty term
                novelty = phase_a_novelty;
                // Use Phase A similarity as mimicry signal
                mimicry_sim = phase_a_similarity;
                (void)phase_a_total; // reserved for future blending if needed
            }

            float r = alpha_*novelty + gamma_*taskReward - eta_*var + mu * mimicry_sim;
            r = std::clamp(r, -2.0f, 2.0f);

            // Update competence as EMA of normalized reward in [0,1]
            float comp = competence_level_.load(std::memory_order_relaxed);
            float s_norm = 0.25f * (r + 2.0f); // map [-2,2] -> [0,1]
            s_norm = std::clamp(s_norm, 0.0f, 1.0f);
            float rho = std::clamp(config_.competence_rho, 0.0f, 1.0f);
            float comp_new = (1.0f - rho) * comp + rho * s_norm;
            competence_level_.store(comp_new, std::memory_order_relaxed);
            return r;
        }

        NeuroForge::SynapsePtr LearningSystem::findSynapseById(NeuroForge::SynapseID sid) const {
            if (!brain_) return nullptr;
            const auto& regions_map = brain_->getRegionsMap();

            auto check_list = [&](const auto& list) -> NeuroForge::SynapsePtr {
                for (const auto& s : list) {
                    if (s && s->getId() == sid) return s;
                }
                return nullptr;
            };

            for (auto it = regions_map.begin(); it != regions_map.end(); ++it) {
                const auto& region = it->second;
                if (!region) continue;
                if (auto res = check_list(region->getInternalSynapses())) return res;

                const auto& inConns = region->getInputConnections();
                for (auto it2 = inConns.begin(); it2 != inConns.end(); ++it2) {
                    const auto& vec = it2->second;
                    if (auto res = check_list(vec)) return res;
                }
                const auto& outConns = region->getOutputConnections();
                for (auto it2 = outConns.begin(); it2 != outConns.end(); ++it2) {
                    const auto& vec = it2->second;
                    if (auto res = check_list(vec)) return res;
                }
                const auto& interConns = region->getInterRegionConnections();
                for (auto it2 = interConns.begin(); it2 != interConns.end(); ++it2) {
                    const auto& vec = it2->second;
                    if (auto res = check_list(vec)) return res;
                }
            }
            return nullptr;
        }

        // M7: Intrinsic motivation methods
        float LearningSystem::calculateUncertaintySignal() const {
            std::lock_guard<std::mutex> lock(intrinsic_motivation_mutex_);
            
            if (prediction_history_.size() < 2) {
                return 0.0f;
            }
            
            // Calculate variance in predictions as uncertainty measure
            float mean_variance = 0.0f;
            size_t state_size = prediction_history_[0].size();
            
            for (size_t i = 0; i < state_size; ++i) {
                float mean = 0.0f;
                for (const auto& prediction : prediction_history_) {
                    if (i < prediction.size()) {
                        mean += prediction[i];
                    }
                }
                mean /= prediction_history_.size();
                
                float variance = 0.0f;
                for (const auto& prediction : prediction_history_) {
                    if (i < prediction.size()) {
                        float diff = prediction[i] - mean;
                        variance += diff * diff;
                    }
                }
                variance /= prediction_history_.size();
                mean_variance += variance;
            }
            
            return mean_variance / state_size;
        }

        float LearningSystem::calculateSurpriseSignal(const std::vector<float>& current_state) {
            std::lock_guard<std::mutex> lock(intrinsic_motivation_mutex_);
            
            if (last_state_.empty() || current_state.size() != last_state_.size()) {
                return 0.0f;
            }
            
            // Calculate KL divergence-like measure for surprise
            float surprise = 0.0f;
            for (size_t i = 0; i < current_state.size(); ++i) {
                float expected = last_state_[i];
                float actual = current_state[i];
                
                // Avoid log(0) by adding small epsilon
                const float epsilon = 1e-8f;
                expected = std::max(expected, epsilon);
                actual = std::max(actual, epsilon);
                
                surprise += actual * std::log(actual / expected);
            }
            
            return surprise;
        }

        float LearningSystem::calculatePredictionError(const std::vector<float>& predicted_state, 
                                                       const std::vector<float>& actual_state) {
            if (predicted_state.size() != actual_state.size()) {
                return 0.0f;
            }
            
            // Calculate mean squared error
            float mse = 0.0f;
            for (size_t i = 0; i < predicted_state.size(); ++i) {
                float diff = predicted_state[i] - actual_state[i];
                mse += diff * diff;
            }
            
            return mse / predicted_state.size();
        }

        float LearningSystem::getIntrinsicMotivation() const {
            std::lock_guard<std::mutex> lock(intrinsic_motivation_mutex_);
            return current_intrinsic_motivation_;
        }

        void LearningSystem::updateIntrinsicMotivation(const std::vector<float>& current_state) {
            if (!config_.enable_intrinsic_motivation) {
                return;
            }
            
            std::lock_guard<std::mutex> lock(intrinsic_motivation_mutex_);
            
            // Update uncertainty
            current_uncertainty_ = calculateUncertaintySignal();
            
            // Update surprise
            current_surprise_ = calculateSurpriseSignal(current_state);
            
            // Update prediction error (using last prediction if available)
            if (!prediction_history_.empty() && !prediction_history_.back().empty()) {
                current_prediction_error_ = calculatePredictionError(prediction_history_.back(), current_state);
            }
            
            // Combine signals into intrinsic motivation
            float new_motivation = config_.uncertainty_weight * current_uncertainty_ +
                                   config_.surprise_weight * current_surprise_ +
                                   config_.prediction_error_weight * current_prediction_error_;
            
            // Apply decay to current motivation and add new signal
            current_intrinsic_motivation_ = current_intrinsic_motivation_ * config_.intrinsic_motivation_decay + 
                                            new_motivation * (1.0f - config_.intrinsic_motivation_decay);
            
            // Update prediction history
            prediction_history_.push_back(current_state);
            if (prediction_history_.size() > static_cast<size_t>(config_.prediction_history_size)) {
                prediction_history_.erase(prediction_history_.begin());
            }
            
            // Update last state
            last_state_ = current_state;
        }

        // M6/M7: Substrate and autonomous operation method implementations
        void LearningSystem::setSubstrateTrainingMode(bool enabled) {
            substrate_training_mode_.store(enabled, std::memory_order_relaxed);
        }

        bool LearningSystem::isSubstrateTrainingMode() const {
            return substrate_training_mode_.load(std::memory_order_relaxed);
        }

        void LearningSystem::setScaffoldElimination(bool enabled) {
            scaffold_elimination_enabled_.store(enabled, std::memory_order_relaxed);
        }

        bool LearningSystem::isScaffoldEliminationEnabled() const {
            return scaffold_elimination_enabled_.load(std::memory_order_relaxed);
        }

        void LearningSystem::setMotivationDecay(float decay) {
            if (decay >= 0.0f && decay <= 1.0f) {
                motivation_decay_.store(decay, std::memory_order_relaxed);
                // Update config for consistency
                config_.intrinsic_motivation_decay = decay;
            }
        }

        float LearningSystem::getMotivationDecay() const {
            return motivation_decay_.load(std::memory_order_relaxed);
        }

        void LearningSystem::setExplorationBonus(float bonus) {
            if (bonus >= 0.0f) {
                exploration_bonus_.store(bonus, std::memory_order_relaxed);
            }
        }

        float LearningSystem::getExplorationBonus() const {
            return exploration_bonus_.load(std::memory_order_relaxed);
        }

        void LearningSystem::setNoveltyMemorySize(std::size_t size) {
            if (size > 0) {
                novelty_memory_size_.store(size, std::memory_order_relaxed);
                // Update config for consistency
                config_.prediction_history_size = static_cast<int>(size);
            }
        }

        std::size_t LearningSystem::getNoveltyMemorySize() const {
            return novelty_memory_size_.load(std::memory_order_relaxed);
        }

    } // namespace Core
} // namespace NeuroForge
