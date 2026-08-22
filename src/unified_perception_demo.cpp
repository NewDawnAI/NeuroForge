/**
 * @file unified_perception_demo.cpp
 * @brief Unified demo integrating WorldModelCortex + VocalMotorCortex
 *
 * Demonstrates closed perception-action-prediction loop.
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <memory>
#include <random>
#include <thread>
#include <vector>

// Core systems
#include "connectivity/ConnectivityManager.h"
#include "core/HypergraphBrain.h"
#include "core/LanguageSystem.h"
#include "core/MemoryDB.h"

// World Model
#include "perception/world/WorldModelCortex.h"
#include "perception/world/WorldState.h"

// Vocal Motor
#include "motor/VocalAction.h"
#include "motor/VocalMotorCortex.h"

// Regions
#include "regions/CorticalRegions.h"

static std::atomic<bool> g_abort{false};

void signalHandler(int) { g_abort.store(true); }

int main(int argc, char *argv[]) {
  std::signal(SIGINT, signalHandler);

  std::cout << "\n";
  std::cout
      << "================================================================\n";
  std::cout << "       NEUROFORGE UNIFIED PERCEPTION DEMO\n";
  std::cout << "       WorldModelCortex + VocalMotorCortex Integration\n";
  std::cout
      << "================================================================\n\n";

  // Parse arguments
  int steps = 500;
  std::string db_path = "unified_perception.db";

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg.rfind("--steps=", 0) == 0) {
      steps = std::stoi(arg.substr(8));
    } else if (arg.rfind("--db=", 0) == 0) {
      db_path = arg.substr(5);
    } else if (arg == "--help") {
      std::cout << "Usage: unified_perception_demo [options]\n"
                << "  --steps=N    Number of steps (default: 500)\n"
                << "  --db=PATH    Memory database path\n";
      return 0;
    }
  }

  // Initialize MemoryDB
  auto memdb = std::make_shared<NeuroForge::Core::MemoryDB>(db_path);
  std::int64_t run_id = 0;
  if (memdb->open() && memdb->ensureSchema()) {
    memdb->beginRun("{\"name\":\"Unified Perception Demo\"}", run_id);
    std::cout << "[Init] MemoryDB opened: " << db_path << " (run=" << run_id
              << ")\n";
  }

  // Initialize brain
  auto connectivity =
      std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
  auto brain =
      std::make_shared<NeuroForge::Core::HypergraphBrain>(connectivity);

  auto visual_cortex =
      std::make_shared<NeuroForge::Regions::VisualCortex>("VisualCortex", 2000);
  visual_cortex->initializeLayers();
  brain->addRegion(visual_cortex);

  auto auditory_cortex = std::make_shared<NeuroForge::Regions::AuditoryCortex>(
      "AuditoryCortex", 2000);
  auditory_cortex->initializeTonotopicMap();
  brain->addRegion(auditory_cortex);

  std::cout << "[Init] Brain with Visual and Auditory Cortex initialized\n";

  // Initialize WorldModelCortex
  NeuroForge::Perception::WorldModelCortexConfig world_cfg;
  world_cfg.encoder_config.latent_dim = 256;
  world_cfg.encoder_config.visual_input_dim = 64;
  world_cfg.encoder_config.auditory_input_dim = 64;

  auto world_model =
      std::make_shared<NeuroForge::Perception::WorldModelCortex>(world_cfg);

  // Track surprise via callback
  float last_surprise = 0.0f;
  float total_surprise = 0.0f;
  int surprise_count = 0;

  world_model->setSurpriseCallback(
      [&](float surprise_level, float /*prediction_error*/) {
        last_surprise = surprise_level;
        total_surprise += surprise_level;
        surprise_count++;
      });

  std::cout << "[Init] WorldModelCortex initialized (latent_dim="
            << world_cfg.encoder_config.latent_dim << ")\n";

  // Initialize VocalMotorCortex
  NeuroForge::Motor::VocalMotorCortexConfig vocal_cfg;
  vocal_cfg.development.initial_stage = NeuroForge::Motor::VocalStage::BABBLING;

  auto vocal_motor =
      std::make_shared<NeuroForge::Motor::VocalMotorCortex>(vocal_cfg);
  std::cout << "[Init] VocalMotorCortex initialized (stage=BABBLING)\n";

  // Random generators
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> uniform(0.0f, 1.0f);

  std::cout << "\n[Loop] Starting unified perception loop (" << steps
            << " steps)...\n\n";

  auto start_time = std::chrono::steady_clock::now();

  for (int step = 0; step < steps && !g_abort.load(); ++step) {
    // 1. Generate synthetic perception
    std::vector<float> visual(64), motion(16), temporal(8), social(32),
        spatial(24), auditory(64), linguistic(32);

    for (auto &v : visual)
      v = uniform(rng);
    for (auto &v : auditory)
      v = uniform(rng) * 0.5f;

    // 2. WorldModelCortex processing
    auto world_state = world_model->processCycle(
        visual, motion, temporal, social, spatial, auditory, linguistic);

    // 3. VocalMotorCortex babbling
    auto vocal_action = vocal_motor->generateAction();

    // Self-hearing: create feedback and feed to motor
    NeuroForge::Motor::VocalFeedback feedback;
    feedback.timestamp_ms = static_cast<uint64_t>(step * 10);
    feedback.observed_pitch = vocal_action.pitch + uniform(rng) * 10.0f;
    feedback.observed_amplitude = vocal_action.amplitude + uniform(rng) * 0.1f;
    feedback.observed_spectrum = vocal_action.spectral_envelope;

    vocal_motor->receiveFeedback(feedback);

    // 4. Log progress
    if (step % 50 == 0 || step == steps - 1) {
      auto stage = vocal_motor->getStage();
      std::string stage_name = "BABBLING";
      switch (stage) {
      case NeuroForge::Motor::VocalStage::PROSODY:
        stage_name = "PROSODY";
        break;
      case NeuroForge::Motor::VocalStage::SYLLABIC:
        stage_name = "SYLLABIC";
        break;
      case NeuroForge::Motor::VocalStage::PROTO_WORD:
        stage_name = "PROTO_WORD";
        break;
      case NeuroForge::Motor::VocalStage::LANGUAGE_BIASED:
        stage_name = "LANGUAGE_BIASED";
        break;
      default:
        break;
      }

      float latent_norm = 0.0f;
      for (float v : world_state.latent)
        latent_norm += v * v;
      latent_norm = std::sqrt(latent_norm);

      std::cout << "[Step " << step << "/" << steps << "] "
                << "Latent: " << std::fixed << std::setprecision(2)
                << latent_norm << " | Uncertainty: " << world_state.uncertainty
                << " | Surprise: " << last_surprise
                << " | Vocal: " << stage_name << "\n";
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  auto end_time = std::chrono::steady_clock::now();
  auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         end_time - start_time)
                         .count();

  std::cout
      << "\n================================================================\n";
  std::cout << "       UNIFIED PERCEPTION DEMO COMPLETE\n";
  std::cout
      << "================================================================\n";
  std::cout << "  Steps:          " << steps << "\n";
  std::cout << "  Duration:       " << duration_ms << " ms\n";
  std::cout << "  Avg Surprise:   "
            << (surprise_count > 0 ? total_surprise / surprise_count : 0.0f)
            << "\n";
  std::cout << "  Vocal Actions:  " << vocal_motor->getActionCount() << "\n";
  std::cout << "  Vocal Stage:    ";
  switch (vocal_motor->getStage()) {
  case NeuroForge::Motor::VocalStage::BABBLING:
    std::cout << "BABBLING";
    break;
  case NeuroForge::Motor::VocalStage::PROSODY:
    std::cout << "PROSODY";
    break;
  case NeuroForge::Motor::VocalStage::SYLLABIC:
    std::cout << "SYLLABIC";
    break;
  case NeuroForge::Motor::VocalStage::PROTO_WORD:
    std::cout << "PROTO_WORD";
    break;
  case NeuroForge::Motor::VocalStage::LANGUAGE_BIASED:
    std::cout << "LANGUAGE_BIASED";
    break;
  }
  std::cout << "\n============================================================="
               "===\n\n";

  return 0;
}
