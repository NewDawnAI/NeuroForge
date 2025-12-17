#include "core/LanguageSystem.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <complex>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NeuroForge {
namespace Core {

LanguageSystem::AcousticFeatures LanguageSystem::extractAcousticFeatures(
    const std::vector<float>& audio_samples, float sample_rate) const {
    
    AcousticFeatures features;
    
    if (audio_samples.empty()) {
        return features;
    }
    
    const std::size_t N = audio_samples.size();
    
    // Safety check for very short audio segments
    if (N < 50) {
        // Just calculate energy for very short segments
        float total_energy = 0.0f;
        for (float sample : audio_samples) {
            total_energy += sample * sample;
        }
        features.energy_envelope = std::sqrt(total_energy / N);
        return features;
    }

    const float dt = 1.0f / sample_rate;
    
    // Calculate energy envelope
    float total_energy = 0.0f;
    for (float sample : audio_samples) {
        total_energy += sample * sample;
    }
    features.energy_envelope = std::sqrt(total_energy / N);
    
    // Simple pitch estimation using autocorrelation
    std::vector<float> autocorr(N / 2, 0.0f);
    for (std::size_t lag = 1; lag < N / 2; ++lag) {
        float sum = 0.0f;
        for (std::size_t i = 0; i < N - lag; ++i) {
            sum += audio_samples[i] * audio_samples[i + lag];
        }
        autocorr[lag] = sum / (N - lag);
    }
    
    // Find peak in autocorrelation for fundamental frequency
    auto max_it = std::max_element(autocorr.begin() + 20, autocorr.end()); // Skip very low frequencies
    if (max_it != autocorr.end()) {
        std::size_t peak_lag = std::distance(autocorr.begin(), max_it);
        features.pitch_contour = sample_rate / peak_lag;
    }
    
    // Spectral centroid calculation (simplified)
    float weighted_freq_sum = 0.0f;
    float magnitude_sum = 0.0f;
    
    for (std::size_t i = 1; i < N / 2; ++i) {
        float freq = (i * sample_rate) / N;
        float magnitude = std::abs(audio_samples[i]); // Simplified - should use FFT
        weighted_freq_sum += freq * magnitude;
        magnitude_sum += magnitude;
    }
    
    if (magnitude_sum > 0.0f) {
        features.spectral_centroid = weighted_freq_sum / magnitude_sum;
    }
    
    // Estimate formants (simplified - using spectral peaks)
    std::vector<float> spectral_peaks;
    for (std::size_t i = 2; i < N / 2 - 2; ++i) {
        float current = std::abs(audio_samples[i]);
        float prev = std::abs(audio_samples[i - 1]);
        float next = std::abs(audio_samples[i + 1]);
        
        if (current > prev && current > next && current > 0.1f * features.energy_envelope) {
            float freq = (i * sample_rate) / N;
            spectral_peaks.push_back(freq);
        }
    }
    
    std::sort(spectral_peaks.begin(), spectral_peaks.end());
    
    if (spectral_peaks.size() >= 1) {
        features.formant_f1 = spectral_peaks[0];
    }
    if (spectral_peaks.size() >= 2) {
        features.formant_f2 = spectral_peaks[1];
    }
    
    // Voicing strength based on periodicity
    features.voicing_strength = (max_it != autocorr.end()) ? 
        std::min(1.0f, *max_it / (features.energy_envelope + 1e-6f)) : 0.0f;
    
    // Calculate intonation slope (pitch change over time)
    // Divide audio into segments and track pitch changes
    const std::size_t num_segments = 5;
    const std::size_t segment_size = N / num_segments;
    std::vector<float> pitch_trajectory;
    
    for (std::size_t seg = 0; seg < num_segments && seg * segment_size < N; ++seg) {
        std::size_t start = seg * segment_size;
        std::size_t end = std::min(start + segment_size, N);
        
        // Calculate pitch for this segment
        std::vector<float> seg_autocorr(segment_size / 2, 0.0f);
        for (std::size_t lag = 1; lag < segment_size / 2 && start + lag < end; ++lag) {
            float sum = 0.0f;
            std::size_t count = 0;
            for (std::size_t i = start; i < end - lag; ++i) {
                sum += audio_samples[i] * audio_samples[i + lag];
                count++;
            }
            if (count > 0) {
                seg_autocorr[lag] = sum / count;
            }
        }
        
        // Find peak for this segment (look for fundamental frequency, not harmonics)
        std::size_t search_start = 10;
        std::size_t search_end = std::min(segment_size / 2, seg_autocorr.size());
        
        // Ensure valid range
        if (search_start < search_end) {
            auto seg_max_it = std::max_element(seg_autocorr.begin() + search_start, seg_autocorr.begin() + search_end);
            if (seg_max_it != seg_autocorr.begin() + search_end && *seg_max_it > 0.1f) {
                std::size_t seg_peak_lag = std::distance(seg_autocorr.begin(), seg_max_it);
                float seg_pitch = sample_rate / seg_peak_lag;
                
                // Filter out unrealistic pitch values
                if (seg_pitch >= 50.0f && seg_pitch <= 500.0f) {
                    pitch_trajectory.push_back(seg_pitch);
                }
            }
        }
    }
    
    // Calculate intonation slope from pitch trajectory
    if (pitch_trajectory.size() >= 2) {
        float pitch_start = pitch_trajectory.front();
        float pitch_end = pitch_trajectory.back();
        float duration = N / sample_rate;
        features.intonation_slope = (pitch_end - pitch_start) / duration;
    } else {
        features.intonation_slope = 0.0f;
    }
    
    // Rhythm pattern (simplified - based on energy variations)
    float energy_variance = 0.0f;
    const std::size_t window_size = N / 10;
    for (std::size_t i = 0; i < N - window_size; i += window_size) {
        float window_energy = 0.0f;
        for (std::size_t j = i; j < i + window_size; ++j) {
            window_energy += audio_samples[j] * audio_samples[j];
        }
        window_energy /= window_size;
        energy_variance += (window_energy - features.energy_envelope) * 
                          (window_energy - features.energy_envelope);
    }
    features.rhythm_pattern = std::sqrt(energy_variance / (N / window_size));
    
    return features;
}

float LanguageSystem::calculateSoundSalience(const AcousticFeatures& features) const {
    float salience = 0.0f;
    
    // High-frequency vowels get attention boost
    if (features.formant_f2 > 1500.0f) {
        salience += 0.3f;
    }
    
    // Enhanced rising intonation detection with proper slope calculation
    if (features.intonation_slope > config_.intonation_threshold) {
        // Give extra boost for rising intonation
        float slope_boost = std::min(0.5f, features.intonation_slope / 10.0f);
        salience += config_.prosody_attention_weight + slope_boost;
    }
    
    // Energy and voicing contribute to salience
    salience += features.energy_envelope * 0.2f;
    salience += features.voicing_strength * 0.2f;
    
    // Motherese features boost attention
    salience += features.motherese_score * config_.motherese_boost;
    
    // Novelty contributes to attention
    salience += features.novelty_score * 0.1f;
    
    return std::min(1.0f, salience);
}

LanguageSystem::PhonemeCluster LanguageSystem::generatePhonemeCluster(
    const AcousticFeatures& features) const {
    
    PhonemeCluster cluster;
    
    // Generate IPA-like symbol based on acoustic features
    cluster.phonetic_symbol = phonemeToIPA(features);
    cluster.acoustic_profile = features;
    
    // Create formant pattern
    cluster.formant_pattern = {features.formant_f1, features.formant_f2};
    
    // Classify vowel/consonant ratio
    if (features.voicing_strength > 0.6f && features.formant_f1 > 250.0f) {
        cluster.vowel_consonant_ratio = 0.8f; // Vowel-like
    } else {
        cluster.vowel_consonant_ratio = 0.2f; // Consonant-like
    }
    
    // Generate acoustic variants
    cluster.variants.push_back(cluster.phonetic_symbol);
    
    // Stability based on consistent acoustic features
    cluster.stability_score = (features.voicing_strength + features.energy_envelope) / 2.0f;
    
    return cluster;
}

std::string LanguageSystem::phonemeToIPA(const AcousticFeatures& features) const {
    // Simplified IPA mapping based on acoustic features
    
    if (features.voicing_strength > 0.6f) {
        // Voiced sounds
        if (features.formant_f1 > 600.0f) {
            if (features.formant_f2 > 1800.0f) return "i"; // High front vowel
            else if (features.formant_f2 > 1200.0f) return "e"; // Mid front vowel
            else return "a"; // Low vowel
        } else if (features.formant_f1 > 400.0f) {
            if (features.formant_f2 > 1500.0f) return "ɪ"; // Near-high front vowel
            else return "ʌ"; // Mid central vowel
        } else {
            if (features.formant_f2 > 1000.0f) return "u"; // High back vowel
            else return "o"; // Mid back vowel
        }
    } else {
        // Unvoiced sounds (consonants)
        if (features.spectral_centroid > 3000.0f) {
            return "s"; // High-frequency fricative
        } else if (features.spectral_centroid > 1500.0f) {
            return "ʃ"; // Mid-frequency fricative
        } else if (features.energy_envelope > 0.5f) {
            return "t"; // Stop consonant
        } else {
            return "h"; // Breathy consonant
        }
    }
}

float LanguageSystem::computeMothereseBias(const AcousticFeatures& features) const {
    float motherese_score = 0.0f;
    
    // High pitch typical of infant-directed speech
    if (features.pitch_contour > 200.0f) {
        motherese_score += 0.4f;
    }
    
    // Exaggerated intonation
    if (std::abs(features.intonation_slope) > 1.0f) {
        motherese_score += 0.3f;
    }
    
    // Clear vowel articulation (high F2)
    if (features.formant_f2 > 1500.0f && features.voicing_strength > 0.7f) {
        motherese_score += 0.3f;
    }
    
    return std::min(1.0f, motherese_score);
}

float LanguageSystem::computeIntonationSalience(const std::vector<float>& pitch_contour) const {
    if (pitch_contour.size() < 2) {
        return 0.0f;
    }
    
    float max_change = 0.0f;
    for (std::size_t i = 1; i < pitch_contour.size(); ++i) {
        float change = std::abs(pitch_contour[i] - pitch_contour[i-1]);
        max_change = std::max(max_change, change);
    }
    
    // Rising intonation gets higher salience
    float final_change = pitch_contour.back() - pitch_contour.front();
    if (final_change > config_.intonation_threshold) {
        return std::min(1.0f, max_change / 50.0f + 0.3f);
    }
    
    return std::min(1.0f, max_change / 100.0f);
}

std::vector<float> LanguageSystem::generateProsodicallyEnhancedEmbedding(
    const AcousticFeatures& acoustic_features) const {
    
    std::vector<float> embedding = generateRandomEmbedding();
    
    // Blend acoustic features into embedding
    if (embedding.size() >= 8) {
        embedding[0] = acoustic_features.pitch_contour / 300.0f; // Normalize to [0,1]
        embedding[1] = acoustic_features.energy_envelope;
        embedding[2] = acoustic_features.formant_f1 / 1000.0f;
        embedding[3] = acoustic_features.formant_f2 / 2500.0f;
        embedding[4] = acoustic_features.voicing_strength;
        embedding[5] = acoustic_features.spectral_centroid / 4000.0f;
        embedding[6] = acoustic_features.attention_score;
        embedding[7] = acoustic_features.motherese_score;
    }
    
    return normalizeEmbedding(embedding);
}

float LanguageSystem::calculateAcousticSimilarity(
    const AcousticFeatures& features1, const AcousticFeatures& features2) const {
    
    // Weighted similarity across acoustic dimensions
    float pitch_sim = 1.0f - std::abs(features1.pitch_contour - features2.pitch_contour) / 300.0f;
    float energy_sim = 1.0f - std::abs(features1.energy_envelope - features2.energy_envelope);
    float f1_sim = 1.0f - std::abs(features1.formant_f1 - features2.formant_f1) / 1000.0f;
    float f2_sim = 1.0f - std::abs(features1.formant_f2 - features2.formant_f2) / 2000.0f;
    float voicing_sim = 1.0f - std::abs(features1.voicing_strength - features2.voicing_strength);
    
    // Weighted combination
    float similarity = 0.2f * pitch_sim + 0.2f * energy_sim + 0.25f * f1_sim + 
                      0.25f * f2_sim + 0.1f * voicing_sim;
    
    return std::max(0.0f, std::min(1.0f, similarity));
}

void LanguageSystem::processAcousticTeacherSignal(
    const std::vector<float>& teacher_audio, const std::string& label, float confidence) {
    
    // Extract acoustic features from teacher signal
    AcousticFeatures teacher_features = extractAcousticFeatures(teacher_audio);
    
    // Calculate salience and attention
    teacher_features.attention_score = calculateSoundSalience(teacher_features);
    teacher_features.motherese_score = computeMothereseBias(teacher_features);
    
    // Find or create token for this acoustic pattern
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    std::lock_guard<std::recursive_mutex> acoustic_lock(acoustic_mutex_);
    
    auto it = token_lookup_.find(label);
    if (it == token_lookup_.end()) {
        // Create new token from acoustic teacher signal
        SymbolicToken token;
        token.symbol = label;
        token.type = TokenType::Word;
        token.activation_strength = teacher_features.attention_score * confidence;
        token.usage_count = 1;
        token.last_used = std::chrono::steady_clock::now();
        
        // Generate prosodically enhanced embedding
        token.embedding = generateProsodicallyEnhancedEmbedding(teacher_features);
        
        // Store acoustic associations
        token.sensory_associations["teacher_pitch"] = teacher_features.pitch_contour;
        token.sensory_associations["teacher_energy"] = teacher_features.energy_envelope;
        token.sensory_associations["teacher_f1"] = teacher_features.formant_f1;
        token.sensory_associations["teacher_f2"] = teacher_features.formant_f2;
        
        std::size_t token_id = vocabulary_.size();
        vocabulary_.push_back(std::move(token));
        token_lookup_[label] = token_id;
        
        // Store in acoustic memory
        acoustic_memory_[label] = teacher_features;
        
    } else {
        // Update existing token with new acoustic experience
        SymbolicToken& token = vocabulary_[it->second];
        
        // Blend embeddings based on acoustic similarity
        if (acoustic_memory_.find(label) != acoustic_memory_.end()) {
            float similarity = calculateAcousticSimilarity(teacher_features, acoustic_memory_[label]);
            
            // Higher similarity = more reinforcement
            token.activation_strength += similarity * confidence * config_.mimicry_learning_rate;
            token.activation_strength = std::min(token.activation_strength, 1.0f);
            
            // Update acoustic memory with weighted average
            AcousticFeatures& stored_features = acoustic_memory_[label];
            float blend_weight = similarity * 0.1f;
            stored_features.pitch_contour = (1.0f - blend_weight) * stored_features.pitch_contour + 
                                          blend_weight * teacher_features.pitch_contour;
            stored_features.energy_envelope = (1.0f - blend_weight) * stored_features.energy_envelope + 
                                            blend_weight * teacher_features.energy_envelope;
            stored_features.formant_f1 = (1.0f - blend_weight) * stored_features.formant_f1 + 
                                        blend_weight * teacher_features.formant_f1;
            stored_features.formant_f2 = (1.0f - blend_weight) * stored_features.formant_f2 + 
                                        blend_weight * teacher_features.formant_f2;
        }
        
        token.usage_count++;
        token.last_used = std::chrono::steady_clock::now();
    }
    
    stats_.successful_mimicry_attempts++;
}

std::vector<float> LanguageSystem::generateAudioSnippet(
    const PhonemeCluster& phoneme, float duration_ms) const {
    
    const float sample_rate = 16000.0f; // 16 kHz
    const std::size_t num_samples = static_cast<std::size_t>((duration_ms / 1000.0f) * sample_rate);
    std::vector<float> audio_snippet(num_samples, 0.0f);
    
    const float dt = 1.0f / sample_rate;
    const AcousticFeatures& features = phoneme.acoustic_profile;
    
    // Generate base waveform
    for (std::size_t i = 0; i < num_samples; ++i) {
        float t = i * dt;
        float sample = 0.0f;
        
        if (features.voicing_strength > 0.3f) {
            // Voiced sound - generate harmonic series
            float fundamental_freq = features.pitch_contour;
            
            // Add harmonics with decreasing amplitude
            for (int harmonic = 1; harmonic <= 5; ++harmonic) {
                float freq = fundamental_freq * harmonic;
                float amplitude = features.energy_envelope / (harmonic * harmonic);
                sample += amplitude * std::sin(2.0f * M_PI * freq * t);
            }
            
            // Add formant resonances
            if (features.formant_f1 > 0.0f) {
                float formant_amp = 0.3f * features.energy_envelope;
                sample += formant_amp * std::sin(2.0f * M_PI * features.formant_f1 * t) * 
                         std::exp(-t * 5.0f); // Decay
            }
            
            if (features.formant_f2 > 0.0f) {
                float formant_amp = 0.2f * features.energy_envelope;
                sample += formant_amp * std::sin(2.0f * M_PI * features.formant_f2 * t) * 
                         std::exp(-t * 8.0f); // Faster decay
            }
            
        } else {
            // Unvoiced sound - generate noise with spectral shaping
            float noise = (const_cast<LanguageSystem*>(this)->uniform_dist_(const_cast<std::mt19937&>(rng_)) - 0.5f) * 2.0f;
            
            // Shape noise based on spectral centroid
            float cutoff_freq = features.spectral_centroid;
            float filter_response = 1.0f / (1.0f + (cutoff_freq / (sample_rate / 2.0f)));
            sample = noise * filter_response * features.energy_envelope;
        }
        
        // Apply envelope shaping
        float envelope = 1.0f;
        float attack_time = 0.01f; // 10ms attack
        float release_time = 0.05f; // 50ms release
        
        if (t < attack_time) {
            envelope = t / attack_time;
        } else if (t > (duration_ms / 1000.0f) - release_time) {
            float release_t = t - ((duration_ms / 1000.0f) - release_time);
            envelope = 1.0f - (release_t / release_time);
        }
        
        // Apply prosodic modulation
        if (features.intonation_slope != 0.0f) {
            float pitch_mod = 1.0f + features.intonation_slope * (t / (duration_ms / 1000.0f));
            sample *= pitch_mod;
        }
        
        audio_snippet[i] = sample * envelope;
    }
    
    // Normalize to prevent clipping
    float max_amplitude = *std::max_element(audio_snippet.begin(), audio_snippet.end());
    if (max_amplitude > 0.0f) {
        for (float& sample : audio_snippet) {
            sample /= max_amplitude;
            sample *= 0.8f; // Leave headroom
        }
    }
    
    return audio_snippet;
}

void LanguageSystem::updateAttentionWeights(const std::vector<AcousticFeatures>& acoustic_stream) {
    std::lock_guard<std::recursive_mutex> lock(acoustic_mutex_);
    
    // Update acoustic stream buffer
    for (const auto& features : acoustic_stream) {
        acoustic_stream_buffer_.push_back(features);
        if (acoustic_stream_buffer_.size() > 100) { // Keep last 100 frames
            acoustic_stream_buffer_.pop_front();
        }
    }
    
    // Calculate attention weights based on salience
    attention_history_.clear();
    for (const auto& features : acoustic_stream_buffer_) {
        float attention_weight = calculateSoundSalience(features);
        attention_history_.push_back(attention_weight);
    }
    
    // Update salience threshold based on recent history
    if (!attention_history_.empty()) {
        float mean_attention = std::accumulate(attention_history_.begin(), 
                                             attention_history_.end(), 0.0f) / attention_history_.size();
        current_salience_threshold_ = mean_attention + 0.1f; // Slightly above average
    }
    
    // Boost tokens associated with high-salience acoustic patterns
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    for (auto& token : vocabulary_) {
        if (token.type == TokenType::Phoneme) {
            // Check if token's acoustic profile matches current salient patterns
            auto acoustic_it = acoustic_memory_.find(token.symbol);
            if (acoustic_it != acoustic_memory_.end()) {
                for (const auto& current_features : acoustic_stream) {
                    float similarity = calculateAcousticSimilarity(acoustic_it->second, current_features);
                    if (similarity > 0.7f && current_features.attention_score > current_salience_threshold_) {
                        token.activation_strength += similarity * config_.prosody_attention_weight * 0.1f;
                        token.activation_strength = std::min(token.activation_strength, 1.0f);
                    }
                }
            }
        }
    }
}

std::vector<LanguageSystem::PhonemeCluster> LanguageSystem::clusterAcousticPatterns(
    const std::vector<AcousticFeatures>& feature_sequence) const {
    
    std::vector<PhonemeCluster> clusters;
    
    if (feature_sequence.empty()) {
        return clusters;
    }
    
    // Simple clustering based on acoustic similarity
    std::vector<bool> clustered(feature_sequence.size(), false);
    
    for (std::size_t i = 0; i < feature_sequence.size(); ++i) {
        if (clustered[i]) continue;
        
        PhonemeCluster cluster = generatePhonemeCluster(feature_sequence[i]);
        std::vector<std::size_t> cluster_members = {i};
        clustered[i] = true;
        
        // Find similar acoustic patterns
        for (std::size_t j = i + 1; j < feature_sequence.size(); ++j) {
            if (clustered[j]) continue;
            
            float similarity = calculateAcousticSimilarity(feature_sequence[i], feature_sequence[j]);
            if (similarity > 0.8f) { // High similarity threshold for clustering
                cluster_members.push_back(j);
                clustered[j] = true;
            }
        }
        
        // Update cluster stability based on member count
        cluster.stability_score = std::min(1.0f, cluster_members.size() / 5.0f);
        
        // Generate variants from cluster members
        for (std::size_t member_idx : cluster_members) {
            if (member_idx != i) {
                std::string variant = phonemeToIPA(feature_sequence[member_idx]);
                if (std::find(cluster.variants.begin(), cluster.variants.end(), variant) == cluster.variants.end()) {
                    cluster.variants.push_back(variant);
                }
            }
        }
        
        clusters.push_back(cluster);
    }
    
    return clusters;
}

} // namespace Core
} // namespace NeuroForge