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

using namespace NeuroForge::Biases;
using namespace NeuroForge::Audio;
using namespace NeuroForge::Core;

class SimpleSocialDemo {
private:
#ifdef NF_HAVE_OPENCV
    SocialPerceptionBias social_bias_;
#endif
    AudioCapture audio_capture_;
#ifdef NF_HAVE_OPENCV
    cv::VideoCapture camera_;
#endif
    bool running_;
    
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
    
public:
    SimpleSocialDemo() 
        : 
#ifdef NF_HAVE_OPENCV
          social_bias_(SocialPerceptionBias::Config{}), 
#endif
          audio_capture_(AudioCapture::Config{}),
          running_(false), fps_(0.0), frame_count_(0), audio_enabled_(false),
          social_region_id_(0) {
        
        last_frame_time_ = std::chrono::steady_clock::now();
        
        // Initialize neural substrate
        initializeBrain();
    }
    
    bool initialize() {
        std::cout << "Initializing Simple Social Perception Demo..." << std::endl;
        
#ifdef NF_HAVE_OPENCV
        // Initialize social perception system
        if (!social_bias_.initialize()) {
            std::cout << "Warning: Some cascade classifiers failed to load. Demo will continue with available features." << std::endl;
        }
#endif
        
#ifdef NF_HAVE_OPENCV
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
        std::cout << "Running in audio-only mode (OpenCV not available)" << std::endl;
#endif
        
        // Initialize audio
        initializeAudio();
        
        std::cout << "Simple Social Perception Demo initialized successfully!" << std::endl;
        return true;
    }
    
    void run() {
        if (!initialize()) {
            return;
        }
        
        running_ = true;
        
        std::cout << "\n=== Simple Social Perception Demo ===" << std::endl;
        std::cout << "ESC/Q: Quit" << std::endl;
        std::cout << "SPACE: Print current statistics" << std::endl;
        std::cout << "====================================\n" << std::endl;
        
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
#ifdef NF_HAVE_OPENCV
            SocialPerceptionBias::AudioBuffer audio = getRealAudio();
            
            // Process frame for social perception
            auto social_events = social_bias_.processSocialFrame(frame, audio);
#endif
            
            // Simple visualization - just show the frame with basic info
            cv::Mat display_frame = frame.clone();
            
            // Add simple text overlay
            std::stringstream info_text;
            info_text << "FPS: " << std::fixed << std::setprecision(1) << fps_ 
#ifdef NF_HAVE_OPENCV
                     << " | Events: " << social_events.size()
#endif
                     << " | Audio: " << (audio_enabled_ ? "ON" : "OFF");
            
            cv::putText(display_frame, info_text.str(), cv::Point(10, 30), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
            
            // Draw simple rectangles for detected faces (if any)
#ifdef NF_HAVE_OPENCV
            for (const auto& event : social_events) {
                if (!event.face_box.empty()) {
                    cv::rectangle(display_frame, event.face_box, cv::Scalar(0, 255, 0), 2);
                    
                    // Add simple text label
                    std::string label = "Face ID:" + std::to_string(event.tracking_id);
                    cv::putText(display_frame, label, 
                               cv::Point(event.face_box.x, event.face_box.y - 10),
                               cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
                }
            }
#endif
            
            // Display frame
            cv::imshow("NeuroForge Simple Social Perception Demo", display_frame);
            
            // Handle keyboard input
            int key = cv::waitKey(1) & 0xFF;
            if (key == 27 || key == 'q' || key == 'Q') {  // ESC or Q
                break;
            } else if (key == ' ') {  // SPACE
                printStatistics();
            }
        }
#else
        // Audio-only mode when OpenCV is not available
        std::cout << "Running in audio-only mode (OpenCV not available)" << std::endl;
        
        while (running_) {
            // Update FPS (for timing purposes)
            updateFPS();
            
#ifdef NF_HAVE_OPENCV
            // Get real audio data from microphone
            SocialPerceptionBias::AudioBuffer audio = getRealAudio();
            
            // Process audio-only social perception
            auto social_events = social_bias_.processAudioOnly(audio);
            
            // Print statistics periodically
            if (frame_count_ % 30 == 0) {  // Every ~1 second at 30 FPS
                std::cout << "Audio Events: " << social_events.size() 
                         << " | Audio: " << (audio_enabled_ ? "ON" : "OFF") << std::endl;
            }
#endif
            
            // Simple timing control
            std::this_thread::sleep_for(std::chrono::milliseconds(33));  // ~30 FPS
            
            // Check for exit (simplified - just check periodically)
            if (frame_count_ % 10 == 0) {
                std::cout << "Press Ctrl+C to exit..." << std::endl;
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
            
            // Wire brain to social perception bias
#ifdef NF_HAVE_OPENCV
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
    
    void printStatistics() {
#ifdef NF_HAVE_OPENCV
        auto stats = social_bias_.getStatistics();
        std::cout << "\n=== Current Statistics ===" << std::endl;
        std::cout << "Total frames processed: " << stats.total_frames_processed << std::endl;
        std::cout << "Faces detected: " << stats.faces_detected << std::endl;
        std::cout << "Gaze events: " << stats.gaze_events_detected << std::endl;
        std::cout << "Lip-sync events: " << stats.lip_sync_events_detected << std::endl;
        std::cout << "Social events created: " << stats.social_events_created << std::endl;
        std::cout << "Current FPS: " << std::fixed << std::setprecision(1) << fps_ << std::endl;
        std::cout << "Audio queue size: " << audio_capture_.getQueueSize() << std::endl;
        std::cout << "System operational: " << (social_bias_.isOperational() ? "YES" : "LIMITED") << std::endl;
        std::cout << "=========================" << std::endl;
#else
        std::cout << "\n=== Current Statistics ===" << std::endl;
        std::cout << "Current FPS: " << std::fixed << std::setprecision(1) << fps_ << std::endl;
        std::cout << "Audio queue size: " << audio_capture_.getQueueSize() << std::endl;
        std::cout << "System operational: LIMITED (OpenCV not available)" << std::endl;
        std::cout << "=========================" << std::endl;
#endif
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
#endif
        
        // Print final statistics
        printStatistics();
        
        std::cout << "Simple Social Perception Demo completed successfully!" << std::endl;
    }
};

int main() {
#ifdef NF_HAVE_OPENCV
    std::cout << "NeuroForge Simple Social Perception Real-Time Demo" << std::endl;
    std::cout << "=================================================" << std::endl;
    
    try {
        SimpleSocialDemo demo;
        demo.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
#else
    std::cout << "Simple Social Demo disabled - OpenCV not available" << std::endl;
    return 0;
#endif
}