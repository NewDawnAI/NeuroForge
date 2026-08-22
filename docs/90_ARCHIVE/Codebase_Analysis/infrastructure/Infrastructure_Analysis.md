# Infrastructure Analysis

This document analyzes the supporting infrastructure of NeuroForge, including connectivity management, hardware acceleration, and visualization tools.

## 1. ConnectivityManager

### Location
- `include/connectivity/ConnectivityManager.h`
- `src/connectivity/ConnectivityManager.cpp`

### Functional Description
A sophisticated factory and manager for neural wiring. It generates synaptic connections between regions based on biological probability distributions.

### Key Features
- **Distributions**: Uniform, Gaussian, Exponential, SmallWorld.
- **Types**: Feedforward, Feedback, Lateral, Global.
- **Initialization**: Can initialize regions with specific patterns (e.g., for cortical layers).
- **Plasticity Configuration**: Sets initial plasticity rules (Hebbian, STDP) for generated connections.

### Usage Example
```cpp
ConnectivityManager conn_mgr;
ConnectivityManager::ConnectionParameters params;
params.type = ConnectivityManager::ConnectivityType::SmallWorld;
params.connection_probability = 0.2f;
conn_mgr.connectRegions("V1", "V2", params);
```

---

## 2. CUDA Acceleration

### Location
- `include/core/CUDAAccel.h`
- `src/cuda/kernels.cu`

### Functional Description
Provides GPU implementations for computationally intensive tasks, primarily the neuron update loop and synapse propagation.

### Key Features
- **Kernels**: Custom CUDA kernels for sparse matrix multiplication (SpMV) typical in SNNs.
- **Integration**: The `HypergraphBrain` detects `NF_HAVE_CUDA` and offloads processing if available.

---

## 3. Visualizer3D

### Location
- `include/viewer/Visualizer3D.h`
- `src/viewer/Visualizer3D.cpp`

### Functional Description
A real-time 3D visualization tool for the brain state. It likely renders neurons as point clouds and synapses as lines/arcs, color-coded by activation or type.

### Key Features
- **Real-time**: Updates with the simulation loop.
- **Interactive**: Allows navigation (zoom/rotate) to inspect specific regions.
- **Debug Info**: Overlays region names and statistics.

