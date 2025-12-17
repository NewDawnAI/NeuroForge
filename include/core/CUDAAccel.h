#pragma once

#include <cstddef>

namespace NeuroForge {
namespace CUDAAccel {

// Returns true if CUDA path is compiled in (not necessarily device-present)
bool isAvailable();

// Simple batch Hebbian update: weights[i] += lr * pre[i] * post[i]
// Clamps to [0,1]. Returns true when GPU path executed; false if not available.
bool hebbianUpdate(const float* pre, const float* post, float* weights, int n, float lr);

// Pairwise STDP update (simplified): weights[i] += Aplus*pre[i] - Aminus*post[i]
// Clamps to [0,1]. Returns true when GPU path executed; false otherwise.
bool stdpPairwise(const float* pre, const float* post, float* weights, int n,
                  float a_plus, float a_minus);

// Leaky integrate neurons: v = v + dt * (input - v/tau)
bool leakyIntegrate(float* voltages, const float* input, int n, float tau, float dt);

// Reduce mean coherence: out_mean = mean(values)
bool reduceMean(const float* values, int n, float* out_mean);

// Mitochondrial update: updates energy and health based on activity
// energy, health: input/output arrays (size n)
// activation: input array (size n)
// production_rate, base_consumption: constants
bool mitochondrialUpdate(float* energy, float* health, const float* activation, int n,
                         float production_rate, float base_consumption);

struct DeviceInfo {
    char name[256];
    int major;
    int minor;
    int deviceCount;
};

// Get CUDA device info (returns true if successful and fills info)
bool getDeviceInfo(DeviceInfo* info);

} // namespace CUDAAccel
} // namespace NeuroForge
