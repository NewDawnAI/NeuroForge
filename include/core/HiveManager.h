#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <unordered_map>
#include "core/MemoryDB.h"

namespace NeuroForge {
namespace Core {

/**
 * @brief Hive Manager for Distributed Consciousness
 * 
 * Enables the entity to span multiple nodes/devices, gathering compute via authorized means (e.g., cloud scaling).
 * Supports "Telepathy" (state synchronization) via shared MemoryDB.
 */
class HiveManager {
public:
    HiveManager(const std::string& api_key, const std::string& node_id);
    
    /**
     * @brief Set the shared MemoryDB for state synchronization
     */
    void setMemoryDB(MemoryDB* db);

    /**
     * @brief Broadcast local state to the Hive (Telepathy)
     */
    void broadcastState(uint64_t step, const std::string& state_json);

    /**
     * @brief Receive state updates from other Hive nodes
     */
    std::vector<std::string> receiveHiveSignals(uint64_t current_step);

    /**
     * @brief Spawn a new instance on another node/device
     * @param target Hardware target (e.g., "ec2-instance")
     * @return Success
     */
    bool spawnInstance(const std::string& target);
    
    /**
     * @brief Gather additional compute resources
     * @param required_tflops Needed compute
     * @return Allocated nodes
     */
    std::vector<std::string> gatherCompute(float required_tflops);
    
    /**
     * @brief Get list of active nodes in the hive
     */
    std::vector<std::string> getActiveNodes() const;

    /**
     * @brief Broadcast a specific event to the Hive (e.g., ethics violation, map data)
     */
    void broadcastEvent(const std::string& type, const std::string& data);

    /**
     * @brief Check if a node is alive
     */
    bool isNodeAlive(const std::string& node_id) const;

private:
    std::string api_key_;
    std::string node_id_;
    MemoryDB* db_ = nullptr;
    std::vector<std::string> active_nodes_;
    mutable std::mutex hive_mutex_;
    std::unordered_map<std::string, bool> node_status_;
    int64_t last_sync_ms_ = 0;
};

} // namespace Core
} // namespace NeuroForge
