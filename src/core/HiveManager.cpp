#include "core/HiveManager.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace NeuroForge {
namespace Core {

HiveManager::HiveManager(const std::string& api_key, const std::string& node_id) 
    : api_key_(api_key), node_id_(node_id) {
    // In a real implementation, this would authenticate with the cloud provider
    // or distributed system coordinator.
    std::cout << "[HiveManager] Initialized node " << node_id << " with API key: " << (api_key.empty() ? "none" : "****") << std::endl;
    
    // Always include self as the primary node
    std::lock_guard<std::mutex> lock(hive_mutex_);
    active_nodes_.push_back(node_id);
    node_status_[node_id] = true;
}

void HiveManager::setMemoryDB(MemoryDB* db) {
    std::lock_guard<std::mutex> lock(hive_mutex_);
    db_ = db;
}

void HiveManager::broadcastState(uint64_t step, const std::string& state_json) {
    if (!db_) return;
    
    // Use current time as timestamp
    auto now = std::chrono::system_clock::now();
    auto ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    int64_t out_id = 0;
    // We use "hive_signal" as the state type and node_id as the region_id for easy filtering
    // Since we don't have a dedicated Hive table yet, we piggyback on SubstrateState
    // Ideally, we should pass the actual run_id here, but for now we use 0 or a convention
    // Assuming run_id is managed externally or we can fetch it. 
    // For this prototype, we'll assume run_id 0 is acceptable for hive signals or we pass it in.
    // Actually, MemoryDB requires a valid run_id for foreign keys usually, but let's check.
    // MemoryDB.cpp checks run_id FK usually. 
    // We will assume the caller ensures run_id 0 exists or we use a valid one.
    // To be safe, let's just use 1 for Hive global channel if possible, or assume db logic handles it.
    // Let's pass 0 for now as a "global" run if allowed, or we might need to add run_id to this method signature.
    // For "Telepathy", it's often cross-run (nodes might have different run IDs).
    // Let's assume we use a shared "Hive Run" ID = 999999 for synchronization?
    // Or better: Use the node's local run_id but query ALL run_ids for hive signals.
    
    // NOTE: For this prototype, we will use run_id=0 (often the 'system' run).
    db_->insertSubstrateState(ts_ms, step, "hive_signal", node_id_, state_json, 0, out_id);
}

void HiveManager::broadcastEvent(const std::string& type, const std::string& data) {
    if (!db_) return;
    auto now = std::chrono::system_clock::now();
    auto ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    int64_t out_id = 0;
    // Broadcast with a specific type prefix or metadata if needed.
    // For simplicity, we use the "hive_signal" mechanism but wrap the data.
    // Ideally, we'd have a separate table or column, but "hive_signal" is flexible.
    // Format: {"type": "EVENT_TYPE", "data": DATA}
    std::string payload = "{\"type\":\"" + type + "\",\"payload\":" + data + "}";
    db_->insertSubstrateState(ts_ms, 0, "hive_signal", node_id_, payload, 0, out_id);
}

std::vector<std::string> HiveManager::receiveHiveSignals(uint64_t current_step) {
    if (!db_) return {};
    
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    // Read signals from the last 1 second (simulating real-time telepathy)
    // We query run_id=0 as per broadcastState
    int64_t start_ms = last_sync_ms_ > 0 ? last_sync_ms_ : (now_ms - 5000); // look back 5s initially
    
    auto states = db_->getSubstrateStates(0, "hive_signal", start_ms, now_ms, 100);
    last_sync_ms_ = now_ms;
    
    std::vector<std::string> signals;
    for (const auto& s : states) {
        if (s.region_id == node_id_) continue; // Ignore self
        signals.push_back("[" + s.region_id + "] " + s.serialized_data);
        
        // Update active nodes list
        std::lock_guard<std::mutex> lock(hive_mutex_);
        if (node_status_.find(s.region_id) == node_status_.end()) {
            active_nodes_.push_back(s.region_id);
            node_status_[s.region_id] = true;
            std::cout << "[HiveManager] Discovered new peer: " << s.region_id << std::endl;
        }
    }
    return signals;
}

bool HiveManager::spawnInstance(const std::string& target) {
    std::lock_guard<std::mutex> lock(hive_mutex_);
    std::cout << "[HiveManager] Attempting to spawn instance on target: " << target << std::endl;
    
    // Stub implementation: Simulate successful spawn on "simulated_cluster"
    if (target.find("simulated") != std::string::npos || target == "localhost") {
        std::string new_node_id = "node_" + std::to_string(active_nodes_.size());
        active_nodes_.push_back(new_node_id);
        node_status_[new_node_id] = true;
        std::cout << "[HiveManager] Successfully spawned " << new_node_id << std::endl;
        return true;
    }
    
    std::cout << "[HiveManager] Target not reachable or unauthorized." << std::endl;
    return false;
}

std::vector<std::string> HiveManager::gatherCompute(float required_tflops) {
    std::lock_guard<std::mutex> lock(hive_mutex_);
    std::cout << "[HiveManager] Gathering compute resources for " << required_tflops << " TFLOPS..." << std::endl;
    
    // Stub: Return existing nodes if they "meet" requirements
    // In reality, this would request spot instances or allocate cluster nodes
    return active_nodes_;
}

std::vector<std::string> HiveManager::getActiveNodes() const {
    std::lock_guard<std::mutex> lock(hive_mutex_);
    return active_nodes_;
}

bool HiveManager::isNodeAlive(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(hive_mutex_);
    auto it = node_status_.find(node_id);
    if (it != node_status_.end()) {
        return it->second;
    }
    return false;
}

} // namespace Core
} // namespace NeuroForge
