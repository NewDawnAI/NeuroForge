#pragma once

#include "Types.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

namespace NeuroForge {
namespace Core {

// Central registry for mapping short keys to Region factory functions
class RegionRegistry {
public:
    // Factory signature: constructs a concrete Region subclass
    using Factory = std::function<NeuroForge::RegionPtr(const std::string& name, std::size_t neuron_count)>;

    // Singleton access
    static RegionRegistry& instance();

    // Register or override a factory under a key (case-insensitive)
    bool registerFactory(const std::string& key, Factory factory);

    // Register an alias that forwards to an existing key (case-insensitive)
    bool registerAlias(const std::string& alias, const std::string& existing_key);

    // Create a region by key; returns nullptr if key not found
    NeuroForge::RegionPtr create(const std::string& key, const std::string& name, std::size_t neuron_count) const;

    // List all registered keys (normalized, sorted)
    std::vector<std::string> listKeys() const;

    // Normalize key (lowercase)
    static std::string normalize(const std::string& s);

private:
    RegionRegistry() = default;
    RegionRegistry(const RegionRegistry&) = delete;
    RegionRegistry& operator=(const RegionRegistry&) = delete;

    mutable std::mutex mtx_;
    std::unordered_map<std::string, Factory> factories_;
};

// Helper RAII registrars to be used in region .cpp files
struct RegisterRegionFactory {
    RegisterRegionFactory(const std::string& key, RegionRegistry::Factory factory) {
        RegionRegistry::instance().registerFactory(key, std::move(factory));
    }
};

struct RegisterRegionAlias {
    RegisterRegionAlias(const std::string& alias, const std::string& existing_key) {
        RegionRegistry::instance().registerAlias(alias, existing_key);
    }
};

} // namespace Core
} // namespace NeuroForge