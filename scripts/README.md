# NeuroForge Performance Benchmark Scripts

This directory contains scripts and tools for benchmarking the performance of the NeuroForge neural substrate architecture.

## Files

### `benchmark_performance.cpp`
The main C++ benchmark application that measures various aspects of the NeuroForge system:

- **Memory Management**: Tests memory allocation, deallocation, and optimization
- **Neural Processing**: Measures neuron processing rates, synapse updates, and activation latency
- **Language Processing**: Benchmarks language model performance and memory usage
- **Worker Threads**: Tests multi-threaded task processing and optimization
- **Overall System**: Comprehensive system-wide performance metrics
- **Resource Usage**: CPU, memory, and I/O utilization tracking

### `CMakeLists.txt`
CMake configuration file for building the benchmark application. Supports:
- Cross-platform compilation (Windows, Linux, macOS)
- Debug and Release build configurations
- Automatic dependency detection
- Custom build targets

### `run_benchmark.ps1`
PowerShell script for Windows that automates the build and execution process:
- Automatic CMake configuration and building
- Multiple benchmark iterations
- Performance summary generation
- Result file management
- Error handling and validation

## Usage

### Windows (PowerShell)

#### Basic Usage
```powershell
# Run single benchmark with default settings
.\run_benchmark.ps1

# Run 5 iterations in Release mode
.\run_benchmark.ps1 Release 5

# Clean build and run with verbose output
.\run_benchmark.ps1 -BuildType Release -Iterations 3 -CleanBuild -Verbose
```

#### Parameters
- `BuildType`: Build configuration (`Debug` or `Release`, default: `Release`)
- `Iterations`: Number of benchmark runs (default: `1`)
- `CleanBuild`: Force clean rebuild (switch)
- `Verbose`: Enable verbose output (switch)

### Manual Build (Cross-platform)

#### Prerequisites
- CMake 3.16 or higher
- C++17 compatible compiler
- Threading library support

#### Build Steps
```bash
# Create build directory
mkdir build/benchmark
cd build/benchmark

# Configure CMake
cmake -S ../../scripts -B . -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . --config Release

# Run benchmark
./bin/benchmark_performance  # Linux/macOS
# or
.\bin\benchmark_performance.exe  # Windows
```

## Output Files

The benchmark generates several output files:

### `benchmark_results.txt`
Detailed performance metrics including:
- Individual test results
- Timing measurements
- Resource utilization data
- Optimization effectiveness scores

### `benchmark_summary.txt`
High-level summary containing:
- Overall performance scores
- Comparison metrics
- System information
- Execution statistics

## Benchmark Metrics

### Memory Management
- **Allocation Speed**: Time to allocate memory blocks
- **Deallocation Speed**: Time to free memory
- **Fragmentation**: Memory fragmentation percentage
- **Pool Efficiency**: Memory pool utilization rate

### Neural Processing
- **Neurons/Second**: Number of neurons processed per second
- **Synapses/Second**: Synapse updates per second
- **Activation Latency**: Average time for neuron activation
- **Propagation Delay**: Signal propagation timing

### Language Processing
- **Token Processing Rate**: Tokens processed per second
- **Memory Efficiency**: Memory usage per token
- **Model Loading Time**: Time to load language models
- **Inference Speed**: Time per inference operation

### Worker Threads
- **Task Throughput**: Tasks completed per second
- **Thread Utilization**: CPU usage across threads
- **Queue Efficiency**: Task queue management performance
- **Synchronization Overhead**: Thread coordination costs

### System Metrics
- **CPU Usage**: Processor utilization percentage
- **Memory Usage**: RAM consumption
- **Cache Hit Rate**: CPU cache effectiveness
- **I/O Throughput**: Disk and network performance

## Performance Optimization

The benchmark helps identify optimization opportunities:

1. **Bottleneck Detection**: Identifies performance bottlenecks
2. **Resource Utilization**: Shows underutilized resources
3. **Scaling Analysis**: Tests performance under different loads
4. **Optimization Validation**: Measures improvement from optimizations

## Troubleshooting

### Common Issues

#### Build Failures
- Ensure CMake 3.16+ is installed
- Verify C++17 compiler support
- Check that all dependencies are available

#### Runtime Errors
- Verify sufficient system resources
- Check file permissions for output directories
- Ensure no conflicting processes are running

#### Performance Issues
- Run in Release mode for accurate measurements
- Close unnecessary applications during benchmarking
- Ensure stable system load during testing

### Debug Mode
For debugging benchmark issues:
```powershell
.\run_benchmark.ps1 Debug 1 -Verbose
```

This enables:
- Debug symbols
- Verbose logging
- Additional error checking
- Performance profiling hooks

## Integration

The benchmark can be integrated into:
- **CI/CD Pipelines**: Automated performance regression testing
- **Development Workflow**: Regular performance validation
- **Optimization Cycles**: Before/after performance comparison
- **System Monitoring**: Continuous performance tracking

## Contributing

When adding new benchmark tests:
1. Follow the existing code structure
2. Add appropriate documentation
3. Include error handling
4. Ensure cross-platform compatibility
5. Update this README with new metrics