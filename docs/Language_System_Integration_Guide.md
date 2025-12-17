# NeuroForge Language System Integration Guide

## Overview

This guide provides step-by-step instructions for integrating NeuroForge's revolutionary acoustic-first language learning system into your applications. The system includes acoustic processing, visual-linguistic integration, and speech production capabilities that enable human-like language acquisition and multimodal communication.

## Table of Contents

1. [System Requirements](#system-requirements)
2. [Quick Start Integration](#quick-start-integration)
3. [Acoustic-First Language Learning](#acoustic-first-language-learning)
4. [Visual-Linguistic Integration](#visual-linguistic-integration)
5. [Speech Production System](#speech-production-system)
6. [Configuration Options](#configuration-options)
7. [API Reference](#api-reference)
8. [Integration Examples](#integration-examples)
9. [Performance Optimization](#performance-optimization)
10. [Troubleshooting](#troubleshooting)

## System Requirements

### Hardware Requirements
- **CPU**: Multi-core processor (4+ cores recommended)
- **Memory**: 8GB RAM minimum, 16GB recommended for large-scale simulations
- **Audio**: Microphone and speakers for speech input/output
- **Camera**: Optional, for visual-linguistic integration features

### Software Dependencies
- **C++20 compatible compiler** (MSVC 2022, GCC 11+, Clang 13+)
- **CMake 3.20+** for build system
- **OpenCV 4.0+** (optional, for enhanced visual processing)
- **Audio drivers** compatible with Windows multimedia API

### Build Dependencies
```bash
# Windows (vcpkg)
vcpkg install opencv4 sqlite3 capnproto

# Linux (apt)
sudo apt install libopencv-dev libsqlite3-dev capnproto
```

## Quick Start Integration

### 1. Basic Setup

```cpp
#include "core/LanguageSystem.h"

// Configure the language system
LanguageSystem::Config config;
config.enable_acoustic_preprocessing = true;
config.enable_prosodic_embeddings = true;
config.enable_vision_grounding = true;
config.enable_face_language_bias = true;
config.enable_speech_output = true;

// Create and initialize
LanguageSystem language_system(config);
language_system.initialize();
```

### 2. Basic Language Learning

```cpp
// Enable acoustic-first babbling
language_system.performAcousticBabbling(5);

// Process teacher signal with acoustic features
std::vector<float> teacher_audio = captureAudio(); // Your audio capture
language_system.processAcousticTeacherSignal(teacher_audio, "mama", 1.0f);

// Generate speech output
auto speech_features = language_system.generateSpeechOutput("hello");
language_system.startSpeechProduction(speech_features);
```

### 3. Integration with Visual System

```cpp
// Process face-speech events
std::vector<float> face_embedding = detectFace(); // Your face detection
std::vector<float> gaze_vector = trackGaze();     // Your gaze tracking
std::vector<float> lip_features = detectLips();  // Your lip detection

language_system.processFaceSpeechEvent(
    face_embedding, gaze_vector, lip_features, "mama", 0.9f);
```

## Acoustic-First Language Learning

### Core Concepts

The acoustic-first approach processes raw audio before creating symbolic tokens, mimicking how infants learn language through sound patterns rather than predefined vocabularies.

### Implementation Steps

#### 1. Audio Capture Setup

```cpp
class AudioCapture {
public:
    std::vector<float> captureAudio(float duration_ms = 1000.0f) {
        // Capture audio at 16kHz sample rate
        const float sample_rate = 16000.0f;
        std::size_t num_samples = static_cast<std::size_t>(
            (duration_ms / 1000.0f) * sample_rate);
        
        std::vector<float> audio_data(num_samples);
        // Your audio capture implementation here
        return audio_data;
    }
};
```

#### 2. Acoustic Feature Processing

```cpp
// Extract acoustic features from raw audio
auto acoustic_features = language_system.extractAcousticFeatures(
    audio_samples, 16000.0f); // 16kHz sample rate

// Features include:
// - pitch_contour: Fundamental frequency trajectory
// - energy_envelope: Amplitude envelope
// - formant_f1, formant_f2: Vowel characteristics
// - voicing_strength: Harmonic content
// - spectral_centroid: Brightness
// - attention_score: Computed salience
```

#### 3. Prosodic Attention

```cpp
// Calculate sound salience for attention
float salience = language_system.calculateSoundSalience(acoustic_features);

// High salience triggers include:
// - Rising intonation (>0.5Hz change)
// - High-frequency vowels (F2 > 1500Hz)
// - Motherese features (high pitch + exaggerated intonation)
// - Energy and voicing patterns
```

#### 4. Phoneme Clustering

```cpp
// Generate phoneme clusters from acoustic features
auto phoneme_cluster = language_system.generatePhonemeCluster(acoustic_features);

// Cluster includes:
// - phonetic_symbol: IPA-like representation ("a", "m", "s")
// - acoustic_profile: Associated acoustic features
// - formant_pattern: F1/F2 frequency pattern
// - vowel_consonant_ratio: Classification score
// - stability_score: Cluster coherence
```

### Configuration Parameters

```cpp
config.prosody_attention_weight = 0.3f;    // Boost for prosodic salience
config.intonation_threshold = 0.5f;        // Hz change for attention trigger
config.motherese_boost = 0.4f;             // Amplification for infant-directed speech
config.formant_clustering_threshold = 50.0f; // Hz threshold for phoneme clustering
```

## Visual-Linguistic Integration

### Face-Speech Coupling

#### 1. Face Detection Integration

```cpp
class FaceDetector {
public:
    struct FaceFeatures {
        std::vector<float> embedding;    // 128-dim face features
        std::vector<float> gaze_vector;  // 2D gaze direction
        std::vector<float> lip_features; // 16-dim lip shape/movement
        float confidence;                // Detection confidence
    };
    
    FaceFeatures detectFace(const cv::Mat& frame) {
        FaceFeatures features;
        // Your face detection implementation
        return features;
    }
};
```

#### 2. Real-Time Face-Speech Processing

```cpp
void processRealTimeFaceSpeech() {
    FaceDetector face_detector;
    AudioCapture audio_capture;
    
    while (running) {
        // Capture synchronized audio and video
        auto audio_data = audio_capture.captureAudio(100.0f); // 100ms chunks
        auto face_features = face_detector.detectFace(camera_frame);
        
        // Extract spoken word (your speech recognition)
        std::string spoken_word = recognizeSpeech(audio_data);
        
        if (!spoken_word.empty() && face_features.confidence > 0.7f) {
            // Process face-speech event
            language_system.processFaceSpeechEvent(
                face_features.embedding,
                face_features.gaze_vector,
                face_features.lip_features,
                spoken_word,
                calculateTemporalAlignment(audio_data, face_features)
            );
        }
    }
}
```

#### 3. Visual Attention Integration

```cpp
// Process visual attention maps
std::vector<float> attention_map = computeVisualAttention(camera_frame);
std::vector<std::string> visible_objects = {"ball", "red", "toy"};

language_system.processVisualAttentionMap(attention_map, visible_objects);

// Boost tokens based on visual salience
for (const auto& object : visible_objects) {
    auto visual_pattern = extractObjectFeatures(object);
    float salience = calculateVisualSalience(visual_pattern);
    
    auto token_id = language_system.getTokenId(object);
    language_system.reinforceVisualGrounding(token_id, visual_pattern, salience);
}
```

### Configuration Parameters

```cpp
config.face_language_coupling = 0.6f;      // Face-speech binding strength
config.gaze_attention_weight = 0.4f;       // Gaze direction influence
config.lip_sync_threshold = 0.3f;          // Minimum lip-speech correlation
config.visual_grounding_boost = 0.5f;      // Visual modality reinforcement
config.cross_modal_decay = 0.01f;          // Association decay rate
```

### Phase A Projection & Token Grounding (New)

```cpp
// Integrate Phase A student embeddings into LanguageSystem tokens
// Assumes Phase A outputs 62‑dim vectors; LanguageSystem expects 256‑dim by default.

#include "core/PhaseAMimicry.h"
#include "core/LanguageSystem.h"

// Create systems and connect
auto language_system_ptr = std::make_shared<LanguageSystem>(config);
language_system_ptr->initialize();                        // Initialize language system
NeuroForge::Core::PhaseAMimicry::Config pacfg;            // Phase A config (defaults ok)
auto phase_a_system = std::make_unique<NeuroForge::Core::PhaseAMimicry>(
    language_system_ptr, /*memory_db*/ nullptr, pacfg);

// Example: take a Phase A student embedding and project it to LanguageSystem dimension
std::vector<float> student_phase_a = {/* 62‑dim student vector */};
std::vector<float> projected = phase_a_system->projectStudent(student_phase_a);
// Comment: projectStudent performs a learned linear map to the target token dimension,
// then L2‑normalizes for stable cosine similarity.

// Inject the projected embedding to ground a token (e.g., "dog")
auto dog_token_id = language_system_ptr->createToken(
    "dog", LanguageSystem::TokenType::Word, projected);
// Comment: createToken stores the embedding and makes it available for clustering/grounding.
```

Notes:
- Projection uses a linear layer keyed by input dimension and targets `LanguageSystem::Config.embedding_dimension`.
- Default `embedding_dimension` is 256; if already matching, Phase A vectors are only normalized.
- References: `src/core/PhaseAMimicry.cpp:1423`–`1454` (`projectStudent`), `src/core/PhaseAMimicry.cpp:486`–`497` (norm logging).

#### CLI Wiring
```text
--phase5-language=on|off         Enable LanguageSystem
--phase-a=on|off                 Enable Phase A Baby Mimicry
--substrate-mode=off|mirror|train|native  Connect Phase A to brain (native enables region plasticity)
```

Recommended:
- Enable `--hippocampal-snapshots=on` to emit `snapshot:phase_a` metrics for evaluation.
- Use `--phase-a-replay-*` flags to activate replay‑weighted learning and hard negatives for faster grounding.

## Speech Production System

### Multimodal Output Generation

#### 1. Speech Feature Generation

```cpp
// Generate complete speech output
auto speech_features = language_system.generateSpeechOutput("hello mama");

// Features include:
// - phoneme_sequence: IPA phonemes with acoustic profiles
// - timing_pattern: Duration for each phoneme
// - prosody_contour: Pitch/stress pattern
// - lip_motion_sequence: 16D lip shapes per phoneme
// - gaze_targets: Gaze direction during speech
```

#### 2. Real-Time Speech Production

```cpp
class SpeechProductionSystem {
private:
    AudioOutput audio_output_;
    LipAnimator lip_animator_;
    GazeController gaze_controller_;
    
public:
    void speakWithMultimodalOutput(const std::string& text) {
        // Generate speech features
        auto speech_features = language_system_.generateSpeechOutput(text);
        
        // Start coordinated production
        language_system_.startSpeechProduction(speech_features);
        
        // Launch parallel systems
        std::thread audio_thread([this, speech_features]() {
            produceAudio(speech_features);
        });
        
        std::thread visual_thread([this, speech_features]() {
            animateLips(speech_features.lip_motion_sequence);
            coordinateGaze(speech_features.gaze_targets);
        });
        
        // Monitor and update
        while (language_system_.getCurrentSpeechState().is_speaking) {
            language_system_.updateSpeechProduction(0.016f); // 60 FPS
            
            // Process self-monitoring feedback
            auto heard_audio = microphone_.captureAudio();
            if (!heard_audio.empty()) {
                language_system_.processSelfAcousticFeedback(heard_audio);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        
        audio_thread.join();
        visual_thread.join();
    }
};
```

#### 3. Self-Monitoring and Feedback

```cpp
// Process acoustic self-monitoring
void processSelfMonitoring() {
    if (language_system.getCurrentSpeechState().is_speaking) {
        // Capture what the system "hears" itself saying
        auto self_audio = microphone.captureAudio();
        language_system.processSelfAcousticFeedback(self_audio);
        
        // Get self-monitoring score
        auto speech_state = language_system.getCurrentSpeechState();
        float quality_score = speech_state.self_monitoring_score;
        
        // Adjust production based on quality
        if (quality_score < 0.6f) {
            // Poor quality - slow down for better articulation
            adjustSpeechRate(0.8f);
        }
    }
}
```

### Configuration Parameters

```cpp
config.speech_production_rate = 1.0f;      // Default speaking rate
config.lip_sync_precision = 0.8f;          // Lip-speech synchronization accuracy
config.gaze_coordination_strength = 0.6f;  // Gaze-speech coupling
config.self_monitoring_weight = 0.4f;      // Self-feedback importance
config.caregiver_mimicry_boost = 0.5f;     // Boost for caregiver imitation
```

## Configuration Options

### Complete Configuration Structure

```cpp
LanguageSystem::Config config;

// Developmental parameters
config.mimicry_learning_rate = 0.01f;
config.grounding_strength = 0.5f;
config.narration_threshold = 0.3f;

// Acoustic processing
config.prosody_attention_weight = 0.3f;
config.intonation_threshold = 0.5f;
config.motherese_boost = 0.4f;
config.formant_clustering_threshold = 50.0f;

// Visual-linguistic integration
config.face_language_coupling = 0.6f;
config.gaze_attention_weight = 0.4f;
config.lip_sync_threshold = 0.3f;
config.visual_grounding_boost = 0.5f;
config.cross_modal_decay = 0.01f;

// Speech production
config.speech_production_rate = 1.0f;
config.lip_sync_precision = 0.8f;
config.gaze_coordination_strength = 0.6f;
config.self_monitoring_weight = 0.4f;
config.caregiver_mimicry_boost = 0.5f;

// Feature toggles
config.enable_acoustic_preprocessing = true;
config.enable_prosodic_embeddings = true;
config.enable_sound_attention_bias = true;
config.enable_vision_grounding = true;
config.enable_face_language_bias = true;
config.enable_speech_output = true;
config.enable_lip_sync = true;
config.enable_gaze_coordination = true;

// Token management
config.max_vocabulary_size = 10000;
config.embedding_dimension = 256;
config.token_decay_rate = 0.001f;

// Developmental timing
config.babbling_duration = 1000;
config.mimicry_duration = 5000;
config.grounding_duration = 10000;
```

## API Reference

### Core Methods

#### Acoustic Processing
```cpp
// Extract acoustic features from audio
AcousticFeatures extractAcousticFeatures(
    const std::vector<float>& audio_samples, 
    float sample_rate = 16000.0f) const;

// Calculate sound salience for attention
float calculateSoundSalience(const AcousticFeatures& features) const;

// Generate phoneme cluster from acoustic features
PhonemeCluster generatePhonemeCluster(const AcousticFeatures& features) const;

// Perform acoustic-first babbling
void performAcousticBabbling(std::size_t num_phonemes = 5);

// Process acoustic teacher signal
void processAcousticTeacherSignal(
    const std::vector<float>& teacher_audio,
    const std::string& label, 
    float confidence = 1.0f);
```

#### Visual Integration
```cpp
// Associate token with visual features
void associateTokenWithVisualFeatures(
    std::size_t token_id,
    const VisualLanguageFeatures& visual_features,
    float confidence = 1.0f);

// Process face-speech event
void processFaceSpeechEvent(
    const std::vector<float>& face_embedding,
    const std::vector<float>& gaze_vector,
    const std::vector<float>& lip_features,
    const std::string& spoken_token,
    float temporal_alignment = 1.0f);

// Process visual attention map
void processVisualAttentionMap(
    const std::vector<float>& attention_map,
    const std::vector<std::string>& active_tokens);

// Reinforce visual grounding
void reinforceVisualGrounding(
    std::size_t token_id,
    const std::vector<float>& visual_pattern,
    float salience_score);
```

#### Speech Production
```cpp
// Generate speech output features
SpeechProductionFeatures generateSpeechOutput(const std::string& text) const;

// Start speech production
void startSpeechProduction(const SpeechProductionFeatures& speech_features);

// Update speech production (call every frame)
void updateSpeechProduction(float delta_time);

// Stop speech production
void stopSpeechProduction();

// Get current speech state
SpeechOutputState getCurrentSpeechState() const;

// Process self-acoustic feedback
void processSelfAcousticFeedback(const std::vector<float>& heard_audio);
```

## Integration Examples

### Example 1: Basic Acoustic Learning

```cpp
#include "core/LanguageSystem.h"

class BasicAcousticLearning {
private:
    LanguageSystem language_system_;
    
public:
    BasicAcousticLearning() {
        LanguageSystem::Config config;
        config.enable_acoustic_preprocessing = true;
        config.enable_prosodic_embeddings = true;
        
        language_system_ = LanguageSystem(config);
        language_system_.initialize();
    }
    
    void learnFromAudio(const std::vector<float>& audio, const std::string& label) {
        // Process acoustic teacher signal
        language_system_.processAcousticTeacherSignal(audio, label, 1.0f);
        
        // Perform some babbling to explore
        language_system_.performAcousticBabbling(3);
        
        // Update development
        language_system_.updateDevelopment(0.1f);
    }
    
    void generateSpeech(const std::string& text) {
        auto speech_features = language_system_.generateSpeechOutput(text);
        language_system_.startSpeechProduction(speech_features);
        
        // Monitor production
        while (language_system_.getCurrentSpeechState().is_speaking) {
            language_system_.updateSpeechProduction(0.016f);
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }
};
```

### Example 2: Face-Speech Integration

```cpp
class FaceSpeechIntegration {
private:
    LanguageSystem language_system_;
    cv::VideoCapture camera_;
    
public:
    void processVideoStream() {
        cv::Mat frame;
        while (camera_.read(frame)) {
            // Detect face features
            auto face_features = detectFaceFeatures(frame);
            
            // Capture audio
            auto audio_data = captureAudio(100.0f); // 100ms
            
            // Recognize speech
            std::string spoken_word = recognizeSpeech(audio_data);
            
            if (!spoken_word.empty() && face_features.confidence > 0.7f) {
                // Process face-speech coupling
                language_system_.processFaceSpeechEvent(
                    face_features.embedding,
                    face_features.gaze_vector,
                    face_features.lip_features,
                    spoken_word,
                    0.9f // High temporal alignment
                );
            }
        }
    }
    
private:
    struct FaceFeatures {
        std::vector<float> embedding;
        std::vector<float> gaze_vector;
        std::vector<float> lip_features;
        float confidence;
    };
    
    FaceFeatures detectFaceFeatures(const cv::Mat& frame) {
        // Your face detection implementation
        FaceFeatures features;
        // ... face detection code ...
        return features;
    }
};
```

### Example 3: Complete Multimodal System

```cpp
class MultimodalLanguageSystem {
private:
    LanguageSystem language_system_;
    AudioCapture audio_capture_;
    FaceDetector face_detector_;
    SpeechSynthesizer speech_synthesizer_;
    
public:
    MultimodalLanguageSystem() {
        LanguageSystem::Config config;
        config.enable_acoustic_preprocessing = true;
        config.enable_vision_grounding = true;
        config.enable_speech_output = true;
        config.enable_face_language_bias = true;
        
        language_system_ = LanguageSystem(config);
        language_system_.initialize();
    }
    
    void runInteractiveSession() {
        while (true) {
            // Listen for input
            auto audio_input = audio_capture_.captureAudio(2000.0f); // 2 seconds
            auto face_features = face_detector_.detectFace();
            
            if (!audio_input.empty()) {
                // Process acoustic input
                std::string recognized_word = recognizeSpeech(audio_input);
                
                if (!recognized_word.empty()) {
                    // Learn from acoustic-visual input
                    if (face_features.confidence > 0.6f) {
                        language_system_.processFaceSpeechEvent(
                            face_features.embedding,
                            face_features.gaze_vector,
                            face_features.lip_features,
                            recognized_word,
                            0.8f
                        );
                    } else {
                        language_system_.processAcousticTeacherSignal(
                            audio_input, recognized_word, 1.0f);
                    }
                    
                    // Generate response
                    std::string response = generateResponse(recognized_word);
                    speakResponse(response);
                }
            }
            
            // Perform developmental babbling
            if (shouldBabble()) {
                language_system_.performAcousticBabbling(2);
            }
            
            // Update development
            language_system_.updateDevelopment(0.1f);
        }
    }
    
private:
    void speakResponse(const std::string& text) {
        auto speech_features = language_system_.generateSpeechOutput(text);
        language_system_.startSpeechProduction(speech_features);
        
        // Coordinate multimodal output
        std::thread audio_thread([this, speech_features]() {
            speech_synthesizer_.synthesize(speech_features.phoneme_sequence);
        });
        
        std::thread visual_thread([this, speech_features]() {
            animateLips(speech_features.lip_motion_sequence);
            coordinateGaze(speech_features.gaze_targets);
        });
        
        // Monitor production
        while (language_system_.getCurrentSpeechState().is_speaking) {
            language_system_.updateSpeechProduction(0.016f);
            
            // Self-monitoring
            auto self_audio = audio_capture_.captureAudio(50.0f);
            if (!self_audio.empty()) {
                language_system_.processSelfAcousticFeedback(self_audio);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        
        audio_thread.join();
        visual_thread.join();
    }
};
```

### Example 4: Triplets Dataset Integration
```powershell
# Run the system in triplets ingestion mode to ground language with image/audio/text
& ".\neuroforge.exe" --phase7=on --phase9=on --substrate-mode=native ^
  --dataset-mode=triplets --dataset-triplets "C:\Data\flickr30k_triplets" ^
  --dataset-limit 1000 --dataset-shuffle=on --reward-scale 1.0 ^
  --memory-db phasec_mem.db --memdb-interval 500 --steps 3000 --log-json=on
```

```cpp
// Conceptual flow (ingestion handled by runtime)
// 1) Phase A registers teacher embeddings from image/audio/text triplets
// 2) Mimicry rewards are delivered to the brain (scaled by reward_scale)
// 3) LanguageSystem strengthens tokens that align with teacher content
// 4) MemoryDB logs rewards and learning_stats for validation
```

```powershell
# Validate grounding and learning via MemoryDB
python .\scripts\db_inspect.py --db .\phasec_mem.db --table reward_log --limit 5
python .\scripts\db_inspect.py --db .\phasec_mem.db --table learning_stats --limit 5
```

## Performance Optimization

### Memory Management
- **Token Limits**: Set appropriate `max_vocabulary_size` based on available memory
- **Association Cleanup**: Cross-modal associations automatically decay and prune
- **Buffer Sizes**: Acoustic and visual stream buffers are limited to prevent bloat

### Real-Time Performance
- **Threading**: Use separate threads for audio, visual, and speech production
- **Frame Rates**: Target 60 FPS for lip-sync, 30 FPS for visual processing
- **Chunk Sizes**: Process audio in 50-100ms chunks for responsiveness

### Optimization Settings
```cpp
// For real-time applications
config.embedding_dimension = 128;          // Smaller embeddings for speed
config.max_vocabulary_size = 5000;         // Limit vocabulary size
config.cross_modal_decay = 0.02f;          // Faster decay for memory efficiency

// For accuracy-focused applications
config.embedding_dimension = 512;          // Larger embeddings for precision
config.max_vocabulary_size = 20000;        // Larger vocabulary
config.cross_modal_decay = 0.005f;         // Slower decay for retention
```

## Troubleshooting

### Common Issues

#### 1. Audio Processing Issues
**Problem**: Poor acoustic feature extraction
**Solution**: 
- Ensure 16kHz sample rate
- Check audio quality and noise levels
- Verify microphone permissions

#### 2. Face Detection Problems
**Problem**: Low face detection confidence
**Solution**:
- Improve lighting conditions
- Ensure camera permissions
- Check OpenCV installation

#### 3. Speech Production Issues
**Problem**: Choppy or distorted speech output
**Solution**:
- Increase audio buffer size
- Check speaker/audio device settings
- Verify real-time thread priorities

#### 4. Memory Issues
**Problem**: High memory usage
**Solution**:
- Reduce `max_vocabulary_size`
- Increase `cross_modal_decay` rate
- Limit acoustic/visual buffer sizes

### Debug Information

```cpp
// Enable verbose logging
language_system.setVerboseLogging(true);

// Get system statistics
auto stats = language_system.getStatistics();
std::cout << "Vocabulary size: " << stats.active_vocabulary_size << std::endl;
std::cout << "Associations: " << stats.grounding_associations_formed << std::endl;

// Check speech production state
auto speech_state = language_system.getCurrentSpeechState();
if (speech_state.is_speaking) {
    std::cout << "Currently speaking phoneme " << speech_state.current_phoneme_index << std::endl;
    std::cout << "Self-monitoring score: " << speech_state.self_monitoring_score << std::endl;
}
```

### Performance Monitoring

```cpp
// Monitor processing times
auto start_time = std::chrono::high_resolution_clock::now();
language_system.processAcousticTeacherSignal(audio, "test", 1.0f);
auto end_time = std::chrono::high_resolution_clock::now();

auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
    end_time - start_time).count();
std::cout << "Processing time: " << duration << " microseconds" << std::endl;
```

## Version Compatibility

### Current Version: 2.0
- Full acoustic-first language learning
- Complete visual-linguistic integration
- Real-time speech production system
- Cross-modal association framework

### Migration from 1.x
- Update configuration structure
- Replace `performBabbling()` with `performAcousticBabbling()`
- Add visual integration components
- Update API calls for new method signatures

### Backward Compatibility
- Legacy token-based babbling still supported
- Original API methods maintained with deprecation warnings
- Gradual migration path available

---

**Last Updated**: December 2024  
**Version**: 2.0  
**Status**: Production Ready

For additional support, please refer to the [troubleshooting guide](Language_System_Troubleshooting.md) or contact the development team.
