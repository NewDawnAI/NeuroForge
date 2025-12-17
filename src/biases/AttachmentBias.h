#pragma once

#include <vector>
#include <deque>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <string>
#include <chrono>

#ifdef NF_HAVE_OPENCV
#include <opencv2/opencv.hpp>
#endif

namespace NeuroForge {
namespace Biases {

/**
 * @brief Attachment Bias System for Caregiver Recognition and Social Bonding
 * 
 * Implements biologically-inspired attachment mechanisms including:
 * - Caregiver face recognition and preference
 * - Voice recognition and familiarity assessment
 * - Proximity seeking and separation distress
 * - Social bonding strength calculation
 * - Attachment security assessment
 * - Stranger anxiety and wariness responses
 */
class AttachmentBias {
public:
    /**
     * @brief Attachment metrics for social bonding assessment
     */
    struct AttachmentMetrics {
        float caregiver_recognition_strength{0.0f};  // How well caregiver is recognized
        float voice_familiarity{0.0f};               // Familiarity with caregiver voice
        float proximity_preference{0.0f};            // Preference for being near caregiver
        float separation_distress{0.0f};             // Distress when separated
        float social_bonding_strength{0.0f};         // Overall attachment strength
        float stranger_wariness{0.0f};               // Wariness of unfamiliar people
        float attachment_security{0.0f};             // Security of attachment bond
        float comfort_seeking{0.0f};                 // Tendency to seek comfort
    };

    /**
     * @brief Caregiver profile for recognition and bonding
     */
    struct CaregiverProfile {
        std::string caregiver_id;                    // Unique identifier
#ifdef NF_HAVE_OPENCV
        cv::Mat face_template;                       // Face recognition template
#endif
        std::vector<float> voice_features;           // Voice characteristic features
        float interaction_frequency{0.0f};          // How often interactions occur
        float positive_interaction_ratio{0.0f};     // Ratio of positive interactions
        float comfort_provision_score{0.0f};        // How much comfort this person provides
        std::chrono::steady_clock::time_point last_seen; // Last interaction time
        float bonding_strength{0.0f};               // Strength of attachment bond
        bool is_primary_caregiver{false};           // Primary attachment figure
    };

    /**
     * @brief Social interaction event for attachment learning
     */
    struct SocialInteraction {
        std::string caregiver_id;                    // Who was involved
#ifdef NF_HAVE_OPENCV
        cv::Rect face_location;                      // Where face was detected
#endif
        std::vector<float> voice_features;           // Voice characteristics
        float interaction_valence{0.0f};            // Positive/negative interaction
        float proximity_distance{0.0f};             // Physical distance
        float interaction_duration{0.0f};           // How long interaction lasted
        std::chrono::steady_clock::time_point timestamp; // When it occurred
        std::string interaction_type;               // Type of interaction
    };

    /**
     * @brief Configuration parameters for attachment system
     */
    struct Config {
        size_t max_caregivers{10};                   // Maximum number of caregivers to track
        size_t interaction_history_size{1000};      // Maximum interaction history
        float face_recognition_threshold{0.7f};     // Threshold for face recognition
        float voice_recognition_threshold{0.6f};    // Threshold for voice recognition
        float bonding_learning_rate{0.05f};         // Rate of bonding strength updates
        float separation_distress_threshold{300.0f}; // Time (seconds) before distress
        float proximity_preference_radius{2.0f};    // Preferred proximity distance (meters)
        float stranger_wariness_threshold{0.3f};    // Threshold for stranger detection
        float attachment_decay_rate{0.001f};        // Rate of attachment decay over time
        bool enable_stranger_anxiety{true};         // Enable stranger anxiety responses
        bool enable_separation_distress{true};      // Enable separation distress
        bool enable_comfort_seeking{true};          // Enable comfort-seeking behavior
    };

    /**
     * @brief Constructor
     * @param config Configuration parameters
     */
    explicit AttachmentBias(const Config& config);

    /**
     * @brief Destructor
     */
    ~AttachmentBias() = default;

    /**
     * @brief Process social interaction for attachment learning
     * @param interaction Social interaction event
     */
    void processSocialInteraction(const SocialInteraction& interaction);

    /**
     * @brief Apply attachment bias to neural features
     * @param features Neural features to modify
     * @param face_locations Detected face locations
     * @param voice_features Current voice features
     * @param grid_size Size of neural grid
     * @return True if bias was applied successfully
     */
    bool applyAttachmentBias(std::vector<float>& features,
#ifdef NF_HAVE_OPENCV
                           const std::vector<cv::Rect>& face_locations,
#else
                           const std::vector<int>& face_locations,
#endif
                           const std::vector<float>& voice_features,
                           int grid_size);

    /**
     * @brief Calculate current attachment metrics
     * @return Current attachment state metrics
     */
    AttachmentMetrics calculateAttachmentMetrics() const;

    /**
     * @brief Register a new caregiver
     * @param caregiver_id Unique identifier for caregiver
     * @param face_template Face recognition template
     * @param voice_features Voice characteristic features
     * @param is_primary Whether this is the primary caregiver
     */
#ifdef NF_HAVE_OPENCV
    void registerCaregiver(const std::string& caregiver_id,
                          const cv::Mat& face_template,
                          const std::vector<float>& voice_features,
                          bool is_primary = false);
#endif // NF_HAVE_OPENCV

    /**
     * @brief Get caregiver profile by ID
     * @param caregiver_id Caregiver identifier
     * @return Caregiver profile or nullptr if not found
     */
    const CaregiverProfile* getCaregiverProfile(const std::string& caregiver_id) const;

    /**
     * @brief Update caregiver bonding strength
     * @param caregiver_id Caregiver identifier
     * @param interaction_valence Positive/negative interaction value
     */
    void updateBondingStrength(const std::string& caregiver_id, float interaction_valence);

    /**
     * @brief Check if currently experiencing separation distress
     * @return True if in separation distress state
     */
    bool isInSeparationDistress() const;

    /**
     * @brief Get stranger wariness level for unknown faces
     * @param face_location Location of unknown face
     * @return Wariness level (0.0 = no wariness, 1.0 = high wariness)
     */
#ifdef NF_HAVE_OPENCV
    float getStrangerWariness(const cv::Rect& face_location) const;
#endif

    /**
     * @brief Reset attachment system
     */
    void reset();

    /**
     * @brief Get configuration
     * @return Current configuration
     */
    const Config& getConfig() const { return config_; }

    /**
     * @brief Get interaction history
     * @return Vector of recent social interactions
     */
    std::vector<SocialInteraction> getInteractionHistory() const;

private:
    Config config_;
    mutable std::mutex attachment_mutex_;

    // Caregiver tracking
    std::unordered_map<std::string, CaregiverProfile> caregivers_;
    std::string primary_caregiver_id_;

    // Interaction history
    std::deque<SocialInteraction> interaction_history_;

    // Current state
    std::chrono::steady_clock::time_point last_caregiver_contact_;
    bool in_separation_distress_{false};
    float current_proximity_distance_{std::numeric_limits<float>::max()};

    // Internal methods
#ifdef NF_HAVE_OPENCV
    float calculateFaceSimilarity(const cv::Mat& face1, const cv::Mat& face2) const;
#endif
    float calculateVoiceSimilarity(const std::vector<float>& voice1, 
                                  const std::vector<float>& voice2) const;
#ifdef NF_HAVE_OPENCV
    std::string identifyCaregiver(const cv::Rect& face_location, 
                                 const std::vector<float>& voice_features) const;
#endif
    void updateSeparationDistress();
#ifdef NF_HAVE_OPENCV
    void applyProximityBias(std::vector<float>& features, 
                           const std::vector<cv::Rect>& face_locations, 
                           int grid_size);
    void applyStrangerWariness(std::vector<float>& features,
                              const std::vector<cv::Rect>& face_locations,
                              int grid_size);
#endif
    void decayAttachmentStrengths();
};

} // namespace Biases
} // namespace NeuroForge