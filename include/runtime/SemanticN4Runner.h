#pragma once

#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Perception {
class SemanticProjection;
class WorldModelCortex;
}
namespace Runtime {

struct SemanticN4Config {
  int steps = 1000;
  std::uint32_t seed = 1337;
  std::string log_csv_path = "semantic_n4_log.csv";
};

int runSemanticN4(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                  NeuroForge::Perception::SemanticProjection &semantic_projector,
                  const SemanticN4Config &cfg);

} // namespace Runtime
} // namespace NeuroForge

