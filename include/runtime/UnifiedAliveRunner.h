#pragma once

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

struct UnifiedAliveConfig {
  std::uint32_t seed = 42;
  std::uint32_t collision_budget = 1000; // Larger budget for long runs
  std::size_t action_dim = 5;

  // Gating Thresholds (Conservative)
  float max_collision_prob = 0.01f;
  int horizon_steps = 10;

  // If true, runs forever until external abort.
  // If false, runs for max_steps (for testing).
  bool infinite_mode = true;
  int max_steps = 10000;

  std::string log_csv_path = "alive_log.csv";
  std::string summary_csv_path = "alive_summary.csv";
};

struct UnifiedAliveSinks {
  std::function<void(const std::string &)> emit_json_line;
};

// Returns 0 on success (or manual abort), non-zero on critical failure.
int runUnifiedAlive(
    NeuroForge::Perception::WorldModelCortex &world_model_cortex,
    NeuroForge::Core::MemoryDB *memdb, std::uint64_t memdb_run_id,
    bool log_json, const UnifiedAliveSinks &sinks,
    const UnifiedAliveConfig &cfg, const std::atomic<bool> &abort_signal);

} // namespace Runtime
} // namespace NeuroForge
