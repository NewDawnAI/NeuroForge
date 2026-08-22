#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
// Windows: mmsystem.h and windows.h headers are included only in implementation
// to avoid global namespace pollution and macro conflicts (like NEAR/FAR).
#ifndef NOMINMAX
#define NOMINMAX
#endif

#elif defined(__linux__)
// Linux: use ALSA for audio capture
// Link with -lasound (libasound2-dev package required)

#elif defined(__APPLE__)
// macOS: use CoreAudio for audio capture
// Link with AudioToolbox.framework
#include <TargetConditionals.h>
#endif

namespace NeuroForge {
namespace Audio {

/**
 * @brief Cross-platform real-time audio capture system
 *
 * Provides microphone input capture with configurable sample rate,
 * buffer management, and audio envelope extraction for lip-sync detection.
 *
 * Supported platforms:
 * - Windows: WaveIn API (mmsystem.h)
 * - Linux: ALSA (libasound2)
 * - macOS: CoreAudio (AudioToolbox)
 */
class AudioCapture {
public:
  /**
   * @brief Audio configuration parameters
   */
  struct Config {
    std::uint32_t sample_rate{44100}; ///< Sample rate in Hz
    std::uint16_t channels{1};        ///< Number of channels (1=mono, 2=stereo)
    std::uint16_t bits_per_sample{16}; ///< Bits per sample
    std::uint32_t buffer_size_ms{100}; ///< Buffer size in milliseconds
    std::uint32_t num_buffers{4};      ///< Number of audio buffers
    float envelope_window_ms{20.0f};   ///< Envelope calculation window in ms
  };

  /**
   * @brief Audio data structure
   */
  struct AudioData {
    std::vector<float> samples;       ///< Raw audio samples [-1,1]
    std::vector<float> envelope;      ///< Audio envelope (amplitude)
    float speech_probability{0.0f};   ///< Estimated speech probability
    std::uint64_t timestamp_ms{0};    ///< Capture timestamp
    std::uint32_t sample_rate{44100}; ///< Sample rate of this data

    AudioData() = default;
    AudioData(size_t size) : samples(size), envelope(size / 10) {}
  };

private:
  // Platform-specific handles
#if defined(_WIN32)
  // Windows (MSVC & MINGW): Use void* for handles to avoid including windows.h
  // in header
  void *wave_in_ = nullptr;      // HWAVEIN
  void *wave_headers_ = nullptr; // WAVEHDR*

  // Opaque structure matching WAVEFORMATEX (18 bytes)
  struct {
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
    uint16_t cbSize;
  } wave_format_;
#elif defined(__linux__)
  // Linux ALSA handle (snd_pcm_t*)
  void *alsa_handle_ = nullptr;
  void *alsa_hw_params_ = nullptr;
#elif defined(__APPLE__)
  // macOS CoreAudio handles
  void *audio_queue_ = nullptr;  // AudioQueueRef
  void *audio_format_ = nullptr; // AudioStreamBasicDescription*
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
  explicit AudioCapture(const Config &config);

  /**
   * @brief Destructor
   */
  ~AudioCapture();

  // Non-copyable but movable
  AudioCapture(const AudioCapture &) = delete;
  AudioCapture &operator=(const AudioCapture &) = delete;
  AudioCapture(AudioCapture &&) = default;
  AudioCapture &operator=(AudioCapture &&) = default;

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
  float estimateSpeechProbability(const AudioData &audio_data);

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
   * @brief Initialize platform-specific audio system
   * @return True if successful
   */
#ifdef _WIN32
  bool initializeWindows();
  void cleanupWindows();
#elif defined(__linux__)
  bool initializeLinux();
  void cleanupLinux();
#elif defined(__APPLE__)
  bool initializeMacOS();
  void cleanupMacOS();
#endif

  /**
   * @brief Audio processing thread function
   */
  void processingThreadFunc();

  /**
   * @brief Process raw audio buffer
   * @param buffer Raw audio buffer
   * @param size Buffer size in bytes
   */
  void processAudioBuffer(const std::int16_t *buffer, size_t size);

  /**
   * @brief Calculate audio envelope
   * @param samples Audio samples
   * @return Envelope values
   */
  std::vector<float> calculateEnvelope(const std::vector<float> &samples);

  /**
   * @brief Convert int16 samples to float [-1,1]
   * @param int16_samples Input samples
   * @return Normalized float samples
   */
  std::vector<float>
  convertToFloat(const std::vector<std::int16_t> &int16_samples);

  /**
   * @brief Detect speech-like patterns in audio
   * @param samples Audio samples to analyze
   * @return Speech probability [0,1]
   */
  float detectSpeechPattern(const std::vector<float> &samples);

  /**
   * @brief Calculate RMS (Root Mean Square) of audio samples
   * @param samples Audio samples
   * @return RMS value
   */
  float calculateRMS(const std::vector<float> &samples);

  /**
   * @brief Calculate zero crossing rate
   * @param samples Audio samples
   * @return Zero crossing rate
   */
  float calculateZeroCrossingRate(const std::vector<float> &samples);

  /**
   * @brief Get current timestamp in milliseconds
   * @return Current timestamp
   */
  std::uint64_t getCurrentTimeMs() const;

#if defined(_WIN32)
  /**
   * @brief Windows audio callback function
   */
  static void __stdcall waveInProc(void *hwi, unsigned int uMsg,
                                   uintptr_t dwInstance, uintptr_t dwParam1,
                                   uintptr_t dwParam2);

  /**
   * @brief Handle Windows audio input message
   */
  void handleAudioInput(void *header);
#endif
};

} // namespace Audio
} // namespace NeuroForge