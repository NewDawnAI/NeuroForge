# Visual-Linguistic Integration in NeuroForge

## Overview

The NeuroForge LanguageSystem now supports comprehensive **visual-linguistic integration**, enabling cross-modal binding between speech, faces, gaze, and visual attention. This system implements biologically-inspired face-speech coupling that mirrors human language acquisition through multimodal grounding.

## Key Features

### 🎭 Face-Speech Coupling
- **Real-time face detection** → language token association
- **Gaze direction tracking** → attention-weighted token activation  
- **Lip-sync correlation** → speech-vision temporal binding
- **Motherese + face boost** → enhanced infant-directed speech learning

### 🧠 Cross-Modal Associations
- **Visual pattern → token mapping** with similarity-based retrieval
- **Temporal alignment scoring** for speech-vision synchronization
- **Association decay** to prevent memory bloat
- **Confidence calculation** based on multimodal evidence

### 👁️ Visual Attention Integration
- **Attention map processing** → token activation boosting
- **Salience-based reinforcement** of visual-linguistic bindings
- **Dynamic threshold adaptation** based on attention history

## Architecture

### Core Data Structures

```cpp
struct VisualLanguageFeatures {
    float face_salience = 0.0f;           // Face detection confidence
    float gaze_alignment = 0.0f;          // Gaze-speech synchronization  
    float lip_sync_score = 0.0f;          // Lip movement correlation
    float attention_focus = 0.0f;         // Visual attention weight
    std::vector<float> face_embedding;    // Face recognition features
    std::vector<float> gaze_vector;       // Gaze direction coordinates
    std::vector<float> lip_features;      // Lip shape/movement features
    float speech_vision_coupling = 0.0f;  // Temporal alignment score
    float motherese_face_boost = 0.0f;    // Infant-directed speech + face
};

struct CrossModalAssociation {
    std::size_t token_id;                 // Associated language token
    std::string modality;                 // "vision", "audio", etc.
    std::vector<float> pattern;           // Sensory pattern vector
    float association_strength = 0.0f;    // Binding strength
    float temporal_alignment = 0.0f;      // Synchronization score
    VisualLanguageFeatures visual_features; // Face/gaze/lip features
    float face_language_confidence = 0.0f; // Face-speech binding confidence
};
```

### Configuration Parameters

```cpp
struct Config {
    // Visual-linguistic integration
    float face_language_coupling = 0.6f;    // Face-speech binding strength
    float gaze_attention_weight = 0.4f;     // Gaze direction influence
    float lip_sync_threshold = 0.3f;        // Minimum lip-speech correlation
    float visual_grounding_boost = 0.5f;    // Visual modality reinforcement
    float cross_modal_decay = 0.01f;        // Association decay rate
    bool enable_face_language_bias = true;  // Boost face-speech associations
};
```

## Integration with Visual Cortex

### 1. Face Detection Integration

Connect face detection outputs to language learning:

```cpp
// When face is detected during speech
std::vector<float> face_embedding = visual_cortex.getFaceEmbedding();
std::vector<float> gaze_vector = visual_cortex.getGazeDirection();
std::vector<float> lip_features = visual_cortex.getLipFeatures();
std::string spoken_word = audio_system.getCurrentWord();

// Create face-speech association
language_system.processFaceSpeechEvent(
    face_embedding, gaze_vector, lip_features, spoken_word, 0.9f);
```

### 2. Visual Attention Integration

Connect attention maps to token activation:

```cpp
// Process visual attention during speech
std::vector<float> attention_map = visual_cortex.getAttentionMap();
std::vector<std::string> active_tokens = {"mama", "face", "smile"};

language_system.processVisualAttentionMap(attention_map, active_tokens);
```

### 3. Object Recognition Grounding

Associate visual objects with language tokens:

```cpp
// When object is recognized during naming
std::size_t ball_token = language_system.createToken("ball", TokenType::Perception);
std::vector<float> ball_visual_pattern = visual_cortex.getObjectFeatures("ball");
float salience = visual_cortex.getObjectSalience("ball");

language_system.reinforceVisualGrounding(ball_token, ball_visual_pattern, salience);
```

### 4. Cross-Modal Pattern Retrieval

Retrieve language tokens from visual patterns:

```cpp
// Find tokens associated with current visual scene
std::vector<float> current_visual_scene = visual_cortex.getSceneFeatures();
auto matching_tokens = language_system.getTokensForVisualPattern(
    current_visual_scene, 0.7f); // 70% similarity threshold

// Activate matching tokens for narration
for (std::size_t token_id : matching_tokens) {
    auto* token = language_system.getToken(token_id);
    token->activation_strength += 0.2f; // Boost activation
}
```

## Face Bias Module Integration

### High-Salience Face Events

```cpp
// Connect face bias module outputs
float face_importance = face_bias_module.getFaceImportanceWeight();
std::vector<float> face_features = face_bias_module.getFaceFeatures();
bool is_caregiver_face = face_bias_module.isCaregiverFace();

if (face_importance > 0.8f && is_caregiver_face) {
    // High-salience face event during speech
    VisualLanguageFeatures visual_features;
    visual_features.face_salience = face_importance;
    visual_features.face_embedding = face_features;
    visual_features.motherese_face_boost = is_caregiver_face ? 0.6f : 0.0f;
    
    std::size_t token_id = language_system.getTokenId(current_spoken_word);
    language_system.associateTokenWithVisualFeatures(token_id, visual_features, 1.0f);
}
```

### Gaze-Speech Synchronization

```cpp
// Track gaze during speech for attention coupling
std::vector<float> gaze_direction = face_bias_module.getGazeDirection();
float gaze_stability = face_bias_module.getGazeStability();
bool looking_at_speaker = face_bias_module.isLookingAtSpeaker();

if (looking_at_speaker && gaze_stability > 0.7f) {
    // Strong gaze-speech coupling
    language_system.processFaceSpeechEvent(
        face_features, gaze_direction, lip_features, 
        spoken_word, gaze_stability);
}
```

## Usage Examples

### Basic Face-Speech Learning

```cpp
// Initialize language system with visual integration
LanguageSystem::Config config;
config.enable_face_language_bias = true;
config.face_language_coupling = 0.6f;
config.visual_grounding_boost = 0.5f;

LanguageSystem language_system(config);
language_system.initialize();

// Process face-speech events
std::vector<float> mama_face = extractFaceFeatures(camera_frame);
std::vector<float> gaze_vector = {0.0f, 0.0f}; // Direct gaze
std::vector<float> lip_movement = extractLipFeatures(camera_frame);

language_system.processFaceSpeechEvent(
    mama_face, gaze_vector, lip_movement, "mama", 0.9f);
```

### Visual Attention-Guided Learning

```cpp
// Connect visual attention to language activation
std::vector<float> attention_map = visual_cortex.computeAttentionMap();
std::vector<std::string> visible_objects = {"ball", "red", "round"};

language_system.processVisualAttentionMap(attention_map, visible_objects);

// Check which tokens were boosted
for (const std::string& obj : visible_objects) {
    auto* token = language_system.getToken(obj);
    if (token && token->activation_strength > 0.5f) {
        std::cout << obj << " token activated by visual attention\n";
    }
}
```

### Cross-Modal Association Queries

```cpp
// Find language tokens associated with visual patterns
std::vector<float> current_face = visual_cortex.getCurrentFaceEmbedding();
auto face_tokens = language_system.getTokensForVisualPattern(current_face, 0.8f);

std::cout << "Tokens associated with current face: ";
for (std::size_t token_id : face_tokens) {
    auto* token = language_system.getToken(token_id);
    std::cout << token->symbol << " ";
}
std::cout << std::endl;

// Get all cross-modal associations for a token
auto mama_associations = language_system.getCrossModalAssociations(mama_token_id);
for (const auto& assoc : mama_associations) {
    std::cout << "Modality: " << assoc.modality 
              << ", Strength: " << assoc.association_strength
              << ", Face confidence: " << assoc.face_language_confidence << std::endl;
}
```

## Performance Considerations

### Memory Management
- Cross-modal associations are limited to 1000 entries with automatic pruning
- Visual feature histories are capped at 10 entries per token
- Association decay prevents indefinite memory growth

### Computational Efficiency
- Cosine similarity used for fast pattern matching
- Temporal alignment scoring with configurable thresholds
- Lazy evaluation of cross-modal confidence calculations

### Integration Points
- **Visual Cortex**: Face detection, object recognition, attention maps
- **Face Bias Module**: Salience weights, caregiver detection, gaze tracking
- **Audio System**: Speech recognition, prosodic features, temporal alignment
- **Motor System**: Action-language grounding, gesture recognition

## Testing and Validation

Run the comprehensive test suite:

```bash
# Compile and run visual-linguistic integration tests
g++ -std=c++17 -I../include src/test_visual_language_integration.cpp \
    src/core/LanguageSystem.cpp src/core/LanguageSystem_Acoustic.cpp \
    src/core/LanguageSystem_Visual.cpp -o test_visual_language

./test_visual_language
```

Expected output:
```
=== NeuroForge Visual-Linguistic Integration Tests ===

Test 1: Face-Speech Coupling... PASSED
Test 2: Visual Attention Integration... PASSED  
Test 3: Cross-Modal Pattern Retrieval... PASSED
Test 4: Face-Language Confidence Calculation... PASSED
Test 5: Cross-Modal Association Decay... PASSED
Test 6: Integrated Face-Speech Learning... PASSED

=== Test Results ===
Passed: 6/6 tests
Success Rate: 100.0%
🎉 All tests passed! Visual-linguistic integration is working correctly.
✅ Face-speech coupling enabled
✅ Cross-modal associations functional  
✅ Visual attention integration active
✅ Ready for connection to visual cortex and face bias modules
```

## Future Enhancements

### Planned Features
- **Gesture-speech integration** for embodied language learning
- **Object permanence** tracking for consistent visual-linguistic bindings
- **Social gaze following** for joint attention language learning
- **Emotional expression** integration with prosodic features

### Research Directions
- **Developmental timing** of face-speech coupling emergence
- **Individual differences** in visual-linguistic integration strength
- **Cross-cultural variations** in motherese-face associations
- **Neuroplasticity** modeling for adaptive cross-modal learning

This visual-linguistic integration system provides the foundation for human-like language grounding through multimodal experience, enabling NeuroForge to learn language the way infants do: through rich sensory associations between sounds, faces, and visual attention.