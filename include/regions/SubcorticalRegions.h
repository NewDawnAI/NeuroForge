#pragma once

#include "core/Region.h"
#include "core/Neuron.h"
#include "core/Synapse.h"
#include <vector>
#include <unordered_map>
#include <queue>
#include <memory>
#include <string>

namespace NeuroForge {
    namespace Regions {

        /**
         * @brief Hippocampus - Critical for memory formation, spatial navigation, and learning
         * 
         * The hippocampus is essential for:
         * - Episodic memory formation and retrieval
         * - Spatial navigation and cognitive mapping
         * - Pattern separation and completion
         * - Memory consolidation
         */
        class Hippocampus : public Core::Region {
        public:
            enum class HippocampalArea {
                CA1,        // Pyramidal cells, memory output
                CA2,        // Social memory processing
                CA3,        // Pattern completion, recurrent connections
                DentateGyrus // Pattern separation, neurogenesis
            };

            enum class MemoryType {
                Episodic,   // Events and experiences
                Spatial,    // Location and navigation
                Semantic,   // Facts and concepts
                Procedural  // Skills and habits
            };

            struct MemoryTrace {
                std::string memory_id;
                MemoryType type;
                std::vector<float> encoding_pattern;
                float consolidation_strength;
                std::chrono::system_clock::time_point timestamp;
                bool is_consolidated;
            };

            struct SpatialCell {
                NeuroForge::NeuronPtr neuron;
                std::pair<float, float> place_field_center;  // x, y coordinates
                float place_field_radius;
                float firing_rate;
                bool is_place_cell;
                bool is_grid_cell;
                bool is_border_cell;
            };

        public:
            explicit Hippocampus(const std::string& name, std::size_t neuron_count = 1000000);
            virtual ~Hippocampus() = default;

            // Memory operations
            void encodeMemory(const std::vector<float>& input_pattern, MemoryType type);
            std::vector<float> retrieveMemory(const std::string& memory_id);
            void consolidateMemories(float consolidation_threshold = 0.7f);
            
            // Spatial navigation
            void updateSpatialPosition(float x, float y);
            std::vector<SpatialCell> getActivePlaceCells() const;
            std::pair<float, float> estimatePosition() const;
            
            // Pattern operations
            std::vector<float> performPatternSeparation(const std::vector<float>& input);
            std::vector<float> performPatternCompletion(const std::vector<float>& partial_input);
            
            // Learning and plasticity
            void enableLongTermPotentiation(bool enable) { ltp_enabled_ = enable; }
            void setLearningRate(float rate) { learning_rate_ = rate; }
            
            // Getters
            std::size_t getMemoryCount() const { return memory_traces_.size(); }
            float getCurrentTheta() const { return theta_rhythm_; }
            const std::vector<MemoryTrace>& getMemoryTraces() const { return memory_traces_; }

        protected:
            void processRegionSpecific(float delta_time) override;
            void initializeHippocampalAreas();
            void generateThetaRhythm(float delta_time);
            void updatePlaceCells(float x, float y);

        private:
            // Hippocampal areas
            std::unordered_map<HippocampalArea, std::vector<NeuroForge::NeuronPtr>> area_neurons_;
            
            // Memory storage
            std::vector<MemoryTrace> memory_traces_;
            std::unordered_map<std::string, std::size_t> memory_index_;
            
            // Spatial navigation
            std::vector<SpatialCell> spatial_cells_;
            std::pair<float, float> current_position_;
            
            // Rhythms and oscillations
            float theta_rhythm_;        // 4-12 Hz theta oscillation
            float gamma_rhythm_;        // 30-100 Hz gamma oscillation
            
            // Learning parameters
            bool ltp_enabled_;
            float learning_rate_;
            float consolidation_rate_;
            
            // Neurogenesis (simplified)
            std::size_t neurogenesis_rate_;
        };

        /**
         * @brief Amygdala - Emotional processing, fear conditioning, and threat detection
         * 
         * The amygdala is responsible for:
         * - Emotional memory formation
         * - Fear and threat detection
         * - Emotional modulation of memory
         * - Fight-or-flight responses
         */
        class Amygdala : public Core::Region {
        public:
            enum class AmygdalaArea {
                Lateral,        // Sensory input processing
                Basal,          // Associative learning
                Central,        // Output to brainstem
                Medial          // Social and reproductive behaviors
            };

            enum class EmotionalState {
                Neutral,
                Fear,
                Anxiety,
                Anger,
                Pleasure,
                Disgust
            };

            struct EmotionalMemory {
                std::string stimulus_id;
                EmotionalState emotional_valence;
                float intensity;
                std::vector<float> stimulus_pattern;
                std::chrono::system_clock::time_point formation_time;
                bool is_conditioned;
            };

            struct ThreatAssessment {
                float threat_level;         // 0.0 to 1.0
                std::vector<float> threat_features;
                EmotionalState response_type;
                float confidence;
                bool requires_immediate_action;
            };

        public:
            explicit Amygdala(const std::string& name, std::size_t neuron_count = 500000);
            virtual ~Amygdala() = default;

            // Emotional processing
            EmotionalState processEmotionalStimulus(const std::vector<float>& stimulus);
            void formEmotionalMemory(const std::vector<float>& stimulus, EmotionalState emotion, float intensity);
            EmotionalMemory retrieveEmotionalMemory(const std::string& stimulus_id);
            
            // Threat detection
            ThreatAssessment assessThreat(const std::vector<float>& sensory_input);
            void updateThreatDatabase(const std::vector<float>& threat_pattern, float danger_level);
            
            // Fear conditioning
            void conditionFearResponse(const std::vector<float>& conditioned_stimulus,
                                     const std::vector<float>& unconditioned_stimulus);
            bool isConditionedStimulus(const std::vector<float>& stimulus);
            
            // Emotional modulation
            float getEmotionalArousal() const { return emotional_arousal_; }
            void modulateMemoryConsolidation(float enhancement_factor);
            
            // Getters
            EmotionalState getCurrentEmotionalState() const { return current_emotional_state_; }
            const std::vector<EmotionalMemory>& getEmotionalMemories() const { return emotional_memories_; }

        protected:
            void processRegionSpecific(float delta_time) override;
            void initializeAmygdalaAreas();
            void updateEmotionalState(float delta_time);

        private:
            // Amygdala areas
            std::unordered_map<AmygdalaArea, std::vector<NeuroForge::NeuronPtr>> area_neurons_;
            
            // Emotional processing
            EmotionalState current_emotional_state_;
            float emotional_arousal_;
            float emotional_decay_rate_;
            
            // Memory storage
            std::vector<EmotionalMemory> emotional_memories_;
            std::vector<std::vector<float>> threat_patterns_;
            
            // Fear conditioning
            std::unordered_map<std::string, std::vector<float>> conditioned_stimuli_;
            float conditioning_strength_;
        };

        /**
         * @brief Thalamus - Sensory relay, attention gating, and consciousness
         * 
         * The thalamus serves as:
         * - Sensory relay station to cortex
         * - Attention and consciousness gating
         * - Sleep-wake cycle regulation
         * - Cortical-subcortical communication hub
         */
        class Thalamus : public Core::Region {
        public:
            enum class ThalamicNucleus {
                LGN,            // Lateral Geniculate Nucleus (visual)
                MGN,            // Medial Geniculate Nucleus (auditory)
                VPL_VPM,        // Ventral Posterior (somatosensory)
                VA_VL,          // Ventral Anterior/Lateral (motor)
                MD,             // Mediodorsal (cognitive)
                Pulvinar,       // Attention and consciousness
                Reticular       // Thalamic reticular nucleus
            };

            enum class ConsciousnessLevel {
                Unconscious,
                Drowsy,
                Alert,
                Hypervigilant
            };

            struct SensoryRelay {
                ThalamicNucleus source_nucleus;
                std::vector<float> sensory_data;
                float relay_strength;
                bool is_gated;
                std::string target_cortical_area;
            };

            struct AttentionGate {
                std::string gate_id;
                float attention_weight;
                bool is_open;
                std::vector<Core::Region::RegionID> target_regions;
            };

        public:
            explicit Thalamus(const std::string& name, std::size_t neuron_count = 800000);
            virtual ~Thalamus() = default;

            // Sensory relay functions
            void relaySensoryInput(ThalamicNucleus nucleus, const std::vector<float>& sensory_data);
            std::vector<float> getSensoryOutput(ThalamicNucleus nucleus);
            void setRelayGating(ThalamicNucleus nucleus, bool is_gated);
            
            // Attention control
            void openAttentionGate(const std::string& gate_id, float attention_weight);
            void closeAttentionGate(const std::string& gate_id);
            void modulateAttention(const std::vector<Core::Region::RegionID>& target_regions, float strength);
            
            // Consciousness and arousal
            void setConsciousnessLevel(ConsciousnessLevel level);
            ConsciousnessLevel getConsciousnessLevel() const { return consciousness_level_; }
            float getArousalLevel() const { return arousal_level_; }
            
            // Sleep-wake regulation
            void regulateSleepWake(float circadian_phase);
            bool isInSleepMode() const { return sleep_mode_; }
            
            // Rhythmic activity
            void generateThalamicRhythms(float delta_time);
            float getAlphaRhythm() const { return alpha_rhythm_; }
            float getSpindleActivity() const { return spindle_activity_; }

        protected:
            void processRegionSpecific(float delta_time) override;
            void initializeThalamicNuclei();
            void updateConsciousnessGating(float delta_time);

        private:
            // Thalamic nuclei
            std::unordered_map<ThalamicNucleus, std::vector<NeuroForge::NeuronPtr>> nucleus_neurons_;
            
            // Sensory relay
            std::unordered_map<ThalamicNucleus, SensoryRelay> sensory_relays_;
            
            // Attention gating
            std::unordered_map<std::string, AttentionGate> attention_gates_;
            
            // Consciousness and arousal
            ConsciousnessLevel consciousness_level_;
            float arousal_level_;
            bool sleep_mode_;
            
            // Rhythmic activity
            float alpha_rhythm_;        // 8-13 Hz
            float spindle_activity_;    // 11-15 Hz sleep spindles
            float circadian_phase_;
        };

        /**
         * @brief Brainstem - Vital functions, arousal, and basic reflexes
         * 
         * The brainstem controls:
         * - Vital functions (breathing, heart rate)
         * - Arousal and consciousness
         * - Basic reflexes and motor patterns
         * - Neurotransmitter systems
         */
        class Brainstem : public Core::Region {
        public:
            enum class BrainstemArea {
                Medulla,        // Vital functions
                Pons,           // Sleep, arousal, facial sensation
                Midbrain,       // Eye movement, reflexes
                ReticularFormation // Arousal, consciousness
            };

            enum class VitalFunction {
                Breathing,
                HeartRate,
                BloodPressure,
                Temperature,
                Swallowing,
                Coughing
            };

            struct VitalSign {
                VitalFunction function;
                float current_value;
                float target_value;
                float regulation_strength;
                bool is_automatic;
            };

            struct ReflexArc {
                std::string reflex_id;
                std::vector<float> trigger_pattern;
                std::vector<float> response_pattern;
                float reflex_strength;
                float latency_ms;
                bool is_active;
            };

        public:
            explicit Brainstem(const std::string& name, std::size_t neuron_count = 300000);
            virtual ~Brainstem() = default;

            // Vital function regulation
            void regulateVitalFunction(VitalFunction function, float target_value);
            float getVitalSign(VitalFunction function) const;
            void setAutomaticRegulation(VitalFunction function, bool automatic);
            
            // Arousal and consciousness
            void modulateArousal(float arousal_level);
            float getArousalOutput() const { return arousal_output_; }
            void setConsciousnessState(bool conscious);
            
            // Reflex processing
            void addReflexArc(const std::string& reflex_id,
                             const std::vector<float>& trigger,
                             const std::vector<float>& response);
            std::vector<float> processReflex(const std::vector<float>& stimulus);
            bool isReflexActive(const std::string& reflex_id) const;
            
            // Neurotransmitter systems (simplified)
            void modulateNeurotransmitter(const std::string& transmitter_type, float level);
            float getNeurotransmitterLevel(const std::string& transmitter_type) const;
            
            // Sleep-wake control
            void initiateSleep();
            void initiateWakefulness();
            bool isAwake() const { return is_awake_; }

        protected:
            void processRegionSpecific(float delta_time) override;
            void initializeBrainstemAreas();
            void updateVitalFunctions(float delta_time);
            void processReflexes(float delta_time);

        private:
            // Brainstem areas
            std::unordered_map<BrainstemArea, std::vector<NeuroForge::NeuronPtr>> area_neurons_;
            
            // Vital functions
            std::unordered_map<VitalFunction, VitalSign> vital_signs_;
            
            // Arousal and consciousness
            float arousal_output_;
            bool is_awake_;
            float consciousness_threshold_;
            
            // Reflexes
            std::unordered_map<std::string, ReflexArc> reflex_arcs_;
            
            // Neurotransmitter systems
            std::unordered_map<std::string, float> neurotransmitter_levels_;
        };

    } // namespace Regions
} // namespace NeuroForge