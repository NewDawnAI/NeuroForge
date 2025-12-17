#include "system_audio_capture.h"
#include <iostream>
#ifdef _WIN32
#include <initguid.h>
#include <ks.h>
#include <ksmedia.h>
#endif

namespace NeuroForge {
namespace Audio {

bool SystemAudioCapture::start() {
    #ifdef _WIN32
    if (running_.load()) return true;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        std::cerr << "WASAPI: CoInitializeEx failed" << std::endl;
        return false;
    }
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enum_));
    if (FAILED(hr)) {
        std::cerr << "WASAPI: MMDeviceEnumerator create failed" << std::endl;
        CoUninitialize();
        return false;
    }
    hr = enum_->GetDefaultAudioEndpoint(eRender, eConsole, &device_);
    if (FAILED(hr)) {
        std::cerr << "WASAPI: GetDefaultAudioEndpoint failed" << std::endl;
        enum_->Release(); enum_ = nullptr; CoUninitialize();
        return false;
    }
    hr = device_->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&client_);
    if (FAILED(hr)) {
        std::cerr << "WASAPI: IAudioClient activate failed" << std::endl;
        device_->Release(); device_ = nullptr; enum_->Release(); enum_ = nullptr; CoUninitialize();
        return false;
    }
    hr = client_->GetMixFormat(&mix_);
    if (FAILED(hr) || !mix_) {
        std::cerr << "WASAPI: GetMixFormat failed" << std::endl;
        client_->Release(); client_ = nullptr; device_->Release(); device_ = nullptr; enum_->Release(); enum_ = nullptr; CoUninitialize();
        return false;
    }
    REFERENCE_TIME dur = 0;
    hr = client_->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, mix_, nullptr);
    if (FAILED(hr)) {
        std::cerr << "WASAPI: Initialize(loopback) failed" << std::endl;
        CoTaskMemFree(mix_); mix_ = nullptr; client_->Release(); client_ = nullptr; device_->Release(); device_ = nullptr; enum_->Release(); enum_ = nullptr; CoUninitialize();
        return false;
    }
    hr = client_->GetService(IID_PPV_ARGS(&capture_));
    if (FAILED(hr)) {
        std::cerr << "WASAPI: GetService(IAudioCaptureClient) failed" << std::endl;
        client_->Release(); client_ = nullptr; device_->Release(); device_ = nullptr; enum_->Release(); enum_ = nullptr; CoUninitialize();
        return false;
    }
    hr = client_->Start();
    if (FAILED(hr)) {
        std::cerr << "WASAPI: Start failed" << std::endl;
        capture_->Release(); capture_ = nullptr; client_->Release(); client_ = nullptr; device_->Release(); device_ = nullptr; enum_->Release(); enum_ = nullptr; CoUninitialize();
        return false;
    }
    running_.store(true);
    th_ = std::thread(&SystemAudioCapture::loop, this);
    return true;
    #else
    return false;
    #endif
}

void SystemAudioCapture::stop() {
    #ifdef _WIN32
    if (!running_.load()) return;
    running_.store(false);
    if (th_.joinable()) th_.join();
    if (client_) client_->Stop();
    if (capture_) { capture_->Release(); capture_ = nullptr; }
    if (mix_) { CoTaskMemFree(mix_); mix_ = nullptr; }
    if (client_) { client_->Release(); client_ = nullptr; }
    if (device_) { device_->Release(); device_ = nullptr; }
    if (enum_) { enum_->Release(); enum_ = nullptr; }
    CoUninitialize();
    #endif
}

std::vector<float> SystemAudioCapture::fetch(std::size_t n) {
    std::vector<float> out(n, 0.0f);
    std::lock_guard<std::mutex> lg(mtx_);
    std::size_t take = std::min(n, ring_.size());
    for (std::size_t i = 0; i < take; ++i) {
        out[i] = ring_.front();
        ring_.pop_front();
    }
    return out;
}

void SystemAudioCapture::loop() {
    #ifdef _WIN32
    DWORD taskIndex = 0;
    HANDLE hTask = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);
    while (running_.load()) {
        UINT32 packet = 0;
        if (FAILED(capture_->GetNextPacketSize(&packet))) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        while (packet > 0) {
            BYTE* data = nullptr; UINT32 frames = 0; DWORD flags = 0; UINT64 pos = 0; UINT64 qpc = 0;
            HRESULT hr = capture_->GetBuffer(&data, &frames, &flags, &pos, &qpc);
            if (FAILED(hr)) break;
            if (frames > 0 && data) {
                std::lock_guard<std::mutex> lg(mtx_);
                const int ch = mix_->nChannels;
                const bool isFloat = (mix_->wFormatTag == WAVE_FORMAT_IEEE_FLOAT);
                if (isFloat) {
                    const float* f = reinterpret_cast<const float*>(data);
                    for (UINT32 i = 0; i < frames; ++i) {
                        float acc = 0.0f;
                        for (int c = 0; c < ch; ++c) acc += f[i * ch + c];
                        ring_.push_back(acc / static_cast<float>(ch));
                    }
                } else {
                    const std::int16_t* s = reinterpret_cast<const std::int16_t*>(data);
                    for (UINT32 i = 0; i < frames; ++i) {
                        long acc = 0;
                        for (int c = 0; c < ch; ++c) acc += s[i * ch + c];
                        ring_.push_back(static_cast<float>(acc) / (32768.0f * static_cast<float>(ch)));
                    }
                }
                std::size_t max_ring = static_cast<std::size_t>(mix_->nSamplesPerSec) * 2;
                while (ring_.size() > max_ring) ring_.pop_front();
            }
            capture_->ReleaseBuffer(frames);
            if (FAILED(capture_->GetNextPacketSize(&packet))) { packet = 0; }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (hTask) AvRevertMmThreadCharacteristics(hTask);
    #endif
}

} // namespace Audio
} // namespace NeuroForge
