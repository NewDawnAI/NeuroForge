#pragma once

#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Perception {
class WorldModelCortex;
}
namespace Runtime {

struct N7MinimalConfig {
  int max_steps = 1000;
  std::uint32_t seed = 7;
  std::uint32_t collision_budget = 250;
  std::string log_csv_path = "n7_minimal_gridworld_log.csv";
};

int runN7Minimal(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                 const N7MinimalConfig &cfg);

} // namespace Runtime
} // namespace NeuroForge

