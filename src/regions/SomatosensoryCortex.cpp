#include "regions/CorticalRegions.h"
#include "core/Neuron.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

namespace NeuroForge {
    namespace Regions {

        SomatosensoryCortex::SomatosensoryCortex(const std::string& name, std::size_t neuron_count)
            : Region(Core::RegionFactory::getNextId(), name, Core::Region::Type::Cortical, Core::Region::ActivationPattern::Synchronous)
            , tactile_sensitivity_(0.7f)
            , pain_threshold_(0.6f)
            , proprioceptive_accuracy_(0.8f)
            , adaptation_rate_(0.05f)
            , cross_modal_integration_(0.5f)
        {
            createNeurons(neuron_count);
            initializeSomatotopicMap();
            initializeModalityProcessing();
            initializeCorticalMagnification();
        }

        void SomatosensoryCortex::initializeSomatotopicMap() {
            const auto& neurons = getNeurons();
            
            // Cortical magnification factors (based on biological data)
            cortical_magnification_[BodyRegion::Face] = 0.25f;      // Large representation
            cortical_magnification_[BodyRegion::Lips] = 0.15f;      // Very large representation
            cortical_magnification_[BodyRegion::Tongue] = 0.10f;    // Large representation
            cortical_magnification_[BodyRegion::Hands] = 0.20f;     // Very large representation
            cortical_magnification_[BodyRegion::Fingers] = 0.15f;   // Large representation
            cortical_magnification_[BodyRegion::Arms] = 0.05f;      // Medium representation
            cortical_magnification_[BodyRegion::Torso] = 0.03f;     // Small representation
            cortical_magnification_[BodyRegion::Legs] = 0.04f;      // Small representation
            cortical_magnification_[BodyRegion::Feet] = 0.02f;      // Small representation
            cortical_magnification_[BodyRegion::Genitals] = 0.01f;  // Small representation

            // Distribute neurons according to cortical magnification
            std::size_t neuron_index = 0;
            for (const auto& [body_region, magnification] : cortical_magnification_) {
                std::size_t region_neurons = static_cast<std::size_t>(neurons.size() * magnification);
                std::vector<NeuroForge::NeuronPtr> region_neuron_group;
                
                for (std::size_t i = 0; i < region_neurons && neuron_index < neurons.size(); ++i) {
                    region_neuron_group.push_back(neurons[neuron_index++]);
                }
                
                somatotopic_map_[body_region] = std::move(region_neuron_group);
            }

            // Initialize somatosensory areas
            std::size_t neurons_per_area = neurons.size() / 4;
            neuron_index = 0;
            
            for (int area = 0; area < 4; ++area) {
                SomatosensoryArea som_area = static_cast<SomatosensoryArea>(area);
                std::vector<NeuroForge::NeuronPtr> area_neurons;
                
                for (std::size_t i = 0; i < neurons_per_area && neuron_index < neurons.size(); ++i) {
                    area_neurons.push_back(neurons[neuron_index++]);
                }
                
                area_neurons_[som_area] = std::move(area_neurons);
            }
        }

        void SomatosensoryCortex::initializeModalityProcessing() {
            const auto& neurons = getNeurons();
            std::size_t neurons_per_modality = neurons.size() / 7; // 7 sensory modalities
            std::size_t neuron_index = 0;
            
            // Distribute neurons among sensory modalities
            for (int modality = 0; modality < 7; ++modality) {
                SensoryModality sens_modality = static_cast<SensoryModality>(modality);
                std::vector<NeuroForge::NeuronPtr> modality_neuron_group;
                
                for (std::size_t i = 0; i < neurons_per_modality && neuron_index < neurons.size(); ++i) {
                    modality_neuron_group.push_back(neurons[neuron_index++]);
                }
                
                modality_neurons_[sens_modality] = std::move(modality_neuron_group);
            }
        }

        void SomatosensoryCortex::initializeCorticalMagnification() {
            // Initialize receptive field sizes (smaller for more sensitive areas)
            receptive_field_sizes_[BodyRegion::Face] = {2.0f, 2.0f};
            receptive_field_sizes_[BodyRegion::Lips] = {1.0f, 1.0f};
            receptive_field_sizes_[BodyRegion::Tongue] = {0.5f, 0.5f};
            receptive_field_sizes_[BodyRegion::Hands] = {1.5f, 1.5f};
            receptive_field_sizes_[BodyRegion::Fingers] = {1.0f, 1.0f};
            receptive_field_sizes_[BodyRegion::Arms] = {5.0f, 5.0f};
            receptive_field_sizes_[BodyRegion::Torso] = {8.0f, 8.0f};
            receptive_field_sizes_[BodyRegion::Legs] = {6.0f, 6.0f};
            receptive_field_sizes_[BodyRegion::Feet] = {4.0f, 4.0f};
            receptive_field_sizes_[BodyRegion::Genitals] = {3.0f, 3.0f};
        }

        void SomatosensoryCortex::processSomatosensoryInput(const SomatosensoryInput& input) {
            // Add to active inputs
            active_inputs_.push_back(input);
            
            // Process based on modality
            auto modality_it = modality_neurons_.find(input.modality);
            if (modality_it != modality_neurons_.end()) {
                // Activate modality-specific neurons
                float activation_strength = input.intensity * tactile_sensitivity_;
                
                for (auto& neuron : modality_it->second) {
                    if (neuron && activation_strength > 0.1f) {
                        neuron->setState(Core::Neuron::State::Active);
                        // Apply temporal dynamics for sustained input
                        if (input.duration > 0.1f) {
                            neuron->setActivation(activation_strength);
                        }
                    }
                }
            }
            
            // Process somatotopic activation
            auto region_it = somatotopic_map_.find(input.body_region);
            if (region_it != somatotopic_map_.end()) {
                float spatial_activation = input.intensity;
                
                // Apply cortical magnification
                auto mag_it = cortical_magnification_.find(input.body_region);
                if (mag_it != cortical_magnification_.end()) {
                    spatial_activation *= (1.0f + mag_it->second);
                }
                
                // Activate somatotopic neurons
                for (std::size_t i = 0; i < region_it->second.size() && i < input.spatial_pattern.size(); ++i) {
                    if (region_it->second[i] && input.spatial_pattern[i] > 0.1f) {
                        region_it->second[i]->setState(Core::Neuron::State::Active);
                        region_it->second[i]->setActivation(spatial_activation * input.spatial_pattern[i]);
                    }
                }
            }
            
            // Handle noxious stimuli (pain processing)
            if (input.is_noxious || input.intensity > pain_threshold_) {
                processNociceptiveSignals(0.01f); // Process pain immediately
            }
        }

        void SomatosensoryCortex::processTactileInput(BodyRegion body_region, const std::vector<float>& tactile_data) {
            SomatosensoryInput input;
            input.body_region = body_region;
            input.modality = SensoryModality::Touch;
            input.intensity = *std::max_element(tactile_data.begin(), tactile_data.end());
            input.duration = 0.1f; // Default duration
            input.spatial_pattern = tactile_data;
            input.temporal_frequency = 0.0f;
            input.is_noxious = false;
            
            processSomatosensoryInput(input);
            
            // Detect tactile features
            auto features = detectTactileFeatures(tactile_data);
            detected_features_.insert(detected_features_.end(), features.begin(), features.end());
        }

        void SomatosensoryCortex::processProprioceptiveInput(const std::vector<float>& joint_angles,
                                                           const std::vector<float>& muscle_tensions) {
            // Process joint position information
            for (std::size_t i = 0; i < joint_angles.size(); ++i) {
                SomatosensoryInput input;
                input.body_region = static_cast<BodyRegion>(i % 10); // Map to body regions
                input.modality = SensoryModality::Proprioception;
                input.intensity = std::abs(joint_angles[i]) * proprioceptive_accuracy_;
                input.duration = 0.05f;
                input.spatial_pattern = {joint_angles[i]};
                input.temporal_frequency = 0.0f;
                input.is_noxious = false;
                
                processSomatosensoryInput(input);
            }
            
            // Process muscle tension information
            for (std::size_t i = 0; i < muscle_tensions.size(); ++i) {
                SomatosensoryInput input;
                input.body_region = static_cast<BodyRegion>(i % 10);
                input.modality = SensoryModality::Pressure;
                input.intensity = muscle_tensions[i];
                input.duration = 0.05f;
                input.spatial_pattern = {muscle_tensions[i]};
                input.temporal_frequency = 0.0f;
                input.is_noxious = muscle_tensions[i] > 0.8f; // High tension can be painful
                
                processSomatosensoryInput(input);
            }
        }

        void SomatosensoryCortex::processPainSignals(BodyRegion body_region, float pain_intensity, 
                                                   const std::string& pain_type) {
            SomatosensoryInput input;
            input.body_region = body_region;
            input.modality = SensoryModality::Pain;
            input.intensity = pain_intensity;
            input.duration = 1.0f; // Pain persists longer
            input.spatial_pattern = {pain_intensity};
            input.temporal_frequency = 0.0f;
            input.is_noxious = true;
            
            processSomatosensoryInput(input);
            
            // Activate insular cortex for pain processing
            auto insula_it = area_neurons_.find(SomatosensoryArea::Insula);
            if (insula_it != area_neurons_.end()) {
                for (auto& neuron : insula_it->second) {
                    if (neuron && pain_intensity > pain_threshold_) {
                        neuron->setState(Core::Neuron::State::Active);
                        neuron->setActivation(pain_intensity);
                    }
                }
            }
        }

        void SomatosensoryCortex::processTemperatureInput(BodyRegion body_region, float temperature, bool is_noxious) {
            SomatosensoryInput input;
            input.body_region = body_region;
            input.modality = SensoryModality::Temperature;
            input.intensity = std::abs(temperature - 37.0f) / 20.0f; // Normalize around body temp
            input.duration = 0.2f;
            input.spatial_pattern = {temperature};
            input.temporal_frequency = 0.0f;
            input.is_noxious = is_noxious || temperature < 10.0f || temperature > 45.0f;
            
            processSomatosensoryInput(input);
        }

        std::vector<SomatosensoryCortex::TactileFeature> SomatosensoryCortex::detectTactileFeatures(const std::vector<float>& tactile_data) {
            std::vector<TactileFeature> features;
            
            if (tactile_data.empty()) return features;
            
            // Edge detection
            for (std::size_t i = 1; i < tactile_data.size(); ++i) {
                if (std::abs(tactile_data[i] - tactile_data[i-1]) > 0.3f) {
                    features.push_back(TactileFeature::EdgeDetection);
                    break;
                }
            }
            
            // Texture roughness
            float variance = 0.0f;
            float mean = std::accumulate(tactile_data.begin(), tactile_data.end(), 0.0f) / tactile_data.size();
            for (float value : tactile_data) {
                variance += (value - mean) * (value - mean);
            }
            variance /= tactile_data.size();
            
            if (variance > 0.1f) {
                features.push_back(TactileFeature::TextureRoughness);
            }
            
            // Pressure distribution
            float max_pressure = *std::max_element(tactile_data.begin(), tactile_data.end());
            if (max_pressure > 0.7f) {
                features.push_back(TactileFeature::PressureDistribution);
            }
            
            // Material hardness (based on pressure gradient)
            float pressure_gradient = 0.0f;
            for (std::size_t i = 1; i < tactile_data.size(); ++i) {
                pressure_gradient += std::abs(tactile_data[i] - tactile_data[i-1]);
            }
            pressure_gradient /= tactile_data.size();
            
            if (pressure_gradient > 0.2f) {
                features.push_back(TactileFeature::MaterialHardness);
            }
            
            return features;
        }

        std::unordered_map<SomatosensoryCortex::BodyRegion, std::vector<float>> SomatosensoryCortex::getBodySchema() const {
            std::unordered_map<BodyRegion, std::vector<float>> body_schema;
            
            for (const auto& [body_region, neurons] : somatotopic_map_) {
                std::vector<float> region_state;
                for (const auto& neuron : neurons) {
                    if (neuron) {
                        region_state.push_back(neuron->getActivation());
                    }
                }
                body_schema[body_region] = region_state;
            }
            
            return body_schema;
        }

        void SomatosensoryCortex::updateCorticalPlasticity(BodyRegion body_region, float usage_frequency) {
            // Update cortical magnification based on usage
            auto mag_it = cortical_magnification_.find(body_region);
            if (mag_it != cortical_magnification_.end()) {
                float plasticity_change = usage_frequency * adaptation_rate_;
                mag_it->second += plasticity_change;
                
                // Clamp to reasonable bounds
                mag_it->second = std::clamp(mag_it->second, 0.01f, 0.5f);
            }
            
            // Update receptive field sizes
            auto rf_it = receptive_field_sizes_.find(body_region);
            if (rf_it != receptive_field_sizes_.end()) {
                // More usage leads to smaller, more precise receptive fields
                float size_change = -usage_frequency * adaptation_rate_ * 0.1f;
                rf_it->second.first += size_change;
                rf_it->second.second += size_change;
                
                // Clamp to reasonable bounds
                rf_it->second.first = std::clamp(rf_it->second.first, 0.5f, 10.0f);
                rf_it->second.second = std::clamp(rf_it->second.second, 0.5f, 10.0f);
            }
        }

        void SomatosensoryCortex::processRegionSpecific(float delta_time) {
            // Process all somatosensory areas
            for (auto& [area, neurons] : area_neurons_) {
                for (auto& neuron : neurons) {
                    if (neuron) {
                        neuron->process(delta_time);
                    }
                }
            }
            
            // Process modality integration
            processModalityIntegration(delta_time);
            
            // Update somatotopic plasticity
            updateSomatotopicPlasticity(delta_time);
            
            // Process nociceptive signals
            processNociceptiveSignals(delta_time);
            
            // Decay active inputs over time
            active_inputs_.erase(
                std::remove_if(active_inputs_.begin(), active_inputs_.end(),
                    [delta_time](SomatosensoryInput& input) {
                        input.duration -= delta_time;
                        return input.duration <= 0.0f;
                    }),
                active_inputs_.end()
            );
            
            // Clear detected features (they will be regenerated next frame)
            detected_features_.clear();
        }

        void SomatosensoryCortex::processModalityIntegration(float delta_time) {
            // Cross-modal integration between different sensory modalities
            if (cross_modal_integration_ > 0.0f) {
                for (auto& [modality1, neurons1] : modality_neurons_) {
                    for (auto& [modality2, neurons2] : modality_neurons_) {
                        if (modality1 != modality2) {
                            // Simple cross-modal enhancement
                            float integration_strength = cross_modal_integration_ * delta_time;
                            
                            for (std::size_t i = 0; i < std::min(neurons1.size(), neurons2.size()); ++i) {
                                if (neurons1[i] && neurons2[i] && 
                                    neurons1[i]->getState() == Core::Neuron::State::Active) {
                                    float enhanced_activation = neurons2[i]->getActivation() * 
                                                              (1.0f + integration_strength);
                                    neurons2[i]->setActivation(enhanced_activation);
                                }
                            }
                        }
                    }
                }
            }
        }

        void SomatosensoryCortex::updateSomatotopicPlasticity(float delta_time) {
            // Update plasticity based on current activity patterns
            for (auto& [body_region, neurons] : somatotopic_map_) {
                float activity_level = 0.0f;
                std::size_t active_neurons = 0;
                
                for (const auto& neuron : neurons) {
                    if (neuron && neuron->getState() == Core::Neuron::State::Active) {
                        activity_level += neuron->getActivation();
                        active_neurons++;
                    }
                }
                
                if (active_neurons > 0) {
                    activity_level /= active_neurons;
                    updateCorticalPlasticity(body_region, activity_level * delta_time);
                }
            }
        }

        void SomatosensoryCortex::processNociceptiveSignals(float delta_time) {
            // Process pain signals with special attention to insular cortex
            auto insula_it = area_neurons_.find(SomatosensoryArea::Insula);
            if (insula_it != area_neurons_.end()) {
                for (const auto& input : active_inputs_) {
                    if (input.is_noxious || input.intensity > pain_threshold_) {
                        // Enhance insular cortex activity for pain processing
                        for (auto& neuron : insula_it->second) {
                            if (neuron) {
                                float pain_enhancement = input.intensity * (1.0f + delta_time);
                                neuron->setActivation(pain_enhancement);
                                neuron->setState(Core::Neuron::State::Active);
                            }
                        }
                    }
                }
            }
        }

    } // namespace Regions
} // namespace NeuroForge