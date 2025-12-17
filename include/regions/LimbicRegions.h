#pragma once

#include "core/Region.h"
#include "core/Neuron.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <chrono>
#include <memory>

namespace NeuroForge {
    namespace Regions {

        // ===== Cingulate Cortex =====
        class CingulateCortex : public Core::Region {
        public:
            enum class CingulateArea {
                Anterior,    // Emotional regulation, conflict monitoring
                Posterior,   // Spatial attention, memory
                Rostral,     // Emotion and cognition integration
                Dorsal       // Cognitive control
            };

            struct ConflictSignal {
                std::string signal_id;
                float conflict_strength;
                std::vector<float> competing_options;
                std::chrono::system_clock::time_point detection_time;
                bool is_resolved;
            };

            struct AttentionControl {
                std::string target_id;
                float attention_weight;
                std::vector<float> attention_pattern;
                bool is_focused;
                float duration;
            };

        private:
            std::unordered_map<CingulateArea, std::vector<NeuroForge::NeuronPtr>> area_neurons_;
            std::vector<ConflictSignal> conflict_signals_;
            std::vector<AttentionControl> attention_controls_;
            float emotional_regulation_strength_;
            float conflict_threshold_;
            float attention_focus_level_;

        public:
            explicit CingulateCortex(const std::string& name, std::size_t neuron_count = 50000);
            
            // Conflict monitoring
            void detectConflict(const std::vector<float>& competing_signals);
            std::vector<ConflictSignal> getActiveConflicts() const;
            void resolveConflict(const std::string& signal_id, const std::vector<float>& resolution);
            
            // Attention control
            void focusAttention(const std::string& target_id, const std::vector<float>& target_pattern);
            void shiftAttention(const std::string& new_target_id);
            float getAttentionLevel() const { return attention_focus_level_; }
            
            // Emotional regulation
            void regulateEmotion(float emotional_intensity, const std::string& emotion_type);
            float getEmotionalRegulationStrength() const { return emotional_regulation_strength_; }
            
        protected:
            void processRegionSpecific(float delta_time) override;
            void initializeCingulateAreas();
        };

        // ===== Insula =====
        class Insula : public Core::Region {
        public:
            enum class InsularArea {
                Anterior,    // Emotional awareness, empathy
                Posterior,   // Sensory integration
                Granular,    // Cognitive processing
                Agranular    // Emotional processing
            };

            struct InteroceptiveSignal {
                std::string signal_type; // heartbeat, breathing, hunger, etc.
                float intensity;
                std::vector<float> signal_pattern;
                std::chrono::system_clock::time_point timestamp;
                bool is_conscious;
            };

            struct EmpathyResponse {
                std::string target_id;
                float empathy_strength;
                std::vector<float> mirrored_emotion;
                bool is_active;
            };

        private:
            std::unordered_map<InsularArea, std::vector<NeuroForge::NeuronPtr>> area_neurons_;
            std::vector<InteroceptiveSignal> interoceptive_signals_;
            std::vector<EmpathyResponse> empathy_responses_;
            float interoceptive_sensitivity_;
            float empathy_threshold_;
            float emotional_awareness_level_;

        public:
            explicit Insula(const std::string& name, std::size_t neuron_count = 30000);
            
            // Interoceptive processing
            void processInteroceptiveSignal(const std::string& signal_type, float intensity);
            std::vector<InteroceptiveSignal> getInteroceptiveState() const;
            float getInteroceptiveSensitivity() const { return interoceptive_sensitivity_; }
            
            // Empathy and social cognition
            void processEmpathicSignal(const std::string& target_id, const std::vector<float>& observed_emotion);
            std::vector<EmpathyResponse> getActiveEmpathyResponses() const;
            
            // Emotional awareness
            void updateEmotionalAwareness(const std::vector<float>& emotional_state);
            float getEmotionalAwarenessLevel() const { return emotional_awareness_level_; }
            
        protected:
            void processRegionSpecific(float delta_time) override;
            void initializeInsularAreas();
        };

        // ===== Self Node (Self-Model) =====
        class SelfNode : public Core::Region {
        public:
            enum class SelfAspect {
                Physical,     // Body schema, physical self
                Cognitive,    // Mental abilities, thoughts
                Emotional,    // Emotional patterns, personality
                Social,       // Social identity, relationships
                Temporal,     // Past experiences, future goals
                Narrative     // Life story, personal meaning
            };

            struct SelfRepresentation {
                SelfAspect aspect;
                std::string aspect_id;
                std::vector<float> representation_pattern;
                float confidence_level;
                std::chrono::system_clock::time_point last_updated;
                bool is_active;
            };

            struct SelfReflection {
                std::string reflection_id;
                std::string trigger_event;
                std::vector<SelfAspect> involved_aspects;
                std::vector<float> reflection_content;
                float insight_level;
                bool is_complete;
            };

            struct MetaCognition {
                std::string process_id;
                std::string cognitive_process; // thinking, remembering, deciding, etc.
                float monitoring_strength;
                std::vector<float> process_state;
                bool requires_control;
            };

        private:
            std::unordered_map<SelfAspect, std::vector<NeuroForge::NeuronPtr>> aspect_neurons_;
            std::unordered_map<SelfAspect, SelfRepresentation> self_representations_;
            std::vector<SelfReflection> active_reflections_;
            std::vector<MetaCognition> metacognitive_processes_;
            float self_awareness_level_;
            float metacognitive_strength_;
            float narrative_coherence_;
            std::string current_identity_;

        public:
            explicit SelfNode(const std::string& name, std::size_t neuron_count = 100000);
            
            // Self-representation management
            void updateSelfRepresentation(SelfAspect aspect, const std::vector<float>& new_representation);
            SelfRepresentation getSelfRepresentation(SelfAspect aspect) const;
            std::vector<SelfRepresentation> getAllSelfRepresentations() const;
            
            // Self-reflection and introspection
            void initiateReflection(const std::string& trigger_event, const std::vector<SelfAspect>& aspects);
            std::vector<SelfReflection> getActiveReflections() const;
            void completeReflection(const std::string& reflection_id, float insight_level);
            
            // Metacognition
            void monitorCognitiveProcess(const std::string& process_id, const std::string& process_type);
            void controlCognitiveProcess(const std::string& process_id, const std::vector<float>& control_signal);
            std::vector<MetaCognition> getMetacognitiveProcesses() const;
            
            // Self-awareness and identity
            float getSelfAwarenessLevel() const { return self_awareness_level_; }
            std::string getCurrentIdentity() const { return current_identity_; }
            void updateIdentity(const std::string& new_identity_aspect);
            
            // Narrative construction
            float getNarrativeCoherence() const { return narrative_coherence_; }
            void integrateExperience(const std::vector<float>& experience_pattern);
            
        protected:
            void processRegionSpecific(float delta_time) override;
            void initializeSelfAspects();
            void updateSelfAwareness();
            void maintainNarrativeCoherence();
        };

        // ===== Default Mode Network =====
        class DefaultModeNetwork : public Core::Region {
        public:
            enum class DMNNode {
                MedialPrefrontal,    // Self-referential thinking
                PosteriorCingulate,  // Autobiographical memory
                AngularGyrus,        // Conceptual processing
                Precuneus,           // Consciousness, self-awareness
                Hippocampus,         // Memory consolidation
                TemporalPole         // Social cognition
            };

            struct SpontaneousThought {
                std::string thought_id;
                std::string content_type; // memory, planning, social, creative
                std::vector<float> thought_pattern;
                float salience;
                std::chrono::system_clock::time_point emergence_time;
                bool is_conscious;
            };

            struct MindWandering {
                std::string episode_id;
                std::vector<SpontaneousThought> thought_stream;
                float attention_decoupling;
                std::chrono::duration<float> duration;
                bool is_active;
            };

        private:
            std::unordered_map<DMNNode, std::vector<NeuroForge::NeuronPtr>> node_neurons_;
            std::vector<SpontaneousThought> spontaneous_thoughts_;
            std::vector<MindWandering> mind_wandering_episodes_;
            float default_activity_level_;
            float task_negative_correlation_;
            float intrinsic_connectivity_;
            bool is_task_active_;

        public:
            explicit DefaultModeNetwork(const std::string& name, std::size_t neuron_count = 80000);
            
            // Spontaneous cognition
            void generateSpontaneousThought(const std::string& content_type);
            std::vector<SpontaneousThought> getCurrentThoughts() const;
            void suppressSpontaneousActivity(float suppression_strength);
            
            // Mind wandering
            void initiateMindWandering();
            void terminateMindWandering();
            bool isMindWandering() const;
            MindWandering getCurrentMindWanderingEpisode() const;
            
            // Task-related modulation
            void setTaskState(bool is_task_active);
            float getTaskNegativeCorrelation() const { return task_negative_correlation_; }
            
            // Network connectivity
            float getIntrinsicConnectivity() const { return intrinsic_connectivity_; }
            void modulateConnectivity(float modulation_strength);
            
        protected:
            void processRegionSpecific(float delta_time) override;
            void initializeDMNNodes();
            void updateDefaultActivity(float delta_time);
        };

    } // namespace Regions
} // namespace NeuroForge