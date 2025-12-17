#include "core/SubstrateLanguageIntegration.h"
#include "core/Neuron.h"
#include "core/Synapse.h"
#include <algorithm>
#include <numeric>
#include <cmath>
#include <random>
#include <iostream>
#include <sstream>
#include <chrono>

namespace NeuroForge {
namespace Core {

SubstrateLanguageIntegration::SubstrateLanguageIntegration(
    std::shared_ptr<LanguageSystem> language_system,
    std::shared_ptr<HypergraphBrain> hypergraph_brain,
    const Config& config)
    : language_system_(language_system)
    , hypergraph_brain_(hypergraph_brain)
    , config_(config) {
}

SubstrateLanguageIntegration::~SubstrateLanguageIntegration() {
    shutdown();
}

bool SubstrateLanguageIntegration::initialize() {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    if (is_initialized_.load()) {
        return true;
    }
    
    if (!language_system_ || !hypergraph_brain_) {
        std::cerr << "SubstrateLanguageIntegration: Missing required system components" << std::endl;
        return false;
    }
    
    // Initialize neural regions for language processing
    if (!createLanguageRegions()) {
        std::cerr << "SubstrateLanguageIntegration: Failed to create language regions" << std::endl;
        return false;
    }
    
    // Connect regions for cross-modal processing
    if (!connectLanguageRegions()) {
        std::cerr << "SubstrateLanguageIntegration: Failed to connect language regions" << std::endl;
        return false;
    }
    
    // Initialize speech production regions
    if (!initializeSpeechProductionRegions()) {
        std::cerr << "SubstrateLanguageIntegration: Failed to initialize speech production regions" << std::endl;
        return false;
    }
    
    // Initialize multimodal stream regions
    if (!initializeMultimodalStreamRegions()) {
        std::cerr << "SubstrateLanguageIntegration: Failed to initialize multimodal stream regions" << std::endl;
        return false;
    }
    
    // Integrate with the learning system
    integrateWithLearningSystem();

    // Enable direct language-to-neuron influence when integration is active
    if (config_.integration_mode != Config::IntegrationMode::Passive) {
        std::weak_ptr<HypergraphBrain> weak_brain = hypergraph_brain_;
        language_system_->setNeuronBiasCallback(
            [weak_brain](NeuroForge::NeuronID neuron_id, float strength) {
                if (auto brain = weak_brain.lock()) {
                    brain->biasNeuronActivation(neuron_id, strength);
                }
            });
    }
    
    is_initialized_.store(true);
    is_active_.store(true);
    
    std::cout << "SubstrateLanguageIntegration: Successfully initialized" << std::endl;
    return true;
}

void SubstrateLanguageIntegration::shutdown() {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    is_active_.store(false);
    is_initialized_.store(false);
    
    // Clear all mappings
    token_bindings_.clear();
    proto_word_patterns_.clear();
    grounding_associations_.clear();
    phoneme_mappings_.clear();
    prosody_patterns_.clear();
    
    // Reset region pointers
    language_region_.reset();
    proto_word_region_.reset();
    prosodic_region_.reset();
    grounding_region_.reset();
    phoneme_region_.reset();
    motor_region_.reset();
    lipsync_region_.reset();
    prosody_control_region_.reset();
    audio_stream_region_.reset();
    visual_stream_region_.reset();
    gaze_coordination_region_.reset();
    multimodal_integration_region_.reset();
}

void SubstrateLanguageIntegration::reset() {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    // Reset all neural bindings and patterns
    for (auto& [token_id, binding] : token_bindings_) {
        binding.binding_strength = 0.0f;
        binding.activation_count = 0;
    }
    
    for (auto& [pattern, neural_pattern] : proto_word_patterns_) {
        neural_pattern.crystallization_strength = 0.0f;
        neural_pattern.neural_stability = 0.0f;
        neural_pattern.is_crystallized = false;
    }
    
    for (auto& [grounding_id, association] : grounding_associations_) {
        association.association_strength = 0.0f;
        association.modality_weights.clear();
    }
    
    // Reset statistics
    resetStatistics();
}

bool SubstrateLanguageIntegration::createLanguageRegions() {
    if (!hypergraph_brain_) {
        return false;
    }
    
    try {
        // Create language processing region
        language_region_ = hypergraph_brain_->createRegion("language_processing", 
                                                          Region::Type::Cortical, 
                                                          Region::ActivationPattern::Asynchronous);
        if (!language_region_) {
            std::cerr << "Failed to create language processing region" << std::endl;
            return false;
        }
        // Create neurons for language processing region
        language_region_->createNeurons(static_cast<std::size_t>(config_.language_region_neurons));
        
        // Create proto-word region
        proto_word_region_ = hypergraph_brain_->createRegion("proto_word", 
                                                           Region::Type::Cortical, 
                                                           Region::ActivationPattern::Synchronous);
        if (!proto_word_region_) {
            std::cerr << "Failed to create proto-word region" << std::endl;
            return false;
        }
        // Create neurons for proto-word region
        proto_word_region_->createNeurons(static_cast<std::size_t>(config_.proto_word_region_neurons));
        
        // Create prosodic region
        prosodic_region_ = hypergraph_brain_->createRegion("prosodic", 
                                                         Region::Type::Cortical, 
                                                         Region::ActivationPattern::Oscillatory);
        if (!prosodic_region_) {
            std::cerr << "Failed to create prosodic region" << std::endl;
            return false;
        }
        // Create neurons for prosodic region
        prosodic_region_->createNeurons(static_cast<std::size_t>(config_.prosodic_region_neurons));
        
        // Create grounding region
        grounding_region_ = hypergraph_brain_->createRegion("grounding", 
                                                          Region::Type::Cortical, 
                                                          Region::ActivationPattern::Competitive);
        if (!grounding_region_) {
            std::cerr << "Failed to create grounding region" << std::endl;
            return false;
        }
        // Create neurons for grounding region
        grounding_region_->createNeurons(static_cast<std::size_t>(config_.grounding_region_neurons));
        
        std::cout << "Successfully created language regions" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception creating language regions: " << e.what() << std::endl;
        return false;
    }
}

bool SubstrateLanguageIntegration::connectLanguageRegions() {
    if (!language_region_ || !proto_word_region_ || !prosodic_region_ || !grounding_region_) {
        return false;
    }
    
    try {
        // Connect language region to proto-word region
        hypergraph_brain_->connectRegions(language_region_->getId(), proto_word_region_->getId(), 0.8f);
        
        // Connect language region to prosodic region
        hypergraph_brain_->connectRegions(language_region_->getId(), prosodic_region_->getId(), 0.6f);
        
        // Connect language region to grounding region
        hypergraph_brain_->connectRegions(language_region_->getId(), grounding_region_->getId(), 0.7f);
        
        // Connect proto-word region to prosodic region
        hypergraph_brain_->connectRegions(proto_word_region_->getId(), prosodic_region_->getId(), 0.5f);
        
        // Connect grounding region to all other regions
        hypergraph_brain_->connectRegions(grounding_region_->getId(), language_region_->getId(), 0.6f);
        hypergraph_brain_->connectRegions(grounding_region_->getId(), proto_word_region_->getId(), 0.5f);
        hypergraph_brain_->connectRegions(grounding_region_->getId(), prosodic_region_->getId(), 0.4f);
        
        std::cout << "Successfully connected language regions" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception connecting language regions: " << e.what() << std::endl;
        return false;
    }
}

bool SubstrateLanguageIntegration::initializeSpeechProductionRegions() {
    if (!hypergraph_brain_) {
        return false;
    }
    
    try {
        // Create phoneme processing region
        phoneme_region_ = hypergraph_brain_->createRegion("phoneme_processing", 
                                                        Region::Type::Cortical, 
                                                        Region::ActivationPattern::Asynchronous);
        if (!phoneme_region_) {
            std::cerr << "Failed to create phoneme processing region" << std::endl;
            return false;
        }
        
        // Create motor coordination region
        motor_region_ = hypergraph_brain_->createRegion("motor_coordination", 
                                                      Region::Type::Cortical, 
                                                      Region::ActivationPattern::Synchronous);
        if (!motor_region_) {
            std::cerr << "Failed to create motor coordination region" << std::endl;
            return false;
        }
        
        // Create lip-sync region
        lipsync_region_ = hypergraph_brain_->createRegion("lipsync_coordination", 
                                                        Region::Type::Cortical, 
                                                        Region::ActivationPattern::Synchronous);
        if (!lipsync_region_) {
            std::cerr << "Failed to create lip-sync coordination region" << std::endl;
            return false;
        }
        
        // Create prosody control region
        prosody_control_region_ = hypergraph_brain_->createRegion("prosody_control", 
                                                                Region::Type::Cortical, 
                                                                Region::ActivationPattern::Oscillatory);
        if (!prosody_control_region_) {
            std::cerr << "Failed to create prosody control region" << std::endl;
            return false;
        }
        
        // Connect speech production regions
        connectSpeechProductionRegions();
        
        std::cout << "Successfully initialized speech production regions" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception initializing speech production regions: " << e.what() << std::endl;
        return false;
    }
}

void SubstrateLanguageIntegration::connectSpeechProductionRegions() {
    if (!phoneme_region_ || !motor_region_ || !lipsync_region_ || !prosody_control_region_) {
        return;
    }
    
    // Connect phoneme region to motor region
    hypergraph_brain_->connectRegions(phoneme_region_->getId(), motor_region_->getId(), 0.8f);
    
    // Connect phoneme region to lip-sync region
    hypergraph_brain_->connectRegions(phoneme_region_->getId(), lipsync_region_->getId(), 0.9f);
    
    // Connect prosody control to motor region
    hypergraph_brain_->connectRegions(prosody_control_region_->getId(), motor_region_->getId(), 0.7f);
    
    // Connect prosody control to phoneme region
    hypergraph_brain_->connectRegions(prosody_control_region_->getId(), phoneme_region_->getId(), 0.6f);
    
    // Connect to existing language regions
    if (language_region_) {
        hypergraph_brain_->connectRegions(language_region_->getId(), phoneme_region_->getId(), 0.8f);
        hypergraph_brain_->connectRegions(language_region_->getId(), prosody_control_region_->getId(), 0.6f);
    }
    
    if (proto_word_region_) {
        hypergraph_brain_->connectRegions(proto_word_region_->getId(), phoneme_region_->getId(), 0.7f);
    }
}

bool SubstrateLanguageIntegration::initializeMultimodalStreamRegions() {
    if (!hypergraph_brain_) {
        std::cerr << "initializeMultimodalStreamRegions: hypergraph_brain_ is null" << std::endl;
        return false;
    }
    
    try {
        // Create audio stream processing region
        audio_stream_region_ = hypergraph_brain_->createRegion("audio_stream", 
                                                             Region::Type::Cortical, 
                                                             Region::ActivationPattern::Asynchronous);
        if (!audio_stream_region_) {
            std::cerr << "Failed to create audio stream region" << std::endl;
            return false;
        }
        
        // Create visual stream processing region
        visual_stream_region_ = hypergraph_brain_->createRegion("visual_stream", 
                                                              Region::Type::Cortical, 
                                                              Region::ActivationPattern::Asynchronous);
        if (!visual_stream_region_) {
            std::cerr << "Failed to create visual stream region" << std::endl;
            return false;
        }
        
        // Create gaze coordination region
        gaze_coordination_region_ = hypergraph_brain_->createRegion("gaze_coordination", 
                                                                  Region::Type::Cortical, 
                                                                  Region::ActivationPattern::Synchronous);
        if (!gaze_coordination_region_) {
            std::cerr << "Failed to create gaze coordination region" << std::endl;
            return false;
        }
        
        // Create multimodal integration region
        multimodal_integration_region_ = hypergraph_brain_->createRegion("multimodal_integration", 
                                                                       Region::Type::Cortical, 
                                                                       Region::ActivationPattern::Competitive);
        if (!multimodal_integration_region_) {
            std::cerr << "Failed to create multimodal integration region" << std::endl;
            return false;
        }
        
        // Connect multimodal regions
        hypergraph_brain_->connectRegions(audio_stream_region_->getId(), multimodal_integration_region_->getId(), 0.8f);
        hypergraph_brain_->connectRegions(visual_stream_region_->getId(), multimodal_integration_region_->getId(), 0.8f);
        hypergraph_brain_->connectRegions(gaze_coordination_region_->getId(), multimodal_integration_region_->getId(), 0.6f);
        
        std::cout << "Successfully initialized multimodal stream regions" << std::endl;
        std::cerr << "initializeMultimodalStreamRegions: connected audio/visual/gaze to integration" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception initializing multimodal stream regions: " << e.what() << std::endl;
        return false;
    }
}

void SubstrateLanguageIntegration::integrateWithLearningSystem() {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!hypergraph_brain_) {
        std::cerr << "SubstrateLanguageIntegration: hypergraph_brain_ is null; cannot integrate LearningSystem" << std::endl;
        return;
    }

    // Obtain the LearningSystem from HypergraphBrain and cache a shared_ptr wrapper
    NeuroForge::Core::LearningSystem* ls_raw = hypergraph_brain_->getLearningSystem();
    if (!ls_raw) {
        // Attempt to initialize a LearningSystem if one is not present
        try {
            hypergraph_brain_->initializeLearning();
            ls_raw = hypergraph_brain_->getLearningSystem();
        } catch (const std::exception& e) {
            std::cerr << "SubstrateLanguageIntegration: Failed to initialize LearningSystem: " << e.what() << std::endl;
        }
        if (!ls_raw) {
            std::cerr << "SubstrateLanguageIntegration: HypergraphBrain has no LearningSystem; integration skipped" << std::endl;
            return;
        }
    }

    // We do not own the LearningSystem; create an aliasing shared_ptr for safe access
    learning_system_ = std::shared_ptr<LearningSystem>(hypergraph_brain_, ls_raw);

    // Ensure learning is enabled at the brain level
    try {
        hypergraph_brain_->setLearningEnabled(true);
    } catch (...) {
        // Non-fatal if enabling fails; continue with best-effort integration
    }

    // Configure attention modulation coherently between integration config and learning system config
    // If integration config enables attention modulation, propagate minimal enabling to LearningSystem
    if (learning_system_) {
        auto ls_cfg = learning_system_->getConfig();
        // Respect existing learning system settings, only toggle enablement if integration requests it
        if (config_.enable_attention_modulation && !ls_cfg.enable_attention_modulation) {
            ls_cfg.enable_attention_modulation = true;
            // Provide a sensible default boost if unset
            if (ls_cfg.attention_boost_factor <= 0.0f) {
                ls_cfg.attention_boost_factor = 1.25f;
            }
            // Keep bounds reasonable in absence of prior configuration
            if (ls_cfg.attention_Amin <= 0.0f) ls_cfg.attention_Amin = 1.0f;
            if (ls_cfg.attention_Amax < ls_cfg.attention_Amin) ls_cfg.attention_Amax = ls_cfg.attention_Amin + 0.5f;
            // Apply anneal default override if not yet configured
            if (ls_cfg.attention_anneal_ms <= 0) {
                ls_cfg.attention_anneal_ms = 500; // default 500ms decay window
            }
            // Default attention mode to ExternalMap to align with forwarded maps
            if (ls_cfg.attention_mode == LearningSystem::AttentionMode::Off) {
                ls_cfg.attention_mode = LearningSystem::AttentionMode::ExternalMap;
            }
            learning_system_->updateConfig(ls_cfg);
        }

    }

    std::cout << "SubstrateLanguageIntegration: LearningSystem integrated and attention wiring applied" << std::endl;
}

// Simplified method implementations
bool SubstrateLanguageIntegration::mapPhonemeToNeuralAssembly(const std::string& phoneme,
                                                             const std::vector<NeuroForge::NeuronID>& assembly_neurons) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    if (!is_active_.load() || assembly_neurons.empty()) {
        return false;
    }
    
    // Create phoneme neural mapping
    PhonemeNeuralMapping mapping;
    mapping.phoneme = phoneme;
    mapping.assembly_neurons = assembly_neurons;
    mapping.activation_strength = 0.0f;
    mapping.motor_coordination = 0.0f;
    mapping.lipsync_coordination = 0.0f;
    mapping.creation_time = std::chrono::steady_clock::now();
    mapping.last_activation = mapping.creation_time;
    mapping.activation_count = 0;
    
    // Store the mapping
    std::lock_guard<std::mutex> phoneme_lock(phoneme_mapping_mutex_);
    phoneme_mappings_[phoneme] = mapping;
    
    return true;
}

void SubstrateLanguageIntegration::activateSpeechProductionNeurons(const std::string& phoneme, float activation_strength) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    if (!is_active_.load()) {
        return;
    }
    
    // Find phoneme mapping
    std::lock_guard<std::mutex> phoneme_lock(phoneme_mapping_mutex_);
    auto it = phoneme_mappings_.find(phoneme);
    if (it != phoneme_mappings_.end()) {
        it->second.activation_strength = activation_strength;
        it->second.last_activation = std::chrono::steady_clock::now();
        it->second.activation_count++;
        
        // Activate motor coordination
        activateMotorCoordination(phoneme, activation_strength);
        
        // Activate lip-sync coordination
        activateLipSyncCoordination(phoneme, activation_strength);
    }
}

void SubstrateLanguageIntegration::integrateLipSyncWithNeuralSubstrate(const std::string& phoneme,
                                                                      const std::vector<float>& lip_motion_features) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    if (!is_active_.load() || lip_motion_features.empty()) {
        std::cerr << "integrateLipSyncWithNeuralSubstrate: skipped (inactive or empty features) for phoneme '" 
                  << phoneme << "'" << std::endl;
        return;
    }
    
    // Find phoneme mapping and update lip-sync coordination
    std::lock_guard<std::mutex> phoneme_lock(phoneme_mapping_mutex_);
    auto it = phoneme_mappings_.find(phoneme);
    if (it != phoneme_mappings_.end()) {
        // Calculate coordination strength from lip motion features
        float coordination_strength = 0.0f;
        for (float shape_value : lip_motion_features) {
            coordination_strength += std::abs(shape_value);
        }
        coordination_strength /= static_cast<float>(lip_motion_features.size());
        
        it->second.lipsync_coordination = coordination_strength;
        std::cerr << "integrateLipSyncWithNeuralSubstrate: phoneme '" << phoneme
                  << "' coordination_strength=" << coordination_strength << std::endl;
    } else {
        std::cerr << "integrateLipSyncWithNeuralSubstrate: phoneme mapping not found for '" << phoneme << "'" << std::endl;
    }
}

bool SubstrateLanguageIntegration::mapProsodyToNeuralPattern(const std::string& pattern_name,
                                                            const std::vector<float>& pitch_contour,
                                                            const std::vector<float>& energy_contour,
                                                            const std::vector<float>& rhythm_pattern) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    if (!is_active_.load()) {
        std::cerr << "mapProsodyToNeuralPattern: skipped (integration inactive) pattern '" << pattern_name << "'" << std::endl;
        return false;
    }
    
    // Create prosody neural pattern
    ProsodyNeuralPattern pattern;
    pattern.pattern_name = pattern_name;
    pattern.pitch_contour = pitch_contour;
    pattern.energy_contour = energy_contour;
    pattern.rhythm_pattern = rhythm_pattern;
    pattern.pattern_strength = 0.0f;
    pattern.stability = 0.0f;
    pattern.is_stable = false;
    pattern.creation_time = std::chrono::steady_clock::now();
    pattern.last_reinforcement = pattern.creation_time;
    
    // Store the pattern
    std::lock_guard<std::mutex> prosody_lock(prosody_pattern_mutex_);
    prosody_patterns_[pattern_name] = pattern;
    std::cerr << "mapProsodyToNeuralPattern: stored pattern '" << pattern_name
              << "' pitch_len=" << pitch_contour.size()
              << " energy_len=" << energy_contour.size()
              << " rhythm_len=" << rhythm_pattern.size() << std::endl;
    
    return true;
}

void SubstrateLanguageIntegration::activateMotorCoordination(const std::string& phoneme, float coordination_strength) {
    // Simplified motor coordination activation
    if (motor_region_) {
        // Activate motor neurons based on phoneme and coordination strength
        // This is a placeholder for more complex motor coordination
    }
}

void SubstrateLanguageIntegration::activateLipSyncCoordination(const std::string& phoneme, float coordination_strength) {
    // Simplified lip-sync coordination activation
    if (lipsync_region_) {
        // Activate lip-sync neurons based on phoneme and coordination strength
        // This is a placeholder for more complex lip-sync coordination
    }
}

// Multimodal stream processing methods
bool SubstrateLanguageIntegration::createAudioProcessingStream(const std::vector<float>& audio_features) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    if (!is_active_.load() || !audio_stream_region_) {
        std::cerr << "createAudioProcessingStream: skipped (inactive or audio_stream_region_ missing)" << std::endl;
        return false;
    }
    
    // Update audio stream state
    std::lock_guard<std::mutex> audio_lock(audio_stream_mutex_);
    current_audio_stream_.features = audio_features;
    current_audio_stream_.activation_strength = 1.0f;
    current_audio_stream_.last_updated = std::chrono::steady_clock::now();
    std::cerr << "createAudioProcessingStream: features_len=" << audio_features.size() << std::endl;
    
    return true;
}

bool SubstrateLanguageIntegration::createVisualProcessingStream(const std::vector<float>& visual_features) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    if (!is_active_.load() || !visual_stream_region_) {
        std::cerr << "createVisualProcessingStream: skipped (inactive or visual_stream_region_ missing)" << std::endl;
        return false;
    }
    
    // Update visual stream state
    std::lock_guard<std::mutex> visual_lock(visual_stream_mutex_);
    current_visual_stream_.features = visual_features;
    current_visual_stream_.activation_strength = 1.0f;
    current_visual_stream_.last_updated = std::chrono::steady_clock::now();
    
    return true;
}

bool SubstrateLanguageIntegration::createGazeCoordinationStream(const std::vector<float>& gaze_targets) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    if (!is_active_.load() || !gaze_coordination_region_) {
        return false;
    }
    
    // Update gaze stream state
    std::lock_guard<std::mutex> gaze_lock(gaze_stream_mutex_);
    current_gaze_stream_.gaze_targets = gaze_targets;
    current_gaze_stream_.activation_strength = 1.0f;
    current_gaze_stream_.last_updated = std::chrono::steady_clock::now();
    
    return true;
}

bool SubstrateLanguageIntegration::synchronizeMultimodalStreams(float temporal_alignment_threshold) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    if (!is_active_.load()) {
        return false;
    }
    
    // Check if all streams are active and synchronized
    auto now = std::chrono::steady_clock::now();
    auto threshold_duration = std::chrono::milliseconds(static_cast<int>(temporal_alignment_threshold * 1000));
    
    bool audio_recent = (now - current_audio_stream_.last_updated) < threshold_duration;
    bool visual_recent = (now - current_visual_stream_.last_updated) < threshold_duration;
    bool gaze_recent = (now - current_gaze_stream_.last_updated) < threshold_duration;
    
    multimodal_state_.streams_synchronized = audio_recent && visual_recent && gaze_recent;
    multimodal_state_.last_sync_time = now;
    
    return multimodal_state_.streams_synchronized;
}

bool SubstrateLanguageIntegration::activateParallelNeuralStreams(const LanguageSystem::SpeechProductionFeatures& speech_features,
                                                                const LanguageSystem::VisualLanguageFeatures& visual_features) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    try {
        // Extract features for parallel processing
        std::vector<float> audio_features;
        for (const auto& phoneme : speech_features.phoneme_sequence) {
            // Use acoustic_profile with correct member names
            const auto& profile = phoneme.acoustic_profile;
            audio_features.push_back(profile.pitch_contour);
            audio_features.push_back(profile.energy_envelope);
            audio_features.push_back(profile.spectral_centroid);
            audio_features.push_back(profile.formant_f1);
            audio_features.push_back(profile.formant_f2);
            // Add formant pattern
            audio_features.insert(audio_features.end(), 
                                phoneme.formant_pattern.begin(), 
                                phoneme.formant_pattern.end());
        }
        
        std::vector<float> visual_stream_features = visual_features.face_embedding;
        std::vector<float> gaze_targets = visual_features.gaze_vector;
        
        // Create parallel streams
        bool audio_success = createAudioProcessingStream(audio_features);
        bool visual_success = createVisualProcessingStream(visual_stream_features);
        bool gaze_success = createGazeCoordinationStream(gaze_targets);
        
        // Synchronize streams if at least two are successful
        if ((audio_success + visual_success + gaze_success) >= 2) {
            return synchronizeMultimodalStreams(0.8f);
        }
        
        return false;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception in activateParallelNeuralStreams: " << e.what() << std::endl;
        return false;
    }
}

// Multimodal attention map processing
bool SubstrateLanguageIntegration::processMultimodalAttentionMap(
    const std::vector<float>& attention_weights,
    const std::vector<std::string>& active_modalities) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!is_active_.load()) {
        return false;
    }

    if (attention_weights.size() != active_modalities.size()) {
        return false;
    }

    // Apply attention modulation per modality
    for (std::size_t i = 0; i < active_modalities.size(); ++i) {
        const auto& modality = active_modalities[i];
        const float weight = attention_weights[i];

        if (modality == "audio") {
            std::lock_guard<std::mutex> audio_lock(audio_stream_mutex_);
            current_audio_stream_.activation_strength = std::max(0.0f, std::min(1.0f, current_audio_stream_.activation_strength * (0.5f + 0.5f * weight)));
        } else if (modality == "visual") {
            std::lock_guard<std::mutex> visual_lock(visual_stream_mutex_);
            current_visual_stream_.activation_strength = std::max(0.0f, std::min(1.0f, current_visual_stream_.activation_strength * (0.5f + 0.5f * weight)));
        } else if (modality == "gaze") {
            std::lock_guard<std::mutex> gaze_lock(gaze_stream_mutex_);
            current_gaze_stream_.activation_strength = std::max(0.0f, std::min(1.0f, current_gaze_stream_.activation_strength * (0.5f + 0.5f * weight)));
        }
    }

    // Update multimodal state strengths as a simple aggregate
    multimodal_state_.audio_activation_strength = current_audio_stream_.activation_strength;
    multimodal_state_.visual_activation_strength = current_visual_stream_.activation_strength;
    multimodal_state_.gaze_activation_strength = current_gaze_stream_.activation_strength;

    // Mark last sync time updated
    multimodal_state_.last_sync_time = std::chrono::steady_clock::now();

    return true;
}

// Multimodal stream coherence update
void SubstrateLanguageIntegration::updateMultimodalStreamCoherence(float delta_time) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!is_active_.load()) {
        return;
    }

    // Simple temporal smoothing toward synchronized state
    const float alpha = std::max(0.0f, std::min(1.0f, delta_time));
    const float mean_strength = (multimodal_state_.audio_activation_strength +
                                 multimodal_state_.visual_activation_strength +
                                 multimodal_state_.gaze_activation_strength) / 3.0f;

    multimodal_state_.audio_activation_strength = (1.0f - alpha) * multimodal_state_.audio_activation_strength + alpha * mean_strength;
    multimodal_state_.visual_activation_strength = (1.0f - alpha) * multimodal_state_.visual_activation_strength + alpha * mean_strength;
    multimodal_state_.gaze_activation_strength = (1.0f - alpha) * multimodal_state_.gaze_activation_strength + alpha * mean_strength;

    multimodal_state_.streams_synchronized = true; // assume convergence for placeholder implementation
    multimodal_state_.last_sync_time = std::chrono::steady_clock::now();
}

// Cross-modal neural state update
void SubstrateLanguageIntegration::updateCrossModalNeuralState(float delta_time) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!is_active_.load()) {
        return;
    }

    // Propagate activations across modalities with a simple decay toward uniformity
    const float propagation_strength = 0.3f;
    propagateActivationAcrossModalities(propagation_strength);

    // Apply minor decay dependent on delta_time
    const float decay = std::max(0.0f, std::min(1.0f, delta_time * 0.1f));
    multimodal_state_.audio_activation_strength *= (1.0f - decay * 0.1f);
    multimodal_state_.visual_activation_strength *= (1.0f - decay * 0.1f);
    multimodal_state_.gaze_activation_strength *= (1.0f - decay * 0.1f);

    multimodal_state_.last_sync_time = std::chrono::steady_clock::now();
}

// Calculate multimodal neural coherence
float SubstrateLanguageIntegration::calculateMultimodalNeuralCoherence() const {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    // Coherence as normalized variance complement across modality strengths
    const float a = multimodal_state_.audio_activation_strength;
    const float v = multimodal_state_.visual_activation_strength;
    const float g = multimodal_state_.gaze_activation_strength;
    const float mean = (a + v + g) / 3.0f;
    const float var = ((a - mean) * (a - mean) + (v - mean) * (v - mean) + (g - mean) * (g - mean)) / 3.0f;
    const float coherence = std::max(0.0f, 1.0f - var); // 0..1
    return coherence;
}

// Audio-visual binding integration
bool SubstrateLanguageIntegration::integrateAudioVisualBinding(
    const std::vector<float>& audio_pattern,
    const std::vector<float>& visual_pattern,
    float temporal_window) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!is_active_.load()) {
        return false;
    }

    if (audio_pattern.empty() || visual_pattern.empty() || audio_pattern.size() != visual_pattern.size()) {
        return false;
    }

    // Compute a simple similarity between patterns
    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_v = 0.0f;
    for (std::size_t i = 0; i < audio_pattern.size(); ++i) {
        dot += audio_pattern[i] * visual_pattern[i];
        norm_a += audio_pattern[i] * audio_pattern[i];
        norm_v += visual_pattern[i] * visual_pattern[i];
    }
    float denom = std::sqrt(std::max(1e-6f, norm_a)) * std::sqrt(std::max(1e-6f, norm_v));
    float similarity = denom > 0.0f ? dot / denom : 0.0f;

    // Update multimodal strength based on similarity and temporal window
    float binding_gain = std::max(0.0f, std::min(1.0f, similarity * (1.0f - std::abs(temporal_window - 0.2f))));
    multimodal_state_.audio_activation_strength = std::max(multimodal_state_.audio_activation_strength, binding_gain);
    multimodal_state_.visual_activation_strength = std::max(multimodal_state_.visual_activation_strength, binding_gain);
    multimodal_state_.streams_synchronized = multimodal_state_.streams_synchronized || (similarity > 0.5f);
    multimodal_state_.last_sync_time = std::chrono::steady_clock::now();

    return true;
}

// Statistics and monitoring
SubstrateLanguageIntegration::Statistics SubstrateLanguageIntegration::getStatistics() const {
    std::lock_guard<std::mutex> stats_lock(statistics_mutex_);
    return statistics_;
}

void SubstrateLanguageIntegration::resetStatistics() {
    std::lock_guard<std::mutex> stats_lock(statistics_mutex_);
    statistics_ = Statistics{};
}

float SubstrateLanguageIntegration::calculateIntegrationCoherence() const {
    // Simplified coherence calculation
    return 0.8f; // Placeholder value
}

std::string SubstrateLanguageIntegration::generateIntegrationReport() const {
    std::ostringstream report;
    report << "SubstrateLanguageIntegration Report:\n";
    report << "- Initialized: " << (is_initialized_.load() ? "Yes" : "No") << "\n";
    report << "- Active: " << (is_active_.load() ? "Yes" : "No") << "\n";
    report << "- Language regions created: " << (language_region_ ? "Yes" : "No") << "\n";
    report << "- Speech production regions created: " << (phoneme_region_ ? "Yes" : "No") << "\n";
    report << "- Multimodal regions created: " << (audio_stream_region_ ? "Yes" : "No") << "\n";
    
    auto stats = getStatistics();
    report << "- Total neural tokens: " << stats.total_neural_tokens << "\n";
    report << "- Active neural patterns: " << stats.active_neural_patterns << "\n";
    report << "- Integration coherence: " << calculateIntegrationCoherence() << "\n";
    
    return report.str();
}

// Configuration management
void SubstrateLanguageIntegration::updateConfig(const Config& new_config) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    config_ = new_config;
}

// Processing methods
void SubstrateLanguageIntegration::processSubstrateLanguageStep(float delta_time) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    
    if (!is_active_.load() || !is_initialized_.load()) {
        return;
    }
    
    // Apply language-specific learning
    applyLanguageSpecificLearning(delta_time);
    
    // Propagate language activations through substrate
    propagateLanguageActivations();
    
    // Update neural language representations
    updateNeuralLanguageRepresentations();
    
    // Update integration statistics
    updateIntegrationStatistics();
}

void SubstrateLanguageIntegration::applyLanguageSpecificLearning(float delta_time) {
    // Apply basic learning updates to language regions
    // This is a simplified implementation
    if (prosodic_region_) {
        // Apply prosodic learning
    }
    
    if (proto_word_region_) {
        // Apply proto-word learning
    }
}

void SubstrateLanguageIntegration::propagateLanguageActivations() {
    // Propagate activations from language system to neural substrate
    // This is a simplified implementation
}

void SubstrateLanguageIntegration::updateNeuralLanguageRepresentations() {
    // Update neural representations based on language system state
    // This is a simplified implementation
}

void SubstrateLanguageIntegration::optimizeNeuralBindings() {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    std::lock_guard<std::mutex> binding_lock(token_binding_mutex_);

    if (token_bindings_.empty()) {
        return;
    }

    for (auto &entry : token_bindings_) {
        auto &binding = entry.second;
        // Recalculate binding strength based on current coherence and recency
        binding.binding_strength = calculateBindingStrength(binding);
        // Light activation to keep assemblies responsive when configured active
        if (is_active_.load() && binding.binding_strength >= config_.neural_token_threshold) {
            activateNeuralAssembly(binding.assembly_neurons, binding.binding_strength);
        }
    }

    updateIntegrationStatistics();
}

void SubstrateLanguageIntegration::pruneInactiveBindings(float inactivity_threshold) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    std::lock_guard<std::mutex> binding_lock(token_binding_mutex_);

    if (token_bindings_.empty()) {
        return;
    }

    for (auto it = token_bindings_.begin(); it != token_bindings_.end(); ) {
        const auto &binding = it->second;
        float strength = binding.binding_strength;
        // Consider current computed strength; remove if below inactivity threshold
        if (strength < inactivity_threshold) {
            it = token_bindings_.erase(it);
            continue;
        }
        ++it;
    }

    updateIntegrationStatistics();
}

void SubstrateLanguageIntegration::consolidateNeuralPatterns() {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    std::lock_guard<std::mutex> pattern_lock(pattern_mutex_);

    if (proto_word_patterns_.empty()) {
        return;
    }

    for (auto &entry : proto_word_patterns_) {
        auto &pattern = entry.second;
        pattern.neural_stability = calculatePatternStability(pattern);
        if (pattern.neural_stability >= config_.pattern_recognition_threshold) {
            pattern.is_crystallized = true;
            strengthenNeuralConnections(pattern.pattern_synapses, config_.crystallization_neural_boost);
        }
    }

    updateIntegrationStatistics();
}

void SubstrateLanguageIntegration::updateIntegrationStatistics() {
    std::lock_guard<std::mutex> lock(statistics_mutex_);
    
    statistics_.total_neural_tokens = token_bindings_.size();
    statistics_.active_neural_patterns = proto_word_patterns_.size();
    statistics_.cross_modal_associations = grounding_associations_.size();
    
    // Calculate average binding strength
    if (!token_bindings_.empty()) {
        float total_strength = 0.0f;
        for (const auto& [token_id, binding] : token_bindings_) {
            total_strength += binding.binding_strength;
        }
        statistics_.average_binding_strength = total_strength / token_bindings_.size();
    }
    
    // Calculate integration coherence
    statistics_.substrate_language_coherence = calculateIntegrationCoherence();
    
    statistics_.neural_language_updates++;
    
    // Calculate integration efficiency
    if (statistics_.total_neural_tokens > 0) {
        statistics_.integration_efficiency = 
            (static_cast<float>(statistics_.crystallized_patterns) / statistics_.total_neural_tokens) * 
            statistics_.substrate_language_coherence;
    }
}

// Internal helpers
float SubstrateLanguageIntegration::calculateBindingStrength(const NeuralTokenBinding& binding) const {
    float coherence = measureNeuralCoherence(binding.assembly_neurons);
    float recency_factor = 0.0f;
    if (binding.activation_count > 0) {
        auto now = std::chrono::steady_clock::now();
        auto dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - binding.last_activation).count();
        // Recent activations boost strength; simple exponential decay
        const float tau = 5.0f; // seconds
        recency_factor = std::exp(-dt / tau);
    }
    // Weighted combination; clamp to [0,1]
    float strength = 0.7f * coherence + 0.3f * recency_factor;
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;
    return strength;
}

float SubstrateLanguageIntegration::calculatePatternStability(const NeuralProtoWordPattern& pattern) const {
    float coherence = measureNeuralCoherence(pattern.pattern_neurons);
    float stability = 0.6f * coherence + 0.4f * pattern.crystallization_strength;
    if (stability < 0.0f) stability = 0.0f;
    if (stability > 1.0f) stability = 1.0f;
    return stability;
}

void SubstrateLanguageIntegration::activateNeuralAssembly(const std::vector<NeuroForge::NeuronID>& assembly, float strength) {
    if (assembly.empty() || strength <= 0.0f) {
        return;
    }
    // Placeholder: activation routed via multimodal integration region if present
    // Concrete neuron-level activation is handled elsewhere in the substrate
}

void SubstrateLanguageIntegration::strengthenNeuralConnections(const std::vector<NeuroForge::SynapseID>& synapses, float factor) {
    if (synapses.empty() || factor <= 0.0f) {
        return;
    }
    // Placeholder: synaptic strengthening is handled by LearningSystem/HypergraphBrain
}

float SubstrateLanguageIntegration::measureNeuralCoherence(const std::vector<NeuroForge::NeuronID>& neurons) const {
    if (neurons.empty()) {
        return 0.0f;
    }
    // Simple heuristic coherence based on assembly size
    const float normalization = 8.0f;
    float coherence = static_cast<float>(neurons.size()) / normalization;
    if (coherence > 1.0f) coherence = 1.0f;
    return coherence;
}

// ===== Proto-word crystallization integration =====

bool SubstrateLanguageIntegration::createNeuralProtoWordPattern(
    const std::string& pattern,
    const std::vector<std::string>& phonemes) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!is_active_.load()) {
        return false;
    }

    std::lock_guard<std::mutex> pattern_lock(pattern_mutex_);
    NeuralProtoWordPattern& np = proto_word_patterns_[pattern];
    np.pattern_signature = pattern;

    // Aggregate neurons from known phoneme mappings to form a proto-word assembly
    {
        std::lock_guard<std::mutex> phoneme_lock(phoneme_mapping_mutex_);
        np.pattern_neurons.clear();
        for (const auto& ph : phonemes) {
            auto it = phoneme_mappings_.find(ph);
            if (it != phoneme_mappings_.end()) {
                const auto& asm_neurons = it->second.assembly_neurons;
                np.pattern_neurons.insert(np.pattern_neurons.end(), asm_neurons.begin(), asm_neurons.end());
            }
        }
    }

    // Initial crystallization strength based on count of contributing phonemes
    float init_strength = 0.0f;
    if (!phonemes.empty()) {
        init_strength = std::min(1.0f, static_cast<float>(phonemes.size()) * 0.1f);
    }
    np.crystallization_strength = std::max(np.crystallization_strength, init_strength);
    np.neural_stability = calculatePatternStability(np);
    np.is_crystallized = (np.neural_stability >= config_.pattern_recognition_threshold);

    // Optional synaptic strengthening on creation when stable enough
    if (np.is_crystallized && !np.pattern_synapses.empty()) {
        strengthenNeuralConnections(np.pattern_synapses, config_.crystallization_neural_boost);
    }

    updateIntegrationStatistics();
    return true;
}

bool SubstrateLanguageIntegration::reinforceNeuralPattern(const std::string& pattern, float reinforcement_strength) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    std::lock_guard<std::mutex> pattern_lock(pattern_mutex_);
    auto it = proto_word_patterns_.find(pattern);
    if (it == proto_word_patterns_.end()) {
        return false;
    }

    float r = std::max(0.0f, reinforcement_strength);
    float weighted = config_.proto_word_stdp_weight * r;
    it->second.crystallization_strength = std::min(1.0f, it->second.crystallization_strength + weighted);
    it->second.neural_stability = calculatePatternStability(it->second);
    if (it->second.neural_stability >= config_.pattern_recognition_threshold) {
        it->second.is_crystallized = true;
        if (!it->second.pattern_synapses.empty()) {
            strengthenNeuralConnections(it->second.pattern_synapses, 0.5f + 0.5f * r);
        }
    }

    updateIntegrationStatistics();
    return true;
}

bool SubstrateLanguageIntegration::crystallizeNeuralPattern(const std::string& pattern) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    std::lock_guard<std::mutex> pattern_lock(pattern_mutex_);
    auto it = proto_word_patterns_.find(pattern);
    if (it == proto_word_patterns_.end()) {
        return false;
    }

    // Recalculate stability and crystallize if threshold met
    it->second.neural_stability = calculatePatternStability(it->second);
    if (it->second.neural_stability >= config_.pattern_recognition_threshold) {
        it->second.is_crystallized = true;
        if (!it->second.pattern_synapses.empty()) {
            strengthenNeuralConnections(it->second.pattern_synapses, config_.crystallization_neural_boost);
        }
    }

    updateIntegrationStatistics();
    return it->second.is_crystallized;
}

SubstrateLanguageIntegration::NeuralProtoWordPattern* SubstrateLanguageIntegration::getNeuralPattern(const std::string& pattern) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    std::lock_guard<std::mutex> pattern_lock(pattern_mutex_);
    auto it = proto_word_patterns_.find(pattern);
    if (it == proto_word_patterns_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> SubstrateLanguageIntegration::getCrystallizedPatterns() const {
    std::vector<std::string> result;
    std::lock_guard<std::mutex> pattern_lock(pattern_mutex_);
    for (const auto& kv : proto_word_patterns_) {
        if (kv.second.is_crystallized) {
            result.push_back(kv.first);
        }
    }
    return result;
}

// ===== Attention modulation bridge =====

void SubstrateLanguageIntegration::modulateAttentionForLanguageLearning(
    const std::unordered_map<NeuroForge::NeuronID, float>& attention_map) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!config_.enable_attention_modulation || attention_map.empty()) {
        return;
    }

    // Modulate token bindings based on attention over their assemblies
    {
        std::lock_guard<std::mutex> binding_lock(token_binding_mutex_);
        for (auto& entry : token_bindings_) {
            auto& binding = entry.second;
            float accum = 0.0f;
            for (const auto& nid : binding.assembly_neurons) {
                auto it = attention_map.find(nid);
                if (it != attention_map.end()) {
                    accum += std::max(0.0f, it->second);
                }
            }
            if (accum > 0.0f) {
                float delta = config_.language_learning_rate * accum;
                binding.binding_strength = std::min(1.0f, binding.binding_strength + delta);
            }
        }
    }

    // Modulate proto-word patterns based on attention to their neurons
    {
        std::lock_guard<std::mutex> pattern_lock(pattern_mutex_);
        for (auto& kv : proto_word_patterns_) {
            auto& pat = kv.second;
            float accum = 0.0f;
            for (const auto& nid : pat.pattern_neurons) {
                auto it = attention_map.find(nid);
                if (it != attention_map.end()) {
                    accum += std::max(0.0f, it->second);
                }
            }
            if (accum > 0.0f) {
                float delta = config_.proto_word_stdp_weight * accum;
                pat.crystallization_strength = std::min(1.0f, pat.crystallization_strength + delta);
                pat.neural_stability = calculatePatternStability(pat);
                if (pat.neural_stability >= config_.pattern_recognition_threshold) {
                    pat.is_crystallized = true;
                    if (!pat.pattern_synapses.empty()) {
                        strengthenNeuralConnections(pat.pattern_synapses, 0.3f + 0.7f * std::min(1.0f, accum));
                    }
                }
            }
        }
    }

    // Forward external attention map to LearningSystem for plasticity modulation
    if (learning_system_) {
        // Pass 0.0f to defer to LearningSystem's configured boost factor
        learning_system_->applyAttentionModulation(attention_map, 0.0f);
    }

    updateIntegrationStatistics();
}

// ===== Missing cross-modal coordination and prosody implementations =====

bool SubstrateLanguageIntegration::establishCrossModalConnections() {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!is_active_.load() || !hypergraph_brain_) {
        std::cerr << "establishCrossModalConnections: skipped (inactive or hypergraph_brain_ missing)" << std::endl;
        return false;
    }

    // Ensure required regions exist
    if (!audio_stream_region_ || !visual_stream_region_ || !multimodal_integration_region_) {
        std::cerr << "establishCrossModalConnections: missing required regions"
                  << " audio=" << (audio_stream_region_ ? "ok" : "null")
                  << " visual=" << (visual_stream_region_ ? "ok" : "null")
                  << " integration=" << (multimodal_integration_region_ ? "ok" : "null")
                  << std::endl;
        return false;
    }

    try {
        // Create baseline connections among audio, visual, and integration regions
        hypergraph_brain_->connectRegions(audio_stream_region_->getId(), visual_stream_region_->getId(), 0.5f);
        hypergraph_brain_->connectRegions(visual_stream_region_->getId(), audio_stream_region_->getId(), 0.5f);
        hypergraph_brain_->connectRegions(audio_stream_region_->getId(), multimodal_integration_region_->getId(), 0.8f);
        hypergraph_brain_->connectRegions(visual_stream_region_->getId(), multimodal_integration_region_->getId(), 0.8f);
        if (gaze_coordination_region_) {
            hypergraph_brain_->connectRegions(gaze_coordination_region_->getId(), multimodal_integration_region_->getId(), 0.6f);
            hypergraph_brain_->connectRegions(gaze_coordination_region_->getId(), visual_stream_region_->getId(), 0.6f);
        }

        // Update statistics
        std::cerr << "establishCrossModalConnections: baseline links established" << std::endl;
        updateIntegrationStatistics();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in establishCrossModalConnections: " << e.what() << std::endl;
        return false;
    }
}

bool SubstrateLanguageIntegration::reinforceCrossModalBinding(const std::string& modality_a,
                                                              const std::string& modality_b,
                                                              float binding_strength) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!is_active_.load() || !hypergraph_brain_) {
        std::cerr << "reinforceCrossModalBinding: skipped (inactive or hypergraph_brain_ missing)" << std::endl;
        return false;
    }

    // Determine regions by modality name
    auto getRegionByName = [&](const std::string& name) -> NeuroForge::RegionPtr {
        if (name == "audio") return audio_stream_region_;
        if (name == "visual") return visual_stream_region_;
        if (name == "gaze") return gaze_coordination_region_;
        if (name == "integration") return multimodal_integration_region_;
        return nullptr;
    };

    NeuroForge::RegionPtr a = getRegionByName(modality_a);
    NeuroForge::RegionPtr b = getRegionByName(modality_b);
    if (!a || !b) {
        std::cerr << "reinforceCrossModalBinding: unknown modality names a='" << modality_a
                  << "' or b='" << modality_b << "'" << std::endl;
        return false;
    }

    try {
        float clamped = std::max(0.0f, std::min(1.0f, binding_strength));
        hypergraph_brain_->connectRegions(a->getId(), b->getId(), clamped);
        // Light activation to reflect reinforcement
        multimodal_state_.streams_synchronized = false;
        std::cerr << "reinforceCrossModalBinding: connected '" << modality_a << "' to '" << modality_b
                  << "' with strength=" << clamped << std::endl;
        updateIntegrationStatistics();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in reinforceCrossModalBinding: " << e.what() << std::endl;
        return false;
    }
}

void SubstrateLanguageIntegration::propagateActivationAcrossModalities(float propagation_strength) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!is_active_.load()) {
        return;
    }

    float alpha = std::max(0.0f, std::min(1.0f, propagation_strength));
    // Move each modality's activation toward the mean by factor alpha
    float mean = (multimodal_state_.audio_activation_strength +
                  multimodal_state_.visual_activation_strength +
                  multimodal_state_.gaze_activation_strength) / 3.0f;

    multimodal_state_.audio_activation_strength = (1.0f - alpha) * multimodal_state_.audio_activation_strength + alpha * mean;
    multimodal_state_.visual_activation_strength = (1.0f - alpha) * multimodal_state_.visual_activation_strength + alpha * mean;
    multimodal_state_.gaze_activation_strength = (1.0f - alpha) * multimodal_state_.gaze_activation_strength + alpha * mean;
}

bool SubstrateLanguageIntegration::processJointAttentionNeurally(const std::vector<float>& shared_attention_target,
                                                                const std::string& associated_token) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!is_active_.load()) {
        std::cerr << "processJointAttentionNeurally: skipped (integration inactive)" << std::endl;
        return false;
    }

    // Simple heuristic: boost visual and gaze activation if joint attention is present
    if (!shared_attention_target.empty()) {
        float target_salience = 0.0f;
        for (float v : shared_attention_target) target_salience += std::abs(v);
        target_salience /= static_cast<float>(shared_attention_target.size());

        float boost = std::max(0.0f, std::min(1.0f, target_salience));
        multimodal_state_.visual_activation_strength = std::min(1.0f, multimodal_state_.visual_activation_strength + 0.3f * boost);
        multimodal_state_.gaze_activation_strength = std::min(1.0f, multimodal_state_.gaze_activation_strength + 0.3f * boost);
        std::cerr << "processJointAttentionNeurally: boosted visual/gaze by " << (0.3f * boost)
                  << " (token='" << associated_token << "')" << std::endl;
    }

    // Optionally reinforce binding with integration region when a token is associated
    if (!associated_token.empty() && multimodal_integration_region_) {
        // Lightweight reinforcement toward integration to reflect joint context
        if (hypergraph_brain_ && visual_stream_region_) {
            hypergraph_brain_->connectRegions(visual_stream_region_->getId(), multimodal_integration_region_->getId(), 0.7f);
        } else {
            std::cerr << "processJointAttentionNeurally: skipping visual->integration reinforcement (brain or region missing)" << std::endl;
        }
        if (hypergraph_brain_ && gaze_coordination_region_) {
            hypergraph_brain_->connectRegions(gaze_coordination_region_->getId(), multimodal_integration_region_->getId(), 0.7f);
        }
    }

    updateIntegrationStatistics();
    return true;
}

bool SubstrateLanguageIntegration::processProsodicPatternNeurally(const LanguageSystem::AcousticFeatures& features,
                                                                  const std::string& co_occurring_token) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!is_active_.load()) {
        std::cerr << "processProsodicPatternNeurally: skipped (integration inactive)" << std::endl;
        return false;
    }

    // Compute salience and create a named prosody pattern based on token if provided
    float salience = calculateNeuralProsodicSalience(features);
    std::string pattern_name = co_occurring_token.empty() ? std::string("prosody_pattern") : ("prosody:" + co_occurring_token);

    // Convert scalar acoustic features into minimal contours for mapping
    std::vector<float> pitch_vec{features.pitch_contour};
    std::vector<float> energy_vec{features.energy_envelope};
    std::vector<float> rhythm_vec{features.rhythm_pattern};

    bool mapped = mapProsodyToNeuralPattern(pattern_name, pitch_vec, energy_vec, rhythm_vec);
    if (!mapped) {
        std::cerr << "processProsodicPatternNeurally: mapping failed for pattern '" << pattern_name << "'" << std::endl;
        return false;
    }

    // Reinforce prosody pattern proportionally to salience
    reinforceProsodicNeuralPattern(pattern_name, salience);
    std::cerr << "processProsodicPatternNeurally: reinforced pattern '" << pattern_name << "' with salience=" << salience << std::endl;
    updateIntegrationStatistics();
    return true;
}

bool SubstrateLanguageIntegration::reinforceProsodicNeuralPattern(const std::string& pattern_name, float reinforcement) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    std::lock_guard<std::mutex> prosody_lock(prosody_pattern_mutex_);
    auto it = prosody_patterns_.find(pattern_name);
    if (it == prosody_patterns_.end()) {
        std::cerr << "reinforceProsodicNeuralPattern: pattern not found '" << pattern_name << "'" << std::endl;
        return false;
    }

    float r = std::max(0.0f, reinforcement);
    it->second.pattern_strength = std::min(1.0f, it->second.pattern_strength + r * 0.5f);
    it->second.stability = std::min(1.0f, it->second.stability + r * 0.3f);
    it->second.is_stable = (it->second.stability >= config_.pattern_recognition_threshold);
    it->second.last_reinforcement = std::chrono::steady_clock::now();

    // Strengthen synapses if available
    if (!it->second.pattern_synapses.empty()) {
        strengthenNeuralConnections(it->second.pattern_synapses, 0.2f + 0.6f * r);
    }

    std::cerr << "reinforceProsodicNeuralPattern: updated pattern '" << pattern_name
              << "' strength=" << it->second.pattern_strength
              << " stability=" << it->second.stability << std::endl;
    return true;
}

float SubstrateLanguageIntegration::calculateNeuralProsodicSalience(const LanguageSystem::AcousticFeatures& features) const {
    // Simple salience measure based on absolute magnitudes of scalar features
    float pitch_s = std::abs(features.pitch_contour);
    float energy_s = std::abs(features.energy_envelope);
    float rhythm_s = std::abs(features.rhythm_pattern);

    // Include intonation and spectral centroid as modifiers if available
    float modifier = 0.0f;
    modifier += std::abs(features.intonation_slope);
    modifier += std::abs(features.spectral_centroid);
    modifier += std::abs(features.voicing_strength);

    float salience = 0.5f * pitch_s + 0.3f * energy_s + 0.2f * rhythm_s + 0.1f * modifier;
    if (salience > 1.0f) salience = 1.0f;
    if (salience < 0.0f) salience = 0.0f;
    return salience;
}


bool SubstrateLanguageIntegration::createNeuralGroundingAssociation(
    std::size_t grounding_id,
    const std::vector<float>& visual_features,
    const std::vector<float>& auditory_features,
    const std::vector<float>& language_features) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    if (!is_active_.load()) {
        std::cerr << "createNeuralGroundingAssociation: skipped (integration inactive)" << std::endl;
        return false;
    }

    // Ensure required regions exist
    if (!visual_stream_region_ || !audio_stream_region_ || !language_region_) {
        std::cerr << "createNeuralGroundingAssociation: missing required regions"
                  << " visual=" << (visual_stream_region_ ? "ok" : "null")
                  << " audio=" << (audio_stream_region_ ? "ok" : "null")
                  << " language=" << (language_region_ ? "ok" : "null")
                  << std::endl;
        return false;
    }

    if (grounding_associations_.find(grounding_id) != grounding_associations_.end()) {
        std::cerr << "createNeuralGroundingAssociation: overwriting existing association id=" << grounding_id << std::endl;
    }

    auto computeWeight = [](const std::vector<float>& vec) -> float {
        if (vec.empty()) return 0.0f;
        float sum = 0.0f;
        for (float v : vec) sum += std::abs(v);
        float w = sum / static_cast<float>(vec.size());
        if (w < 0.0f) w = 0.0f;
        if (w > 1.0f) w = 1.0f;
        return w;
    };

    NeuralGroundingAssociation assoc;
    assoc.grounding_id = grounding_id;
    assoc.visual_region = visual_stream_region_->getId();
    assoc.auditory_region = audio_stream_region_->getId();
    assoc.language_region = language_region_->getId();

    // Compute modality weights from provided feature magnitudes
    float vw = computeWeight(visual_features);
    float aw = computeWeight(auditory_features);
    float lw = computeWeight(language_features);
    assoc.modality_weights["visual"] = vw;
    assoc.modality_weights["auditory"] = aw;
    assoc.modality_weights["language"] = lw;

    // Overall association strength as average of modality weights (clamped)
    float strength = (vw + aw + lw) / 3.0f;
    if (strength < 0.0f) strength = 0.0f;
    if (strength > 1.0f) strength = 1.0f;
    // Bias toward configured default when features are weak
    if (strength < 0.001f) {
        strength = std::max(0.0f, std::min(1.0f, config_.grounding_association_strength));
    }
    assoc.association_strength = strength;

    // Establish cross-modal connections reflecting this association (best-effort)
    if (hypergraph_brain_) {
        try {
            hypergraph_brain_->connectRegions(assoc.visual_region, assoc.auditory_region, strength);
            hypergraph_brain_->connectRegions(assoc.visual_region, assoc.language_region, strength);
            hypergraph_brain_->connectRegions(assoc.auditory_region, assoc.language_region, strength);
            if (multimodal_integration_region_) {
                hypergraph_brain_->connectRegions(multimodal_integration_region_->getId(), assoc.language_region, 0.5f + 0.5f * strength);
            }
            if (grounding_region_) {
                hypergraph_brain_->connectRegions(grounding_region_->getId(), assoc.language_region, 0.5f + 0.5f * strength);
            }
            std::cerr << "createNeuralGroundingAssociation: connected regions V/A/L with strength=" << strength << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Exception in createNeuralGroundingAssociation connections: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "createNeuralGroundingAssociation: hypergraph_brain_ missing, skipping connectivity" << std::endl;
    }

    grounding_associations_[grounding_id] = std::move(assoc);
    std::cerr << "createNeuralGroundingAssociation: stored association id=" << grounding_id << " strength=" << strength << std::endl;
    updateIntegrationStatistics();
    return true;
}

bool SubstrateLanguageIntegration::strengthenGroundingAssociation(std::size_t grounding_id, float strength_boost) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);

    auto it = grounding_associations_.find(grounding_id);
    if (it == grounding_associations_.end()) {
        std::cerr << "strengthenGroundingAssociation: association not found id=" << grounding_id << std::endl;
        return false;
    }

    float boost = std::max(0.0f, strength_boost);
    it->second.association_strength = std::min(1.0f, it->second.association_strength + boost);

    // Modestly increase per-modality weights to reflect reinforcement
    for (auto& kv : it->second.modality_weights) {
        kv.second = std::min(1.0f, kv.second + 0.5f * boost);
    }

    // Reinforce region connectivity according to updated strength
    if (hypergraph_brain_) {
        try {
            float s = it->second.association_strength;
            NeuroForge::RegionID v = it->second.visual_region;
            NeuroForge::RegionID a = it->second.auditory_region;
            NeuroForge::RegionID l = it->second.language_region;
            hypergraph_brain_->connectRegions(v, a, s);
            hypergraph_brain_->connectRegions(v, l, s);
            hypergraph_brain_->connectRegions(a, l, s);
            std::cerr << "strengthenGroundingAssociation: reinforced connectivity id=" << grounding_id << " strength=" << s << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Exception in strengthenGroundingAssociation: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "strengthenGroundingAssociation: hypergraph_brain_ missing, skipping connectivity" << std::endl;
    }

    updateIntegrationStatistics();
    return true;
}

SubstrateLanguageIntegration::NeuralGroundingAssociation* SubstrateLanguageIntegration::getGroundingAssociation(std::size_t grounding_id) {
    std::lock_guard<std::recursive_mutex> lock(integration_mutex_);
    auto it = grounding_associations_.find(grounding_id);
    if (it == grounding_associations_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::size_t> SubstrateLanguageIntegration::getStableGroundingAssociations(float threshold) const {
    std::vector<std::size_t> result;
    for (const auto& entry : grounding_associations_) {
        if (entry.second.association_strength >= threshold) {
            result.push_back(entry.first);
        }
    }
    return result;
}


} // namespace Core
} // namespace NeuroForge
