#pragma once

#include "Types.h"
#include <memory>
#include <unordered_map>

namespace NeuroForge {
namespace Core {

// Forward declarations
class HypergraphBrain;
class LanguageSystem;
class SubstrateLanguageAdapter;

/**
 * @brief Standalone Substrate Language Manager for Milestone 5
 * 
 * This manager integrates the SubstrateLanguageAdapter with the brain
 * without creating circular dependencies in the header files.
 */
class SubstrateLanguageManager {
public:
    /**
     * @brief Initialize substrate language adapter for a brain
     */
    static bool initializeForBrain(HypergraphBrain* brain, 
                                  std::shared_ptr<LanguageSystem> language_system);

    /**
     * @brief Process substrate language for a brain
     */
    static void processSubstrateLanguage(HypergraphBrain* brain, float delta_time);

    /**
     * @brief Get adapter for a brain
     */
    static SubstrateLanguageAdapter* getAdapter(HypergraphBrain* brain);

    /**
     * @brief Shutdown adapter for a brain
     */
    static void shutdownForBrain(HypergraphBrain* brain);

    /**
     * @brief Shutdown all adapters
     */
    static void shutdownAll();

private:
    static std::unordered_map<HypergraphBrain*, std::unique_ptr<SubstrateLanguageAdapter>> adapters_;
};

} // namespace Core
} // namespace NeuroForge