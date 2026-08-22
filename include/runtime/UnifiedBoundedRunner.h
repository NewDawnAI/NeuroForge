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

struct UnifiedBoundedConfig {
  int max_steps = 5000;
  int warmup_steps = 50;
  int max_episode_steps = 128;
  std::uint32_t collision_budget = 50;
  std::uint32_t seed = 7;
  std::size_t action_dim = 5;
  std::string log_csv_path = "unified_bounded_log.csv";
  std::string summary_csv_path = "unified_bounded_summary.csv";

  bool stage_d3_live = false;
  std::string stage_d3_live_mode = "match";
  int stage_d3_match_window = 500;
  double stage_d3_min_match_rate = 0.80;
  double stage_d3_min_advantage = 17.0;
  double stage_d3_min_pred_dist_advantage = 0.0;
  double stage_d3_max_pred_collision = 0.05;
  double stage_d3_min_dist_improve = 0.05;
};

struct UnifiedBoundedSinks {
  std::function<void(const std::string &)> emit_json_line;
  std::function<void(std::int64_t ts_ms, std::uint64_t step,
                     const std::string &event, const std::string &payload_json)>
      insert_experience;
};

int runUnifiedBounded(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                     NeuroForge::Core::MemoryDB *memdb,
                     std::uint64_t memdb_run_id, bool log_json,
                     const UnifiedBoundedSinks &sinks,
                     const UnifiedBoundedConfig &cfg);

} // namespace Runtime
} // namespace NeuroForge
