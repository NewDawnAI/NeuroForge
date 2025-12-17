#pragma once

#include "core/Region.h"
#include "core/Synapse.h"
#include "core/Neuron.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <random>
#include <chrono>
#include <mutex>

namespace NeuroForge {
    namespace Connectivity {

        /**
         * @brief Manages connectivity patterns and initialization between brain regions
         * 
         * The ConnectivityManager handles the complex task of establishing connections
         * between different brain regions according to neurobiologically plausible patterns.
         * It supports various connectivity types including feedforward, feedback, lateral,
         * and global connections with different probability distributions and weight patterns.
         */
        class ConnectivityManager {
        public:
            /**
             * @brief Types of connectivity patterns between regions
             */
            enum class ConnectivityType {
                Feedforward,    ///< Hierarchical forward connections (e.g., V1 -> V2)
                Feedback,       ///< Top-down feedback connections (e.g., PFC -> sensory areas)
                Lateral,        ///< Connections within the same hierarchical level
                Reciprocal,     ///< Bidirectional connections between regions
                Global,         ///< Long-range connections (e.g., thalamo-cortical)
                Sparse,         ///< Sparse random connections
                Dense,          ///< Dense local connections
                Modular         ///< Connections respecting modular organization
            };

            /**
             * @brief Connection probability distributions
             */
            enum class ProbabilityDistribution {
                Uniform,        ///< Uniform random probability
                Gaussian,       ///< Distance-dependent Gaussian
                Exponential,    ///< Exponentially decaying with distance
                PowerLaw,       ///< Power-law distribution
                SmallWorld      ///< Small-world network properties
            };

            /**
             * @brief Parameters for establishing connections between regions
             */
            struct ConnectionParameters {
                ConnectivityType type = ConnectivityType::Sparse;
                ProbabilityDistribution distribution = ProbabilityDistribution::Uniform;
                float connection_probability = 0.1f;    ///< Base connection probability
                float weight_mean = 0.5f;               ///< Mean synaptic weight
                float weight_std = 0.1f;                ///< Standard deviation of weights
                float distance_decay = 1.0f;            ///< Distance decay factor
                bool bidirectional = false;             ///< Whether to create reciprocal connections
                std::size_t max_connections_per_neuron = 1000; ///< Limit connections per neuron
                float plasticity_rate = 0.01f;          ///< Initial plasticity learning rate
                NeuroForge::Core::Synapse::PlasticityRule plasticity_rule = NeuroForge::Core::Synapse::PlasticityRule::None; ///< Initial plasticity rule
            };

            /**
             * @brief Information about an established connection between regions
             */
            struct RegionConnection {
                std::string source_region_id;
                std::string target_region_id;
                ConnectivityType type;
                std::size_t synapse_count;
                float average_weight;
                float connection_strength;
                bool is_active;
                std::chrono::system_clock::time_point creation_time;
                // Track plasticity configuration used when establishing this connection
                float plasticity_rate = 0.0f;
                NeuroForge::Core::Synapse::PlasticityRule plasticity_rule = NeuroForge::Core::Synapse::PlasticityRule::None;
            };

            /**
             * @brief Initialization patterns for different brain region types
             */
            struct InitializationPattern {
                std::string pattern_name;
                std::size_t neuron_count;
                std::vector<float> initial_activation_pattern;
                std::unordered_map<std::string, float> region_specific_parameters;
                std::function<void(RegionPtr)> custom_initializer;
            };

        public:
            ConnectivityManager();
            ~ConnectivityManager() = default;

            // ===== Region Registration =====
            
            /**
             * @brief Register a region for connectivity management
             * @param region Shared pointer to the region
             */
            void registerRegion(RegionPtr region);
            
            /**
             * @brief Unregister a region from connectivity management
             * @param region_id ID of the region to unregister
             */
            void unregisterRegion(const std::string& region_id);
            
            /**
             * @brief Get a registered region by ID
             * @param region_id ID of the region
             * @return Shared pointer to the region, or nullptr if not found
             */
            RegionPtr getRegion(const std::string& region_id) const;
            
            /**
             * @brief Get all registered regions
             */
            std::vector<RegionPtr> getAllRegions() const;

            // ===== Connection Creation =====
            
            /**
             * @brief Create connections between two regions with specified parameters
             * @param source_id ID of the source region
             * @param target_id ID of the target region
             * @param params Connection parameters
             * @return Number of synapses created
             */
            std::size_t connectRegions(const std::string& source_id, 
                                     const std::string& target_id,
                                     const ConnectionParameters& params);
            
            /**
             * @brief Create connections using a named connectivity pattern
             */
            std::size_t connectRegionsWithPattern(const std::string& source_id,
                                                 const std::string& target_id,
                                                 const std::string& pattern_name);
            
            /**
             * @brief Establish a simplified cortical hierarchy
             */
            void establishCorticalHierarchy(const std::vector<std::string>& region_hierarchy,
                                          const std::vector<ConnectionParameters>& params);
            
            /**
             * @brief Establish thalamo-cortical connections
             */
            void establishThalamoCorticaConnections(const std::string& thalamus_id,
                                                   const std::vector<std::string>& cortical_regions,
                                                   const ConnectionParameters& params);
            
            /**
             * @brief Establish limbic system connections
             */
            void establishLimbicConnections(const std::vector<std::string>& limbic_regions,
                                          const ConnectionParameters& params);

            // ===== Initialization Patterns =====
            
            void initializeRegion(const std::string& region_id, const InitializationPattern& pattern);
            
            void initializeAllRegions();
            
            void addInitializationPattern(const std::string& pattern_name, const InitializationPattern& pattern);
            
            std::vector<std::string> getAvailablePatterns() const;

            // ===== Connection Management =====
            
            std::size_t disconnectRegions(const std::string& source_id, const std::string& target_id);
            
            void modifyConnectionStrength(const std::string& source_id, 
                                        const std::string& target_id,
                                        float strength_multiplier);
            
            RegionConnection getConnectionInfo(const std::string& source_id, 
                                             const std::string& target_id) const;
            
            std::vector<RegionConnection> getAllConnections() const;
            
            std::vector<std::vector<float>> getConnectivityMatrix() const;
            
            std::unordered_map<std::string, float> analyzeNetworkProperties() const;
            
            std::unordered_map<std::string, float> getConnectivityStatistics() const;

            // ===== Utilities =====
            
            void setRandomSeed(std::uint32_t seed);
            
            void reset();
            
            /**
             * @brief Export connectivity configuration to JSON
             * @return JSON string representation
             */
            std::string exportToJson() const;
            
            /**
             * @brief Import connectivity configuration from JSON
             * @param json_config JSON configuration string
             * @return True if import was successful
             */
            bool importFromJson(const std::string& json_config);
            
            /**
             * @brief Get total number of synapses created by this manager
             * @return Total synapse count
             */
            std::size_t getTotalSynapseCount() const;

        private:
            // ===== Internal Helper Methods =====
            
            /**
             * @brief Calculate connection probability based on distance and parameters
             */
            float calculateConnectionProbability(const ConnectionParameters& params, 
                                                float distance) const;
            
            /**
             * @brief Generate synaptic weight based on parameters
             */
            float generateSynapticWeight(const ConnectionParameters& params) const;
            
            /**
             * @brief Create individual synapse between neurons
             */
            SynapsePtr createSynapse(NeuronPtr source, NeuronPtr target, 
                                   const ConnectionParameters& params) const;
            
            /**
             * @brief Calculate distance between neurons (simplified)
             */
            float calculateNeuronDistance(NeuronPtr neuron1, NeuronPtr neuron2) const;
            
            /**
             * @brief Initialize default connectivity patterns
             */
            void initializeDefaultPatterns();
            
            /**
             * @brief Validate connection parameters
             */
            bool validateConnectionParameters(const ConnectionParameters& params) const;

        private:
            // ===== Member Variables =====
            
            /// Registered regions indexed by ID
            std::unordered_map<std::string, RegionPtr> regions_;
            
            /// Active connections between regions
            std::vector<RegionConnection> connections_;
            
            /// Available initialization patterns
            std::unordered_map<std::string, InitializationPattern> initialization_patterns_;
            
            /// Predefined connectivity patterns
            std::unordered_map<std::string, ConnectionParameters> connectivity_patterns_;
            
            /// Random number generator for connectivity
            mutable std::mt19937 rng_;
            
            /// Connection ID counter
            std::size_t connection_id_counter_;
            
            /// Total number of synapses managed
            std::size_t total_synapses_;
            
            /// Whether the manager has been initialized
            bool is_initialized_;
            
            /// Mutex to protect connections_ vector from concurrent access
            mutable std::mutex connections_mutex_;
        };

        // ===== Type Aliases =====
        using ConnectivityManagerPtr = std::shared_ptr<ConnectivityManager>;

    } // namespace Connectivity
} // namespace NeuroForge