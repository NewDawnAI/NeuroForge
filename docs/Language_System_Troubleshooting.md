# NeuroForge Language System Troubleshooting Guide

## Overview

This guide provides solutions to common issues encountered when integrating and using NeuroForge's acoustic-first language learning system. It covers compilation problems, runtime errors, performance issues, and integration challenges.

## Table of Contents

1. [Quick Diagnostics](#quick-diagnostics)
2. [Compilation Issues](#compilation-issues)
3. [Runtime Errors](#runtime-errors)
4. [Audio Processing Problems](#audio-processing-problems)
5. [Visual Integration Issues](#visual-integration-issues)
6. [Speech Production Problems](#speech-production-problems)
7. [Performance Issues](#performance-issues)
8. [Memory Problems](#memory-problems)
9. [Configuration Issues](#configuration-issues)
10. [Integration Problems](#integration-problems)
11. [Debug Tools and Logging](#debug-tools-and-logging)
12. [Known Issues and Workarounds](#known-issues-and-workarounds)

## Quick Diagnostics

### System Health Check

```cpp
// Basic system health check
bool checkLanguageSystemHealth(LanguageSystem& language_system) {
    try {
        // Test initialization
        if (!language_system.initialize()) {
            std::cerr << "❌ Initialization failed" << std::endl;
            return false;
        }
        
        // Test basic functionality
        language_system.performAcousticBabbling(1);
        auto stats = language_system.getStatistics();
        
        std::cout << "✅ Language system healthy" << std::endl;
        std::cout << "   Vocabulary size: " << stats.active_vocabulary_size << std::endl;
        std::cout << "   Current stage: " << static_cast<int>(stats.current_stage) << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ Health check failed: " << e.what() << std::endl;
        return false;
    }
}
```

### Version Information

```cpp
// Check version compatibility
void checkVersionInfo() {
    std::cout << "NeuroForge Language System" << std::endl;
    std::cout << "Version: " << NEUROFORGE_LANGUAGE_VERSION_MAJOR << "." 
              << NEUROFORGE_LANGUAGE_VERSION_MINOR << "." 
              << NEUROFORGE_LANGUAGE_VERSION_PATCH << std::endl;
    
    #ifdef NF_HAVE_OPENCV
    std::cout << "OpenCV: Available" << std::endl;
    #else
    std::cout << "OpenCV: Not available" << std::endl;
    #endif
    
    #ifdef NF_HAVE_SQLITE3
    std::cout << "SQLite3: Available" << std::endl;
    #else
    std::cout << "SQLite3: Not available" << std::endl;
    #endif
}
```

## Compilation Issues

### Issue: M_PI Undeclared Identifier

**Error Message**:
```
error C2065: 'M_PI': undeclared identifier
```

**Solution**:
```cpp
// Add to the top of your source files
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Or use the standard library constant (C++20)
#include <numbers>
// Use std::numbers::pi instead of M_PI
```

**Root Cause**: Windows MSVC doesn't define M_PI by default.

### Issue: Missing OpenCV Headers

**Error Message**:
```
fatal error: 'opencv2/opencv.hpp' file not found
```

**Solution**:
1. Install OpenCV via vcpkg:
```bash
vcpkg install opencv4
```

2. Update CMakeLists.txt:
```cmake
find_package(OpenCV REQUIRED)
target_link_libraries(your_target ${OpenCV_LIBS})
```

3. Or disable visual features:
```cpp
LanguageSystem::Config config;
config.enable_vision_grounding = false;
config.enable_face_language_bias = false;
```

### Issue: C++20 Standard Not Supported

**Error Message**:
```
error: 'std::chrono::steady_clock::time_point' requires C++20
```

**Solution**:
1. Update compiler flags:
```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

2. For older compilers, use compatibility mode:
```cpp
// Replace std::chrono::steady_clock::time_point with
using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
```

### Issue: Linking Errors with Audio Libraries

**Error Message**:
```
undefined reference to 'waveInOpen'
```

**Solution** (Windows):
```cmake
if(WIN32)
    target_link_libraries(your_target winmm)
endif()
```

**Solution** (Linux):
```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED alsa)
target_link_libraries(your_target ${ALSA_LIBRARIES})
```

## Runtime Errors

### Issue: Initialization Failure

**Error Message**:
```
LanguageSystem initialization failed
```

**Diagnostic Steps**:
```cpp
bool diagnoseInitialization(LanguageSystem& language_system) {
    try {
        // Check configuration
        auto config = language_system.getConfig();
        if (config.max_vocabulary_size == 0) {
            std::cerr << "❌ Invalid vocabulary size" << std::endl;
            return false;
        }
        
        if (config.embedding_dimension == 0) {
            std::cerr << "❌ Invalid embedding dimension" << std::endl;
            return false;
        }
        
        // Check memory availability
        std::size_t estimated_memory = config.max_vocabulary_size * 
                                      config.embedding_dimension * sizeof(float);
        std::cout << "Estimated memory usage: " << estimated_memory / 1024 / 1024 << " MB" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "❌ Initialization diagnostic failed: " << e.what() << std::endl;
        return false;
    }
}
```

**Solutions**:
1. Reduce memory requirements:
```cpp
config.max_vocabulary_size = 5000;  // Reduce from default 10000
config.embedding_dimension = 128;   // Reduce from default 256
```

2. Check system resources:
```cpp
// Monitor memory usage
#ifdef _WIN32
#include <windows.h>
MEMORYSTATUSEX memInfo;
memInfo.dwLength = sizeof(MEMORYSTATUSEX);
GlobalMemoryStatusEx(&memInfo);
std::cout << "Available memory: " << memInfo.ullAvailPhys / 1024 / 1024 << " MB" << std::endl;
#endif
```

### Issue: Token Creation Failures

**Error Message**:
```
Failed to create token: vocabulary full
```

**Solution**:
```cpp
// Check vocabulary capacity before creating tokens
auto stats = language_system.getStatistics();
if (stats.active_vocabulary_size >= config.max_vocabulary_size * 0.9f) {
    std::cout << "⚠️ Vocabulary nearly full, triggering cleanup" << std::endl;
    
    // Force vocabulary pruning
    language_system.pruneVocabulary();
    
    // Or increase capacity
    config.max_vocabulary_size *= 2;
    language_system.updateConfig(config);
}
```

### Issue: Segmentation Fault in Audio Processing

**Error Message**:
```
Segmentation fault in extractAcousticFeatures
```

**Diagnostic**:
```cpp
bool validateAudioInput(const std::vector<float>& audio_samples) {
    if (audio_samples.empty()) {
        std::cerr << "❌ Empty audio input" << std::endl;
        return false;
    }
    
    if (audio_samples.size() < 160) { // Minimum 10ms at 16kHz
        std::cerr << "❌ Audio too short: " << audio_samples.size() << " samples" << std::endl;
        return false;
    }
    
    // Check for invalid values
    for (float sample : audio_samples) {
        if (std::isnan(sample) || std::isinf(sample)) {
            std::cerr << "❌ Invalid audio sample detected" << std::endl;
            return false;
        }
        if (std::abs(sample) > 10.0f) {
            std::cerr << "⚠️ Unusually large audio sample: " << sample << std::endl;
        }
    }
    
    return true;
}
```

**Solution**:
```cpp
// Validate and sanitize audio input
std::vector<float> sanitizeAudio(const std::vector<float>& input) {
    std::vector<float> output;
    output.reserve(input.size());
    
    for (float sample : input) {
        if (std::isnan(sample) || std::isinf(sample)) {
            output.push_back(0.0f);
        } else {
            output.push_back(std::clamp(sample, -1.0f, 1.0f));
        }
    }
    
    return output;
}
```

## Audio Processing Problems

### Issue: Poor Acoustic Feature Extraction

**Symptoms**:
- Low pitch detection accuracy
- Incorrect formant frequencies
- Poor prosodic attention

**Diagnostic**:
```cpp
void diagnoseAudioQuality(const std::vector<float>& audio_samples) {
    // Check signal-to-noise ratio
    float signal_power = 0.0f;
    for (float sample : audio_samples) {
        signal_power += sample * sample;
    }
    signal_power /= audio_samples.size();
    
    std::cout << "Signal power: " << signal_power << std::endl;
    if (signal_power < 0.001f) {
        std::cout << "⚠️ Very low signal power - check microphone" << std::endl;
    }
    
    // Check for clipping
    int clipped_samples = 0;
    for (float sample : audio_samples) {
        if (std::abs(sample) > 0.95f) {
            clipped_samples++;
        }
    }
    
    float clipping_ratio = static_cast<float>(clipped_samples) / audio_samples.size();
    if (clipping_ratio > 0.01f) {
        std::cout << "⚠️ Audio clipping detected: " << clipping_ratio * 100 << "%" << std::endl;
    }
}
```

**Solutions**:
1. **Improve audio quality**:
```cpp
// Apply pre-processing
std::vector<float> preprocessAudio(const std::vector<float>& input) {
    std::vector<float> output = input;
    
    // Remove DC offset
    float mean = std::accumulate(output.begin(), output.end(), 0.0f) / output.size();
    for (float& sample : output) {
        sample -= mean;
    }
    
    // Apply gentle normalization
    float max_abs = 0.0f;
    for (float sample : output) {
        max_abs = std::max(max_abs, std::abs(sample));
    }
    
    if (max_abs > 0.0f && max_abs < 0.1f) {
        float gain = 0.5f / max_abs;
        for (float& sample : output) {
            sample *= gain;
        }
    }
    
    return output;
}
```

2. **Adjust feature extraction parameters**:
```cpp
config.formant_clustering_threshold = 30.0f;  // More sensitive clustering
config.prosody_attention_weight = 0.5f;       // Increase prosodic sensitivity
config.intonation_threshold = 0.3f;           // Lower threshold for attention
```

### Issue: Microphone Access Problems

**Error Message**:
```
Failed to open audio device
```

**Solutions**:

**Windows**:
```cpp
#include <windows.h>
#include <mmsystem.h>

bool checkMicrophoneAccess() {
    UINT num_devices = waveInGetNumDevs();
    if (num_devices == 0) {
        std::cerr << "❌ No audio input devices found" << std::endl;
        return false;
    }
    
    std::cout << "✅ Found " << num_devices << " audio input devices" << std::endl;
    
    // Test opening default device
    HWAVEIN hwi;
    WAVEFORMATEX wfx = {};
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 1;
    wfx.nSamplesPerSec = 16000;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    
    MMRESULT result = waveInOpen(&hwi, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "❌ Failed to open microphone: " << result << std::endl;
        return false;
    }
    
    waveInClose(hwi);
    std::cout << "✅ Microphone access confirmed" << std::endl;
    return true;
}
```

**Linux**:
```bash
# Check ALSA devices
arecord -l

# Test recording
arecord -d 1 -f cd test.wav && aplay test.wav
```

### Issue: Sample Rate Mismatch

**Symptoms**:
- Distorted pitch detection
- Incorrect timing patterns

**Solution**:
```cpp
// Resample audio to 16kHz
std::vector<float> resampleTo16kHz(const std::vector<float>& input, float input_rate) {
    if (std::abs(input_rate - 16000.0f) < 1.0f) {
        return input; // Already correct rate
    }
    
    float ratio = 16000.0f / input_rate;
    std::size_t output_size = static_cast<std::size_t>(input.size() * ratio);
    std::vector<float> output(output_size);
    
    // Simple linear interpolation resampling
    for (std::size_t i = 0; i < output_size; ++i) {
        float src_index = i / ratio;
        std::size_t src_i = static_cast<std::size_t>(src_index);
        float frac = src_index - src_i;
        
        if (src_i + 1 < input.size()) {
            output[i] = input[src_i] * (1.0f - frac) + input[src_i + 1] * frac;
        } else {
            output[i] = input[src_i];
        }
    }
    
    return output;
}
```

## Visual Integration Issues

### Issue: Face Detection Failures

**Symptoms**:
- Low face detection confidence
- No face-speech coupling

**Diagnostic**:
```cpp
void diagnoseFaceDetection(const cv::Mat& frame) {
    if (frame.empty()) {
        std::cerr << "❌ Empty camera frame" << std::endl;
        return;
    }
    
    std::cout << "Frame size: " << frame.cols << "x" << frame.rows << std::endl;
    std::cout << "Frame type: " << frame.type() << std::endl;
    
    // Check brightness
    cv::Scalar mean_brightness = cv::mean(frame);
    std::cout << "Mean brightness: " << mean_brightness[0] << std::endl;
    
    if (mean_brightness[0] < 50) {
        std::cout << "⚠️ Frame too dark for face detection" << std::endl;
    } else if (mean_brightness[0] > 200) {
        std::cout << "⚠️ Frame too bright for face detection" << std::endl;
    }
    
    // Check for motion blur
    cv::Mat gray, laplacian;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::Laplacian(gray, laplacian, CV_64F);
    cv::Scalar variance = cv::mean(laplacian);
    
    std::cout << "Sharpness score: " << variance[0] << std::endl;
    if (variance[0] < 100) {
        std::cout << "⚠️ Frame may be blurry" << std::endl;
    }
}
```

**Solutions**:
1. **Improve lighting conditions**:
```cpp
cv::Mat enhanceFrame(const cv::Mat& input) {
    cv::Mat enhanced;
    
    // Apply histogram equalization
    if (input.channels() == 1) {
        cv::equalizeHist(input, enhanced);
    } else {
        cv::Mat yuv;
        cv::cvtColor(input, yuv, cv::COLOR_BGR2YUV);
        std::vector<cv::Mat> channels;
        cv::split(yuv, channels);
        cv::equalizeHist(channels[0], channels[0]);
        cv::merge(channels, yuv);
        cv::cvtColor(yuv, enhanced, cv::COLOR_YUV2BGR);
    }
    
    return enhanced;
}
```

2. **Adjust detection parameters**:
```cpp
config.face_language_coupling = 0.4f;  // Lower threshold
config.lip_sync_threshold = 0.2f;      // More lenient lip sync
```

### Issue: Camera Access Problems

**Error Message**:
```
Failed to open camera device
```

**Solutions**:

**Windows**:
```cpp
bool testCameraAccess() {
    for (int i = 0; i < 5; ++i) {
        cv::VideoCapture cap(i);
        if (cap.isOpened()) {
            std::cout << "✅ Camera " << i << " available" << std::endl;
            
            cv::Mat frame;
            if (cap.read(frame) && !frame.empty()) {
                std::cout << "   Resolution: " << frame.cols << "x" << frame.rows << std::endl;
                return true;
            }
        }
    }
    
    std::cerr << "❌ No working cameras found" << std::endl;
    return false;
}
```

**Linux**:
```bash
# List video devices
ls /dev/video*

# Test camera with v4l2
v4l2-ctl --list-devices
v4l2-ctl --device=/dev/video0 --list-formats-ext
```

### Issue: Gaze Tracking Inaccuracy

**Symptoms**:
- Poor gaze alignment scores
- Inconsistent joint attention detection

**Solution**:
```cpp
// Implement gaze smoothing
class GazeTracker {
private:
    std::deque<cv::Point2f> gaze_history_;
    static constexpr size_t HISTORY_SIZE = 5;
    
public:
    cv::Point2f smoothGaze(const cv::Point2f& raw_gaze) {
        gaze_history_.push_back(raw_gaze);
        if (gaze_history_.size() > HISTORY_SIZE) {
            gaze_history_.pop_front();
        }
        
        // Calculate weighted average (recent samples have higher weight)
        cv::Point2f smoothed(0, 0);
        float total_weight = 0;
        
        for (size_t i = 0; i < gaze_history_.size(); ++i) {
            float weight = static_cast<float>(i + 1);
            smoothed += gaze_history_[i] * weight;
            total_weight += weight;
        }
        
        return smoothed / total_weight;
    }
};
```

## Speech Production Problems

### Issue: Choppy Speech Output

**Symptoms**:
- Interrupted audio playback
- Inconsistent lip sync

**Diagnostic**:
```cpp
void diagnoseSpeechProduction(LanguageSystem& language_system) {
    auto speech_state = language_system.getCurrentSpeechState();
    
    if (speech_state.is_speaking) {
        std::cout << "Current phoneme: " << speech_state.current_phoneme_index << std::endl;
        std::cout << "Time offset: " << speech_state.current_time_offset << "ms" << std::endl;
        std::cout << "Self-monitoring score: " << speech_state.self_monitoring_score << std::endl;
        
        if (speech_state.current_lip_shape.empty()) {
            std::cout << "⚠️ No lip shape data" << std::endl;
        }
        
        if (speech_state.self_monitoring_score < 0.3f) {
            std::cout << "⚠️ Low speech quality detected" << std::endl;
        }
    }
}
```

**Solutions**:
1. **Increase audio buffer size**:
```cpp
// Platform-specific audio buffer configuration
#ifdef _WIN32
// Increase DirectSound buffer size
const int BUFFER_SIZE = 4096;  // Increase from default 1024
#endif
```

2. **Use separate thread for audio**:
```cpp
class ThreadedSpeechProduction {
private:
    std::thread audio_thread_;
    std::atomic<bool> should_stop_{false};
    
public:
    void startProduction(const SpeechProductionFeatures& features) {
        audio_thread_ = std::thread([this, features]() {
            while (!should_stop_ && isPlaying()) {
                processAudioChunk();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    void stopProduction() {
        should_stop_ = true;
        if (audio_thread_.joinable()) {
            audio_thread_.join();
        }
    }
};
```

### Issue: Poor Lip Sync Quality

**Symptoms**:
- Lips don't match speech
- Delayed visual response

**Solution**:
```cpp
// Implement predictive lip sync
class PredictiveLipSync {
private:
    std::queue<std::vector<float>> lip_shape_queue_;
    std::chrono::steady_clock::time_point last_update_;
    
public:
    std::vector<float> getPredictedLipShape(float lookahead_ms = 50.0f) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_update_).count();
        
        // Predict lip shape based on timing
        if (!lip_shape_queue_.empty() && elapsed + lookahead_ms > 0) {
            return lip_shape_queue_.front();
        }
        
        // Return neutral shape if no prediction available
        return std::vector<float>(16, 0.3f);
    }
};
```

### Issue: Self-Monitoring Failures

**Symptoms**:
- No acoustic feedback processing
- Poor speech quality scores

**Solution**:
```cpp
// Implement robust self-monitoring
void robustSelfMonitoring(LanguageSystem& language_system) {
    try {
        // Capture self-audio with error handling
        std::vector<float> self_audio = captureSelfAudio();
        
        if (validateAudioInput(self_audio)) {
            language_system.processSelfAcousticFeedback(self_audio);
        } else {
            std::cout << "⚠️ Invalid self-audio, skipping feedback" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Self-monitoring error: " << e.what() << std::endl;
        
        // Fallback: reduce speech rate for better quality
        auto speech_state = language_system.getCurrentSpeechState();
        if (speech_state.self_monitoring_score < 0.5f) {
            adjustSpeechRate(0.8f);  // Slow down 20%
        }
    }
}
```

## Performance Issues

### Issue: High CPU Usage

**Symptoms**:
- System slowdown during language processing
- Dropped audio frames

**Diagnostic**:
```cpp
#include <chrono>

class PerformanceMonitor {
private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::string operation_name_;
    
public:
    PerformanceMonitor(const std::string& name) : operation_name_(name) {
        start_time_ = std::chrono::high_resolution_clock::now();
    }
    
    ~PerformanceMonitor() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time_).count();
        
        std::cout << operation_name_ << " took " << duration << " μs" << std::endl;
        
        if (duration > 10000) {  // > 10ms
            std::cout << "⚠️ Slow operation detected" << std::endl;
        }
    }
};

// Usage
void monitoredOperation() {
    PerformanceMonitor monitor("Acoustic Processing");
    language_system.extractAcousticFeatures(audio_data);
}
```

**Solutions**:
1. **Optimize processing frequency**:
```cpp
// Reduce processing frequency for non-critical operations
class AdaptiveProcessor {
private:
    std::chrono::steady_clock::time_point last_process_time_;
    float process_interval_ms_ = 50.0f;  // Start with 20 FPS
    
public:
    bool shouldProcess() {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_process_time_).count();
        
        if (elapsed >= process_interval_ms_) {
            last_process_time_ = now;
            return true;
        }
        return false;
    }
    
    void adjustInterval(float cpu_usage) {
        if (cpu_usage > 80.0f) {
            process_interval_ms_ *= 1.2f;  // Slow down
        } else if (cpu_usage < 50.0f) {
            process_interval_ms_ *= 0.9f;  // Speed up
        }
        
        process_interval_ms_ = std::clamp(process_interval_ms_, 16.0f, 200.0f);
    }
};
```

2. **Use smaller processing chunks**:
```cpp
config.embedding_dimension = 128;      // Reduce from 256
config.max_vocabulary_size = 5000;     // Reduce from 10000
config.cross_modal_decay = 0.02f;      // Faster cleanup
```

### Issue: Memory Leaks

**Symptoms**:
- Increasing memory usage over time
- System becomes unstable

**Diagnostic**:
```cpp
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>

void monitorMemoryUsage() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        std::cout << "Working set: " << pmc.WorkingSetSize / 1024 / 1024 << " MB" << std::endl;
        std::cout << "Peak working set: " << pmc.PeakWorkingSetSize / 1024 / 1024 << " MB" << std::endl;
        
        static SIZE_T last_working_set = 0;
        if (last_working_set > 0) {
            SIZE_T growth = pmc.WorkingSetSize - last_working_set;
            if (growth > 10 * 1024 * 1024) {  // > 10MB growth
                std::cout << "⚠️ Memory growth detected: " << growth / 1024 / 1024 << " MB" << std::endl;
            }
        }
        last_working_set = pmc.WorkingSetSize;
    }
}
#endif
```

**Solutions**:
1. **Enable automatic cleanup**:
```cpp
config.cross_modal_decay = 0.05f;      // Aggressive cleanup
config.token_decay_rate = 0.01f;       // Faster token pruning

// Manual cleanup trigger
void performCleanup(LanguageSystem& language_system) {
    auto stats = language_system.getStatistics();
    
    if (stats.active_vocabulary_size > config.max_vocabulary_size * 0.8f) {
        language_system.pruneVocabulary();
        std::cout << "Performed vocabulary cleanup" << std::endl;
    }
}
```

2. **Limit buffer sizes**:
```cpp
// In LanguageSystem configuration
config.max_acoustic_buffer_size = 100;    // Limit acoustic history
config.max_visual_buffer_size = 50;       // Limit visual history
config.max_association_history = 1000;    // Limit cross-modal associations
```

## Memory Problems

### Issue: Out of Memory Errors

**Error Message**:
```
std::bad_alloc: Memory allocation failed
```

**Solutions**:
1. **Reduce memory footprint**:
```cpp
// Minimal configuration for low-memory systems
LanguageSystem::Config low_memory_config;
low_memory_config.max_vocabulary_size = 2000;
low_memory_config.embedding_dimension = 64;
low_memory_config.enable_prosodic_embeddings = false;  // Saves memory
low_memory_config.cross_modal_decay = 0.1f;           // Aggressive cleanup
```

2. **Implement memory monitoring**:
```cpp
class MemoryGuard {
private:
    std::size_t max_memory_mb_;
    
public:
    MemoryGuard(std::size_t max_mb) : max_memory_mb_(max_mb) {}
    
    bool checkMemoryLimit() {
        #ifdef _WIN32
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            std::size_t current_mb = pmc.WorkingSetSize / 1024 / 1024;
            if (current_mb > max_memory_mb_) {
                std::cout << "⚠️ Memory limit exceeded: " << current_mb << " MB" << std::endl;
                return false;
            }
        }
        #endif
        return true;
    }
};
```

### Issue: Memory Fragmentation

**Symptoms**:
- Allocation failures despite available memory
- Performance degradation over time

**Solution**:
```cpp
// Use memory pools for frequent allocations
class AudioBufferPool {
private:
    std::queue<std::vector<float>> available_buffers_;
    std::mutex pool_mutex_;
    
public:
    std::vector<float> getBuffer(std::size_t size) {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        
        if (!available_buffers_.empty()) {
            auto buffer = std::move(available_buffers_.front());
            available_buffers_.pop();
            
            if (buffer.size() != size) {
                buffer.resize(size);
            }
            return buffer;
        }
        
        return std::vector<float>(size);
    }
    
    void returnBuffer(std::vector<float>&& buffer) {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        
        if (available_buffers_.size() < 10) {  // Limit pool size
            available_buffers_.push(std::move(buffer));
        }
    }
};
```

## Configuration Issues

### Issue: Invalid Configuration Parameters

**Error Message**:
```
Invalid configuration: parameter out of range
```

**Solution**:
```cpp
bool validateConfiguration(const LanguageSystem::Config& config) {
    bool valid = true;
    
    // Check ranges
    if (config.mimicry_learning_rate < 0.0f || config.mimicry_learning_rate > 1.0f) {
        std::cerr << "❌ Invalid mimicry_learning_rate: " << config.mimicry_learning_rate << std::endl;
        valid = false;
    }
    
    if (config.max_vocabulary_size == 0 || config.max_vocabulary_size > 100000) {
        std::cerr << "❌ Invalid max_vocabulary_size: " << config.max_vocabulary_size << std::endl;
        valid = false;
    }
    
    if (config.embedding_dimension == 0 || config.embedding_dimension > 2048) {
        std::cerr << "❌ Invalid embedding_dimension: " << config.embedding_dimension << std::endl;
        valid = false;
    }
    
    // Check feature dependencies
    if (config.enable_speech_output && !config.enable_acoustic_preprocessing) {
        std::cerr << "⚠️ Speech output requires acoustic preprocessing" << std::endl;
    }
    
    if (config.enable_face_language_bias && !config.enable_vision_grounding) {
        std::cerr << "⚠️ Face-language bias requires vision grounding" << std::endl;
    }
    
    return valid;
}
```

### Issue: Feature Conflicts

**Symptoms**:
- Unexpected behavior with certain feature combinations
- Performance issues with specific configurations

**Solution**:
```cpp
LanguageSystem::Config optimizeConfiguration(const LanguageSystem::Config& input) {
    LanguageSystem::Config optimized = input;
    
    // Resolve conflicts
    if (optimized.enable_speech_output && !optimized.enable_acoustic_preprocessing) {
        std::cout << "Auto-enabling acoustic preprocessing for speech output" << std::endl;
        optimized.enable_acoustic_preprocessing = true;
    }
    
    // Performance optimizations
    if (optimized.max_vocabulary_size > 20000) {
        std::cout << "Large vocabulary detected, adjusting decay rates" << std::endl;
        optimized.cross_modal_decay *= 2.0f;
        optimized.token_decay_rate *= 2.0f;
    }
    
    // Memory optimizations
    std::size_t estimated_memory = optimized.max_vocabulary_size * 
                                  optimized.embedding_dimension * sizeof(float);
    if (estimated_memory > 500 * 1024 * 1024) {  // > 500MB
        std::cout << "High memory usage predicted, reducing embedding dimension" << std::endl;
        optimized.embedding_dimension = std::min(optimized.embedding_dimension, 256ul);
    }
    
    return optimized;
}
```

## Integration Problems

### Issue: Thread Safety Violations

**Error Message**:
```
Access violation / Segmentation fault in multi-threaded code
```

**Solution**:
```cpp
// Ensure proper synchronization
class ThreadSafeLanguageSystem {
private:
    LanguageSystem language_system_;
    std::mutex system_mutex_;
    
public:
    void safeProcessAudio(const std::vector<float>& audio, const std::string& label) {
        std::lock_guard<std::mutex> lock(system_mutex_);
        language_system_.processAcousticTeacherSignal(audio, label, 1.0f);
    }
    
    void safeUpdateDevelopment(float delta_time) {
        std::lock_guard<std::mutex> lock(system_mutex_);
        language_system_.updateDevelopment(delta_time);
    }
    
    Statistics safeGetStatistics() {
        std::lock_guard<std::mutex> lock(system_mutex_);
        return language_system_.getStatistics();
    }
};
```

### Issue: Callback Integration Problems

**Symptoms**:
- Callbacks not triggered
- Inconsistent callback timing

**Solution**:
```cpp
// Implement robust callback system
class LanguageSystemCallbacks {
public:
    using TokenCreatedCallback = std::function<void(std::size_t, const std::string&)>;
    using StageChangedCallback = std::function<void(DevelopmentalStage, DevelopmentalStage)>;
    
private:
    std::vector<TokenCreatedCallback> token_callbacks_;
    std::vector<StageChangedCallback> stage_callbacks_;
    std::mutex callback_mutex_;
    
public:
    void registerTokenCallback(TokenCreatedCallback callback) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        token_callbacks_.push_back(callback);
    }
    
    void triggerTokenCreated(std::size_t token_id, const std::string& symbol) {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        for (auto& callback : token_callbacks_) {
            try {
                callback(token_id, symbol);
            } catch (const std::exception& e) {
                std::cerr << "Callback error: " << e.what() << std::endl;
            }
        }
    }
};
```

## Debug Tools and Logging

### Enable Verbose Logging

```cpp
// Enable detailed logging
void enableDebugLogging(LanguageSystem& language_system) {
    language_system.setLogLevel(LogLevel::DEBUG);
    language_system.enablePerformanceLogging(true);
    language_system.enableMemoryLogging(true);
}

// Custom logging implementation
class LanguageSystemLogger {
public:
    enum Level { DEBUG, INFO, WARNING, ERROR };
    
    static void log(Level level, const std::string& message) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S") << "] ";
        
        switch (level) {
            case DEBUG:   std::cout << "[DEBUG] "; break;
            case INFO:    std::cout << "[INFO]  "; break;
            case WARNING: std::cout << "[WARN]  "; break;
            case ERROR:   std::cout << "[ERROR] "; break;
        }
        
        std::cout << message << std::endl;
    }
};
```

### Performance Profiling

```cpp
// Profile critical operations
class OperationProfiler {
private:
    std::unordered_map<std::string, std::vector<double>> timings_;
    
public:
    void recordTiming(const std::string& operation, double duration_ms) {
        timings_[operation].push_back(duration_ms);
        
        // Keep only recent timings
        if (timings_[operation].size() > 100) {
            timings_[operation].erase(timings_[operation].begin());
        }
    }
    
    void printStatistics() {
        for (const auto& [operation, times] : timings_) {
            if (times.empty()) continue;
            
            double sum = std::accumulate(times.begin(), times.end(), 0.0);
            double avg = sum / times.size();
            double max_time = *std::max_element(times.begin(), times.end());
            
            std::cout << operation << ": avg=" << avg << "ms, max=" << max_time << "ms" << std::endl;
        }
    }
};
```

### Memory Debugging

```cpp
// Track memory allocations
class MemoryTracker {
private:
    std::atomic<std::size_t> total_allocated_{0};
    std::atomic<std::size_t> peak_allocated_{0};
    
public:
    void recordAllocation(std::size_t size) {
        total_allocated_ += size;
        
        std::size_t current = total_allocated_.load();
        std::size_t peak = peak_allocated_.load();
        
        while (current > peak && !peak_allocated_.compare_exchange_weak(peak, current)) {
            // Retry if another thread updated peak
        }
    }
    
    void recordDeallocation(std::size_t size) {
        total_allocated_ -= size;
    }
    
    void printStatistics() {
        std::cout << "Current allocation: " << total_allocated_.load() / 1024 / 1024 << " MB" << std::endl;
        std::cout << "Peak allocation: " << peak_allocated_.load() / 1024 / 1024 << " MB" << std::endl;
    }
};
```

## Known Issues and Workarounds

### Issue: Windows Audio Latency

**Description**: High audio latency on Windows systems affecting real-time processing.

**Workaround**:
```cpp
// Use WASAPI for lower latency on Windows
#ifdef _WIN32
#include <audioclient.h>

class LowLatencyAudio {
private:
    IAudioClient* audio_client_ = nullptr;
    
public:
    bool initializeLowLatency() {
        // Initialize WASAPI in exclusive mode for lower latency
        // Implementation details depend on your audio framework
        return true;
    }
};
#endif
```

### Issue: OpenCV Threading Conflicts

**Description**: OpenCV threading conflicts with language system threads.

**Workaround**:
```cpp
// Disable OpenCV threading
cv::setNumThreads(1);

// Or use OpenCV in separate process
class IsolatedVisionProcessor {
public:
    void processInSeparateProcess(const cv::Mat& frame) {
        // Use inter-process communication to isolate OpenCV
    }
};
```

### Issue: Memory Alignment on ARM Processors

**Description**: Performance issues on ARM processors due to memory alignment.

**Workaround**:
```cpp
// Ensure proper alignment for SIMD operations
alignas(16) std::vector<float> aligned_audio_buffer;

// Or use aligned allocator
template<typename T>
using aligned_vector = std::vector<T, boost::alignment::aligned_allocator<T, 16>>;
```

### Issue: Real-Time Constraints

**Description**: Missing real-time deadlines in time-critical applications.

**Workaround**:
```cpp
// Use priority scheduling
#ifdef _WIN32
SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
#endif

#ifdef __linux__
struct sched_param param;
param.sched_priority = 99;
pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
#endif

// Implement deadline monitoring
class DeadlineMonitor {
private:
    std::chrono::steady_clock::time_point deadline_;
    
public:
    void setDeadline(std::chrono::milliseconds timeout) {
        deadline_ = std::chrono::steady_clock::now() + timeout;
    }
    
    bool checkDeadline() {
        return std::chrono::steady_clock::now() < deadline_;
    }
};
```

## Getting Additional Help

### Diagnostic Information to Collect

When reporting issues, please include:

1. **System Information**:
```cpp
void collectSystemInfo() {
    std::cout << "=== System Information ===" << std::endl;
    std::cout << "OS: " << getOSVersion() << std::endl;
    std::cout << "Compiler: " << getCompilerVersion() << std::endl;
    std::cout << "NeuroForge Version: " << getNeuroForgeVersion() << std::endl;
    
    #ifdef NF_HAVE_OPENCV
    std::cout << "OpenCV Version: " << CV_VERSION << std::endl;
    #endif
    
    std::cout << "Available Memory: " << getAvailableMemory() << " MB" << std::endl;
    std::cout << "CPU Cores: " << std::thread::hardware_concurrency() << std::endl;
}
```

2. **Configuration Used**:
```cpp
void dumpConfiguration(const LanguageSystem::Config& config) {
    std::cout << "=== Configuration ===" << std::endl;
    std::cout << "max_vocabulary_size: " << config.max_vocabulary_size << std::endl;
    std::cout << "embedding_dimension: " << config.embedding_dimension << std::endl;
    std::cout << "enable_acoustic_preprocessing: " << config.enable_acoustic_preprocessing << std::endl;
    // ... dump all relevant config parameters
}
```

3. **Error Context**:
```cpp
void captureErrorContext(const std::exception& e) {
    std::cout << "=== Error Context ===" << std::endl;
    std::cout << "Exception: " << e.what() << std::endl;
    std::cout << "Stack trace: " << getStackTrace() << std::endl;
    std::cout << "System state: " << getSystemState() << std::endl;
}
```

### Contact Information

For additional support:
- **GitHub Issues**: [NeuroForge Repository Issues](https://github.com/your-repo/NeuroForge/issues)
- **Documentation**: [Complete Documentation](README.md)
- **Email Support**: anoldeb15632665@gmail.com

---

**Last Updated**: December 2024  
**Version**: 2.0  
**Status**: Production Ready

This troubleshooting guide is continuously updated based on user feedback and newly discovered issues.
## Phase A Exports & Teacher Embeddings

### Issue: `rewards.csv`, `phase_a_teacher.csv`, `phase_a_student.csv` are empty

**Cause**:
- Exports are generated post‑run. During an ongoing ADS‑1/Phase A session, CSVs can remain empty.

**Verify**:
- Check MemoryDB contents:
```powershell
# Rewards
sqlite3 .\experiments\ADS1.db "SELECT COUNT(*) FROM reward_log;"
# Typed substrate states
sqlite3 .\experiments\ADS1.db "SELECT COUNT(*) FROM substrate_states WHERE state_type='phase_a_teacher';"
sqlite3 .\experiments\ADS1.db "SELECT COUNT(*) FROM substrate_states WHERE state_type='phase_a_student';"
```

**Export**:
```powershell
python tools\export_embeddings_rewards.py --db .\experiments\ADS1.db --out_dir .\exports\ADS1
```

### Issue: Teacher embeddings not loading from JSON files

**Symptoms**:
- Zero similarity/reward; export files remain empty; warnings about teacher vector length.

**Solution**:
- `--teacher-embed=PATH` now supports JSON arrays (e.g., `[0.12, -0.34, ...]`), CSV, and whitespace‑separated floats. The loader preserves numeric characters (`0–9`, `-`, `+`, `.`, `e`, `E`) and treats all other characters as separators.
- Provide a valid numeric array and ensure `--substrate-mode=native|mirror|train` with `--mimicry-internal=off` to route rewards externally.

**Example**:
```powershell
& ".\neuroforge.exe" --mimicry=on --mirror-mode=vision --substrate-mode=native ^
  --teacher-embed .\data\teacher_clip_vec.json --reward-scale 1.0 --memory-db .\experiments\ADS1.db --steps 1000
```
