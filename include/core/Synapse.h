#pragma once

#include "Types.h"
#include <cstdint>
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>

namespace NeuroForge {
    namespace Core {

        // Forward declarations
        class Neuron;

        /**
         * @brief Core Synapse class for the NeuroForge hypergraph brain
         * 
         * Represents connections between neurons with:
         * - Adjustable synaptic weights
         * - Plasticity mechanisms (Hebbian learning, STDP)
         * - Signal propagation with delays
         * - Memory-efficient sparse representation
         */
        class Synapse {
        public:

            // Use the global SynapseType from Types.h
            using Type = NeuroForge::SynapseType;

            /**
             * @brief Virtual Synapse for procedural connectivity (1B+ neuron scale)
             */
            struct VirtualSynapse {
                static float weight(uint64_t pre, uint64_t post, uint64_t seed = 0x517cc1e6) {
                    // Simple PCG-like hash for deterministic weights
                    uint64_t state = pre ^ post ^ seed;
                    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
                    state ^= state >> 22;
                    return (state & 0xFFFFFF) / float(0xFFFFFF) * 2.0f - 1.0f; // ±1.0 initial
                }

                static float delay(uint64_t pre, uint64_t post, uint64_t seed = 0x517cc1e6) {
                    uint64_t state = pre ^ post ^ (seed + 1);
                    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
                    state ^= state >> 22;
                    return 1.0f + ((state & 0xFF) / 255.0f) * 4.0f; // 1ms - 5ms delay
                }

                static bool exists(uint64_t pre, uint64_t post, float p = 0.01f, uint64_t seed = 0x517cc1e6) {
                    // WyHash-like mixing for existence check
                    uint64_t h = pre ^ post ^ seed;
                    h ^= h >> 33;
                    h *= 0xff51afd7ed558ccdULL;
                    h ^= h >> 33;
                    h *= 0xc4ceb9fe1a85ec53ULL;
                    h ^= h >> 33;
                    return h < (p * 18446744073709551615ULL);
                }
            };

            /**
             * @brief Plasticity rules for synaptic weight adaptation
             */
            enum class PlasticityRule {
                None = 0,           ///< No plasticity
                Hebbian = 1,        ///< Basic Hebbian learning
                STDP = 2,           ///< Spike-timing dependent plasticity
                BCM = 3,            ///< Bienenstock-Cooper-Munro rule
                Oja = 4             ///< Oja's rule for normalization
            };

            /**
             * @brief Constructor
             * @param id Unique synapse identifier
             * @param source Source neuron (weak pointer)
             * @param target Target neuron (weak pointer)
             * @param initial_weight Initial synaptic weight
             * @param type Synapse type (excitatory/inhibitory/modulatory)
             */
            Synapse(NeuroForge::SynapseID id, 
                   NeuroForge::NeuronWeakPtr source, 
                   NeuroForge::NeuronWeakPtr target,
                   NeuroForge::Weight initial_weight = 0.1f,
                   NeuroForge::SynapseType type = NeuroForge::SynapseType::Excitatory);

            /**
             * @brief Destructor
             */
            ~Synapse() = default;

            // Copy and move semantics
            Synapse(const Synapse&) = delete;
            Synapse& operator=(const Synapse&) = delete;
            Synapse(Synapse&&) = default;
            Synapse& operator=(Synapse&&) = default;

            /**
             * @brief Get unique synapse ID
             * @return SynapseID
             */
            NeuroForge::SynapseID getId() const noexcept { return id_; }

            /**
             * @brief Get current synaptic weight
             * @return Current weight value
             */
            NeuroForge::Weight getWeight() const noexcept;

            /**
             * @brief Set synaptic weight
             * @param weight New weight value
             */
            void setWeight(NeuroForge::Weight weight) noexcept;

            /**
             * @brief Get synapse type
             * @return Synapse type
             */
            NeuroForge::SynapseType getType() const noexcept { return type_; }

            /**
             * @brief Set synapse type
             * @param type New synapse type
             */
            void setType(NeuroForge::SynapseType type) noexcept { type_ = type; }

            /**
             * @brief Get plasticity rule
             * @return Current plasticity rule
             */
            PlasticityRule getPlasticityRule() const noexcept { return plasticity_rule_; }

            /**
             * @brief Set plasticity rule
             * @param rule New plasticity rule
             */
            void setPlasticityRule(PlasticityRule rule) noexcept { plasticity_rule_ = rule; }

            /**
             * @brief Get learning rate for plasticity
             * @return Learning rate
             */
            float getLearningRate() const noexcept { return learning_rate_; }

            /**
             * @brief Set learning rate for plasticity
             * @param rate New learning rate
             */
            void setLearningRate(float rate) noexcept { learning_rate_ = rate; }

            /**
             * @brief Get propagation delay
             * @return Delay in milliseconds
             */
            float getDelay() const noexcept { return delay_ms_; }

            /**
             * @brief Set propagation delay
             * @param delay_ms Delay in milliseconds
             */
            void setDelay(float delay_ms) noexcept { delay_ms_ = delay_ms; }

            /**
             * @brief Get source neuron
             * @return Weak pointer to source neuron
             */
            NeuroForge::NeuronWeakPtr getSource() const noexcept { return source_; }

            /**
             * @brief Get target neuron
             * @return Weak pointer to target neuron
             */
            NeuroForge::NeuronWeakPtr getTarget() const noexcept { return target_; }

            /**
             * @brief Check if synapse is valid (both neurons exist)
             * @return True if both source and target neurons are valid
             */
            bool isValid() const noexcept;

            /**
             * @brief Propagate signal from source to target
             * @param signal_strength Signal strength from source neuron
             */
            void propagateSignal(float signal_strength);

            /**
             * @brief Get weighted input for target neuron
             * @return Weighted signal value
             */
            float getWeightedInput() const noexcept;

            /**
             * @brief Update synaptic weight based on plasticity rule
             * @param pre_activation Pre-synaptic activation
             * @param post_activation Post-synaptic activation
             * @param delta_time Time step
             */
            void updateWeight(float pre_activation, float post_activation, float delta_time);

            /**
             * @brief Apply Hebbian learning rule
             * @param pre_activation Pre-synaptic activation
             * @param post_activation Post-synaptic activation
             * @param delta_time Time step
             */
            void applyHebbianLearning(float pre_activation, float post_activation, float delta_time);

            /**
             * @brief Apply STDP (Spike-Timing Dependent Plasticity)
             * @param pre_spike_time Pre-synaptic spike time
             * @param post_spike_time Post-synaptic spike time
             */
            void applySTDP(NeuroForge::TimePoint pre_spike_time, NeuroForge::TimePoint post_spike_time);

            /**
             * @brief Reset synapse to initial state
             */
            void reset();

            /**
             * @brief Get memory usage of this synapse
             * @return Memory usage in bytes
             */
            std::size_t getMemoryUsage() const noexcept;

            /**
             * @brief Get minimum allowed weight
             * @return Minimum weight value
             */
            NeuroForge::Weight getMinWeight() const noexcept { return min_weight_; }

            /**
             * @brief Get maximum allowed weight
             * @return Maximum weight value
             */
            NeuroForge::Weight getMaxWeight() const noexcept { return max_weight_; }

            /**
             * @brief Set weight bounds
             * @param min_weight Minimum allowed weight
             * @param max_weight Maximum allowed weight
             */
            void setWeightBounds(NeuroForge::Weight min_weight, NeuroForge::Weight max_weight) noexcept {
                min_weight_ = min_weight;
                max_weight_ = max_weight;
            }

            /**
             * @brief Public wrapper to apply safety guardrails to a proposed weight change.
             *        Useful for accelerated update paths to preserve CPU semantics.
             * @param delta_w Proposed weight change
             * @return Guardrail-adjusted, safe weight change
             */
            float applySafetyGuardrailsPublic(float delta_w) const noexcept { return applySafetyGuardrails(delta_w); }

            /**
             * @brief Get synapse statistics
             * @return Struct containing usage statistics
             */
            struct Statistics {
                std::uint64_t signal_count;     ///< Number of signals propagated
                std::uint64_t update_count;     ///< Number of weight updates
                NeuroForge::Weight min_weight;  ///< Minimum weight reached
                NeuroForge::Weight max_weight;  ///< Maximum weight reached
                NeuroForge::Weight avg_weight;  ///< Average weight over time
            };

            Statistics getStatistics() const noexcept;

            // --- R-STDP-lite eligibility API ---
            float getEligibility() const noexcept { return eligibility_.load(std::memory_order_relaxed); }
            void setEligibility(float e) noexcept { eligibility_.store(e, std::memory_order_relaxed); }
            void decayEligibility(float decay_rate, float dt) noexcept;
            void accumulateEligibility(float pre_activation, float post_activation, float scale = 1.0f) noexcept;

        private:
            NeuroForge::SynapseID id_;                  ///< Unique synapse identifier
            NeuroForge::NeuronWeakPtr source_;          ///< Source neuron (weak reference)
            NeuroForge::NeuronWeakPtr target_;          ///< Target neuron (weak reference)
            
            std::atomic<NeuroForge::Weight> weight_;    ///< Synaptic weight (thread-safe)
            NeuroForge::Weight initial_weight_;         ///< Initial weight for reset
            NeuroForge::SynapseType type_;              ///< Synapse type
            
            PlasticityRule plasticity_rule_;           ///< Current plasticity rule
            float learning_rate_;                       ///< Learning rate for plasticity
            float delay_ms_;                            ///< Propagation delay in milliseconds
            
            // Signal buffering for delayed propagation
            struct DelayedSignal {
                float strength;
                NeuroForge::TimePoint delivery_time;
            };
            
            mutable std::vector<DelayedSignal> signal_buffer_;  ///< Buffer for delayed signals
            mutable std::mutex signal_buffer_mutex_;            ///< Mutex to protect signal_buffer_ access
            
            // Weight bounds
            NeuroForge::Weight min_weight_;             ///< Minimum allowed weight
            NeuroForge::Weight max_weight_;             ///< Maximum allowed weight
            
            // Statistics tracking
            mutable std::atomic<std::uint64_t> signal_count_;           ///< Signal propagation count
            mutable std::atomic<std::uint64_t> update_count_;           ///< Weight update count
            mutable std::atomic<NeuroForge::Weight> weight_sum_;        ///< Sum for average calculation
            mutable std::atomic<NeuroForge::Weight> min_recorded_weight_; ///< Minimum recorded weight
            mutable std::atomic<NeuroForge::Weight> max_recorded_weight_; ///< Maximum recorded weight

            // --- R-STDP-lite state ---
            std::atomic<float> eligibility_{0.0f};      ///< Eligibility trace e_ij
            float eligibility_decay_rate_{0.1f};        ///< Default decay rate per second
            float eligibility_cap_{1.0f};               ///< Clamp magnitude of eligibility
            
            // --- Safety guardrails ---
            static constexpr float MAX_GRADIENT_MAGNITUDE = 0.5f;    ///< Maximum allowed gradient magnitude
            static constexpr float MAX_WEIGHT_CHANGE_PER_STEP = 0.1f; ///< Maximum weight change per update
            static constexpr float STABILITY_EPSILON = 1e-8f;        ///< Small value to prevent division by zero
            mutable std::atomic<std::uint32_t> consecutive_large_updates_{0}; ///< Count of consecutive large updates
            static constexpr std::uint32_t MAX_CONSECUTIVE_LARGE_UPDATES = 10; ///< Threshold for instability detection
            
            /**
             * @brief Process delayed signals
             */
            void processDelayedSignals() const;
            
            /**
             * @brief Update statistics
             */
            void updateStatistics() const noexcept;
            
            /**
             * @brief Apply safety guardrails to weight change
             * @param delta_w Proposed weight change
             * @return Clipped and safe weight change
             */
            float applySafetyGuardrails(float delta_w) const noexcept;
            
            /**
             * @brief Check for training instability
             * @param delta_w Weight change magnitude
             * @return True if system appears unstable
             */
            bool checkInstability(float delta_w) const noexcept;
        };

        /**
         * @brief Synapse factory for efficient creation and ID management
         */
        class SynapseFactory {
        public:
            /**
             * @brief Create a new synapse with auto-generated ID
             * @param source Source neuron
             * @param target Target neuron
             * @param weight Initial weight
             * @param type Synapse type
             * @return Shared pointer to synapse
             */
            static NeuroForge::SynapsePtr createSynapse(
                NeuroForge::NeuronWeakPtr source,
                NeuroForge::NeuronWeakPtr target,
                NeuroForge::Weight weight = 0.1f,
                NeuroForge::SynapseType type = NeuroForge::SynapseType::Excitatory);

            /**
             * @brief Create a synapse with specific ID
             * @param id Specific synapse ID
             * @param source Source neuron
             * @param target Target neuron
             * @param weight Initial weight
             * @param type Synapse type
             * @return Shared pointer to synapse
             */
            static NeuroForge::SynapsePtr createSynapse(
                NeuroForge::SynapseID id,
                NeuroForge::NeuronWeakPtr source,
                NeuroForge::NeuronWeakPtr target,
                NeuroForge::Weight weight = 0.1f,
                NeuroForge::SynapseType type = NeuroForge::SynapseType::Excitatory);

            /**
             * @brief Get next available synapse ID
             * @return Next ID
             */
            static NeuroForge::SynapseID getNextId();

            /**
             * @brief Reset ID counter (for testing)
             */
            static void resetIdCounter();

        private:
            static std::atomic<NeuroForge::SynapseID> next_id_;    ///< Next available ID
        };

    } // namespace Core
} // namespace NeuroForge
