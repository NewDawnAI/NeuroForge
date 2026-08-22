# NeuroForge Language System API Reference

## Overview

This document provides a complete API reference for NeuroForge's acoustic-first language learning system, including all classes, methods, parameters, and configuration options for the integrated acoustic processing, visual-linguistic integration, and speech production capabilities.

## Table of Contents

1. [Core Classes](#core-classes)
2. [Configuration Structures](#configuration-structures)
3. [Data Structures](#data-structures)
4. [Acoustic Processing API](#acoustic-processing-api)
5. [Visual Integration API](#visual-integration-api)
6. [Speech Production API](#speech-production-api)
7. [Core Language System API](#core-language-system-api)
8. [Utility Functions](#utility-functions)
9. [Error Handling](#error-handling)
10. [Version Information](#version-information)

## Core Classes

### LanguageSystem

**Header**: `include/core/LanguageSystem.h`  
**Implementation**: `src/core/LanguageSystem.cpp`, `src/core/LanguageSystem_Acoustic.cpp`, `src/core/LanguageSystem_Visual.cpp`, `src/core/LanguageSystem_SpeechProduction.cpp`

The main class for acoustic-first language learning with multimodal integration.

```cpp
class LanguageSystem {
public:
    // Constructor
    explicit LanguageSystem(const Config& config = Config{});
    
    // Destructor
    ~LanguageSystem() = default;
    
    // Core lifecycle methods
    bool initialize();
    void shutdown();
    void reset();
    void updateDevelopment(float delta_time);
};
```

## Configuration Structures

### LanguageSystem::Config

Complete configuration structure for the language system.

```cpp
struct Config {
    // Developmental parameters
    float mimicry_learning_rate = 0.01f;     ///< Rate of teacher imitation [0.0-1.0]
    float grounding_strength = 0.5f;         ///< Sensory-symbol association strength [0.0-1.0]
    float narration_threshold = 0.3f;        ///< Activation threshold for internal speech [0.0-1.0]
    
    // Acoustic processing parameters
    float prosody_attention_weight = 0.3f;   ///< Boost for prosodic salience [0.0-1.0]
    float intonation_threshold = 0.5f;      ///< Hz change for attention trigger [0.1-5.0]
    float motherese_boost = 0.4f;           ///< Amplification for infant-directed speech [0.0-1.0]
    float formant_clustering_threshold = 50.0f; ///< Hz threshold for phoneme clustering [10.0-200.0]
    
    // Visual-linguistic integration
    float face_language_coupling = 0.6f;    ///< Face-speech binding strength [0.0-1.0]
    float gaze_attention_weight = 0.4f;     ///< Gaze direction influence [0.0-1.0]
    float lip_sync_threshold = 0.3f;        ///< Minimum lip-speech correlation [0.0-1.0]
    float visual_grounding_boost = 0.5f;    ///< Visual modality reinforcement [0.0-1.0]
    float cross_modal_decay = 0.01f;        ///< Association decay rate [0.001-0.1]
    
    // Speech production parameters
    float speech_production_rate = 1.0f;    ///< Default speaking rate [0.5-2.0]
    float lip_sync_precision = 0.8f;        ///< Lip-speech synchronization accuracy [0.0-1.0]
    float gaze_coordination_strength = 0.6f; ///< Gaze-speech coupling [0.0-1.0]
    float self_monitoring_weight = 0.4f;    ///< Self-feedback importance [0.0-1.0]
    float caregiver_mimicry_boost = 0.5f;   ///< Boost for caregiver imitation [0.0-1.0]
    
    // Feature toggles
    bool enable_acoustic_preprocessing = true; ///< Process raw audio before tokenization
    bool enable_prosodic_embeddings = true;   ///< Include prosodic features in embeddings
    bool enable_sound_attention_bias = true;  ///< Boost salient acoustic patterns
    bool enable_vision_grounding = true;     ///< Link tokens to visual patterns
    bool enable_face_language_bias = true;   ///< Boost face-speech associations
    bool enable_speech_output = true;       ///< Enable speech production
    bool enable_lip_sync = true;            ///< Enable lip movement generation
    bool enable_gaze_coordination = true;   ///< Enable gaze-speech coupling
    
    // Token management
    std::size_t max_vocabulary_size = 10000; ///< Maximum number of tokens [1000-50000]
    std::size_t embedding_dimension = 256;   ///< Token embedding size [64-1024]
    float token_decay_rate = 0.001f;         ///< Unused token forgetting rate [0.0001-0.01]
    
    // Developmental timing
    std::uint64_t babbling_duration = 1000;  ///< Steps in babbling stage [100-10000]
    std::uint64_t mimicry_duration = 5000;   ///< Steps in mimicry stage [1000-20000]
    std::uint64_t grounding_duration = 10000; ///< Steps in grounding stage [5000-50000]
    
    // Teacher system
    bool enable_teacher_mode = false;        ///< Use external teacher signals
    float teacher_influence = 0.8f;          ///< Strength of teacher guidance [0.0-1.0]
};
```

### Token Management API

#### createToken()
```cpp
// Create or retrieve a token and optionally set its embedding.
std::size_t createToken(
    const std::string& symbol,
    TokenType type,
    const std::vector<float>& embedding = {});
```
**Description**: Creates a language token; when an `embedding` is provided, it is stored as the token’s representation.  
**Notes**:
- Embedding length should match `Config.embedding_dimension` (default 256).
- If length differs, the system pads/truncates per configuration or logs a mismatch.

**Example (Phase A integration)**:
```cpp
// Project Phase A student embedding to LanguageSystem dimension and ground a token.
std::vector<float> student_phase_a = {/* 62‑dim student vector */};
std::vector<float> projected = phase_a_system->projectStudent(student_phase_a); // Linear 62→256 + L2 normalize
auto token_id = language_system.createToken("dog", LanguageSystem::TokenType::Word, projected);
// Comment: Injecting a projected multimodal student embedding grounds the token for clustering and similarity.
```

#### getTokenId()
```cpp
std::size_t getTokenId(const std::string& symbol) const;
```
**Description**: Returns the token id for a symbol, creating a placeholder if absent.

#### setTokenEmbedding()
```cpp
void setTokenEmbedding(std::size_t token_id, const std::vector<float>& embedding);
```
**Description**: Sets or updates a token’s embedding vector.  
**Notes**: Ensure dimensionality equals `Config.embedding_dimension`.

## Data Structures

### AcousticFeatures

Represents extracted acoustic features from audio input.

```cpp
struct AcousticFeatures {
    float pitch_contour = 0.0f;           ///< Fundamental frequency trajectory [50.0-500.0 Hz]
    float energy_envelope = 0.0f;         ///< Amplitude envelope [0.0-1.0]
    float rhythm_pattern = 0.0f;          ///< Temporal rhythm score [0.0-1.0]
    float formant_f1 = 0.0f;             ///< First formant frequency [200.0-1000.0 Hz]
    float formant_f2 = 0.0f;             ///< Second formant frequency [500.0-3000.0 Hz]
    float voicing_strength = 0.0f;        ///< Voiced/unvoiced classification [0.0-1.0]
    float spectral_centroid = 0.0f;      ///< Spectral brightness [500.0-4000.0 Hz]
    float intonation_slope = 0.0f;       ///< Rising/falling intonation [-5.0-5.0 Hz/s]
    
    // Salience metrics
    float attention_score = 0.0f;         ///< Computed attention weight [0.0-1.0]
    float novelty_score = 0.0f;          ///< Acoustic novelty measure [0.0-1.0]
    float motherese_score = 0.0f;        ///< Infant-directed speech features [0.0-1.0]
};
```

### PhonemeCluster

Represents an acoustic-based phoneme cluster.

```cpp
struct PhonemeCluster {
    std::string phonetic_symbol;          ///< IPA-like representation
    AcousticFeatures acoustic_profile;    ///< Associated acoustic features
    std::vector<float> formant_pattern;   ///< Formant frequency pattern
    float vowel_consonant_ratio = 0.5f;   ///< V/C classification score [0.0-1.0]
    std::vector<std::string> variants;    ///< Acoustic variations
    float stability_score = 0.0f;        ///< Cluster coherence measure [0.0-1.0]
};
```

### VisualLanguageFeatures

Represents visual features for linguistic integration.

```cpp
struct VisualLanguageFeatures {
    float face_salience = 0.0f;           ///< Face detection confidence [0.0-1.0]
    float gaze_alignment = 0.0f;          ///< Gaze-speech synchronization [0.0-1.0]
    float lip_sync_score = 0.0f;          ///< Lip movement correlation [0.0-1.0]
    float attention_focus = 0.0f;         ///< Visual attention weight [0.0-1.0]
    std::vector<float> face_embedding;    ///< Face recognition features [128-dim]
    std::vector<float> gaze_vector;       ///< Gaze direction coordinates [2D]
    std::vector<float> lip_features;      ///< Lip shape/movement features [16-dim]
    std::vector<float> object_features;   ///< Object recognition features [variable]
    
    // Cross-modal binding strength
    float speech_vision_coupling = 0.0f;  ///< Temporal alignment score [0.0-1.0]
    float motherese_face_boost = 0.0f;    ///< Infant-directed speech + face [0.0-1.0]
};
```

### SpeechProductionFeatures

Represents features for speech production output.

```cpp
struct SpeechProductionFeatures {
    std::vector<PhonemeCluster> phoneme_sequence;  ///< Sequence of phonemes to produce
    std::vector<float> timing_pattern;             ///< Temporal timing for each phoneme [ms]
    std::vector<float> prosody_contour;            ///< Pitch/stress pattern [Hz]
    float speech_rate = 1.0f;                     ///< Speaking rate multiplier [0.5-2.0]
    float emotional_coloring = 0.0f;              ///< Emotional expression level [0.0-1.0]
    
    // Visual synchronization
    std::vector<std::vector<float>> lip_motion_sequence; ///< Lip shapes for each phoneme [16-dim]
    std::vector<float> gaze_targets;               ///< Gaze direction during speech [2D per phoneme]
    float facial_expression_intensity = 0.0f;     ///< Expression strength [0.0-1.0]
    
    // Feedback and monitoring
    float confidence_score = 0.0f;                ///< Production confidence [0.0-1.0]
    bool requires_feedback = true;                ///< Whether to monitor output
    std::chrono::steady_clock::time_point start_time; ///< Production start timestamp
};
```

### SpeechOutputState

Represents the current state of speech production.

```cpp
struct SpeechOutputState {
    bool is_speaking = false;                      ///< Currently producing speech
    std::size_t current_phoneme_index = 0;        ///< Current position in sequence
    float current_time_offset = 0.0f;             ///< Time within current phoneme [ms]
    std::vector<float> current_lip_shape;         ///< Current lip configuration [16-dim]
    std::vector<float> current_gaze_direction;    ///< Current gaze target [2D]
    
    // Feedback monitoring
    std::vector<float> acoustic_feedback;         ///< Heard audio during production
    float self_monitoring_score = 0.0f;          ///< Self-assessment of output quality [0.0-1.0]
    bool caregiver_attention_detected = false;   ///< Listener attention status
};
```

## Acoustic Processing API

### Feature Extraction

#### extractAcousticFeatures()
```cpp
AcousticFeatures extractAcousticFeatures(
    const std::vector<float>& audio_samples, 
    float sample_rate = 16000.0f) const;
```
**Description**: Extracts comprehensive acoustic features from raw audio data.

**Parameters**:
- `audio_samples`: Raw audio data as float values [-1.0, 1.0]
- `sample_rate`: Audio sample rate in Hz (default: 16000.0f)

**Returns**: `AcousticFeatures` structure with extracted features

**Example**:
```cpp
std::vector<float> audio = captureAudio();
auto features = language_system.extractAcousticFeatures(audio, 16000.0f);
std::cout << "Pitch: " << features.pitch_contour << " Hz" << std::endl;
```

#### calculateSoundSalience()
```cpp
float calculateSoundSalience(const AcousticFeatures& features) const;
```
**Description**: Computes attention weight based on acoustic salience.

**Parameters**:
- `features`: Acoustic features structure

**Returns**: Salience score [0.0-1.0]

**Example**:
```cpp
auto features = language_system.extractAcousticFeatures(audio);
float salience = language_system.calculateSoundSalience(features);
if (salience > 0.7f) {
    std::cout << "High salience audio detected!" << std::endl;
}
```

### Phoneme Processing

#### generatePhonemeCluster()
```cpp
PhonemeCluster generatePhonemeCluster(const AcousticFeatures& features) const;
```
**Description**: Creates a phoneme cluster from acoustic features.

**Parameters**:
- `features`: Acoustic features to cluster

**Returns**: `PhonemeCluster` with IPA symbol and acoustic profile

#### generatePhonemeSequence()
```cpp
std::vector<PhonemeCluster> generatePhonemeSequence(const std::string& text) const;
```
**Description**: Converts text to phoneme sequence with acoustic features.

**Parameters**:
- `text`: Input text to convert

**Returns**: Vector of phoneme clusters

### Babbling and Learning

#### performAcousticBabbling()
```cpp
void performAcousticBabbling(std::size_t num_phonemes = 5);
```
**Description**: Performs acoustic-first babbling with phoneme generation.

**Parameters**:
- `num_phonemes`: Number of phonemes to generate (default: 5)

**Example**:
```cpp
// Generate 10 acoustic phonemes
language_system.performAcousticBabbling(10);
```

#### processAcousticTeacherSignal()
```cpp
void processAcousticTeacherSignal(
    const std::vector<float>& teacher_audio,
    const std::string& label, 
    float confidence = 1.0f);
```
**Description**: Processes teacher audio signal for acoustic learning.

**Parameters**:
- `teacher_audio`: Raw audio from teacher
- `label`: Associated text label
- `confidence`: Signal confidence [0.0-1.0]

**Example**:
```cpp
std::vector<float> mama_audio = recordTeacherSaying("mama");
language_system.processAcousticTeacherSignal(mama_audio, "mama", 1.0f);
```

## Visual Integration API

### Face-Speech Coupling

#### associateTokenWithVisualFeatures()
```cpp
void associateTokenWithVisualFeatures(
    std::size_t token_id,
    const VisualLanguageFeatures& visual_features,
    float confidence = 1.0f);
```
**Description**: Associates a language token with visual features.

**Parameters**:
- `token_id`: Token identifier
- `visual_features`: Visual feature structure
- `confidence`: Association confidence [0.0-1.0]

#### processFaceSpeechEvent()
```cpp
void processFaceSpeechEvent(
    const std::vector<float>& face_embedding,
    const std::vector<float>& gaze_vector,
    const std::vector<float>& lip_features,
    const std::string& spoken_token,
    float temporal_alignment = 1.0f);
```
**Description**: Processes synchronized face and speech data.

**Parameters**:
- `face_embedding`: Face recognition features [128-dim]
- `gaze_vector`: Gaze direction [2D coordinates]
- `lip_features`: Lip shape/movement [16-dim]
- `spoken_token`: Simultaneously spoken word
- `temporal_alignment`: Synchronization quality [0.0-1.0]

**Example**:
```cpp
auto face_features = detectFace(camera_frame);
auto gaze = trackGaze(camera_frame);
auto lips = detectLips(camera_frame);
std::string word = recognizeSpeech(audio);

language_system.processFaceSpeechEvent(
    face_features, gaze, lips, word, 0.9f);
```

### Visual Attention

#### processVisualAttentionMap()
```cpp
void processVisualAttentionMap(
    const std::vector<float>& attention_map,
    const std::vector<std::string>& active_tokens);
```
**Description**: Processes visual attention map to boost token activations.

**Parameters**:
- `attention_map`: Spatial attention weights
- `active_tokens`: Currently relevant tokens

#### reinforceVisualGrounding()
```cpp
void reinforceVisualGrounding(
    std::size_t token_id,
    const std::vector<float>& visual_pattern,
    float salience_score);
```
**Description**: Reinforces visual-linguistic associations.

**Parameters**:
- `token_id`: Token to reinforce
- `visual_pattern`: Associated visual pattern
- `salience_score`: Visual salience [0.0-1.0]

### Cross-Modal Queries

#### getTokensForVisualPattern()
```cpp
std::vector<std::size_t> getTokensForVisualPattern(
    const std::vector<float>& visual_pattern,
    float similarity_threshold = 0.7f) const;
```
**Description**: Retrieves tokens associated with visual patterns.

**Parameters**:
- `visual_pattern`: Query visual pattern
- `similarity_threshold`: Minimum similarity [0.0-1.0]

**Returns**: Vector of matching token IDs

#### getCrossModalAssociations()
```cpp
std::vector<CrossModalAssociation> getCrossModalAssociations(
    std::size_t token_id) const;
```
**Description**: Gets all cross-modal associations for a token.

**Parameters**:
- `token_id`: Token identifier

**Returns**: Vector of cross-modal associations

## Speech Production API

### Speech Generation

#### generateSpeechOutput()
```cpp
SpeechProductionFeatures generateSpeechOutput(const std::string& text) const;
SpeechProductionFeatures generateSpeechOutput(
    const std::vector<std::string>& token_sequence) const;
```
**Description**: Generates complete speech production features.

**Parameters**:
- `text`: Input text to synthesize
- `token_sequence`: Sequence of tokens to synthesize

**Returns**: Complete speech production features

**Example**:
```cpp
auto speech_features = language_system.generateSpeechOutput("hello mama");
std::cout << "Phonemes: " << speech_features.phoneme_sequence.size() << std::endl;
std::cout << "Lip shapes: " << speech_features.lip_motion_sequence.size() << std::endl;
```

#### generateLipMotionSequence()
```cpp
std::vector<std::vector<float>> generateLipMotionSequence(
    const std::vector<PhonemeCluster>& phonemes) const;
```
**Description**: Generates lip motion sequence for phonemes.

**Parameters**:
- `phonemes`: Phoneme sequence

**Returns**: 16-dimensional lip shapes per phoneme

#### generateProsodyContour()
```cpp
std::vector<float> generateProsodyContour(
    const std::vector<PhonemeCluster>& phonemes,
    float emotional_intensity = 0.0f) const;
```
**Description**: Generates natural prosody contour.

**Parameters**:
- `phonemes`: Phoneme sequence
- `emotional_intensity`: Emotional coloring [0.0-1.0]

**Returns**: Pitch contour in Hz

### Production Control

#### startSpeechProduction()
```cpp
void startSpeechProduction(const SpeechProductionFeatures& speech_features);
```
**Description**: Initiates speech production with multimodal output.

**Parameters**:
- `speech_features`: Complete speech production features

#### updateSpeechProduction()
```cpp
void updateSpeechProduction(float delta_time);
```
**Description**: Updates speech production state (call every frame).

**Parameters**:
- `delta_time`: Time elapsed since last update [seconds]

**Example**:
```cpp
language_system.startSpeechProduction(speech_features);

while (language_system.getCurrentSpeechState().is_speaking) {
    language_system.updateSpeechProduction(0.016f); // 60 FPS
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}
```

#### stopSpeechProduction()
```cpp
void stopSpeechProduction();
```
**Description**: Stops current speech production.

#### getCurrentSpeechState()
```cpp
SpeechOutputState getCurrentSpeechState() const;
```
**Description**: Gets current speech production state.

**Returns**: Current speech output state

### Self-Monitoring

#### processSelfAcousticFeedback()
```cpp
void processSelfAcousticFeedback(const std::vector<float>& heard_audio);
```
**Description**: Processes self-heard audio for quality monitoring.

**Parameters**:
- `heard_audio`: Audio captured during speech production

#### processCaregiverResponse()
```cpp
void processCaregiverResponse(
    const VisualLanguageFeatures& caregiver_reaction,
    const AcousticFeatures& caregiver_audio);
```
**Description**: Processes caregiver response for learning.

**Parameters**:
- `caregiver_reaction`: Visual reaction features
- `caregiver_audio`: Caregiver audio response

#### calculateSpeechProductionQuality()
```cpp
float calculateSpeechProductionQuality(
    const SpeechProductionFeatures& intended,
    const std::vector<float>& actual_audio) const;
```
**Description**: Calculates speech production quality score.

**Parameters**:
- `intended`: Intended speech features
- `actual_audio`: Actually produced audio

**Returns**: Quality score [0.0-1.0]

## Core Language System API

### Token Management

#### createToken()
```cpp
std::size_t createToken(
    const std::string& symbol, 
    TokenType type,
    const std::vector<float>& embedding = {});
```
**Description**: Creates a new language token.

**Parameters**:
- `symbol`: Token symbol/text
- `type`: Token type (Phoneme, Word, Action, etc.)
- `embedding`: Optional initial embedding

**Returns**: Token ID

#### getToken()
```cpp
SymbolicToken* getToken(const std::string& symbol);
SymbolicToken* getToken(std::size_t token_id);
```
**Description**: Retrieves token by symbol or ID.

**Parameters**:
- `symbol`: Token symbol
- `token_id`: Token identifier

**Returns**: Pointer to token or nullptr

### Development Control

#### updateDevelopment()
```cpp
void updateDevelopment(float delta_time);
```
**Description**: Updates developmental progression.

**Parameters**:
- `delta_time`: Time elapsed [seconds]

#### getCurrentStage()
```cpp
DevelopmentalStage getCurrentStage() const noexcept;
```
**Description**: Gets current developmental stage.

**Returns**: Current stage (Chaos, Babbling, Mimicry, etc.)

#### advanceToStage()
```cpp
void advanceToStage(DevelopmentalStage stage);
```
**Description**: Manually advances to developmental stage.

**Parameters**:
- `stage`: Target developmental stage

### Statistics and Monitoring

#### getStatistics()
```cpp
Statistics getStatistics() const;
```
**Description**: Gets system statistics.

**Returns**: Statistics structure with metrics

#### generateLanguageReport()
```cpp
std::string generateLanguageReport() const;
```
**Description**: Generates comprehensive language report.

**Returns**: Formatted report string

## Utility Functions

### Audio Processing Utilities

#### generateAudioSnippet()
```cpp
std::vector<float> generateAudioSnippet(
    const PhonemeCluster& phoneme, 
    float duration_ms = 200.0f) const;
```
**Description**: Generates audio waveform for phoneme.

**Parameters**:
- `phoneme`: Phoneme cluster
- `duration_ms`: Audio duration in milliseconds

**Returns**: Audio samples at 16kHz

### Similarity Calculations

#### calculateAcousticSimilarity()
```cpp
float calculateAcousticSimilarity(
    const AcousticFeatures& features1,
    const AcousticFeatures& features2) const;
```
**Description**: Calculates acoustic similarity between features.

**Parameters**:
- `features1`, `features2`: Acoustic features to compare

**Returns**: Similarity score [0.0-1.0]

#### calculateFaceLanguageConfidence()
```cpp
float calculateFaceLanguageConfidence(
    const VisualLanguageFeatures& visual_features,
    const AcousticFeatures& acoustic_features) const;
```
**Description**: Calculates face-language binding confidence.

**Parameters**:
- `visual_features`: Visual language features
- `acoustic_features`: Acoustic features

**Returns**: Confidence score [0.0-1.0]

## Error Handling

### Exception Types

The language system uses standard C++ exceptions:

- `std::invalid_argument`: Invalid parameter values
- `std::runtime_error`: Runtime processing errors
- `std::out_of_range`: Index out of bounds
- `std::bad_alloc`: Memory allocation failures

### Error Checking

```cpp
try {
    language_system.initialize();
    language_system.performAcousticBabbling(10);
} catch (const std::invalid_argument& e) {
    std::cerr << "Invalid parameter: " << e.what() << std::endl;
} catch (const std::runtime_error& e) {
    std::cerr << "Runtime error: " << e.what() << std::endl;
}
```

### Return Value Checking

```cpp
// Check initialization success
if (!language_system.initialize()) {
    std::cerr << "Failed to initialize language system" << std::endl;
    return -1;
}

// Check token creation
auto token_id = language_system.createToken("test", TokenType::Word);
auto* token = language_system.getToken(token_id);
if (!token) {
    std::cerr << "Failed to create token" << std::endl;
}
```

## Version Information

### Current Version: 2.0.0

**Release Date**: December 2024

**Major Features**:
- Acoustic-first language learning
- Visual-linguistic integration
- Real-time speech production
- Cross-modal association framework

### API Versioning

```cpp
// Version macros (defined in LanguageSystem.h)
#define NEUROFORGE_LANGUAGE_VERSION_MAJOR 2
#define NEUROFORGE_LANGUAGE_VERSION_MINOR 0
#define NEUROFORGE_LANGUAGE_VERSION_PATCH 0

// Runtime version check
auto version = language_system.getVersion();
std::cout << "Language System v" << version.major << "." 
          << version.minor << "." << version.patch << std::endl;
```

### Compatibility

- **Minimum C++ Standard**: C++20
- **Compiler Support**: MSVC 2022+, GCC 11+, Clang 13+
- **Platform Support**: Windows 10/11, Linux (Ubuntu 20.04+)
- **Dependencies**: OpenCV 4.0+ (optional), SQLite3, Cap'n Proto

### Breaking Changes from v1.x

1. **Configuration Structure**: Extended with acoustic and visual parameters
2. **Method Signatures**: Enhanced with multimodal parameters
3. **Return Types**: New data structures for acoustic and visual features
4. **Dependencies**: Additional OpenCV requirement for visual features

### Migration Guide

```cpp
// v1.x code
language_system.performBabbling(5);

// v2.x equivalent
language_system.performAcousticBabbling(5);

// v1.x configuration
LanguageSystem::Config config;
config.enable_teacher_mode = true;

// v2.x enhanced configuration
LanguageSystem::Config config;
config.enable_teacher_mode = true;
config.enable_acoustic_preprocessing = true;
config.enable_vision_grounding = true;
config.enable_speech_output = true;
```

---

**Last Updated**: December 2024  
**API Version**: 2.0.0  
**Documentation Status**: Complete

For implementation examples and integration guides, see [Language_System_Integration_Guide.md](Language_System_Integration_Guide.md).
