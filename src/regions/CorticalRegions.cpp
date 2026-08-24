#include "regions/CorticalRegions.h"
#include <algorithm>
#include <random>
#include <cmath>
#include <complex>
#include "core/RegionRegistry.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
    // Simple FFT implementation (Cooley-Tukey)
    // Input: data (real), Output: complex spectrum
    // Note: data size must be power of 2
    void fft(std::vector<std::complex<float>>& x) {
        const size_t N = x.size();
        if (N <= 1) return;

        std::vector<std::complex<float>> even(N / 2);
        std::vector<std::complex<float>> odd(N / 2);

        for (size_t i = 0; i < N / 2; ++i) {
            even[i] = x[2 * i];
            odd[i] = x[2 * i + 1];
        }

        fft(even);
        fft(odd);

        for (size_t k = 0; k < N / 2; ++k) {
            std::complex<float> t = std::polar(1.0f, -2.0f * (float)M_PI * k / N) * odd[k];
            x[k] = even[k] + t;
            x[k + N / 2] = even[k] - t;
        }
    }

    // Apply Hanning Window
    void applyWindow(std::vector<float>& data) {
        const size_t N = data.size();
        for (size_t i = 0; i < N; ++i) {
            float multiplier = 0.5f * (1.0f - std::cos(2.0f * (float)M_PI * i / (N - 1)));
            data[i] *= multiplier;
        }
    }
}

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
            
            // Map the input across the region's neurons in order, so neuron
            // position corresponds to position in the visual field.
            //
            // This previously looped over layer_neurons_ and wrote
            // visual_input[i] into neuron i of EVERY layer. Two consequences,
            // both silent: only the first neurons_per_layer values of the input
            // were ever read -- 16 of 64 for a 64-neuron region, so 75% of the
            // field was discarded -- and all four layers received an identical
            // copy of them. Any downstream reader splitting the region's neurons
            // to recover spatial structure therefore saw the same values on both
            // sides. Measured 2026-08-24: a left/right split of VisualCortex
            // returned a difference of ~0 by construction, which left a
            // phototaxis policy unable to tell left from right and performing at
            // the random-walk baseline.
            //
            // Writing straight into the region's neuron vector keeps input index
            // and neuron index aligned. Layers still share those neurons via
            // layer_neurons_, so layer-wise processing is unaffected.
            const auto& all_neurons = getNeurons();
            const std::size_t n = std::min(all_neurons.size(), visual_input.size());
            for (std::size_t i = 0; i < n; ++i) {
                if (all_neurons[i]) {
                    float v = std::clamp(visual_input[i], 0.0f, 1.0f);
                    all_neurons[i]->setActivation(v);
                    // Ensure process() can register a threshold crossing and emit a spike callback
                    all_neurons[i]->setState(Core::Neuron::State::Inactive);
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
            
            // 1. Prepare data for FFT (Pad to power of 2)
            size_t n = 1;
            while (n < audio_input.size()) n <<= 1;
            
            std::vector<std::complex<float>> spectrum(n);
            std::vector<float> windowed_input = audio_input;
            windowed_input.resize(n, 0.0f);
            
            // 2. Apply Window
            applyWindow(windowed_input);
            
            for (size_t i = 0; i < n; ++i) {
                spectrum[i] = windowed_input[i];
            }
            
            // 3. Compute FFT
            fft(spectrum);
            
            // 4. Compute Magnitude Spectrum (only first half needed)
            std::vector<float> magnitudes(n / 2);
            float max_mag = 0.0f;
            for (size_t i = 0; i < n / 2; ++i) {
                magnitudes[i] = std::abs(spectrum[i]);
                if (magnitudes[i] > max_mag) max_mag = magnitudes[i];
            }
            
            // Normalize
            if (max_mag > 0.0f) {
                for (auto& m : magnitudes) m /= max_mag;
            }
            
            // 5. Map to Neurons (Tonotopic Mapping)
            // Assuming 16kHz sample rate for mapping context, 
            // Nyquist is 8kHz. frequency_map_ goes up to 20kHz, so we clip.
            float sample_rate = 16000.0f; 
            float hz_per_bin = sample_rate / n;
            
            auto& a1_neurons = area_neurons_[AuditoryArea::A1];
            
            for (std::size_t i = 0; i < a1_neurons.size(); ++i) {
                if (!a1_neurons[i]) continue;
                
                // Get preferred frequency from map (mapped 1:1 with neurons index for now)
                // In reality, A1 neurons would be a subset, but let's assume direct mapping 
                // or we use the global frequency_map_ if it aligns.
                // The initializeTonotopicMap() logic maps ALL neurons to frequencies.
                // Let's look up the frequency for this specific neuron index.
                // Since A1 is a subset, we need to find which global index it corresponds to.
                // Simplified: We'll re-calculate preferred frequency for A1 specifically.
                
                float normalized_pos = static_cast<float>(i) / a1_neurons.size();
                float preferred_freq = 20.0f * std::pow(1000.0f, normalized_pos); // 20Hz - 20kHz
                
                // Find FFT bin
                size_t bin = static_cast<size_t>(preferred_freq / hz_per_bin);
                
                float activation = 0.0f;
                if (bin < magnitudes.size()) {
                    activation = magnitudes[bin];
                    
                    // Add some spectral spreading (leakage simulation / overlapping receptive fields)
                    // Reduced spreading to maintain peak sharpness for detection
                    if (bin > 0) activation += 0.2f * magnitudes[bin-1];
                    if (bin < magnitudes.size() - 1) activation += 0.2f * magnitudes[bin+1];
                    activation = std::min(1.0f, activation);
                }
                
                a1_neurons[i]->setActivation(activation);
                a1_neurons[i]->setState(Core::Neuron::State::Inactive); // Ready to fire in process()
            }
            
            // 6. Process Higher Areas (A2, Planum, STG)
            // In a real brain, A1 feeds A2. Here we simulate "feature extraction" driving higher areas.
            // detecting features will populate detected_sounds_, which we can then use to drive higher areas.
            detectFeatures();
        }

        std::vector<float> AuditoryCortex::analyzeFrequencies(const std::vector<float>& frequencies) {
            // Deprecated helper, but keeping for interface compatibility
            // Just returns the frequencies that match our tonotopic map
            return frequencies;
        }

        std::vector<AuditoryCortex::SoundFeature> AuditoryCortex::detectFeatures() {
            detected_sounds_.clear();
            
            // Analyze A1 activity to find features
            const auto& a1_neurons = area_neurons_[AuditoryArea::A1];
            if (a1_neurons.empty()) return detected_sounds_;
            
            // 1. Peak Detection (Formants)
            std::vector<std::pair<float, float>> peaks; // (frequency, magnitude)
            
            for (size_t i = 2; i < a1_neurons.size() - 2; ++i) {
                float v = a1_neurons[i]->getActivation();
                if (v > 0.2f && // Lower threshold slightly 
                    v >= a1_neurons[i-1]->getActivation() && // Allow equal (plateau)
                    v >= a1_neurons[i+1]->getActivation()) {
                    
                    // Check neighbors further out to ensure it's a local peak/plateau
                    if (v > a1_neurons[i-2]->getActivation() && v > a1_neurons[i+2]->getActivation()) {
                        float normalized_pos = static_cast<float>(i) / a1_neurons.size();
                        float freq = 20.0f * std::pow(1000.0f, normalized_pos);
                        peaks.push_back({freq, v});
                    }
                }
            }
            
            // 2. Feature Classification
            if (peaks.empty()) return detected_sounds_;
            
            // Sort by magnitude
            std::sort(peaks.begin(), peaks.end(), [](const auto& a, const auto& b) {
                return a.second > b.second;
            });
            
            // Check for Pitch (Fundamental Frequency) - usually the lowest strong peak
            // Increased threshold to 1000Hz to cover female/child speech and music
            if (peaks[0].first < 1000.0f) {
                detected_sounds_.push_back(SoundFeature::Pitch);
            }
            
            // Check for Formants (Vowel-like structure)
            // Needs at least 2 distinct peaks in speech range
            int speech_peaks = 0;
            for (const auto& p : peaks) {
                if (p.first > 200.0f && p.first < 4000.0f) speech_peaks++;
            }
            
            if (speech_peaks >= 2) {
                detected_sounds_.push_back(SoundFeature::Phoneme);
                
                // Activate Planum Temporale (Language Area)
                auto& pt_neurons = area_neurons_[AuditoryArea::Planum];
                thread_local std::mt19937 rng(1337u);
                std::bernoulli_distribution fire(0.1);
                for (auto& n : pt_neurons) {
                    // Random sparse activation to simulate phoneme recognition
                    if (fire(rng)) {
                        n->setActivation(0.8f); 
                    }
                }
            }
            
            // Check for Rhythm (Broadband bursts)
            // If many peaks across spectrum
            if (peaks.size() > 5) {
                detected_sounds_.push_back(SoundFeature::Timbre);
            }
            
            return detected_sounds_;
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
            
            // Continuous feature detection
            // detectFeatures(); // Already called in processAudioInput for efficiency
            
            // Apply auditory attention
            if (auditory_attention_ > 0.0f) {
                // Enhance processing in attended areas
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

        PrefrontalCortex::Decision PrefrontalCortex::makeDecisionLearned(
            const std::vector<float>& options,
            const std::vector<float>& values,
            std::size_t n_actions,
            float temperature,
            std::mt19937& rng) {

            Decision decision;
            decision.decision_id = "learned_" + std::to_string(decision_queue_.size());
            decision.options = options;
            decision.option_values = values;
            decision.confidence = 0.0f;
            decision.selected_option = 0;
            decision.is_final = false;

            if (n_actions == 0) {
                return decision;
            }

            // Observation: the two input vectors concatenated, plus a bias term
            // so an action can acquire an unconditional preference.
            policy_last_x_.clear();
            policy_last_x_.reserve(options.size() + values.size() + 1);
            policy_last_x_.insert(policy_last_x_.end(), options.begin(), options.end());
            policy_last_x_.insert(policy_last_x_.end(), values.begin(), values.end());
            policy_last_x_.push_back(1.0f);
            const std::size_t n_features = policy_last_x_.size();

            // Weights start at zero, so the initial policy is uniform: no action
            // is preferred before anything has been learned, and the first
            // episodes are pure exploration rather than an arbitrary bias.
            if (policy_w_.size() != n_actions ||
                (!policy_w_.empty() && policy_w_[0].size() != n_features)) {
                policy_w_.assign(n_actions, std::vector<float>(n_features, 0.0f));
            }

            std::vector<float> scores(n_actions, 0.0f);
            for (std::size_t a = 0; a < n_actions; ++a) {
                float acc = 0.0f;
                for (std::size_t f = 0; f < n_features; ++f) {
                    acc += policy_w_[a][f] * policy_last_x_[f];
                }
                scores[a] = acc;
            }

            const float temp = (temperature > 1e-4f) ? temperature : 1e-4f;
            const float max_score = *std::max_element(scores.begin(), scores.end());
            float sum_exp = 0.0f;
            policy_last_p_.assign(n_actions, 0.0f);
            for (std::size_t a = 0; a < n_actions; ++a) {
                // Subtract the max before exponentiating; without it a confident
                // policy overflows to inf and the distribution becomes NaN.
                policy_last_p_[a] = std::exp((scores[a] - max_score) / temp);
                sum_exp += policy_last_p_[a];
            }
            for (auto& pv : policy_last_p_) {
                pv /= sum_exp;
            }

            // Sample rather than argmax. This IS the exploration.
            std::uniform_real_distribution<float> u(0.0f, 1.0f);
            float r = u(rng);
            std::size_t chosen = n_actions - 1;
            float cum = 0.0f;
            for (std::size_t a = 0; a < n_actions; ++a) {
                cum += policy_last_p_[a];
                if (r <= cum) {
                    chosen = a;
                    break;
                }
            }

            policy_last_action_ = chosen;
            policy_has_trace_ = true;

            decision.selected_option = chosen;
            decision.confidence = policy_last_p_[chosen];
            decision.is_final = true;
            decision_queue_.push(decision);
            return decision;
        }

        void PrefrontalCortex::reinforcePolicy(float reward, float learning_rate) {
            if (!policy_has_trace_ || policy_w_.empty()) {
                return;
            }
            const std::size_t n_actions = policy_w_.size();
            const std::size_t n_features = policy_last_x_.size();
            if (policy_last_p_.size() != n_actions) {
                return;
            }
            for (std::size_t a = 0; a < n_actions; ++a) {
                const float indicator = (a == policy_last_action_) ? 1.0f : 0.0f;
                const float advantage = indicator - policy_last_p_[a];
                if (advantage == 0.0f) {
                    continue;
                }
                const float scale = learning_rate * reward * advantage;
                for (std::size_t f = 0; f < n_features; ++f) {
                    policy_w_[a][f] += scale * policy_last_x_[f];
                    // Bound the weights. An unbounded linear score saturates the
                    // softmax, after which the policy stops exploring and cannot
                    // recover from a bad early run of rewards.
                    policy_w_[a][f] = std::max(-10.0f, std::min(10.0f, policy_w_[a][f]));
                }
            }
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
