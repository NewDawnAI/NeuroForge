#include "audio_capture.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <chrono>

namespace NeuroForge {
namespace Audio {

AudioCapture::AudioCapture(const Config& config) 
    : config_(config), capturing_(false), initialized_(false), current_buffer_(0) {
    
#ifdef _WIN32
    wave_in_ = nullptr;
    wave_headers_ = nullptr;
#endif
    
    // Calculate envelope window in samples
    envelope_window_samples_ = static_cast<size_t>(
        (config_.envelope_window_ms / 1000.0f) * config_.sample_rate);
    
    // Initialize audio buffers
    size_t buffer_size_samples = (config_.sample_rate * config_.buffer_size_ms) / 1000;
    audio_buffers_.resize(config_.num_buffers);
    for (auto& buffer : audio_buffers_) {
        buffer.resize(buffer_size_samples * config_.channels);
    }
}

AudioCapture::~AudioCapture() {
    stopCapture();
    cleanupWindows();
}

bool AudioCapture::initialize() {
    if (initialized_.load()) {
        return true;
    }
    
    std::cout << "Initializing audio capture system..." << std::endl;
    
#ifdef _WIN32
    if (!initializeWindows()) {
        std::cerr << "Failed to initialize Windows audio system" << std::endl;
        return false;
    }
#else
    std::cerr << "Audio capture not implemented for this platform" << std::endl;
    return false;
#endif
    
    initialized_.store(true);
    std::cout << "Audio capture initialized: " 
              << config_.sample_rate << "Hz, " 
              << config_.channels << " channels, "
              << config_.bits_per_sample << " bits" << std::endl;
    
    return true;
}

bool AudioCapture::startCapture() {
    if (!initialized_.load()) {
        std::cerr << "Audio capture not initialized" << std::endl;
        return false;
    }
    
    if (capturing_.load()) {
        return true;  // Already capturing
    }
    
    std::cout << "Starting audio capture..." << std::endl;
    
#ifdef _WIN32
    // Start recording
    MMRESULT result = waveInStart(wave_in_);
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to start audio recording: " << result << std::endl;
        return false;
    }
#endif
    
    capturing_.store(true);
    
    // Start processing thread
    processing_thread_ = std::thread(&AudioCapture::processingThreadFunc, this);
    
    std::cout << "Audio capture started successfully" << std::endl;
    return true;
}

void AudioCapture::stopCapture() {
    if (!capturing_.load()) {
        return;
    }
    
    std::cout << "Stopping audio capture..." << std::endl;
    
    capturing_.store(false);
    
#ifdef _WIN32
    if (wave_in_) {
        waveInStop(wave_in_);
        waveInReset(wave_in_);
    }
#endif
    
    // Wait for processing thread to finish
    if (processing_thread_.joinable()) {
        processing_thread_.join();
    }
    
    std::cout << "Audio capture stopped" << std::endl;
}

AudioCapture::AudioData AudioCapture::getLatestAudio(std::uint32_t max_age_ms) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    if (audio_queue_.empty()) {
        return AudioData();
    }
    
    // Get the most recent audio data
    AudioData latest = audio_queue_.back();
    
    // Check if it's too old
    std::uint64_t current_time = getCurrentTimeMs();
    if (current_time - latest.timestamp_ms > max_age_ms) {
        return AudioData();  // Too old
    }
    
    return latest;
}

std::vector<float> AudioCapture::getAudioEnvelope(float window_ms) {
    auto audio_data = getLatestAudio();
    if (audio_data.samples.empty()) {
        return std::vector<float>();
    }
    
    return calculateEnvelope(audio_data.samples);
}

float AudioCapture::estimateSpeechProbability(const AudioData& audio_data) {
    if (audio_data.samples.empty()) {
        return 0.0f;
    }
    
    return detectSpeechPattern(audio_data.samples);
}

size_t AudioCapture::getQueueSize() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queue_mutex_));
    return audio_queue_.size();
}

void AudioCapture::clearQueue() {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(queue_mutex_));
    audio_queue_.clear();
}

#ifdef _WIN32
bool AudioCapture::initializeWindows() {
    // Set up wave format
    wave_format_.wFormatTag = WAVE_FORMAT_PCM;
    wave_format_.nChannels = config_.channels;
    wave_format_.nSamplesPerSec = config_.sample_rate;
    wave_format_.wBitsPerSample = config_.bits_per_sample;
    wave_format_.nBlockAlign = (config_.channels * config_.bits_per_sample) / 8;
    wave_format_.nAvgBytesPerSec = wave_format_.nSamplesPerSec * wave_format_.nBlockAlign;
    wave_format_.cbSize = 0;
    
    // Open wave input device
    MMRESULT result = waveInOpen(&wave_in_, WAVE_MAPPER, &wave_format_,
                                (DWORD_PTR)waveInProc, (DWORD_PTR)this,
                                CALLBACK_FUNCTION);
    
    if (result != MMSYSERR_NOERROR) {
        std::cerr << "Failed to open wave input device: " << result << std::endl;
        return false;
    }
    
    // Prepare wave headers
    wave_headers_ = new WAVEHDR[config_.num_buffers];
    for (std::uint32_t i = 0; i < config_.num_buffers; ++i) {
        wave_headers_[i].lpData = reinterpret_cast<LPSTR>(audio_buffers_[i].data());
        wave_headers_[i].dwBufferLength = static_cast<DWORD>(
            audio_buffers_[i].size() * sizeof(std::int16_t));
        wave_headers_[i].dwFlags = 0;
        wave_headers_[i].dwLoops = 0;
        
        result = waveInPrepareHeader(wave_in_, &wave_headers_[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR) {
            std::cerr << "Failed to prepare wave header " << i << ": " << result << std::endl;
            return false;
        }
        
        result = waveInAddBuffer(wave_in_, &wave_headers_[i], sizeof(WAVEHDR));
        if (result != MMSYSERR_NOERROR) {
            std::cerr << "Failed to add wave buffer " << i << ": " << result << std::endl;
            return false;
        }
    }
    
    return true;
}

void AudioCapture::cleanupWindows() {
    if (wave_in_) {
        waveInStop(wave_in_);
        waveInReset(wave_in_);
        
        // Unprepare headers
        if (wave_headers_) {
            for (std::uint32_t i = 0; i < config_.num_buffers; ++i) {
                waveInUnprepareHeader(wave_in_, &wave_headers_[i], sizeof(WAVEHDR));
            }
            delete[] wave_headers_;
            wave_headers_ = nullptr;
        }
        
        waveInClose(wave_in_);
        wave_in_ = nullptr;
    }
}

void CALLBACK AudioCapture::waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                                      DWORD_PTR dwParam1, DWORD_PTR dwParam2) {
    AudioCapture* capture = reinterpret_cast<AudioCapture*>(dwInstance);
    
    switch (uMsg) {
        case WIM_DATA:
            if (capture && capture->capturing_.load()) {
                WAVEHDR* header = reinterpret_cast<WAVEHDR*>(dwParam1);
                capture->handleAudioInput(header);
            }
            break;
            
        case WIM_OPEN:
            std::cout << "Audio input device opened" << std::endl;
            break;
            
        case WIM_CLOSE:
            std::cout << "Audio input device closed" << std::endl;
            break;
    }
}

void AudioCapture::handleAudioInput(WAVEHDR* header) {
    if (!header || header->dwBytesRecorded == 0) {
        return;
    }
    
    // Process the audio data
    const std::int16_t* samples = reinterpret_cast<const std::int16_t*>(header->lpData);
    size_t num_samples = header->dwBytesRecorded / sizeof(std::int16_t);
    
    processAudioBuffer(samples, num_samples);
    
    // Re-add the buffer for continued recording
    if (capturing_.load()) {
        waveInAddBuffer(wave_in_, header, sizeof(WAVEHDR));
    }
}
#endif

void AudioCapture::processingThreadFunc() {
    std::cout << "Audio processing thread started" << std::endl;
    
    while (capturing_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Clean up old audio data
        std::lock_guard<std::mutex> lock(queue_mutex_);
        std::uint64_t current_time = getCurrentTimeMs();
        
        while (!audio_queue_.empty() && 
               (current_time - audio_queue_.front().timestamp_ms) > 1000) {  // Keep 1 second
            audio_queue_.pop_front();
        }
    }
    
    std::cout << "Audio processing thread stopped" << std::endl;
}

void AudioCapture::processAudioBuffer(const std::int16_t* buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    
    // Convert to vector
    std::vector<std::int16_t> int16_samples(buffer, buffer + size);
    
    // Convert to float and create audio data
    AudioData audio_data;
    audio_data.samples = convertToFloat(int16_samples);
    audio_data.envelope = calculateEnvelope(audio_data.samples);
    audio_data.speech_probability = detectSpeechPattern(audio_data.samples);
    audio_data.timestamp_ms = getCurrentTimeMs();
    audio_data.sample_rate = config_.sample_rate;
    
    // Add to queue and ring buffer
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    // Update ring buffer for continuous fetching
    for (float s : audio_data.samples) {
        sample_ring_.push_back(s);
    }
    // Limit ring buffer size (e.g. 2 seconds worth)
    size_t max_ring_size = static_cast<size_t>(config_.sample_rate) * 2;
    while (sample_ring_.size() > max_ring_size) {
        sample_ring_.pop_front();
    }
    
    audio_queue_.push_back(std::move(audio_data));
    
    // Limit queue size
    while (audio_queue_.size() > 100) {  // Keep last 100 buffers
        audio_queue_.pop_front();
    }
}

std::vector<float> AudioCapture::fetch(size_t n_samples) {
    std::vector<float> out(n_samples, 0.0f);
    std::lock_guard<std::mutex> lg(queue_mutex_);
    size_t available = sample_ring_.size();
    size_t take = (n_samples < available) ? n_samples : available;
    
    for (size_t i = 0; i < take; ++i) {
        out[i] = sample_ring_.front();
        sample_ring_.pop_front();
    }
    return out;
}

std::vector<float> AudioCapture::calculateEnvelope(const std::vector<float>& samples) {
    if (samples.empty()) {
        return std::vector<float>();
    }
    
    std::vector<float> envelope;
    size_t window_size = (static_cast<size_t>(1) > envelope_window_samples_) ? static_cast<size_t>(1) : envelope_window_samples_;
    
    for (size_t i = 0; i < samples.size(); i += window_size / 2) {  // 50% overlap
        size_t end_idx = (i + window_size < samples.size()) ? i + window_size : samples.size();
        
        // Calculate RMS for this window
        float rms = 0.0f;
        for (size_t j = i; j < end_idx; ++j) {
            rms += samples[j] * samples[j];
        }
        rms = std::sqrt(rms / (end_idx - i));
        
        envelope.push_back(rms);
    }
    
    return envelope;
}

std::vector<float> AudioCapture::convertToFloat(const std::vector<std::int16_t>& int16_samples) {
    std::vector<float> float_samples;
    float_samples.reserve(int16_samples.size());
    
    for (std::int16_t sample : int16_samples) {
        float_samples.push_back(static_cast<float>(sample) / 32768.0f);
    }
    
    return float_samples;
}

float AudioCapture::detectSpeechPattern(const std::vector<float>& samples) {
    if (samples.empty()) {
        return 0.0f;
    }
    
    // Calculate various speech indicators
    float rms = calculateRMS(samples);
    float zcr = calculateZeroCrossingRate(samples);
    
    // Simple speech detection heuristic
    float speech_prob = 0.0f;
    
    // RMS-based energy detection
    if (rms > 0.01f && rms < 0.8f) {  // Reasonable energy range for speech
        speech_prob += 0.4f;
    }
    
    // Zero crossing rate (speech typically has moderate ZCR)
    if (zcr > 0.02f && zcr < 0.3f) {
        speech_prob += 0.3f;
    }
    
    // Spectral characteristics (simplified)
    // In a full implementation, this would include FFT analysis
    if (rms > 0.05f) {
        speech_prob += 0.3f;
    }
    
    return (speech_prob < 0.0f) ? 0.0f : ((speech_prob > 1.0f) ? 1.0f : speech_prob);
}

float AudioCapture::calculateRMS(const std::vector<float>& samples) {
    if (samples.empty()) {
        return 0.0f;
    }
    
    float sum_squares = 0.0f;
    for (float sample : samples) {
        sum_squares += sample * sample;
    }
    
    return std::sqrt(sum_squares / samples.size());
}

float AudioCapture::calculateZeroCrossingRate(const std::vector<float>& samples) {
    if (samples.size() < 2) {
        return 0.0f;
    }
    
    int zero_crossings = 0;
    for (size_t i = 1; i < samples.size(); ++i) {
        if ((samples[i] >= 0.0f && samples[i-1] < 0.0f) ||
            (samples[i] < 0.0f && samples[i-1] >= 0.0f)) {
            zero_crossings++;
        }
    }
    
    return static_cast<float>(zero_crossings) / (samples.size() - 1);
}

std::uint64_t AudioCapture::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace Audio
} // namespace NeuroForge
