#include "runtime/SemanticN2Runner.h"

#include "perception/world/SemanticProjection.h"
#include "perception/world/WorldModelCortex.h"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Runtime {

static std::uint64_t steady_ms_now() {
  using namespace std::chrono;
  return static_cast<std::uint64_t>(
      duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

static NeuroForge::Perception::ActiveConcept make_concept(std::uint64_t now_ms,
                                                          const std::string &lbl) {
  NeuroForge::Perception::ActiveConcept ac;
  ac.concept_id = lbl;
  ac.embedding.assign(64, 0.0f);
  if (lbl == "biology") {
    std::fill(ac.embedding.begin(), ac.embedding.begin() + 20, 1.0f);
  } else if (lbl == "physics") {
    std::fill(ac.embedding.begin() + 20, ac.embedding.begin() + 40, 1.0f);
  } else if (lbl == "psychology") {
    std::fill(ac.embedding.begin() + 40, ac.embedding.begin() + 60, 1.0f);
  }
  ac.activation = 1.0f;
  ac.grounding_confidence = 1.0f;
  ac.predictive_power = 1.0f;
  ac.last_grounded_ms = now_ms;
  return ac;
}

int runSemanticN2(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                  NeuroForge::Perception::SemanticProjection &semantic_projector,
                  const SemanticN2Config &cfg) {
  std::ofstream n2_log(cfg.log_csv_path);
  if (n2_log.is_open() && n2_log.tellp() == 0) {
    n2_log << "step,timestamp,latent_norm,injection_count,active_concepts\n";
  }

  for (int i = 0; i < cfg.steps; ++i) {
    std::uint64_t now_ms = steady_ms_now();

    std::vector<std::string> target_concepts;
    if (i >= 100 && i < 200) {
      target_concepts = {"biology"};
    } else if (i >= 200 && i < 300) {
      target_concepts = {"biology", "physics"};
    } else if (i >= 300 && i < 400) {
      target_concepts = {"physics"};
    } else if (i >= 400 && i < 500) {
      target_concepts = {"physics", "psychology"};
    } else if (i >= 600 && i < 800) {
      bool phase_a = ((i - 600) / 10) % 2 == 0;
      target_concepts =
          phase_a ? std::vector<std::string>{"biology"}
                  : std::vector<std::string>{"psychology"};
    }

    std::vector<NeuroForge::Perception::ActiveConcept> active_concepts;
    active_concepts.reserve(target_concepts.size());
    for (const auto &lbl : target_concepts) {
      active_concepts.push_back(make_concept(now_ms, lbl));
    }

    std::vector<float> semantic_input =
        semantic_projector.projectFromConcepts(active_concepts, now_ms);

    auto world_state =
        world_model_cortex.processCycle({}, {}, {}, {}, {}, {}, {}, semantic_input);

    if (n2_log.is_open()) {
      float ln = 0.0f;
      for (float v : world_state.latent) {
        ln += v * v;
      }
      ln = std::sqrt(ln);

      std::string concept_list;
      for (const auto &ac : active_concepts) {
        if (!concept_list.empty()) {
          concept_list += ";";
        }
        concept_list += ac.concept_id;
      }
      if (concept_list.empty()) {
        concept_list = "none";
      }

      n2_log << i << "," << now_ms << "," << ln << "," << active_concepts.size()
             << "," << concept_list << "\n";
      if (i % 10 == 0) {
        n2_log.flush();
      }
    }
  }

  return 0;
}

} // namespace Runtime
} // namespace NeuroForge

