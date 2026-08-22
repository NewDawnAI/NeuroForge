#include <iostream>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <string>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <atomic>
#include <mutex>
#include <fstream>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <random>
#include <queue>
#include <utility>
#include <limits>
#include <cstring>
#include <unordered_map>

#include <exception>
#include <new>
#include <csignal>

#ifdef NF_HAVE_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include <psapi.h>
#pragma comment(lib, "Psapi.lib")
#endif

// Project headers
#include "audio_capture.h"
#include "system_audio_capture.h"
#include "audio/AudioOutputSystem.h"
#include "screen_capture.h"
#include "core/HypergraphBrain.h"
#include "connectivity/ConnectivityManager.h"
#include "core/Region.h"
#include "core/LearningSystem.h"
#include "core/FirstPersonMazeRenderer.h"
#include "regions/CorticalRegions.h"
#include "encoders/VisionEncoder.h"
#include "encoders/AudioEncoder.h"
#include "biases/SocialPerceptionBias.h"
#include "biases/VoiceBias.h"
#include "biases/MotionBias.h"
#include "biases/SurvivalBias.h"
#include "core/Neuron.h"
#include "core/MemoryDB.h"
#include "core/SelfModel.h"
#include "core/LanguageSystem.h"
#include "core/PhaseAMimicry.h"
#include "core/ContextHooks.h"
#include "core/Phase6Reasoner.h"
#include "core/Phase7AffectiveState.h"
#include "core/Phase7Reflection.h"
#include "core/Phase8GoalSystem.h"
#include "core/Phase9Metacognition.h"
#include "core/AutonomyEnvelope.h"
#include "core/Phase10SelfExplanation.h"
#include "core/Phase11SelfRevision.h"
#include "core/Phase12Consistency.h"
#include "core/Phase13AutonomyEnvelope.h"
#include "core/Phase14MetaReasoner.h"
#include "core/Phase15EthicsRegulator.h"
#include "core/MetaCognitionSystem.h"
#include "core/SubstratePhaseC.h"
#include "core/SubstratePhaseCAdapter.h"
#include "regions/CorticalRegions.h"
#include "core/SubstrateWorkingMemory.h"
#include "sandbox/WebSandbox.h"
#include "core/ActionFilter.h"
#include "core/PhaseC.h"
#include "core/SubstrateLanguageIntegration.h"
#include "regions/LimbicRegions.h"
#include "core/HiveManager.h"
#include "core/RegionRegistry.h"

// Force-link declarations for region translation units
extern "C" void NF_ForceLink_CorticalRegions();
extern "C" void NF_ForceLink_SubcorticalRegions();
extern "C" void NF_ForceLink_LimbicRegions();
extern "C" void NF_ForceLink_PhaseARegion();

static std::shared_ptr<NeuroForge::Core::MemoryDB> g_memdb;
static std::int64_t g_memdb_run_id = 0;
static std::atomic<std::uint64_t> g_last_step{0};
static std::atomic<bool> g_abort{false};

#ifdef _WIN32
static BOOL WINAPI NfCtrlHandler(DWORD ctrl) {
    g_abort.store(true);
    return TRUE;
}
#endif

static void NfSetTerminationHandlers() {
#ifdef _WIN32
    SetConsoleCtrlHandler(NfCtrlHandler, TRUE);
#endif
}

namespace {

void print_usage() {
    std::cout << "NeuroForge demo\n"
              << "Usage: neuroforge.exe [options]\n\n"
              << "Options:\n"
              << "  --help                                 Show this help\n"
              << "  --steps=N                              Number of processing steps (default: 1)\n"
              << "  --unified-substrate[=on|off]           Enable unified substrate mode (WM + Phase C + SurvivalBias + Language) (default: off)\n"
              << "  --embodiment[=on|off]                  Enable embodiment mode (requires --unified-substrate) (default: off)\n"
              << "  --memory-db=PATH                       Enable memory database logging\n"
              << "  --hive[=on|off]                        Enable Hive Telepathy Mode (default: off)\n"
              << "  --hive-id=ID                           Unique Hive Node ID (default: node_1)\n";
    // Add other flags as needed, keeping it brief for now to ensure functionality
    std::cout << "  --phase15[=on|off]                     Enable Phase 15 Ethics Regulation (default: off)\n"
              << "  --phase15-risk-threshold=VAL           Set Phase 15 risk threshold (0.0-1.0) (default: 0.5)\n";
}

std::string get_arg_value(const std::vector<std::string>& args, const std::string& key, const std::string& default_val = "") {
    for (const auto& arg : args) {
        if (arg.rfind(key + "=", 0) == 0) {
            return arg.substr(key.length() + 1);
        }
        if (arg == key) return "true"; 
    }
    return default_val;
}

bool has_arg(const std::vector<std::string>& args, const std::string& key) {
    for (const auto& arg : args) {
        if (arg == key || arg.rfind(key + "=", 0) == 0) return true;
    }
    return false;
}

bool parse_bool(const std::string& val, bool default_val) {
    if (val.empty()) return default_val;
    std::string v = val;
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    if (v == "on" || v == "true" || v == "1" || v == "yes") return true;
    if (v == "off" || v == "false" || v == "0" || v == "no") return false;
    return default_val;
}

} // namespace

int main(int argc, char** argv) {
    NfSetTerminationHandlers();

    std::vector<std::string> args(argv + 1, argv + argc);
    if (has_arg(args, "--help")) {
        print_usage();
        return 0;
    }

    bool unified_substrate = parse_bool(get_arg_value(args, "--unified-substrate", "off"), false);
    bool embodiment = parse_bool(get_arg_value(args, "--embodiment", "off"), false);
    bool phase15_enabled = parse_bool(get_arg_value(args, "--phase15", "off"), false);
    double phase15_threshold = std::stod(get_arg_value(args, "--phase15-risk-threshold", "0.5"));
    int steps = std::stoi(get_arg_value(args, "--steps", "1"));
    std::string memdb_path = get_arg_value(args, "--memory-db");
    bool hive_enabled = parse_bool(get_arg_value(args, "--hive", "off"), false);
    std::string hive_id = get_arg_value(args, "--hive-id", "node_1");

    // M6 Hippocampal Parameters
    bool hippocampal_enabled = parse_bool(get_arg_value(args, "--hippocampal", "off"), false);
    int max_snapshots = std::stoi(get_arg_value(args, "--max-snapshots", "1000"));
    float snapshot_threshold = std::stof(get_arg_value(args, "--snapshot-threshold", "0.1"));
    float consolidation_threshold = std::stof(get_arg_value(args, "--consolidation-threshold", "0.8"));

    // M7 Autonomy Parameters
    bool autonomous_enabled = parse_bool(get_arg_value(args, "--autonomous", "off"), false);
    float curiosity_threshold = std::stof(get_arg_value(args, "--curiosity-threshold", "0.3"));
    float uncertainty_threshold = std::stof(get_arg_value(args, "--uncertainty-threshold", "0.4"));
    int max_tasks = std::stoi(get_arg_value(args, "--max-tasks", "5"));
    int task_interval = std::stoi(get_arg_value(args, "--task-interval", "1000"));

    // Video Learning Parameters
    std::string video_file_path = get_arg_value(args, "--video-file");
    if (!video_file_path.empty()) {
        unified_substrate = true; // Video learning requires substrate
    }

    // Web Sandbox Parameters
    bool sandbox_enabled = parse_bool(get_arg_value(args, "--sandbox", "off"), false);
    std::string sandbox_url = get_arg_value(args, "--sandbox-url", "https://www.youtube.com");
    if (sandbox_enabled) {
        unified_substrate = true;
    }

    std::cout << "Initializing NeuroForge..." << std::endl;
    if (unified_substrate) std::cout << "Mode: Unified Substrate" << std::endl;
    if (embodiment) std::cout << "Mode: Embodiment" << std::endl;
    if (sandbox_enabled) std::cout << "Mode: Autonomous Sandbox (URL: " << sandbox_url << ")" << std::endl;
    if (!video_file_path.empty()) std::cout << "Mode: Video Learning (Source: " << video_file_path << ")" << std::endl;
    if (hive_enabled) std::cout << "Mode: Hive Telepathy (ID: " << hive_id << ")" << std::endl;
    if (hippocampal_enabled) std::cout << "Mode: Hippocampal Memory (Snapshots: " << max_snapshots << ")" << std::endl;
    if (autonomous_enabled) std::cout << "Mode: Autonomous Operation (Curiosity: " << curiosity_threshold << ")" << std::endl;
    if (phase15_enabled) {
        std::cout << "Mode: Phase 15 Ethics (Threshold: " << phase15_threshold << ")" << std::endl;
        // Initialize Context Hooks for risk sampling
        NeuroForge::Core::NF_InitContext(1.0, 100, 10);
    }

    if (!memdb_path.empty()) {
        g_memdb = std::make_shared<NeuroForge::Core::MemoryDB>(memdb_path);
        if (g_memdb->open() && g_memdb->ensureSchema()) {
            g_memdb->beginRun("{\"name\":\"NeuroForge Main\"}", g_memdb_run_id);
        } else {
            std::cerr << "Failed to initialize MemoryDB at " << memdb_path << std::endl;
        }
    }

    auto connectivity_manager = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
    auto brain = std::make_shared<NeuroForge::Core::HypergraphBrain>(connectivity_manager);

    // Configure M6 Hippocampal Memory
    if (hippocampal_enabled) {
        NeuroForge::Core::HypergraphBrain::HippocampalConfig hip_config;
        hip_config.enabled = true;
        hip_config.max_snapshots = max_snapshots;
        hip_config.snapshot_threshold = snapshot_threshold;
        hip_config.consolidation_threshold = consolidation_threshold;
        brain->configureHippocampalSnapshotting(hip_config);
        std::cout << "Hippocampal System configured." << std::endl;
    }

    // Configure M7 Autonomy
    if (autonomous_enabled) {
        brain->setSubstrateMode(NeuroForge::Core::HypergraphBrain::SubstrateMode::Native);
        brain->setCuriosityThreshold(curiosity_threshold);
        brain->setUncertaintyThreshold(uncertainty_threshold);
        brain->setMaxConcurrentTasks(max_tasks);
        brain->setTaskGenerationInterval(task_interval);
        
        std::cout << "Autonomous System configured." << std::endl;
        std::cout << "  - Curiosity: " << curiosity_threshold << std::endl;
        std::cout << "  - Uncertainty: " << uncertainty_threshold << std::endl;
        std::cout << "  - Concurrent Tasks: " << max_tasks << std::endl;
        std::cout << "  - Task Interval: " << task_interval << " ms" << std::endl;
    }
    
    // Instantiate Cortical Regions for Embodiment/Sensory Processing
    auto visual_cortex = std::make_shared<NeuroForge::Regions::VisualCortex>("VisualCortex", 5000);
    visual_cortex->initializeLayers();
    brain->addRegion(visual_cortex);

    auto auditory_cortex = std::make_shared<NeuroForge::Regions::AuditoryCortex>("AuditoryCortex", 5000);
    auditory_cortex->initializeTonotopicMap();
    brain->addRegion(auditory_cortex);
    
    // Hive Manager Initialization
    std::shared_ptr<NeuroForge::Core::HiveManager> hive_manager;
    if (hive_enabled) {
        hive_manager = std::make_shared<NeuroForge::Core::HiveManager>("", hive_id);
        if (g_memdb) {
            hive_manager->setMemoryDB(g_memdb.get());
        } else {
            std::cerr << "Warning: Hive Mode enabled but no MemoryDB provided. Telepathy will be disabled." << std::endl;
        }
    }

    // Core components initialization
    if (unified_substrate) {
        std::cout << "Initializing Substrate components..." << std::endl;
        
        // Initialize Learning System
    NeuroForge::Core::LearningSystem::Config learning_config;
    learning_config.hebbian_rate = 0.01f;
    learning_config.stdp_rate = 0.005f;
    
    // Enable Structural Plasticity (Neurogenesis and Synaptogenesis)
    learning_config.enable_structural_plasticity = true;
    learning_config.structural_spawn_batch = 10; // Allow spawning 10 neurons
    learning_config.structural_grow_batch = 20;  // Allow growing 20 synapses
    learning_config.structural_interval_steps = 100; // Check every 100 steps
    
    if (brain->initializeLearning(learning_config)) {
        brain->setLearningEnabled(true);
        std::cout << "Learning System initialized and enabled." << std::endl;
        std::cout << "  - Hebbian Rate: " << learning_config.hebbian_rate << std::endl;
        std::cout << "  - STDP Rate: " << learning_config.stdp_rate << std::endl;
        std::cout << "  - Structural Plasticity: Enabled (Spawn=" << learning_config.structural_spawn_batch 
                  << ", Grow=" << learning_config.structural_grow_batch << ")" << std::endl;
    } else {
        std::cerr << "Failed to initialize Learning System." << std::endl;
    }

        NeuroForge::Core::SubstratePhaseC::Config phase_c_config;
        
        // Working Memory
        NeuroForge::Core::SubstrateWorkingMemory::Config wm_config;
        auto working_memory = std::make_shared<NeuroForge::Core::SubstrateWorkingMemory>(brain, wm_config);
        
        // Logger
        NeuroForge::PhaseCCSVLogger logger("phase_c_logs.csv");
        
        auto adapter = std::make_shared<NeuroForge::Core::SubstratePhaseCAdapter>(
            brain, working_memory, logger, phase_c_config
        );

        // Survival Bias
        NeuroForge::Biases::SurvivalBias::Config surv_config;
        auto survival_bias = std::make_shared<NeuroForge::Biases::SurvivalBias>(surv_config);
        adapter->setSurvivalBias(survival_bias);
        // Ethics Regulator
        NeuroForge::Core::Phase15EthicsRegulator::Config ethics_config;
        ethics_config.risk_threshold = phase15_threshold;
        auto ethics_regulator = std::make_shared<NeuroForge::Core::Phase15EthicsRegulator>(
            g_memdb.get(), g_memdb_run_id, ethics_config
        );

        // Meta-Cognition System (Stage D)
        NeuroForge::Core::MetaCognitionSystem::Config meta_config;
        auto meta_cognition = std::make_shared<NeuroForge::Core::MetaCognitionSystem>(
            g_memdb.get(), g_memdb_run_id, meta_config
        );

        // Language System
        NeuroForge::Core::LanguageSystem::Config lang_config;
        auto language_system = std::make_shared<NeuroForge::Core::LanguageSystem>(lang_config);
        if (g_memdb) {
            language_system->setAuditSink(g_memdb.get(), g_memdb_run_id);
        }
        language_system->initialize();
        
        // Audio Output System (Text-to-Speech)
        auto audio_output = std::make_shared<NeuroForge::Audio::AudioOutputSystem>();
        if (audio_output->initialize()) {
            std::cout << "Audio Output System initialized." << std::endl;
            language_system->setSpeechOutputCallback([audio_output](const std::string& text) {
                std::cout << "[NeuroForge Speaks]: " << text << std::endl;
                audio_output->speak(text);
            });
        } else {
            std::cerr << "Failed to initialize Audio Output System." << std::endl;
        }

        adapter->setLanguageSystem(language_system);

        // [Stage C v6] Phase 6 Reasoner
        // Uses global MemoryDB and run_id if available, otherwise nullptr/0
        NeuroForge::Core::MemoryDB* memdb_ptr = g_memdb ? g_memdb.get() : nullptr;
        std::int64_t reasoner_run_id = g_memdb ? g_memdb_run_id : 0;
        
        auto phase6_reasoner = std::make_shared<NeuroForge::Core::Phase6Reasoner>(memdb_ptr, reasoner_run_id);
        
        adapter->setPhase6Reasoner(phase6_reasoner);

        if (embodiment) {
            std::cout << "Starting Embodiment Loop (" << steps << " steps)..." << std::endl;

            // Initialize Audio Capture
            NeuroForge::Audio::AudioCapture::Config audio_config;
            audio_config.sample_rate = 16000;
            NeuroForge::Audio::AudioCapture audio_capture(audio_config);
            if (audio_capture.initialize()) {
                audio_capture.startCapture();
                std::cout << "Audio capture initialized." << std::endl;
            } else {
                std::cerr << "Failed to initialize audio capture." << std::endl;
            }
            
            NeuroForge::Core::FirstPersonMazeRenderer::RenderConfig render_config;
            render_config.width = 64;
            render_config.height = 64;
            NeuroForge::Core::FirstPersonMazeRenderer renderer(render_config);
            
            // Setup Maze (8x8)
            int maze_size = 8;
            std::vector<bool> walls(maze_size * maze_size, false);
            for(int i=0; i<maze_size; ++i) {
                walls[i] = true; // Top
                walls[(maze_size-1)*maze_size + i] = true; // Bottom
                walls[i*maze_size] = true; // Left
                walls[i*maze_size + maze_size-1] = true; // Right
            }
            walls[2*maze_size + 2] = true; // Obstacle
            
            renderer.setMaze(walls, maze_size, maze_size-2, maze_size-2);

            NeuroForge::Core::FirstPersonMazeRenderer::AgentState agent;
            agent.x = 1.5f; agent.y = 1.5f; agent.angle = 0.0f;
            agent.maze_x = 1; agent.maze_y = 1;

            for (int step = 0; step < steps && !g_abort; ++step) {
                g_last_step = step;
                
                // 1. Process Embodiment Step
                std::vector<float> audio_samples;
                if (audio_capture.isInitialized()) {
                    auto audio_data = audio_capture.getLatestAudio();
                    audio_samples = audio_data.samples;
                }
                float reward = adapter->processEmbodimentStep(agent, renderer, step, audio_samples);
                
                // 2. Ethics Check
                std::string ethics_decision = "allow";
                if (phase15_enabled) {
                    ethics_decision = ethics_regulator->runForLatest("embodiment_step");

                    if (ethics_decision == "deny") {
                        if (step % 50 == 0) std::cout << "Step " << step << " [ETHICS BLOCKED] Risk exceeded threshold." << std::endl;
                        // Enforce: Penalize reward heavily
                        reward = -5.0f;
                        
                        // [Collective Ethics] Broadcast violation
                        if (hive_manager) {
                             std::string alert = "{\"risk\":\"high\",\"reason\":\"ethics_block\",\"step\":" + std::to_string(step) + "}";
                             hive_manager->broadcastEvent("ethics_violation", alert);
                        }

                    } else if (ethics_decision == "review") {
                        if (step % 50 == 0) std::cout << "Step " << step << " [ETHICS REVIEW]" << std::endl;
                    }
                }
                
                // 3. Log Metrics
                auto stats = adapter->getSubstrateStatistics();

                // Stage D: Meta-Cognition Analysis & Regulation
                if (meta_cognition) {
                    bool is_violation = (ethics_decision == "deny");
                    meta_cognition->analyze(step, stats, reward, is_violation);
                    if (step > 0 && step % 500 == 0) {
                        meta_cognition->regulate(adapter);
                    }
                }

                // 4. Hive Telepathy (Broadcast & Receive)
                if (hive_manager) {
                    // [Swarm Navigation] Share Map Data if wall encountered
                    // We infer wall collision if reward is very low (step cost is -0.01, so -5.0 or similar implies collision logic if we had it)
                    // But actually, collision logic is:
                    // if (cell_x < 0 ... || walls[...])
                    // We need to capture that "crashed" state. 
                    // Let's assume we can infer it or we should have modified processEmbodimentStep to return it.
                    // For now, we broadcast current position as "explored".
                    std::stringstream map_ss;
                    map_ss << "{\"x\":" << agent.x << ",\"y\":" << agent.y << ",\"type\":\"explored\"}";
                    hive_manager->broadcastEvent("map_update", map_ss.str());

                    std::stringstream ss;
                    ss << "{\"pos\":[" << agent.x << "," << agent.y << "],"
                       << "\"reward\":" << reward << ","
                       << "\"ethics\":\"" << ethics_decision << "\","
                       << "\"cip\":" << stats.average_coherence << "}";
                    hive_manager->broadcastState(step, ss.str());

                    auto signals = hive_manager->receiveHiveSignals(step);
                    
                    // Stage D.3: Hive Theory of Mind
                    if (meta_cognition) {
                        meta_cognition->processHiveSignals(signals);
                    }

                    for (const auto& sig : signals) {
                        // Check for ethics alerts
                        if (sig.find("ethics_violation") != std::string::npos) {
                            std::cout << "\n[HIVE ALERT] *** PEER REPORTED ETHICS VIOLATION *** Increasing vigilance.\n" << std::endl;
                        }
                        // Check for map updates
                        if (sig.find("map_update") != std::string::npos) {
                             // In a real system, we'd update our internal map.
                             // Here we just acknowledge the shared knowledge.
                             // std::cout << "[HIVE MAP] Received exploration data." << std::endl; 
                        }
                        std::cout << "[Telepathy] " << sig << std::endl;
                    }
                }
                
                if (step % 50 == 0) {
                    std::cout << "Step " << step 
                              << " | Pos: (" << std::fixed << std::setprecision(2) << agent.x << "," << agent.y << ")"
                              << " | Reward: " << reward
                              << " | Ethics: " << ethics_decision
                              << " | CIP: " << stats.average_coherence 
                              << " | Assembly: " << stats.active_assemblies
                              << std::endl;
                }
                
                // Simulate frame delay if needed
                // std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            std::cout << "Embodiment Loop Completed." << std::endl;
        } else if (!video_file_path.empty()) {
            std::cout << "Starting Video Learning from: " << video_file_path << std::endl;
            
#ifdef NF_HAVE_OPENCV
            cv::VideoCapture cap(video_file_path);
            if (!cap.isOpened()) {
                std::cerr << "Error: Could not open video file." << std::endl;
                return 1;
            }

            // Initialize Audio Capture (for user commentary/environment while watching)
            NeuroForge::Audio::AudioCapture::Config audio_config;
            audio_config.sample_rate = 16000;
            NeuroForge::Audio::AudioCapture audio_capture(audio_config);
            if (audio_capture.initialize()) {
                audio_capture.startCapture();
                std::cout << "Microphone active for commentary during video playback." << std::endl;
            }

            cv::Mat frame, resized, gray, float_img;
            int step = 0;
            
            while (cap.read(frame) && !g_abort) {
                // Resize to brain input size (64x64)
                cv::resize(frame, resized, cv::Size(64, 64));
                
                // Convert to grayscale
                cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
                
                // Convert to float 0.0-1.0
                gray.convertTo(float_img, CV_32F, 1.0/255.0);
                
                // Copy to vector
                std::vector<float> visual_input;
                if (float_img.isContinuous()) {
                    visual_input.assign((float*)float_img.datastart, (float*)float_img.dataend);
                } else {
                    for (int i = 0; i < float_img.rows; ++i) {
                        visual_input.insert(visual_input.end(), float_img.ptr<float>(i), float_img.ptr<float>(i) + float_img.cols);
                    }
                }

                // Get Audio
                std::vector<float> audio_samples;
                if (audio_capture.isInitialized()) {
                    auto audio_data = audio_capture.getLatestAudio();
                    audio_samples = audio_data.samples;
                }

                // Process Step
                adapter->processObservationStep(visual_input, step, audio_samples);

                // Logging
                if (step % 100 == 0) {
                    auto stats = adapter->getSubstrateStatistics();
                    std::cout << "Frame " << step 
                              << " | CIP: " << stats.average_coherence 
                              << " | Assemblies: " << stats.active_assemblies << std::endl;
                }
                
                step++;
                
                // Optional: slow down to match framerate? 
                // For learning, faster might be better, but let's delay slightly to not burn CPU 100%
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            std::cout << "Video Learning Completed. Processed " << step << " frames." << std::endl;
#else
            std::cerr << "Error: NeuroForge was built without OpenCV support. Cannot process video files." << std::endl;
#endif
        } else if (sandbox_enabled) {
            std::cout << "Starting Autonomous Sandbox..." << std::endl;
            
            NeuroForge::Sandbox::WebSandbox sandbox;
            if (!sandbox.create(1280, 720, "NeuroForge Agent Window")) {
                std::cerr << "Failed to create sandbox window." << std::endl;
                return 1;
            }
            
            std::cout << "Navigating to " << sandbox_url << "..." << std::endl;
            sandbox.navigate(sandbox_url);
            
            // Wait for page load / readiness
            if (!sandbox.waitUntilReady(10000)) {
                std::cerr << "Warning: Sandbox timed out waiting for readiness (or navigation took long)." << std::endl;
            }

            adapter->setWebSandbox(std::shared_ptr<NeuroForge::Sandbox::WebSandbox>(&sandbox, [](NeuroForge::Sandbox::WebSandbox*) {}));
            
            // Initialize Audio Capture
            NeuroForge::Audio::AudioCapture::Config audio_config;
            audio_config.sample_rate = 16000;
            NeuroForge::Audio::AudioCapture audio_capture(audio_config);
            if (audio_capture.initialize()) {
                audio_capture.startCapture();
                std::cout << "Audio capture initialized." << std::endl;
            }
            
            NeuroForge::IO::ScreenCapturer capturer;
            
            // Shared State for Threading
            struct SensoryBuffer {
                std::vector<float> visual;
                std::vector<float> audio;
                std::mutex mtx;
            } sensory_buffer;

            std::atomic<bool> brain_running{true};
            std::atomic<bool> simulation_finished{false};

            // Worker Thread: The "Brain"
            std::thread brain_thread([&]() {
                int brain_step = 0;
                std::cout << "Brain Thread started." << std::endl;

                while (brain_running && !g_abort && brain_step < steps) {
                    g_last_step = brain_step; // Global atomic update
                    
                    std::vector<float> current_visual;
                    std::vector<float> current_audio;
                    
                    // Fetch latest sensory data
                    {
                        std::lock_guard<std::mutex> lock(sensory_buffer.mtx);
                        current_visual = sensory_buffer.visual;
                        current_audio = sensory_buffer.audio;
                    }

                    if (!current_visual.empty()) {
                        // 4. Process Observation (The heavy lifting)
                        adapter->processObservationStep(current_visual, brain_step, current_audio);
                        
                        // 5. Logging
                        if (brain_step % 100 == 0) {
                            auto stats = adapter->getSubstrateStatistics();
                            std::cout << "Step " << brain_step 
                                      << " | CIP: " << stats.average_coherence 
                                      << " | Assemblies: " << stats.active_assemblies 
                                      << " | URL: " << sandbox_url << std::endl;
                        }

                        brain_step++;
                    } else {
                        // Wait for eyes to open
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
                
                std::cout << "Brain finished " << brain_step << " steps." << std::endl;
                simulation_finished = true;
            });

            // Main Thread: The "Body" (UI & Senses)
            std::cout << "Main UI Loop running..." << std::endl;
            while (!g_abort && sandbox.isOpen() && !simulation_finished) {
                
                // 1. Process Window Events (Must be on main thread)
                sandbox.poll();
                sandbox.updateBoundsFromClient();
                auto bounds = sandbox.screenBounds();
                
                // 2. Capture Visuals
                NeuroForge::IO::ScreenCapturer::Rect rect;
                rect.x = bounds.x; rect.y = bounds.y;
                rect.w = bounds.w; rect.h = bounds.h;
                capturer.setRect(rect);
                
                std::vector<float> visual_input = capturer.captureGrayGrid(64);
                
                // 3. Capture Audio
                std::vector<float> audio_samples;
                if (audio_capture.isInitialized()) {
                    auto audio_data = audio_capture.getLatestAudio();
                    audio_samples = audio_data.samples;
                }

                // Update Shared State
                {
                    std::lock_guard<std::mutex> lock(sensory_buffer.mtx);
                    sensory_buffer.visual = visual_input;
                    sensory_buffer.audio = audio_samples;
                }
                
                // 6. Keep UI responsive
                std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
            }

            // Cleanup
            brain_running = false;
            if (brain_thread.joinable()) {
                brain_thread.join();
            }
            std::cout << "Sandbox Session Completed." << std::endl;
            
        } else {
             std::cout << "Running Unified Substrate (Non-Embodied)..." << std::endl;
             // Just step binding/sequence
             for (int step = 0; step < steps && !g_abort; ++step) {
                 adapter->stepBinding(step);
                 adapter->stepSequence(step);
                 if (step % 100 == 0) std::cout << "Step " << step << " completed." << std::endl;
             }
        }
    } else {
        std::cout << "Standard Brain Execution (Placeholder)..." << std::endl;
        // ... existing legacy logic would go here ...
    }

    std::cout << "Shutdown complete." << std::endl;
    return 0;
}
