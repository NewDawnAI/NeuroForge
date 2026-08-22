#include "runtime/SemanticInjectionRunner.h"

#include "perception/world/SemanticProjection.h"
#include "perception/world/WorldModelCortex.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <vector>

namespace NeuroForge {
namespace Runtime {

static std::uint64_t steady_ms_now() {
  using namespace std::chrono;
  return static_cast<std::uint64_t>(
      duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

int runSemanticInjection(
    NeuroForge::Perception::WorldModelCortex &world_model_cortex,
    NeuroForge::Perception::SemanticProjection &semantic_projector,
    const SemanticInjectionConfig &cfg) {
  std::ofstream inject_log(cfg.log_csv_path);

  for (int i = 0; i < cfg.steps; ++i) {
    std::uint64_t now_ms = steady_ms_now();
    bool injection_active = (i >= 100 && i <= 200);

    std::vector<NeuroForge::Perception::ActiveConcept> active_concepts;
    if (injection_active) {
      NeuroForge::Perception::ActiveConcept ac1;
      ac1.concept_id = "synthetic_motion";
      ac1.embedding.assign(64, 1.0f);
      ac1.activation = 1.0f;
      ac1.grounding_confidence = 1.0f;
      ac1.predictive_power = 1.0f;
      ac1.last_grounded_ms = now_ms;
      active_concepts.push_back(ac1);

      NeuroForge::Perception::ActiveConcept ac2;
      ac2.concept_id = "synthetic_object";
      ac2.embedding.assign(64, 0.5f);
      ac2.activation = 0.8f;
      ac2.grounding_confidence = 0.8f;
      ac2.predictive_power = 0.8f;
      ac2.last_grounded_ms = now_ms;
      active_concepts.push_back(ac2);
    }

    std::vector<float> semantic_input =
        semantic_projector.projectFromConcepts(active_concepts, now_ms);

    float semantic_mag = 0.0f;
    for (float v : semantic_input) {
      semantic_mag += v * v;
    }
    semantic_mag = std::sqrt(semantic_mag);

    auto world_state =
        world_model_cortex.processCycle({}, {}, {}, {}, {}, {}, {}, semantic_input);

    if (inject_log.is_open()) {
      float latent_mag = 0.0f;
      for (float v : world_state.latent) {
        latent_mag += v * v;
      }
      latent_mag = std::sqrt(latent_mag);

      inject_log << i << "," << now_ms << "," << (injection_active ? 1 : 0) << ","
                 << semantic_mag << "," << latent_mag << "\n";
      if (i % 10 == 0) {
        inject_log.flush();
      }
    }
  }

  return 0;
}

} // namespace Runtime
} // namespace NeuroForge

