#pragma once

#include <vector>
#include <array>
#include <memory>
#include <mutex>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <string>

namespace NeuroForge {
namespace Biases {

/**
 * @brief Voice Bias System for Human Voice Prioritization
 * 
 * Implements biologically-inspired voice detection and prioritization mechanisms
 * that enhance processing of human vocal frequencies and phoneme patterns.
 * Based on infant voice preference and speech perception research.
 */
class VoiceBias {
public:
    /**
     * @brief Configuration parameters for voice bias
     */
    struct Config {
        // Frequency analysis parameters
        float fundamental_freq_min{85.0f};          ///< Minimum human fundamental frequency (Hz)
        float fundamental_freq_max{255.0f};         ///< Maximum human fundamental frequency (Hz)
        float formant_freq_min{200.0f};             ///< Minimum formant frequency (Hz)
        float formant_freq_max{3500.0f};            ///< Maximum formant frequency (Hz)
        
        // Voice prioritization parameters
        float voice_priority_multiplier{2.5f};      ///< Voice attention boost factor
        float phoneme_recognition_threshold{0.4f};   ///< Phoneme detection threshold
        float voice_continuity_bonus{1.3f};         ///< Bonus for continuous voice
        float harmonic_enhancement{1.8f};           ///< Harmonic structure enhancement
        
        // Temporal parameters
        std::uint32_t analysis_window_ms{25};       ///< Analysis window duration
        std::uint32_t hop_length_ms{10};            ///< Hop length for sliding window
        std::uint32_t voice_memory_ms{500};         ///< Voice continuity memory
        
        // Feature extraction
        std::uint32_t num_mel_filters{26};          ///< Number of mel-scale filters
        std::uint32_t num_mfcc_coeffs{13};          ///< Number of MFCC coefficients
        bool enable_phoneme_templates{true};        ///< Enable phoneme pattern matching
        bool enable_prosody_analysis{true};         ///< Enable prosodic feature analysis
        
        // Bias strength modulation
        float infant_directed_speech_boost{3.0f};   ///< IDS (motherese) enhancement
        float emotional_speech_boost{2.2f};         ///< Emotional speech enhancement
        float background_noise_suppression{0.6f};   ///< Non-voice suppression factor

        // System parameters
        float sample_rate{44100.0f};                ///< Audio sample rate in Hz
    };
    
    /**
     * @brief Voice characteristics detected in audio
     */
    struct VoiceFeatures {
        float fundamental_frequency{0.0f};          ///< F0 frequency (Hz)
        float voice_probability{0.0f};              ///< Probability of voice presence
        float harmonic_ratio{0.0f};                 ///< Harmonic-to-noise ratio
        float spectral_centroid{0.0f};              ///< Spectral centroid (Hz)
        float spectral_rolloff{0.0f};               ///< Spectral rolloff frequency
        float zero_crossing_rate{0.0f};             ///< Zero crossing rate
        std::vector<float> formant_frequencies;     ///< Formant frequencies
        std::vector<float> mfcc_coefficients;       ///< MFCC features
        float prosody_score{0.0f};                  ///< Prosodic feature score
        bool is_infant_directed{false};             ///< Infant-directed speech flag
        std::string detected_phoneme;               ///< Most likely phoneme
        float phoneme_confidence{0.0f};             ///< Phoneme detection confidence
    };
    
    /**
     * @brief Phoneme template for pattern matching
     */
    struct PhonemeTemplate {
        std::string phoneme_symbol;                 ///< IPA symbol or identifier
        std::vector<float> formant_pattern;         ///< Expected formant frequencies
        std::vector<float> mfcc_pattern;            ///< Expected MFCC pattern
        float duration_ms{50.0f};                   ///< Typical duration
        float frequency_tolerance{0.15f};           ///< Matching tolerance
    };
    
    /**
     * @brief Voice continuity tracking
     */
    struct VoiceContinuity {
        bool is_active{false};                      ///< Voice currently active
        std::uint64_t start_time_ms{0};            ///< Voice onset time
        std::uint64_t last_update_ms{0};           ///< Last voice detection time
        float accumulated_confidence{0.0f};         ///< Accumulated voice confidence
        float speaker_consistency{0.0f};           ///< Speaker identity consistency
        std::vector<float> speaker_profile;         ///< Speaker voice characteristics
    };

private:
    Config config_;
    mutable std::mutex voice_mutex_;
    
    // Voice analysis state
    VoiceContinuity voice_continuity_;
    std::vector<VoiceFeatures> recent_features_;
    std::unordered_map<std::string, PhonemeTemplate> phoneme_templates_;
    
    // Frequency analysis buffers
    std::vector<float> analysis_buffer_;
    std::vector<float> window_function_;
    std::vector<float> mel_filter_bank_;
    std::vector<float> fft_magnitude_;
    
    // Statistics and performance tracking
    std::uint64_t total_voice_detections_{0};
    std::uint64_t total_phoneme_matches_{0};
    std::uint64_t total_processing_calls_{0};
    float average_voice_confidence_{0.0f};

public:
    /**
     * @brief Constructor with configuration
     */
    explicit VoiceBias(const Config& config);
    
    /**
     * @brief Destructor
     */
    ~VoiceBias() = default;
    
    // Disable copy constructor and assignment
    VoiceBias(const VoiceBias&) = delete;
    VoiceBias& operator=(const VoiceBias&) = delete;
    
    /**
     * @brief Apply voice bias to audio features
     * @param features Audio feature vector to modify
     * @param audio_data Raw audio samples
     * @param sample_rate Audio sample rate (Hz)
     * @param grid_size Size of the neural grid
     * @return True if voice bias was applied
     */
    bool applyVoiceBias(std::vector<float>& features,
                       const std::vector<float>& audio_data,
                       float sample_rate,
                       int grid_size);
    
    /**
     * @brief Analyze audio for voice characteristics
     * @param audio_data Raw audio samples
     * @param sample_rate Audio sample rate (Hz)
     * @return Detected voice features
     */
    VoiceFeatures analyzeVoiceFeatures(const std::vector<float>& audio_data,
                                      float sample_rate);
    
    /**
     * @brief Detect phonemes in audio features
     * @param voice_features Analyzed voice features
     * @return Best matching phoneme and confidence
     */
    std::pair<std::string, float> detectPhoneme(const VoiceFeatures& voice_features);
    
    /**
     * @brief Update voice continuity tracking
     * @param voice_features Current voice features
     * @param timestamp_ms Current timestamp
     */
    void updateVoiceContinuity(const VoiceFeatures& voice_features,
                              std::uint64_t timestamp_ms);
    
    /**
     * @brief Apply voice-specific attention boost
     * @param features Feature vector to enhance
     * @param voice_features Detected voice characteristics
     * @param grid_size Neural grid size
     */
    void applyVoiceAttentionBoost(std::vector<float>& features,
                                 const VoiceFeatures& voice_features,
                                 int grid_size);
    
    /**
     * @brief Suppress non-voice background noise
     * @param features Feature vector to modify
     * @param voice_probability Probability of voice presence
     */
    void applyBackgroundSuppression(std::vector<float>& features,
                                   float voice_probability);
    
    /**
     * @brief Get current voice continuity state
     * @return Voice continuity information
     */
    VoiceContinuity getVoiceContinuity() const;
    
    /**
     * @brief Get recent voice features history
     * @param max_history Maximum number of recent features
     * @return Vector of recent voice features
     */
    std::vector<VoiceFeatures> getRecentVoiceFeatures(size_t max_history = 10) const;
    
    /**
     * @brief Update configuration
     * @param new_config New configuration parameters
     */
    void updateConfig(const Config& new_config);
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    Config getConfig() const;
    
    /**
     * @brief Get voice bias statistics
     * @return Statistics about voice processing
     */
    struct Statistics {
        std::uint64_t total_voice_detections;
        std::uint64_t total_phoneme_matches;
        std::uint64_t total_processing_calls;
        float average_voice_confidence;
        float voice_detection_rate;
        float phoneme_accuracy;
        std::uint64_t active_voice_duration_ms;
        size_t phoneme_templates_loaded;
    };
    
    Statistics getStatistics() const;
    
    /**
     * @brief Reset voice bias state
     */
    void reset();

private:
    /**
     * @brief Initialize phoneme templates
     */
    void initializePhonemeTemplates();
    
    /**
     * @brief Initialize mel-scale filter bank
     * @param sample_rate Audio sample rate
     * @param fft_size FFT size
     */
    void initializeMelFilterBank(float sample_rate, size_t fft_size);
    
    /**
     * @brief Initialize window function for FFT
     * @param window_size Size of the analysis window
     */
    void initializeWindowFunction(size_t window_size);
    
    /**
     * @brief Extract fundamental frequency (F0)
     * @param audio_data Audio samples
     * @param sample_rate Sample rate
     * @return Fundamental frequency in Hz
     */
    float extractFundamentalFrequency(const std::vector<float>& audio_data,
                                     float sample_rate);
    
    /**
     * @brief Extract formant frequencies
     * @param audio_data Audio samples
     * @param sample_rate Sample rate
     * @return Vector of formant frequencies
     */
    std::vector<float> extractFormantFrequencies(const std::vector<float>& audio_data,
                                                float sample_rate);
    
    /**
     * @brief Extract MFCC coefficients
     * @param audio_data Audio samples
     * @param sample_rate Sample rate
     * @return MFCC coefficient vector
     */
    std::vector<float> extractMFCCCoefficients(const std::vector<float>& audio_data,
                                              float sample_rate);
    
    /**
     * @brief Calculate harmonic-to-noise ratio
     * @param audio_data Audio samples
     * @param fundamental_freq F0 frequency
     * @param sample_rate Sample rate
     * @return Harmonic-to-noise ratio
     */
    float calculateHarmonicRatio(const std::vector<float>& audio_data,
                                float fundamental_freq,
                                float sample_rate);
    
    /**
     * @brief Detect infant-directed speech characteristics
     * @param voice_features Voice feature analysis
     * @return True if IDS characteristics detected
     */
    bool detectInfantDirectedSpeech(const VoiceFeatures& voice_features);
    
    /**
     * @brief Calculate voice probability from features
     * @param voice_features Analyzed voice features
     * @return Probability of voice presence (0-1)
     */
    float calculateVoiceProbability(const VoiceFeatures& voice_features);
    
    /**
     * @brief Apply Hamming window to audio data
     * @param data Audio data to window
     */
    void applyHammingWindow(std::vector<float>& data);
    
    /**
     * @brief Perform FFT magnitude calculation
     * @param audio_data Windowed audio data
     * @return Magnitude spectrum
     */
    std::vector<float> calculateFFTMagnitude(const std::vector<float>& audio_data);
    
    /**
     * @brief Get current timestamp in milliseconds
     * @return Current timestamp
     */
    std::uint64_t getCurrentTimestamp() const;
    
    /**
     * @brief Validate voice features for consistency
     * @param features Voice features to validate
     * @return True if features are valid
     */
    bool validateVoiceFeatures(const VoiceFeatures& features) const;
};

} // namespace Biases
} // namespace NeuroForge