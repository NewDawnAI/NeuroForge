# CUDAAccel API

Version: v0.15.0-rc1-12-gdf2999b5
Timestamp: 2025-12-08

## Overview
- Accelerated ops: Hebbian, STDP, leaky integrate, reduce mean, mitochondrial updates

## API
- nf_cuda_init()
- nf_cuda_hebbian(batch)
- nf_cuda_stdp(pairs)
- nf_cuda_leaky_integrate(state)
- nf_cuda_reduce_mean(vec)
- nf_cuda_mito_update(params)

## Batching
- Prefer batches ≥ 8k pairs for GPU; fallback to CPU below threshold

## Memory Layout
- SoA for spikes/times; contiguous weights; device buffers reused per step

## Errors & Fallbacks
- Probe availability; on error, emit WARN and switch to CPU path

## Performance
- Record timings; ensure parity within ±3% metrics across CPU/GPU for validated ops

