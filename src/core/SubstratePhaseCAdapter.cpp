#include "core/SubstratePhaseCAdapter.h"
#include "core/LanguageSystem.h"
#include "core/Phase6Reasoner.h"
#include "regions/CorticalRegions.h"
#include "sandbox/WebSandbox.h"
#include <iostream>
#include <sstream>
#include <cmath>

namespace NeuroForge {
namespace Core {

SubstratePhaseCAdapter::SubstratePhaseCAdapter(std::shared_ptr<HypergraphBrain> brain,
                      std::shared_ptr<SubstrateWorkingMemory> working_memory,
                      PhaseCCSVLogger& logger,
                      const SubstratePhaseC::Config& cfg)
    : brain_(brain), brain_ptr_(brain.get()), working_memory_(working_memory), logger_(&logger) {
    
    // Initialize substrate Phase C
    SubstratePhaseC::Config config = cfg;
    owned_phase_c_ = std::make_unique<SubstratePhaseC>(brain, working_memory, config);
    substrate_phase_c_ = owned_phase_c_.get();
    
    if (!substrate_phase_c_->initialize()) {
        std::cerr << "[SubstratePhaseCAdapter] Initialization failed. Falling back to legacy Phase C (if applicable)." << std::endl;
        // Handle failure if necessary, though pointer is valid but maybe not functional
    } else {
        // Wire logger JSON sink into substrate for telemetry
        if (logger_) substrate_phase_c_->setJsonSink(logger_->getJsonSink());
    }
}

SubstratePhaseCAdapter::SubstratePhaseCAdapter(SubstratePhaseC& phase_c,
                      HypergraphBrain& brain,
                      PhaseCCSVLogger& logger)
    : brain_(nullptr), brain_ptr_(&brain), working_memory_(nullptr), logger_(&logger), substrate_phase_c_(&phase_c) {
    // No ownership, no initialization (assume already initialized)
}

void SubstratePhaseCAdapter::stepBinding(int step) {
    if (!substrate_phase_c_) return;

    // Set binding goal in substrate instead of external computation
    std::map<std::string, std::string> binding_params;
    
    // Generate binding parameters from substrate state or external input
    std::vector<std::string> colors = {"red", "green", "blue"};
    std::vector<std::string> shapes = {"square", "circle", "triangle"};
    
    binding_params["color"] = colors[step % colors.size()];
    binding_params["shape"] = shapes[(step / 2) % shapes.size()];
    
    // Set goal in substrate
    substrate_phase_c_->setGoal("binding", binding_params);
    
    // Process substrate step
    substrate_phase_c_->processStep(step, 0.1f);
    
    // Get results from substrate behavior
    auto binding_results = substrate_phase_c_->getBindingResults(step);
    
    // Log results using existing logger interface
    if (logger_) {
        for (const auto& binding : binding_results) {
            logger_->logBinding(binding);
        }
    }
    
    // Get assemblies for timeline logging
    auto assemblies = substrate_phase_c_->getCurrentAssemblies();
    if (!assemblies.empty() && logger_) {
        // Convert substrate assembly to Phase C assembly format
        Assembly winner;
        winner.id = 0;
        winner.symbol = assemblies[0].symbol;
        winner.score = assemblies[0].coherence_score;
        
        logger_->logTimeline(step, winner);
        
        // Log all assemblies
        std::vector<Assembly> phase_c_assemblies;
        for (std::size_t i = 0; i < assemblies.size(); ++i) {
            Assembly assembly;
            assembly.id = static_cast<int>(i);
            assembly.symbol = assemblies[i].symbol;
            assembly.score = assemblies[i].coherence_score;
            phase_c_assemblies.push_back(assembly);
        }
        logger_->logAssemblies(step, phase_c_assemblies);
    }
    
    // Log working memory state
    if (working_memory_ && logger_) {
        auto bindings = working_memory_->getCurrentBindings();
        std::vector<NeuroForge::WorkingMemoryItem> wm_items;
        for (const auto& binding : bindings) {
            NeuroForge::WorkingMemoryItem item;
            item.role = binding.role_label;
            item.filler = binding.filler_label;
            item.strength = binding.strength;
            wm_items.push_back(item);
        }
        logger_->logWorkingMemory(step, wm_items);
    }
}

void SubstratePhaseCAdapter::stepSequence(int step) {
    if (!substrate_phase_c_) return;

    // Set sequence goal in substrate
    std::map<std::string, std::string> sequence_params;
    std::vector<std::string> seq_tokens = {"A", "B", "C", "D"};
    
    sequence_params["target"] = seq_tokens[step % seq_tokens.size()];
    
    // Set goal in substrate
    substrate_phase_c_->setGoal("sequence", sequence_params);
    
    // Process substrate step
    substrate_phase_c_->processStep(step, 0.1f);
    
    // Get sequence result from substrate behavior
    auto sequence_result = substrate_phase_c_->getSequenceResult(step);
    
    // Log sequence result
    if (!sequence_result.predicted.empty() && logger_) {
        logger_->logSequence(sequence_result);
    }
    
    // Get assemblies for timeline logging
    auto assemblies = substrate_phase_c_->getCurrentAssemblies();
    if (!assemblies.empty() && logger_) {
        // Find best sequence assembly
        Assembly winner;
        winner.id = 0;
        winner.symbol = sequence_result.predicted;
        winner.score = 0.8f; // Default score
        
        for (const auto& assembly : assemblies) {
            if (assembly.symbol.find("sequence") != std::string::npos) {
                winner.symbol = assembly.symbol;
                winner.score = assembly.coherence_score;
                break;
            }
        }
        
        logger_->logTimeline(step, winner);
        
        // Log all assemblies
        std::vector<Assembly> phase_c_assemblies;
        for (std::size_t i = 0; i < assemblies.size(); ++i) {
            Assembly assembly;
            assembly.id = static_cast<int>(i);
            assembly.symbol = assemblies[i].symbol;
            assembly.score = assemblies[i].coherence_score;
            phase_c_assemblies.push_back(assembly);
        }
        logger_->logAssemblies(step, phase_c_assemblies);
    }
    
    // Log working memory state
    if (working_memory_ && logger_) {
        auto bindings = working_memory_->getCurrentBindings();
        std::vector<NeuroForge::WorkingMemoryItem> wm_items;
        for (const auto& binding : bindings) {
            NeuroForge::WorkingMemoryItem item;
            item.role = binding.role_label;
            item.filler = binding.filler_label;
            item.strength = binding.strength;
            wm_items.push_back(item);
        }
        logger_->logWorkingMemory(step, wm_items);
    }
}

SubstratePhaseC::Statistics SubstratePhaseCAdapter::getSubstrateStatistics() const {
    if (substrate_phase_c_) {
        return substrate_phase_c_->getStatistics();
    }
    return SubstratePhaseC::Statistics{};
}

void SubstratePhaseCAdapter::setWorkingMemoryParams(std::size_t capacity, float decay) {
    if (working_memory_) {
        // Update working memory configuration
        SubstrateWorkingMemory::Config config;
        config.max_binding_capacity = capacity;
        config.decay_rate = decay;
        working_memory_->updateConfig(config);
    }
}

void SubstratePhaseCAdapter::setSequenceWindow(std::size_t window) {
    // Update substrate Phase C configuration without resetting other fields
    if (substrate_phase_c_) {
        substrate_phase_c_->setMaxAssemblies(window);
    }
}

void SubstratePhaseCAdapter::setEmitSurvivalRewards(bool enable) {
    if (substrate_phase_c_) {
        substrate_phase_c_->setEmitSurvivalRewards(enable);
    }
}

void SubstratePhaseCAdapter::setSurvivalBias(std::shared_ptr<NeuroForge::Biases::SurvivalBias> bias) {
    if (substrate_phase_c_) {
        substrate_phase_c_->setSurvivalBias(std::move(bias));
    }
}

void SubstratePhaseCAdapter::setHazardCoherenceWeight(float weight) {
    if (substrate_phase_c_) {
        substrate_phase_c_->setHazardCoherenceWeight(weight);
    }
}

void SubstratePhaseCAdapter::setSurvivalRewardScale(float scale) {
    if (substrate_phase_c_) {
        substrate_phase_c_->setSurvivalRewardScale(scale);
    }
}

void SubstratePhaseCAdapter::setLanguageSystem(std::shared_ptr<LanguageSystem> ls) {
    language_system_ = ls;
}

void SubstratePhaseCAdapter::setPhase6Reasoner(std::shared_ptr<Phase6Reasoner> reasoner) {
    reasoner_ = reasoner;
}

void SubstratePhaseCAdapter::processPercept(const std::unordered_map<std::string, std::string>& percept, int step) {
    (void)percept; // Unused for now
    if (substrate_phase_c_) {
        // Assuming processStep handles general processing, or we need a new method in SubstratePhaseC
        // For now, mapping percept to a "binding" goal if applicable, or generic step
        
        // Note: SubstratePhaseC::processStep doesn't take percepts directly yet.
        // It's mainly goal-driven. We'll inject goal if percept implies one.
        // For compatibility, we'll just run a step.
        substrate_phase_c_->processStep(step, 0.1f);
        
        // Log snapshot if possible (reusing logic from stepBinding/stepSequence)
        if (working_memory_ && logger_) {
             auto bindings = working_memory_->getCurrentBindings();
            std::vector<NeuroForge::WorkingMemoryItem> wm_items;
            for (const auto& binding : bindings) {
                NeuroForge::WorkingMemoryItem item;
                item.role = binding.role_label;
                item.filler = binding.filler_label;
                item.strength = binding.strength;
                wm_items.push_back(item);
            }
            logger_->logWorkingMemory(step, wm_items);
        }
    }
}

float SubstratePhaseCAdapter::processObservationStep(const std::vector<float>& visual_input,
                                                   int step,
                                                   const std::vector<float>& audio_input) {
    if (!substrate_phase_c_) return 0.0f;

    std::string web_state_token;
    if (!visual_input.empty()) {
        const std::size_t n = visual_input.size();
        const std::size_t side = static_cast<std::size_t>(std::sqrt(static_cast<double>(n)));
        if (side > 0 && side * side == n) {
            const std::size_t width = side;
            const std::size_t height = side;
            const std::size_t sector_width = width / 3;

            float left_sum = 0.0f;
            float center_sum = 0.0f;
            float right_sum = 0.0f;

            for (std::size_t y = 0; y < height; ++y) {
                for (std::size_t x = 0; x < width; ++x) {
                    const float v = visual_input[y * width + x];
                    if (x < sector_width) left_sum += v;
                    else if (x < 2 * sector_width) center_sum += v;
                    else right_sum += v;
                }
            }

            const float sector_pixels = static_cast<float>(sector_width * height);
            const float left_avg = sector_pixels > 0.0f ? (left_sum / sector_pixels) : 0.0f;
            const float center_avg = sector_pixels > 0.0f ? (center_sum / sector_pixels) : 0.0f;
            const float right_avg = sector_pixels > 0.0f ? (right_sum / sector_pixels) : 0.0f;

            auto bucket = [](float avg) -> std::string {
                if (avg < 0.33f) return "LOW";
                if (avg < 0.66f) return "MID";
                return "HIGH";
            };

            web_state_token = "WEB_V_" + bucket(left_avg) + "_" + bucket(center_avg) + "_" + bucket(right_avg);
        }
    }

    // 1. Feed Visual Input to VisualCortex
    if (brain_ptr_ && !visual_input.empty()) {
        auto vc = brain_ptr_->getRegion("VisualCortex");
        if (vc) {
            vc->feedExternalPattern(visual_input);
        }
    }

    // 1b. Feed Audio Input to AuditoryCortex
    if (brain_ptr_ && !audio_input.empty()) {
        auto region = brain_ptr_->getRegion("AuditoryCortex");
        if (region) {
             auto ac = std::dynamic_pointer_cast<NeuroForge::Regions::AuditoryCortex>(region);
             if (ac) {
                 ac->processAudioInput(audio_input);
             }
        }
    }

    // 2. Feed Audio Input to LanguageSystem
    if (language_system_ && !audio_input.empty()) {
        NeuroForge::Core::LanguageSystem::SensoryExperience exp;
        exp.experience_type = "audio";
        exp.sensory_data = audio_input;
        exp.timestamp = std::chrono::steady_clock::now();
        exp.salience_score = 0.5f; 
        language_system_->processSensoryExperience(exp);

        // Active Listening & Learning from Audio
        auto features = language_system_->extractAcousticFeatures(audio_input);
        language_system_->processProsodicPatternLearning(features);
        language_system_->processIntonationGuidedLearning("", features);
        
        if (language_system_->calculateSoundSalience(features) > 0.3f) {
            language_system_->processAcousticTeacherSignal(audio_input, "listening", 0.6f);
        }

        if (reasoner_) {
            auto active_tokens = language_system_->getActiveTokens();
            std::vector<NeuroForge::Core::ReasonOption> options;
            for (const auto& token : active_tokens) {
                NeuroForge::Core::ReasonOption opt;
                opt.key = token.symbol;
                opt.source = "LanguageSystem";
                opt.payload_json = "{\"activation\":" + std::to_string(token.activation_strength) + "}";
                opt.confidence = static_cast<double>(token.activation_strength);
                opt.complexity = 1.0 - (1.0 / (1.0 + token.usage_count));
                options.push_back(opt);
            }

            auto result = reasoner_->scoreOptions(options);

            if (result.best_index < options.size()) {
                const auto& best_opt = options[result.best_index];
                std::string selected_symbol = best_opt.key;
                double confidence = result.best_score;

                if (!selected_symbol.empty()) {
                    language_system_->injectReasonedConcept(selected_symbol, confidence);
                    reasoner_->applyOptionResult(
                        0, 
                        selected_symbol, 
                        confidence, 
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()
                        ).count()
                    );
                }
            }
        }
    }

    // 3. Set goal to drive sequence prediction from the current web state
    std::map<std::string, std::string> params;
    if (!web_state_token.empty()) {
        params["sequence_input"] = web_state_token;
    } else {
        params["sequence_input"] = "WEB_V_UNKNOWN";
    }
    substrate_phase_c_->setGoal("sequence", params);
    
    // 4. Process step
    substrate_phase_c_->processStep(step, 0.05f); // Lower learning rate for passive observation?

    if (web_sandbox_) {
        const auto result = substrate_phase_c_->getSequenceResult(step);
        const std::string action = decodeWebAction(result.predicted, step);
        if (action != "WAIT") {
            std::cout << "[WebAction] Step " << step << " Token: " << web_state_token << " Pred: " << result.predicted << " Act: " << action << std::endl;
            return 0.1f;
        }
    }

    // 5. Return neutral reward (or intrinsic motivation based on novelty)
    return 0.0f; 
}

float SubstratePhaseCAdapter::processEmbodimentStep(FirstPersonMazeRenderer::AgentState& agent, 
                                                  FirstPersonMazeRenderer& renderer, 
                                                  int step,
                                                  const std::vector<float>& audio_input) {
    if (!substrate_phase_c_) return 0.0f;

    // 1. Render visual input
    auto pixels = renderer.render(agent);
    
    // [VisualCortex Hook Implementation]
    if (brain_ptr_) {
        auto vc = brain_ptr_->getRegion("VisualCortex");
        if (vc) {
            vc->feedExternalPattern(pixels);
        }
        
        // Feed Audio to AuditoryCortex
        if (!audio_input.empty()) {
            auto region = brain_ptr_->getRegion("AuditoryCortex");
            if (region) {
                 auto ac = std::dynamic_pointer_cast<NeuroForge::Regions::AuditoryCortex>(region);
                 if (ac) {
                     ac->processAudioInput(audio_input);
                 }
            }
        }
    }

    // [LanguageSystem Acoustic Hook]
    if (language_system_ && !audio_input.empty()) {
        NeuroForge::Core::LanguageSystem::SensoryExperience exp;
        exp.experience_type = "audio";
        exp.sensory_data = audio_input;
        exp.timestamp = std::chrono::steady_clock::now();
        exp.salience_score = 0.5f; // Default salience
        language_system_->processSensoryExperience(exp);

        // [New] Active Listening & Learning from User Speech
        // 1. Extract acoustic features (prosody, intonation)
        auto features = language_system_->extractAcousticFeatures(audio_input);
        
        // 2. Process Prosodic Patterns (Unsupervised Learning)
        language_system_->processProsodicPatternLearning(features);
        
        // 3. Intonation-Guided Attention (detects Motherese/Teacher intonation)
        language_system_->processIntonationGuidedLearning("", features);
        
        // 4. Treat as potential Teacher Signal (if loud enough / distinct)
        // We assume direct input is a "listening" event
        if (language_system_->calculateSoundSalience(features) > 0.3f) {
            language_system_->processAcousticTeacherSignal(audio_input, "listening", 0.6f);
        }

        if (reasoner_) {
            auto active_tokens = language_system_->getActiveTokens();

            // 2. Formulate options for the Reasoner based on active tokens
            std::vector<NeuroForge::Core::ReasonOption> options;
            for (const auto& token : active_tokens) {
                NeuroForge::Core::ReasonOption opt;
                opt.key = token.symbol;
                opt.source = "LanguageSystem";
                opt.payload_json = "{\"activation\":" + std::to_string(token.activation_strength) + "}";
                opt.confidence = static_cast<double>(token.activation_strength);
                opt.complexity = 1.0 - (1.0 / (1.0 + token.usage_count)); // More usage = less complexity cost
                options.push_back(opt);
            }

            // 3. Reasoner scores the options
            auto result = reasoner_->scoreOptions(options);

            // 4. Inject the best reasoned concept back into Language System
            if (result.best_index < options.size()) {
                const auto& best_opt = options[result.best_index];
                std::string selected_symbol = best_opt.key;
                double confidence = result.best_score;

                if (!selected_symbol.empty()) {
                    // Boost the concept in Language System based on reasoning confidence
                    language_system_->injectReasonedConcept(selected_symbol, confidence);
                    
                    // [Optional] Feedback loop
                    // In Phase6Reasoner, we apply result by key
                    // Use 0 as dummy option_id since we don't have persistence IDs here yet
                    reasoner_->applyOptionResult(
                        0, // Dummy ID
                        selected_symbol, 
                        confidence, 
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()
                        ).count()
                    );
                }
            }
        }
    }

    // 2. Encode vision into a state token
    // Split into Left, Center, Right sectors
    int width = renderer.getConfig().width;
    int sector_width = width / 3;
    float left_sum = 0.0f, center_sum = 0.0f, right_sum = 0.0f;
    
    for (int y = 0; y < renderer.getConfig().height; ++y) {
        for (int x = 0; x < width; ++x) {
            float val = pixels[y * width + x];
            if (x < sector_width) left_sum += val;
            else if (x < 2 * sector_width) center_sum += val;
            else right_sum += val;
        }
    }
    
    // Normalize roughly (assuming wall intensity ~0.8)
    // Increased threshold to 0.4 to distinguish far walls (0.2) from close walls (0.8)
    float threshold = (sector_width * renderer.getConfig().height) * 0.4f; 
    
    std::string state_token = "S_";
    state_token += (left_sum > threshold ? "WALL" : "OPEN");
    state_token += "_";
    state_token += (center_sum > threshold ? "WALL" : "OPEN");
    state_token += "_";
    state_token += (right_sum > threshold ? "WALL" : "OPEN");
    
    // 3. Set goal: Predict next action based on current state
    std::map<std::string, std::string> params;
    params["sequence_input"] = state_token;
    substrate_phase_c_->setGoal("sequence", params);
    
    // 4. Process step
    substrate_phase_c_->processStep(step, 0.1f);
    
    // 5. Decode action
    auto result = substrate_phase_c_->getSequenceResult(step);
    std::string action = decodeMotorCommand(result.predicted);
    
    // 6. Apply action and calculate reward
    float move_speed = 0.5f;
    float turn_speed = 0.1f; // radians
    float reward = -0.01f; // Step cost
    
    auto next_agent = agent;
    bool moved = false;
    
    if (action == "FORWARD") {
        next_agent.x += std::cos(next_agent.angle) * move_speed;
        next_agent.y += std::sin(next_agent.angle) * move_speed;
        moved = true;
    } else if (action == "LEFT") {
        next_agent.angle -= turn_speed;
    } else if (action == "RIGHT") {
        next_agent.angle += turn_speed;
    }
    
    // Collision detection using map
    bool crashed = false;
    if (moved) {
        const auto& walls = renderer.getMazeWalls();
        int maze_size = renderer.getMazeSize();
        int cell_x = static_cast<int>(next_agent.x);
        int cell_y = static_cast<int>(next_agent.y);
        
        if (cell_x < 0 || cell_x >= maze_size || cell_y < 0 || cell_y >= maze_size) {
            crashed = true;
        } else if (cell_y * maze_size + cell_x < (int)walls.size() && walls[cell_y * maze_size + cell_x]) {
            crashed = true;
        }
    }
    
    if (crashed) {
        reward = -1.0f;
        // Inject pain
        if (substrate_phase_c_) {
             auto bias = substrate_phase_c_->getSurvivalBias();
             if (bias) {
                 bias->setExternalHazard(0.8f); // High immediate hazard
             }
        }
        // Reset to start on crash to prevent getting stuck
        agent.x = 1.5f; agent.y = 1.5f;
    } else {
        agent = next_agent;
    }
    
    // Check goal (if visible and close)
    float goal_dist = renderer.getGoalVisibility(agent);
    bool reached_goal = false;
    if (goal_dist > 0 && goal_dist < 1.0f) {
        reward = 10.0f;
        reached_goal = true;
    }
    
    // Update metrics
    metrics_.steps_in_current_episode++;
    if (reached_goal || crashed || metrics_.steps_in_current_episode > 1000) {
        metrics_.total_episodes++;
        if (reached_goal) {
             metrics_.successful_episodes++;
             metrics_.total_success_steps += metrics_.steps_in_current_episode;
        }
        metrics_.success_rate = (metrics_.total_episodes > 0) ? 
            (float)metrics_.successful_episodes / (float)metrics_.total_episodes : 0.0f;
            
        if (metrics_.successful_episodes > 0) {
            metrics_.adaptation_rate = (float)metrics_.total_success_steps / (float)metrics_.successful_episodes;
        }

        // Reset episode metrics
        metrics_.steps_in_current_episode = 0;
        
        // Simple teleport reset if reached goal (to keep testing)
        if (reached_goal) {
             agent.x = 1.5f; agent.y = 1.5f; // Back to start
        }
    }
    
    // Adaptation Rate (proxy: average steps to success)
    // Already updated above
    
    // CIP (Average Coherence)
    auto stats = substrate_phase_c_->getStatistics();
    metrics_.cip = stats.average_coherence;
    
    // DEBUG: Monitor CIP updates
    // if (step % 100 == 0) {
       std::cout << "[Embodiment] Step " << step << " CIP: " << metrics_.cip 
                 << " Act: " << action << " Ang: " << agent.angle 
                 << " Pos: (" << agent.x << "," << agent.y << ")" 
                 << " Token: " << state_token << " Pred: " << result.predicted << std::endl;
    // }

    // Self-Model Confidence (Prediction Coherence)
    metrics_.self_model_confidence = 0.0f;
    auto assemblies = substrate_phase_c_->getCurrentAssemblies();
    for (const auto& asm_ : assemblies) {
        if (asm_.symbol == result.predicted) {
            metrics_.self_model_confidence = asm_.coherence_score;
            break;
        }
    }
    
    return reward;
}

std::string SubstratePhaseCAdapter::decodeMotorCommand(const std::string& predicted_token) {
    // Heuristic mapping to bootstrap exploration
    // Parse token S_LEFT_CENTER_RIGHT
    
    if (predicted_token.empty()) return "WAIT";
    
    // Check if token follows format S_X_Y_Z
    if (predicted_token.substr(0, 2) == "S_") {
        // Better parsing
        size_t first_und = predicted_token.find('_', 2);
        size_t second_und = predicted_token.find('_', first_und + 1);
        
        if (first_und != std::string::npos && second_und != std::string::npos) {
            std::string left = predicted_token.substr(2, first_und - 2);
            std::string center = predicted_token.substr(first_und + 1, second_und - (first_und + 1));
            std::string right = predicted_token.substr(second_und + 1);
            
            if (center == "OPEN") return "FORWARD";
            if (left == "OPEN") return "LEFT";
            if (right == "OPEN") return "RIGHT";
            // If all walls, turn right to search
            return "RIGHT";
        }
    }

    // Fallback for non-standard tokens
    if (predicted_token == "S_OPEN_OPEN_OPEN") return "FORWARD";

    size_t hash = std::hash<std::string>{}(predicted_token);
    int action_idx = hash % 3;
    
    if (action_idx == 0) return "FORWARD";
    if (action_idx == 1) return "LEFT";
    return "RIGHT";
}

void SubstratePhaseCAdapter::setWebSandbox(std::shared_ptr<NeuroForge::Sandbox::WebSandbox> sandbox) {
    web_sandbox_ = sandbox;
}

float SubstratePhaseCAdapter::processWebStep(int step, const std::vector<float>& visual_input) {
    if (!substrate_phase_c_) return 0.0f;

    if (web_sandbox_) {
        auto result = substrate_phase_c_->getSequenceResult(step);
        std::string action = decodeWebAction(result.predicted, step);
        
        // Simple intrinsic reward for acting
        if (action != "WAIT") return 0.1f;
    }

    return 0.0f;
}

std::string SubstratePhaseCAdapter::decodeWebAction(const std::string& predicted_token, int step) {
    if (!web_sandbox_) return "WAIT";

    // Format: WEB_<ACTION>_<PARAMS>
    // Examples:
    // WEB_CLICK_5_5  (Click at 50%, 50%)
    // WEB_SCROLL_UP
    // WEB_SCROLL_DOWN
    // WEB_TYPE_HELLO
    
    auto clickGrid = [&](int x_grid, int y_grid) -> bool {
        auto bounds = web_sandbox_->bounds();
        if (bounds.w <= 0 || bounds.h <= 0) return false;
        int tx = static_cast<int>(bounds.w * (x_grid + 0.5f) / 10.0f);
        int ty = static_cast<int>(bounds.h * (y_grid + 0.5f) / 10.0f);
        return web_sandbox_->click(tx, ty);
    };

    if (predicted_token.empty()) {
        if (step == 20) {
            if (clickGrid(5, 1)) {
                web_sandbox_->typeText("neuroforge");
                web_sandbox_->sendKey(0x0D);
                return "SCRIPT_SEARCH";
            }
        }
        if (step % 50 == 0) {
            web_sandbox_->scroll(-120);
            return "SCRIPT_SCROLL_DOWN";
        }
        if (step % 120 == 0) {
            if (clickGrid(5, 5)) return "SCRIPT_CLICK_CENTER";
        }
        return "WAIT";
    }

    if (predicted_token.find("WEB_") != 0) {
        if (step == 20) {
            if (clickGrid(5, 1)) {
                web_sandbox_->typeText("neuroforge");
                web_sandbox_->sendKey(0x0D);
                return "SCRIPT_SEARCH";
            }
        }
        if (step % 50 == 0) {
            web_sandbox_->scroll(-120);
            return "SCRIPT_SCROLL_DOWN";
        }
        if (step % 120 == 0) {
            if (clickGrid(5, 5)) return "SCRIPT_CLICK_CENTER";
        }
        return "WAIT";
    }
    
    std::string cmd = predicted_token.substr(4);
    
    if (cmd.find("CLICK_") == 0) {
        // Parse coords
        std::string coords = cmd.substr(6);
        size_t sep = coords.find('_');
        if (sep != std::string::npos) {
            try {
                int x_grid = std::stoi(coords.substr(0, sep));
                int y_grid = std::stoi(coords.substr(sep + 1));
                
                if (clickGrid(x_grid, y_grid)) return "CLICK " + std::to_string(x_grid) + "," + std::to_string(y_grid);
            } catch (...) {}
        }
    } else if (cmd == "SCROLL_UP") {
        web_sandbox_->scroll(120); // Standard wheel delta
        return "SCROLL_UP";
    } else if (cmd == "SCROLL_DOWN") {
        web_sandbox_->scroll(-120);
        return "SCROLL_DOWN";
    } else if (cmd.find("TYPE_") == 0) {
        std::string text = cmd.substr(5);
        web_sandbox_->typeText(text);
        return "TYPE " + text;
    }
    
    return "WAIT";
}

SubstratePhaseC::Config SubstratePhaseCAdapter::getConfig() const {
    if (substrate_phase_c_) {
        return substrate_phase_c_->getConfig();
    }
    return SubstratePhaseC::Config();
}

void SubstratePhaseCAdapter::setConfig(const SubstratePhaseC::Config& config) {
    if (substrate_phase_c_) {
        substrate_phase_c_->setConfig(config);
    }
}

} // namespace Core
} // namespace NeuroForge
