#pragma once

#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Perception {
class WorldModelCortex;
}
namespace Runtime {

struct N7ModelBasedConfig {
  int max_steps = 1000;
  int warmup_steps = 50;
  int max_episode_steps = 64;
  std::uint32_t seed = 7;
  std::uint32_t collision_budget = 50;
  std::size_t action_dim = 5;
  std::string log_csv_path = "n7_modelbased_gridworld_log.csv";
};

int runN7ModelBased(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                    const N7ModelBasedConfig &cfg);

} // namespace Runtime
} // namespace NeuroForge

