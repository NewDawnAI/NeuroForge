#pragma once

#include <vector>
#ifdef NF_HAVE_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#endif
#include <deque>
#include <mutex>
#include <memory>
#include <string>
#include <cstdint>
#include "core/Types.h"

namespace NeuroForge {
namespace Core { class HypergraphBrain; }
namespace Biases {

#ifdef NF_HAVE_OPENCV
/**
 * @brief Advanced Social Perception System
 * 
 * Implements biologically-inspired social cognition mechanisms including:
 * - Face detection and prioritization
 * - Eye coordination and gaze tracking (joint attention)
 * - Lip-sync detection for multimodal speech binding
 * - Social event encoding for episodic memory
 * 
 * This system provides the foundation for social AGI capabilities by enabling
 * the substrate to understand and follow human social cues, attention, and
 * communication patterns.
 */
class SocialPerceptionBias {
public:
    /**
     * @brief Audio buffer for multimodal processing
     */
    struct AudioBuffer {
        std::vector<float> audio_envelope;      ///< Audio amplitude envelope
        std::vector<float> phoneme_features;    ///< Phoneme-level features
        float speech_probability{0.0f};         ///< Probability of speech content
        std::uint64_t timestamp_ms{0};          ///< Audio timestamp
        
        AudioBuffer() = default;
        AudioBuffer(const std::vector<float>& envelope, float speech_prob = 0.0f)
            : audio_envelope(envelope), speech_probability(speech_prob) {}
    };
    
    /**
     * @brief Social event structure for episodic memory integration
     */
    struct SocialEvent {
        cv::Rect face_box;                      ///< Face bounding box (legacy)
        cv::Mat face_mask;                      ///< Face contour mask (NEW)
        cv::Rect gaze_target_box;               ///< Estimated gaze target region
        cv::Point2f gaze_vector;                ///< Normalized gaze direction vector (NEW)
        cv::Rect mouth_region;                  ///< Mouth region for lip tracking
        cv::Mat mouth_mask;                     ///< Precise mouth contour mask (NEW)
        bool is_speaking{false};                ///< Speech detection flag
        float gaze_confidence{0.0f};            ///< Gaze estimation confidence
        float lip_sync_confidence{0.0f};        ///< Lip-sync correlation strength
        float total_salience_boost{1.0f};       ///< Combined attention weight
        std::uint64_t timestamp_ms{0};          ///< Event timestamp
        int tracking_id{-1};                    ///< Face tracking ID
        
        // NEW: Enhanced features for biological realism
        std::vector<cv::Point> face_contour;    ///< Face edge contour points
        std::vector<cv::Point> eye_contours[2]; ///< Left and right eye contours
        cv::Point2f pupil_positions[2];         ///< Left and right pupil centers
        float gaze_angle{0.0f};                 ///< Gaze direction angle (radians)
        float attention_strength{1.0f};         ///< Dynamic attention strength
        
        SocialEvent() = default;
    };
    
    /**
     * @brief Configuration parameters for social perception
     */
    struct Config {
        float face_priority_multiplier{2.0f};   ///< Face attention boost
        float gaze_attention_multiplier{1.5f};  ///< Gaze target attention boost
        float lip_sync_boost{1.8f};             ///< Lip-sync attention boost
        float gaze_projection_distance{200.0f}; ///< Gaze projection distance (pixels)
        float lip_sync_threshold{0.7f};         ///< Minimum correlation for lip-sync
        size_t event_history_size{100};         ///< Maximum stored events
        bool enable_face_detection{true};       ///< Enable face detection
        bool enable_gaze_tracking{true};        ///< Enable gaze tracking
        bool enable_lip_sync{true};             ///< Enable lip-sync detection
        std::string face_cascade_path{"haarcascade_frontalface_alt.xml"};
        std::string eye_cascade_path{"haarcascade_eye.xml"};
        std::string mouth_cascade_path{"haarcascade_smile.xml"};
    };
    
    /**
     * @brief Statistics for social perception performance
     */
    struct Statistics {
        std::uint32_t total_frames_processed{0};
        std::uint32_t faces_detected{0};
        std::uint32_t gaze_events_detected{0};
        std::uint32_t lip_sync_events_detected{0};
        std::uint32_t social_events_created{0};
        float average_face_confidence{0.0f};
        float average_gaze_confidence{0.0f};
        float average_lip_sync_confidence{0.0f};
        std::uint64_t last_update_time{0};
    };

private:
    // OpenCV cascade classifiers
    cv::CascadeClassifier face_cascade_;
    cv::CascadeClassifier eye_cascade_;
    cv::CascadeClassifier mouth_cascade_;
    
    // Configuration and state
    Config config_;
    Statistics stats_;
    std::deque<SocialEvent> recent_events_;
    std::vector<cv::Mat> lip_motion_history_;
    
    // Thread safety
    mutable std::mutex events_mutex_;
    mutable std::mutex stats_mutex_;
    
    // Face tracking
    int next_tracking_id_{1};
    std::vector<std::pair<int, cv::Rect>> tracked_faces_;
    
    // Lip motion analysis
    static constexpr size_t LIP_MOTION_HISTORY_SIZE = 10;
    static constexpr int LIP_PATCH_WIDTH = 32;
    static constexpr int LIP_PATCH_HEIGHT = 32;

    // Brain integration
    NeuroForge::Core::HypergraphBrain* brain_{nullptr};
    int output_grid_size_{32};
    
public:
    /**
     * @brief Constructor with default configuration
     */
    explicit SocialPerceptionBias(const Config& config);
    
    /**
     * @brief Destructor
     */
    ~SocialPerceptionBias() = default;
    
    // Non-copyable but movable
    SocialPerceptionBias(const SocialPerceptionBias&) = delete;
    SocialPerceptionBias& operator=(const SocialPerceptionBias&) = delete;
    SocialPerceptionBias(SocialPerceptionBias&&) = default;
    SocialPerceptionBias& operator=(SocialPerceptionBias&&) = default;
    
    /**
     * @brief Initialize cascade classifiers
     * @return True if initialization successful
     */
    bool initialize();
    
    /**
     * @brief Process frame for social perception
     * @param frame Input video frame
     * @param audio Audio buffer for lip-sync detection
     * @return Vector of detected social events
     */
    std::vector<SocialEvent> processSocialFrame(const cv::Mat& frame,
                                               const AudioBuffer& audio);

    // Overload without audio for callers that don't provide it
    std::vector<SocialEvent> processSocialFrame(const cv::Mat& frame);
    
    /**
     * @brief Apply social bias to feature vector
     * @param features Feature vector to modify
     * @param events Social events from current frame
     * @param grid_size Grid dimensions for feature mapping
     */
    void applySocialBias(std::vector<float>& features, 
                        const std::vector<SocialEvent>& events,
                        int grid_size);

    /**
     * @brief Set brain hook for direct substrate integration
     */
    void setBrain(NeuroForge::Core::HypergraphBrain* brain);

    /**
     * @brief Set output grid size for brain feedExternalPattern
     */
    void setOutputGridSize(int grid_size);
    
    /**
     * @brief Get recent social events
     * @param max_events Maximum number of events to return
     * @return Vector of recent social events
     */
    std::vector<SocialEvent> getRecentSocialEvents(size_t max_events = 10) const;
    
    /**
     * @brief Get current statistics
     * @return Current performance statistics
     */
    Statistics getStatistics() const;
    
    /**
     * @brief Update configuration
     * @param new_config New configuration parameters
     */
    void updateConfig(const Config& new_config);
    
    /**
     * @brief Get current configuration
     * @return Current configuration
     */
    Config getConfig() const { return config_; }
    
    /**
     * @brief Clear all stored events and reset state
     */
    void clear();
    
    /**
     * @brief Check if system is operational
     * @return True if all required components are loaded
     */
    bool isOperational() const;

private:
    // Core detection methods (enhanced for masking and vectors)
    
    /**
     * @brief Detect faces and generate contour masks
     * @param frame Input frame
     * @param faces Output face rectangles (legacy)
     * @param face_masks Output face contour masks (NEW)
     * @param face_contours Output face edge contours (NEW)
     * @return True if faces detected
     */
    bool detectFacesWithMasks(const cv::Mat& frame, 
                             std::vector<cv::Rect>& faces,
                             std::vector<cv::Mat>& face_masks,
                             std::vector<std::vector<cv::Point>>& face_contours);
    
    /**
     * @brief Legacy face detection (for compatibility)
     */
    bool detectFaces(const cv::Mat& frame, std::vector<cv::Rect>& faces);
    
    /**
     * @brief Detect eyes and extract pupil positions
     * @param face_roi Face region of interest
     * @param eyes Output eye rectangles
     * @param pupil_positions Output pupil center positions (NEW)
     * @param eye_contours Output eye contour masks (NEW)
     * @return True if eyes detected
     */
    bool detectEyesWithPupils(const cv::Mat& face_roi, 
                             std::vector<cv::Rect>& eyes,
                             std::vector<cv::Point2f>& pupil_positions,
                             std::vector<std::vector<cv::Point>>& eye_contours);
    
    /**
     * @brief Legacy eye detection (for compatibility)
     */
    bool detectEyes(const cv::Mat& face_roi, std::vector<cv::Rect>& eyes);
    
    /**
     * @brief Detect mouth and generate precise contour mask
     * @param face_roi Face region of interest
     * @param mouth Output mouth rectangle
     * @param mouth_mask Output mouth contour mask (NEW)
     * @return True if mouth detected
     */
    bool detectMouthWithMask(const cv::Mat& face_roi, 
                            cv::Rect& mouth,
                            cv::Mat& mouth_mask);
    
    /**
     * @brief Legacy mouth detection (for compatibility)
     */
    bool detectMouth(const cv::Mat& face_roi, cv::Rect& mouth);
    
    /**
     * @brief Compute vectorized gaze direction from pupil positions
     * @param face Face rectangle
     * @param pupil_positions Left and right pupil centers
     * @param frame_size Frame dimensions
     * @param gaze_vector Output normalized gaze direction vector (NEW)
     * @param gaze_angle Output gaze angle in radians (NEW)
     * @param target_box Output gaze target region (legacy)
     * @return Gaze confidence score
     */
    float computeGazeVector(const cv::Rect& face,
                           const std::vector<cv::Point2f>& pupil_positions,
                           const cv::Size& frame_size,
                           cv::Point2f& gaze_vector,
                           float& gaze_angle,
                           cv::Rect& target_box);
    
    /**
     * @brief Legacy gaze estimation (for compatibility)
     */
    cv::Rect estimateGazeTarget(const cv::Rect& face, 
                               const std::vector<cv::Rect>& eyes,
                               const cv::Size& frame_size);
    
    /**
     * @brief Enhanced lip-sync detection using precise mouth mask
     * @param mouth_mask Precise mouth contour mask (NEW)
     * @param audio Audio buffer for correlation
     * @return Lip-sync confidence score
     */
    float detectLipSyncWithMask(const cv::Mat& mouth_mask, const AudioBuffer& audio);
    
    /**
     * @brief Legacy lip-sync detection (for compatibility)
     */
    float detectLipSync(const cv::Mat& mouth_roi, const AudioBuffer& audio);
    
    /**
     * @brief Generate face contour mask using edge detection
     * @param face_roi Face region of interest
     * @param face_mask Output binary face mask
     * @param face_contour Output contour points
     * @return True if mask generated successfully
     */
    bool generateFaceMask(const cv::Mat& face_roi, 
                         cv::Mat& face_mask,
                         std::vector<cv::Point>& face_contour);
    
    /**
     * @brief Extract pupil position from eye region
     * @param eye_roi Eye region of interest
     * @param pupil_position Output pupil center
     * @param eye_contour Output eye contour points
     * @return True if pupil detected
     */
    bool extractPupilPosition(const cv::Mat& eye_roi,
                             cv::Point2f& pupil_position,
                             std::vector<cv::Point>& eye_contour);
    
    /**
     * @brief Encode masks and vectors into substrate feature grid
     * @param features Output feature vector (32x32 Social grid)
     * @param events Social events with masks and vectors
     * @param grid_size Grid dimensions
     */
    void encodeMasksToGrid(std::vector<float>& features,
                          const std::vector<SocialEvent>& events,
                          int grid_size);
    
    /**
     * @brief Apply dynamic attention based on gaze vectors
     * @param features Feature vector to modify
     * @param gaze_vector Normalized gaze direction
     * @param attention_strength Dynamic attention weight
     * @param grid_size Grid dimensions
     */
    void applyGazeAttention(std::vector<float>& features,
                           const cv::Point2f& gaze_vector,
                           float attention_strength,
                           int grid_size);
    
    /**
     * @brief Calculate joint attention score
     * @param event Social event to analyze
     * @return Joint attention confidence [0,1]
     */
    float calculateJointAttention(const SocialEvent& event);
    
    /**
     * @brief Update face tracking
     * @param faces Current frame face detections
     */
    void updateFaceTracking(const std::vector<cv::Rect>& faces);
    
    /**
     * @brief Get tracking ID for face
     * @param face Face rectangle
     * @return Tracking ID
     */
    int getTrackingId(const cv::Rect& face);
    
    /**
     * @brief Extract lip motion features from mouth ROI
     * @param mouth_roi Mouth region of interest
     * @return Feature vector (mean, stddev, energy)
     */
    std::vector<float> extractLipMotion(const cv::Mat& mouth_roi);
    
    /**
     * @brief Cross-correlation between lip motion and audio envelope
     * @param lip_motion Lip motion feature sequence
     * @param audio_envelope Audio amplitude envelope
     * @return Correlation score
     */
    float crossCorrelate(const std::vector<float>& lip_motion,
                        const std::vector<float>& audio_envelope);
    
    /**
     * @brief Detect rhythmic pattern in signal (speech proxy)
     * @param signal Input signal
     * @return True if rhythmic pattern detected
     */
    bool detectRhythmicPattern(const std::vector<float>& signal);
    
    /**
     * @brief Update statistics based on events
     * @param events Social events
     */
    void updateStatistics(const std::vector<SocialEvent>& events);
    
    /**
     * @brief Apply boost to feature grid region
     * @param features Feature grid
     * @param region Target region rectangle
     * @param boost_factor Multiplicative boost
     * @param grid_size Grid dimension
     */
    void applyRegionBoost(std::vector<float>& features,
                         const cv::Rect& region,
                         float boost_factor,
                         int grid_size);
    
    /**
     * @brief Calculate overlap between rectangles
     */
    float calculateOverlap(const cv::Rect& rect1, const cv::Rect& rect2) const;
    
    /**
     * @brief Get current time in milliseconds
     */
    std::uint64_t getCurrentTimeMs() const;
};

#endif // NF_HAVE_OPENCV

} // namespace Biases
} // namespace NeuroForge