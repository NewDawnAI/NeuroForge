#include "core/LanguageSystem.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>

namespace NeuroForge {
namespace Core {

void LanguageSystem::associateTokenWithVisualFeatures(
    std::size_t token_id, const VisualLanguageFeatures& visual_features, float confidence) {
    
    std::lock_guard<std::recursive_mutex> visual_lock(visual_mutex_);
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    std::lock_guard<std::recursive_mutex> grounding_lock(grounding_mutex_);
    
    if (token_id >= vocabulary_.size()) {
        return;
    }
    
    // Store visual features for this token
    token_visual_features_[token_id].push_back(visual_features);
    
    // Limit history to prevent memory bloat
    if (token_visual_features_[token_id].size() > 10) {
        token_visual_features_[token_id].erase(token_visual_features_[token_id].begin());
    }
    
    // Update token activation based on visual salience
    SymbolicToken& token = vocabulary_[token_id];
    float visual_boost = visual_features.face_salience * config_.visual_grounding_boost;
    visual_boost += visual_features.attention_focus * config_.gaze_attention_weight;
    visual_boost += visual_features.motherese_face_boost * config_.motherese_boost;
    
    token.activation_strength += visual_boost * confidence;
    token.activation_strength = std::min(token.activation_strength, 1.0f);
    
    // Store visual associations in token's sensory associations
    token.sensory_associations["face_salience"] = visual_features.face_salience;
    token.sensory_associations["gaze_alignment"] = visual_features.gaze_alignment;
    token.sensory_associations["lip_sync"] = visual_features.lip_sync_score;
    token.sensory_associations["attention_focus"] = visual_features.attention_focus;
    
    stats_.grounding_associations_formed++;
}

void LanguageSystem::processFaceSpeechEvent(
    const std::vector<float>& face_embedding,
    const std::vector<float>& gaze_vector,
    const std::vector<float>& lip_features,
    const std::string& spoken_token,
    float temporal_alignment) {
    
    std::lock_guard<std::recursive_mutex> visual_lock(visual_mutex_);
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    // Find or create token for spoken word
    auto token_it = token_lookup_.find(spoken_token);
    std::size_t token_id;
    
    if (token_it == token_lookup_.end()) {
        // Create new token with visual grounding
        token_id = createToken(spoken_token, TokenType::Word);
    } else {
        token_id = token_it->second;
    }
    
    // Create visual-language features
    VisualLanguageFeatures visual_features;
    visual_features.face_embedding = face_embedding;
    visual_features.gaze_vector = gaze_vector;
    visual_features.lip_features = lip_features;
    
    // Calculate face salience based on embedding strength
    if (!face_embedding.empty()) {
        float face_magnitude = 0.0f;
        for (float val : face_embedding) {
            face_magnitude += val * val;
        }
        // Normalize by sqrt of size to get RMS value, then scale appropriately
        visual_features.face_salience = std::sqrt(face_magnitude / face_embedding.size());
    }
    
    // Calculate gaze alignment (simplified - based on vector consistency)
    if (gaze_vector.size() >= 2) {
        float gaze_magnitude = std::sqrt(gaze_vector[0] * gaze_vector[0] + 
                                       gaze_vector[1] * gaze_vector[1]);
        visual_features.gaze_alignment = std::min(1.0f, gaze_magnitude);
    }
    
    // Calculate lip-sync score based on lip feature variation
    if (!lip_features.empty()) {
        float lip_variation = 0.0f;
        float mean_lip = std::accumulate(lip_features.begin(), lip_features.end(), 0.0f) / lip_features.size();
        for (float val : lip_features) {
            lip_variation += (val - mean_lip) * (val - mean_lip);
        }
        visual_features.lip_sync_score = std::sqrt(lip_variation / lip_features.size());
    }
    
    // Calculate speech-vision coupling based on temporal alignment
    visual_features.speech_vision_coupling = temporal_alignment;
    
    // Boost for motherese + face combination
    if (visual_features.face_salience > 0.6f && temporal_alignment > 0.8f) {
        visual_features.motherese_face_boost = config_.face_language_coupling;
    }
    
    // Associate token with visual features
    associateTokenWithVisualFeatures(token_id, visual_features, temporal_alignment);
    
    // Create cross-modal association
    CrossModalAssociation association;
    association.token_id = token_id;
    association.modality = "vision";
    association.pattern = face_embedding;
    association.association_strength = visual_features.face_salience * temporal_alignment;
    association.temporal_alignment = temporal_alignment;
    association.visual_features = visual_features;
    
    association.face_language_confidence = calculateFaceLanguageConfidence(visual_features, AcousticFeatures{});
    association.last_reinforced = std::chrono::steady_clock::now();
    
    // Store cross-modal association
    {
        std::lock_guard<std::recursive_mutex> grounding_lock(grounding_mutex_);
        cross_modal_associations_.push_back(association);
        
        // Limit association history
        if (cross_modal_associations_.size() > 1000) {
            cross_modal_associations_.erase(cross_modal_associations_.begin());
        }
    }
}

float LanguageSystem::calculateFaceLanguageConfidence(
    const VisualLanguageFeatures& visual_features,
    const AcousticFeatures& acoustic_features) const {
    
    float confidence = 0.0f;
    
    // Face salience contributes to confidence
    confidence += visual_features.face_salience * 0.3f;
    
    // Gaze alignment indicates attention
    confidence += visual_features.gaze_alignment * 0.2f;
    
    // Lip-sync correlation is strong indicator
    confidence += visual_features.lip_sync_score * 0.4f;
    
    // Speech-vision temporal coupling
    confidence += visual_features.speech_vision_coupling * 0.1f;
    
    // Acoustic features enhance confidence if available
    if (acoustic_features.energy_envelope > 0.0f) {
        // Motherese + face is highly confident
        confidence += acoustic_features.motherese_score * visual_features.face_salience * 0.3f;
        
        // Clear speech with face detection
        if (acoustic_features.voicing_strength > 0.6f && visual_features.face_salience > 0.5f) {
            confidence += 0.2f;
        }
    }
    
    return std::min(1.0f, confidence);
}

void LanguageSystem::processVisualAttentionMap(
    const std::vector<float>& attention_map,
    const std::vector<std::string>& active_tokens) {
    
    std::lock_guard<std::recursive_mutex> visual_lock(visual_mutex_);
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    // Guard against empty attention maps to avoid undefined behavior
    if (attention_map.empty()) {
        current_attention_map_.clear();
        return; // Nothing to process
    }
    
    current_attention_map_ = attention_map;
    
    // Calculate overall attention focus
    float total_attention = std::accumulate(attention_map.begin(), attention_map.end(), 0.0f);
    float max_attention = *std::max_element(attention_map.begin(), attention_map.end());
    
    // Boost tokens that correspond to high-attention visual areas
    for (const std::string& token_str : active_tokens) {
        auto token_it = token_lookup_.find(token_str);
        if (token_it != token_lookup_.end()) {
            std::size_t token_id = token_it->second;
            SymbolicToken& token = vocabulary_[token_id];
            
            // Boost activation based on attention focus
            float attention_boost = (max_attention / (total_attention + 1e-6f)) * config_.gaze_attention_weight;
            token.activation_strength += attention_boost;
            token.activation_strength = std::min(token.activation_strength, 1.0f);
            
            // Update visual associations
            token.sensory_associations["visual_attention"] = max_attention;
        }
    }
}

void LanguageSystem::reinforceVisualGrounding(
    std::size_t token_id, const std::vector<float>& visual_pattern, float salience_score) {
    
    std::lock_guard<std::recursive_mutex> grounding_lock(grounding_mutex_);
    std::lock_guard<std::recursive_mutex> vocab_lock(vocabulary_mutex_);
    
    if (token_id >= vocabulary_.size()) {
        return;
    }
    
    // Find existing visual associations for this token
    bool found_existing = false;
    for (auto& association : cross_modal_associations_) {
        if (association.token_id == token_id && association.modality == "vision") {
            // Reinforce existing association
            float similarity = cosineSimilarity(association.pattern, visual_pattern);
            if (similarity > 0.7f) {
                association.association_strength += salience_score * config_.visual_grounding_boost * 0.1f;
                association.association_strength = std::min(association.association_strength, 1.0f);
                association.last_reinforced = std::chrono::steady_clock::now();
                found_existing = true;
                break;
            }
        }
    }
    
    if (!found_existing) {
        // Create new visual association
        CrossModalAssociation new_association;
        new_association.token_id = token_id;
        new_association.modality = "vision";
        new_association.pattern = visual_pattern;
        new_association.association_strength = salience_score * config_.visual_grounding_boost;
        new_association.temporal_alignment = 1.0f;
        new_association.last_reinforced = std::chrono::steady_clock::now();
        
        cross_modal_associations_.push_back(new_association);
    }
    
    // Update token activation
    SymbolicToken& token = vocabulary_[token_id];
    token.activation_strength += salience_score * config_.visual_grounding_boost;
    token.activation_strength = std::min(token.activation_strength, 1.0f);
    
    stats_.grounding_associations_formed++;
}

std::vector<std::size_t> LanguageSystem::getTokensForVisualPattern(
    const std::vector<float>& visual_pattern, float similarity_threshold) const {
    
    std::lock_guard<std::recursive_mutex> grounding_lock(grounding_mutex_);
    
    std::vector<std::size_t> matching_tokens;
    
    for (const auto& association : cross_modal_associations_) {
        if (association.modality == "vision") {
            float similarity = cosineSimilarity(association.pattern, visual_pattern);
            if (similarity >= similarity_threshold) {
                matching_tokens.push_back(association.token_id);
            }
        }
    }
    
    // Remove duplicates
    std::sort(matching_tokens.begin(), matching_tokens.end());
    matching_tokens.erase(std::unique(matching_tokens.begin(), matching_tokens.end()), 
                         matching_tokens.end());
    
    return matching_tokens;
}

std::vector<LanguageSystem::CrossModalAssociation> LanguageSystem::getCrossModalAssociations(
    std::size_t token_id) const {
    
    std::lock_guard<std::recursive_mutex> grounding_lock(grounding_mutex_);
    
    std::vector<CrossModalAssociation> token_associations;
    
    for (const auto& association : cross_modal_associations_) {
        if (association.token_id == token_id) {
            token_associations.push_back(association);
        }
    }
    
    return token_associations;
}

void LanguageSystem::updateCrossModalAssociations(
    const std::vector<CrossModalAssociation>& associations) {
    
    std::lock_guard<std::recursive_mutex> grounding_lock(grounding_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    // Update existing associations and add new ones
    for (const auto& new_association : associations) {
        bool found_existing = false;
        
        for (auto& existing : cross_modal_associations_) {
            if (existing.token_id == new_association.token_id && 
                existing.modality == new_association.modality) {
                
                // Check pattern similarity
                float similarity = cosineSimilarity(existing.pattern, new_association.pattern);
                if (similarity > 0.8f) {
                    // Update existing association
                    existing.association_strength = std::max(existing.association_strength, 
                                                           new_association.association_strength);
                    existing.temporal_alignment = new_association.temporal_alignment;
                    existing.visual_features = new_association.visual_features;
                    existing.face_language_confidence = new_association.face_language_confidence;
                    existing.last_reinforced = now;
                    found_existing = true;
                    break;
                }
            }
        }
        
        if (!found_existing) {
            CrossModalAssociation association = new_association;
            association.last_reinforced = now;
            cross_modal_associations_.push_back(association);
        }
    }
    
    // Decay old associations
    for (auto& association : cross_modal_associations_) {
        auto time_since_reinforcement = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - association.last_reinforced).count();
        
        if (time_since_reinforcement > 10000) { // 10 seconds
            association.association_strength *= (1.0f - config_.cross_modal_decay);
        }
    }
    
    // Remove very weak associations
    cross_modal_associations_.erase(
        std::remove_if(cross_modal_associations_.begin(), cross_modal_associations_.end(),
                      [](const CrossModalAssociation& assoc) {
                          return assoc.association_strength < 0.1f;
                      }),
        cross_modal_associations_.end());
}

} // namespace Core
} // namespace NeuroForge