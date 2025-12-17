#include "regions/LimbicRegions.h"
#include <algorithm>
#include <random>
#include <cmath>
#include <chrono>

namespace NeuroForge {
    namespace Regions {

        // ===== Cingulate Cortex Implementation =====

        CingulateCortex::CingulateCortex(const std::string& name, std::size_t neuron_count)
             : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Cortical, Core::Region::ActivationPattern::Competitive)
            , emotional_regulation_strength_(0.5f)
            , conflict_threshold_(0.7f)
            , attention_focus_level_(0.5f)
        {
            createNeurons(neuron_count);
            initializeCingulateAreas();
        }

        void CingulateCortex::initializeCingulateAreas() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_area = neurons.size() / 4; // Divide among 4 cingulate areas
            
            std::size_t neuron_index = 0;
            for (int area = 0; area < 4; ++area) {
                CingulateArea cing_area = static_cast<CingulateArea>(area);
                std::vector<NeuroForge::NeuronPtr> area_neurons;
                
                for (std::size_t i = 0; i < neurons_per_area && neuron_index < neurons.size(); ++i) {
                    area_neurons.push_back(neurons[neuron_index++]);
                }
                
                area_neurons_[cing_area] = std::move(area_neurons);
            }
        }

        void CingulateCortex::detectConflict(const std::vector<float>& competing_signals) {
            if (competing_signals.size() < 2) return;
            
            // Calculate conflict strength based on signal competition
            float max_signal = *std::max_element(competing_signals.begin(), competing_signals.end());
            float min_signal = *std::min_element(competing_signals.begin(), competing_signals.end());
            float conflict_strength = 1.0f - (max_signal - min_signal);
            
            if (conflict_strength > conflict_threshold_) {
                ConflictSignal signal;
                signal.signal_id = "conflict_" + std::to_string(conflict_signals_.size());
                signal.conflict_strength = conflict_strength;
                signal.competing_options = competing_signals;
                signal.detection_time = std::chrono::system_clock::now();
                signal.is_resolved = false;
                
                conflict_signals_.push_back(signal);
                
                // Activate anterior cingulate neurons for conflict monitoring
                auto anterior_it = area_neurons_.find(CingulateArea::Anterior);
                if (anterior_it != area_neurons_.end()) {
                    for (std::size_t i = 0; i < anterior_it->second.size() && i < competing_signals.size(); ++i) {
                        if (anterior_it->second[i] && competing_signals[i] > 0.3f) {
                            anterior_it->second[i]->setState(Core::Neuron::State::Active);
                        }
                    }
                }
            }
        }

        std::vector<CingulateCortex::ConflictSignal> CingulateCortex::getActiveConflicts() const {
            std::vector<ConflictSignal> active_conflicts;
            
            for (const auto& signal : conflict_signals_) {
                if (!signal.is_resolved) {
                    active_conflicts.push_back(signal);
                }
            }
            
            return active_conflicts;
        }

        void CingulateCortex::resolveConflict(const std::string& signal_id, const std::vector<float>& resolution) {
            for (auto& signal : conflict_signals_) {
                if (signal.signal_id == signal_id) {
                    signal.is_resolved = true;
                    
                    // Activate dorsal cingulate for cognitive control
                    auto dorsal_it = area_neurons_.find(CingulateArea::Dorsal);
                    if (dorsal_it != area_neurons_.end()) {
                        for (std::size_t i = 0; i < dorsal_it->second.size() && i < resolution.size(); ++i) {
                            if (dorsal_it->second[i] && resolution[i] > 0.3f) {
                                dorsal_it->second[i]->setState(Core::Neuron::State::Active);
                            }
                        }
                    }
                    break;
                }
            }
        }

        void CingulateCortex::focusAttention(const std::string& target_id, const std::vector<float>& target_pattern) {
            AttentionControl control;
            control.target_id = target_id;
            control.attention_weight = 1.0f;
            control.attention_pattern = target_pattern;
            control.is_focused = true;
            control.duration = 0.0f;
            
            attention_controls_.push_back(control);
            attention_focus_level_ = 0.8f;
            
            // Activate posterior cingulate for attention control
            auto posterior_it = area_neurons_.find(CingulateArea::Posterior);
            if (posterior_it != area_neurons_.end()) {
                for (std::size_t i = 0; i < posterior_it->second.size() && i < target_pattern.size(); ++i) {
                    if (posterior_it->second[i] && target_pattern[i] > 0.3f) {
                        posterior_it->second[i]->setState(Core::Neuron::State::Active);
                    }
                }
            }
        }

        void CingulateCortex::shiftAttention(const std::string& new_target_id) {
            (void)new_target_id;
            // Deactivate current attention
            for (auto& control : attention_controls_) {
                if (control.is_focused) {
                    control.is_focused = false;
                }
            }
            
            attention_focus_level_ *= 0.5f; // Reduce focus during shift
        }

        void CingulateCortex::regulateEmotion(float emotional_intensity, const std::string& emotion_type) {
            (void)emotion_type;

            // Emotional regulation through rostral cingulate
            auto rostral_it = area_neurons_.find(CingulateArea::Rostral);
            if (rostral_it != area_neurons_.end()) {
                float regulation_signal = emotional_regulation_strength_ * emotional_intensity;
                
                for (auto& neuron : rostral_it->second) {
                    if (neuron && regulation_signal > 0.3f) {
                        neuron->setState(Core::Neuron::State::Active);
                    }
                }
            }
            
            // Adjust regulation strength based on success
            if (emotional_intensity > 0.8f) {
                emotional_regulation_strength_ = std::min(1.0f, emotional_regulation_strength_ + 0.1f);
            }
        }

        void CingulateCortex::processRegionSpecific(float delta_time) {
            // Update attention controls
            for (auto& control : attention_controls_) {
                if (control.is_focused) {
                    control.duration += delta_time;
                    
                    // Attention naturally decays over time
                    control.attention_weight *= (1.0f - 0.1f * delta_time);
                    if (control.attention_weight < 0.1f) {
                        control.is_focused = false;
                    }
                }
            }
            
            // Update attention focus level
            float active_attention = 0.0f;
            for (const auto& control : attention_controls_) {
                if (control.is_focused) {
                    active_attention += control.attention_weight;
                }
            }
            attention_focus_level_ = std::min(1.0f, active_attention);
            
            // Process cingulate areas
            for (auto& [area, neurons] : area_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        neuron->process(delta_time);
                    }
                }
            }
        }

        // ===== Insula Implementation =====

        Insula::Insula(const std::string& name, std::size_t neuron_count)
            : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Cortical, Core::Region::ActivationPattern::Asynchronous)
            , interoceptive_sensitivity_(0.5f)
            , empathy_threshold_(0.6f)
            , emotional_awareness_level_(0.5f)
        {
            createNeurons(neuron_count);
            initializeInsularAreas();
        }

        void Insula::initializeInsularAreas() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_area = neurons.size() / 4; // Divide among 4 insular areas
            
            std::size_t neuron_index = 0;
            for (int area = 0; area < 4; ++area) {
                InsularArea ins_area = static_cast<InsularArea>(area);
                std::vector<NeuroForge::NeuronPtr> area_neurons;
                
                for (std::size_t i = 0; i < neurons_per_area && neuron_index < neurons.size(); ++i) {
                    area_neurons.push_back(neurons[neuron_index++]);
                }
                
                area_neurons_[ins_area] = std::move(area_neurons);
            }
        }

        void Insula::processInteroceptiveSignal(const std::string& signal_type, float intensity) {
            InteroceptiveSignal signal;
            signal.signal_type = signal_type;
            signal.intensity = intensity;
            signal.signal_pattern = {intensity}; // Simplified pattern
            signal.timestamp = std::chrono::system_clock::now();
            signal.is_conscious = (intensity > 0.5f * interoceptive_sensitivity_);
            
            interoceptive_signals_.push_back(signal);
            
            // Activate posterior insula for interoceptive processing
            auto posterior_it = area_neurons_.find(InsularArea::Posterior);
            if (posterior_it != area_neurons_.end()) {
                for (auto& neuron : posterior_it->second) {
                    if (neuron && intensity > 0.3f) {
                        neuron->setState(Core::Neuron::State::Active);
                    }
                }
            }
            
            // Update emotional awareness based on interoceptive signals
            emotional_awareness_level_ = std::min(1.0f, emotional_awareness_level_ + intensity * 0.1f);
        }

        std::vector<Insula::InteroceptiveSignal> Insula::getInteroceptiveState() const {
            std::vector<InteroceptiveSignal> current_signals;
            
            auto now = std::chrono::system_clock::now();
            for (const auto& signal : interoceptive_signals_) {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - signal.timestamp);
                if (duration.count() < 10) { // Signals valid for 10 seconds
                    current_signals.push_back(signal);
                }
            }
            
            return current_signals;
        }

        void Insula::processEmpathicSignal(const std::string& target_id, const std::vector<float>& observed_emotion) {
            if (observed_emotion.empty()) return;
            
            float emotion_intensity = 0.0f;
            for (float value : observed_emotion) {
                emotion_intensity += std::abs(value);
            }
            emotion_intensity /= observed_emotion.size();
            
            if (emotion_intensity > empathy_threshold_) {
                EmpathyResponse response;
                response.target_id = target_id;
                response.empathy_strength = emotion_intensity * 0.7f; // Attenuated mirroring
                response.mirrored_emotion = observed_emotion;
                response.is_active = true;
                
                // Scale down the mirrored emotion
                for (float& value : response.mirrored_emotion) {
                    value *= response.empathy_strength;
                }
                
                empathy_responses_.push_back(response);
                
                // Activate anterior insula for empathic processing
                auto anterior_it = area_neurons_.find(InsularArea::Anterior);
                if (anterior_it != area_neurons_.end()) {
                    for (std::size_t i = 0; i < anterior_it->second.size() && i < observed_emotion.size(); ++i) {
                        if (anterior_it->second[i] && observed_emotion[i] > 0.6f) {
                            anterior_it->second[i]->setState(Core::Neuron::State::Active);
                        }
                    }
                }
            }
        }

        std::vector<Insula::EmpathyResponse> Insula::getActiveEmpathyResponses() const {
            std::vector<EmpathyResponse> active_responses;
            
            for (const auto& response : empathy_responses_) {
                if (response.is_active) {
                    active_responses.push_back(response);
                }
            }
            
            return active_responses;
        }

        void Insula::updateEmotionalAwareness(const std::vector<float>& emotional_state) {
            if (emotional_state.empty()) return;
            
            float emotional_complexity = 0.0f;
            for (std::size_t i = 1; i < emotional_state.size(); ++i) {
                emotional_complexity += std::abs(emotional_state[i] - emotional_state[i-1]);
            }
            emotional_complexity /= (emotional_state.size() - 1);
            
            emotional_awareness_level_ = std::min(1.0f, emotional_complexity * interoceptive_sensitivity_);
            
            // Activate granular insula for emotional awareness
            auto granular_it = area_neurons_.find(InsularArea::Granular);
            if (granular_it != area_neurons_.end()) {
                for (std::size_t i = 0; i < granular_it->second.size() && i < emotional_state.size(); ++i) {
                    if (granular_it->second[i] && emotional_state[i] > 0.5f) {
                        granular_it->second[i]->setState(Core::Neuron::State::Active);
                    }
                }
            }
        }

        void Insula::processRegionSpecific(float delta_time) {
            // Decay old interoceptive signals
            auto now = std::chrono::system_clock::now();
            interoceptive_signals_.erase(
                std::remove_if(interoceptive_signals_.begin(), interoceptive_signals_.end(),
                    [now](const InteroceptiveSignal& signal) {
                        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - signal.timestamp);
                        return duration.count() > 30; // Remove signals older than 30 seconds
                    }),
                interoceptive_signals_.end()
            );
            
            // Decay empathy responses
            for (auto& response : empathy_responses_) {
                response.empathy_strength *= (1.0f - 0.1f * delta_time);
                if (response.empathy_strength < 0.1f) {
                    response.is_active = false;
                }
            }
            
            // Process insular areas
            for (auto& [area, neurons] : area_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        neuron->process(delta_time);
                    }
                }
            }
        }

        // ===== Self Node Implementation =====

        SelfNode::SelfNode(const std::string& name, std::size_t neuron_count)
            : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Special, Core::Region::ActivationPattern::Layered)
            , self_awareness_level_(0.5f)
            , metacognitive_strength_(0.5f)
            , narrative_coherence_(0.5f)
            , current_identity_("developing_self")
        {
            createNeurons(neuron_count);
            initializeSelfAspects();
        }

        void SelfNode::initializeSelfAspects() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_aspect = neurons.size() / 6; // Divide among 6 self aspects
            
            std::size_t neuron_index = 0;
            for (int aspect = 0; aspect < 6; ++aspect) {
                SelfAspect self_aspect = static_cast<SelfAspect>(aspect);
                std::vector<NeuroForge::NeuronPtr> aspect_neurons;
                
                for (std::size_t i = 0; i < neurons_per_aspect && neuron_index < neurons.size(); ++i) {
                    aspect_neurons.push_back(neurons[neuron_index++]);
                }
                
                aspect_neurons_[self_aspect] = std::move(aspect_neurons);
                
                // Initialize self representation for this aspect
                SelfRepresentation repr;
                repr.aspect = self_aspect;
                repr.aspect_id = "aspect_" + std::to_string(aspect);
                repr.representation_pattern = std::vector<float>(10, 0.5f); // Default neutral pattern
                repr.confidence_level = 0.3f; // Low initial confidence
                repr.last_updated = std::chrono::system_clock::now();
                repr.is_active = true;
                
                self_representations_[self_aspect] = repr;
            }
        }

        void SelfNode::updateSelfRepresentation(SelfAspect aspect, const std::vector<float>& new_representation) {
            auto it = self_representations_.find(aspect);
            if (it != self_representations_.end()) {
                it->second.representation_pattern = new_representation;
                it->second.last_updated = std::chrono::system_clock::now();
                it->second.confidence_level = std::min(1.0f, it->second.confidence_level + 0.1f);
                
                // Activate corresponding aspect neurons
                auto aspect_it = aspect_neurons_.find(aspect);
                if (aspect_it != aspect_neurons_.end()) {
                    for (std::size_t i = 0; i < aspect_it->second.size() && i < new_representation.size(); ++i) {
                        if (aspect_it->second[i] && new_representation[i] > 0.3f) {
                            aspect_it->second[i]->setState(Core::Neuron::State::Active);
                        }
                    }
                }
            }
        }

        SelfNode::SelfRepresentation SelfNode::getSelfRepresentation(SelfAspect aspect) const {
            auto it = self_representations_.find(aspect);
            if (it != self_representations_.end()) {
                return it->second;
            }
            
            // Return empty representation if not found
            SelfRepresentation empty_repr;
            empty_repr.aspect = aspect;
            empty_repr.confidence_level = 0.0f;
            empty_repr.is_active = false;
            return empty_repr;
        }

        std::vector<SelfNode::SelfRepresentation> SelfNode::getAllSelfRepresentations() const {
            std::vector<SelfRepresentation> all_representations;
            
            for (const auto& [aspect, repr] : self_representations_) {
                if (repr.is_active) {
                    all_representations.push_back(repr);
                }
            }
            
            return all_representations;
        }

        void SelfNode::initiateReflection(const std::string& trigger_event, const std::vector<SelfAspect>& aspects) {
            SelfReflection reflection;
            reflection.reflection_id = "reflection_" + std::to_string(active_reflections_.size());
            reflection.trigger_event = trigger_event;
            reflection.involved_aspects = aspects;
            reflection.reflection_content = std::vector<float>(aspects.size() * 10, 0.0f); // Initialize content
            reflection.insight_level = 0.0f;
            reflection.is_complete = false;
            
            active_reflections_.push_back(reflection);
            
            // Activate neurons for involved aspects
            for (SelfAspect aspect : aspects) {
                auto aspect_it = aspect_neurons_.find(aspect);
                if (aspect_it != aspect_neurons_.end()) {
                    for (auto& neuron : aspect_it->second) {
                        if (neuron) {
                            neuron->setState(Core::Neuron::State::Active);
                        }
                    }
                }
            }
        }

        std::vector<SelfNode::SelfReflection> SelfNode::getActiveReflections() const {
            std::vector<SelfReflection> active;
            
            for (const auto& reflection : active_reflections_) {
                if (!reflection.is_complete) {
                    active.push_back(reflection);
                }
            }
            
            return active;
        }

        void SelfNode::completeReflection(const std::string& reflection_id, float insight_level) {
            for (auto& reflection : active_reflections_) {
                if (reflection.reflection_id == reflection_id) {
                    reflection.is_complete = true;
                    reflection.insight_level = insight_level;
                    
                    // Update self-awareness based on insight
                    self_awareness_level_ = std::min(1.0f, self_awareness_level_ + insight_level * 0.1f);
                    break;
                }
            }
        }

        void SelfNode::monitorCognitiveProcess(const std::string& process_id, const std::string& process_type) {
            MetaCognition metacog;
            metacog.process_id = process_id;
            metacog.cognitive_process = process_type;
            metacog.monitoring_strength = metacognitive_strength_;
            metacog.process_state = std::vector<float>(5, 0.5f); // Default state
            metacog.requires_control = false;
            
            metacognitive_processes_.push_back(metacog);
        }

        void SelfNode::controlCognitiveProcess(const std::string& process_id, const std::vector<float>& control_signal) {
            for (auto& metacog : metacognitive_processes_) {
                if (metacog.process_id == process_id) {
                    metacog.process_state = control_signal;
                    metacog.requires_control = false;
                    
                    // Strengthen metacognitive ability
                    metacognitive_strength_ = std::min(1.0f, metacognitive_strength_ + 0.05f);
                    break;
                }
            }
        }

        std::vector<SelfNode::MetaCognition> SelfNode::getMetacognitiveProcesses() const {
            return metacognitive_processes_;
        }

        void SelfNode::updateIdentity(const std::string& new_identity_aspect) {
            current_identity_ = new_identity_aspect;
            
            // Update narrative coherence based on identity consistency
            narrative_coherence_ = std::min(1.0f, narrative_coherence_ + 0.1f);
        }

        void SelfNode::integrateExperience(const std::vector<float>& experience_pattern) {
            // Integrate experience into narrative self
            auto narrative_it = self_representations_.find(SelfAspect::Narrative);
            if (narrative_it != self_representations_.end()) {
                // Simple integration: average with existing pattern
                for (std::size_t i = 0; i < std::min(experience_pattern.size(), 
                                                    narrative_it->second.representation_pattern.size()); ++i) {
                    narrative_it->second.representation_pattern[i] = 
                        (narrative_it->second.representation_pattern[i] + experience_pattern[i]) * 0.5f;
                }
                
                narrative_it->second.last_updated = std::chrono::system_clock::now();
            }
        }

        void SelfNode::updateSelfAwareness() {
            // Calculate self-awareness based on active representations
            float total_confidence = 0.0f;
            int active_aspects = 0;
            
            for (const auto& [aspect, repr] : self_representations_) {
                if (repr.is_active) {
                    total_confidence += repr.confidence_level;
                    active_aspects++;
                }
            }
            
            if (active_aspects > 0) {
                self_awareness_level_ = total_confidence / active_aspects;
            }
        }

        void SelfNode::maintainNarrativeCoherence() {
            // Check consistency between different self aspects
            float coherence_sum = 0.0f;
            int comparisons = 0;
            
            for (const auto& [aspect1, repr1] : self_representations_) {
                for (const auto& [aspect2, repr2] : self_representations_) {
                    if (aspect1 != aspect2 && repr1.is_active && repr2.is_active) {
                        // Calculate similarity between representations
                        float similarity = 0.0f;
                        std::size_t min_size = std::min(repr1.representation_pattern.size(), 
                                                       repr2.representation_pattern.size());
                        
                        for (std::size_t i = 0; i < min_size; ++i) {
                            similarity += 1.0f - std::abs(repr1.representation_pattern[i] - 
                                                         repr2.representation_pattern[i]);
                        }
                        similarity /= min_size;
                        
                        coherence_sum += similarity;
                        comparisons++;
                    }
                }
            }
            
            if (comparisons > 0) {
                narrative_coherence_ = coherence_sum / comparisons;
            }
        }

        void SelfNode::processRegionSpecific(float delta_time) {
            // Update self-awareness
            updateSelfAwareness();
            
            // Maintain narrative coherence
            maintainNarrativeCoherence();
            
            // Process ongoing reflections
            for (auto& reflection : active_reflections_) {
                if (!reflection.is_complete) {
                    // Gradually develop insight
                    reflection.insight_level += 0.1f * delta_time;
                    
                    if (reflection.insight_level > 0.8f) {
                        reflection.is_complete = true;
                    }
                }
            }
            
            // Process metacognitive monitoring
            for (auto& metacog : metacognitive_processes_) {
                // Check if cognitive process needs control
                float process_deviation = 0.0f;
                for (float state_value : metacog.process_state) {
                    process_deviation += std::abs(state_value - 0.5f);
                }
                process_deviation /= metacog.process_state.size();
                
                if (process_deviation > 0.3f) {
                    metacog.requires_control = true;
                }
            }
            
            // Process self aspect neurons
            for (auto& [aspect, neurons] : aspect_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        neuron->process(delta_time);
                    }
                }
            }
        }

        // ===== Default Mode Network Implementation =====

        DefaultModeNetwork::DefaultModeNetwork(const std::string& name, std::size_t neuron_count)
            : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Cortical, Core::Region::ActivationPattern::Oscillatory)
            , default_activity_level_(0.7f)
            , task_negative_correlation_(-0.6f)
            , intrinsic_connectivity_(0.8f)
            , is_task_active_(false)
        {
            createNeurons(neuron_count);
            initializeDMNNodes();
        }

        void DefaultModeNetwork::initializeDMNNodes() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_node = neurons.size() / 6; // Divide among 6 DMN nodes
            
            std::size_t neuron_index = 0;
            for (int node = 0; node < 6; ++node) {
                DMNNode dmn_node = static_cast<DMNNode>(node);
                std::vector<NeuroForge::NeuronPtr> node_neurons;
                
                for (std::size_t i = 0; i < neurons_per_node && neuron_index < neurons.size(); ++i) {
                    node_neurons.push_back(neurons[neuron_index++]);
                }
                
                node_neurons_[dmn_node] = std::move(node_neurons);
            }
        }

        void DefaultModeNetwork::generateSpontaneousThought(const std::string& content_type) {
            SpontaneousThought thought;
            thought.thought_id = "thought_" + std::to_string(spontaneous_thoughts_.size());
            thought.content_type = content_type;
            thought.thought_pattern = std::vector<float>(10, 0.0f); // Initialize pattern
            thought.salience = 0.5f;
            thought.emergence_time = std::chrono::system_clock::now();
            thought.is_conscious = (default_activity_level_ > 0.6f);
            
            // Generate content based on type
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            
            for (float& value : thought.thought_pattern) {
                value = dist(gen);
            }
            
            spontaneous_thoughts_.push_back(thought);
            
            // Activate appropriate DMN nodes based on content type
            if (content_type == "memory") {
                auto hippocampus_it = node_neurons_.find(DMNNode::Hippocampus);
                if (hippocampus_it != node_neurons_.end()) {
                    for (auto& neuron : hippocampus_it->second) {
                        if (neuron) {
                            neuron->setState(Core::Neuron::State::Active);
                        }
                    }
                }
            } else if (content_type == "social") {
                auto temporal_it = node_neurons_.find(DMNNode::TemporalPole);
                if (temporal_it != node_neurons_.end()) {
                    for (auto& neuron : temporal_it->second) {
                        if (neuron) {
                            neuron->setState(Core::Neuron::State::Active);
                        }
                    }
                }
            }
        }

        std::vector<DefaultModeNetwork::SpontaneousThought> DefaultModeNetwork::getCurrentThoughts() const {
            std::vector<SpontaneousThought> current_thoughts;
            
            auto now = std::chrono::system_clock::now();
            for (const auto& thought : spontaneous_thoughts_) {
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - thought.emergence_time);
                if (duration.count() < 60) { // Thoughts valid for 1 minute
                    current_thoughts.push_back(thought);
                }
            }
            
            return current_thoughts;
        }

        void DefaultModeNetwork::suppressSpontaneousActivity(float suppression_strength) {
            default_activity_level_ *= (1.0f - suppression_strength);
            default_activity_level_ = std::max(0.1f, default_activity_level_);
        }

        void DefaultModeNetwork::initiateMindWandering() {
            MindWandering episode;
            episode.episode_id = "wandering_" + std::to_string(mind_wandering_episodes_.size());
            episode.attention_decoupling = 0.8f;
            episode.duration = std::chrono::duration<float>(0.0f);
            episode.is_active = true;
            
            mind_wandering_episodes_.push_back(episode);
            
            // Generate initial thoughts for mind wandering
            generateSpontaneousThought("memory");
            generateSpontaneousThought("planning");
        }

        void DefaultModeNetwork::terminateMindWandering() {
            for (auto& episode : mind_wandering_episodes_) {
                if (episode.is_active) {
                    episode.is_active = false;
                }
            }
        }

        bool DefaultModeNetwork::isMindWandering() const {
            for (const auto& episode : mind_wandering_episodes_) {
                if (episode.is_active) {
                    return true;
                }
            }
            return false;
        }

        DefaultModeNetwork::MindWandering DefaultModeNetwork::getCurrentMindWanderingEpisode() const {
            for (const auto& episode : mind_wandering_episodes_) {
                if (episode.is_active) {
                    return episode;
                }
            }
            
            // Return empty episode if none active
            MindWandering empty_episode;
            empty_episode.is_active = false;
            return empty_episode;
        }

        void DefaultModeNetwork::setTaskState(bool is_task_active) {
            is_task_active_ = is_task_active;
            
            if (is_task_active) {
                // Suppress default mode activity during task
                default_activity_level_ *= 0.3f;
                terminateMindWandering();
            } else {
                // Restore default mode activity
                default_activity_level_ = std::min(0.8f, default_activity_level_ + 0.2f);
            }
        }

        void DefaultModeNetwork::modulateConnectivity(float modulation_strength) {
            intrinsic_connectivity_ += modulation_strength;
            intrinsic_connectivity_ = std::clamp(intrinsic_connectivity_, 0.0f, 1.0f);
        }

        void DefaultModeNetwork::updateDefaultActivity(float delta_time) {
            if (!is_task_active_) {
                // Gradually increase default activity when not task-engaged
                default_activity_level_ += 0.1f * delta_time;
                default_activity_level_ = std::min(0.9f, default_activity_level_);
                
                // Spontaneously generate thoughts
                static float thought_timer = 0.0f;
                thought_timer += delta_time;
                
                if (thought_timer > 5.0f) { // Generate thought every 5 seconds
                    std::vector<std::string> thought_types = {"memory", "planning", "social", "creative"};
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dist(0, static_cast<int>(thought_types.size() - 1));
                    
                    generateSpontaneousThought(thought_types[dist(gen)]);
                    thought_timer = 0.0f;
                }
            }
        }

        void DefaultModeNetwork::processRegionSpecific(float delta_time) {
            // Update default mode activity
            updateDefaultActivity(delta_time);
            
            // Update mind wandering episodes
            for (auto& episode : mind_wandering_episodes_) {
                if (episode.is_active) {
                    episode.duration += std::chrono::duration<float>(delta_time);
                    
                    // Add thoughts to the episode
                    auto current_thoughts = getCurrentThoughts();
                    for (const auto& thought : current_thoughts) {
                        // Check if thought is already in episode
                        bool found = false;
                        for (const auto& episode_thought : episode.thought_stream) {
                            if (episode_thought.thought_id == thought.thought_id) {
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            episode.thought_stream.push_back(thought);
                        }
                    }
                }
            }
            
            // Process DMN nodes
            for (auto& [node, neurons] : node_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        // Modulate activity based on default mode level
                        float activity_modulation = default_activity_level_ * intrinsic_connectivity_;
                        neuron->process(delta_time * activity_modulation);
                    }
                }
            }
        }

        // Expose a no-op symbol so the main executable can force-link this TU and run static registrars
        extern "C" void NF_ForceLink_LimbicRegions() {}

    } // namespace Regions
} // namespace NeuroForge