#pragma once

/**
 * @file GrandUnifiedRunner.h
 * @brief Phase N-Genesis: Grand Unified Runner
 *
 * Merges Web perception (WebSandboxRunner) with embodied action
 * (UnifiedAliveRunner) and wires in the full cognitive stack:
 *   Phase 7 – Reflection
 *   Phase 8 – Goal System
 *   Phase 9 – Metacognition
 *   SelfModel – Identity/Personality
 *   LanguageSystem + AcquisitionLoop
 *   NoveltyBias + Attention Modulator
 */

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace NeuroForge {
namespace Core {
class MemoryDB;
}
namespace Perception {
class WorldModelCortex;
}
namespace Runtime {

struct GrandUnifiedConfig {
  // ── Web Perception ──
  std::string start_url = "https://en.wikipedia.org/wiki/Special:Random";
  int window_width = 1280;
  int window_height = 800;
  bool enable_vision = false; // Capture screenshots (expensive)
  bool enable_camera = false; // Phase 4: Capture real-world frames from webcam
  bool enable_web = true;     // Set false for --no-webview fallback

  // ── GridWorld Embodiment ──
  std::uint32_t seed = 42;
  std::size_t action_dim = 5;

  // ── Gating Thresholds ──
  float max_collision_prob = 0.01f;
  int horizon_steps = 10;

  // ── Language ──
  std::size_t max_vocabulary = 50000;
  int embedding_dim = 64;

  // ── Loop Control ──
  bool infinite_mode = true;
  int max_steps = 100000;

  // ── Reflection/Cognition ──
  int reflect_every_n_episodes = 5; // Phase 7 reflection cadence
  double goal_decay_rate = 0.001;   // Phase 8 stability decay/s

  // ── Logging ──
  std::string log_csv_path = "genesis_log.csv";

  // ── Parallel Loop (Stage 2) ──
  bool enable_parallel =
      false;              // Opt-in: true = multi-threaded, false = sequential
  int perception_hz = 50; // Thread 1: WorldModel + perceive
  int grid_hz = 20;       // Thread 2: GridWorld motor actions
  int web_hz = 2;         // Thread 3: WebSandbox language acq
  int cognition_hz = 1;   // Thread 4: Reflection + Metacog
  int consolidation_interval_s = 30; // Thread 5: Dream/consolidation interval

  // ── Brain Persistence ──
  std::string brain_state_dir = "data/brain_state";
  bool auto_save = true; // Save state on shutdown
  bool auto_load = true; // Load state on startup

  // ── Conversational Interface ──
  bool conversational_mode = false;

  // ── Circadian Rhythm (Phase 3) ──
  bool start_asleep = false;
  int wake_duration_s = 15;  // Time awake before sleeping (15 sec test)
  int sleep_duration_s = 10; // Time spent sleeping (10 sec test)
};

struct GrandUnifiedSinks {
  std::function<void(const std::string &)> emit_json_line;
};

/// The Genesis Loop — all subsystems active.
/// Returns 0 on clean exit, non-zero on critical failure.
int runGrandUnified(
    NeuroForge::Perception::WorldModelCortex &world_model_cortex,
    NeuroForge::Core::MemoryDB *memdb, std::uint64_t memdb_run_id,
    bool log_json, const GrandUnifiedSinks &sinks,
    const GrandUnifiedConfig &cfg, const std::atomic<bool> &abort_signal);

} // namespace Runtime
} // namespace NeuroForge
