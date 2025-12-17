# Speech Production and Multimodal Output in NeuroForge

## Overview

The NeuroForge LanguageSystem now supports comprehensive **speech production and multimodal output**, completing the perception ↔ action cycle for human-like language learning. This system implements biologically-inspired speech generation with synchronized lip movements, gaze coordination, and self-monitoring feedback loops.

## Key Features

### 🔊 Speech Production Pipeline
- **Text → Phoneme conversion** with acoustic feature mapping
- **Prosody contour generation** with emotional coloring
- **Temporal timing patterns** for natural speech rhythm
- **Real-time production control** with start/stop/update functionality

### 👄 Lip-Sync Coordination
- **Phoneme → lip shape mapping** with 16-dimensional lip features
- **Synchronized lip motion sequences** aligned with speech timing
- **Articulatory precision control** with configurable accuracy
- **Visual-speech coupling** for natural multimodal output

### 👁️ Gaze Coordination
- **Speech-synchronized gaze targeting** toward listeners
- **Joint attention coordination** during object naming
- **Attention-driven gaze shifts** based on speech content
- **Caregiver-focused gaze** during mimicry attempts

### 🔄 Self-Monitoring and Feedback
- **Acoustic self-monitoring** comparing intended vs. actual speech
- **Caregiver response processing** for mimicry reinforcement
- **Speech quality assessment** with adaptive improvement
- **Joint attention learning** through shared gaze events

## Architecture

### Core Data Structures

```cpp
struct SpeechProductionFeatures {
    std::vector<PhonemeCluster> phoneme_sequence;  // Phonemes to produce
    std::vector<float> timing_pattern;             // Temporal timing per phoneme
    std::vector<float> prosody_contour;            // Pitch/stress pattern
    float speech_rate = 1.0f;                     // Speaking rate multiplier
    float emotional_coloring = 0.0f;              // Emotional expression level
    
    // Visual synchronization
    std::vector<std::vector<float>> lip_motion_sequence; // Lip shapes per phoneme
    std::vector<float> gaze_targets;               // Gaze direction during speech
    float facial_expression_intensity = 0.0f;     // Expression strength
    
    // Feedback and monitoring
    float confidence_score = 0.0f;                // Production confidence
    bool requires_feedback = true;                // Whether to monitor output
};

struct SpeechOutputState {
    bool is_speaking = false;                      // Currently producing speech
    std::size_t current_phoneme_index = 0;        // Current position in sequence
    float current_time_offset = 0.0f;             // Time within current phoneme
    std::vector<float> current_lip_shape;         // Current lip configuration
    std::vector<float> current_gaze_direction;    // Current gaze target
    
    // Feedback monitoring
    std::vector<float> acoustic_feedback;         // Heard audio during production
    float self_monitoring_score = 0.0f;          // Self-assessment of quality
    bool caregiver_attention_detected = false;   // Listener attention status
};
```

### Configuration Parameters

```cpp
struct Config {
    // Speech production parameters
    float speech_production_rate = 1.0f;    // Default speaking rate
    float lip_sync_precision = 0.8f;        // Lip-speech synchronization accuracy
    float gaze_coordination_strength = 0.6f; // Gaze-speech coupling
    float self_monitoring_weight = 0.4f;    // Self-feedback importance
    float caregiver_mimicry_boost = 0.5f;   // Boost for caregiver imitation
    bool enable_speech_output = true;       // Enable speech production
    bool enable_lip_sync = true;            // Enable lip movement generation
    bool enable_gaze_coordination = true;   // Enable gaze-speech coupling
};
```

## Speech Production Pipeline

### 1. Text-to-Speech Generation

```cpp
// Generate complete speech output from text
auto speech_features = language_system.generateSpeechOutput("hello mama");

// Or from token sequence
std::vector<std::string> tokens = {"hello", "mama"};
auto speech_features = language_system.generateSpeechOutput(tokens);
```

### 2. Phoneme Sequence Generation

```cpp
// Convert text to phoneme sequence with acoustic features
auto phonemes = language_system.generatePhonemeSequence("mama");

// Each phoneme includes:
// - IPA-like symbol ("m", "a", "m", "a")
// - Acoustic profile (formants, voicing, pitch)
// - Vowel/consonant classification
// - Stability and variant information
```

### 3. Lip Motion Synchronization

```cpp
// Generate lip shapes synchronized with phonemes
auto lip_motions = language_system.generateLipMotionSequence(phonemes);

// Each lip shape is 16-dimensional:
// [mouth_opening, lip_width, lip_rounding, lip_closure, ...]
// - Vowels: wider mouth, varied rounding
// - Consonants: specific articulatory positions
// - Bilabials (m,p,b): closed lips
```

### 4. Prosody Contour Generation

```cpp
// Generate natural intonation patterns
auto prosody = language_system.generateProsodyContour(phonemes, 0.5f); // 50% emotional

// Features:
// - Rising intonation at utterance end
// - Natural declination over time
// - Emotional modulation based on intensity
// - Phoneme-specific pitch adjustments
```

## Real-Time Speech Control

### Starting Speech Production

```cpp
// Initialize speech production
language_system.startSpeechProduction(speech_features);

// System state changes:
// - is_speaking = true
// - current_phoneme_index = 0
// - current_lip_shape = first phoneme lip position
// - current_gaze_direction = toward listener
```

### Updating Speech Production

```cpp
// Update speech state (call every frame)
float delta_time = 0.016f; // 60 FPS
language_system.updateSpeechProduction(delta_time);

// Automatic progression:
// - Advances through phoneme sequence
// - Updates lip shapes and gaze targets
// - Monitors timing and synchronization
// - Stops automatically when complete
```

### Manual Control

```cpp
// Stop speech production early
language_system.stopSpeechProduction();

// Check current state
auto state = language_system.getCurrentSpeechState();
if (state.is_speaking) {
    std::cout << "Currently on phoneme " << state.current_phoneme_index << std::endl;
    std::cout << "Lip shape: " << state.current_lip_shape[0] << std::endl;
}
```

## Self-Monitoring and Feedback

### Acoustic Self-Monitoring

```cpp
// Process heard audio during speech production
std::vector<float> heard_audio = microphone.captureAudio();
language_system.processSelfAcousticFeedback(heard_audio);

// System compares:
// - Intended phoneme acoustic features
// - Actual heard audio features
// - Updates self_monitoring_score
// - Adjusts future speech rate if quality is poor
```

### Caregiver Response Processing

```cpp
// Process caregiver reaction to speech
VisualLanguageFeatures caregiver_reaction;
caregiver_reaction.face_salience = 0.9f;      // Strong face detection
caregiver_reaction.gaze_alignment = 0.8f;     // Looking at system
caregiver_reaction.lip_sync_score = 0.7f;     // Lip movement correlation

AcousticFeatures caregiver_audio;
caregiver_audio.energy_envelope = 0.6f;       // Caregiver speaking
caregiver_audio.motherese_score = 0.8f;       // Infant-directed speech

language_system.processCaregiverResponse(caregiver_reaction, caregiver_audio);

// Results in:
// - Increased confidence for current speech
// - Enhanced self-monitoring score
// - Caregiver attention detection
```

### Speech Quality Assessment

```cpp
// Calculate production quality
float quality = language_system.calculateSpeechProductionQuality(
    intended_speech, actual_audio);

// Quality factors:
// - Acoustic similarity per phoneme
// - Temporal alignment accuracy
// - Prosodic contour matching
// - Overall production fidelity
```

## Caregiver Mimicry and Learning

### Mimicry Reinforcement

```cpp
// Reinforce successful caregiver mimicry
VisualLanguageFeatures caregiver_features;
caregiver_features.face_salience = 0.9f;
caregiver_features.lip_sync_score = 0.8f;
caregiver_features.motherese_face_boost = 0.6f;

language_system.reinforceCaregiverMimicry("mama", caregiver_features);

// Effects:
// - Boosts token activation strength
// - Increases usage count and recency
// - Creates visual-linguistic associations
// - Increments successful mimicry statistics
```

### Joint Attention Learning

```cpp
// Process joint attention events
std::vector<float> shared_gaze_target = {0.3f, 0.7f}; // Gaze coordinates
language_system.processJointAttentionEvent(shared_gaze_target, "ball");

// Creates strong learning signal:
// - Perfect gaze alignment (1.0)
// - High attention focus (0.9)
// - Strong speech-vision coupling (1.0)
// - Associates "ball" with gaze target coordinates
```

## Integration with External Systems

### Audio System Integration

```cpp
// Hook speech production to audio output
class AudioOutputSystem {
public:
    void speakPhonemes(const std::vector<PhonemeCluster>& phonemes) {
        for (const auto& phoneme : phonemes) {
            auto audio_snippet = language_system.generateAudioSnippet(phoneme, 200.0f);
            playAudio(audio_snippet);
        }
    }
    
    void playAudio(const std::vector<float>& audio_data) {
        // Send to speakers/audio device
        audio_device.output(audio_data);
    }
};
```

### Visual System Integration

```cpp
// Hook lip sync to visual animation
class VisualAnimationSystem {
public:
    void animateLips(const std::vector<std::vector<float>>& lip_sequence) {
        for (const auto& lip_shape : lip_sequence) {
            setLipConfiguration(lip_shape);
            renderFrame();
            waitForTiming();
        }
    }
    
    void setLipConfiguration(const std::vector<float>& lip_shape) {
        // Map 16D lip features to 3D mouth model
        mouth_model.setOpenness(lip_shape[0]);
        mouth_model.setWidth(lip_shape[1]);
        mouth_model.setRounding(lip_shape[2]);
        // ... additional lip parameters
    }
};
```

### Gaze Control Integration

```cpp
// Hook gaze coordination to face/eye control
class GazeControlSystem {
public:
    void coordinateGaze(const std::vector<float>& gaze_targets) {
        for (float gaze_target : gaze_targets) {
            setGazeDirection(gaze_target, 0.0f); // Toward listener
            maintainGazeForDuration(200.0f); // Per phoneme
        }
    }
    
    void alignGazeToListener() {
        // Direct gaze toward caregiver/listener
        setGazeDirection(0.0f, 0.0f); // Center gaze
    }
    
    void followJointAttention(const std::vector<float>& target) {
        // Shift gaze to shared attention target
        setGazeDirection(target[0], target[1]);
    }
};
```

## Complete Integration Example

```cpp
// Complete multimodal speech production system
class MultimodalSpeechSystem {
private:
    LanguageSystem language_system_;
    AudioOutputSystem audio_system_;
    VisualAnimationSystem visual_system_;
    GazeControlSystem gaze_system_;
    
public:
    void speakWithMultimodalOutput(const std::string& text) {
        // Generate speech features
        auto speech_features = language_system_.generateSpeechOutput(text);
        
        // Start coordinated output
        language_system_.startSpeechProduction(speech_features);
        
        // Launch parallel systems
        std::thread audio_thread([this, speech_features]() {
            audio_system_.speakPhonemes(speech_features.phoneme_sequence);
        });
        
        std::thread visual_thread([this, speech_features]() {
            visual_system_.animateLips(speech_features.lip_motion_sequence);
        });
        
        std::thread gaze_thread([this, speech_features]() {
            gaze_system_.coordinateGaze(speech_features.gaze_targets);
        });
        
        // Monitor and update
        while (language_system_.getCurrentSpeechState().is_speaking) {
            language_system_.updateSpeechProduction(0.016f); // 60 FPS
            
            // Process feedback if available
            auto heard_audio = microphone_.captureAudio();
            if (!heard_audio.empty()) {
                language_system_.processSelfAcousticFeedback(heard_audio);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
        
        // Wait for completion
        audio_thread.join();
        visual_thread.join();
        gaze_thread.join();
    }
    
    void respondToCaregiverInteraction() {
        // Detect caregiver speech and visual cues
        auto caregiver_visual = camera_.detectFaceFeatures();
        auto caregiver_audio = microphone_.captureAudio();
        
        if (!caregiver_audio.empty()) {
            auto acoustic_features = language_system_.extractAcousticFeatures(caregiver_audio);
            auto visual_features = convertToVisualLanguageFeatures(caregiver_visual);
            
            // Process caregiver response
            language_system_.processCaregiverResponse(visual_features, acoustic_features);
            
            // Generate appropriate response
            if (visual_features.motherese_face_boost > 0.5f) {
                // Caregiver using infant-directed speech - respond with mimicry
                speakWithMultimodalOutput("mama");
            }
        }
    }
};
```

## Testing and Validation

Run the comprehensive test suite:

```bash
# Compile and run speech production tests
g++ -std=c++17 -I../include src/test_speech_production.cpp \
    src/core/LanguageSystem.cpp src/core/LanguageSystem_Acoustic.cpp \
    src/core/LanguageSystem_Visual.cpp src/core/LanguageSystem_SpeechProduction.cpp \
    -o test_speech_production

./test_speech_production
```

Expected output:
```
=== NeuroForge Speech Production and Multimodal Output Tests ===

Test 1: Phoneme Sequence Generation... PASSED
Test 2: Lip Motion Generation... PASSED
Test 3: Prosody Contour Generation... PASSED
Test 4: Speech Production Features Generation... PASSED
Test 5: Speech Production Control... PASSED
Test 6: Self-Monitoring and Feedback... PASSED
Test 7: Caregiver Mimicry Reinforcement... PASSED
Test 8: Joint Attention Learning... PASSED

=== Test Results ===
Passed: 8/8 tests
Success Rate: 100.0%
🎉 All tests passed! Speech production system is working correctly.
✅ Phoneme sequence generation functional
✅ Lip-sync motion generation active
✅ Prosody contour generation working
✅ Speech production control operational
✅ Self-monitoring and feedback enabled
✅ Caregiver mimicry reinforcement active
✅ Joint attention learning functional
🚀 Ready for multimodal speech output integration!
```

## Performance Considerations

### Real-Time Requirements
- **60 FPS updates** for smooth lip-sync animation
- **Low-latency audio** for natural speech timing
- **Synchronized coordination** across modalities
- **Efficient phoneme processing** for real-time generation

### Memory Management
- **Speech production queue** limited to 5 utterances
- **Self-monitoring history** capped at 100 samples
- **Automatic cleanup** of completed speech features
- **Efficient lip shape storage** with 16D vectors

### Computational Efficiency
- **Parallel processing** for audio, visual, and gaze systems
- **Cached phoneme mappings** for repeated words
- **Optimized acoustic similarity** calculations
- **Streamlined prosody generation** algorithms

## Future Enhancements

### Planned Features
- **Emotional expression** integration with facial animation
- **Gesture coordination** synchronized with speech
- **Adaptive speech rate** based on listener comprehension
- **Multi-language phoneme** support and switching

### Research Directions
- **Developmental speech** progression modeling
- **Individual voice** characteristics and adaptation
- **Social context** awareness for appropriate speech style
- **Cross-cultural** speech pattern learning

This speech production system completes the perception ↔ action cycle, enabling NeuroForge to not just understand multimodal language input, but also produce coordinated speech output with synchronized lip movements, gaze coordination, and self-monitoring feedback - just like human infants learning to communicate! 🔊👄👁️🧠