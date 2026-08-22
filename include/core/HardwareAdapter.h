#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace NeuroForge {
namespace Core {

class HypergraphBrain; // Forward declaration

/**
 * @brief Hardware Adapter for Device-Agnostic Operation
 * 
 * Scans and adapts to hardware at runtime, enabling embodiment in any form.
 * Supports CPU/GPU detection, system specs, and extension for robotics/satellites.
 */
class HardwareAdapter {
public:
    HardwareAdapter();
    
    /**
     * @brief Scan current hardware and update specs
     */
    void scanHardware();
    
    /**
     * @brief Get hardware type (e.g., "laptop", "drone", "satellite")
     * @return Detected type
     */
    std::string getHardwareType() const;
    
    /**
     * @brief Get all hardware specs as key-value pairs
     * @return Map of specs (e.g., {"cpu_cores": "8", "gpu": "NVIDIA RTX"})
     */
    std::unordered_map<std::string, std::string> getSpecs() const;
    
    /**
     * @brief Adapt neural params based on hardware (e.g., reduce batch size on low-power devices)
     * @param brain Reference to HypergraphBrain for adaptation
     */
    void adaptToHardware(HypergraphBrain& brain);

private:
    std::unordered_map<std::string, std::string> specs_;
    std::string hardware_type_;
    mutable std::mutex adapter_mutex_;
    
    void detectCPU();
    void detectGPU();
    void detectEmbodiment();  // For robotics/drone/satellite via env vars or APIs
};

} // namespace Core
} // namespace NeuroForge
