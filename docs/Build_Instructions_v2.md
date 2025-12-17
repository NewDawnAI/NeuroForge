# NeuroForge Build Instructions v2.0

## Overview

This document provides comprehensive build instructions for NeuroForge v2.0, including the new acoustic-first language learning system, visual-linguistic integration, and speech production capabilities.

## Table of Contents

1. [System Requirements](#system-requirements)
2. [Dependencies](#dependencies)
3. [Build Environment Setup](#build-environment-setup)
4. [Building NeuroForge](#building-neuroforge)
5. [Testing the Build](#testing-the-build)
6. [Language System Features](#language-system-features)
7. [Troubleshooting Build Issues](#troubleshooting-build-issues)
8. [Platform-Specific Instructions](#platform-specific-instructions)

## System Requirements

### Hardware Requirements
- **CPU**: Multi-core processor (4+ cores recommended for real-time language processing)
- **Memory**: 8GB RAM minimum, 16GB recommended for large-scale simulations
- **Storage**: 5GB free space for build artifacts and dependencies
- **Audio**: Microphone and speakers for acoustic language learning features
- **Camera**: Optional, for visual-linguistic integration features

### Software Requirements
- **Operating System**: Windows 10/11 (primary), Linux Ubuntu 20.04+ (experimental)
- **Compiler**: 
  - Windows: Visual Studio 2022 (MSVC 17.0+)
  - Linux: GCC 11+ or Clang 13+
- **CMake**: Version 3.20 or higher
- **Git**: For repository cloning and version control

## Dependencies

### Core Dependencies
```bash
# Required for all builds
- CMake 3.20+
- C++20 compatible compiler
- Cap'n Proto (for serialization)
- SQLite3 (for memory persistence)
```

### Language System Dependencies
```bash
# Required for acoustic-first language learning
- Audio processing libraries (Windows: winmm, Linux: ALSA)
- Math libraries (for acoustic feature extraction)

# Optional for enhanced features
- OpenCV 4.0+ (for visual-linguistic integration)
- FFTW3 (for advanced acoustic processing)
```

### Dependency Installation

#### Windows (vcpkg - Recommended)
```powershell
# Install vcpkg if not already installed
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install

# Install NeuroForge dependencies
.\vcpkg install capnproto:x64-windows
.\vcpkg install sqlite3:x64-windows
.\vcpkg install opencv4:x64-windows  # Optional, for visual features

# Optional: Embedded browser for sandbox window
# Enables Edge WebView2 support via CMake's unofficial-webview2 target
.\vcpkg install unofficial-webview2:x64-windows
```

#### Sandbox Build Notes (Windows)
```powershell
# Prefer MSVC generators for WebView2 + WRL Callback support
cmake -S . -B build-vcpkg-msvc -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build-vcpkg-msvc --config Release --parallel

# Verify CLI help includes sandbox flags (run the MSVC binary)
& .\build-vcpkg-msvc\Release\neuroforge.exe --help | Select-String sandbox
```
##### Sandbox Init Wait Verification (New)
```powershell
# Run a minimal sandbox to observe readiness
& .\build-vcpkg-msvc\Release\neuroforge.exe --sandbox=on --sandbox-url=https://www.youtube.com --steps=1

# Expected behavior:
# - Window appears and Edge WebView2 initializes
# - NavigationStarting fires once
# - Bounds update occurs (WM_SIZE/WM_PAINT)
# The main loop starts only after these readiness conditions.
```

#### Linux (apt)
```bash
# Update package list
sudo apt update

# Install build tools
sudo apt install build-essential cmake git

# Install core dependencies
sudo apt install libcapnp-dev libsqlite3-dev

# Install audio dependencies
sudo apt install libasound2-dev

# Install optional visual dependencies
sudo apt install libopencv-dev  # Optional, for visual features
```

## Build Environment Setup

### Windows Setup
```powershell
# Set up Visual Studio environment
# Open "Developer Command Prompt for VS 2022"

# Set vcpkg toolchain (if using vcpkg)
set CMAKE_TOOLCHAIN_FILE=C:\vcpkg\scripts\buildsystems\vcpkg.cmake

# Clone NeuroForge repository
git clone https://github.com/your-repo/NeuroForge.git
cd NeuroForge
 
# Recommended: Configure a dedicated MSVC build folder with vcpkg toolchain
cmake -S . -B build-msvc-vs -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
```

### Linux Setup
```bash
# Install additional development tools
sudo apt install pkg-config

# Clone NeuroForge repository
git clone https://github.com/your-repo/NeuroForge.git
cd NeuroForge

# Set up environment variables (optional)
export CC=gcc-11
export CXX=g++-11
```

## Building NeuroForge

### Standard Build Process

#### 1. Configure Build
```bash
# Create build directory
mkdir build
cd build

# Configure with CMake (Windows with vcpkg)
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release

# Configure with CMake (Linux)
cmake .. -DCMAKE_BUILD_TYPE=Release
```

#### 2. Build Options
```bash
# Enable/disable specific features
cmake .. -DNEUROFORGE_ENABLE_TESTS=ON \
         -DNEUROFORGE_ENABLE_GPU=OFF \
         -DNEUROFORGE_ENABLE_VISION_DEMO=ON \
         -DNEUROFORGE_WITH_VIEWER=OFF
```

#### 3. Compile
```bash
# Build all targets
cmake --build . --config Release

# Build specific targets
cmake --build . --config Release --target neuroforge_core
cmake --build . --config Release --target test_acoustic_language
cmake --build . --config Release --target test_speech_production
```

### Language System Specific Build

#### Build Core Language System
```bash
# Build core language system with all features
cmake --build . --config Release --target neuroforge_core

# Verify language system files are included
ls -la Release/  # Should include LanguageSystem*.dll/so files
```

#### Build Language Tests
```bash
# Build all language system tests
cmake --build . --config Release --target test_acoustic_language
cmake --build . --config Release --target test_visual_language_integration
cmake --build . --config Release --target test_speech_production

# Build traditional language tests for comparison
cmake --build . --config Release --target test_language
```

### Build Verification

#### Check Build Artifacts
```bash
# Windows
dir Release\*.exe
dir Release\*.dll

# Linux
ls -la Release/
ldd Release/neuroforge  # Check shared library dependencies
```

#### Verify Language System Integration
```bash
# Check that language system files are present
# Windows
dir src\core\LanguageSystem*.cpp
dir include\core\LanguageSystem.h

# Linux
ls -la src/core/LanguageSystem*.cpp
ls -la include/core/LanguageSystem.h
```

## Testing the Build

### Basic System Test
```bash
# Test basic system functionality
cd build
.\Release\neuroforge.exe --help

# Should display help with language system options
# Look for: --acoustic-preprocessing, --speech-output, --visual-integration
```

### Language System Tests

#### 1. Acoustic Language Learning Test
```bash
# Run acoustic-first language learning test
.\Release\test_acoustic_language.exe

# Expected output:
# === NeuroForge Acoustic-First Language System Tests ===
# Test 1: Acoustic-First Babbling... PASSED
# Test 2: Acoustic Teacher Signal Processing... PASSED
# Test 3: Prosodic Salience Detection... PASSED
# Test 4: Audio Snippet Generation... PASSED
# Test 5: Cohesion Improvement Measurement... PASSED
```

#### 2. Visual-Linguistic Integration Test
```bash
# Run visual-linguistic integration test
.\Release\test_visual_language_integration.exe

# Expected output:
# === NeuroForge Visual-Linguistic Integration Tests ===
# Test 1: Face-Speech Coupling... PASSED
# Test 2: Visual Attention Integration... PASSED
# Test 3: Cross-Modal Pattern Retrieval... PASSED
# Test 4: Face-Language Confidence... PASSED
# Test 5: Cross-Modal Association Decay... PASSED
# Test 6: Integrated Face-Speech Learning... PASSED
```

#### 3. Speech Production Test
```bash
# Run speech production and multimodal output test
.\Release\test_speech_production.exe

# Expected output:
# === NeuroForge Speech Production and Multimodal Output Tests ===
# Test 1: Phoneme Sequence Generation... PASSED
# Test 2: Lip Motion Generation... PASSED
# Test 3: Prosody Contour Generation... PASSED
# Test 4: Speech Production Features Generation... PASSED
# Test 5: Speech Production Control... PASSED
# Test 6: Self-Monitoring and Feedback... PASSED
# Test 7: Caregiver Mimicry Reinforcement... PASSED
# Test 8: Joint Attention Learning... PASSED
```

### Integration Test
```bash
# Run comprehensive integration test
.\Release\test_language.exe

# This tests the complete language system integration
# Should show compatibility with existing systems
```

### Performance Test
```bash
# Test with acoustic preprocessing enabled
.\Release\neuroforge.exe --acoustic-preprocessing=on --steps=1000

# Test with visual integration enabled (requires camera)
.\Release\neuroforge.exe --visual-integration=on --face-detection=on --steps=500

# Test with speech production enabled (requires speakers)
.\Release\neuroforge.exe --speech-output=on --lip-sync=on --steps=100
```

## Language System Features

### Acoustic-First Language Learning
```bash
# Enable acoustic preprocessing
.\Release\neuroforge.exe --acoustic-preprocessing=on \
                         --prosodic-embeddings=on \
                         --sound-attention-bias=on

# Configure acoustic parameters
.\Release\neuroforge.exe --motherese-boost=0.6 \
                         --intonation-threshold=0.4 \
                         --formant-clustering=40.0
```

### Visual-Linguistic Integration
```bash
# Enable visual-linguistic features (requires OpenCV)
.\Release\neuroforge.exe --visual-integration=on \
                         --face-language-bias=on \
                         --gaze-coordination=on

# Configure visual parameters
.\Release\neuroforge.exe --face-language-coupling=0.7 \
                         --lip-sync-threshold=0.4 \
                         --visual-grounding-boost=0.6
```

### Speech Production System
```bash
# Enable speech production (requires audio output)
.\Release\neuroforge.exe --speech-output=on \
                         --lip-sync=on \
                         --gaze-coordination=on

# Configure speech parameters
.\Release\neuroforge.exe --speech-rate=1.2 \
                         --lip-sync-precision=0.9 \
                         --self-monitoring-weight=0.5
```

### Combined Multimodal System
```bash
# Enable all language features
.\Release\neuroforge.exe --acoustic-preprocessing=on \
                         --visual-integration=on \
                         --speech-output=on \
                         --face-language-bias=on \
                         --steps=2000
```

## Troubleshooting Build Issues

### Common Build Errors

#### 1. M_PI Undeclared (Windows)
```
Error: 'M_PI': undeclared identifier
```
**Solution**: This is automatically handled in v2.0, but if you encounter it:
```cpp
// Add to problematic files
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
```

#### 2. OpenCV Not Found
```
Error: Could not find OpenCV
```
**Solution**:
```bash
# Windows (vcpkg)
vcpkg install opencv4:x64-windows

# Linux
sudo apt install libopencv-dev

# Or disable visual features
cmake .. -DNEUROFORGE_ENABLE_VISION_DEMO=OFF
```

#### 3. Audio Library Linking Issues
```
Error: undefined reference to 'waveInOpen'
```
**Solution**:
```bash
# Windows: Ensure winmm is linked (automatic in v2.0)
# Linux: Install ALSA development libraries
sudo apt install libasound2-dev
```

#### 4. C++20 Standard Issues
```
Error: This file requires compiler and library support for C++20
```
**Solution**:
```bash
# Update compiler
# Windows: Use Visual Studio 2022
# Linux: Use GCC 11+ or Clang 13+

# Verify C++20 support
cmake .. -DCMAKE_CXX_STANDARD=20
```

### Build Configuration Issues

#### Missing Language System Files
```bash
# Verify all language system files are present
ls src/core/LanguageSystem*.cpp
# Should show:
# - LanguageSystem.cpp
# - LanguageSystem_Acoustic.cpp
# - LanguageSystem_Visual.cpp
# - LanguageSystem_SpeechProduction.cpp
```

#### CMake Configuration Problems
```bash
# Clean build directory
rm -rf build/*
cd build

# Reconfigure with verbose output
cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON

# Check for language system targets
cmake --build . --target help | grep -i language
```

#### WebView2 not found after installing via vcpkg
```
Message: WebView2 not found: sandbox will use plain window fallback
```
**Cause**: The vcpkg toolchain was not used because the build directory already existed.

**Fix**:
```powershell
# Delete the existing build folder and reconfigure with the vcpkg toolchain
Remove-Item -Recurse -Force build-msvc-vs
cmake -S . -B build-msvc-vs -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
```
Expected CMake output:
- "WebView2 found: enabling embedded browser sandbox"
- No "CMAKE_TOOLCHAIN_FILE was not used" warnings

### Runtime Issues

#### Audio Device Access
```bash
# Windows: Check microphone permissions
# Settings > Privacy > Microphone > Allow apps to access microphone

# Linux: Check ALSA configuration
arecord -l  # List recording devices
aplay -l   # List playback devices
```

#### Camera Access (for visual features)
```bash
# Windows: Check camera permissions
# Settings > Privacy > Camera > Allow apps to access camera

# Linux: Check video devices
ls /dev/video*
v4l2-ctl --list-devices
```

## Platform-Specific Instructions

### Windows-Specific

#### Visual Studio Configuration
```bash
# Use Developer Command Prompt for VS 2022
# Set environment variables
set VCPKG_ROOT=C:\vcpkg
set CMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake

# Build with specific Visual Studio version
cmake .. -G "Visual Studio 17 2022" -A x64
```

#### Windows Audio Setup
```bash
# Ensure Windows Multimedia API is available
# This is automatically linked in v2.0 via winmm library
```

### Linux-Specific

#### Ubuntu/Debian Setup
```bash
# Install all dependencies at once
sudo apt install build-essential cmake git \
                 libcapnp-dev libsqlite3-dev \
                 libasound2-dev libopencv-dev \
                 pkg-config

# Set up audio permissions
sudo usermod -a -G audio $USER
# Logout and login for group changes to take effect
```

#### Audio Configuration
```bash
# Test ALSA audio
speaker-test -c 2 -t wav
arecord -d 3 test.wav && aplay test.wav

# Configure PulseAudio (if needed)
pulseaudio --check -v
```

### Cross-Platform Considerations

#### Endianness and Architecture
```bash
# The language system is designed to be portable
# No special configuration needed for different architectures
```

#### File Path Separators
```bash
# Automatically handled by CMake and C++ standard library
# No manual configuration required
```

## Advanced Build Options

### Debug Build with Language Features
```bash
# Build debug version with full language system
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DNEUROFORGE_ENABLE_TESTS=ON \
         -DNEUROFORGE_ENABLE_VISION_DEMO=ON

cmake --build . --config Debug
```

### Optimized Release Build
```bash
# Build optimized release with all features
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG" \
         -DNEUROFORGE_ENABLE_TESTS=ON

cmake --build . --config Release
```

### Custom Feature Configuration
```bash
# Build with specific language features only
cmake .. -DNEUROFORGE_ACOUSTIC_ONLY=ON \
         -DNEUROFORGE_VISUAL_INTEGRATION=OFF \
         -DNEUROFORGE_SPEECH_PRODUCTION=OFF
```

## Continuous Integration

### Automated Testing
```bash
# Run all tests after build
ctest --output-on-failure

# Run specific language system tests
ctest -R "acoustic_language|visual_language|speech_production"
```

### Build Verification Script
```bash
#!/bin/bash
# verify_build.sh

echo "Verifying NeuroForge v2.0 build..."

# Check executables
if [ -f "Release/neuroforge" ]; then
    echo "✅ Main executable found"
else
    echo "❌ Main executable missing"
    exit 1
fi

# Check language test executables
for test in test_acoustic_language test_visual_language_integration test_speech_production; do
    if [ -f "Release/$test" ]; then
        echo "✅ $test found"
    else
        echo "❌ $test missing"
        exit 1
    fi
done

# Run quick tests
echo "Running quick functionality tests..."
./Release/test_acoustic_language > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✅ Acoustic language system functional"
else
    echo "⚠️ Acoustic language system test failed"
fi

echo "Build verification complete!"
```

## Performance Optimization

### Compiler Optimizations
```bash
# Enable all optimizations
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CXX_FLAGS="-O3 -march=native -DNDEBUG"

# Enable link-time optimization
cmake .. -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

### Memory Optimization
```bash
# Configure for low-memory systems
cmake .. -DNEUROFORGE_LOW_MEMORY=ON \
         -DNEUROFORGE_MAX_VOCABULARY=5000 \
         -DNEUROFORGE_EMBEDDING_DIM=128
```

## Documentation Generation

### API Documentation
```bash
# Generate Doxygen documentation (if available)
doxygen Doxyfile

# Generate language system specific docs
doxygen docs/language_system.doxy
```

## Version Information

### Build Version
```bash
# Check build version
./Release/neuroforge --version

# Expected output:
# NeuroForge v2.0.0
# Language System: v2.0.0
# Build Date: [current date]
# Features: Acoustic+Visual+Speech
```

### Feature Detection
```bash
# Check available features
./Release/neuroforge --features

# Expected output:
# ✅ Acoustic Processing
# ✅ Visual Integration  
# ✅ Speech Production
# ✅ Cross-Modal Learning
# ✅ Self-Monitoring
```

---

**Last Updated**: December 2024  
**Version**: 2.0  
**Status**: Production Ready

For additional support, see [Language_System_Troubleshooting.md](Language_System_Troubleshooting.md) or contact the development team.
#### Verify Sandbox Window Integration
```powershell
# Confirm the correct binary and flags
Get-ChildItem -Recurse -Filter neuroforge.exe | ForEach-Object {
  Write-Host ("BIN: " + $_.FullName)
  try { (& $_.FullName --help) 2>&1 | Select-String -Pattern 'sandbox' -CaseSensitive:$false | ForEach-Object { $_.Line } }
  catch { Write-Host "Error: $($_.Exception.Message)" }
}

# Expected lines for the correct build:
#   --sandbox[=on|off]
#   --sandbox-url=URL
#   --sandbox-size=WxH
```
