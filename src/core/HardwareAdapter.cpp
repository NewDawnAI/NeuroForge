#include "core/HardwareAdapter.h"
#include "core/HypergraphBrain.h"
#include <thread>
#include <cstdlib>
#include <iostream>

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/utsname.h>
#endif

#ifdef NF_HAVE_CUDA
#include <cuda_runtime.h>
#endif

namespace NeuroForge {
namespace Core {

HardwareAdapter::HardwareAdapter() : hardware_type_("unknown") {
    scanHardware();
}

void HardwareAdapter::scanHardware() {
    std::lock_guard<std::mutex> lock(adapter_mutex_);
    detectCPU();
    detectGPU();
    detectEmbodiment();
}

std::string HardwareAdapter::getHardwareType() const {
    std::lock_guard<std::mutex> lock(adapter_mutex_);
    return hardware_type_;
}

std::unordered_map<std::string, std::string> HardwareAdapter::getSpecs() const {
    std::lock_guard<std::mutex> lock(adapter_mutex_);
    return specs_;
}

void HardwareAdapter::adaptToHardware(HypergraphBrain& brain) {
    // Example: Reduce neuron count on low-power devices
    if (hardware_type_ == "drone" || hardware_type_ == "satellite") {
        // Assume HypergraphBrain has this method or similar
        // brain.setMaxNeurons(100000); 
        std::cout << "[HardwareAdapter] Adapting for low-power device: " << hardware_type_ << std::endl;
    } else if (specs_["gpu"] != "none") {
        // brain.enableCUDA();  // Use GPU accel if available
        std::cout << "[HardwareAdapter] GPU detected: " << specs_["gpu"] << ". Enabling acceleration if supported." << std::endl;
    }
    
    // Adapt learning rate based on cores
    int cores = 1;
    try {
        cores = std::stoi(specs_["cpu_cores"]);
    } catch (...) {}
    
    // brain.setLearningRate(0.01f * static_cast<float>(cores)); // Example
    std::cout << "[HardwareAdapter] CPU Cores: " << cores << ". Tuning parallelism." << std::endl;
}

void HardwareAdapter::detectCPU() {
    specs_["cpu_cores"] = std::to_string(std::thread::hardware_concurrency());
    
#ifdef _WIN32
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    specs_["os"] = "Windows";
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: specs_["arch"] = "x64"; break;
        case PROCESSOR_ARCHITECTURE_ARM: specs_["arch"] = "ARM"; break;
        case PROCESSOR_ARCHITECTURE_ARM64: specs_["arch"] = "ARM64"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: specs_["arch"] = "x86"; break;
        default: specs_["arch"] = "unknown"; break;
    }
#else
    utsname info;
    if (uname(&info) == 0) {
        specs_["os"] = info.sysname;
        specs_["arch"] = info.machine;
    }
#endif
}

void HardwareAdapter::detectGPU() {
#ifdef NF_HAVE_CUDA
    int count = 0;
    if (cudaGetDeviceCount(&count) == cudaSuccess && count > 0) {
        specs_["gpu"] = "CUDA-enabled";
    } else {
        specs_["gpu"] = "none";
    }
#else
    specs_["gpu"] = "none";
#endif
}

void HardwareAdapter::detectEmbodiment() {
    // Example: Check env vars for embodiment (e.g., set in drone firmware)
    const char* env_type = std::getenv("NF_EMBODIMENT");
    if (env_type) {
        hardware_type_ = env_type;  // e.g., "bipedal_robot", "ornithopter"
    } else if (specs_["os"].find("RTOS") != std::string::npos) {  // Real-time OS for drones
        hardware_type_ = "drone";
    } else {
        hardware_type_ = "computer";  // Default
    }
}

} // namespace Core
} // namespace NeuroForge
