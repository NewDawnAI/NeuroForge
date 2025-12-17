#include "core/SubstrateLanguageManager.h"
#include "core/SubstrateLanguageAdapter.h"
#include "core/LanguageSystem.h"
#include "core/HypergraphBrain.h"
#include <memory>
#include <iostream>

namespace NeuroForge {
namespace Core {

// Static member definition
std::unordered_map<HypergraphBrain*, std::unique_ptr<SubstrateLanguageAdapter>> 
    SubstrateLanguageManager::adapters_;

bool SubstrateLanguageManager::initializeForBrain(HypergraphBrain* brain, 
                                                 std::shared_ptr<LanguageSystem> language_system) {
    if (!brain || !language_system) {
        return false;
    }

    // Create the adapter with default config
    SubstrateLanguageAdapter::Config config;
    auto adapter = std::make_unique<SubstrateLanguageAdapter>(
        std::shared_ptr<HypergraphBrain>(brain, [](HypergraphBrain*){}), // Non-owning shared_ptr
        language_system,
        config
    );

    if (!adapter->initialize()) {
        return false;
    }

    // Store the adapter in the static map
    adapters_[brain] = std::move(adapter);
    
    std::cout << "[SubstrateLanguageManager] Initialized substrate language adapter for brain" << std::endl;
    return true;
}

void SubstrateLanguageManager::processSubstrateLanguage(HypergraphBrain* brain, float delta_time) {
    auto it = adapters_.find(brain);
    if (it != adapters_.end() && it->second) {
        it->second->processSubstrateActivations(delta_time);
    }
}

SubstrateLanguageAdapter* SubstrateLanguageManager::getAdapter(HypergraphBrain* brain) {
    auto it = adapters_.find(brain);
    return (it != adapters_.end()) ? it->second.get() : nullptr;
}

void SubstrateLanguageManager::shutdownForBrain(HypergraphBrain* brain) {
    auto it = adapters_.find(brain);
    if (it != adapters_.end()) {
        if (it->second) {
            it->second->shutdown();
        }
        adapters_.erase(it);
        std::cout << "[SubstrateLanguageManager] Shutdown substrate language adapter for brain" << std::endl;
    }
}

void SubstrateLanguageManager::shutdownAll() {
    for (auto& [brain, adapter] : adapters_) {
        if (adapter) {
            adapter->shutdown();
        }
    }
    adapters_.clear();
    std::cout << "[SubstrateLanguageManager] Shutdown all substrate language adapters" << std::endl;
}

} // namespace Core
} // namespace NeuroForge