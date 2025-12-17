#include "core/Neuron.h"
#include "core/Synapse.h"
#include <algorithm>
#include <cmath>
#include <mutex>

namespace NeuroForge {
    namespace Core {

        std::atomic<NeuronID> NeuronFactory::next_id_{1};
        // Define static spike callback
        Neuron::SpikeCallback Neuron::spike_callback_ = nullptr;

        Neuron::Neuron(NeuronID id, ActivationValue threshold)
            : id_(id)
            , activation_(0.0f)
            , state_(State::Inactive)
            , threshold_(threshold)
            , decay_rate_(0.01f)
            , refractory_timer_(0.0f)
            , refractory_period_(1.0f)
            , fire_count_(0)
            , process_count_(0) {
        }

        Neuron::ActivationValue Neuron::getActivation() const noexcept {
            return activation_.load(std::memory_order_relaxed);
        }

        void Neuron::setActivation(ActivationValue value) noexcept {
            activation_.store(value, std::memory_order_relaxed);
        }

        Neuron::State Neuron::getState() const noexcept {
            return state_.load(std::memory_order_relaxed);
        }

        void Neuron::setState(State state) noexcept {
            state_.store(state, std::memory_order_relaxed);
        }

        void Neuron::addInputSynapse(SynapsePtr synapse) {
            if (!synapse) return;
            std::lock_guard<std::mutex> lock(synapse_mutex_);
            input_synapses_.push_back(synapse);
        }

        void Neuron::addOutputSynapse(SynapsePtr synapse) {
            if (!synapse) return;
            std::lock_guard<std::mutex> lock(synapse_mutex_);
            output_synapses_.push_back(synapse);
        }

        void Neuron::removeInputSynapse(SynapsePtr synapse) {
            std::lock_guard<std::mutex> lock(synapse_mutex_);
            input_synapses_.erase(std::remove(input_synapses_.begin(), input_synapses_.end(), synapse), input_synapses_.end());
        }

        void Neuron::removeOutputSynapse(SynapsePtr synapse) {
            std::lock_guard<std::mutex> lock(synapse_mutex_);
            output_synapses_.erase(std::remove(output_synapses_.begin(), output_synapses_.end(), synapse), output_synapses_.end());
        }

        void Neuron::reserveInputSynapses(std::size_t capacity) {
            std::lock_guard<std::mutex> lock(synapse_mutex_);
            if (input_synapses_.capacity() < capacity) {
                input_synapses_.reserve(capacity);
            }
        }

        void Neuron::reserveOutputSynapses(std::size_t capacity) {
            std::lock_guard<std::mutex> lock(synapse_mutex_);
            if (output_synapses_.capacity() < capacity) {
                output_synapses_.reserve(capacity);
            }
        }

        void Neuron::process(float delta_time) {
            process_count_.fetch_add(1, std::memory_order_relaxed);
            
            // Handle refractory period
            if (getState() == State::Refractory) {
                refractory_timer_ -= delta_time;
                if (refractory_timer_ <= 0.0f) {
                    setState(State::Inactive);
                    refractory_timer_ = 0.0f;
                }
                return;
            }
            
            auto current_state = getState();
            auto current_activation = getActivation();
            
            // Collect inputs with try_lock on synapse_mutex_ to avoid deadlocks
            std::vector<NeuroForge::SynapsePtr> input_synapses_copy;
            {
                std::unique_lock<std::mutex> lock(synapse_mutex_, std::try_to_lock);
                if (!lock.owns_lock()) {
                    // Can't get lock, skip this processing cycle to avoid deadlock
                    return;
                }
                input_synapses_copy.reserve(input_synapses_.size());
                for (const auto& synapse : input_synapses_) {
                    input_synapses_copy.push_back(synapse);
                }
            }
            
            float input_sum = 0.0f;
            for (const auto& synapse : input_synapses_copy) {
                if (synapse) {
                    input_sum += synapse->getWeightedInput();
                }
            }

            // Apply activation function (sigmoid-like)
            float new_activation = current_activation + input_sum * delta_time;
            
            // Apply decay
            new_activation *= (1.0f - decay_rate_ * delta_time);
            
            // Clamp activation between 0 and 1
            new_activation = std::max(0.0f, std::min(1.0f, new_activation));
            
            setActivation(new_activation);

            // Check for firing
            if (new_activation >= threshold_ && current_state != State::Active) {
                setState(State::Active);
                fire_count_.fetch_add(1, std::memory_order_relaxed);

                // Notify spike to global callback if set
                if (spike_callback_) {
                    spike_callback_(id_);
                }
                
                // Propagate signal through output synapses
                // Use try_lock to avoid deadlock - if we can't get the lock, skip signal propagation
                std::vector<NeuroForge::SynapsePtr> output_synapses_copy;
                {
                    std::unique_lock<std::mutex> lock(synapse_mutex_, std::try_to_lock);
                    if (!lock.owns_lock()) {
                        // Can't get lock, skip signal propagation to avoid deadlock
                        setState(State::Refractory);
                        refractory_timer_ = refractory_period_;
                        return;
                    }
                    output_synapses_copy.reserve(output_synapses_.size());
                    for (const auto& synapse : output_synapses_) {
                        output_synapses_copy.push_back(synapse);
                    }
                }
                
                for (const auto& synapse : output_synapses_copy) {
                    if (synapse) {
                        synapse->propagateSignal(new_activation);
                    }
                }
                
                // Enter refractory period
                setState(State::Refractory);
                refractory_timer_ = refractory_period_;
                
            } else if (new_activation < threshold_ && current_state == State::Active) {
                setState(State::Inactive);
            }
        }

        void Neuron::reset() {
            setActivation(0.0f);
            setState(State::Inactive);
            refractory_timer_ = 0.0f;
            fire_count_.store(0, std::memory_order_relaxed);
            process_count_.store(0, std::memory_order_relaxed);
        }

        bool Neuron::isFiring() const noexcept {
            return getActivation() >= threshold_ && getState() == State::Active;
        }

        std::vector<SynapsePtr> Neuron::getInputSynapses() const {
            std::lock_guard<std::mutex> lock(synapse_mutex_);
            return input_synapses_;
        }

        std::vector<SynapsePtr> Neuron::getOutputSynapses() const {
            std::lock_guard<std::mutex> lock(synapse_mutex_);
            return output_synapses_;
        }

        std::size_t Neuron::getMemoryUsage() const noexcept {
            // Rough estimate of memory usage
            std::size_t size = sizeof(*this);
            {
                std::lock_guard<std::mutex> lock(synapse_mutex_);
                size += input_synapses_.capacity() * sizeof(SynapsePtr);
                size += output_synapses_.capacity() * sizeof(SynapsePtr);
            }
            return size;
        }

        std::unique_ptr<Neuron> NeuronFactory::createNeuron(Neuron::ActivationValue threshold) {
            return std::make_unique<Neuron>(next_id_.fetch_add(1, std::memory_order_relaxed), threshold);
        }

        std::unique_ptr<Neuron> NeuronFactory::createNeuron(NeuronID id, Neuron::ActivationValue threshold) {
            // Ensure factory's next_id_ advances beyond explicitly assigned IDs to avoid collisions
            auto current_next = next_id_.load(std::memory_order_relaxed);
            while (id >= current_next) {
                next_id_.compare_exchange_weak(current_next, id + 1, std::memory_order_relaxed);
                current_next = next_id_.load(std::memory_order_relaxed);
            }
            return std::make_unique<Neuron>(id, threshold);
        }

        NeuronID NeuronFactory::getNextId() {
            return next_id_.load(std::memory_order_relaxed);
        }

        void NeuronFactory::resetIdCounter() {
            next_id_.store(1, std::memory_order_relaxed);
        }

    } // namespace Core
} // namespace NeuroForge