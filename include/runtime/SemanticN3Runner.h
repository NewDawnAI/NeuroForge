#pragma once

#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Perception {
class SemanticProjection;
class WorldModelCortex;
}
namespace Runtime {

struct SemanticN3Config {
  int steps = 1000;
  std::string log_csv_path = "semantic_n3_log.csv";
};

int runSemanticN3(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                  NeuroForge::Perception::SemanticProjection &semantic_projector,
                  const SemanticN3Config &cfg);

} // namespace Runtime
} // namespace NeuroForge

