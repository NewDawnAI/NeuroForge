#include "biases/SocialPerceptionBias.h"
#include "audio_capture.h"
#include "core/HypergraphBrain.h"
#include "connectivity/ConnectivityManager.h"
#ifdef NF_HAVE_OPENCV
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#endif
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

using namespace NeuroForge::Biases;
using namespace NeuroForge::Audio;
using namespace NeuroForge::Core;

class SocialPerceptionDemo {
private:
#ifdef NF_HAVE_OPENCV
    SocialPerceptionBias social_bias_;
#endif
#ifdef NF_HAVE_OPENCV
    cv::VideoCapture camera_;
#endif
    AudioCapture audio_capture_;
    bool running_;
    bool show_debug_info_;
    bool show_gaze_tracking_;
    bool show_lip_sync_;
    
    // Neural substrate components
    std::shared_ptr<NeuroForge::Connectivity::ConnectivityManager> connectivity_manager_;
    std::unique_ptr<HypergraphBrain> brain_;
    NeuroForge::RegionID social_region_id_;
    
    // Performance metrics
    std::chrono::steady_clock::time_point last_frame_time_;
    double fps_;
    int frame_count_;
    
    // Audio processing
    bool audio_enabled_;
    
#ifdef NF_HAVE_OPENCV
    // Visualization colors
    const cv::Scalar FACE_COLOR = cv::Scalar(0, 255, 0);      // Green
    const cv::Scalar GAZE_COLOR = cv::Scalar(255, 0, 0);      // Blue
    const cv::Scalar MOUTH_COLOR = cv::Scalar(0, 255, 255);   // Yellow
    const cv::Scalar SPEAKING_COLOR = cv::Scalar(0, 0, 255);  // Red
    const cv::Scalar CONTOUR_COLOR = cv::Scalar(255, 255, 0); // Cyan for contours
    const cv::Scalar PUPIL_COLOR = cv::Scalar(255, 255, 255); // White for pupils
    
    // Visualization modes
    bool show_masks_;
    bool show_contours_;
    bool show_vectors_;
#endif
    
public:
    SocialPerceptionDemo() 
        : 
#ifdef NF_HAVE_OPENCV
          social_bias_(SocialPerceptionBias::Config{}), 
#endif
          audio_capture_(AudioCapture::Config{}),
          running_(false), show_debug_info_(true), show_gaze_tracking_(true), 
          show_lip_sync_(true), fps_(0.0), frame_count_(0), audio_enabled_(false),
          social_region_id_(0)
#ifdef NF_HAVE_OPENCV
          , show_masks_(true), show_contours_(true), show_vectors_(true)
#endif
    {
        
        last_frame_time_ = std::chrono::steady_clock::now();

#ifdef NF_HAVE_OPENCV
        // Tune config for stronger lip-sync detection using mouth cascade
        {
            auto cfg = social_bias_.getConfig();
            cfg.mouth_cascade_path = "haarcascade_mcs_mouth.xml"; // Prefer mouth-specific cascade
            cfg.lip_sync_threshold = 0.6f; // Slightly lower threshold to capture more speech events
            cfg.lip_sync_boost = 2.2f;     // Boost salience when speaking is detected
            social_bias_.updateConfig(cfg);
        }
#endif
        
        // Initialize neural substrate
        initializeBrain();
    }
    
    bool initialize() {
        std::cout << "Initializing Social Perception Demo..." << std::endl;
        
#ifdef NF_HAVE_OPENCV
        // Initialize social perception system
        if (!social_bias_.initialize()) {
            std::cout << "Warning: Some cascade classifiers failed to load. Demo will continue with available features." << std::endl;
        }
        
        // Initialize camera
        camera_.open(0);  // Default camera
        if (!camera_.isOpened()) {
            std::cerr << "Error: Could not open camera!" << std::endl;
            return false;
        }
        
        // Set camera properties
        camera_.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        camera_.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        camera_.set(cv::CAP_PROP_FPS, 30);
        
        std::cout << "Camera initialized: " 
                  << camera_.get(cv::CAP_PROP_FRAME_WIDTH) << "x" 
                  << camera_.get(cv::CAP_PROP_FRAME_HEIGHT) 
                  << " @ " << camera_.get(cv::CAP_PROP_FPS) << " FPS" << std::endl;
#else
        std::cout << "OpenCV not available - running in audio-only mode" << std::endl;
#endif
        
        // Initialize audio (simplified - just generate mock audio for demo)
        initializeAudio();
        
        std::cout << "Social Perception Demo initialized successfully!" << std::endl;
        return true;
    }
    
    void run() {
        if (!initialize()) {
            return;
        }
        
        running_ = true;
        
        std::cout << "\n=== Social Perception Demo Controls ===" << std::endl;
        std::cout << "ESC/Q: Quit" << std::endl;
        std::cout << "D: Toggle debug info" << std::endl;
        std::cout << "G: Toggle gaze tracking visualization" << std::endl;
        std::cout << "L: Toggle lip-sync visualization" << std::endl;
        std::cout << "A: Toggle audio processing" << std::endl;
        std::cout << "R: Reset statistics" << std::endl;
        std::cout << "======================================\n" << std::endl;
        
#ifdef NF_HAVE_OPENCV
        cv::Mat frame;
        
        while (running_) {
            // Capture frame
            if (!camera_.read(frame)) {
                std::cerr << "Error: Could not read frame from camera!" << std::endl;
                break;
            }
            
            // Update FPS
            updateFPS();
            
            // Get real audio data from microphone
            SocialPerceptionBias::AudioBuffer audio = getRealAudio();
            
            // Process frame for social perception
            auto social_events = social_bias_.processSocialFrame(frame, audio);
            
            // Note: Skipping applySocialBias for demo to avoid size mismatch issues
            // In production, this would apply attention weights to feature vectors
            
            try {
                // Visualize results
                cv::Mat display_frame = frame.clone();
                visualizeSocialEvents(display_frame, social_events);
                
                // Add debug information
                if (show_debug_info_) {
                    addDebugInfo(display_frame, social_events);
                }
                
                // Display frame
                cv::imshow("NeuroForge Social Perception Demo", display_frame);
            } catch (const cv::Exception& e) {
                std::cerr << "OpenCV processing error: " << e.what() << std::endl;
                // Continue with basic frame display
                cv::imshow("NeuroForge Social Perception Demo", frame);
            } catch (const std::exception& e) {
                std::cerr << "Processing error: " << e.what() << std::endl;
                // Continue with basic frame display
                cv::imshow("NeuroForge Social Perception Demo", frame);
            }
            
            // Handle keyboard input
            int key = cv::waitKey(1) & 0xFF;
            if (!handleKeyPress(key)) {
                break;
            }
        }
#else
        std::cout << "OpenCV not available - running in audio-only mode" << std::endl;
        while (running_) {
#ifdef NF_HAVE_OPENCV
            // Get real audio data from microphone
            SocialPerceptionBias::AudioBuffer audio = getRealAudio();
            
            // Process audio for social perception (without video)
            // This would be a simplified version for audio-only processing
#endif
            
            // Simple delay to prevent busy loop
            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS equivalent
            
            // Check for exit condition (simplified without OpenCV key handling)
            // In a real implementation, you might use console input or other methods
            static int counter = 0;
            if (++counter > 3000) { // Auto-exit after ~100 seconds
                std::cout << "Auto-exiting audio-only demo" << std::endl;
                break;
            }
        }
#endif
        
        cleanup();
    }
    
private:
    void initializeBrain() {
        std::cout << "Initializing neural substrate..." << std::endl;
        
        // Create connectivity manager
        connectivity_manager_ = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
        
        // Create brain with 100 Hz processing frequency
        brain_ = std::make_unique<HypergraphBrain>(connectivity_manager_, 100.0f);
        
        if (!brain_->initialize()) {
            std::cerr << "Warning: Failed to initialize HypergraphBrain" << std::endl;
            return;
        }
        
        // Create Social region for social perception processing
        auto social_region = brain_->createRegion("SocialPerception", 
                                                 Region::Type::Cortical, 
                                                 Region::ActivationPattern::Synchronous);
        if (social_region) {
            social_region_id_ = social_region->getId();
            
            // Map Social modality to this region
            brain_->mapModality(NeuroForge::Modality::Social, social_region_id_);
            
#ifdef NF_HAVE_OPENCV
            // Wire brain to social perception bias
            social_bias_.setBrain(brain_.get());
            social_bias_.setOutputGridSize(32);  // 32x32 grid for social features
#endif
            
            std::cout << "Social region created with ID: " << social_region_id_ << std::endl;
        } else {
            std::cerr << "Warning: Failed to create Social region" << std::endl;
        }
        
        std::cout << "Neural substrate initialization complete." << std::endl;
    }
    
    void initializeAudio() {
        std::cout << "Initializing real-time audio capture..." << std::endl;
        
        // Audio capture is already initialized in constructor
        if (audio_capture_.initialize()) {
            if (audio_capture_.startCapture()) {
                audio_enabled_ = true;
                std::cout << "Real-time audio capture started successfully!" << std::endl;
            } else {
                std::cout << "Failed to start audio capture - continuing without audio" << std::endl;
                audio_enabled_ = false;
            }
        } else {
            std::cout << "Failed to initialize audio capture - continuing without audio" << std::endl;
            audio_enabled_ = false;
        }
    }
    
#ifdef NF_HAVE_OPENCV
    SocialPerceptionBias::AudioBuffer getRealAudio() {
        if (!audio_enabled_ || !audio_capture_.isCapturing()) {
            return SocialPerceptionBias::AudioBuffer();
        }
        
        // Get latest audio data from microphone
        auto audio_data = audio_capture_.getLatestAudio(100);  // Max 100ms old
        
        if (audio_data.samples.empty()) {
            return SocialPerceptionBias::AudioBuffer();
        }
        
        // Convert to SocialPerceptionBias format
        SocialPerceptionBias::AudioBuffer social_audio;
        social_audio.audio_envelope = audio_data.envelope;
        social_audio.speech_probability = audio_data.speech_probability;
        social_audio.timestamp_ms = audio_data.timestamp_ms;
        
        // Extract phoneme features (simplified)
        social_audio.phoneme_features.resize(10);
        for (size_t i = 0; i < social_audio.phoneme_features.size() && i < audio_data.envelope.size(); ++i) {
            social_audio.phoneme_features[i] = audio_data.envelope[i];
        }
        
        return social_audio;
    }
#endif
    
    void updateFPS() {
        frame_count_++;
        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time - last_frame_time_).count();
        
        if (elapsed >= 1000) {  // Update FPS every second
            fps_ = frame_count_ * 1000.0 / elapsed;
            frame_count_ = 0;
            last_frame_time_ = current_time;
        }
    }
    
#ifdef NF_HAVE_OPENCV
    void visualizeSocialEvents(cv::Mat& frame, const std::vector<SocialPerceptionBias::SocialEvent>& events) {
        try {
            for (const auto& event : events) {
                // NEW: Draw face contour mask instead of bounding box
                if (show_contours_ && !event.face_mask.empty() && !event.face_contour.empty()) {
                    // Draw face contour for biological realism
                    cv::Scalar face_color = FACE_COLOR;
                    if (event.is_speaking) {
                        face_color = SPEAKING_COLOR;  // Red if speaking
                    }
                    
                    // Draw face contour points
                    std::vector<std::vector<cv::Point>> contours = {event.face_contour};
                    cv::drawContours(frame, contours, -1, face_color, 2);
                    
                    // Optionally overlay the face mask with transparency
                     if (show_masks_ && event.face_mask.size() == cv::Size(event.face_box.width, event.face_box.height)) {
                        cv::Mat mask_overlay;
                        cv::cvtColor(event.face_mask, mask_overlay, cv::COLOR_GRAY2BGR);
                        cv::Mat face_roi = frame(event.face_box);
                        cv::addWeighted(face_roi, 0.7, mask_overlay, 0.3, 0, face_roi);
                    }
                    
                    // Add face tracking ID
                    if (event.tracking_id >= 0 && !event.face_contour.empty()) {
                        std::string id_text = "ID:" + std::to_string(event.tracking_id);
                        cv::Point text_pos = event.face_contour[0];  // Use first contour point
                        text_pos.y -= 10;
                        if (text_pos.y > 0) {
                            cv::putText(frame, id_text, text_pos,
                                       cv::FONT_HERSHEY_SIMPLEX, 0.5, face_color, 1);
                        }
                    }
                }
                // Fallback: Draw legacy face bounding box if no mask available
                else if (!event.face_box.empty() && 
                    event.face_box.x >= 0 && event.face_box.y >= 0 &&
                    event.face_box.x + event.face_box.width <= frame.cols &&
                    event.face_box.y + event.face_box.height <= frame.rows) {
                    
                    cv::Scalar face_color = FACE_COLOR;
                    if (event.is_speaking) {
                        face_color = SPEAKING_COLOR;
                    }
                    
                    cv::rectangle(frame, event.face_box, face_color, 2);
                    
                    // Add face tracking ID
                    if (event.tracking_id >= 0) {
                        std::string id_text = "ID:" + std::to_string(event.tracking_id);
                        cv::Point text_pos(event.face_box.x, event.face_box.y - 10);
                        if (text_pos.y > 0) {
                            cv::putText(frame, id_text, text_pos,
                                       cv::FONT_HERSHEY_SIMPLEX, 0.5, face_color, 1);
                        }
                    }
                }
                
                // NEW: Draw vectorized gaze arrows instead of target boxes
                if (show_gaze_tracking_ && show_vectors_ && event.gaze_confidence > 0.3f) {
                    // Draw gaze vector arrow from face center
                    if (!event.face_box.empty() && 
                        (event.gaze_vector.x != 0 || event.gaze_vector.y != 0)) {
                        
                        cv::Point face_center(event.face_box.x + event.face_box.width / 2,
                                             event.face_box.y + event.face_box.height / 2);
                        
                        // Calculate arrow end point using gaze vector
                        float arrow_length = 100.0f * event.gaze_confidence;  // Scale by confidence
                        cv::Point arrow_end(
                            face_center.x + static_cast<int>(event.gaze_vector.x * arrow_length),
                            face_center.y + static_cast<int>(event.gaze_vector.y * arrow_length)
                        );
                        
                        // Ensure arrow end is within frame
                        arrow_end.x = (std::max)(0, (std::min)(arrow_end.x, frame.cols - 1));
                        arrow_end.y = (std::max)(0, (std::min)(arrow_end.y, frame.rows - 1));
                        
                        // Draw vectorized gaze arrow
                        cv::arrowedLine(frame, face_center, arrow_end, GAZE_COLOR, 3, 8, 0, 0.3);
                        
                        // Draw pupil positions if available
                         for (int i = 0; i < 2; ++i) {
                             if (event.pupil_positions[i].x > 0 && event.pupil_positions[i].y > 0) {
                                 cv::Point pupil_global(
                                     static_cast<int>(event.pupil_positions[i].x),
                                     static_cast<int>(event.pupil_positions[i].y)
                                 );
                                 if (pupil_global.x >= 0 && pupil_global.y >= 0 && 
                                     pupil_global.x < frame.cols && pupil_global.y < frame.rows) {
                                     cv::circle(frame, pupil_global, 3, PUPIL_COLOR, -1);  // White pupil dots
                                 }
                             }
                         }
                        
                        // Add gaze vector info
                        std::stringstream gaze_text;
                        gaze_text << "Gaze: (" << std::fixed << std::setprecision(2) 
                                 << event.gaze_vector.x << "," << event.gaze_vector.y 
                                 << ") C:" << event.gaze_confidence;
                        cv::Point gaze_text_pos(face_center.x + 20, face_center.y - 20);
                        if (gaze_text_pos.y > 0 && gaze_text_pos.x < frame.cols - 100) {
                            cv::putText(frame, gaze_text.str(), gaze_text_pos,
                                       cv::FONT_HERSHEY_SIMPLEX, 0.4, GAZE_COLOR, 1);
                        }
                    }
                    // Fallback: Draw legacy gaze target box
                    else if (!event.gaze_target_box.empty() && 
                        event.gaze_target_box.x >= 0 && event.gaze_target_box.y >= 0 &&
                        event.gaze_target_box.x + event.gaze_target_box.width <= frame.cols &&
                        event.gaze_target_box.y + event.gaze_target_box.height <= frame.rows) {
                        
                        cv::rectangle(frame, event.gaze_target_box, GAZE_COLOR, 2);
                        
                        // Draw gaze line from face center to gaze target
                        if (!event.face_box.empty()) {
                            cv::Point face_center(event.face_box.x + event.face_box.width / 2,
                                                 event.face_box.y + event.face_box.height / 2);
                            cv::Point gaze_center(event.gaze_target_box.x + event.gaze_target_box.width / 2,
                                                 event.gaze_target_box.y + event.gaze_target_box.height / 2);
                            
                            if (face_center.x >= 0 && face_center.y >= 0 && 
                                face_center.x < frame.cols && face_center.y < frame.rows &&
                                gaze_center.x >= 0 && gaze_center.y >= 0 && 
                                gaze_center.x < frame.cols && gaze_center.y < frame.rows) {
                                cv::arrowedLine(frame, face_center, gaze_center, GAZE_COLOR, 2);
                            }
                        }
                    }
                }
                
                // NEW: Draw precise mouth mask instead of bounding box
                if (show_lip_sync_ && show_masks_ && !event.mouth_mask.empty()) {
                    cv::Scalar mouth_color = MOUTH_COLOR;
                    if (event.is_speaking) {
                        mouth_color = SPEAKING_COLOR;
                    }
                    
                    // Overlay mouth mask with transparency
                    if (!event.mouth_region.empty() && 
                        event.mouth_mask.size() == cv::Size(event.mouth_region.width, event.mouth_region.height)) {
                        cv::Mat mask_overlay;
                        cv::cvtColor(event.mouth_mask, mask_overlay, cv::COLOR_GRAY2BGR);
                        cv::Mat mouth_roi = frame(event.mouth_region);
                        cv::addWeighted(mouth_roi, 0.6, mask_overlay, 0.4, 0, mouth_roi);
                        
                        // Draw mouth region outline
                        cv::rectangle(frame, event.mouth_region, mouth_color, 1);
                    }
                    
                    if (event.lip_sync_confidence > 0.5f) {
                        std::stringstream lip_text;
                        lip_text << "Lip: " << std::fixed << std::setprecision(2) << event.lip_sync_confidence;
                        cv::Point lip_text_pos(event.mouth_region.x, event.mouth_region.y - 5);
                        if (lip_text_pos.y > 0) {
                            cv::putText(frame, lip_text.str(), lip_text_pos,
                                       cv::FONT_HERSHEY_SIMPLEX, 0.3, mouth_color, 1);
                        }
                    }
                }
                // Fallback: Draw legacy mouth bounding box
                else if (show_lip_sync_ && !event.mouth_region.empty() &&
                    event.mouth_region.x >= 0 && event.mouth_region.y >= 0 &&
                    event.mouth_region.x + event.mouth_region.width <= frame.cols &&
                    event.mouth_region.y + event.mouth_region.height <= frame.rows) {
                    
                    cv::Scalar mouth_color = MOUTH_COLOR;
                    if (event.is_speaking) {
                        mouth_color = SPEAKING_COLOR;
                    }
                    
                    cv::rectangle(frame, event.mouth_region, mouth_color, 1);
                    
                    if (event.lip_sync_confidence > 0.5f) {
                        std::stringstream lip_text;
                        lip_text << "Lip: " << std::fixed << std::setprecision(2) << event.lip_sync_confidence;
                        cv::Point lip_text_pos(event.mouth_region.x, event.mouth_region.y - 5);
                        if (lip_text_pos.y > 0) {
                            cv::putText(frame, lip_text.str(), lip_text_pos,
                                       cv::FONT_HERSHEY_SIMPLEX, 0.3, mouth_color, 1);
                        }
                    }
                }
                
                // Draw eye contours if available
                if (show_contours_) {
                    for (int i = 0; i < 2; ++i) {
                        if (!event.eye_contours[i].empty()) {
                            std::vector<std::vector<cv::Point>> eye_contours = {event.eye_contours[i]};
                            cv::drawContours(frame, eye_contours, -1, CONTOUR_COLOR, 1);  // Cyan eye contours
                        }
                    }
                }
                
                // Add enhanced salience boost indicator
                if (event.total_salience_boost > 1.5f && !event.face_box.empty()) {
                    cv::Point indicator_pos(event.face_box.x + event.face_box.width - 10, 
                                           event.face_box.y + 10);
                    if (indicator_pos.x >= 5 && indicator_pos.y >= 5 && 
                        indicator_pos.x < frame.cols - 5 && indicator_pos.y < frame.rows - 5) {
                        cv::circle(frame, indicator_pos, 5, cv::Scalar(255, 255, 0), -1);  // Cyan circle for high salience
                        
                        // Add attention strength indicator
                        std::stringstream attention_text;
                        attention_text << "A:" << std::fixed << std::setprecision(1) << event.attention_strength;
                        cv::Point attention_text_pos(indicator_pos.x - 30, indicator_pos.y + 15);
                        if (attention_text_pos.y < frame.rows - 5) {
                            cv::putText(frame, attention_text.str(), attention_text_pos,
                                       cv::FONT_HERSHEY_SIMPLEX, 0.3, cv::Scalar(255, 255, 0), 1);
                        }
                    }
                }
            }
        } catch (const cv::Exception& e) {
            std::cerr << "OpenCV visualization error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Visualization error: " << e.what() << std::endl;
        }
    }
#else
    void visualizeSocialEvents(int dummy_param, int dummy_param2) {
        // OpenCV not available - no visualization
    }
#endif
    
#ifdef NF_HAVE_OPENCV
    void addDebugInfo(cv::Mat& frame, const std::vector<SocialPerceptionBias::SocialEvent>& events) {
        try {
            int y_offset = 30;
            const int line_height = 25;
            
            // FPS
            std::stringstream fps_text;
            fps_text << "FPS: " << std::fixed << std::setprecision(1) << fps_;
            cv::putText(frame, fps_text.str(), cv::Point(10, y_offset), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
            y_offset += line_height;
            
            // Social events count
            std::stringstream events_text;
            events_text << "Social Events: " << events.size();
            cv::putText(frame, events_text.str(), cv::Point(10, y_offset), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
            y_offset += line_height;
            
            // Statistics
            auto stats = social_bias_.getStatistics();
            std::stringstream stats_text;
            stats_text << "Faces: " << stats.faces_detected 
                       << " | Gaze: " << stats.gaze_events_detected
                       << " | Lip: " << stats.lip_sync_events_detected;
            cv::putText(frame, stats_text.str(), cv::Point(10, y_offset), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
            y_offset += line_height;
            
            // Audio status
            std::string audio_status = audio_enabled_ ? "Audio: ON" : "Audio: OFF";
            cv::putText(frame, audio_status, cv::Point(10, y_offset), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1);
            y_offset += line_height;
            
            // System status
            std::string system_status = social_bias_.isOperational() ? "System: OPERATIONAL" : "System: LIMITED";
            cv::Scalar status_color = social_bias_.isOperational() ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 165, 255);
            cv::putText(frame, system_status, cv::Point(10, y_offset), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.5, status_color, 1);
            
            // Legend - ensure it fits within frame bounds
            int legend_x = frame.cols - 200;
            int legend_y = 30;
            
            if (legend_x > 0 && legend_y > 0) {
                cv::putText(frame, "Legend:", cv::Point(legend_x, legend_y), 
                           cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 255, 255), 1);
                legend_y += 20;
                
                if (legend_y < frame.rows - 60) {  // Ensure legend fits
                    cv::rectangle(frame, cv::Rect(legend_x, legend_y - 10, 15, 10), FACE_COLOR, -1);
                    cv::putText(frame, "Face", cv::Point(legend_x + 20, legend_y), 
                               cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
                    legend_y += 15;
                    
                    cv::rectangle(frame, cv::Rect(legend_x, legend_y - 10, 15, 10), GAZE_COLOR, -1);
                    cv::putText(frame, "Gaze", cv::Point(legend_x + 20, legend_y), 
                               cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
                    legend_y += 15;
                    
                    cv::rectangle(frame, cv::Rect(legend_x, legend_y - 10, 15, 10), SPEAKING_COLOR, -1);
                    cv::putText(frame, "Speaking", cv::Point(legend_x + 20, legend_y), 
                               cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
                }
            }
        } catch (const cv::Exception& e) {
            std::cerr << "OpenCV debug info error: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Debug info error: " << e.what() << std::endl;
        }
    }
#endif
    
    bool handleKeyPress(int key) {
        switch (key) {
            case 27:  // ESC
            case 'q':
            case 'Q':
                return false;  // Exit
                
            case 'd':
            case 'D':
                show_debug_info_ = !show_debug_info_;
                std::cout << "Debug info: " << (show_debug_info_ ? "ON" : "OFF") << std::endl;
                break;
                
            case 'g':
            case 'G':
                show_gaze_tracking_ = !show_gaze_tracking_;
                std::cout << "Gaze tracking visualization: " << (show_gaze_tracking_ ? "ON" : "OFF") << std::endl;
                break;
                
            case 'l':
            case 'L':
                show_lip_sync_ = !show_lip_sync_;
                std::cout << "Lip-sync visualization: " << (show_lip_sync_ ? "ON" : "OFF") << std::endl;
                break;
                
            case 'a':
            case 'A':
                audio_enabled_ = !audio_enabled_;
                std::cout << "Audio processing: " << (audio_enabled_ ? "ON" : "OFF") << std::endl;
                break;
                
            case 'r':
            case 'R':
#ifdef NF_HAVE_OPENCV
                social_bias_.clear();
#endif
                std::cout << "Statistics reset" << std::endl;
                break;
                
            // NEW: Enhanced visualization controls
            case 'm':
            case 'M':
#ifdef NF_HAVE_OPENCV
                show_masks_ = !show_masks_;
                std::cout << "Face/Mouth masks: " << (show_masks_ ? "ON" : "OFF") << std::endl;
#endif
                break;
                
            case 'c':
            case 'C':
#ifdef NF_HAVE_OPENCV
                show_contours_ = !show_contours_;
                std::cout << "Face/Eye contours: " << (show_contours_ ? "ON" : "OFF") << std::endl;
#endif
                break;
                
            case 'v':
            case 'V':
#ifdef NF_HAVE_OPENCV
                show_vectors_ = !show_vectors_;
                std::cout << "Gaze vectors: " << (show_vectors_ ? "ON" : "OFF") << std::endl;
#endif
                break;
                
            case 'h':
            case 'H':
                std::cout << "\n=== Social Perception Demo Controls ===" << std::endl;
                std::cout << "ESC/Q: Exit" << std::endl;
                std::cout << "D: Toggle debug info" << std::endl;
                std::cout << "G: Toggle gaze tracking" << std::endl;
                std::cout << "L: Toggle lip-sync visualization" << std::endl;
                std::cout << "A: Toggle audio processing" << std::endl;
                std::cout << "R: Reset statistics" << std::endl;
                std::cout << "M: Toggle face/mouth masks (NEW)" << std::endl;
                std::cout << "C: Toggle face/eye contours (NEW)" << std::endl;
                std::cout << "V: Toggle gaze vectors (NEW)" << std::endl;
                std::cout << "H: Show this help" << std::endl;
                std::cout << "======================================\n" << std::endl;
                break;
                
            default:
                break;
        }
        
        return true;  // Continue running
    }
    
    void cleanup() {
        std::cout << "\nCleaning up..." << std::endl;
        
        // Stop audio capture
        if (audio_capture_.isCapturing()) {
            audio_capture_.stopCapture();
        }
        
#ifdef NF_HAVE_OPENCV
        if (camera_.isOpened()) {
            camera_.release();
        }
        
        cv::destroyAllWindows();
        
        // Print final statistics
        auto final_stats = social_bias_.getStatistics();
        std::cout << "\n=== Final Statistics ===" << std::endl;
        std::cout << "Total frames processed: " << final_stats.total_frames_processed << std::endl;
        std::cout << "Faces detected: " << final_stats.faces_detected << std::endl;
        std::cout << "Gaze events: " << final_stats.gaze_events_detected << std::endl;
        std::cout << "Lip-sync events: " << final_stats.lip_sync_events_detected << std::endl;
        std::cout << "Social events created: " << final_stats.social_events_created << std::endl;
        std::cout << "Average face confidence: " << final_stats.average_face_confidence << std::endl;
        std::cout << "Average gaze confidence: " << final_stats.average_gaze_confidence << std::endl;
        std::cout << "Average lip-sync confidence: " << final_stats.average_lip_sync_confidence << std::endl;
#endif
        std::cout << "=========================" << std::endl;
        
        std::cout << "Social Perception Demo completed successfully!" << std::endl;
    }
};

int main() {
#ifdef NF_HAVE_OPENCV
    std::cout << "NeuroForge Social Perception Real-Time Demo" << std::endl;
    std::cout << "===========================================" << std::endl;
    
    try {
        SocialPerceptionDemo demo;
        demo.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
#else
    std::cout << "Social Perception Demo disabled - OpenCV not available" << std::endl;
    return 0;
#endif
}