#include "core/LanguageSystem.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <sstream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NeuroForge {
namespace Core {

LanguageSystem::SpeechProductionFeatures LanguageSystem::generateSpeechOutput(
    const std::string& text) const {
    
    // Tokenize text into individual words
    std::vector<std::string> tokens;
    std::istringstream iss(text);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return generateSpeechOutput(tokens);
}

LanguageSystem::SpeechProductionFeatures LanguageSystem::generateSpeechOutput(
    const std::vector<std::string>& token_sequence) const {
    
    SpeechProductionFeatures features;
    
    // Generate phoneme sequence from tokens
    features.phoneme_sequence = generatePhonemeSequence(
        std::accumulate(token_sequence.begin(), token_sequence.end(), std::string(),
                       [](const std::string& a, const std::string& b) {
                           return a.empty() ? b : a + " " + b;
                       }));
    
    // Generate timing pattern (simplified - equal timing for now)
    features.timing_pattern.resize(features.phoneme_sequence.size());
    float base_duration = 200.0f / config_.speech_production_rate; // 200ms base per phoneme
    
    for (std::size_t i = 0; i < features.timing_pattern.size(); ++i) {
        // Vowels get longer duration, consonants shorter
        float duration_multiplier = (features.phoneme_sequence[i].vowel_consonant_ratio > 0.5f) ? 1.2f : 0.8f;
        features.timing_pattern[i] = base_duration * duration_multiplier;
    }
    
    // Generate prosody contour
    features.prosody_contour = generateProsodyContour(features.phoneme_sequence);
    
    // Generate lip motion sequence if enabled
    if (config_.enable_lip_sync) {
        features.lip_motion_sequence = generateLipMotionSequence(features.phoneme_sequence);
    }
    
    // Generate gaze targets if enabled
    if (config_.enable_gaze_coordination) {
        features.gaze_targets.resize(features.phoneme_sequence.size());
        // Default gaze toward listener (0,0 = direct gaze)
        std::fill(features.gaze_targets.begin(), features.gaze_targets.end(), 0.0f);
    }
    
    // Set production parameters
    features.speech_rate = config_.speech_production_rate;
    features.confidence_score = 0.8f; // Default confidence
    features.requires_feedback = true;
    features.start_time = std::chrono::steady_clock::now();
    
    return features;
}

std::vector<LanguageSystem::PhonemeCluster> LanguageSystem::generatePhonemeSequence(
    const std::string& text) const {
    
    std::vector<PhonemeCluster> phonemes;
    
    // Simple text-to-phoneme conversion (could be enhanced with proper phonetic dictionary)
    for (char c : text) {
        if (std::isalpha(c)) {
            PhonemeCluster phoneme;
            
            // Map characters to IPA-like phonemes (simplified)
            char lower_c = std::tolower(c);
            switch (lower_c) {
                case 'a': phoneme.phonetic_symbol = "a"; break;
                case 'e': phoneme.phonetic_symbol = "e"; break;
                case 'i': phoneme.phonetic_symbol = "i"; break;
                case 'o': phoneme.phonetic_symbol = "o"; break;
                case 'u': phoneme.phonetic_symbol = "u"; break;
                case 'm': phoneme.phonetic_symbol = "m"; break;
                case 'n': phoneme.phonetic_symbol = "n"; break;
                case 'p': phoneme.phonetic_symbol = "p"; break;
                case 'b': phoneme.phonetic_symbol = "b"; break;
                case 't': phoneme.phonetic_symbol = "t"; break;
                case 'd': phoneme.phonetic_symbol = "d"; break;
                case 'k': phoneme.phonetic_symbol = "k"; break;
                case 'g': phoneme.phonetic_symbol = "g"; break;
                case 's': phoneme.phonetic_symbol = "s"; break;
                case 'z': phoneme.phonetic_symbol = "z"; break;
                case 'f': phoneme.phonetic_symbol = "f"; break;
                case 'v': phoneme.phonetic_symbol = "v"; break;
                case 'l': phoneme.phonetic_symbol = "l"; break;
                case 'r': phoneme.phonetic_symbol = "r"; break;
                default: phoneme.phonetic_symbol = "ə"; break; // Schwa for unknown
            }
            
            // Set acoustic features based on phoneme type
            if (lower_c == 'a' || lower_c == 'e' || lower_c == 'i' || 
                lower_c == 'o' || lower_c == 'u') {
                // Vowel
                phoneme.vowel_consonant_ratio = 0.9f;
                phoneme.acoustic_profile.voicing_strength = 0.8f;
                phoneme.acoustic_profile.formant_f1 = 400.0f + (lower_c - 'a') * 100.0f;
                phoneme.acoustic_profile.formant_f2 = 1200.0f + (lower_c - 'a') * 200.0f;
            } else {
                // Consonant
                phoneme.vowel_consonant_ratio = 0.1f;
                phoneme.acoustic_profile.voicing_strength = 0.3f;
                phoneme.acoustic_profile.formant_f1 = 200.0f;
                phoneme.acoustic_profile.formant_f2 = 800.0f;
            }
            
            phoneme.acoustic_profile.pitch_contour = 150.0f; // Default pitch
            phoneme.acoustic_profile.energy_envelope = 0.7f;
            phoneme.stability_score = 0.8f;
            
            phonemes.push_back(phoneme);
        }
    }
    
    return phonemes;
}

std::vector<std::vector<float>> LanguageSystem::generateLipMotionSequence(
    const std::vector<PhonemeCluster>& phonemes) const {
    
    std::vector<std::vector<float>> lip_sequence;
    
    for (const auto& phoneme : phonemes) {
        std::vector<float> lip_shape(16, 0.0f); // 16-dimensional lip features
        
        // Generate lip shape based on phoneme characteristics
        std::string symbol = phoneme.phonetic_symbol;
        
        if (symbol == "a") {
            // Open mouth for 'a'
            lip_shape[0] = 0.8f; // mouth opening
            lip_shape[1] = 0.6f; // lip width
            lip_shape[2] = 0.2f; // lip rounding
        } else if (symbol == "o" || symbol == "u") {
            // Rounded lips for 'o', 'u'
            lip_shape[0] = 0.5f; // mouth opening
            lip_shape[1] = 0.3f; // lip width
            lip_shape[2] = 0.9f; // lip rounding
        } else if (symbol == "i" || symbol == "e") {
            // Spread lips for 'i', 'e'
            lip_shape[0] = 0.4f; // mouth opening
            lip_shape[1] = 0.8f; // lip width
            lip_shape[2] = 0.1f; // lip rounding
        } else if (symbol == "m" || symbol == "p" || symbol == "b") {
            // Closed lips for bilabials
            lip_shape[0] = 0.0f; // mouth opening
            lip_shape[1] = 0.5f; // lip width
            lip_shape[2] = 0.3f; // lip rounding
            lip_shape[3] = 0.8f; // lip closure
        } else {
            // Default neutral position
            lip_shape[0] = 0.3f; // mouth opening
            lip_shape[1] = 0.5f; // lip width
            lip_shape[2] = 0.2f; // lip rounding
        }
        
        // Add some variation based on acoustic features
        float voicing_influence = phoneme.acoustic_profile.voicing_strength;
        for (std::size_t i = 4; i < 16; ++i) {
            lip_shape[i] = voicing_influence * (0.1f + (i % 3) * 0.1f);
        }
        
        lip_sequence.push_back(lip_shape);
    }
    
    return lip_sequence;
}

std::vector<float> LanguageSystem::generateProsodyContour(
    const std::vector<PhonemeCluster>& phonemes, float emotional_intensity) const {
    
    std::vector<float> prosody_contour;
    prosody_contour.reserve(phonemes.size());
    
    float base_pitch = 150.0f; // Base fundamental frequency
    float pitch_range = 50.0f + emotional_intensity * 100.0f; // Emotional range
    
    for (std::size_t i = 0; i < phonemes.size(); ++i) {
        float position = static_cast<float>(i) / phonemes.size();
        
        // Generate natural intonation pattern
        float pitch_contour = base_pitch;
        
        // Rising intonation at end (question-like)
        if (position > 0.7f) {
            pitch_contour += pitch_range * (position - 0.7f) / 0.3f;
        }
        
        // Slight declination over utterance
        pitch_contour -= 20.0f * position;
        
        // Add phoneme-specific pitch modifications
        if (phonemes[i].vowel_consonant_ratio > 0.5f) {
            // Vowels carry pitch better
            pitch_contour += 10.0f;
        }
        
        // Emotional coloring
        pitch_contour += emotional_intensity * pitch_range * 
                        std::sin(2.0f * M_PI * position * 2.0f); // Emotional modulation
        
        prosody_contour.push_back(pitch_contour);
    }
    
    return prosody_contour;
}

void LanguageSystem::startSpeechProduction(const SpeechProductionFeatures& speech_features) {
    std::lock_guard<std::recursive_mutex> lock(speech_mutex_);
    
    if (!config_.enable_speech_output) {
        return;
    }
    
    // Initialize speech output state
    speech_output_state_.is_speaking = true;
    speech_output_state_.current_phoneme_index = 0;
    speech_output_state_.current_time_offset = 0.0f;
    speech_output_state_.self_monitoring_score = 0.0f;
    speech_output_state_.caregiver_attention_detected = false;
    
    // Set initial lip shape and gaze
    if (!speech_features.lip_motion_sequence.empty()) {
        speech_output_state_.current_lip_shape = speech_features.lip_motion_sequence[0];
    }
    
    if (!speech_features.gaze_targets.empty()) {
        speech_output_state_.current_gaze_direction = {speech_features.gaze_targets[0], 0.0f};
    }
    
    // Add to production queue
    speech_production_queue_.push_back(speech_features);
    
    // Limit queue size
    if (speech_production_queue_.size() > 5) {
        speech_production_queue_.pop_front();
    }
}

void LanguageSystem::updateSpeechProduction(float delta_time) {
    std::lock_guard<std::recursive_mutex> lock(speech_mutex_);
    
    if (!speech_output_state_.is_speaking || speech_production_queue_.empty()) {
        return;
    }
    
    const auto& current_speech = speech_production_queue_.front();
    
    // Update time offset
    speech_output_state_.current_time_offset += delta_time * 1000.0f; // Convert to ms
    
    // Check if we need to advance to next phoneme
    if (speech_output_state_.current_phoneme_index < current_speech.timing_pattern.size()) {
        float current_phoneme_duration = current_speech.timing_pattern[speech_output_state_.current_phoneme_index];
        
        if (speech_output_state_.current_time_offset >= current_phoneme_duration) {
            // Advance to next phoneme
            speech_output_state_.current_phoneme_index++;
            speech_output_state_.current_time_offset = 0.0f;
            
            // Update lip shape and gaze for new phoneme
            if (speech_output_state_.current_phoneme_index < current_speech.lip_motion_sequence.size()) {
                speech_output_state_.current_lip_shape = 
                    current_speech.lip_motion_sequence[speech_output_state_.current_phoneme_index];
            }
            
            if (speech_output_state_.current_phoneme_index < current_speech.gaze_targets.size()) {
                speech_output_state_.current_gaze_direction = {
                    current_speech.gaze_targets[speech_output_state_.current_phoneme_index], 0.0f};
            }
        }
    }
    
    // Check if speech is complete
    if (speech_output_state_.current_phoneme_index >= current_speech.phoneme_sequence.size()) {
        stopSpeechProduction();
    }
}

void LanguageSystem::stopSpeechProduction() {
    std::lock_guard<std::recursive_mutex> lock(speech_mutex_);
    
    speech_output_state_.is_speaking = false;
    speech_output_state_.current_phoneme_index = 0;
    speech_output_state_.current_time_offset = 0.0f;
    
    // Clear current production from queue
    if (!speech_production_queue_.empty()) {
        speech_production_queue_.pop_front();
    }
    
    // Reset lip shape and gaze to neutral
    speech_output_state_.current_lip_shape.assign(16, 0.3f); // Neutral lip position
    speech_output_state_.current_gaze_direction = {0.0f, 0.0f}; // Direct gaze
}

void LanguageSystem::processSelfAcousticFeedback(const std::vector<float>& heard_audio) {
    std::lock_guard<std::recursive_mutex> lock(speech_mutex_);
    
    if (!speech_output_state_.is_speaking || speech_production_queue_.empty()) {
        return;
    }
    
    // Store acoustic feedback for self-monitoring
    speech_output_state_.acoustic_feedback = heard_audio;
    
    // Calculate self-monitoring score (simplified)
    const auto& current_speech = speech_production_queue_.front();
    if (speech_output_state_.current_phoneme_index < current_speech.phoneme_sequence.size()) {
        
        // Extract acoustic features from feedback
        AcousticFeatures feedback_features = extractAcousticFeatures(heard_audio);
        
        // Compare with intended phoneme
        const auto& intended_phoneme = current_speech.phoneme_sequence[speech_output_state_.current_phoneme_index];
        float similarity = calculateAcousticSimilarity(feedback_features, intended_phoneme.acoustic_profile);
        
        // Update self-monitoring score
        speech_output_state_.self_monitoring_score = similarity;
        
        // Store in history
        self_monitoring_history_.push_back(similarity);
        if (self_monitoring_history_.size() > 100) {
            self_monitoring_history_.erase(self_monitoring_history_.begin());
        }
        
        // Adjust future production based on feedback
        if (similarity < current_speech_quality_threshold_) {
            // Poor quality - slow down speech rate for better articulation
            const_cast<SpeechProductionFeatures&>(current_speech).speech_rate *= 0.9f;
        }
    }
}

// processCaregiverResponse function moved to main LanguageSystem.cpp to avoid duplicate definition

float LanguageSystem::calculateSpeechProductionQuality(
    const SpeechProductionFeatures& intended,
    const std::vector<float>& actual_audio) const {
    
    if (actual_audio.empty() || intended.phoneme_sequence.empty()) {
        return 0.0f;
    }
    
    // Extract acoustic features from actual audio
    AcousticFeatures actual_features = extractAcousticFeatures(actual_audio);
    
    float total_quality = 0.0f;
    std::size_t phoneme_count = 0;
    
    // Compare each intended phoneme with corresponding audio segment
    float segment_duration = static_cast<float>(actual_audio.size()) / intended.phoneme_sequence.size();
    
    for (std::size_t i = 0; i < intended.phoneme_sequence.size(); ++i) {
        // Extract audio segment for this phoneme
        std::size_t start_idx = static_cast<std::size_t>(i * segment_duration);
        std::size_t end_idx = static_cast<std::size_t>((i + 1) * segment_duration);
        end_idx = std::min(end_idx, actual_audio.size());
        
        if (start_idx < end_idx) {
            std::vector<float> phoneme_audio(actual_audio.begin() + start_idx, 
                                           actual_audio.begin() + end_idx);
            
            AcousticFeatures phoneme_features = extractAcousticFeatures(phoneme_audio);
            float similarity = calculateAcousticSimilarity(phoneme_features, 
                                                         intended.phoneme_sequence[i].acoustic_profile);
            
            total_quality += similarity;
            phoneme_count++;
        }
    }
    
    return phoneme_count > 0 ? total_quality / phoneme_count : 0.0f;
}

void LanguageSystem::reinforceCaregiverMimicry(
    const std::string& spoken_token,
    const VisualLanguageFeatures& caregiver_features) {
    
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    std::lock_guard<std::recursive_mutex> visual_lock(visual_mutex_);
    
    // Find token in vocabulary
    auto token_it = token_lookup_.find(spoken_token);
    if (token_it == token_lookup_.end()) {
        return;
    }
    
    std::size_t token_id = token_it->second;
    SymbolicToken& token = vocabulary_[token_id];
    
    // Calculate mimicry reinforcement based on caregiver response
    float mimicry_strength = caregiver_features.face_salience * caregiver_features.gaze_alignment;
    
    if (caregiver_features.lip_sync_score > config_.lip_sync_threshold) {
        // Strong lip-sync correlation indicates successful mimicry
        mimicry_strength += caregiver_features.lip_sync_score * 0.5f;
    }
    
    if (caregiver_features.motherese_face_boost > 0.0f) {
        // Caregiver is using infant-directed speech - strong positive signal
        mimicry_strength += caregiver_features.motherese_face_boost;
    }
    
    // Reinforce token based on mimicry success
    token.activation_strength += mimicry_strength * config_.caregiver_mimicry_boost;
    token.activation_strength = std::min(token.activation_strength, 1.0f);
    
    // Update usage statistics
    token.usage_count++;
    token.last_used = std::chrono::steady_clock::now();
    
    // Associate with caregiver visual features
    associateTokenWithVisualFeatures(token_id, caregiver_features, mimicry_strength);
    
    stats_.successful_mimicry_attempts++;
}

void LanguageSystem::processJointAttentionEvent(
    const std::vector<float>& shared_gaze_target,
    const std::string& spoken_token) {
    
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    std::lock_guard<std::recursive_mutex> visual_lock(visual_mutex_);
    
    // Find or create token
    auto token_it = token_lookup_.find(spoken_token);
    std::size_t token_id;
    
    if (token_it == token_lookup_.end()) {
        token_id = createToken(spoken_token, TokenType::Perception);
    } else {
        token_id = token_it->second;
    }
    
    // Create visual features for joint attention
    VisualLanguageFeatures joint_attention_features;
    joint_attention_features.gaze_vector = shared_gaze_target;
    joint_attention_features.gaze_alignment = 1.0f; // Perfect alignment in joint attention
    joint_attention_features.attention_focus = 0.9f; // High attention focus
    joint_attention_features.speech_vision_coupling = 1.0f; // Strong coupling
    
    // Joint attention is a strong learning signal
    float joint_attention_strength = 0.8f;
    
    // Associate token with joint attention features
    associateTokenWithVisualFeatures(token_id, joint_attention_features, joint_attention_strength);
    
    // Boost token activation
    SymbolicToken& token = vocabulary_[token_id];
    token.activation_strength += joint_attention_strength * config_.visual_grounding_boost;
    token.activation_strength = std::min(token.activation_strength, 1.0f);
    
    // Store gaze target in sensory associations
    token.sensory_associations["joint_attention_x"] = shared_gaze_target.size() > 0 ? shared_gaze_target[0] : 0.0f;
    token.sensory_associations["joint_attention_y"] = shared_gaze_target.size() > 1 ? shared_gaze_target[1] : 0.0f;
    token.sensory_associations["joint_attention_strength"] = joint_attention_strength;
    
    stats_.grounding_associations_formed++;
}

} // namespace Core
} // namespace NeuroForge