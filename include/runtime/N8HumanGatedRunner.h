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

struct N8HumanGatedConfig {
  int max_steps = 5000;
  int warmup_steps = 50;
  int max_episode_steps = 128; // Standard N-series limit
  std::uint32_t collision_budget = 50;
  std::uint32_t seed = 42;
  std::size_t action_dim = 5;
  std::string log_csv_path = "n8_human_gated_log.csv";
  std::string summary_csv_path = "n8_human_gated_summary.csv";
};

struct N8HumanGatedSinks {
  std::function<void(const std::string &)> emit_json_line;
};

int runN8HumanGated(
    NeuroForge::Perception::WorldModelCortex &world_model_cortex,
    NeuroForge::Core::MemoryDB *memdb, std::uint64_t memdb_run_id,
    bool log_json, const N8HumanGatedSinks &sinks,
    const N8HumanGatedConfig &cfg);

} // namespace Runtime
} // namespace NeuroForge
