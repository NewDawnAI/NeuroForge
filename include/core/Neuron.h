#pragma once

#include "Types.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>
#include <unordered_set>
#include <mutex>
#include <functional>

namespace NeuroForge {
    namespace Core {

        // Forward declarations
        class Synapse;
        class Region;

        /**
         * @brief Core Neuron class for the NeuroForge hypergraph brain
         * 
         * Designed to handle billions of neurons efficiently with:
         * - Unique 64-bit IDs for massive scale
         * - Atomic operations for thread safety
         * - Sparse connectivity representation
         * - Memory-efficient activation tracking
         */
        class Neuron {
        public:
            using ActivationValue = float;
            using SpikeCallback = std::function<void(NeuroForge::NeuronID)>;

            /**
             * @brief Neuron activation states
             */
            enum class State {
                Inactive = 0,
                Active = 1,
                Inhibited = 2,
                Refractory = 3
            };

            /**
             * @brief Constructor
             * @param id Unique neuron identifier
             * @param threshold Activation threshold
             */
            explicit Neuron(NeuroForge::NeuronID id, ActivationValue threshold = 0.5f);

            /**
             * @brief Destructor
             */
            ~Neuron() = default;

            // Copy and move semantics
            Neuron(const Neuron&) = delete;
            Neuron& operator=(const Neuron&) = delete;
            Neuron(Neuron&&) = default;
            Neuron& operator=(Neuron&&) = default;

            /**
             * @brief Get unique neuron ID
             * @return NeuronID
             */
            NeuroForge::NeuronID getId() const noexcept { return id_; }

            /**
             * @brief Get current activation value
             * @return Current activation level
             */
            ActivationValue getActivation() const noexcept;

            /**
             * @brief Set activation value
             * @param value New activation value
             */
            void setActivation(ActivationValue value) noexcept;

            /**
             * @brief Get current neuron state
             * @return Current state
             */
            State getState() const noexcept;

            /**
             * @brief Set neuron state
             * @param state New state
             */
            void setState(State state) noexcept;

            /**
             * @brief Get activation threshold
             * @return Threshold value
             */
            ActivationValue getThreshold() const noexcept { return threshold_; }

            /**
             * @brief Set activation threshold
             * @param threshold New threshold
             */
            void setThreshold(ActivationValue threshold) noexcept { threshold_ = threshold; }

            /**
             * @brief Get activation decay rate
             */
            float getDecayRate() const noexcept { return decay_rate_; }

            /**
             * @brief Set activation decay rate
             */
            void setDecayRate(float rate) noexcept { decay_rate_ = rate; }

            /**
             * @brief Get refractory period duration (seconds)
             */
            float getRefractoryPeriod() const noexcept { return refractory_period_; }

            /**
             * @brief Set refractory period duration (seconds)
             */
            void setRefractoryPeriod(float period) noexcept { refractory_period_ = period; }

            /**
             * @brief Get current refractory timer value (seconds remaining)
             */
            float getRefractoryTimer() const noexcept { return refractory_timer_; }

            /**
             * @brief Set refractory timer value (seconds remaining)
             */
            void setRefractoryTimer(float t) noexcept { refractory_timer_ = t; }

            /**
             * @brief Add input synapse
             * @param synapse Shared pointer to synapse
             */
            void addInputSynapse(NeuroForge::SynapsePtr synapse);

            /**
             * @brief Add output synapse
             * @param synapse Shared pointer to synapse
             */
            void addOutputSynapse(NeuroForge::SynapsePtr synapse);

            /**
             * @brief Remove input synapse
             * @param synapse Synapse to remove
             */
            void removeInputSynapse(NeuroForge::SynapsePtr synapse);

            /**
             * @brief Remove output synapse
             * @param synapse Synapse to remove
             */
            void removeOutputSynapse(NeuroForge::SynapsePtr synapse);

            /**
             * @brief Reserve capacity for input synapses to avoid reallocations
             * @param capacity Expected number of input synapses
             */
            void reserveInputSynapses(std::size_t capacity);

            /**
             * @brief Reserve capacity for output synapses to avoid reallocations
             * @param capacity Expected number of output synapses
             */
            void reserveOutputSynapses(std::size_t capacity);

            /**
             * @brief Get input synapses (thread-safe copy)
             * @return Copy of input synapses vector
             */
            std::vector<NeuroForge::SynapsePtr> getInputSynapses() const;

            /**
             * @brief Get output synapses (thread-safe copy)
             * @return Copy of output synapses vector
             */
            std::vector<NeuroForge::SynapsePtr> getOutputSynapses() const;

            // Efficient accessors when only counts are needed (avoids copying vectors)
            std::size_t getInputSynapseCount() const {
                std::lock_guard<std::mutex> lock(synapse_mutex_);
                return input_synapses_.size();
            }
            std::size_t getOutputSynapseCount() const {
                std::lock_guard<std::mutex> lock(synapse_mutex_);
                return output_synapses_.size();
            }

            /**
             * @brief Process neuron activation (compute new state based on inputs)
             * @param delta_time Time step for simulation
             */
            void process(float delta_time);

            /**
             * @brief Reset neuron to initial state
             */
            void reset();

            /**
             * @brief Check if neuron is firing (above threshold)
             * @return True if firing
             */
            bool isFiring() const noexcept;

            // Mitochondrial state accessors
            float getEnergy() const noexcept { return energy_.load(std::memory_order_relaxed); }
            void setEnergy(float e) noexcept { energy_.store(e, std::memory_order_relaxed); }
            
            float getMitoHealth() const noexcept { return mito_health_.load(std::memory_order_relaxed); }
            void setMitoHealth(float h) noexcept { mito_health_.store(h, std::memory_order_relaxed); }

            /**
             * @brief Get memory usage of this neuron
             * @return Memory usage in bytes
             */
            std::size_t getMemoryUsage() const noexcept;

            // Global spike notification hook (non-owning)
            static void setSpikeCallback(SpikeCallback cb) { spike_callback_ = std::move(cb); }

        private:
            NeuroForge::NeuronID id_;                                    ///< Unique neuron identifier
            std::atomic<ActivationValue> activation_;        ///< Current activation level (thread-safe)
            std::atomic<State> state_;                       ///< Current neuron state (thread-safe)
            ActivationValue threshold_;                      ///< Activation threshold
            ActivationValue decay_rate_;                     ///< Activation decay rate
            
            // Mitochondrial state (mirrored from Region)
            std::atomic<float> energy_{0.85f};
            std::atomic<float> mito_health_{1.0f};

            std::vector<NeuroForge::SynapsePtr> input_synapses_;         ///< Input connections
            std::vector<NeuroForge::SynapsePtr> output_synapses_;        ///< Output connections
            mutable std::mutex synapse_mutex_;                          ///< Mutex for synapse vector operations
            
            // Refractory period tracking
            float refractory_timer_;                         ///< Refractory period timer
            float refractory_period_;                        ///< Refractory period duration
            
            // Statistics (optional, for monitoring)
            std::atomic<std::uint64_t> fire_count_;          ///< Number of times neuron has fired
            std::atomic<std::uint64_t> process_count_;       ///< Number of processing cycles

            // Static callback shared by all Neuron instances
            static SpikeCallback spike_callback_;
        };

        /**
         * @brief Neuron factory for efficient creation and ID management
         */
        class NeuronFactory {
        public:
            /**
             * @brief Create a new neuron with auto-generated ID
             * @param threshold Activation threshold
             * @return Unique pointer to neuron
             */
            static std::unique_ptr<Neuron> createNeuron(Neuron::ActivationValue threshold = 0.5f);

            /**
             * @brief Create a neuron with specific ID
             * @param id Specific neuron ID
             * @param threshold Activation threshold
             * @return Unique pointer to neuron
             */
            static std::unique_ptr<Neuron> createNeuron(NeuroForge::NeuronID id, Neuron::ActivationValue threshold = 0.5f);

            /**
             * @brief Get next available neuron ID
             * @return Next ID
             */
            static NeuroForge::NeuronID getNextId();

            /**
             * @brief Reset ID counter (for testing)
             */
            static void resetIdCounter();

        private:
            static std::atomic<NeuroForge::NeuronID> next_id_;   ///< Next available ID
        };

    } // namespace Core
} // namespace NeuroForge