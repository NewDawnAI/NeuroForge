# GPU Acceleration (Optional)

NeuroForge supports optional CUDA acceleration for hot learning loops (Hebbian, STDP). The CPU path remains the default and is unchanged when CUDA is disabled.

## Requirements
- NVIDIA GPU with Compute Capability ≥ 7.5 (e.g., RTX 20xx/30xx)
- CUDA Toolkit 11.8+ installed and in PATH (`nvcc --version`)

## Build with CUDA

```powershell
cmake -B build-cuda -DENABLE_CUDA=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-cuda --config Release --parallel
```

## Run with GPU preference

```powershell
.\build-cuda\neuroforge.exe --gpu --unified-substrate=on --enable-learning --steps 1000
```

## Notes
- GPU fast-path is gated by `--gpu`, batch size thresholds, and device availability.
- CPU semantics are preserved via per-synapse guardrails and the same weight clamping logic.
- If CUDA is not available, the system falls back to CPU automatically.

## Windows Toolchain Tips (Visual Studio + CUDA)

On Windows, NVCC typically uses the Microsoft C++ toolchain as its host compiler. Ensure these are in place:

- Install Visual Studio 2022 with “Desktop development with C++,” or Visual Studio 2022 Build Tools with the C++ workload.
- During CUDA Toolkit installation, select “Visual Studio Integration” for your VS version (e.g., VS 2022). This installs CUDA build customizations (`CUDA <ver>.props/targets`).
- Configure with the Visual Studio generator and CUDA toolset:

```powershell
cmake -B build-cuda-vs -G "Visual Studio 17 2022" -A x64 -T cuda=11.8 -DENABLE_CUDA=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=75
cmake --build build-cuda-vs --config Release --parallel
```

- Alternatively, for Ninja: ensure the MSVC toolchain is on PATH (e.g., open “x64 Native Tools Command Prompt for VS 2022” which runs `vcvars64.bat`). Then:

```powershell
cmake -B build-cuda-ninja -G "Ninja" -DENABLE_CUDA=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_CUDA_ARCHITECTURES=75
cmake --build build-cuda-ninja --parallel
```

- Common errors and fixes:
  - `nvcc fatal : Cannot find compiler 'cl.exe' in PATH` → open VS dev prompt or run `vcvars64.bat`.
  - Missing `CUDA 11.8.props` under VS build customizations → re-run CUDA installer with VS integration enabled.
