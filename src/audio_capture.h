#pragma once

#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace NeuroForge {
namespace Audio {

/**
 * @brief Real-time audio capture system for Windows
 * 
 * Provides microphone input capture with configurable sample rate,
 * buffer management, and audio envelope extraction for lip-sync detection.
 */
class AudioCapture {
public:
    /**
     * @brief Audio configuration parameters
     */
    struct Config {
        std::uint32_t sample_rate{44100};       ///< Sample rate in Hz
        std::uint16_t channels{1};              ///< Number of channels (1=mono, 2=stereo)
        std::uint16_t bits_per_sample{16};      ///< Bits per sample
        std::uint32_t buffer_size_ms{100};      ///< Buffer size in milliseconds
        std::uint32_t num_buffers{4};           ///< Number of audio buffers
        float envelope_window_ms{20.0f};        ///< Envelope calculation window in ms
    };
    
    /**
     * @brief Audio data structure
     */
    struct AudioData {
        std::vector<float> samples;             ///< Raw audio samples [-1,1]
        std::vector<float> envelope;            ///< Audio envelope (amplitude)
        float speech_probability{0.0f};         ///< Estimated speech probability
        std::uint64_t timestamp_ms{0};          ///< Capture timestamp
        std::uint32_t sample_rate{44100};       ///< Sample rate of this data
        
        AudioData() = default;
        AudioData(size_t size) : samples(size), envelope(size / 10) {}
    };

private:
#ifdef _WIN32
    HWAVEIN wave_in_;
    WAVEHDR* wave_headers_;
    WAVEFORMATEX wave_format_;
#endif
    
    Config config_;
    std::atomic<bool> capturing_;
    std::atomic<bool> initialized_;
    
    // Audio buffer management
    std::deque<AudioData> audio_queue_;
    std::deque<float> sample_ring_;
    std::mutex queue_mutex_;
    std::thread processing_thread_;
    
    // Buffer management
    std::vector<std::vector<std::int16_t>> audio_buffers_;
    std::atomic<int> current_buffer_;
    
    // Audio processing
    std::vector<float> envelope_buffer_;
    size_t envelope_window_samples_;
    
public:
    /**
     * @brief Constructor with configuration
     */
    explicit AudioCapture(const Config& config);
    
    /**
     * @brief Destructor
     */
    ~AudioCapture();
    
    // Non-copyable but movable
    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;
    AudioCapture(AudioCapture&&) = default;
    AudioCapture& operator=(AudioCapture&&) = default;
    
    /**
     * @brief Initialize audio capture system
     * @return True if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Start audio capture
     * @return True if capture started successfully
     */
    bool startCapture();
    
    /**
     * @brief Stop audio capture
     */
    void stopCapture();
    
    /**
     * @brief Check if currently capturing
     * @return True if capturing audio
     */
    bool isCapturing() const { return capturing_.load(); }
    
    /**
     * @brief Check if system is initialized
     * @return True if initialized
     */
    bool isInitialized() const { return initialized_.load(); }
    
    /**
     * @brief Get latest audio data
     * @param max_age_ms Maximum age of audio data in milliseconds
     * @return Latest audio data or empty if none available
     */
    AudioData getLatestAudio(std::uint32_t max_age_ms = 200);

    /**
     * @brief Fetch N samples from the capture buffer (FIFO)
     * @param n_samples Number of samples to fetch
     * @return Vector containing N samples (padded with zeros if not enough data)
     */
    std::vector<float> fetch(size_t n_samples);
    
    /**
     * @brief Get audio envelope for lip-sync detection
     * @param window_ms Time window for envelope calculation
     * @return Audio envelope vector
     */
    std::vector<float> getAudioEnvelope(float window_ms = 50.0f);
    
    /**
     * @brief Estimate speech probability from audio
     * @param audio_data Audio data to analyze
     * @return Speech probability [0,1]
     */
    float estimateSpeechProbability(const AudioData& audio_data);
    
    /**
     * @brief Get current configuration
     * @return Current audio configuration
     */
    Config getConfig() const { return config_; }
    
    /**
     * @brief Get number of available audio samples in queue
     * @return Number of audio data samples available
     */
    size_t getQueueSize() const;
    
    /**
     * @brief Clear audio queue
     */
    void clearQueue();

private:
    /**
     * @brief Initialize Windows audio system
     * @return True if successful
     */
    bool initializeWindows();
    
    /**
     * @brief Cleanup Windows audio system
     */
    void cleanupWindows();
    
    /**
     * @brief Audio processing thread function
     */
    void processingThreadFunc();
    
    /**
     * @brief Process raw audio buffer
     * @param buffer Raw audio buffer
     * @param size Buffer size in bytes
     */
    void processAudioBuffer(const std::int16_t* buffer, size_t size);
    
    /**
     * @brief Calculate audio envelope
     * @param samples Audio samples
     * @return Envelope values
     */
    std::vector<float> calculateEnvelope(const std::vector<float>& samples);
    
    /**
     * @brief Convert int16 samples to float [-1,1]
     * @param int16_samples Input samples
     * @return Normalized float samples
     */
    std::vector<float> convertToFloat(const std::vector<std::int16_t>& int16_samples);
    
    /**
     * @brief Detect speech-like patterns in audio
     * @param samples Audio samples to analyze
     * @return Speech probability [0,1]
     */
    float detectSpeechPattern(const std::vector<float>& samples);
    
    /**
     * @brief Calculate RMS (Root Mean Square) of audio samples
     * @param samples Audio samples
     * @return RMS value
     */
    float calculateRMS(const std::vector<float>& samples);
    
    /**
     * @brief Calculate zero crossing rate
     * @param samples Audio samples
     * @return Zero crossing rate
     */
    float calculateZeroCrossingRate(const std::vector<float>& samples);
    
    /**
     * @brief Get current timestamp in milliseconds
     * @return Current timestamp
     */
    std::uint64_t getCurrentTimeMs() const;

#ifdef _WIN32
    /**
     * @brief Windows audio callback function
     */
    static void CALLBACK waveInProc(HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance,
                                   DWORD_PTR dwParam1, DWORD_PTR dwParam2);
    
    /**
     * @brief Handle Windows audio input message
     */
    void handleAudioInput(WAVEHDR* header);
#endif
};

} // namespace Audio
} // namespace NeuroForge