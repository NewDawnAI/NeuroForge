#include "regions/CorticalRegions.h"
#include <algorithm>
#include <random>
#include <cmath>
#include "core/RegionRegistry.h"

namespace NeuroForge {
    namespace Regions {

        // ===== VisualCortex Implementation =====

        VisualCortex::VisualCortex(const std::string& name, std::size_t neuron_count)
            : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Cortical, Core::Region::ActivationPattern::Layered)
            , visual_attention_focus_(0.5f)
            , receptive_field_center_{0.0f, 0.0f}
            , receptive_field_size_(1.0f)
        {
            // Initialize with specified neuron count
            createNeurons(neuron_count);
            initializeLayers();
        }

        void VisualCortex::initializeLayers() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_layer = neurons.size() / 4; // Divide among 4 layers
            
            std::size_t neuron_index = 0;
            for (int layer = 0; layer < 4; ++layer) {
                VisualLayer visual_layer = static_cast<VisualLayer>(layer);
                std::vector<NeuroForge::NeuronPtr> layer_neurons;
                
                for (std::size_t i = 0; i < neurons_per_layer && neuron_index < neurons.size(); ++i) {
                    layer_neurons.push_back(neurons[neuron_index++]);
                }
                
                layer_neurons_[visual_layer] = std::move(layer_neurons);
            }
        }

        void VisualCortex::processVisualInput(const std::vector<float>& visual_input) {
            // Basic visual processing - in a full implementation, this would include
            // edge detection, feature extraction, etc.
            
            if (visual_input.empty()) return;
            
            // Drive neuron activations from input so spikes are produced in process()
            for (auto& [layer, neurons] : layer_neurons_) {
                for (std::size_t i = 0; i < neurons.size() && i < visual_input.size(); ++i) {
                    if (neurons[i]) {
                        float v = std::clamp(visual_input[i], 0.0f, 1.0f);
                        neurons[i]->setActivation(v);
                        // Ensure process() can register a threshold crossing and emit a spike callback
                        neurons[i]->setState(Core::Neuron::State::Inactive);
                    }
                }
            }
        }

        std::vector<VisualCortex::FeatureType> VisualCortex::detectFeatures(VisualLayer layer) {
            std::vector<FeatureType> features;
            
            auto layer_it = layer_neurons_.find(layer);
            if (layer_it == layer_neurons_.end()) {
                return features;
            }
            
            // Simulate feature detection based on layer activity
            std::size_t active_neurons = 0;
            for (const auto& neuron : layer_it->second) {
                if (neuron && neuron->isFiring()) {
                    active_neurons++;
                }
            }
            
            // Simple heuristic for feature detection
            float activity_ratio = static_cast<float>(active_neurons) / layer_it->second.size();
            
            switch (layer) {
                case VisualLayer::V1:
                    if (activity_ratio > 0.3f) features.push_back(FeatureType::Edge);
                    if (activity_ratio > 0.5f) features.push_back(FeatureType::Corner);
                    break;
                case VisualLayer::V2:
                    if (activity_ratio > 0.4f) features.push_back(FeatureType::Texture);
                    break;
                case VisualLayer::V4:
                    if (activity_ratio > 0.3f) features.push_back(FeatureType::Color);
                    if (activity_ratio > 0.6f) features.push_back(FeatureType::Shape);
                    break;
                case VisualLayer::IT:
                    if (activity_ratio > 0.5f) features.push_back(FeatureType::Object);
                    break;
            }
            
            detected_features_ = features;
            return features;
        }

        void VisualCortex::processRegionSpecific(float delta_time) {
            // Process each visual layer
            for (auto& [layer, neurons] : layer_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        neuron->process(delta_time);
                    }
                }
                
                // Detect features in this layer
                detectFeatures(layer);
            }
            
            // Apply attention modulation
            if (visual_attention_focus_ > 0.0f) {
                // Enhance processing in attended areas
                // This is a simplified attention mechanism
            }
        }

        // ===== AuditoryCortex Implementation =====

        AuditoryCortex::AuditoryCortex(const std::string& name, std::size_t neuron_count)
            : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Cortical, Core::Region::ActivationPattern::Synchronous)
            , auditory_attention_(0.5f)
            , frequency_resolution_(100.0f)
        {
            createNeurons(neuron_count);
            initializeTonotopicMap();
        }

        void AuditoryCortex::initializeTonotopicMap() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_area = neurons.size() / 4; // Divide among 4 areas
            
            // Initialize frequency map (logarithmic scale from 20Hz to 20kHz)
            frequency_map_.resize(neurons.size());
            for (std::size_t i = 0; i < frequency_map_.size(); ++i) {
                float normalized_pos = static_cast<float>(i) / frequency_map_.size();
                frequency_map_[i] = 20.0f * std::pow(1000.0f, normalized_pos); // 20Hz to 20kHz
            }
            
            std::size_t neuron_index = 0;
            for (int area = 0; area < 4; ++area) {
                AuditoryArea auditory_area = static_cast<AuditoryArea>(area);
                std::vector<NeuroForge::NeuronPtr> area_neurons;
                
                for (std::size_t i = 0; i < neurons_per_area && neuron_index < neurons.size(); ++i) {
                    area_neurons.push_back(neurons[neuron_index++]);
                }
                
                area_neurons_[auditory_area] = std::move(area_neurons);
            }
        }

        void AuditoryCortex::processAudioInput(const std::vector<float>& audio_input) {
            if (audio_input.empty()) return;
            
            // Drive neuron activations from audio features so spikes are produced in process()
            for (auto& [area, neurons] : area_neurons_) {
                for (std::size_t i = 0; i < neurons.size() && i < audio_input.size(); ++i) {
                    if (neurons[i]) {
                        float v = std::clamp(audio_input[i], 0.0f, 1.0f);
                        neurons[i]->setActivation(v);
                        // Ensure process() can register a threshold crossing and emit a spike callback
                        neurons[i]->setState(Core::Neuron::State::Inactive);
                    }
                }
            }
        }

        std::vector<float> AuditoryCortex::analyzeFrequencies(const std::vector<float>& frequencies) {
            std::vector<float> analysis_results;
            
            // Simple frequency analysis - in a full implementation, this would be FFT-based
            for (float freq : frequencies) {
                // Find closest frequency in tonotopic map
                auto closest_it = std::min_element(frequency_map_.begin(), frequency_map_.end(),
                    [freq](float a, float b) {
                        return std::abs(a - freq) < std::abs(b - freq);
                    });
                
                if (closest_it != frequency_map_.end()) {
                    analysis_results.push_back(*closest_it);
                }
            }
            
            return analysis_results;
        }

        void AuditoryCortex::processRegionSpecific(float delta_time) {
            // Process each auditory area
            for (auto& [area, neurons] : area_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        neuron->process(delta_time);
                    }
                }
            }
            
            // Apply auditory attention
            if (auditory_attention_ > 0.0f) {
                // Enhance processing based on attention
            }
        }

        // ===== MotorCortex Implementation =====

        MotorCortex::MotorCortex(const std::string& name, std::size_t neuron_count)
            : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Cortical, Core::Region::ActivationPattern::Asynchronous)
            , motor_learning_rate_(0.1f)
        {
            createNeurons(neuron_count);
            initializeSomatotopicMap();
        }

        void MotorCortex::initializeSomatotopicMap() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_area = neurons.size() / 4; // Divide among 4 motor areas
            std::size_t neurons_per_body_part = neurons.size() / 8; // Divide among 8 body parts
            
            std::size_t neuron_index = 0;
            
            // Initialize motor areas
            for (int area = 0; area < 4; ++area) {
                MotorArea motor_area = static_cast<MotorArea>(area);
                std::vector<NeuroForge::NeuronPtr> area_neurons;
                
                for (std::size_t i = 0; i < neurons_per_area && neuron_index < neurons.size(); ++i) {
                    area_neurons.push_back(neurons[neuron_index++]);
                }
                
                area_neurons_[motor_area] = std::move(area_neurons);
            }
            
            // Initialize somatotopic map
            neuron_index = 0;
            for (int part = 0; part < 8; ++part) {
                BodyPart body_part = static_cast<BodyPart>(part);
                std::vector<NeuroForge::NeuronPtr> part_neurons;
                
                for (std::size_t i = 0; i < neurons_per_body_part && neuron_index < neurons.size(); ++i) {
                    part_neurons.push_back(neurons[neuron_index++]);
                }
                
                somatotopic_map_[body_part] = std::move(part_neurons);
            }
        }

        void MotorCortex::planMovement(BodyPart target_part,
                                      const std::vector<float>& movement_vector,
                                      float force) {
            MotorCommand command;
            command.target_part = target_part;
            command.movement_vector = movement_vector;
            command.force_magnitude = force;
            command.duration = 1.0f; // Default 1 second
            command.is_executed = false;
            
            motor_command_queue_.push(command);
        }

        void MotorCortex::executeMotorCommands() {
            while (!motor_command_queue_.empty()) {
                MotorCommand command = motor_command_queue_.front();
                motor_command_queue_.pop();
                
                // Activate neurons for this body part
                auto part_it = somatotopic_map_.find(command.target_part);
                if (part_it != somatotopic_map_.end()) {
                    for (auto& neuron : part_it->second) {
                        if (neuron) {
                            neuron->setState(Core::Neuron::State::Active);
                        }
                    }
                }
                
                command.is_executed = true;
                active_commands_.push_back(command);
            }
        }

        std::vector<MotorCortex::MotorCommand> MotorCortex::getPendingCommands() const {
            std::vector<MotorCommand> pending;
            std::queue<MotorCommand> temp_queue = motor_command_queue_;
            
            while (!temp_queue.empty()) {
                pending.push_back(temp_queue.front());
                temp_queue.pop();
            }
            
            return pending;
        }

        void MotorCortex::processRegionSpecific(float delta_time) {
            // Execute pending motor commands
            executeMotorCommands();
            
            // Process motor areas
            for (auto& [area, neurons] : area_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        neuron->process(delta_time);
                    }
                }
            }
            
            // Update active commands
            for (auto& command : active_commands_) {
                command.duration -= delta_time;
            }
            
            // Remove completed commands
            active_commands_.erase(
                std::remove_if(active_commands_.begin(), active_commands_.end(),
                    [](const MotorCommand& cmd) { return cmd.duration <= 0.0f; }),
                active_commands_.end());
        }

        // ===== PrefrontalCortex Implementation =====

        PrefrontalCortex::PrefrontalCortex(const std::string& name, std::size_t neuron_count)
            : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Cortical, Core::Region::ActivationPattern::Competitive)
            , cognitive_load_(0.0f)
            , attention_control_strength_(0.5f)
        {
            createNeurons(neuron_count);
            initializeExecutiveFunctions();
        }

        void PrefrontalCortex::initializeExecutiveFunctions() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_area = neurons.size() / 4; // Divide among 4 prefrontal areas
            
            std::size_t neuron_index = 0;
            for (int area = 0; area < 4; ++area) {
                PrefrontalArea pfc_area = static_cast<PrefrontalArea>(area);
                std::vector<NeuroForge::NeuronPtr> area_neurons;
                
                for (std::size_t i = 0; i < neurons_per_area && neuron_index < neurons.size(); ++i) {
                    area_neurons.push_back(neurons[neuron_index++]);
                }
                
                area_neurons_[pfc_area] = std::move(area_neurons);
            }
            
            // Initialize working memory buffer
            working_memory_buffer_.resize(10); // 10 working memory slots
        }

        void PrefrontalCortex::storeInWorkingMemory(const std::vector<float>& information) {
            if (working_memory_buffer_.size() < 10) {
                working_memory_buffer_.push_back(information);
            } else {
                // Replace oldest item (FIFO)
                working_memory_buffer_.erase(working_memory_buffer_.begin());
                working_memory_buffer_.push_back(information);
            }
            
            // Update cognitive load
            cognitive_load_ = static_cast<float>(working_memory_buffer_.size()) / 10.0f;
        }

        PrefrontalCortex::Decision PrefrontalCortex::makeDecision(const std::vector<float>& options,
                                                                 const std::vector<float>& values) {
            Decision decision;
            decision.decision_id = "decision_" + std::to_string(decision_queue_.size());
            decision.options = options;
            decision.option_values = values;
            decision.confidence = 0.0f;
            decision.selected_option = 0;
            decision.is_final = false;
            
            if (!options.empty() && !values.empty()) {
                // Simple decision making: select option with highest value
                auto max_it = std::max_element(values.begin(), values.end());
                decision.selected_option = std::distance(values.begin(), max_it);
                decision.confidence = *max_it;
                decision.is_final = true;
            }
            
            decision_queue_.push(decision);
            return decision;
        }

        void PrefrontalCortex::controlAttention(const std::vector<Core::Region::RegionID>& target_regions,
                                               float strength) {
            (void)target_regions;
            attention_control_strength_ = strength;
            
            // In a full implementation, this would send attention signals to target regions
            // For now, we just store the attention control strength
        }

        void PrefrontalCortex::processRegionSpecific(float delta_time) {
            // Process prefrontal areas
            for (auto& [area, neurons] : area_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        neuron->process(delta_time);
                    }
                }
            }
            
            // Process decisions
            if (!decision_queue_.empty()) {
                // In a full implementation, this would involve complex decision processing
                decision_queue_.pop(); // Remove processed decision
            }
            
            // Update cognitive load based on activity
            std::size_t active_neurons = 0;
            std::size_t total_neurons = 0;
            
            for (const auto& [area, neurons] : area_neurons_) {
                for (const auto& neuron : neurons) {
                    if (neuron) {
                        total_neurons++;
                        if (neuron->isFiring()) {
                            active_neurons++;
                        }
                    }
                }
            }
            
            if (total_neurons > 0) {
                float activity_ratio = static_cast<float>(active_neurons) / total_neurons;
                cognitive_load_ = (cognitive_load_ + activity_ratio) / 2.0f; // Running average
            }
        }

    } // namespace Regions
} // namespace NeuroForge


namespace {
// Register cortical region factories
static const NeuroForge::Core::RegisterRegionFactory reg_visual(
    "visual",
    [](const std::string& name, std::size_t n){ return std::make_shared<NeuroForge::Regions::VisualCortex>(name, n); }
);
static const NeuroForge::Core::RegisterRegionAlias alias_vc("vc", "visual");

static const NeuroForge::Core::RegisterRegionFactory reg_auditory(
    "auditory",
    [](const std::string& name, std::size_t n){ return std::make_shared<NeuroForge::Regions::AuditoryCortex>(name, n); }
);
static const NeuroForge::Core::RegisterRegionAlias alias_ac("ac", "auditory");

static const NeuroForge::Core::RegisterRegionFactory reg_motor(
    "motor",
    [](const std::string& name, std::size_t n){ return std::make_shared<NeuroForge::Regions::MotorCortex>(name, n); }
);
static const NeuroForge::Core::RegisterRegionAlias alias_mc("mc", "motor");

static const NeuroForge::Core::RegisterRegionFactory reg_pfc(
    "pfc",
    [](const std::string& name, std::size_t n){ return std::make_shared<NeuroForge::Regions::PrefrontalCortex>(name, n); }
);

static const NeuroForge::Core::RegisterRegionFactory reg_somatosensory(
    "somatosensory",
    [](const std::string& name, std::size_t n){ return std::make_shared<NeuroForge::Regions::SomatosensoryCortex>(name, n); }
);
static const NeuroForge::Core::RegisterRegionAlias alias_sc("sc", "somatosensory");

// Expose a no-op symbol so the main executable can force-link this TU and run static registrars
extern "C" void NF_ForceLink_CorticalRegions() {}
}