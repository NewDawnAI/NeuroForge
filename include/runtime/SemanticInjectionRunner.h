#pragma once

#include <cstdint>
#include <string>

namespace NeuroForge {
namespace Perception {
class SemanticProjection;
class WorldModelCortex;
}
namespace Runtime {

struct SemanticInjectionConfig {
  int steps = 1000;
  std::string log_csv_path = "semantic_injection_log.csv";
};

int runSemanticInjection(
    NeuroForge::Perception::WorldModelCortex &world_model_cortex,
    NeuroForge::Perception::SemanticProjection &semantic_projector,
    const SemanticInjectionConfig &cfg);

} // namespace Runtime
} // namespace NeuroForge

