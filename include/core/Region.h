#pragma once

#include "Types.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>

namespace NeuroForge {
    namespace Core {

        // Forward declarations
        class HypergraphBrain;
        class Region;
        class MemoryDB;

        // RegionPtr and RegionWeakPtr are now defined in Types.h

        /**
         * @brief Base class for all brain regions in the hypergraph architecture
         * 
         * Manages collections of neurons, their connectivity patterns, and region-specific
         * processing logic. Designed to handle millions of neurons per region efficiently.
         */
        class Region {
        public:
            using RegionID = NeuroForge::RegionID;
            using NeuronContainer = std::vector<NeuroForge::NeuronPtr>;
            using SynapseContainer = std::vector<NeuroForge::SynapsePtr>;
            using ConnectionMap = std::unordered_map<NeuroForge::NeuronID, std::vector<NeuroForge::SynapsePtr>>;
            using RegionConnections = std::unordered_map<NeuroForge::RegionID, std::vector<NeuroForge::SynapsePtr>>;
            using ProcessingFunction = std::function<void(Region&, float)>;

            /**
             * @brief Region types for different brain areas
             */
            enum class Type {
                Cortical,       // Cortical regions (visual, auditory, motor, etc.)
                Subcortical,    // Subcortical structures (hippocampus, amygdala, etc.)
                Brainstem,      // Brainstem regions (medulla, pons, etc.)
                Special,        // Special regions (self-node, hardware interface, etc.)
                Custom          // User-defined regions
            };

            /**
             * @brief Region activation patterns
             */
            enum class ActivationPattern {
                Synchronous,    // All neurons process simultaneously
                Asynchronous,   // Neurons process independently
                Layered,        // Layer-by-layer processing
                Competitive,    // Winner-take-all dynamics
                Oscillatory     // Rhythmic activation patterns
            };

            /**
             * @brief Region statistics for monitoring and analysis
             */
            struct Statistics {
                std::size_t neuron_count = 0;
                std::size_t synapse_count = 0;
                std::size_t active_neurons = 0;
                float average_activation = 0.0f;
                float total_energy = 0.0f;
                std::size_t memory_usage = 0;
                std::chrono::milliseconds processing_time{0};
                float avg_mitochondrial_energy = 0.0f;
                float avg_mitochondrial_health = 0.0f;
                float metabolic_stress = 0.0f;
            };

            /**
             * @brief Mitochondrial state for metabolic simulation
             */
            struct MitochondrialState {
                float energy = 0.85f;        // [0-1] current ATP analog
                float health = 1.0f;         // [0-1] long-term capacity (slow)
                float production_rate = 0.002f;
                float base_consumption = 0.0001f;
            };

        protected:
            NeuroForge::RegionID id_;
            std::string name_;
            Type type_;
            ActivationPattern activation_pattern_;
            
            // Neuron and synapse management
            NeuronContainer neurons_;
            // Snapshot for lock-free read access during process()
            std::shared_ptr<const NeuronContainer> neurons_snapshot_;

            // Mitochondrial state (parallel to neurons_)
            std::vector<MitochondrialState> mito_states_;
            
            SynapseContainer internal_synapses_;  // Synapses within this region
            ConnectionMap input_connections_;     // Incoming connections by neuron ID
            ConnectionMap output_connections_;    // Outgoing connections by neuron ID
            RegionConnections inter_region_connections_; // Connections to other regions
            
            // Processing and state
            std::atomic<bool> is_active_{false};
            std::atomic<float> global_activation_{0.0f};
            ProcessingFunction custom_processor_;
            
            // Thread safety
            mutable std::mutex region_mutex_;
            mutable std::mutex connection_mutex_;
            
            // Statistics and monitoring
            mutable Statistics stats_;
            std::atomic<std::uint64_t> processing_cycles_{0};

        public:
            /**
             * @brief Construct a new Region
             * 
             * @param id Unique region identifier
             * @param name Human-readable region name
             * @param type Region type classification
             * @param pattern Activation pattern for this region
             */
            Region(RegionID id, 
                   const std::string& name, 
                   Type type = Type::Custom,
                   ActivationPattern pattern = ActivationPattern::Asynchronous);

            virtual ~Region() = default;

            // Basic properties
            RegionID getId() const noexcept { return id_; }
            const std::string& getName() const noexcept { return name_; }
            Type getType() const noexcept { return type_; }
            ActivationPattern getActivationPattern() const noexcept { return activation_pattern_; }
            
            void setActivationPattern(ActivationPattern pattern) noexcept { activation_pattern_ = pattern; }
            void setCustomProcessor(ProcessingFunction processor) { custom_processor_ = std::move(processor); }

            // Neuron management
            /**
             * @brief Add a neuron to this region
             * @param neuron Shared pointer to the neuron
             * @return True if successfully added
             */
            bool addNeuron(NeuroForge::NeuronPtr neuron);

            /**
             * @brief Remove a neuron from this region
             * @param neuron_id ID of the neuron to remove
             * @return True if successfully removed
             */
            bool removeNeuron(NeuroForge::NeuronID neuron_id);

            /**
             * @brief Get neuron by ID
             * @param neuron_id ID of the neuron
             * @return Shared pointer to neuron, or nullptr if not found
             */
            NeuroForge::NeuronPtr getNeuron(NeuroForge::NeuronID neuron_id) const;

            /**
             * @brief Get all neurons in this region
             * @return Const reference to neuron container
             */
            const NeuronContainer& getNeurons() const noexcept { return neurons_; }

            /**
             * @brief Get number of neurons in this region
             * @return Number of neurons
             */
            std::size_t getNeuronCount() const noexcept { return neurons_.size(); }

            /**
             * @brief Create and add multiple neurons to this region
             * @param count Number of neurons to create
             * @return Vector of created neuron pointers
             */
            std::vector<NeuroForge::NeuronPtr> createNeurons(std::size_t count);

            /**
             * @brief Structurally spawn neurons when metabolic state permits
             * @param count Number of neurons to attempt to spawn
             * @param energy_gate Minimum average mitochondrial energy required [0,1]
             * @return Vector of created neuron pointers (empty if gated off)
             */
            std::vector<NeuroForge::NeuronPtr> spawnNeurons(std::size_t count, float energy_gate = 0.5f);

            /**
             * @brief Prune weak internal synapses below threshold
             * @param weight_threshold Absolute weight threshold for pruning
             * @return Number of pruned synapses
             */
            std::size_t pruneWeakSynapses(float weight_threshold);

            /**
             * @brief Grow new internal synapses among active neurons
             * @param max_new Maximum number of new synapses to create
             * @param min_activation Minimum activation to consider a neuron active [0,1]
             * @param initial_weight Initial weight for new synapses
             * @param type Synapse type for new connections
             * @return Number of grown synapses
             */
            std::size_t growSynapses(std::size_t max_new,
                                     float min_activation = 0.6f,
                                     NeuroForge::Weight initial_weight = 0.05f,
                                     NeuroForge::SynapseType type = NeuroForge::SynapseType::Excitatory);

            // Synapse and connectivity management
            /**
             * @brief Add an internal synapse (within this region)
             * @param synapse Shared pointer to the synapse
             * @return True if successfully added
             */
            bool addInternalSynapse(NeuroForge::SynapsePtr synapse);

            /**
             * @brief Connect two neurons within this region
             * @param source_id Source neuron ID
             * @param target_id Target neuron ID
             * @param weight Synapse weight
             * @param type Synapse type
             * @return Shared pointer to created synapse, or nullptr if failed
             */
            NeuroForge::SynapsePtr connectNeurons(NeuroForge::NeuronID source_id, 
                                                 NeuroForge::NeuronID target_id,
                                                 NeuroForge::Weight weight = 0.5f,
                                                 NeuroForge::SynapseType type = NeuroForge::SynapseType::Excitatory);

            /**
             * @brief Connect this region to another region
             * @param target_region Target region
             * @param source_neuron_id Source neuron in this region
             * @param target_neuron_id Target neuron in target region
             * @param weight Synapse weight
             * @param type Synapse type
             * @return Shared pointer to created synapse, or nullptr if failed
             */
            NeuroForge::SynapsePtr connectToRegion(NeuroForge::RegionPtr target_region,
                                                  NeuroForge::NeuronID source_neuron_id,
                                                  NeuroForge::NeuronID target_neuron_id,
                                                  NeuroForge::Weight weight = 0.5f,
                                                  NeuroForge::SynapseType type = NeuroForge::SynapseType::Excitatory);

            /**
             * @brief Connect this region to another region with an explicit Synapse ID
             * @param target_region Target region
             * @param source_neuron_id Source neuron in this region
             * @param target_neuron_id Target neuron in target region
             * @param weight Synapse weight
             * @param type Synapse type
             * @param explicit_id Explicit synapse ID to assign
             * @return Shared pointer to created synapse, or nullptr if failed
             */
            NeuroForge::SynapsePtr connectNeurons(NeuroForge::NeuronID source_id,
                                                 NeuroForge::NeuronID target_id,
                                                 NeuroForge::Weight weight,
                                                 NeuroForge::SynapseType type,
                                                 NeuroForge::SynapseID explicit_id);

            /**
             * @brief Connect this region to another region with an explicit Synapse ID
             * @param target_region Target region
             * @param source_neuron_id Source neuron in this region
             * @param target_neuron_id Target neuron in target region
             * @param weight Synapse weight
             * @param type Synapse type
             * @param explicit_id Explicit synapse ID to assign
             * @return Shared pointer to created synapse, or nullptr if failed
             */
            NeuroForge::SynapsePtr connectToRegion(NeuroForge::RegionPtr target_region,
                                                  NeuroForge::NeuronID source_neuron_id,
                                                  NeuroForge::NeuronID target_neuron_id,
                                                  NeuroForge::Weight weight,
                                                  NeuroForge::SynapseType type,
                                                  NeuroForge::SynapseID explicit_id);

            /**
             * @brief Get all synapses in this region
             * @return Const reference to synapse container
             */
            const SynapseContainer& getInternalSynapses() const noexcept { return internal_synapses_; }

            /**
             * @brief Get a specific synapse by ID
             * @param synapse_id ID of the synapse to find
             * @return Shared pointer to synapse, or nullptr if not found
             */
            NeuroForge::SynapsePtr getSynapse(NeuroForge::SynapseID synapse_id) const;

            // New public connection getters for lock-free read access
            const ConnectionMap& getInputConnections() const noexcept { return input_connections_; }
            const ConnectionMap& getOutputConnections() const noexcept { return output_connections_; }
            const RegionConnections& getInterRegionConnections() const noexcept { return inter_region_connections_; }
            
            // Capacity reservation helpers to reduce reallocations during bulk connection creation
            void reserveInterRegionConnections(NeuroForge::RegionID target_region_id, std::size_t additional);
            void reserveInputConnections(NeuroForge::NeuronID target_neuron_id, std::size_t additional);
            void reserveOutputConnections(NeuroForge::NeuronID source_neuron_id, std::size_t additional);
            // Processing and simulation
            /**
             * @brief Process one simulation step for this region
             * @param delta_time Time step in seconds
             */
            virtual void process(float delta_time);

            /**
             * @brief Initialize the region (called once before simulation)
             */
            virtual void initialize();

            /**
             * @brief Reset the region to initial state
             */
            virtual void reset();

            // Substrate hooks (default minimal implementations)
            /**
             * @brief Feed an external activation pattern into the region's neurons.
             * Values are clamped to [0,1]. If the pattern is shorter than the neuron count,
             * remaining neurons keep their current activation.
             */
            virtual void feedExternalPattern(const std::vector<float>& pattern);

            /**
             * @brief Read out the current neuron activation vector from this region.
             * Fills 'out' with size equal to neuron count, using 0.0f for null entries.
             */
            virtual void readoutVector(std::vector<float>& out) const;

            /**
             * @brief Apply a neuromodulator level to this region (e.g., dopamine/serotonin proxy).
             * Default behavior gently biases activations by a small factor of level in [-1,1].
             */
            virtual void applyNeuromodulator(float level);


            /**
             * @brief Attach a MemoryDB to this region for persistence/logging.
             * Default implementation is a no-op.
             */
            virtual void setMemoryDB(std::shared_ptr<MemoryDB> db, std::int64_t run_id) { (void)db; (void)run_id; }

            /**
             * @brief Activate or deactivate this region
             * @param active New activation state
             */
            void setActive(bool active) noexcept { is_active_.store(active, std::memory_order_relaxed); }

            /**
             * @brief Check if region is currently active
             * @return True if active
             */
            bool isActive() const noexcept { return is_active_.load(std::memory_order_relaxed); }

            /**
             * @brief Get global activation level for the region
             * @return Global activation value
             */
            float getGlobalActivation() const noexcept { return global_activation_.load(std::memory_order_relaxed); }

            /**
             * @brief Set a custom processing function to be called during process()
             * @param processor Function that takes (Region&, delta_time)
             */
            void setProcessingFunction(ProcessingFunction processor) { custom_processor_ = std::move(processor); }

            /**
             * @brief Get region statistics
             * @return Statistics struct with various metrics
             */
            Statistics getStatistics() const;

            /**
             * @brief Get memory usage for this region
             * @return Approximate memory usage in bytes
             */
            std::size_t getMemoryUsage() const;

            /**
             * @brief Get total processing cycles for this region
             * @return Number of processing cycles
             */
            std::uint64_t getProcessingCycles() const noexcept {
                return processing_cycles_.load(std::memory_order_relaxed);
            }

            /**
             * @brief Get type as string
             * @return String representation of the region type
             */
            std::string getTypeString() const;

            /**
             * @brief Get activation pattern as string
             * @return String representation of the activation pattern
             */
            std::string getActivationPatternString() const;

        protected:
            /**
             * @brief Update internal statistics (thread-safe)
             */
            void updateStatistics() const;

            /**
             * @brief Process neurons (potentially heavy operation)
             */
            void processNeurons(float delta_time);

            /**
             * @brief Process neurons from a provided copy to avoid locking
             */
            void processNeuronsFromCopy(const NeuronContainer& neurons_copy, float delta_time);

            /**
             * @brief SIMD-optimized processing for layered activation patterns
             */
            void processLayeredWithSIMD(const NeuronContainer& neurons_copy, float delta_time);

            /**
             * @brief SIMD-optimized processing for oscillatory activation patterns
             */
            void processOscillatoryWithSIMD(const NeuronContainer& neurons_copy, float delta_time);

            /**
             * @brief Region-specific processing hook overridden by derived regions
             * Default implementation is a no-op.
             */
            virtual void processRegionSpecific(float delta_time) { (void)delta_time; }

            /**
             * @brief Calculate global activation across neurons
             */
            float calculateGlobalActivation() const;

            /**
             * @brief Update the neurons snapshot (called under region_mutex_)
             */
            void updateSnapshot();
        };

        /**
         * @brief Factory class for creating regions with unique IDs
         */
        class RegionFactory {
        private:
            static std::atomic<NeuroForge::RegionID> next_id_;

        public:
            /**
             * @brief Create a new region with auto-generated ID
             */
            static NeuroForge::RegionPtr createRegion(const std::string& name,
                                          Region::Type type = Region::Type::Custom,
                                          Region::ActivationPattern pattern = Region::ActivationPattern::Asynchronous);

            /**
             * @brief Create a new region with an explicit ID
             */
            static NeuroForge::RegionPtr createRegion(NeuroForge::RegionID id,
                                          const std::string& name,
                                          Region::Type type = Region::Type::Custom,
                                          Region::ActivationPattern pattern = Region::ActivationPattern::Asynchronous);

            /**
             * @brief Get next available region ID
             */
            static NeuroForge::RegionID getNextId();

            /**
             * @brief Reset region ID counter (use with caution)
             */
            static void resetIdCounter();
        };

    } // namespace Core
} // namespace NeuroForge
