#include "core/RegionRegistry.h"
#include "core/Region.h"
#include "regions/CorticalRegions.h"
#include "regions/SubcorticalRegions.h"
#include <cctype>
#include <mutex>

namespace NeuroForge {
namespace Core {

RegionRegistry& RegionRegistry::instance() {
    static RegionRegistry* inst = nullptr;
    static std::once_flag flag;
    std::call_once(flag, []() {
        inst = new RegionRegistry();
    });
    return *inst;
}

std::string RegionRegistry::normalize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

bool RegionRegistry::registerFactory(const std::string& key, Factory factory) {
    if (!factory) return false;
    const std::string k = normalize(key);
    std::lock_guard<std::mutex> lock(mtx_);
    factories_[k] = std::move(factory);
    return true;
}

bool RegionRegistry::registerAlias(const std::string& alias, const std::string& existing_key) {
    const std::string a = normalize(alias);
    const std::string k = normalize(existing_key);
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = factories_.find(k);
    if (it == factories_.end()) return false;
    factories_[a] = it->second; // copy the callable
    return true;
}

NeuroForge::RegionPtr RegionRegistry::create(const std::string& key, const std::string& name, std::size_t neuron_count) const {
    const std::string k = normalize(key);
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = factories_.find(k);
    if (it == factories_.end()) return nullptr;
    return (it->second)(name, neuron_count);
}

std::vector<std::string> RegionRegistry::listKeys() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::string> keys;
    keys.reserve(factories_.size());
    for (const auto& kv : factories_) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    return keys;
}

// Default registrations live in regions compilation units to avoid static init order issues.

} // namespace Core
} // namespace NeuroForge