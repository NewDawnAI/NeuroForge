#include "core/Synapse.h"
#include "core/Neuron.h"
#include <algorithm>
#include <cmath>

namespace NeuroForge {
namespace Core {

// Helper clamp function (local)
static inline float clampf(float v, float lo, float hi) {
    return std::max(lo, std::min(v, hi));
}

// Static member initialization
std::atomic<NeuroForge::SynapseID> SynapseFactory::next_id_{1};

// Synapse Implementation
Synapse::Synapse(NeuroForge::SynapseID id, 
                        NeuroForge::NeuronWeakPtr source, 
                        NeuroForge::NeuronWeakPtr target,
                        NeuroForge::Weight initial_weight,
                        NeuroForge::SynapseType type)
            : id_(id)
            , source_(source)
            , target_(target)
            , weight_(initial_weight)
            , initial_weight_(initial_weight)
            , type_(type)
            , plasticity_rule_(PlasticityRule::None)
            , learning_rate_(0.01f)
            , delay_ms_(1.0f)
            , min_weight_(-1.0f)
            , max_weight_(1.0f)
            , signal_count_(0)
            , update_count_(0)
            , weight_sum_(initial_weight)
            , min_recorded_weight_(initial_weight)
            , max_recorded_weight_(initial_weight) {
            
            // Reserve space for signal buffer
            signal_buffer_.reserve(10);
        }

        Weight Synapse::getWeight() const noexcept {
            return weight_.load(std::memory_order_relaxed);
        }

        void Synapse::setWeight(Weight weight) noexcept {
            // Clamp weight to bounds
            weight = std::max(min_weight_, std::min(max_weight_, weight));
            weight_.store(weight, std::memory_order_relaxed);
            updateStatistics();
        }

        bool Synapse::isValid() const noexcept {
            return !source_.expired() && !target_.expired();
        }

        void Synapse::propagateSignal(float signal_strength) {
            if (!isValid()) {
                return;
            }

            signal_count_.fetch_add(1, std::memory_order_relaxed);

            auto current_time = std::chrono::steady_clock::now();
            
            if (delay_ms_ <= 0.0f) {
                // Immediate propagation
                auto target = target_.lock();
                if (target) {
                    float weighted_signal = signal_strength * getWeight();
                    
                    // Apply synapse type modifier
                    switch (type_) {
                        case NeuroForge::SynapseType::Excitatory:
                            // Positive contribution
                            break;
                        case NeuroForge::SynapseType::Inhibitory:
                            // Negative contribution
                            weighted_signal = -std::abs(weighted_signal);
                            break;
                        case NeuroForge::SynapseType::Modulatory:
                            // Modulate existing activation (multiplicative)
                            weighted_signal *= 0.1f; // Reduced impact for modulation
                            break;
                    }
                    
                    // Add to target neuron's input (this would be handled by the target neuron)
                    // For now, we store the signal for the neuron to process
                }
            } else {
                // Delayed propagation
                auto delivery_time = current_time + std::chrono::milliseconds(static_cast<int>(delay_ms_));
                std::lock_guard<std::mutex> lock(signal_buffer_mutex_);
                signal_buffer_.push_back({signal_strength * getWeight(), delivery_time});
            }
        }

        float Synapse::getWeightedInput() const noexcept {
            processDelayedSignals();
            
            if (!isValid()) {
                return 0.0f;
            }

            auto source = source_.lock();
            if (!source) {
                return 0.0f;
            }

            float source_activation = source->getActivation();
            float weighted_input = source_activation * getWeight();
            
            // Apply synapse type modifier
            switch (type_) {
                case NeuroForge::SynapseType::Excitatory:
                    // Positive contribution
                    break;
                case NeuroForge::SynapseType::Inhibitory:
                    // Negative contribution
                    weighted_input = -std::abs(weighted_input);
                    break;
                case NeuroForge::SynapseType::Modulatory:
                    // Modulate existing activation
                    weighted_input *= 0.1f;
                    break;
            }
            
            return weighted_input;
        }

        void Synapse::updateWeight(float pre_activation, float post_activation, float delta_time) {
            if (plasticity_rule_ == PlasticityRule::None) {
                return;
            }

            update_count_.fetch_add(1, std::memory_order_relaxed);

            switch (plasticity_rule_) {
                case PlasticityRule::Hebbian:
                    applyHebbianLearning(pre_activation, post_activation, delta_time);
                    break;
                    
                case PlasticityRule::BCM: {
                    // Bienenstock-Cooper-Munro rule
                    float theta = 0.5f; // Sliding threshold
                    float delta_w = learning_rate_ * post_activation * 
                                   (post_activation - theta) * pre_activation * delta_time;
                    // Apply enhanced safety guardrails
                    delta_w = applySafetyGuardrails(delta_w);
                    setWeight(getWeight() + delta_w);
                    break;
                }
                
                case PlasticityRule::Oja: {
                    // Oja's rule for weight normalization
                    float current_weight = getWeight();
                    float delta_w = learning_rate_ * post_activation * 
                                   (pre_activation - post_activation * current_weight) * delta_time;
                    // Apply enhanced safety guardrails
                    delta_w = applySafetyGuardrails(delta_w);
                    setWeight(current_weight + delta_w);
                    break;
                }
                
                default:
                    break;
            }
        }

        void Synapse::applyHebbianLearning(float pre_activation, float post_activation, float delta_time) {
            // Basic Hebbian rule: Δw = η * pre * post
            float delta_w = learning_rate_ * pre_activation * post_activation * delta_time;
            // Apply enhanced safety guardrails
            delta_w = applySafetyGuardrails(delta_w);
            setWeight(getWeight() + delta_w);
        }

        void Synapse::applySTDP(TimePoint pre_spike_time, TimePoint post_spike_time) {
            if (plasticity_rule_ != PlasticityRule::STDP) {
                return;
            }

            // Calculate time difference in milliseconds
            auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
                post_spike_time - pre_spike_time).count();
            
            float delta_w = 0.0f;
            
            if (time_diff > 0) {
                // Post-synaptic spike after pre-synaptic (LTP)
                delta_w = learning_rate_ * std::exp(-static_cast<float>(time_diff) / 20.0f);
            } else if (time_diff < 0) {
                // Pre-synaptic spike after post-synaptic (LTD)
                delta_w = -learning_rate_ * std::exp(static_cast<float>(time_diff) / 20.0f);
            }
            
            // Apply enhanced safety guardrails
            delta_w = applySafetyGuardrails(delta_w);
            setWeight(getWeight() + delta_w);
        }

        void Synapse::reset() {
            setWeight(initial_weight_);
            {
                std::lock_guard<std::mutex> lock(signal_buffer_mutex_);
                signal_buffer_.clear();
            }
            signal_count_.store(0, std::memory_order_relaxed);
            update_count_.store(0, std::memory_order_relaxed);
            weight_sum_.store(initial_weight_, std::memory_order_relaxed);
            min_recorded_weight_.store(initial_weight_, std::memory_order_relaxed);
            max_recorded_weight_.store(initial_weight_, std::memory_order_relaxed);
        }

        std::size_t Synapse::getMemoryUsage() const noexcept {
            std::size_t base_size = sizeof(Synapse);
            std::size_t buffer_size;
            {
                std::lock_guard<std::mutex> lock(signal_buffer_mutex_);
                buffer_size = signal_buffer_.capacity() * sizeof(DelayedSignal);
            }
            return base_size + buffer_size;
        }

        Synapse::Statistics Synapse::getStatistics() const noexcept {
            Statistics stats;
            stats.signal_count = signal_count_.load(std::memory_order_relaxed);
            stats.update_count = update_count_.load(std::memory_order_relaxed);
            stats.min_weight = min_recorded_weight_.load(std::memory_order_relaxed);
            stats.max_weight = max_recorded_weight_.load(std::memory_order_relaxed);
            
            // Calculate average weight
            auto update_count = stats.update_count;
            if (update_count > 0) {
                stats.avg_weight = weight_sum_.load(std::memory_order_relaxed) / static_cast<float>(update_count);
            } else {
                stats.avg_weight = getWeight();
            }
            
            return stats;
        }

        void Synapse::processDelayedSignals() const {
            std::lock_guard<std::mutex> lock(signal_buffer_mutex_);
            
            if (signal_buffer_.empty()) {
                return;
            }

            auto current_time = std::chrono::steady_clock::now();
            
            // Process and remove delivered signals
            auto it = signal_buffer_.begin();
            while (it != signal_buffer_.end()) {
                if (current_time >= it->delivery_time) {
                    // Signal should be delivered
                    auto target = target_.lock();
                    if (target) {
                        // Apply the delayed signal (implementation depends on target neuron interface)
                        // For now, we just remove the signal from buffer
                    }
                    it = signal_buffer_.erase(it);
                } else {
                    ++it;
                }
            }
        }

        void Synapse::updateStatistics() const noexcept {
            auto current_weight = getWeight();
            
            // Update weight sum for average calculation
            auto current_sum = weight_sum_.load(std::memory_order_relaxed);
            while (!weight_sum_.compare_exchange_weak(current_sum, current_sum + current_weight, std::memory_order_relaxed)) {
                // Retry until successful
            }
            
            // Update min/max weights
            auto current_min = min_recorded_weight_.load(std::memory_order_relaxed);
            while (current_weight < current_min) {
                if (min_recorded_weight_.compare_exchange_weak(current_min, current_weight, std::memory_order_relaxed)) {
                    break;
                }
            }
            
            auto current_max = max_recorded_weight_.load(std::memory_order_relaxed);
            while (current_weight > current_max) {
                if (max_recorded_weight_.compare_exchange_weak(current_max, current_weight, std::memory_order_relaxed)) {
                    break;
                }
            }
        }

        float Synapse::applySafetyGuardrails(float delta_w) const noexcept {
            // Check for NaN or infinite values
            if (!std::isfinite(delta_w)) {
                return 0.0f;
            }
            
            // Apply gradient magnitude clipping
            float magnitude = std::abs(delta_w);
            if (magnitude > MAX_GRADIENT_MAGNITUDE) {
                delta_w = (delta_w > 0.0f ? 1.0f : -1.0f) * MAX_GRADIENT_MAGNITUDE;
            }
            
            // Apply per-step weight change limit
            delta_w = clampf(delta_w, -MAX_WEIGHT_CHANGE_PER_STEP, MAX_WEIGHT_CHANGE_PER_STEP);
            
            // Check for instability and reduce learning if detected
            if (checkInstability(magnitude)) {
                delta_w *= 0.1f; // Reduce learning rate by 90% if unstable
            }
            
            return delta_w;
        }

        bool Synapse::checkInstability(float delta_w_magnitude) const noexcept {
            // Consider an update "large" if it's more than half the maximum allowed
            bool is_large_update = delta_w_magnitude > (MAX_WEIGHT_CHANGE_PER_STEP * 0.5f);
            
            if (is_large_update) {
                auto current_count = consecutive_large_updates_.fetch_add(1, std::memory_order_relaxed);
                return (current_count + 1) >= MAX_CONSECUTIVE_LARGE_UPDATES;
            } else {
                // Reset counter on small update
                consecutive_large_updates_.store(0, std::memory_order_relaxed);
                return false;
            }
        }

        // SynapseFactory Implementation
        std::shared_ptr<Synapse> SynapseFactory::createSynapse(
            std::weak_ptr<Neuron> source,
            std::weak_ptr<Neuron> target,
            Weight weight,
            SynapseType type) {
            
            auto id = next_id_.fetch_add(1, std::memory_order_relaxed);
            return std::make_shared<Synapse>(id, source, target, weight, type);
        }

        std::shared_ptr<Synapse> SynapseFactory::createSynapse(
            SynapseID id,
            std::weak_ptr<Neuron> source,
            std::weak_ptr<Neuron> target,
            Weight weight,
            SynapseType type) {
            
            // Update next_id if necessary to avoid conflicts
            auto current_next = next_id_.load(std::memory_order_relaxed);
            while (id >= current_next) {
                next_id_.compare_exchange_weak(current_next, id + 1, std::memory_order_relaxed);
                current_next = next_id_.load(std::memory_order_relaxed);
            }
            
            return std::make_shared<Synapse>(id, source, target, weight, type);
        }

        SynapseID SynapseFactory::getNextId() {
            return next_id_.load(std::memory_order_relaxed);
        }

        void SynapseFactory::resetIdCounter() {
            next_id_.store(1, std::memory_order_relaxed);
        }

    void Synapse::decayEligibility(float decay_rate, float dt) noexcept {
        // Exponential decay: e <- e * exp(-decay_rate * dt)
        float e = eligibility_.load(std::memory_order_relaxed);
        float factor = std::exp(-decay_rate * std::max(0.0f, dt));
        e *= factor;
        // small threshold to zero-out
        if (std::fabs(e) < 1e-6f) e = 0.0f;
        eligibility_.store(clampf(e, -eligibility_cap_, eligibility_cap_), std::memory_order_relaxed);
    }

    void Synapse::accumulateEligibility(float pre_act, float post_act, float scale) noexcept {
        // Simple co-activation product scaled
        float delta = scale * pre_act * post_act;
        float e = eligibility_.load(std::memory_order_relaxed);
        e += delta;
        eligibility_.store(clampf(e, -eligibility_cap_, eligibility_cap_), std::memory_order_relaxed);
    }

    } // namespace Core
} // namespace NeuroForge