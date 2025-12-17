#include "AttachmentBias.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace NeuroForge {
namespace Biases {

AttachmentBias::AttachmentBias(const Config& config) 
    : config_(config), last_caregiver_contact_(std::chrono::steady_clock::now()) {
}

void AttachmentBias::processSocialInteraction(const SocialInteraction& interaction) {
    std::lock_guard<std::mutex> lock(attachment_mutex_);
    
    // Add to interaction history
    interaction_history_.push_back(interaction);
    if (interaction_history_.size() > config_.interaction_history_size) {
        interaction_history_.pop_front();
    }
    
    // Update last contact time
    last_caregiver_contact_ = interaction.timestamp;
    in_separation_distress_ = false;
    
    // Update proximity distance
    current_proximity_distance_ = interaction.proximity_distance;
    
    // Update bonding strength for this caregiver
    updateBondingStrength(interaction.caregiver_id, interaction.interaction_valence);
    
    // Update caregiver profile if exists
    auto it = caregivers_.find(interaction.caregiver_id);
    if (it != caregivers_.end()) {
        auto& profile = it->second;
        profile.last_seen = interaction.timestamp;
        profile.interaction_frequency += 1.0f;
        
        // Update positive interaction ratio
        float total_interactions = profile.interaction_frequency;
        if (interaction.interaction_valence > 0.0f) {
            profile.positive_interaction_ratio = 
                (profile.positive_interaction_ratio * (total_interactions - 1.0f) + 1.0f) / total_interactions;
        } else {
            profile.positive_interaction_ratio = 
                (profile.positive_interaction_ratio * (total_interactions - 1.0f)) / total_interactions;
        }
        
        // Update comfort provision score based on interaction type and valence
        if (interaction.interaction_type == "comfort" || interaction.interaction_type == "soothing") {
            profile.comfort_provision_score = std::min(1.0f, 
                profile.comfort_provision_score + config_.bonding_learning_rate * interaction.interaction_valence);
        }
    }
}

bool AttachmentBias::applyAttachmentBias(std::vector<float>& features,
#ifdef NF_HAVE_OPENCV
                                       const std::vector<cv::Rect>& face_locations,
#else
                                       const std::vector<int>& face_locations,
#endif
                                       const std::vector<float>& voice_features,
                                       int grid_size) {
    std::lock_guard<std::mutex> lock(attachment_mutex_);
    
    if (features.empty() || grid_size <= 0) {
        return false;
    }
    
    // Update separation distress state
    updateSeparationDistress();
    
    // Decay attachment strengths over time
    decayAttachmentStrengths();
    
#ifdef NF_HAVE_OPENCV
    // Apply proximity bias for caregivers
    applyProximityBias(features, face_locations, grid_size);
    
    // Apply stranger wariness
    if (config_.enable_stranger_anxiety) {
        applyStrangerWariness(features, face_locations, grid_size);
    }
    
    // Enhance features for recognized caregivers
    for (const auto& face_rect : face_locations) {
        std::string caregiver_id = identifyCaregiver(face_rect, voice_features);
        
        if (!caregiver_id.empty()) {
            auto it = caregivers_.find(caregiver_id);
            if (it != caregivers_.end()) {
                const auto& profile = it->second;
                
                // Calculate grid positions for face location
                int center_x = face_rect.x + face_rect.width / 2;
                int center_y = face_rect.y + face_rect.height / 2;
                int grid_x = (center_x * grid_size) / 640; // Assuming 640px width
                int grid_y = (center_y * grid_size) / 480; // Assuming 480px height
                
                // Clamp to grid bounds
                grid_x = std::max(0, std::min(grid_size - 1, grid_x));
                grid_y = std::max(0, std::min(grid_size - 1, grid_y));
                
                // Apply attachment bias enhancement
                float enhancement = profile.bonding_strength * 2.0f;
                if (profile.is_primary_caregiver) {
                    enhancement *= 1.5f; // Extra boost for primary caregiver
                }
                
                // Apply enhancement to surrounding grid cells
                int radius = std::max(1, grid_size / 10);
                for (int dy = -radius; dy <= radius; ++dy) {
                    for (int dx = -radius; dx <= radius; ++dx) {
                        int nx = grid_x + dx;
                        int ny = grid_y + dy;
                        
                        if (nx >= 0 && nx < grid_size && ny >= 0 && ny < grid_size) {
                            int idx = ny * grid_size + nx;
                            if (idx < static_cast<int>(features.size())) {
                                float distance = std::sqrt(dx * dx + dy * dy);
                                float weight = std::exp(-distance / radius);
                                features[idx] += enhancement * weight;
                            }
                        }
                    }
                }
            }
        }
    }
#endif // NF_HAVE_OPENCV
    
    return true;
}

AttachmentBias::AttachmentMetrics AttachmentBias::calculateAttachmentMetrics() const {
    std::lock_guard<std::mutex> lock(attachment_mutex_);
    
    AttachmentMetrics metrics;
    
    if (caregivers_.empty()) {
        return metrics;
    }
    
    // Find primary caregiver or strongest bond
    const CaregiverProfile* primary = nullptr;
    float max_bonding = 0.0f;
    
    for (const auto& pair : caregivers_) {
        const auto& profile = pair.second;
        if (profile.is_primary_caregiver || profile.bonding_strength > max_bonding) {
            primary = &profile;
            max_bonding = profile.bonding_strength;
        }
    }
    
    if (primary) {
        metrics.caregiver_recognition_strength = primary->bonding_strength;
        // Calculate voice familiarity based on interaction count (estimated from bonding strength)
        metrics.voice_familiarity = std::min(1.0f, primary->bonding_strength);
        metrics.social_bonding_strength = primary->bonding_strength;
        // Use bonding strength as proxy for attachment security
        metrics.attachment_security = primary->bonding_strength;
        // Use bonding strength as proxy for comfort seeking
        metrics.comfort_seeking = primary->bonding_strength;
    }
    
    // Calculate separation distress
    auto now = std::chrono::steady_clock::now();
    auto time_since_contact = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_caregiver_contact_).count();
    
    if (time_since_contact > config_.separation_distress_threshold) {
        metrics.separation_distress = std::min(1.0f, 
            (time_since_contact - config_.separation_distress_threshold) / config_.separation_distress_threshold);
    }
    
    // Calculate proximity preference
    if (current_proximity_distance_ < config_.proximity_preference_radius) {
        metrics.proximity_preference = 1.0f - (current_proximity_distance_ / config_.proximity_preference_radius);
    }
    
    // Calculate stranger wariness (average across recent unknown faces)
    float total_wariness = 0.0f;
    int wariness_count = 0;
    
    for (const auto& interaction : interaction_history_) {
        if (caregivers_.find(interaction.caregiver_id) == caregivers_.end()) {
            total_wariness += config_.stranger_wariness_threshold;
            wariness_count++;
        }
    }
    
    if (wariness_count > 0) {
        metrics.stranger_wariness = total_wariness / wariness_count;
    }
    
    return metrics;
}

#ifdef NF_HAVE_OPENCV
void AttachmentBias::registerCaregiver(const std::string& caregiver_id,
                                     const cv::Mat& face_template,
                                     const std::vector<float>& voice_features,
                                     bool is_primary) {
    std::lock_guard<std::mutex> lock(attachment_mutex_);
    
    if (caregivers_.size() >= config_.max_caregivers && 
        caregivers_.find(caregiver_id) == caregivers_.end()) {
        return; // Maximum caregivers reached
    }
    
#ifdef NF_HAVE_OPENCV
    CaregiverProfile profile;
    profile.caregiver_id = caregiver_id;
    profile.face_template = face_template.clone();
    profile.voice_features = voice_features;
    profile.is_primary_caregiver = is_primary;
    profile.last_seen = std::chrono::steady_clock::now();
    profile.bonding_strength = is_primary ? 0.3f : 0.1f; // Initial bonding
    
    caregivers_[caregiver_id] = profile;
    
    if (is_primary) {
        primary_caregiver_id_ = caregiver_id;
    }
#endif
}
#endif // NF_HAVE_OPENCV

const AttachmentBias::CaregiverProfile* AttachmentBias::getCaregiverProfile(const std::string& caregiver_id) const {
    std::lock_guard<std::mutex> lock(attachment_mutex_);
    
    auto it = caregivers_.find(caregiver_id);
    return (it != caregivers_.end()) ? &it->second : nullptr;
}

void AttachmentBias::updateBondingStrength(const std::string& caregiver_id, float interaction_valence) {
    auto it = caregivers_.find(caregiver_id);
    if (it != caregivers_.end()) {
        auto& profile = it->second;
        
        // Update bonding strength based on interaction valence
        float delta = config_.bonding_learning_rate * interaction_valence;
        profile.bonding_strength = std::max(0.0f, std::min(1.0f, profile.bonding_strength + delta));
        
        // Primary caregivers get bonus bonding
        if (profile.is_primary_caregiver) {
            profile.bonding_strength = std::min(1.0f, profile.bonding_strength + delta * 0.5f);
        }
    }
}

bool AttachmentBias::isInSeparationDistress() const {
    std::lock_guard<std::mutex> lock(attachment_mutex_);
    return in_separation_distress_;
}

#ifdef NF_HAVE_OPENCV
float AttachmentBias::getStrangerWariness(const cv::Rect& face_location) const {
    std::lock_guard<std::mutex> lock(attachment_mutex_);
    
    if (!config_.enable_stranger_anxiety) {
        return 0.0f;
    }
    
    // Check if this face matches any known caregiver
    for (const auto& pair : caregivers_) {
        const auto& profile = pair.second;
        if (!profile.face_template.empty()) {
            // Simple face matching (in real implementation, use proper face recognition)
            // For now, assume unknown faces trigger wariness
        }
    }
    
    // Return wariness level for unknown faces
    return config_.stranger_wariness_threshold;
}
#endif // NF_HAVE_OPENCV

void AttachmentBias::reset() {
    std::lock_guard<std::mutex> lock(attachment_mutex_);
    
    caregivers_.clear();
    interaction_history_.clear();
    primary_caregiver_id_.clear();
    last_caregiver_contact_ = std::chrono::steady_clock::now();
    in_separation_distress_ = false;
    current_proximity_distance_ = std::numeric_limits<float>::max();
}

std::vector<AttachmentBias::SocialInteraction> AttachmentBias::getInteractionHistory() const {
    std::lock_guard<std::mutex> lock(attachment_mutex_);
    
    return std::vector<SocialInteraction>(interaction_history_.begin(), interaction_history_.end());
}

#ifdef NF_HAVE_OPENCV
float AttachmentBias::calculateFaceSimilarity(const cv::Mat& face1, const cv::Mat& face2) const {
    if (face1.empty() || face2.empty() || face1.size() != face2.size()) {
        return 0.0f;
    }
    
    // Simple correlation-based similarity (in real implementation, use proper face recognition)
    cv::Mat result;
    cv::matchTemplate(face1, face2, result, cv::TM_CCOEFF_NORMED);
    
    double similarity;
    cv::minMaxLoc(result, nullptr, &similarity);
    
    return static_cast<float>(std::max(0.0, similarity));
}
#endif // NF_HAVE_OPENCV

float AttachmentBias::calculateVoiceSimilarity(const std::vector<float>& voice1, 
                                             const std::vector<float>& voice2) const {
    if (voice1.empty() || voice2.empty() || voice1.size() != voice2.size()) {
        return 0.0f;
    }
    
    // Calculate cosine similarity
    float dot_product = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;
    
    for (size_t i = 0; i < voice1.size(); ++i) {
        dot_product += voice1[i] * voice2[i];
        norm1 += voice1[i] * voice1[i];
        norm2 += voice2[i] * voice2[i];
    }
    
    if (norm1 == 0.0f || norm2 == 0.0f) {
        return 0.0f;
    }
    
    return dot_product / (std::sqrt(norm1) * std::sqrt(norm2));
}

#ifdef NF_HAVE_OPENCV
std::string AttachmentBias::identifyCaregiver(const cv::Rect& face_location, 
                                            const std::vector<float>& voice_features) const {
    float best_similarity = 0.0f;
    std::string best_match;
    
    for (const auto& pair : caregivers_) {
        const auto& profile = pair.second;
        
        // Voice similarity
        float voice_sim = calculateVoiceSimilarity(voice_features, profile.voice_features);
        
        // Combined similarity (in real implementation, also use face similarity)
        float combined_similarity = voice_sim;
        
        if (combined_similarity > best_similarity && 
            combined_similarity > config_.voice_recognition_threshold) {
            best_similarity = combined_similarity;
            best_match = pair.first;
        }
    }
    
    return best_match;
}
#endif // NF_HAVE_OPENCV

void AttachmentBias::updateSeparationDistress() {
    if (!config_.enable_separation_distress) {
        return;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_contact = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_caregiver_contact_).count();
    
    in_separation_distress_ = (time_since_contact > config_.separation_distress_threshold);
}

#ifdef NF_HAVE_OPENCV
void AttachmentBias::applyProximityBias(std::vector<float>& features, 
                                      const std::vector<cv::Rect>& face_locations, 
                                      int grid_size) {
    // Enhance features when caregivers are at preferred proximity
    if (current_proximity_distance_ <= config_.proximity_preference_radius) {
        float proximity_bonus = 1.0f - (current_proximity_distance_ / config_.proximity_preference_radius);
        
        // Apply bonus to entire feature map
        for (float& feature : features) {
            feature += proximity_bonus * 0.1f;
        }
    }
}

void AttachmentBias::applyStrangerWariness(std::vector<float>& features,
                                         const std::vector<cv::Rect>& face_locations,
                                         int grid_size) {
    for (const auto& face_rect : face_locations) {
        std::string caregiver_id = identifyCaregiver(face_rect, {});
        
        if (caregiver_id.empty()) {
            // Unknown face - apply wariness
            int center_x = face_rect.x + face_rect.width / 2;
            int center_y = face_rect.y + face_rect.height / 2;
            int grid_x = (center_x * grid_size) / 640;
            int grid_y = (center_y * grid_size) / 480;
            
            grid_x = std::max(0, std::min(grid_size - 1, grid_x));
            grid_y = std::max(0, std::min(grid_size - 1, grid_y));
            
            // Apply wariness suppression
            float wariness = config_.stranger_wariness_threshold;
            int radius = std::max(1, grid_size / 8);
            
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = grid_x + dx;
                    int ny = grid_y + dy;
                    
                    if (nx >= 0 && nx < grid_size && ny >= 0 && ny < grid_size) {
                        int idx = ny * grid_size + nx;
                        if (idx < static_cast<int>(features.size())) {
                            float distance = std::sqrt(dx * dx + dy * dy);
                            float weight = std::exp(-distance / radius);
                            features[idx] -= wariness * weight * 0.5f;
                        }
                    }
                }
            }
        }
    }
}
#endif // NF_HAVE_OPENCV

void AttachmentBias::decayAttachmentStrengths() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto& pair : caregivers_) {
        auto& profile = pair.second;
        
        // Calculate time since last interaction
        auto time_since_interaction = std::chrono::duration_cast<std::chrono::hours>(
            now - profile.last_seen).count();
        
        // Apply decay based on time
        float decay = config_.attachment_decay_rate * time_since_interaction;
        profile.bonding_strength = std::max(0.0f, profile.bonding_strength - decay);
        
        // Primary caregivers decay slower
        if (profile.is_primary_caregiver) {
            profile.bonding_strength = std::max(0.1f, profile.bonding_strength);
        }
    }
}

} // namespace Biases
} // namespace NeuroForge