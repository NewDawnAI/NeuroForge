#pragma once

#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Perception {
class WorldModelCortex;
}
namespace Runtime {

struct N7BoundedConfig {
  int max_steps = 1000;
  int max_episode_steps = 64;
  std::uint32_t seed = 7;
  std::uint32_t collision_budget = 50;
  std::string log_csv_path = "n7_bounded_gridworld_log.csv";
};

int runN7Bounded(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                 const N7BoundedConfig &cfg);

} // namespace Runtime
} // namespace NeuroForge

