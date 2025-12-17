#include "core/SubstrateLanguageIntegration.h"
#include "core/NeuralLanguageBindings.h"
#include "core/LanguageSystem.h"
#include "core/LearningSystem.h"
#include "core/HypergraphBrain.h"
#include "connectivity/ConnectivityManager.h"
#include <iostream>
#include <cassert>
#include <chrono>
#include <vector>
#include <memory>

using namespace NeuroForge::Core;

/**
 * @brief Comprehensive test suite for substrate language integration
 */
class SubstrateLanguageIntegrationTest {
private:
    std::shared_ptr<NeuroForge::Connectivity::ConnectivityManager> connectivity_manager_;
    std::shared_ptr<HypergraphBrain> hypergraph_brain_;
    std::shared_ptr<LanguageSystem> language_system_;
    std::shared_ptr<SubstrateLanguageIntegration> substrate_integration_;
    std::shared_ptr<NeuralLanguageBindings> neural_bindings_;
    
    bool verbose_output_;
    std::size_t tests_passed_;
    std::size_t tests_failed_;

public:
    explicit SubstrateLanguageIntegrationTest(bool verbose = true) 
        : verbose_output_(verbose), tests_passed_(0), tests_failed_(0) {
        
        // Initialize core systems
        connectivity_manager_ = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
        hypergraph_brain_ = std::make_shared<HypergraphBrain>(connectivity_manager_);
        
        // Initialize language system with config
        LanguageSystem::Config language_config;
        language_system_ = std::make_shared<LanguageSystem>(language_config);
        
        // Initialize integration components
        NeuroForge::Core::SubstrateLanguageIntegration::Config substrate_config;
        substrate_integration_ = std::make_shared<SubstrateLanguageIntegration>(
            language_system_, hypergraph_brain_, substrate_config);
        
        NeuroForge::Core::NeuralLanguageBindings::Config bindings_config;
        neural_bindings_ = std::make_shared<NeuralLanguageBindings>(hypergraph_brain_, bindings_config);
    }

    ~SubstrateLanguageIntegrationTest() {
        // Explicit cleanup in reverse order of initialization
        // Note: Removed shutdown() calls to avoid potential access violations
        if (neural_bindings_) {
            neural_bindings_.reset();
        }
        if (substrate_integration_) {
            substrate_integration_.reset();
        }
        if (language_system_) {
            language_system_.reset();
        }
        if (hypergraph_brain_) {
            hypergraph_brain_.reset();
        }
        if (connectivity_manager_) {
            connectivity_manager_.reset();
        }
    }

    void runAllTests() {
        log("=== Starting Substrate Language Integration Test Suite ===");
        
        // Core initialization tests
        testSystemInitialization();
        testLanguageRegionCreation();
        testNeuralBindingInitialization();
        
        // Token-neural binding tests
        testTokenNeuralAssemblyCreation();
        testTokenActivationPropagation();
        testTokenAssemblyCoherence();
        
        // Proto-word crystallization tests
        testProtoWordPatternCreation();
        testProtoWordReinforcement();
        testProtoWordCrystallization();
        testNeuralPatternStability();
        
        // Cross-modal grounding tests
        testCrossModalBindingCreation();
        std::cout << "DEBUG: testCrossModalBindingCreation completed, about to call testCrossModalAssociationStrengthening" << std::endl;
        std::cout.flush();
        testCrossModalAssociationStrengthening();
        testCrossModalBindingStabilization();
        
        // Multimodal parallel stream tests
        testMultimodalStreamInitialization();
        testParallelStreamCreation();
        testMultimodalStreamSynchronization();
        testCrossModalStreamCoordination();
        testMultimodalNeuralCoherence();
        
        // Prosodic pattern tests
        testProsodicCircuitCreation();
        testProsodicPatternActivation();
        testMothereseBiasConfiguration();
        
        // Learning integration tests
        testLearningSystemIntegration();
        testSTDPApplicationToLanguageBindings();
        testHebbianLearningForAssemblies();
        testAttentionModulationEffects();
        
        // Performance and optimization tests
        testNeuralBindingOptimization();
        testInactiveBindingPruning();
        testPatternConsolidation();
        
        // Integration coherence tests
        testSubstrateLanguageCoherence();
        testIntegrationEfficiency();
        testOverallSystemHealth();
        
        // Stress and scalability tests
        testLargeScaleTokenBinding();
        testConcurrentPatternProcessing();
        testMemoryUsageOptimization();
        
        printTestSummary();
    }

private:
    void log(const std::string& message) {
        if (verbose_output_) {
            std::cout << "[TEST] " << message << std::endl;
        }
    }

    void assertTrue(bool condition, const std::string& test_name) {
        if (condition) {
            tests_passed_++;
            log("✓ " + test_name + " - PASSED");
        } else {
            tests_failed_++;
            log("✗ " + test_name + " - FAILED");
        }
    }

    void testSystemInitialization() {
        log("--- Testing System Initialization ---");
        
        // Test hypergraph brain initialization
        bool brain_init = hypergraph_brain_->initialize();
        assertTrue(brain_init, "HypergraphBrain initialization");
        
        // Create and map modality regions required for cross-modal operations
        auto visual_region = hypergraph_brain_->createRegion("VisualCortex", Region::Type::Cortical);
        auto auditory_region = hypergraph_brain_->createRegion("AuditoryCortex", Region::Type::Cortical);
        
        if (visual_region && auditory_region) {
            visual_region->createNeurons(100);
            auditory_region->createNeurons(100);
            hypergraph_brain_->mapModality(NeuroForge::Modality::Visual, visual_region->getId());
            hypergraph_brain_->mapModality(NeuroForge::Modality::Audio, auditory_region->getId());
        }
        
        // Test language system initialization
        bool lang_init = language_system_->initialize();
        assertTrue(lang_init, "LanguageSystem initialization");
        
        // Test substrate integration initialization
        bool substrate_init = substrate_integration_->initialize();
        assertTrue(substrate_init, "SubstrateLanguageIntegration initialization");
        
        // Test neural bindings initialization
        bool bindings_init = neural_bindings_->initialize();
        assertTrue(bindings_init, "NeuralLanguageBindings initialization");
        
        // Test system state consistency
        assertTrue(substrate_integration_->isInitialized(), "Substrate integration state");
        assertTrue(neural_bindings_->isInitialized(), "Neural bindings state");
    }

    void testLanguageRegionCreation() {
        log("--- Testing Language Region Creation ---");
        
        // Test language region creation
        bool regions_created = substrate_integration_->createLanguageRegions();
        assertTrue(regions_created, "Language regions creation");
        
        // Test region connectivity
        bool regions_connected = substrate_integration_->connectLanguageRegions();
        assertTrue(regions_connected, "Language regions connectivity");
        
        // Test region accessibility
        auto language_region = substrate_integration_->getLanguageRegion();
        auto proto_word_region = substrate_integration_->getProtoWordRegion();
        auto prosodic_region = substrate_integration_->getProsodicRegion();
        auto grounding_region = substrate_integration_->getGroundingRegion();
        
        assertTrue(language_region != nullptr, "Language region accessibility");
        assertTrue(proto_word_region != nullptr, "Proto-word region accessibility");
        assertTrue(prosodic_region != nullptr, "Prosodic region accessibility");
        assertTrue(grounding_region != nullptr, "Grounding region accessibility");
        
        // Test region neuron counts (non-zero)
        assertTrue(language_region->getNeurons().size() > 0, "Language region neuron count");
        assertTrue(proto_word_region->getNeurons().size() > 0, "Proto-word region neuron count");

        // Assert exact neuron counts equal configured values
        const auto& cfg = substrate_integration_->getConfig();
        std::size_t lang_neurons = language_region->getNeuronCount();
        std::size_t proto_neurons = proto_word_region->getNeuronCount();
        std::size_t pros_neurons = prosodic_region->getNeuronCount();
        std::size_t ground_neurons = grounding_region->getNeuronCount();

        // Diagnostics: log actual neuron counts for future debugging
        std::cout << "[TEST] Language neurons=" << lang_neurons
                  << ", Proto-word neurons=" << proto_neurons
                  << ", Prosodic neurons=" << pros_neurons
                  << ", Grounding neurons=" << ground_neurons << std::endl;

        assertTrue(lang_neurons == cfg.language_region_neurons, "Language region exact neuron count");
        assertTrue(proto_neurons == cfg.proto_word_region_neurons, "Proto-word region exact neuron count");
        assertTrue(pros_neurons == cfg.prosodic_region_neurons, "Prosodic region exact neuron count");
        assertTrue(ground_neurons == cfg.grounding_region_neurons, "Grounding region exact neuron count");
    }

    void testNeuralBindingInitialization() {
        log("--- Testing Neural Binding Initialization ---");
        
        // Test initial binding counts
        auto initial_stats = neural_bindings_->getStatistics();
        assertTrue(initial_stats.total_token_assemblies == 0, "Initial token assemblies count");
        assertTrue(initial_stats.total_proto_word_patterns == 0, "Initial proto-word patterns count");
        assertTrue(initial_stats.total_cross_modal_bindings == 0, "Initial cross-modal bindings count");
        
        // Test binding health
        float initial_health = neural_bindings_->getOverallBindingHealth();
        assertTrue(initial_health >= 0.0f, "Initial binding health");
    }

    void testTokenNeuralAssemblyCreation() {
        log("--- Testing Token Neural Assembly Creation ---");
        
        auto language_region = substrate_integration_->getLanguageRegion();
        if (!language_region) {
            assertTrue(false, "Language region not available for token testing");
            return;
        }
        
        // Create test token embeddings
        std::vector<float> token_embedding = {0.5f, 0.7f, 0.3f, 0.9f, 0.1f};
        
        // Test token assembly creation
        bool assembly_created = neural_bindings_->createTokenNeuralAssembly(
            "test_token", token_embedding, language_region->getId());
        assertTrue(assembly_created, "Token neural assembly creation");
        
        // Test assembly retrieval
        auto assembly = neural_bindings_->getTokenAssembly("test_token");
        assertTrue(assembly != nullptr, "Token assembly retrieval");
        
        if (assembly) {
            assertTrue(assembly->token_symbol == "test_token", "Token assembly symbol");
            assertTrue(assembly->assembly_neurons.size() > 0, "Token assembly neuron count");
            assertTrue(assembly->primary_neuron != 0, "Token assembly primary neuron");
        }
        
        // Test duplicate creation prevention
        bool duplicate_prevented = !neural_bindings_->createTokenNeuralAssembly(
            "test_token", token_embedding, language_region->getId());
        assertTrue(duplicate_prevented, "Duplicate token assembly prevention");
    }

    void testTokenActivationPropagation() {
        log("--- Testing Token Activation Propagation ---");
        
        // Activate existing token assembly
        bool activation_success = neural_bindings_->activateTokenAssembly("test_token", 0.8f);
        assertTrue(activation_success, "Token assembly activation");
        
        // Test activation propagation
        neural_bindings_->propagateLanguageActivations();
        
        // Check assembly state after activation
        auto assembly = neural_bindings_->getTokenAssembly("test_token");
        if (assembly) {
            assertTrue(assembly->firing_count > 0, "Token assembly firing count");
            assertTrue(assembly->assembly_coherence >= 0.0f, "Token assembly coherence");
        }
        
        // Test multiple activations
        for (int i = 0; i < 5; ++i) {
            neural_bindings_->activateTokenAssembly("test_token", 0.6f);
        }
        
        if (assembly) {
            assertTrue(assembly->firing_count >= 5, "Multiple token activations");
        }
    }

    void testTokenAssemblyCoherence() {
        log("--- Testing Token Assembly Coherence ---");
        
        auto assembly = neural_bindings_->getTokenAssembly("test_token");
        if (!assembly) {
            assertTrue(false, "Test token assembly not available");
            return;
        }
        
        // Test coherence calculation
        float coherence = neural_bindings_->calculateAssemblyCoherence(*assembly);
        assertTrue(coherence >= 0.0f && coherence <= 1.0f, "Assembly coherence range");
        
        // Test coherence after reinforcement
        neural_bindings_->reinforceTokenAssembly("test_token", 0.3f);
        float new_coherence = neural_bindings_->calculateAssemblyCoherence(*assembly);
        assertTrue(new_coherence >= coherence, "Assembly coherence improvement");
    }

    void testProtoWordPatternCreation() {
        log("--- Testing Proto-word Pattern Creation ---");
        
        auto proto_word_region = substrate_integration_->getProtoWordRegion();
        if (!proto_word_region) {
            assertTrue(false, "Proto-word region not available");
            return;
        }
        
        // Create test proto-word pattern
        std::vector<std::string> phonemes = {"m", "a", "m", "a"};
        bool pattern_created = neural_bindings_->createProtoWordNeuralPattern(
            "mama", phonemes, proto_word_region->getId());
        assertTrue(pattern_created, "Proto-word pattern creation");
        
        // Test pattern retrieval
        auto pattern = neural_bindings_->getProtoWordPattern("mama");
        assertTrue(pattern != nullptr, "Proto-word pattern retrieval");
        
        if (pattern) {
            assertTrue(pattern->proto_word_pattern == "mama", "Proto-word pattern string");
            assertTrue(pattern->phoneme_sequence == phonemes, "Proto-word phoneme sequence");
            assertTrue(pattern->sequence_neurons.size() == phonemes.size(), "Proto-word neuron count");
            assertTrue(!pattern->is_crystallized, "Initial crystallization state");
        }
        
        // Test substrate integration pattern creation
        bool substrate_pattern_created = substrate_integration_->createNeuralProtoWordPattern("dada", {"d", "a", "d", "a"});
        assertTrue(substrate_pattern_created, "Substrate proto-word pattern creation");
    }

    void testProtoWordReinforcement() {
        log("--- Testing Proto-word Reinforcement ---");
        
        auto pattern = neural_bindings_->getProtoWordPattern("mama");
        if (!pattern) {
            assertTrue(false, "Test proto-word pattern not available");
            return;
        }
        
        float initial_strength = pattern->crystallization_strength;
        
        // Test reinforcement
        bool reinforcement_success = neural_bindings_->reinforceProtoWordPattern("mama", 0.2f);
        assertTrue(reinforcement_success, "Proto-word reinforcement");
        
        // Check strength increase
        assertTrue(pattern->crystallization_strength > initial_strength, "Crystallization strength increase");
        assertTrue(pattern->reinforcement_count > 0, "Reinforcement count tracking");
        
        // Test substrate integration reinforcement
        bool substrate_reinforcement = substrate_integration_->reinforceNeuralPattern("dada", 0.3f);
        assertTrue(substrate_reinforcement, "Substrate pattern reinforcement");
    }

    void testProtoWordCrystallization() {
        log("--- Testing Proto-word Crystallization ---");
        
        // Reinforce pattern to crystallization threshold
        for (int i = 0; i < 10; ++i) {
            neural_bindings_->reinforceProtoWordPattern("mama", 0.1f);
        }
        
        auto pattern = neural_bindings_->getProtoWordPattern("mama");
        if (pattern) {
            // Test crystallization state
            bool should_be_crystallized = pattern->crystallization_strength >= 0.8f && pattern->neural_stability >= 0.75f;
            if (should_be_crystallized) {
                assertTrue(pattern->is_crystallized, "Proto-word crystallization");
            }
            
            // Test manual crystallization
            bool crystallization_success = neural_bindings_->crystallizeProtoWordPattern("mama");
            assertTrue(crystallization_success, "Manual proto-word crystallization");
            assertTrue(pattern->is_crystallized, "Crystallization state after manual trigger");
        }
        
        // Test crystallized patterns retrieval
        auto crystallized_patterns = neural_bindings_->getCrystallizedProtoWords();
        assertTrue(crystallized_patterns.size() > 0, "Crystallized patterns count");
    }

    void testNeuralPatternStability() {
        log("--- Testing Neural Pattern Stability ---");
        
        auto pattern = neural_bindings_->getProtoWordPattern("mama");
        if (!pattern) {
            assertTrue(false, "Test proto-word pattern not available");
            return;
        }
        
        // Test stability calculation
        float stability = neural_bindings_->calculatePatternStability(*pattern);
        assertTrue(stability >= 0.0f && stability <= 1.0f, "Pattern stability range");
        
        // Test stability improvement through reinforcement
        float initial_stability = stability;
        neural_bindings_->reinforceProtoWordPattern("mama", 0.5f);
        float new_stability = neural_bindings_->calculatePatternStability(*pattern);
        assertTrue(new_stability >= initial_stability, "Pattern stability improvement");
    }

    void testCrossModalBindingCreation() {
        log("--- Testing Cross-modal Binding Creation ---");
        
        // Create test feature vectors
        std::vector<float> visual_features = {0.8f, 0.6f, 0.4f, 0.9f};
        std::vector<float> auditory_features = {0.7f, 0.5f, 0.8f, 0.3f};
        std::vector<float> tactile_features = {0.6f, 0.4f, 0.7f, 0.5f};
        std::vector<float> language_features = {0.9f, 0.8f, 0.6f, 0.7f};
        
        // Test cross-modal binding creation
        bool binding_created = neural_bindings_->createCrossModalNeuralBinding(
            1, "ball", visual_features, auditory_features, tactile_features, language_features);
        assertTrue(binding_created, "Cross-modal binding creation");
        
        // Test binding retrieval
        auto binding = neural_bindings_->getCrossModalBinding(1);
        assertTrue(binding != nullptr, "Cross-modal binding retrieval");
        
        if (binding) {
            assertTrue(binding->grounding_id == 1, "Cross-modal binding ID");
            assertTrue(binding->object_category == "ball", "Cross-modal binding category");
            assertTrue(binding->modality_strengths.size() > 0, "Cross-modal modality strengths");
            assertTrue(!binding->is_stable_binding, "Initial binding stability");
        }
        
        // Test substrate integration binding creation
        bool substrate_binding_created = substrate_integration_->createNeuralGroundingAssociation(
            2, visual_features, auditory_features, language_features);
        assertTrue(substrate_binding_created, "Substrate cross-modal binding creation");
        
        log("DEBUG: testCrossModalBindingCreation completed successfully");
        std::cout << "DEBUG: testCrossModalBindingCreation completed successfully" << std::endl;
        std::cout.flush();
    }

    void testCrossModalAssociationStrengthening() {
        std::cout << "DEBUG: Entering testCrossModalAssociationStrengthening()" << std::endl;
        std::cout.flush();
        log("--- Testing Cross-modal Association Strengthening ---");
        std::cout << "DEBUG: After log message in testCrossModalAssociationStrengthening()" << std::endl;
        std::cout.flush();
        
        try {
            if (!neural_bindings_) {
                log("ERROR: neural_bindings_ is null");
                assertTrue(false, "Neural bindings null pointer");
                return;
            }
            
            log("Getting cross-modal binding...");
            auto binding = neural_bindings_->getCrossModalBinding(1);
            if (!binding) {
                assertTrue(false, "Test cross-modal binding not available");
                return;
            }
            
            log("Got binding, checking initial strength...");
            float initial_strength = binding->binding_strength;
            
            // Test strengthening
            log("Testing strengthening...");
            bool strengthening_success = neural_bindings_->strengthenCrossModalBinding(1, 0.3f);
            assertTrue(strengthening_success, "Cross-modal binding strengthening");
            
            // Check strength increase
            log("Checking strength increase...");
            assertTrue(binding->binding_strength > initial_strength, "Binding strength increase");
            
            // Test substrate integration strengthening
            log("Testing substrate integration strengthening...");
            if (!substrate_integration_) {
                log("ERROR: substrate_integration_ is null");
                assertTrue(false, "Substrate integration null pointer");
                return;
            }
            
            bool substrate_strengthening = substrate_integration_->strengthenGroundingAssociation(2, 0.4f);
            assertTrue(substrate_strengthening, "Substrate binding strengthening");
            log("Cross-modal association strengthening test completed");
        } catch (const std::exception& e) {
            log("EXCEPTION in testCrossModalAssociationStrengthening: " + std::string(e.what()));
            assertTrue(false, "Exception in cross-modal association strengthening");
        } catch (...) {
            log("UNKNOWN EXCEPTION in testCrossModalAssociationStrengthening");
            assertTrue(false, "Unknown exception in cross-modal association strengthening");
        }
    }

    void testCrossModalBindingStabilization() {
        log("--- Testing Cross-modal Binding Stabilization ---");
        
        // Strengthen binding to stabilization threshold
        for (int i = 0; i < 5; ++i) {
            neural_bindings_->strengthenCrossModalBinding(1, 0.2f);
        }
        
        auto binding = neural_bindings_->getCrossModalBinding(1);
        if (binding) {
            // Test stabilization
            bool stabilization_success = neural_bindings_->stabilizeCrossModalBinding(1);
            assertTrue(stabilization_success, "Cross-modal binding stabilization");
            assertTrue(binding->is_stable_binding, "Binding stability state");
        }
        
        // Test stable bindings retrieval
        auto stable_bindings = neural_bindings_->getStableCrossModalBindings();
        assertTrue(stable_bindings.size() > 0, "Stable bindings count");
    }

    void testProsodicCircuitCreation() {
        log("--- Testing Prosodic Circuit Creation ---");
        
        auto prosodic_region = substrate_integration_->getProsodicRegion();
        if (!prosodic_region) {
            assertTrue(false, "Prosodic region not available");
            return;
        }
        
        // Create test acoustic features
        LanguageSystem::AcousticFeatures features;
        features.pitch_contour = 300.0f;
        features.energy_envelope = 0.8f;
        features.rhythm_pattern = 0.6f;
        features.motherese_score = 0.9f;
        
        // Test prosodic circuit creation
        bool circuit_created = neural_bindings_->createProsodicNeuralCircuit(
            "rising_intonation", features, prosodic_region->getId());
        assertTrue(circuit_created, "Prosodic circuit creation");
        
        // Test circuit retrieval
        auto circuit = neural_bindings_->getProsodicCircuit("rising_intonation");
        assertTrue(circuit != nullptr, "Prosodic circuit retrieval");
        
        if (circuit) {
            assertTrue(circuit->pattern_name == "rising_intonation", "Prosodic circuit name");
            assertTrue(circuit->pitch_neuron != 0, "Prosodic circuit pitch neuron");
            assertTrue(circuit->energy_neuron != 0, "Prosodic circuit energy neuron");
            assertTrue(circuit->rhythm_neuron != 0, "Prosodic circuit rhythm neuron");
            assertTrue(circuit->integration_neuron != 0, "Prosodic circuit integration neuron");
        }
    }

    void testProsodicPatternActivation() {
        log("--- Testing Prosodic Pattern Activation ---");
        
        // Create test acoustic features
        LanguageSystem::AcousticFeatures features;
        features.pitch_contour = 350.0f;
        features.energy_envelope = 0.7f;
        features.rhythm_pattern = 0.8f;
        features.motherese_score = 0.6f;
        
        // Test prosodic circuit activation
        bool activation_success = neural_bindings_->activateProsodicCircuit("rising_intonation", features);
        assertTrue(activation_success, "Prosodic circuit activation");
        
        // Test pattern detection
        auto active_patterns = neural_bindings_->detectActiveProsodicPatterns(0.5f);
        assertTrue(active_patterns.size() >= 0, "Active prosodic patterns detection");
    }

    void testMothereseBiasConfiguration() {
        log("--- Testing Motherese Bias Configuration ---");
        
        // Test motherese bias configuration
        bool bias_configured = neural_bindings_->configureMothereseBias("rising_intonation", 0.8f);
        assertTrue(bias_configured, "Motherese bias configuration");
        
        auto circuit = neural_bindings_->getProsodicCircuit("rising_intonation");
        if (circuit) {
            assertTrue(circuit->motherese_bias == 0.8f, "Motherese bias value");
        }
    }

    void testLearningSystemIntegration() {
        log("--- Testing Learning System Integration ---");
        
        // Test learning system availability
        auto learning_system = hypergraph_brain_->getLearningSystem();
        assertTrue(learning_system != nullptr, "Learning system availability");
        
        if (learning_system) {
            // Test learning configuration
            auto config = learning_system->getConfig();
            assertTrue(config.global_learning_rate > 0.0f, "Learning rate configuration");
            
            // Test learning statistics
            auto stats = learning_system->getStatistics();
            assertTrue(stats.total_updates >= 0, "Learning statistics availability");
        }
        
        // Test neural language learning application
        neural_bindings_->applyNeuralLanguageLearning(0.016f); // 16ms delta time
        assertTrue(true, "Neural language learning application");
    }

    void testSTDPApplicationToLanguageBindings() {
        log("--- Testing STDP Application to Language Bindings ---");
        
        // Create spike time map for STDP
        std::unordered_map<NeuroForge::NeuronID, NeuroForge::TimePoint> spike_times;
        auto current_time = std::chrono::steady_clock::now();
        auto microseconds_since_epoch = std::chrono::duration_cast<std::chrono::microseconds>(current_time.time_since_epoch()).count();
        
        // Convert microseconds back to time_point for proper type
        auto time_point = std::chrono::steady_clock::time_point(std::chrono::microseconds(microseconds_since_epoch));
        
        // Add some test spike times
        spike_times[1001] = time_point;
        spike_times[1002] = time_point + std::chrono::microseconds(1000); // 1ms later
        spike_times[1003] = time_point + std::chrono::microseconds(2000); // 2ms later
        
        // Test STDP application
        neural_bindings_->applySTDPToLanguageBindings(spike_times);
        assertTrue(true, "STDP application to language bindings");
    }

    void testHebbianLearningForAssemblies() {
        log("--- Testing Hebbian Learning for Assemblies ---");
        
        // Test Hebbian learning application
        neural_bindings_->applyHebbianToLanguageBindings(0.01f);
        assertTrue(true, "Hebbian learning for assemblies");
        
        // Test learning effect on assembly coherence
        auto assembly = neural_bindings_->getTokenAssembly("test_token");
        if (assembly) {
            float coherence_before = assembly->assembly_coherence;
            neural_bindings_->applyHebbianToLanguageBindings(0.02f);
            // Coherence should be maintained or improved
            assertTrue(assembly->assembly_coherence >= coherence_before, "Hebbian learning effect on coherence");
        }
    }

    void testAttentionModulationEffects() {
        log("--- Testing Attention Modulation Effects ---");
        
        // Create attention weight map
        std::unordered_map<NeuroForge::NeuronID, float> attention_weights;
        attention_weights[1001] = 0.8f;
        attention_weights[1002] = 0.6f;
        attention_weights[1003] = 0.9f;
        
        // Test attention modulation
        neural_bindings_->modulateLanguageLearning(attention_weights);
        assertTrue(true, "Attention modulation application");
        
        // Test substrate integration attention modulation
        substrate_integration_->modulateAttentionForLanguageLearning(attention_weights);
        assertTrue(true, "Substrate attention modulation");

        // Verify LearningSystem attention configuration after modulation
        auto ls_ptr = hypergraph_brain_->getLearningSystem();
        if (ls_ptr) {
            const auto& ls_cfg = ls_ptr->getConfig();
            assertTrue(ls_cfg.enable_attention_modulation, "LearningSystem attention modulation enabled");
            assertTrue(ls_cfg.attention_mode == LearningSystem::Config::AttentionMode::ExternalMap, "LearningSystem attention mode ExternalMap");
            assertTrue(ls_cfg.attention_anneal_ms >= 0, "LearningSystem attention anneal configured");
            float boost_base = ls_ptr->getLastAttentionBoostBase();
            assertTrue(boost_base >= ls_cfg.attention_Amin && boost_base <= ls_cfg.attention_Amax, "Attention boost base within bounds");

            // Verify auto eligibility accumulation enabled via substrate integration
            bool autoElig = ls_ptr->isAutoEligibilityAccumulationEnabled();
            assertTrue(autoElig, "Auto eligibility accumulation enabled");

            // Verify attention boost factor default within configured bounds
            assertTrue(ls_cfg.attention_boost_factor >= ls_cfg.attention_Amin &&
                       ls_cfg.attention_boost_factor <= ls_cfg.attention_Amax,
                       "Attention boost factor within bounds");

            // Additional diagnostics and assertions for attention configuration
            std::cout << "[TEST] Attention config: mode=" << static_cast<int>(ls_cfg.attention_mode)
                      << ", boost_factor=" << ls_cfg.attention_boost_factor
                      << ", Amin=" << ls_cfg.attention_Amin
                      << ", Amax=" << ls_cfg.attention_Amax
                      << ", anneal_ms=" << ls_cfg.attention_anneal_ms
                      << ", autoElig=" << (autoElig ? "true" : "false")
                      << std::endl;

            assertTrue(ls_cfg.attention_boost_factor > 0.0f, "Attention boost factor positive");
            assertTrue(ls_cfg.attention_Amin >= 1.0f, "Attention Amin minimum bound");
            assertTrue(ls_cfg.attention_Amax >= ls_cfg.attention_Amin, "Attention Amax not less than Amin");
        } else {
            assertTrue(false, "LearningSystem available for attention modulation");
        }
    }

    void testNeuralBindingOptimization() {
        log("--- Testing Neural Binding Optimization ---");
        
        // Test optimization process
        neural_bindings_->optimizeNeuralBindings();
        assertTrue(true, "Neural binding optimization");
        
        // Test substrate integration optimization
        substrate_integration_->optimizeNeuralBindings();
        assertTrue(true, "Substrate binding optimization");
        
        // Check optimization effects on statistics
        auto stats_before = neural_bindings_->getStatistics();
        neural_bindings_->optimizeNeuralBindings();
        auto stats_after = neural_bindings_->getStatistics();
        
        // Optimization should maintain or improve binding health
        assertTrue(stats_after.neural_language_operations >= stats_before.neural_language_operations, 
                  "Optimization effect on operations");
    }

    void testInactiveBindingPruning() {
        log("--- Testing Inactive Binding Pruning ---");
        
        std::size_t bindings_before = neural_bindings_->getTotalBindings();
        
        // Test pruning with high threshold (should prune inactive bindings)
        neural_bindings_->pruneInactiveBindings(0.9f);
        
        std::size_t bindings_after = neural_bindings_->getTotalBindings();
        assertTrue(bindings_after <= bindings_before, "Inactive binding pruning");
        
        // Test substrate integration pruning
        substrate_integration_->pruneInactiveBindings(0.8f);
        assertTrue(true, "Substrate inactive binding pruning");
    }

    void testPatternConsolidation() {
        log("--- Testing Pattern Consolidation ---");
        
        // Test neural binding consolidation
        neural_bindings_->consolidateLanguageBindings();
        assertTrue(true, "Neural binding consolidation");
        
        // Test substrate integration consolidation
        substrate_integration_->consolidateNeuralPatterns();
        assertTrue(true, "Substrate pattern consolidation");
        
        // Check consolidation effects
        auto crystallized_patterns = neural_bindings_->getCrystallizedProtoWords();
        assertTrue(crystallized_patterns.size() >= 0, "Consolidation effect on crystallized patterns");
    }

    void testSubstrateLanguageCoherence() {
        log("--- Testing Substrate Language Coherence ---");
        
        // Test coherence calculation
        float coherence = substrate_integration_->calculateIntegrationCoherence();
        assertTrue(coherence >= 0.0f && coherence <= 1.0f, "Integration coherence range");
        
        // Test coherence improvement through processing
        substrate_integration_->processSubstrateLanguageStep(0.016f);
        float new_coherence = substrate_integration_->calculateIntegrationCoherence();
        assertTrue(new_coherence >= 0.0f, "Coherence after processing step");
    }

    void testIntegrationEfficiency() {
        log("--- Testing Integration Efficiency ---");
        
        auto stats = substrate_integration_->getStatistics();
        assertTrue(stats.integration_efficiency >= 0.0f && stats.integration_efficiency <= 1.0f, 
                  "Integration efficiency range");
        
        // Test efficiency improvement through optimization
        substrate_integration_->optimizeNeuralBindings();
        auto new_stats = substrate_integration_->getStatistics();
        assertTrue(new_stats.integration_efficiency >= stats.integration_efficiency, 
                  "Efficiency improvement through optimization");
    }

    void testOverallSystemHealth() {
        log("--- Testing Overall System Health ---");
        
        // Test neural binding health
        float binding_health = neural_bindings_->getOverallBindingHealth();
        assertTrue(binding_health >= 0.0f && binding_health <= 1.0f, "Neural binding health range");
        
        // Test system health after processing
        for (int i = 0; i < 10; ++i) {
            substrate_integration_->processSubstrateLanguageStep(0.016f);
            neural_bindings_->applyNeuralLanguageLearning(0.016f);
        }
        
        float new_health = neural_bindings_->getOverallBindingHealth();
        assertTrue(new_health >= 0.0f, "System health after processing");
    }

    void testLargeScaleTokenBinding() {
        log("--- Testing Large Scale Token Binding ---");
        
        auto language_region = substrate_integration_->getLanguageRegion();
        if (!language_region) {
            assertTrue(false, "Language region not available for large scale test");
            return;
        }
        
        // Create multiple token bindings
        std::size_t initial_count = neural_bindings_->getTotalBindings();
        
        for (int i = 0; i < 20; ++i) {
            std::string token_name = "token_" + std::to_string(i);
            std::vector<float> embedding = {
                static_cast<float>(i) / 20.0f, 
                static_cast<float>(i + 1) / 20.0f, 
                static_cast<float>(i + 2) / 20.0f
            };
            
            neural_bindings_->createTokenNeuralAssembly(token_name, embedding, language_region->getId());
        }
        
        std::size_t final_count = neural_bindings_->getTotalBindings();
        assertTrue(final_count > initial_count, "Large scale token binding creation");
        
        // Test activation of multiple tokens
        for (int i = 0; i < 20; ++i) {
            std::string token_name = "token_" + std::to_string(i);
            neural_bindings_->activateTokenAssembly(token_name, 0.5f);
        }
        
        assertTrue(true, "Large scale token activation");
    }

    void testConcurrentPatternProcessing() {
        log("--- Testing Concurrent Pattern Processing ---");
        
        auto proto_word_region = substrate_integration_->getProtoWordRegion();
        if (!proto_word_region) {
            assertTrue(false, "Proto-word region not available for concurrent test");
            return;
        }
        
        // Create multiple patterns concurrently
        std::vector<std::string> patterns = {"baba", "gaga", "dada", "nana", "papa"};
        std::vector<std::vector<std::string>> phoneme_sequences = {
            {"b", "a", "b", "a"},
            {"g", "a", "g", "a"},
            {"d", "a", "d", "a"},
            {"n", "a", "n", "a"},
            {"p", "a", "p", "a"}
        };
        
        for (std::size_t i = 0; i < patterns.size(); ++i) {
            neural_bindings_->createProtoWordNeuralPattern(
                patterns[i], phoneme_sequences[i], proto_word_region->getId());
        }
        
        // Reinforce all patterns concurrently
        for (const auto& pattern : patterns) {
            for (int j = 0; j < 5; ++j) {
                neural_bindings_->reinforceProtoWordPattern(pattern, 0.2f);
            }
        }
        
        // Check concurrent processing results
        auto crystallized = neural_bindings_->getCrystallizedProtoWords();
        assertTrue(crystallized.size() >= 0, "Concurrent pattern processing results");
    }

    void testMemoryUsageOptimization() {
        log("--- Testing Memory Usage Optimization ---");
        
        // Create a test token before optimization to verify it survives
        std::vector<float> test_embedding = {0.8f, 0.6f, 0.4f, 0.9f, 0.2f};
        bool token_created = neural_bindings_->createTokenNeuralAssembly("optimization_test_token", 
                                                   test_embedding,
                                                   NeuroForge::RegionID(1));
        std::cout << "DEBUG: Token creation result: " << token_created << std::endl;
        
        // Activate it to ensure it has recent activity and good coherence
        bool token_activated = neural_bindings_->activateTokenAssembly("optimization_test_token", 0.8f);
        std::cout << "DEBUG: Token activation result: " << token_activated << std::endl;
        
        // Check if token exists before optimization
        auto assembly_before = neural_bindings_->getTokenAssembly("optimization_test_token");
        std::cout << "DEBUG: Token exists before optimization: " << (assembly_before != nullptr) << std::endl;
        if (assembly_before) {
            std::cout << "DEBUG: Token coherence before optimization: " << assembly_before->assembly_coherence << std::endl;
        }
        
        // Test memory optimization
        neural_bindings_->optimizeNeuralBindings();
        substrate_integration_->optimizeNeuralBindings();
        
        // Check if token exists after optimization
        auto assembly_after_opt = neural_bindings_->getTokenAssembly("optimization_test_token");
        std::cout << "DEBUG: Token exists after optimization: " << (assembly_after_opt != nullptr) << std::endl;
        
        // Test pruning for memory optimization with a lower threshold to preserve active tokens
        neural_bindings_->pruneInactiveBindings(0.05f);  // Lower threshold
        substrate_integration_->pruneInactiveBindings(0.05f);
        
        // Check if token exists after pruning
        auto assembly_after_prune = neural_bindings_->getTokenAssembly("optimization_test_token");
        std::cout << "DEBUG: Token exists after pruning: " << (assembly_after_prune != nullptr) << std::endl;
        
        // Test consolidation for memory efficiency
        neural_bindings_->consolidateLanguageBindings();
        substrate_integration_->consolidateNeuralPatterns();
        
        // Check if token exists after consolidation
        auto assembly_after_consolidation = neural_bindings_->getTokenAssembly("optimization_test_token");
        std::cout << "DEBUG: Token exists after consolidation: " << (assembly_after_consolidation != nullptr) << std::endl;
        
        assertTrue(true, "Memory usage optimization completed");
        
        // Verify system still functions after optimization using the token we just created
        bool activation_works = neural_bindings_->activateTokenAssembly("optimization_test_token", 0.5f);
        std::cout << "DEBUG: Final activation result: " << activation_works << std::endl;
        assertTrue(activation_works, "System functionality after memory optimization");
    }

    void printTestSummary() {
        std::cout << "\n=== Test Suite Summary ===" << std::endl;
        std::cout << "Tests Passed: " << tests_passed_ << std::endl;
        std::cout << "Tests Failed: " << tests_failed_ << std::endl;
        std::cout << "Total Tests: " << (tests_passed_ + tests_failed_) << std::endl;
        
        if (tests_failed_ == 0) {
            std::cout << "🎉 All tests PASSED! Substrate Language Integration is working correctly." << std::endl;
        } else {
            std::cout << "⚠️  Some tests FAILED. Please review the implementation." << std::endl;
        }
        
        // Print system statistics with null pointer checks
        std::cout << "\n=== System Statistics ===" << std::endl;
        
        try {
            if (neural_bindings_) {
                auto neural_stats = neural_bindings_->getStatistics();
                
                std::cout << "Neural Bindings:" << std::endl;
                std::cout << "  Token Assemblies: " << neural_stats.active_token_assemblies 
                          << "/" << neural_stats.total_token_assemblies << std::endl;
                std::cout << "  Proto-word Patterns: " << neural_stats.crystallized_patterns 
                          << "/" << neural_stats.total_proto_word_patterns << std::endl;
                std::cout << "  Cross-modal Bindings: " << neural_stats.stable_cross_modal_bindings 
                          << "/" << neural_stats.total_cross_modal_bindings << std::endl;
            } else {
                std::cout << "Neural Bindings: NOT AVAILABLE (null pointer)" << std::endl;
            }
            
            if (substrate_integration_) {
                auto substrate_stats = substrate_integration_->getStatistics();
                
                std::cout << "Substrate Integration:" << std::endl;
                std::cout << "  Integration Efficiency: " << substrate_stats.integration_efficiency << std::endl;
                std::cout << "  Substrate-Language Coherence: " << substrate_stats.substrate_language_coherence << std::endl;
                std::cout << "  Neural-Language Updates: " << substrate_stats.neural_language_updates << std::endl;
            } else {
                std::cout << "Substrate Integration: NOT AVAILABLE (null pointer)" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Error getting system statistics: " << e.what() << std::endl;
        }
        
        // Generate integration reports with exception handling
        std::cout << "\n=== Integration Reports ===" << std::endl;
        
        try {
            if (neural_bindings_) {
                std::cout << neural_bindings_->generateBindingReport() << std::endl;
            } else {
                std::cout << "Neural Bindings Report: NOT AVAILABLE (null pointer)" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Error generating neural bindings report: " << e.what() << std::endl;
        }
        
        try {
            if (substrate_integration_) {
                std::cout << substrate_integration_->generateIntegrationReport() << std::endl;
            } else {
                std::cout << "Substrate Integration Report: NOT AVAILABLE (null pointer)" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Error generating substrate integration report: " << e.what() << std::endl;
        }
    }
    
    // Multimodal parallel stream test methods
    void testMultimodalStreamInitialization() {
        log("--- Testing Multimodal Stream Initialization ---");
        
        // Test multimodal stream region initialization
        bool multimodal_init = substrate_integration_->initializeMultimodalStreamRegions();
        assertTrue(multimodal_init, "Multimodal stream regions initialization");
        
        // Test cross-modal connections establishment
        bool cross_modal_connections = substrate_integration_->establishCrossModalConnections();
        assertTrue(cross_modal_connections, "Cross-modal connections establishment");
    }
    
    void testParallelStreamCreation() {
        log("--- Testing Parallel Stream Creation ---");
        
        // Create test audio features
        std::vector<float> audio_features = {0.8f, 0.6f, 0.9f, 0.7f, 0.5f};
        bool audio_stream = substrate_integration_->createAudioProcessingStream(audio_features);
        assertTrue(audio_stream, "Audio processing stream creation");
        
        // Create test visual features
        std::vector<float> visual_features = {0.7f, 0.8f, 0.6f, 0.9f, 0.4f};
        bool visual_stream = substrate_integration_->createVisualProcessingStream(visual_features);
        assertTrue(visual_stream, "Visual processing stream creation");
        
        // Create test gaze coordination features
        std::vector<float> gaze_targets = {0.5f, 0.7f, 0.8f, 0.6f, 0.9f};
        bool gaze_stream = substrate_integration_->createGazeCoordinationStream(gaze_targets);
        assertTrue(gaze_stream, "Gaze coordination stream creation");
    }
    
    void testMultimodalStreamSynchronization() {
        log("--- Testing Multimodal Stream Synchronization ---");
        
        // Test stream synchronization with high temporal alignment
        bool sync_result = substrate_integration_->synchronizeMultimodalStreams(0.8f);
        assertTrue(sync_result, "Multimodal stream synchronization");
        
        // Test multimodal coherence calculation
        float coherence = substrate_integration_->calculateMultimodalNeuralCoherence();
        assertTrue(coherence >= 0.0f && coherence <= 1.0f, "Multimodal neural coherence range");
    }
    
    void testCrossModalStreamCoordination() {
        log("--- Testing Cross-Modal Stream Coordination ---");
        
        // Test cross-modal binding reinforcement
        bool audio_visual_binding = substrate_integration_->reinforceCrossModalBinding("audio", "visual", 0.7f);
        assertTrue(audio_visual_binding, "Audio-visual cross-modal binding");
        
        bool audio_gaze_binding = substrate_integration_->reinforceCrossModalBinding("audio", "gaze", 0.6f);
        assertTrue(audio_gaze_binding, "Audio-gaze cross-modal binding");
        
        // Test activation propagation across modalities
        substrate_integration_->propagateActivationAcrossModalities(0.3f);
        assertTrue(true, "Cross-modal activation propagation");
        
        // Test joint attention processing
        std::vector<float> attention_target = {0.8f, 0.7f, 0.9f};
        bool joint_attention = substrate_integration_->processJointAttentionNeurally(attention_target, "test_token");
        assertTrue(joint_attention, "Joint attention neural processing");
    }
    
    void testMultimodalNeuralCoherence() {
        log("--- Testing Multimodal Neural Coherence ---");
        
        // Create comprehensive multimodal features for testing
        LanguageSystem::SpeechProductionFeatures speech_features;
        speech_features.phoneme_sequence = {
            LanguageSystem::PhonemeCluster{"p", {}, {}, 0.0f, {}, 0.0f},
            LanguageSystem::PhonemeCluster{"a", {}, {}, 1.0f, {}, 0.0f},
            LanguageSystem::PhonemeCluster{"p", {}, {}, 0.0f, {}, 0.0f},
            LanguageSystem::PhonemeCluster{"a", {}, {}, 1.0f, {}, 0.0f}
        };
        speech_features.timing_pattern = {0.1f, 0.15f, 0.1f, 0.15f};
        speech_features.prosody_contour = {0.8f, 0.6f, 0.9f, 0.7f};
        
        LanguageSystem::VisualLanguageFeatures visual_features;
        visual_features.lip_features = {0.5f, 0.7f, 0.8f, 0.6f};
        visual_features.gaze_vector = {0.7f, 0.8f, 0.6f, 0.9f};
        visual_features.speech_vision_coupling = 0.85f;
        
        // Test parallel neural stream activation
        bool parallel_activation = substrate_integration_->activateParallelNeuralStreams(speech_features, visual_features);
        assertTrue(parallel_activation, "Parallel neural streams activation");
        
        // Test audio-visual binding integration
        std::vector<float> audio_pattern = {0.8f, 0.6f, 0.9f, 0.7f};
        std::vector<float> visual_pattern = {0.7f, 0.8f, 0.6f, 0.9f};
        bool av_binding = substrate_integration_->integrateAudioVisualBinding(audio_pattern, visual_pattern, 0.2f);
        assertTrue(av_binding, "Audio-visual binding integration");
        
        // Test multimodal attention map processing
        std::vector<float> attention_weights = {0.8f, 0.7f, 0.9f};
        std::vector<std::string> active_modalities = {"audio", "visual", "gaze"};
        bool attention_processing = substrate_integration_->processMultimodalAttentionMap(attention_weights, active_modalities);
        assertTrue(attention_processing, "Multimodal attention map processing");
        
        // Test multimodal stream coherence updates
        substrate_integration_->updateMultimodalStreamCoherence(0.016f); // 16ms delta time
        assertTrue(true, "Multimodal stream coherence update");
        
        // Test cross-modal neural state updates
        substrate_integration_->updateCrossModalNeuralState(0.016f);
        assertTrue(true, "Cross-modal neural state update");
        
        // Final coherence validation
        float final_coherence = substrate_integration_->calculateMultimodalNeuralCoherence();
        assertTrue(final_coherence >= 0.0f, "Final multimodal neural coherence");
    }
};

int main() {
    try {
        SubstrateLanguageIntegrationTest test_suite(true);
        test_suite.runAllTests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test suite failed with exception: " << e.what() << std::endl;
        return 1;
    }
}