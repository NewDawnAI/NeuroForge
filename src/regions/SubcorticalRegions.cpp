#include "regions/SubcorticalRegions.h"
#include <algorithm>
#include <random>
#include <cmath>
#include <chrono>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "core/RegionRegistry.h"

namespace {
    constexpr float PI_F = 3.14159265358979323846f;
    constexpr float TWO_PI = 2.0f * PI_F;
}

namespace NeuroForge {
    namespace Regions {

        // ===== Hippocampus Implementation =====

        Hippocampus::Hippocampus(const std::string& name, std::size_t neuron_count)
            : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Subcortical, Core::Region::ActivationPattern::Oscillatory)
            , current_position_{0.0f, 0.0f}
            , theta_rhythm_(0.0f)
            , gamma_rhythm_(0.0f)
            , ltp_enabled_(true)
            , learning_rate_(0.01f)
            , consolidation_rate_(0.001f)
            , neurogenesis_rate_(100) // New neurons per day (simplified)
        {
            createNeurons(neuron_count);
            initializeHippocampalAreas();
        }

        void Hippocampus::initializeHippocampalAreas() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_area = neurons.size() / 4; // Divide among 4 hippocampal areas
            
            std::size_t neuron_index = 0;
            for (int area = 0; area < 4; ++area) {
                HippocampalArea hipp_area = static_cast<HippocampalArea>(area);
                std::vector<NeuroForge::NeuronPtr> area_neurons;
                
                for (std::size_t i = 0; i < neurons_per_area && neuron_index < neurons.size(); ++i) {
                    area_neurons.push_back(neurons[neuron_index++]);
                }
                
                area_neurons_[hipp_area] = std::move(area_neurons);
            }
            
            // Initialize spatial cells (place cells, grid cells, etc.)
            spatial_cells_.reserve(neurons.size() / 10); // 10% are spatial cells
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_real_distribution<float> pos_dist(-10.0f, 10.0f);
            std::uniform_real_distribution<float> radius_dist(0.5f, 2.0f);
            
            for (std::size_t i = 0; i < neurons.size() / 10; ++i) {
                SpatialCell cell;
                cell.neuron = neurons[i];
                cell.place_field_center = {pos_dist(gen), pos_dist(gen)};
                cell.place_field_radius = radius_dist(gen);
                cell.firing_rate = 0.0f;
                cell.is_place_cell = true;
                cell.is_grid_cell = false;
                cell.is_border_cell = false;
                
                spatial_cells_.push_back(cell);
            }
        }

        void Hippocampus::encodeMemory(const std::vector<float>& input_pattern, MemoryType type) {
            MemoryTrace trace;
            trace.memory_id = "memory_" + std::to_string(memory_traces_.size());
            trace.type = type;
            trace.encoding_pattern = input_pattern;
            trace.consolidation_strength = 0.1f; // Initial weak encoding
            trace.timestamp = std::chrono::system_clock::now();
            trace.is_consolidated = false;
            
            memory_traces_.push_back(trace);
            memory_index_[trace.memory_id] = memory_traces_.size() - 1;
            
            // Activate CA3 neurons for pattern encoding
            auto ca3_it = area_neurons_.find(HippocampalArea::CA3);
            if (ca3_it != area_neurons_.end()) {
                for (std::size_t i = 0; i < ca3_it->second.size() && i < input_pattern.size(); ++i) {
                    if (ca3_it->second[i] && input_pattern[i] > 0.5f) {
                        ca3_it->second[i]->setState(Core::Neuron::State::Active);
                    }
                }
            }
        }

        std::vector<float> Hippocampus::retrieveMemory(const std::string& memory_id) {
            auto it = memory_index_.find(memory_id);
            if (it != memory_index_.end() && it->second < memory_traces_.size()) {
                const auto& trace = memory_traces_[it->second];
                
                // Activate CA1 neurons for memory retrieval
                auto ca1_it = area_neurons_.find(HippocampalArea::CA1);
                if (ca1_it != area_neurons_.end()) {
                    for (std::size_t i = 0; i < ca1_it->second.size() && i < trace.encoding_pattern.size(); ++i) {
                        if (ca1_it->second[i] && trace.encoding_pattern[i] > 0.5f) {
                            ca1_it->second[i]->setState(Core::Neuron::State::Active);
                        }
                    }
                }
                
                return trace.encoding_pattern;
            }
            
            return {}; // Memory not found
        }

        void Hippocampus::consolidateMemories(float consolidation_threshold) {
            for (auto& trace : memory_traces_) {
                if (!trace.is_consolidated && trace.consolidation_strength >= consolidation_threshold) {
                    trace.is_consolidated = true;
                    // In a full implementation, this would transfer to neocortex
                }
                
                // Gradually increase consolidation strength
                trace.consolidation_strength += consolidation_rate_;
                trace.consolidation_strength = std::min(trace.consolidation_strength, 1.0f);
            }
        }

        void Hippocampus::updateSpatialPosition(float x, float y) {
            current_position_ = {x, y};
            updatePlaceCells(x, y);
        }

        void Hippocampus::updatePlaceCells(float x, float y) {
            for (auto& cell : spatial_cells_) {
                if (cell.is_place_cell && cell.neuron) {
                    // Calculate distance from place field center
                    float dx = x - cell.place_field_center.first;
                    float dy = y - cell.place_field_center.second;
                    float distance = std::sqrt(dx * dx + dy * dy);
                    
                    // Gaussian firing rate based on distance
                    if (distance <= cell.place_field_radius) {
                        float normalized_distance = distance / cell.place_field_radius;
                        cell.firing_rate = std::exp(-normalized_distance * normalized_distance);
                        
                        if (cell.firing_rate > 0.5f) {
                            cell.neuron->setState(Core::Neuron::State::Active);
                        }
                    } else {
                        cell.firing_rate = 0.0f;
                    }
                }
            }
        }

        std::vector<Hippocampus::SpatialCell> Hippocampus::getActivePlaceCells() const {
            std::vector<SpatialCell> active_cells;
            
            for (const auto& cell : spatial_cells_) {
                if (cell.firing_rate > 0.1f) {
                    active_cells.push_back(cell);
                }
            }
            
            return active_cells;
        }

        std::pair<float, float> Hippocampus::estimatePosition() const {
            // Population vector decoding of place cells
            float weighted_x = 0.0f, weighted_y = 0.0f, total_weight = 0.0f;
            
            for (const auto& cell : spatial_cells_) {
                if (cell.firing_rate > 0.1f) {
                    weighted_x += cell.place_field_center.first * cell.firing_rate;
                    weighted_y += cell.place_field_center.second * cell.firing_rate;
                    total_weight += cell.firing_rate;
                }
            }
            
            if (total_weight > 0.0f) {
                return {weighted_x / total_weight, weighted_y / total_weight};
            }
            
            return current_position_;
        }

        void Hippocampus::generateThetaRhythm(float delta_time) {
            // Generate 8 Hz theta rhythm
            static float theta_phase = 0.0f;
            theta_phase += TWO_PI * 8.0f * delta_time; // 8 Hz
            if (theta_phase > TWO_PI) theta_phase -= TWO_PI;
            
            theta_rhythm_ = std::sin(theta_phase);
            
            // Generate 40 Hz gamma rhythm nested in theta
            static float gamma_phase = 0.0f;
            gamma_phase += TWO_PI * 40.0f * delta_time; // 40 Hz
            if (gamma_phase > TWO_PI) gamma_phase -= TWO_PI;
            
            gamma_rhythm_ = std::sin(gamma_phase) * (0.5f + 0.5f * theta_rhythm_);
        }

        void Hippocampus::processRegionSpecific(float delta_time) {
            // Generate rhythmic activity
            generateThetaRhythm(delta_time);
            
            // Process hippocampal areas
            for (auto& [area, neurons] : area_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        // Modulate neuron activity with theta rhythm
                        float theta_modulation = 0.5f + 0.5f * theta_rhythm_;
                        neuron->process(delta_time * theta_modulation);
                    }
                }
            }
            
            // Consolidate memories
            consolidateMemories();
        }

        // ===== Amygdala Implementation =====

        Amygdala::Amygdala(const std::string& name, std::size_t neuron_count)
            : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Subcortical, Core::Region::ActivationPattern::Competitive)
            , current_emotional_state_(EmotionalState::Neutral)
            , emotional_arousal_(0.0f)
            , emotional_decay_rate_(0.1f)
            , conditioning_strength_(0.5f)
        {
            createNeurons(neuron_count);
            initializeAmygdalaAreas();
        }

        void Amygdala::initializeAmygdalaAreas() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_area = neurons.size() / 4; // Divide among 4 amygdala areas
            
            std::size_t neuron_index = 0;
            for (int area = 0; area < 4; ++area) {
                AmygdalaArea amyg_area = static_cast<AmygdalaArea>(area);
                std::vector<NeuroForge::NeuronPtr> area_neurons;
                
                for (std::size_t i = 0; i < neurons_per_area && neuron_index < neurons.size(); ++i) {
                    area_neurons.push_back(neurons[neuron_index++]);
                }
                
                area_neurons_[amyg_area] = std::move(area_neurons);
            }
        }

        Amygdala::EmotionalState Amygdala::processEmotionalStimulus(const std::vector<float>& stimulus) {
            if (stimulus.empty()) return EmotionalState::Neutral;
            
            // Simple emotional classification based on stimulus features
            float stimulus_intensity = 0.0f;
            for (float value : stimulus) {
                stimulus_intensity += std::abs(value);
            }
            stimulus_intensity /= stimulus.size();
            
            // Check for conditioned fear responses
            for (const auto& [cs_id, cs_pattern] : conditioned_stimuli_) {
                float similarity = 0.0f;
                for (std::size_t i = 0; i < std::min(stimulus.size(), cs_pattern.size()); ++i) {
                    similarity += 1.0f - std::abs(stimulus[i] - cs_pattern[i]);
                }
                similarity /= std::min(stimulus.size(), cs_pattern.size());
                
                if (similarity > 0.7f) {
                    current_emotional_state_ = EmotionalState::Fear;
                    emotional_arousal_ = std::min(1.0f, emotional_arousal_ + 0.5f);
                    return current_emotional_state_;
                }
            }
            
            // Basic emotional classification
            if (stimulus_intensity > 0.8f) {
                current_emotional_state_ = EmotionalState::Fear;
                emotional_arousal_ = std::min(1.0f, emotional_arousal_ + 0.3f);
            } else if (stimulus_intensity > 0.6f) {
                current_emotional_state_ = EmotionalState::Anxiety;
                emotional_arousal_ = std::min(1.0f, emotional_arousal_ + 0.2f);
            } else if (stimulus_intensity < 0.2f) {
                current_emotional_state_ = EmotionalState::Neutral;
            }
            
            return current_emotional_state_;
        }

        void Amygdala::formEmotionalMemory(const std::vector<float>& stimulus, EmotionalState emotion, float intensity) {
            EmotionalMemory memory;
            memory.stimulus_id = "emotion_" + std::to_string(emotional_memories_.size());
            memory.emotional_valence = emotion;
            memory.intensity = intensity;
            memory.stimulus_pattern = stimulus;
            memory.formation_time = std::chrono::system_clock::now();
            memory.is_conditioned = false;
            
            emotional_memories_.push_back(memory);
            
            // Activate lateral amygdala for memory formation
            auto lateral_it = area_neurons_.find(AmygdalaArea::Lateral);
            if (lateral_it != area_neurons_.end()) {
                for (std::size_t i = 0; i < lateral_it->second.size() && i < stimulus.size(); ++i) {
                    if (lateral_it->second[i] && stimulus[i] > 0.3f) {
                        lateral_it->second[i]->setState(Core::Neuron::State::Active);
                    }
                }
            }
        }

        Amygdala::ThreatAssessment Amygdala::assessThreat(const std::vector<float>& sensory_input) {
            ThreatAssessment assessment;
            assessment.threat_level = 0.0f;
            assessment.threat_features = sensory_input;
            assessment.response_type = EmotionalState::Neutral;
            assessment.confidence = 0.0f;
            assessment.requires_immediate_action = false;
            
            // Compare with known threat patterns
            for (const auto& threat_pattern : threat_patterns_) {
                float similarity = 0.0f;
                for (std::size_t i = 0; i < std::min(sensory_input.size(), threat_pattern.size()); ++i) {
                    similarity += 1.0f - std::abs(sensory_input[i] - threat_pattern[i]);
                }
                similarity /= std::min(sensory_input.size(), threat_pattern.size());
                
                if (similarity > assessment.threat_level) {
                    assessment.threat_level = similarity;
                    assessment.confidence = similarity;
                }
            }
            
            // Determine response based on threat level
            if (assessment.threat_level > 0.8f) {
                assessment.response_type = EmotionalState::Fear;
                assessment.requires_immediate_action = true;
            } else if (assessment.threat_level > 0.5f) {
                assessment.response_type = EmotionalState::Anxiety;
            }
            
            return assessment;
        }

        void Amygdala::conditionFearResponse(const std::vector<float>& conditioned_stimulus,
                                           const std::vector<float>& unconditioned_stimulus) {
            std::string cs_id = "cs_" + std::to_string(conditioned_stimuli_.size());
            conditioned_stimuli_[cs_id] = conditioned_stimulus;
            
            // Suppress unused parameter warning for unconditioned_stimulus (future use placeholder)
            (void)unconditioned_stimulus;
            
            // Form emotional memory for this conditioning
            formEmotionalMemory(conditioned_stimulus, EmotionalState::Fear, conditioning_strength_);
        }

        void Amygdala::updateEmotionalState(float delta_time) {
            // Emotional decay over time
            emotional_arousal_ *= (1.0f - emotional_decay_rate_ * delta_time);
            
            if (emotional_arousal_ < 0.1f) {
                current_emotional_state_ = EmotionalState::Neutral;
                emotional_arousal_ = 0.0f;
            }
        }

        void Amygdala::processRegionSpecific(float delta_time) {
            // Update emotional state
            updateEmotionalState(delta_time);
            
            // Process amygdala areas
            for (auto& [area, neurons] : area_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        // Modulate processing based on emotional arousal
                        float arousal_modulation = 1.0f + emotional_arousal_;
                        neuron->process(delta_time * arousal_modulation);
                    }
                }
            }
        }

        // ===== Thalamus Implementation =====

        Thalamus::Thalamus(const std::string& name, std::size_t neuron_count)
            : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Subcortical, Core::Region::ActivationPattern::Synchronous)
            , consciousness_level_(ConsciousnessLevel::Alert)
            , arousal_level_(0.5f)
            , sleep_mode_(false)
            , alpha_rhythm_(0.0f)
            , spindle_activity_(0.0f)
            , circadian_phase_(0.0f)
        {
            createNeurons(neuron_count);
            initializeThalamicNuclei();
        }

        void Thalamus::initializeThalamicNuclei() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_nucleus = neurons.size() / 7; // Divide among 7 nuclei
            
            std::size_t neuron_index = 0;
            for (int nucleus = 0; nucleus < 7; ++nucleus) {
                ThalamicNucleus thal_nucleus = static_cast<ThalamicNucleus>(nucleus);
                std::vector<NeuroForge::NeuronPtr> nucleus_neurons;
                
                for (std::size_t i = 0; i < neurons_per_nucleus && neuron_index < neurons.size(); ++i) {
                    nucleus_neurons.push_back(neurons[neuron_index++]);
                }
                
                nucleus_neurons_[thal_nucleus] = std::move(nucleus_neurons);
                
                // Initialize sensory relay for this nucleus
                SensoryRelay relay;
                relay.source_nucleus = thal_nucleus;
                relay.relay_strength = 1.0f;
                relay.is_gated = false;
                
                switch (thal_nucleus) {
                    case ThalamicNucleus::LGN:
                        relay.target_cortical_area = "VisualCortex";
                        break;
                    case ThalamicNucleus::MGN:
                        relay.target_cortical_area = "AuditoryCortex";
                        break;
                    case ThalamicNucleus::VPL_VPM:
                        relay.target_cortical_area = "SomatosensoryCortex";
                        break;
                    case ThalamicNucleus::VA_VL:
                        relay.target_cortical_area = "MotorCortex";
                        break;
                    default:
                        relay.target_cortical_area = "PrefrontalCortex";
                        break;
                }
                
                sensory_relays_[thal_nucleus] = relay;
            }
        }

        void Thalamus::relaySensoryInput(ThalamicNucleus nucleus, const std::vector<float>& sensory_data) {
            auto relay_it = sensory_relays_.find(nucleus);
            if (relay_it != sensory_relays_.end() && !relay_it->second.is_gated) {
                relay_it->second.sensory_data = sensory_data;
                
                // Activate corresponding thalamic neurons
                auto nucleus_it = nucleus_neurons_.find(nucleus);
                if (nucleus_it != nucleus_neurons_.end()) {
                    for (std::size_t i = 0; i < nucleus_it->second.size() && i < sensory_data.size(); ++i) {
                        if (nucleus_it->second[i] && sensory_data[i] > 0.3f) {
                            nucleus_it->second[i]->setState(Core::Neuron::State::Active);
                        }
                    }
                }
            }
        }

        std::vector<float> Thalamus::getSensoryOutput(ThalamicNucleus nucleus) {
            auto relay_it = sensory_relays_.find(nucleus);
            if (relay_it != sensory_relays_.end()) {
                // Apply relay strength and consciousness gating
                std::vector<float> output = relay_it->second.sensory_data;
                float gating_factor = arousal_level_ * relay_it->second.relay_strength;
                
                for (float& value : output) {
                    value *= gating_factor;
                }
                
                return output;
            }
            
            return {};
        }

        void Thalamus::setConsciousnessLevel(ConsciousnessLevel level) {
            consciousness_level_ = level;
            
            switch (level) {
                case ConsciousnessLevel::Unconscious:
                    arousal_level_ = 0.0f;
                    sleep_mode_ = true;
                    break;
                case ConsciousnessLevel::Drowsy:
                    arousal_level_ = 0.3f;
                    sleep_mode_ = false;
                    break;
                case ConsciousnessLevel::Alert:
                    arousal_level_ = 0.7f;
                    sleep_mode_ = false;
                    break;
                case ConsciousnessLevel::Hypervigilant:
                    arousal_level_ = 1.0f;
                    sleep_mode_ = false;
                    break;
            }
        }

        void Thalamus::generateThalamicRhythms(float delta_time) {
            // Generate alpha rhythm (10 Hz)
            static float alpha_phase = 0.0f;
            alpha_phase += TWO_PI * 10.0f * delta_time;
            if (alpha_phase > TWO_PI) alpha_phase -= TWO_PI;
            alpha_rhythm_ = std::sin(alpha_phase) * arousal_level_;
            
            // Generate sleep spindles (12 Hz) during low arousal
            if (arousal_level_ < 0.4f) {
                static float spindle_phase = 0.0f;
                spindle_phase += TWO_PI * 12.0f * delta_time;
                if (spindle_phase > TWO_PI) spindle_phase -= TWO_PI;
                spindle_activity_ = std::sin(spindle_phase) * (0.4f - arousal_level_);
            } else {
                spindle_activity_ = 0.0f;
            }
        }

        void Thalamus::processRegionSpecific(float delta_time) {
            // Generate thalamic rhythms
            generateThalamicRhythms(delta_time);
            
            // Process thalamic nuclei
            for (auto& [nucleus, neurons] : nucleus_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        // Modulate with thalamic rhythms and arousal
                        float rhythm_modulation = 0.5f + 0.5f * (alpha_rhythm_ + spindle_activity_);
                        neuron->process(delta_time * rhythm_modulation * arousal_level_);
                    }
                }
            }
        }

        // ===== Brainstem Implementation =====

        Brainstem::Brainstem(const std::string& name, std::size_t neuron_count)
            : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Brainstem, Core::Region::ActivationPattern::Asynchronous)
            , arousal_output_(0.5f)
            , is_awake_(true)
            , consciousness_threshold_(0.3f)
        {
            createNeurons(neuron_count);
            initializeBrainstemAreas();
            
            // Initialize vital signs
            vital_signs_[VitalFunction::Breathing] = {VitalFunction::Breathing, 16.0f, 16.0f, 1.0f, true};
            vital_signs_[VitalFunction::HeartRate] = {VitalFunction::HeartRate, 70.0f, 70.0f, 1.0f, true};
            vital_signs_[VitalFunction::BloodPressure] = {VitalFunction::BloodPressure, 120.0f, 120.0f, 1.0f, true};
            vital_signs_[VitalFunction::Temperature] = {VitalFunction::Temperature, 37.0f, 37.0f, 1.0f, true};
            
            // Initialize neurotransmitter levels
            neurotransmitter_levels_["dopamine"] = 0.5f;
            neurotransmitter_levels_["serotonin"] = 0.5f;
            neurotransmitter_levels_["norepinephrine"] = 0.5f;
            neurotransmitter_levels_["acetylcholine"] = 0.5f;
        }

        void Brainstem::initializeBrainstemAreas() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_area = neurons.size() / 4; // Divide among 4 brainstem areas
            
            std::size_t neuron_index = 0;
            for (int area = 0; area < 4; ++area) {
                BrainstemArea bs_area = static_cast<BrainstemArea>(area);
                std::vector<NeuroForge::NeuronPtr> area_neurons;
                
                for (std::size_t i = 0; i < neurons_per_area && neuron_index < neurons.size(); ++i) {
                    area_neurons.push_back(neurons[neuron_index++]);
                }
                
                area_neurons_[bs_area] = std::move(area_neurons);
            }
        }

        void Brainstem::regulateVitalFunction(VitalFunction function, float target_value) {
            auto it = vital_signs_.find(function);
            if (it != vital_signs_.end()) {
                it->second.target_value = target_value;
            }
        }

        float Brainstem::getVitalSign(VitalFunction function) const {
            auto it = vital_signs_.find(function);
            if (it != vital_signs_.end()) {
                return it->second.current_value;
            }
            return 0.0f;
        }

        void Brainstem::updateVitalFunctions(float delta_time) {
            for (auto& [function, sign] : vital_signs_) {
                if (sign.is_automatic) {
                    // Simple regulation toward target value
                    float error = sign.target_value - sign.current_value;
                    sign.current_value += error * sign.regulation_strength * delta_time;
                }
            }
        }

        void Brainstem::modulateArousal(float arousal_level) {
            arousal_output_ = std::clamp(arousal_level, 0.0f, 1.0f);
            is_awake_ = (arousal_output_ > consciousness_threshold_);
        }

        void Brainstem::addReflexArc(const std::string& reflex_id,
                                   const std::vector<float>& trigger,
                                   const std::vector<float>& response) {
            ReflexArc arc;
            arc.reflex_id = reflex_id;
            arc.trigger_pattern = trigger;
            arc.response_pattern = response;
            arc.reflex_strength = 1.0f;
            arc.latency_ms = 50.0f; // 50ms typical reflex latency
            arc.is_active = true;
            
            reflex_arcs_[reflex_id] = arc;
        }

        std::vector<float> Brainstem::processReflex(const std::vector<float>& stimulus) {
            for (auto& [reflex_id, arc] : reflex_arcs_) {
                if (!arc.is_active) continue;
                
                // Check if stimulus matches trigger pattern
                float similarity = 0.0f;
                for (std::size_t i = 0; i < std::min(stimulus.size(), arc.trigger_pattern.size()); ++i) {
                    similarity += 1.0f - std::abs(stimulus[i] - arc.trigger_pattern[i]);
                }
                similarity /= std::min(stimulus.size(), arc.trigger_pattern.size());
                
                if (similarity > 0.8f) {
                    // Trigger reflex response
                    std::vector<float> response = arc.response_pattern;
                    for (float& value : response) {
                        value *= arc.reflex_strength;
                    }
                    return response;
                }
            }
            
            return {}; // No reflex triggered
        }

        void Brainstem::processRegionSpecific(float delta_time) {
            // Update vital functions
            updateVitalFunctions(delta_time);
            
            // Process brainstem areas
            for (auto& [area, neurons] : area_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        neuron->process(delta_time);
                    }
                }
            }
            
            // Update neurotransmitter levels (simplified)
            for (auto& [transmitter, level] : neurotransmitter_levels_) {
                // Maintain homeostatic levels
                if (level < 0.5f) {
                    level += 0.1f * delta_time;
                } else if (level > 0.5f) {
                    level -= 0.1f * delta_time;
                }
                level = std::clamp(level, 0.0f, 1.0f);
            }
        }

    } // namespace Regions
} // namespace NeuroForge


namespace {
// Register subcortical region factories
static const NeuroForge::Core::RegisterRegionFactory reg_hipp(
    "hippocampus",
    [](const std::string& name, std::size_t n){ return std::make_shared<NeuroForge::Regions::Hippocampus>(name, n); }
);
static const NeuroForge::Core::RegisterRegionAlias alias_hpc("hpc", "hippocampus");

static const NeuroForge::Core::RegisterRegionFactory reg_amyg(
    "amygdala",
    [](const std::string& name, std::size_t n){ return std::make_shared<NeuroForge::Regions::Amygdala>(name, n); }
);

static const NeuroForge::Core::RegisterRegionFactory reg_thal(
    "thalamus",
    [](const std::string& name, std::size_t n){ return std::make_shared<NeuroForge::Regions::Thalamus>(name, n); }
);

static const NeuroForge::Core::RegisterRegionFactory reg_brainstem(
    "brainstem",
    [](const std::string& name, std::size_t n){ return std::make_shared<NeuroForge::Regions::Brainstem>(name, n); }
);
}

// Expose a no-op symbol so the main executable can force-link this TU and run static registrars
extern "C" void NF_ForceLink_SubcorticalRegions() {}