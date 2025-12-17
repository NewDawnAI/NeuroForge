#pragma once

#include <vector>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <thread>
#include <deque>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "Avrt.lib")
#pragma comment(lib, "Mmdevapi.lib")
#endif

namespace NeuroForge {
namespace Audio {

class SystemAudioCapture {
public:
    struct Config { std::uint32_t sample_rate{48000}; std::uint16_t channels{2}; };

    explicit SystemAudioCapture(const Config& cfg) : cfg_(cfg) {}
    ~SystemAudioCapture() { stop(); }

    bool start();
    void stop();
    std::vector<float> fetch(std::size_t n);

private:
    Config cfg_{};
    std::atomic<bool> running_{false};
    std::mutex mtx_;
    std::thread th_;
    std::deque<float> ring_;

    #ifdef _WIN32
    IMMDeviceEnumerator* enum_{nullptr};
    IMMDevice* device_{nullptr};
    IAudioClient* client_{nullptr};
    IAudioCaptureClient* capture_{nullptr};
    WAVEFORMATEX* mix_{nullptr};
    HANDLE event_{nullptr};
    #endif

    void loop();
};

} // namespace Audio
} // namespace NeuroForge

