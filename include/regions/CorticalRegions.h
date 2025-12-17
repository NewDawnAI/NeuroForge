#pragma once

#include "core/Region.h"
#include "core/Neuron.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <queue>

namespace NeuroForge {
    namespace Regions {

        /**
         * @brief Visual Cortex - Primary visual processing region
         * 
         * Handles visual input processing, feature detection, and visual pattern recognition.
         * Organized in hierarchical layers (V1, V2, V4, IT) for progressive feature extraction.
         */
        class VisualCortex : public Core::Region {
        public:
            /**
             * @brief Visual processing layers
             */
            enum class VisualLayer {
                V1,     // Primary visual cortex - edge detection, orientation
                V2,     // Secondary visual cortex - complex patterns
                V4,     // Color and shape processing
                IT      // Inferotemporal cortex - object recognition
            };

            /**
             * @brief Visual feature types
             */
            enum class FeatureType {
                Edge,
                Corner,
                Color,
                Motion,
                Texture,
                Shape,
                Object
            };

        private:
            std::unordered_map<VisualLayer, std::vector<NeuroForge::NeuronPtr>> layer_neurons_;
            std::vector<FeatureType> detected_features_;
            float visual_attention_focus_;
            std::pair<float, float> receptive_field_center_;
            float receptive_field_size_;

        public:
            /**
             * @brief Construct Visual Cortex
             * @param name Region name
             * @param neuron_count Total neurons in region
             */
            explicit VisualCortex(const std::string& name = "VisualCortex", 
                                 std::size_t neuron_count = 100000);

            /**
             * @brief Initialize visual processing layers
             */
            void initializeLayers();

            /**
             * @brief Process visual input
             * @param visual_input Raw visual data
             */
            void processVisualInput(const std::vector<float>& visual_input);

            /**
             * @brief Detect visual features
             * @param layer Target visual layer
             * @return Detected features
             */
            std::vector<FeatureType> detectFeatures(VisualLayer layer);

            /**
             * @brief Set attention focus
             * @param focus_strength Attention strength (0.0 to 1.0)
             */
            void setAttentionFocus(float focus_strength) { visual_attention_focus_ = focus_strength; }

            /**
             * @brief Get current attention focus
             * @return Attention focus strength
             */
            float getAttentionFocus() const noexcept { return visual_attention_focus_; }

        protected:
            void processRegionSpecific(float delta_time) override;
        };

        /**
         * @brief Auditory Cortex - Primary auditory processing region
         * 
         * Handles sound processing, frequency analysis, and auditory pattern recognition.
         * Organized tonotopically with frequency-specific processing areas.
         */
        class AuditoryCortex : public Core::Region {
        public:
            /**
             * @brief Auditory processing areas
             */
            enum class AuditoryArea {
                A1,     // Primary auditory cortex
                A2,     // Secondary auditory cortex
                Planum, // Planum temporale - language processing
                STG     // Superior temporal gyrus - complex sounds
            };

            /**
             * @brief Sound feature types
             */
            enum class SoundFeature {
                Frequency,
                Amplitude,
                Timbre,
                Rhythm,
                Pitch,
                Phoneme,
                Music
            };

        private:
            std::unordered_map<AuditoryArea, std::vector<NeuroForge::NeuronPtr>> area_neurons_;
            std::vector<float> frequency_map_; // Tonotopic organization
            std::vector<SoundFeature> detected_sounds_;
            float auditory_attention_;
            float frequency_resolution_;

        public:
            /**
             * @brief Construct Auditory Cortex
             * @param name Region name
             * @param neuron_count Total neurons in region
             */
            explicit AuditoryCortex(const std::string& name = "AuditoryCortex",
                                   std::size_t neuron_count = 50000);

            /**
             * @brief Initialize tonotopic organization
             */
            void initializeTonotopicMap();

            /**
             * @brief Process auditory input
             * @param audio_input Raw audio data
             */
            void processAudioInput(const std::vector<float>& audio_input);

            /**
             * @brief Analyze frequency spectrum
             * @param frequencies Input frequencies
             * @return Frequency analysis results
             */
            std::vector<float> analyzeFrequencies(const std::vector<float>& frequencies);

            /**
             * @brief Set auditory attention
             * @param attention Attention level (0.0 to 1.0)
             */
            void setAuditoryAttention(float attention) { auditory_attention_ = attention; }

        protected:
            void processRegionSpecific(float delta_time) override;
        };

        /**
         * @brief Motor Cortex - Primary motor control region
         * 
         * Controls voluntary motor movements and motor planning.
         * Organized somatotopically with body part-specific motor areas.
         */
        class MotorCortex : public Core::Region {
        public:
            /**
             * @brief Motor areas
             */
            enum class MotorArea {
                M1,     // Primary motor cortex
                PMC,    // Premotor cortex
                SMA,    // Supplementary motor area
                PPC     // Posterior parietal cortex
            };

            /**
             * @brief Body parts for motor control
             */
            enum class BodyPart {
                Head,
                Arms,
                Hands,
                Torso,
                Legs,
                Feet,
                Face,
                Eyes
            };

            /**
             * @brief Motor command structure
             */
            struct MotorCommand {
                BodyPart target_part;
                std::vector<float> movement_vector;
                float force_magnitude;
                float duration;
                bool is_executed;
            };

        private:
            std::unordered_map<MotorArea, std::vector<NeuroForge::NeuronPtr>> area_neurons_;
            std::unordered_map<BodyPart, std::vector<NeuroForge::NeuronPtr>> somatotopic_map_;
            std::queue<MotorCommand> motor_command_queue_;
            std::vector<MotorCommand> active_commands_;
            float motor_learning_rate_;

        public:
            /**
             * @brief Construct Motor Cortex
             * @param name Region name
             * @param neuron_count Total neurons in region
             */
            explicit MotorCortex(const std::string& name = "MotorCortex",
                                std::size_t neuron_count = 75000);

            /**
             * @brief Initialize somatotopic organization
             */
            void initializeSomatotopicMap();

            /**
             * @brief Plan motor movement
             * @param target_part Body part to move
             * @param movement_vector Movement direction and magnitude
             * @param force Force to apply
             */
            void planMovement(BodyPart target_part, 
                            const std::vector<float>& movement_vector,
                            float force = 1.0f);

            /**
             * @brief Execute motor commands
             */
            void executeMotorCommands();

            /**
             * @brief Get pending motor commands
             * @return Vector of pending commands
             */
            std::vector<MotorCommand> getPendingCommands() const;

            /**
             * @brief Set motor learning rate
             * @param rate Learning rate (0.0 to 1.0)
             */
            void setMotorLearningRate(float rate) { motor_learning_rate_ = rate; }

        protected:
            void processRegionSpecific(float delta_time) override;
        };

        /**
         * @brief Somatosensory Cortex - Primary somatosensory processing region
         * 
         * Handles tactile, proprioceptive, and pain processing with somatotopic organization.
         * Processes touch, pressure, temperature, pain, and body position information.
         */
        class SomatosensoryCortex : public Core::Region {
        public:
            /**
             * @brief Somatosensory areas
             */
            enum class SomatosensoryArea {
                S1,     // Primary somatosensory cortex - basic touch
                S2,     // Secondary somatosensory cortex - complex touch
                PPC,    // Posterior parietal cortex - spatial integration
                Insula  // Insular cortex - pain and interoception
            };

            /**
             * @brief Body regions for somatotopic mapping
             */
            enum class BodyRegion {
                Face,
                Lips,
                Tongue,
                Hands,
                Fingers,
                Arms,
                Torso,
                Legs,
                Feet,
                Genitals
            };

            /**
             * @brief Sensory modalities
             */
            enum class SensoryModality {
                Touch,          // Light touch, pressure
                Proprioception, // Body position, movement
                Pain,           // Nociceptive signals
                Temperature,    // Thermal sensation
                Vibration,      // Vibratory sensation
                Texture,        // Surface texture
                Pressure        // Deep pressure
            };

            /**
             * @brief Somatosensory input structure
             */
            struct SomatosensoryInput {
                BodyRegion body_region;
                SensoryModality modality;
                float intensity;        // 0.0 to 1.0
                float duration;         // Duration of stimulus
                std::vector<float> spatial_pattern; // Spatial distribution
                float temporal_frequency; // For vibration/texture
                bool is_noxious;        // Pain indicator
            };

            /**
             * @brief Tactile feature types
             */
            enum class TactileFeature {
                EdgeDetection,
                TextureRoughness,
                ShapeContour,
                MaterialHardness,
                TemperatureGradient,
                VibrationPattern,
                PressureDistribution
            };

        private:
            std::unordered_map<SomatosensoryArea, std::vector<NeuroForge::NeuronPtr>> area_neurons_;
            std::unordered_map<BodyRegion, std::vector<NeuroForge::NeuronPtr>> somatotopic_map_;
            std::unordered_map<SensoryModality, std::vector<NeuroForge::NeuronPtr>> modality_neurons_;
            std::vector<SomatosensoryInput> active_inputs_;
            std::vector<TactileFeature> detected_features_;
            
            // Processing parameters
            float tactile_sensitivity_;
            float pain_threshold_;
            float proprioceptive_accuracy_;
            float adaptation_rate_;
            float cross_modal_integration_;
            
            // Somatotopic organization parameters
            std::unordered_map<BodyRegion, float> cortical_magnification_;
            std::unordered_map<BodyRegion, std::pair<float, float>> receptive_field_sizes_;

        public:
            /**
             * @brief Construct Somatosensory Cortex
             * @param name Region name
             * @param neuron_count Total neurons in region
             */
            explicit SomatosensoryCortex(const std::string& name = "SomatosensoryCortex",
                                       std::size_t neuron_count = 80000);

            /**
             * @brief Initialize somatotopic organization
             */
            void initializeSomatotopicMap();

            /**
             * @brief Initialize sensory modality processing
             */
            void initializeModalityProcessing();

            /**
             * @brief Process somatosensory input
             * @param input Somatosensory input data
             */
            void processSomatosensoryInput(const SomatosensoryInput& input);

            /**
             * @brief Process tactile input
             * @param body_region Target body region
             * @param tactile_data Tactile sensor data
             */
            void processTactileInput(BodyRegion body_region, const std::vector<float>& tactile_data);

            /**
             * @brief Process proprioceptive input
             * @param joint_angles Joint angle data
             * @param muscle_tensions Muscle tension data
             */
            void processProprioceptiveInput(const std::vector<float>& joint_angles,
                                          const std::vector<float>& muscle_tensions);

            /**
             * @brief Process pain signals
             * @param body_region Pain location
             * @param pain_intensity Pain intensity (0.0 to 1.0)
             * @param pain_type Type of pain (sharp, dull, burning, etc.)
             */
            void processPainSignals(BodyRegion body_region, float pain_intensity, 
                                  const std::string& pain_type = "general");

            /**
             * @brief Process temperature input
             * @param body_region Temperature sensor location
             * @param temperature Temperature value
             * @param is_noxious Whether temperature is harmful
             */
            void processTemperatureInput(BodyRegion body_region, float temperature, bool is_noxious = false);

            /**
             * @brief Detect tactile features
             * @param tactile_data Raw tactile data
             * @return Detected tactile features
             */
            std::vector<TactileFeature> detectTactileFeatures(const std::vector<float>& tactile_data);

            /**
             * @brief Get body schema representation
             * @return Current body schema state
             */
            std::unordered_map<BodyRegion, std::vector<float>> getBodySchema() const;

            /**
             * @brief Update cortical plasticity
             * @param body_region Region to adapt
             * @param usage_frequency Frequency of use
             */
            void updateCorticalPlasticity(BodyRegion body_region, float usage_frequency);

            /**
             * @brief Set tactile sensitivity
             * @param sensitivity Sensitivity level (0.0 to 1.0)
             */
            void setTactileSensitivity(float sensitivity) { tactile_sensitivity_ = sensitivity; }

            /**
             * @brief Set pain threshold
             * @param threshold Pain threshold (0.0 to 1.0)
             */
            void setPainThreshold(float threshold) { pain_threshold_ = threshold; }

            /**
             * @brief Get active somatosensory inputs
             * @return Vector of active inputs
             */
            const std::vector<SomatosensoryInput>& getActiveInputs() const noexcept {
                return active_inputs_;
            }

            /**
             * @brief Get detected tactile features
             * @return Vector of detected features
             */
            const std::vector<TactileFeature>& getDetectedFeatures() const noexcept {
                return detected_features_;
            }

        protected:
            void processRegionSpecific(float delta_time) override;
            
        private:
            void initializeCorticalMagnification();
            void processModalityIntegration(float delta_time);
            void updateSomatotopicPlasticity(float delta_time);
            void processNociceptiveSignals(float delta_time);
        };

        /**
         * @brief Prefrontal Cortex - Executive control and higher-order cognition
         * 
         * Handles executive functions, working memory, decision making, and planning.
         * Critical for self-awareness and cognitive control.
         */
        class PrefrontalCortex : public Core::Region {

        public:
            /**
             * @brief Prefrontal areas
             */
            enum class PrefrontalArea {
                DLPFC,  // Dorsolateral prefrontal cortex - working memory
                VMPFC,  // Ventromedial prefrontal cortex - decision making
                ACC,    // Anterior cingulate cortex - conflict monitoring
                OFC     // Orbitofrontal cortex - reward processing
            };

            /**
             * @brief Executive function types
             */
            enum class ExecutiveFunction {
                WorkingMemory,
                AttentionControl,
                CognitiveFlexibility,
                InhibitoryControl,
                Planning,
                DecisionMaking,
                ConflictResolution
            };

            /**
             * @brief Decision structure
             */
            struct Decision {
                std::string decision_id;
                std::vector<float> options;
                std::vector<float> option_values;
                float confidence;
                std::size_t selected_option;
                bool is_final;
            };

        private:
            std::unordered_map<PrefrontalArea, std::vector<NeuroForge::NeuronPtr>> area_neurons_;
            std::vector<std::vector<float>> working_memory_buffer_;
            std::queue<Decision> decision_queue_;
            std::vector<ExecutiveFunction> active_functions_;
            float cognitive_load_;
            float attention_control_strength_;

        public:
            /**
             * @brief Construct Prefrontal Cortex
             * @param name Region name
             * @param neuron_count Total neurons in region
             */
            explicit PrefrontalCortex(const std::string& name = "PrefrontalCortex",
                                     std::size_t neuron_count = 150000);

            /**
             * @brief Initialize executive function areas
             */
            void initializeExecutiveFunctions();

            /**
             * @brief Store information in working memory
             * @param information Information to store
             */
            void storeInWorkingMemory(const std::vector<float>& information);

            /**
             * @brief Make decision based on options
             * @param options Available options
             * @param values Option values/utilities
             * @return Decision structure
             */
            Decision makeDecision(const std::vector<float>& options,
                                const std::vector<float>& values);

            /**
             * @brief Control attention focus
             * @param target_regions Regions to focus attention on
             * @param strength Attention strength
             */
            void controlAttention(const std::vector<Core::Region::RegionID>& target_regions,
                                float strength);

            /**
             * @brief Get current cognitive load
             * @return Cognitive load (0.0 to 1.0)
             */
            float getCognitiveLoad() const noexcept { return cognitive_load_; }

            /**
             * @brief Get working memory contents
             * @return Working memory buffer
             */
            const std::vector<std::vector<float>>& getWorkingMemory() const noexcept {
                return working_memory_buffer_;
            }

        protected:
            void processRegionSpecific(float delta_time) override;
        };

        // Type aliases for convenience
        using VisualCortexPtr = std::shared_ptr<VisualCortex>;
        using AuditoryCortexPtr = std::shared_ptr<AuditoryCortex>;
        using MotorCortexPtr = std::shared_ptr<MotorCortex>;
        using SomatosensoryCortexPtr = std::shared_ptr<SomatosensoryCortex>;
        using PrefrontalCortexPtr = std::shared_ptr<PrefrontalCortex>;

    } // namespace Regions
} // namespace NeuroForge