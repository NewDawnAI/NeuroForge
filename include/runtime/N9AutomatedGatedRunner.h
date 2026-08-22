#pragma once

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

struct N9AutomatedGatedConfig {
  int max_steps = 5000;
  int warmup_steps = 50;
  int max_episode_steps = 128;
  std::uint32_t collision_budget = 50;
  std::uint32_t seed = 42;
  std::size_t action_dim = 5;

  // Gating Thresholds
  float max_collision_prob = 0.01f; // Strict safety
  float max_uncertainty = 0.4f;     // Confidence threshold
  int horizon_steps = 10;           // How far to predict ahead for gating

  std::string log_csv_path = "n9_auto_gated_log.csv";
  std::string summary_csv_path = "n9_auto_gated_summary.csv";
};

struct N9AutomatedGatedSinks {
  std::function<void(const std::string &)> emit_json_line;
};

int runN9AutomatedGated(
    NeuroForge::Perception::WorldModelCortex &world_model_cortex,
    NeuroForge::Core::MemoryDB *memdb, std::uint64_t memdb_run_id,
    bool log_json, const N9AutomatedGatedSinks &sinks,
    const N9AutomatedGatedConfig &cfg);

} // namespace Runtime
} // namespace NeuroForge
