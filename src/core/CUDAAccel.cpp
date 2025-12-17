#include "core/CUDAAccel.h"
#include <cstring>
#ifdef NF_HAVE_CUDA
#include <cuda_runtime.h>
#endif

#ifdef NF_HAVE_CUDA
extern "C" {
    bool nf_cuda_hebbian_update(const float* pre, const float* post, float* weights, int n, float lr);
    bool nf_cuda_stdp_pairwise(const float* pre, const float* post, float* weights, int n, float a_plus, float a_minus);
    bool nf_cuda_leaky_integrate(float* voltages, const float* input, int n, float tau, float dt);
    bool nf_cuda_reduce_mean(const float* values, int n, float* out_mean);
    bool nf_cuda_mitochondrial_update(float* energy, float* health, const float* activation, int n, float production_rate, float base_consumption);
}
#endif

namespace NeuroForge {
namespace CUDAAccel {

bool isAvailable() {
#ifdef NF_HAVE_CUDA
    return true;
#else
    return false;
#endif
}

bool hebbianUpdate(const float* pre, const float* post, float* weights, int n, float lr) {
#ifdef NF_HAVE_CUDA
    return nf_cuda_hebbian_update(pre, post, weights, n, lr);
#else
    (void)pre; (void)post; (void)weights; (void)n; (void)lr;
    return false;
#endif
}

bool stdpPairwise(const float* pre, const float* post, float* weights, int n,
                  float a_plus, float a_minus) {
#ifdef NF_HAVE_CUDA
    return nf_cuda_stdp_pairwise(pre, post, weights, n, a_plus, a_minus);
#else
    (void)pre; (void)post; (void)weights; (void)n; (void)a_plus; (void)a_minus;
    return false;
#endif
}

bool leakyIntegrate(float* voltages, const float* input, int n, float tau, float dt) {
#ifdef NF_HAVE_CUDA
    return nf_cuda_leaky_integrate(voltages, input, n, tau, dt);
#else
    (void)voltages; (void)input; (void)n; (void)tau; (void)dt;
    return false;
#endif
}

bool reduceMean(const float* values, int n, float* out_mean) {
#ifdef NF_HAVE_CUDA
    return nf_cuda_reduce_mean(values, n, out_mean);
#else
    (void)values; (void)n; if (out_mean) *out_mean = 0.0f; return false;
#endif
}

bool mitochondrialUpdate(float* energy, float* health, const float* activation, int n,
                         float production_rate, float base_consumption) {
#ifdef NF_HAVE_CUDA
    return nf_cuda_mitochondrial_update(energy, health, activation, n, production_rate, base_consumption);
#else
    (void)energy; (void)health; (void)activation; (void)n; (void)production_rate; (void)base_consumption;
    return false;
#endif
}

bool getDeviceInfo(DeviceInfo* info) {
#ifdef NF_HAVE_CUDA
    if (!info) return false;
    int count = 0;
    // Ensure a context is created for device query
    (void)cudaFree(0);
    cudaError_t e = cudaGetDeviceCount(&count);
    if (e != cudaSuccess || count <= 0) { return false; }
    cudaDeviceProp prop{};
    if (cudaGetDeviceProperties(&prop, 0) != cudaSuccess) { return false; }
    strncpy(info->name, prop.name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
    info->major = prop.major;
    info->minor = prop.minor;
    info->deviceCount = count;
    return true;
#else
    (void)info; return false;
#endif
}

} // namespace CUDAAccel
} // namespace NeuroForge
