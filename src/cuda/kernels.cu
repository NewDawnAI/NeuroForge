#include <cuda_runtime.h>
#include <algorithm>
#include <cmath>

__global__ void kernel_hebbian(float* w, const float* pre, const float* post, int n, float lr) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        float upd = lr * pre[i] * post[i];
        float nw = w[i] + upd;
        // Clamp to [0,1]
        nw = fminf(1.0f, fmaxf(0.0f, nw));
        w[i] = nw;
    }
}

__global__ void kernel_stdp_pairwise(float* w, const float* pre, const float* post, int n, float a_plus, float a_minus) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        float upd = a_plus * pre[i] - a_minus * post[i];
        float nw = w[i] + upd;
        nw = fminf(1.0f, fmaxf(0.0f, nw));
        w[i] = nw;
    }
}

__global__ void kernel_leaky_integrate(float* v, const float* in, int n, float tau, float dt) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        float vi = v[i];
        float dv = dt * (in[i] - vi / tau);
        v[i] = vi + dv;
    }
}

__global__ void kernel_reduce_mean(const float* x, int n, float* out) {
    __shared__ float sdata[256];
    int tid = threadIdx.x;
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    float val = 0.0f;
    if (i < n) val = x[i];
    sdata[tid] = val;
    __syncthreads();
    // Reduce within block
    for (int s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) sdata[tid] += sdata[tid + s];
        __syncthreads();
    }
    if (tid == 0) {
        atomicAdd(out, sdata[0]);
    }
}

__global__ void kernel_mitochondrial_update(float* energy, float* health, const float* activation, int n,
                                            float production_rate, float base_consumption) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < n) {
        float act = activation[i];
        float e = energy[i];
        float h = health[i];

        bool spiking = (act > 0.8f);

        // Production: Base + Activity-dependent boost
        float production = production_rate * (0.7f + 0.3f * act);
        
        // Consumption: Base + Spike cost + Maintenance
        float consumption = base_consumption;
        if (spiking) consumption += 0.012f;
        else if (act > 0.1f) consumption += 0.002f;
        
        // Update energy
        e = fminf(1.0f, fmaxf(0.0f, e + production - consumption));

        // Health dynamics (slow variable)
        if (e < 0.3f) {
            // Chronic overload
            h = fminf(1.0f, fmaxf(0.1f, h - 0.00001f));
        } else if (e > 0.7f) {
            // Recovery
            h = fminf(1.0f, fmaxf(0.1f, h + 0.00002f));
        }

        energy[i] = e;
        health[i] = h;
    }
}

static bool checkCuda(const char* ctx) {
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        // In a production build, route errors to a logger
        return false;
    }
    return true;
}

extern "C" bool nf_cuda_hebbian_update(const float* pre_h, const float* post_h, float* weights_h, int n, float lr) {
    float *pre_d = nullptr, *post_d = nullptr, *w_d = nullptr;
    size_t bytes = static_cast<size_t>(n) * sizeof(float);
    if (cudaMalloc(&pre_d, bytes) != cudaSuccess) return false;
    if (cudaMalloc(&post_d, bytes) != cudaSuccess) { cudaFree(pre_d); return false; }
    if (cudaMalloc(&w_d, bytes) != cudaSuccess) { cudaFree(pre_d); cudaFree(post_d); return false; }
    if (cudaMemcpy(pre_d, pre_h, bytes, cudaMemcpyHostToDevice) != cudaSuccess) { cudaFree(pre_d); cudaFree(post_d); cudaFree(w_d); return false; }
    if (cudaMemcpy(post_d, post_h, bytes, cudaMemcpyHostToDevice) != cudaSuccess) { cudaFree(pre_d); cudaFree(post_d); cudaFree(w_d); return false; }
    if (cudaMemcpy(w_d, weights_h, bytes, cudaMemcpyHostToDevice) != cudaSuccess) { cudaFree(pre_d); cudaFree(post_d); cudaFree(w_d); return false; }
    int threads = 256;
    int blocks = (n + threads - 1) / threads;
    // optional timing
    cudaEvent_t start, stop; cudaEventCreate(&start); cudaEventCreate(&stop);
    cudaEventRecord(start);
    kernel_hebbian<<<blocks, threads>>>(w_d, pre_d, post_d, n, lr);
    cudaDeviceSynchronize();
    if (!checkCuda("kernel_hebbian")) { cudaFree(pre_d); cudaFree(post_d); cudaFree(w_d); return false; }
    cudaEventRecord(stop); cudaEventSynchronize(stop);
    float ms = 0.0f; cudaEventElapsedTime(&ms, start, stop);
    cudaEventDestroy(start); cudaEventDestroy(stop);
    bool ok = (cudaMemcpy(weights_h, w_d, bytes, cudaMemcpyDeviceToHost) == cudaSuccess);
    cudaFree(pre_d); cudaFree(post_d); cudaFree(w_d);
    printf("[GPU] Hebbian: %d synapses in %.2f ms\n", n, ms);
    return ok;
}

extern "C" bool nf_cuda_stdp_pairwise(const float* pre_h, const float* post_h, float* weights_h, int n, float a_plus, float a_minus) {
    float *pre_d = nullptr, *post_d = nullptr, *w_d = nullptr;
    size_t bytes = static_cast<size_t>(n) * sizeof(float);
    if (cudaMalloc(&pre_d, bytes) != cudaSuccess) return false;
    if (cudaMalloc(&post_d, bytes) != cudaSuccess) { cudaFree(pre_d); return false; }
    if (cudaMalloc(&w_d, bytes) != cudaSuccess) { cudaFree(pre_d); cudaFree(post_d); return false; }
    if (cudaMemcpy(pre_d, pre_h, bytes, cudaMemcpyHostToDevice) != cudaSuccess) { cudaFree(pre_d); cudaFree(post_d); cudaFree(w_d); return false; }
    if (cudaMemcpy(post_d, post_h, bytes, cudaMemcpyHostToDevice) != cudaSuccess) { cudaFree(pre_d); cudaFree(post_d); cudaFree(w_d); return false; }
    if (cudaMemcpy(w_d, weights_h, bytes, cudaMemcpyHostToDevice) != cudaSuccess) { cudaFree(pre_d); cudaFree(post_d); cudaFree(w_d); return false; }
    int threads = 256;
    int blocks = (n + threads - 1) / threads;
    cudaEvent_t start, stop; cudaEventCreate(&start); cudaEventCreate(&stop);
    cudaEventRecord(start);
    kernel_stdp_pairwise<<<blocks, threads>>>(w_d, pre_d, post_d, n, a_plus, a_minus);
    cudaDeviceSynchronize();
    if (!checkCuda("kernel_stdp_pairwise")) { cudaFree(pre_d); cudaFree(post_d); cudaFree(w_d); return false; }
    cudaEventRecord(stop); cudaEventSynchronize(stop);
    float ms = 0.0f; cudaEventElapsedTime(&ms, start, stop);
    cudaEventDestroy(start); cudaEventDestroy(stop);
    bool ok = (cudaMemcpy(weights_h, w_d, bytes, cudaMemcpyDeviceToHost) == cudaSuccess);
    cudaFree(pre_d); cudaFree(post_d); cudaFree(w_d);
    printf("[GPU] STDP: %d pairs in %.2f ms\n", n, ms);
    return ok;
}

extern "C" bool nf_cuda_leaky_integrate(float* voltages_h, const float* input_h, int n, float tau, float dt) {
    float *v_d = nullptr, *in_d = nullptr;
    size_t bytes = static_cast<size_t>(n) * sizeof(float);
    if (cudaMalloc(&v_d, bytes) != cudaSuccess) return false;
    if (cudaMalloc(&in_d, bytes) != cudaSuccess) { cudaFree(v_d); return false; }
    if (cudaMemcpy(v_d, voltages_h, bytes, cudaMemcpyHostToDevice) != cudaSuccess) { cudaFree(v_d); cudaFree(in_d); return false; }
    if (cudaMemcpy(in_d, input_h, bytes, cudaMemcpyHostToDevice) != cudaSuccess) { cudaFree(v_d); cudaFree(in_d); return false; }
    int threads = 256;
    int blocks = (n + threads - 1) / threads;
    kernel_leaky_integrate<<<blocks, threads>>>(v_d, in_d, n, tau, dt);
    cudaDeviceSynchronize();
    if (!checkCuda("kernel_leaky_integrate")) { cudaFree(v_d); cudaFree(in_d); return false; }
    bool ok = (cudaMemcpy(voltages_h, v_d, bytes, cudaMemcpyDeviceToHost) == cudaSuccess);
    cudaFree(v_d); cudaFree(in_d);
    return ok;
}

extern "C" bool nf_cuda_reduce_mean(const float* values_h, int n, float* out_mean_h) {
    float *x_d = nullptr, *sum_d = nullptr;
    size_t bytes = static_cast<size_t>(n) * sizeof(float);
    if (cudaMalloc(&x_d, bytes) != cudaSuccess) return false;
    if (cudaMalloc(&sum_d, sizeof(float)) != cudaSuccess) { cudaFree(x_d); return false; }
    float zero = 0.0f;
    cudaMemcpy(sum_d, &zero, sizeof(float), cudaMemcpyHostToDevice);
    cudaMemcpy(x_d, values_h, bytes, cudaMemcpyHostToDevice);
    int threads = 256;
    int blocks = (n + threads - 1) / threads;
    kernel_reduce_mean<<<blocks, threads>>>(x_d, n, sum_d);
    cudaDeviceSynchronize();
    if (!checkCuda("kernel_reduce_mean")) { cudaFree(x_d); cudaFree(sum_d); return false; }
    float sum_h = 0.0f;
    cudaMemcpy(&sum_h, sum_d, sizeof(float), cudaMemcpyDeviceToHost);
    *out_mean_h = (n > 0) ? (sum_h / static_cast<float>(n)) : 0.0f;
    cudaFree(x_d); cudaFree(sum_d);
    return true;
}

extern "C" bool nf_cuda_mitochondrial_update(float* energy_h, float* health_h, const float* activation_h, int n,
                                             float production_rate, float base_consumption) {
    float *e_d = nullptr, *h_d = nullptr, *act_d = nullptr;
    size_t bytes = static_cast<size_t>(n) * sizeof(float);
    
    if (cudaMalloc(&e_d, bytes) != cudaSuccess) return false;
    if (cudaMalloc(&h_d, bytes) != cudaSuccess) { cudaFree(e_d); return false; }
    if (cudaMalloc(&act_d, bytes) != cudaSuccess) { cudaFree(e_d); cudaFree(h_d); return false; }

    if (cudaMemcpy(e_d, energy_h, bytes, cudaMemcpyHostToDevice) != cudaSuccess) { cudaFree(e_d); cudaFree(h_d); cudaFree(act_d); return false; }
    if (cudaMemcpy(h_d, health_h, bytes, cudaMemcpyHostToDevice) != cudaSuccess) { cudaFree(e_d); cudaFree(h_d); cudaFree(act_d); return false; }
    if (cudaMemcpy(act_d, activation_h, bytes, cudaMemcpyHostToDevice) != cudaSuccess) { cudaFree(e_d); cudaFree(h_d); cudaFree(act_d); return false; }

    int threads = 256;
    int blocks = (n + threads - 1) / threads;

    kernel_mitochondrial_update<<<blocks, threads>>>(e_d, h_d, act_d, n, production_rate, base_consumption);
    cudaDeviceSynchronize();

    bool ok = true;
    if (!checkCuda("kernel_mitochondrial_update")) { ok = false; }
    else {
        if (cudaMemcpy(energy_h, e_d, bytes, cudaMemcpyDeviceToHost) != cudaSuccess) ok = false;
        if (cudaMemcpy(health_h, h_d, bytes, cudaMemcpyDeviceToHost) != cudaSuccess) ok = false;
    }

    cudaFree(e_d); cudaFree(h_d); cudaFree(act_d);
    return ok;
}
