#include "VoiceBias.h"
#include <chrono>
#include <numeric>
#include <cstring>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NeuroForge {
namespace Biases {

VoiceBias::VoiceBias(const Config& config) : config_(config) {
    // Initialize analysis buffers using configured sample rate
    float initial_sample_rate = config_.sample_rate > 0.0f ? config_.sample_rate : 44100.0f;
    size_t window_size = static_cast<size_t>(static_cast<float>(config_.analysis_window_ms) / 1000.0f * initial_sample_rate);
    
    // Ensure window size is power of 2 for efficient FFT if needed, or at least reasonable
    if (window_size < 128) window_size = 128;
    
    analysis_buffer_.resize(window_size);
    window_function_.resize(window_size);
    fft_magnitude_.resize(window_size / 2 + 1);
    
    // Initialize processing components
    initializeWindowFunction(window_size);
    initializeMelFilterBank(initial_sample_rate, window_size);
    initializePhonemeTemplates();
    
    // Initialize voice continuity
    voice_continuity_ = VoiceContinuity{};
    recent_features_.reserve(20); // Keep last 20 feature frames
}

bool VoiceBias::applyVoiceBias(std::vector<float>& features,
                              const std::vector<float>& audio_data,
                              float sample_rate,
                              int grid_size) {
    if (audio_data.empty() || features.empty()) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(voice_mutex_);
    ++total_processing_calls_;
    
    // Analyze voice characteristics
    VoiceFeatures voice_features = analyzeVoiceFeatures(audio_data, sample_rate);
    
    // Update voice continuity tracking
    std::uint64_t current_time = getCurrentTimestamp();
    updateVoiceContinuity(voice_features, current_time);
    
    // Apply voice-specific enhancements if voice is detected
    if (voice_features.voice_probability > config_.phoneme_recognition_threshold) {
        ++total_voice_detections_;
        
        // Apply attention boost for voice regions
        applyVoiceAttentionBoost(features, voice_features, grid_size);
        
        // Detect and enhance phoneme patterns
        auto [phoneme, confidence] = detectPhoneme(voice_features);
        if (confidence > config_.phoneme_recognition_threshold) {
            ++total_phoneme_matches_;
            voice_features.detected_phoneme = phoneme;
            voice_features.phoneme_confidence = confidence;
        }
        
        // Store recent features for continuity analysis
        recent_features_.push_back(voice_features);
        if (recent_features_.size() > 20) {
            recent_features_.erase(recent_features_.begin());
        }
        
        // Update running statistics
        average_voice_confidence_ = (average_voice_confidence_ * 0.95f) + 
                                   (voice_features.voice_probability * 0.05f);
        
        return true;
    } else {
        // Apply background suppression for non-voice audio
        applyBackgroundSuppression(features, voice_features.voice_probability);
        return false;
    }
}

VoiceBias::VoiceFeatures VoiceBias::analyzeVoiceFeatures(const std::vector<float>& audio_data,
                                                        float sample_rate) {
    VoiceFeatures features;
    
    if (audio_data.empty()) {
        return features;
    }
    
    // Extract fundamental frequency (F0)
    features.fundamental_frequency = extractFundamentalFrequency(audio_data, sample_rate);
    
    // Extract formant frequencies
    features.formant_frequencies = extractFormantFrequencies(audio_data, sample_rate);
    
    // Extract MFCC coefficients
    features.mfcc_coefficients = extractMFCCCoefficients(audio_data, sample_rate);
    
    // Calculate harmonic-to-noise ratio
    features.harmonic_ratio = calculateHarmonicRatio(audio_data, 
                                                    features.fundamental_frequency, 
                                                    sample_rate);
    
    // Calculate spectral features
    std::vector<float> magnitude_spectrum = calculateFFTMagnitude(audio_data);
    
    // Spectral centroid
    float weighted_sum = 0.0f;
    float magnitude_sum = 0.0f;
    for (size_t i = 0; i < magnitude_spectrum.size(); ++i) {
        float freq = (i * sample_rate) / (2.0f * magnitude_spectrum.size());
        weighted_sum += freq * magnitude_spectrum[i];
        magnitude_sum += magnitude_spectrum[i];
    }
    features.spectral_centroid = magnitude_sum > 0 ? weighted_sum / magnitude_sum : 0.0f;
    
    // Spectral rolloff (95% of energy)
    float cumulative_energy = 0.0f;
    float total_energy = std::accumulate(magnitude_spectrum.begin(), magnitude_spectrum.end(), 0.0f);
    float rolloff_threshold = total_energy * 0.95f;
    
    for (size_t i = 0; i < magnitude_spectrum.size(); ++i) {
        cumulative_energy += magnitude_spectrum[i];
        if (cumulative_energy >= rolloff_threshold) {
            features.spectral_rolloff = (i * sample_rate) / (2.0f * magnitude_spectrum.size());
            break;
        }
    }
    
    // Zero crossing rate
    int zero_crossings = 0;
    for (size_t i = 1; i < audio_data.size(); ++i) {
        if ((audio_data[i] >= 0) != (audio_data[i-1] >= 0)) {
            ++zero_crossings;
        }
    }
    features.zero_crossing_rate = static_cast<float>(zero_crossings) / audio_data.size();
    
    // Calculate voice probability
    features.voice_probability = calculateVoiceProbability(features);
    
    // Detect infant-directed speech
    features.is_infant_directed = detectInfantDirectedSpeech(features);
    
    // Calculate prosody score
    if (config_.enable_prosody_analysis) {
        // Simple prosody based on F0 variation and spectral dynamics
        float f0_variation = features.fundamental_frequency > 0 ? 
            std::abs(features.fundamental_frequency - 150.0f) / 150.0f : 0.0f;
        float spectral_variation = features.spectral_centroid > 0 ? 
            std::abs(features.spectral_centroid - 1000.0f) / 1000.0f : 0.0f;
        features.prosody_score = std::min(1.0f, (f0_variation + spectral_variation) / 2.0f);
    }
    
    return features;
}

std::pair<std::string, float> VoiceBias::detectPhoneme(const VoiceFeatures& voice_features) {
    if (!config_.enable_phoneme_templates || phoneme_templates_.empty()) {
        return {"", 0.0f};
    }
    
    std::string best_phoneme;
    float best_confidence = 0.0f;
    
    for (const auto& [phoneme_name, template_data] : phoneme_templates_) {
        float confidence = 0.0f;
        int matches = 0;
        
        // Compare formant frequencies
        if (!voice_features.formant_frequencies.empty() && 
            !template_data.formant_pattern.empty()) {
            
            size_t min_formants = std::min(voice_features.formant_frequencies.size(),
                                         template_data.formant_pattern.size());
            
            for (size_t i = 0; i < min_formants; ++i) {
                float expected = template_data.formant_pattern[i];
                float actual = voice_features.formant_frequencies[i];
                float tolerance = expected * template_data.frequency_tolerance;
                
                if (std::abs(actual - expected) <= tolerance) {
                    confidence += 1.0f / min_formants;
                    ++matches;
                }
            }
        }
        
        // Compare MFCC coefficients
        if (!voice_features.mfcc_coefficients.empty() && 
            !template_data.mfcc_pattern.empty()) {
            
            size_t min_mfcc = std::min(voice_features.mfcc_coefficients.size(),
                                     template_data.mfcc_pattern.size());
            
            float mfcc_similarity = 0.0f;
            for (size_t i = 0; i < min_mfcc; ++i) {
                float diff = std::abs(voice_features.mfcc_coefficients[i] - 
                                    template_data.mfcc_pattern[i]);
                mfcc_similarity += std::exp(-diff * diff); // Gaussian similarity
            }
            mfcc_similarity /= min_mfcc;
            confidence = (confidence + mfcc_similarity) / 2.0f;
        }
        
        // Update best match
        if (confidence > best_confidence) {
            best_confidence = confidence;
            best_phoneme = phoneme_name;
        }
    }
    
    return {best_phoneme, best_confidence};
}

void VoiceBias::updateVoiceContinuity(const VoiceFeatures& voice_features,
                                     std::uint64_t timestamp_ms) {
    bool voice_detected = voice_features.voice_probability > config_.phoneme_recognition_threshold;
    
    if (voice_detected) {
        if (!voice_continuity_.is_active) {
            // Voice onset
            voice_continuity_.is_active = true;
            voice_continuity_.start_time_ms = timestamp_ms;
            voice_continuity_.accumulated_confidence = voice_features.voice_probability;
            
            // Initialize speaker profile
            voice_continuity_.speaker_profile.clear();
            voice_continuity_.speaker_profile.push_back(voice_features.fundamental_frequency);
            voice_continuity_.speaker_profile.push_back(voice_features.spectral_centroid);
            if (!voice_features.formant_frequencies.empty()) {
                voice_continuity_.speaker_profile.insert(
                    voice_continuity_.speaker_profile.end(),
                    voice_features.formant_frequencies.begin(),
                    voice_features.formant_frequencies.end());
            }
        } else {
            // Voice continuation
            voice_continuity_.accumulated_confidence = 
                (voice_continuity_.accumulated_confidence * 0.9f) + 
                (voice_features.voice_probability * 0.1f);
            
            // Update speaker consistency
            if (!voice_continuity_.speaker_profile.empty()) {
                float consistency = 0.0f;
                int comparisons = 0;
                
                // Compare F0
                if (voice_continuity_.speaker_profile.size() > 0) {
                    float f0_diff = std::abs(voice_features.fundamental_frequency - 
                                           voice_continuity_.speaker_profile[0]);
                    consistency += std::exp(-f0_diff / 50.0f); // 50Hz tolerance
                    ++comparisons;
                }
                
                // Compare spectral centroid
                if (voice_continuity_.speaker_profile.size() > 1) {
                    float sc_diff = std::abs(voice_features.spectral_centroid - 
                                           voice_continuity_.speaker_profile[1]);
                    consistency += std::exp(-sc_diff / 500.0f); // 500Hz tolerance
                    ++comparisons;
                }
                
                voice_continuity_.speaker_consistency = comparisons > 0 ? 
                    consistency / comparisons : 0.0f;
            }
        }
        
        voice_continuity_.last_update_ms = timestamp_ms;
    } else {
        // Check if voice should be considered ended
        if (voice_continuity_.is_active && 
            (timestamp_ms - voice_continuity_.last_update_ms) > config_.voice_memory_ms) {
            voice_continuity_.is_active = false;
            voice_continuity_.accumulated_confidence = 0.0f;
            voice_continuity_.speaker_consistency = 0.0f;
        }
    }
}

void VoiceBias::applyVoiceAttentionBoost(std::vector<float>& features,
                                        const VoiceFeatures& voice_features,
                                        int grid_size) {
    if (features.empty() || grid_size <= 0) {
        return;
    }
    
    float boost_factor = config_.voice_priority_multiplier;
    
    // Additional boost for infant-directed speech
    if (voice_features.is_infant_directed) {
        boost_factor *= config_.infant_directed_speech_boost;
    }
    
    // Additional boost for emotional speech (high prosody)
    if (voice_features.prosody_score > 0.7f) {
        boost_factor *= config_.emotional_speech_boost;
    }
    
    // Additional boost for harmonic content
    if (voice_features.harmonic_ratio > 0.5f) {
        boost_factor *= config_.harmonic_enhancement;
    }
    
    // Additional boost for voice continuity
    if (voice_continuity_.is_active && voice_continuity_.speaker_consistency > 0.7f) {
        boost_factor *= config_.voice_continuity_bonus;
    }
    
    // Apply boost to all features (representing distributed voice processing)
    for (float& feature : features) {
        feature *= boost_factor;
    }
}

void VoiceBias::applyBackgroundSuppression(std::vector<float>& features,
                                          float voice_probability) {
    if (features.empty()) {
        return;
    }
    
    // Suppress non-voice content
    float suppression_factor = config_.background_noise_suppression;
    
    // Stronger suppression for clearly non-voice content
    if (voice_probability < 0.1f) {
        suppression_factor *= 0.5f;
    }
    
    for (float& feature : features) {
        feature *= suppression_factor;
    }
}

VoiceBias::VoiceContinuity VoiceBias::getVoiceContinuity() const {
    std::lock_guard<std::mutex> lock(voice_mutex_);
    return voice_continuity_;
}

std::vector<VoiceBias::VoiceFeatures> VoiceBias::getRecentVoiceFeatures(size_t max_history) const {
    std::lock_guard<std::mutex> lock(voice_mutex_);
    
    if (recent_features_.size() <= max_history) {
        return recent_features_;
    }
    
    return std::vector<VoiceFeatures>(
        recent_features_.end() - max_history,
        recent_features_.end()
    );
}

void VoiceBias::updateConfig(const Config& new_config) {
    std::lock_guard<std::mutex> lock(voice_mutex_);
    config_ = new_config;
    
    // Reinitialize components if necessary
    size_t window_size = static_cast<size_t>(config_.analysis_window_ms * 44.1f);
    if (analysis_buffer_.size() != window_size) {
        analysis_buffer_.resize(window_size);
        window_function_.resize(window_size);
        fft_magnitude_.resize(window_size / 2 + 1);
        initializeWindowFunction(window_size);
    }
}

VoiceBias::Config VoiceBias::getConfig() const {
    std::lock_guard<std::mutex> lock(voice_mutex_);
    return config_;
}

VoiceBias::Statistics VoiceBias::getStatistics() const {
    std::lock_guard<std::mutex> lock(voice_mutex_);
    
    Statistics stats;
    stats.total_voice_detections = total_voice_detections_;
    stats.total_phoneme_matches = total_phoneme_matches_;
    stats.total_processing_calls = total_processing_calls_;
    stats.average_voice_confidence = average_voice_confidence_;
    stats.voice_detection_rate = total_processing_calls_ > 0 ? 
        static_cast<float>(total_voice_detections_) / total_processing_calls_ : 0.0f;
    stats.phoneme_accuracy = total_voice_detections_ > 0 ? 
        static_cast<float>(total_phoneme_matches_) / total_voice_detections_ : 0.0f;
    stats.active_voice_duration_ms = voice_continuity_.is_active ? 
        getCurrentTimestamp() - voice_continuity_.start_time_ms : 0;
    stats.phoneme_templates_loaded = phoneme_templates_.size();
    
    return stats;
}

void VoiceBias::reset() {
    std::lock_guard<std::mutex> lock(voice_mutex_);
    
    voice_continuity_ = VoiceContinuity{};
    recent_features_.clear();
    total_voice_detections_ = 0;
    total_phoneme_matches_ = 0;
    total_processing_calls_ = 0;
    average_voice_confidence_ = 0.0f;
}

// Private implementation methods

void VoiceBias::initializePhonemeTemplates() {
    // Initialize basic English phoneme templates
    // These are simplified templates - in practice, these would be learned or loaded from data
    
    // Vowels
    phoneme_templates_["a"] = {"a", {730, 1090, 2440}, {-1.2f, 0.8f, -0.3f}, 80.0f, 0.15f};
    phoneme_templates_["e"] = {"e", {530, 1840, 2480}, {-0.8f, 1.2f, 0.1f}, 70.0f, 0.15f};
    phoneme_templates_["i"] = {"i", {270, 2290, 3010}, {0.2f, 1.8f, 0.5f}, 60.0f, 0.15f};
    phoneme_templates_["o"] = {"o", {570, 840, 2410}, {-1.0f, 0.3f, -0.2f}, 90.0f, 0.15f};
    phoneme_templates_["u"] = {"u", {300, 870, 2240}, {-0.5f, -0.2f, -0.8f}, 85.0f, 0.15f};
    
    // Consonants (simplified)
    phoneme_templates_["p"] = {"p", {}, {-2.0f, -1.5f, 0.8f}, 20.0f, 0.20f};
    phoneme_templates_["t"] = {"t", {}, {-1.8f, -1.0f, 1.2f}, 15.0f, 0.20f};
    phoneme_templates_["k"] = {"k", {}, {-2.2f, -0.8f, 0.5f}, 25.0f, 0.20f};
    phoneme_templates_["s"] = {"s", {}, {0.5f, 2.0f, 1.8f}, 100.0f, 0.18f};
    phoneme_templates_["f"] = {"f", {}, {0.2f, 1.5f, 1.5f}, 80.0f, 0.18f};
}

void VoiceBias::initializeMelFilterBank(float sample_rate, size_t fft_size) {
    // Initialize mel-scale filter bank for MFCC extraction
    mel_filter_bank_.resize(config_.num_mel_filters * (fft_size / 2 + 1));
    
    // Mel scale conversion functions
    auto hz_to_mel = [](float hz) { return 2595.0f * std::log10(1.0f + hz / 700.0f); };
    auto mel_to_hz = [](float mel) { return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f); };
    
    float mel_min = hz_to_mel(0.0f);
    float mel_max = hz_to_mel(sample_rate / 2.0f);
    float mel_step = (mel_max - mel_min) / (config_.num_mel_filters + 1);
    
    // Create triangular filters
    for (size_t i = 0; i < config_.num_mel_filters; ++i) {
        float mel_center = mel_min + (i + 1) * mel_step;
        float mel_left = mel_min + i * mel_step;
        float mel_right = mel_min + (i + 2) * mel_step;
        
        float hz_center = mel_to_hz(mel_center);
        float hz_left = mel_to_hz(mel_left);
        float hz_right = mel_to_hz(mel_right);
        
        size_t bin_left = static_cast<size_t>(hz_left * fft_size / sample_rate);
        size_t bin_center = static_cast<size_t>(hz_center * fft_size / sample_rate);
        size_t bin_right = static_cast<size_t>(hz_right * fft_size / sample_rate);
        
        // Fill triangular filter
        for (size_t j = bin_left; j <= bin_right && j < fft_size / 2 + 1; ++j) {
            float weight = 0.0f;
            if (j <= bin_center) {
                weight = static_cast<float>(j - bin_left) / (bin_center - bin_left);
            } else {
                weight = static_cast<float>(bin_right - j) / (bin_right - bin_center);
            }
            mel_filter_bank_[i * (fft_size / 2 + 1) + j] = weight;
        }
    }
}

void VoiceBias::initializeWindowFunction(size_t window_size) {
    // Initialize Hamming window
    for (size_t i = 0; i < window_size; ++i) {
        window_function_[i] = 0.54f - 0.46f * std::cos(2.0f * M_PI * i / (window_size - 1));
    }
}

float VoiceBias::extractFundamentalFrequency(const std::vector<float>& audio_data,
                                            float sample_rate) {
    if (audio_data.empty()) {
        return 0.0f;
    }
    
    // Simple autocorrelation-based F0 estimation
    size_t min_period = static_cast<size_t>(sample_rate / config_.fundamental_freq_max);
    size_t max_period = static_cast<size_t>(sample_rate / config_.fundamental_freq_min);
    
    if (max_period >= audio_data.size()) {
        return 0.0f;
    }
    
    float best_correlation = 0.0f;
    size_t best_period = 0;
    
    for (size_t period = min_period; period <= max_period; ++period) {
        float correlation = 0.0f;
        size_t samples = audio_data.size() - period;
        
        for (size_t i = 0; i < samples; ++i) {
            correlation += audio_data[i] * audio_data[i + period];
        }
        
        correlation /= samples;
        
        if (correlation > best_correlation) {
            best_correlation = correlation;
            best_period = period;
        }
    }
    
    return best_period > 0 ? sample_rate / best_period : 0.0f;
}

std::vector<float> VoiceBias::extractFormantFrequencies(const std::vector<float>& audio_data,
                                                       float sample_rate) {
    // Simplified formant extraction using spectral peaks
    std::vector<float> magnitude_spectrum = calculateFFTMagnitude(audio_data);
    std::vector<float> formants;
    
    // Find peaks in the spectrum within formant frequency range
    size_t min_bin = static_cast<size_t>(config_.formant_freq_min * magnitude_spectrum.size() * 2 / sample_rate);
    size_t max_bin = static_cast<size_t>(config_.formant_freq_max * magnitude_spectrum.size() * 2 / sample_rate);
    
    for (size_t i = min_bin + 1; i < max_bin - 1 && i < magnitude_spectrum.size(); ++i) {
        if (magnitude_spectrum[i] > magnitude_spectrum[i-1] && 
            magnitude_spectrum[i] > magnitude_spectrum[i+1]) {
            
            float freq = (i * sample_rate) / (2.0f * magnitude_spectrum.size());
            formants.push_back(freq);
            
            if (formants.size() >= 4) break; // Typically track first 4 formants
        }
    }
    
    return formants;
}

std::vector<float> VoiceBias::extractMFCCCoefficients(const std::vector<float>& audio_data,
                                                     float sample_rate) {
    std::vector<float> mfcc(config_.num_mfcc_coeffs, 0.0f);
    
    if (audio_data.empty() || mel_filter_bank_.empty()) {
        return mfcc;
    }
    
    // Get magnitude spectrum
    std::vector<float> magnitude_spectrum = calculateFFTMagnitude(audio_data);
    
    // Apply mel filter bank
    std::vector<float> mel_energies(config_.num_mel_filters, 0.0f);
    size_t fft_bins = magnitude_spectrum.size();
    
    for (size_t i = 0; i < config_.num_mel_filters; ++i) {
        for (size_t j = 0; j < fft_bins; ++j) {
            mel_energies[i] += magnitude_spectrum[j] * mel_filter_bank_[i * fft_bins + j];
        }
        mel_energies[i] = std::log(std::max(mel_energies[i], 1e-10f)); // Log energy
    }
    
    // DCT to get MFCC coefficients
    for (size_t i = 0; i < config_.num_mfcc_coeffs; ++i) {
        for (size_t j = 0; j < config_.num_mel_filters; ++j) {
            mfcc[i] += mel_energies[j] * std::cos(M_PI * i * (j + 0.5f) / config_.num_mel_filters);
        }
        mfcc[i] *= std::sqrt(2.0f / config_.num_mel_filters);
    }
    
    return mfcc;
}

float VoiceBias::calculateHarmonicRatio(const std::vector<float>& audio_data,
                                       float fundamental_freq,
                                       float sample_rate) {
    if (fundamental_freq <= 0 || audio_data.empty()) {
        return 0.0f;
    }
    
    std::vector<float> magnitude_spectrum = calculateFFTMagnitude(audio_data);
    
    float harmonic_energy = 0.0f;
    float total_energy = 0.0f;
    
    // Calculate energy in harmonic frequencies
    for (int harmonic = 1; harmonic <= 10; ++harmonic) {
        float harmonic_freq = fundamental_freq * harmonic;
        if (harmonic_freq > sample_rate / 2) break;
        
        size_t bin = static_cast<size_t>(harmonic_freq * magnitude_spectrum.size() * 2 / sample_rate);
        if (bin < magnitude_spectrum.size()) {
            harmonic_energy += magnitude_spectrum[bin] * magnitude_spectrum[bin];
        }
    }
    
    // Calculate total energy
    for (float mag : magnitude_spectrum) {
        total_energy += mag * mag;
    }
    
    return total_energy > 0 ? harmonic_energy / total_energy : 0.0f;
}

bool VoiceBias::detectInfantDirectedSpeech(const VoiceFeatures& voice_features) {
    // IDS characteristics: higher F0, exaggerated prosody, slower tempo
    bool high_f0 = voice_features.fundamental_frequency > 200.0f;
    bool high_prosody = voice_features.prosody_score > 0.6f;
    bool clear_harmonics = voice_features.harmonic_ratio > 0.4f;
    
    return high_f0 && high_prosody && clear_harmonics;
}

float VoiceBias::calculateVoiceProbability(const VoiceFeatures& voice_features) {
    float probability = 0.0f;
    int factors = 0;
    
    // F0 in human voice range
    if (voice_features.fundamental_frequency >= config_.fundamental_freq_min &&
        voice_features.fundamental_frequency <= config_.fundamental_freq_max) {
        probability += 0.3f;
    }
    ++factors;
    
    // Harmonic content
    probability += voice_features.harmonic_ratio * 0.25f;
    ++factors;
    
    // Formant presence
    if (!voice_features.formant_frequencies.empty()) {
        probability += 0.2f;
    }
    ++factors;
    
    // Spectral characteristics
    if (voice_features.spectral_centroid > 200.0f && voice_features.spectral_centroid < 3000.0f) {
        probability += 0.15f;
    }
    ++factors;
    
    // Zero crossing rate (moderate for voice)
    if (voice_features.zero_crossing_rate > 0.01f && voice_features.zero_crossing_rate < 0.3f) {
        probability += 0.1f;
    }
    ++factors;
    
    return std::min(1.0f, probability);
}

void VoiceBias::applyHammingWindow(std::vector<float>& data) {
    if (data.size() != window_function_.size()) {
        return;
    }
    
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] *= window_function_[i];
    }
}

std::vector<float> VoiceBias::calculateFFTMagnitude(const std::vector<float>& audio_data) {
    // Simplified FFT magnitude calculation
    // In practice, this would use a proper FFT library like FFTW
    
    size_t n = audio_data.size();
    std::vector<float> magnitude(n / 2 + 1, 0.0f);
    
    // Simple DFT for demonstration (very inefficient for large sizes)
    for (size_t k = 0; k < magnitude.size(); ++k) {
        std::complex<float> sum(0.0f, 0.0f);
        for (size_t n_idx = 0; n_idx < n; ++n_idx) {
            float angle = -2.0f * M_PI * k * n_idx / n;
            sum += audio_data[n_idx] * std::complex<float>(std::cos(angle), std::sin(angle));
        }
        magnitude[k] = std::abs(sum);
    }
    
    return magnitude;
}

std::uint64_t VoiceBias::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool VoiceBias::validateVoiceFeatures(const VoiceFeatures& features) const {
    return features.voice_probability >= 0.0f && features.voice_probability <= 1.0f &&
           features.fundamental_frequency >= 0.0f &&
           features.harmonic_ratio >= 0.0f && features.harmonic_ratio <= 1.0f &&
           features.spectral_centroid >= 0.0f &&
           features.zero_crossing_rate >= 0.0f && features.zero_crossing_rate <= 1.0f;
}

} // namespace Biases
} // namespace NeuroForge