#pragma once

#include "Types.h"
#include "LanguageSystem.h"
#include "HypergraphBrain.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>

namespace NeuroForge {
    namespace Core {

        /**
         * @brief Substrate-to-Language Adapter for Milestone 5
         * 
         * Implements token discovery from substrate representations instead of external grounding.
         * Discovers stable neural assemblies and labels them with tokens post hoc.
         */
        class SubstrateLanguageAdapter {
        public:
            /**
             * @brief Configuration for substrate language adaptation
             */
            struct Config {
                float stability_threshold = 0.7f;      ///< Minimum stability for assembly detection
                float novelty_threshold = 0.3f;        ///< Minimum novelty for new token creation
                std::size_t min_assembly_size = 3;     ///< Minimum neurons in stable assembly
                std::size_t max_tokens_per_cycle = 5;  ///< Maximum new tokens per discovery cycle
                float activation_window = 0.1f;        ///< Time window for co-activation detection
                float decay_rate = 0.95f;              ///< Decay rate for assembly stability tracking
            };

            /**
             * @brief Detected neural assembly
             */
            struct NeuralAssembly {
                std::vector<NeuroForge::NeuronID> neurons;  ///< Neurons in assembly
                std::vector<float> activation_pattern;      ///< Typical activation pattern
                float stability_score = 0.0f;               ///< Stability over time
                std::size_t occurrence_count = 0;           ///< Number of times observed
                std::chrono::steady_clock::time_point last_seen; ///< Last observation time
                std::string generated_token;                 ///< Associated token symbol
            };

            /**
             * @brief Constructor
             */
            SubstrateLanguageAdapter(std::shared_ptr<HypergraphBrain> brain,
                                   std::shared_ptr<LanguageSystem> language_system,
                                   const Config& config);

            /**
             * @brief Initialize the adapter
             */
            bool initialize();

            /**
             * @brief Shutdown the adapter
             */
            void shutdown();

            /**
             * @brief Process substrate activations to discover token-worthy assemblies
             */
            void processSubstrateActivations(float delta_time);

            /**
             * @brief Detect stable neural assemblies from current brain state
             */
            std::vector<NeuralAssembly> detectStableAssemblies();

            /**
             * @brief Generate token symbol for a neural assembly
             */
            std::string generateTokenForAssembly(const NeuralAssembly& assembly);

            /**
             * @brief Check if assembly is novel enough to warrant new token
             */
            bool isNovelAssembly(const NeuralAssembly& assembly) const;

            /**
             * @brief Update stability scores for existing assemblies
             */
            void updateAssemblyStabilities(float delta_time);

            /**
             * @brief Get current discovered assemblies
             */
            const std::vector<NeuralAssembly>& getDiscoveredAssemblies() const { return discovered_assemblies_; }

            /**
             * @brief Get adapter statistics
             */
            struct Statistics {
                std::size_t assemblies_discovered = 0;
                std::size_t tokens_created = 0;
                std::size_t stable_assemblies = 0;
                float average_stability = 0.0f;
                std::size_t processing_cycles = 0;
            };
            Statistics getStatistics() const;

            /**
             * @brief Update configuration
             */
            void updateConfig(const Config& config) { config_ = config; }

        private:
            Config config_;
            std::shared_ptr<HypergraphBrain> brain_;
            std::shared_ptr<LanguageSystem> language_system_;

            // Assembly tracking
            std::vector<NeuralAssembly> discovered_assemblies_;
            std::unordered_map<std::string, std::size_t> assembly_lookup_;
            
            // Activation history for stability analysis
            std::vector<std::vector<std::pair<NeuroForge::NeuronID, float>>> activation_history_;
            std::size_t history_window_size_ = 10;
            
            // Statistics
            mutable std::mutex stats_mutex_;
            Statistics stats_;
            
            // Token generation
            std::atomic<std::size_t> token_counter_{0};
            
            // Helper methods
            std::vector<std::pair<NeuroForge::NeuronID, float>> getCurrentActivations();
            float calculateAssemblyStability(const NeuralAssembly& assembly) const;
            float calculateAssemblySimilarity(const NeuralAssembly& a, const NeuralAssembly& b) const;
            std::string generateUniqueTokenSymbol();
            void pruneStaleAssemblies();
            void updateStatistics();
        };

    } // namespace Core
} // namespace NeuroForge