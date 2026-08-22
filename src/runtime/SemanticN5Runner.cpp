#include "runtime/SemanticN5Runner.h"

#include "perception/world/SemanticProjection.h"
#include "perception/world/WorldModelCortex.h"

#include <algorithm>
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

static float latent_norm(const NeuroForge::Perception::WorldState &s) {
  float n = 0.0f;
  for (float v : s.latent) {
    n += v * v;
  }
  return std::sqrt(n);
}

static float latent_entropy(const NeuroForge::Perception::WorldState &s) {
  float abs_sum = 0.0f;
  for (float v : s.latent) {
    abs_sum += std::fabs(v);
  }
  if (abs_sum <= 1e-12f) {
    return 0.0f;
  }
  float ent = 0.0f;
  for (float v : s.latent) {
    float p = std::fabs(v) / abs_sum;
    if (p > 1e-12f) {
      ent -= p * std::log(p);
    }
  }
  return ent;
}

int runSemanticN5(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                  NeuroForge::Perception::SemanticProjection &semantic_projector,
                  const SemanticN5Config &cfg) {
  std::ofstream n5_log(cfg.log_csv_path);
  if (n5_log.is_open() && n5_log.tellp() == 0) {
    n5_log << "step,prediction_error,semantic_weight,visual_weight,"
              "auditory_weight,latent_norm,latent_entropy\n";
  }

  std::vector<float> base_visual(64);
  for (std::size_t vi = 0; vi < base_visual.size(); ++vi) {
    base_visual[vi] = static_cast<float>(vi % 16) / 16.0f * 0.5f;
  }

  auto make_incoherent_visual = [&](int step_index) {
    std::vector<float> out = base_visual;
    std::size_t shift = static_cast<std::size_t>((step_index * 7) % out.size());
    std::rotate(out.begin(), out.begin() + shift, out.end());
    for (std::size_t vi = 0; vi < out.size(); ++vi) {
      out[vi] = (out[vi] * 0.7f) +
                (std::sin(0.1f * step_index + static_cast<float>(vi)) * 0.1f);
    }
    return out;
  };

  for (int i = 0; i < cfg.steps; ++i) {
    std::uint64_t now_ms = steady_ms_now();

    std::string phase = "none";
    if (i < 100) {
      phase = "baseline";
    } else if (i < 201) {
      phase = "multi_modal_injection";
    } else if (i < 301) {
      phase = "violation";
    } else if (i < 451) {
      phase = "recovery";
    } else if (i < 701) {
      phase = "alternating_stress";
    } else {
      phase = "cooldown";
    }

    bool injection_active = (phase == "multi_modal_injection");
    std::vector<NeuroForge::Perception::ActiveConcept> active_concepts;
    if (injection_active) {
      NeuroForge::Perception::ActiveConcept ac;
      ac.concept_id = "biology";
      ac.embedding.assign(64, 0.0f);
      std::fill(ac.embedding.begin(),
                ac.embedding.begin() + std::min<std::size_t>(20, ac.embedding.size()),
                1.0f);
      ac.activation = 1.0f;
      ac.grounding_confidence = 1.0f;
      ac.predictive_power = 1.0f;
      ac.last_grounded_ms = now_ms;
      active_concepts.push_back(ac);
    }

    std::vector<float> visual_input;
    if (phase == "cooldown") {
      visual_input.clear();
    } else if (phase == "violation") {
      visual_input = make_incoherent_visual(i);
    } else if (phase == "alternating_stress") {
      bool noisy = ((i - 451) / 10) % 2 == 0;
      visual_input = noisy ? make_incoherent_visual(i) : base_visual;
    } else {
      visual_input = base_visual;
    }

    std::vector<float> semantic_input =
        semantic_projector.projectFromConcepts(active_concepts, now_ms);

    NeuroForge::Perception::WorldState world_state =
        world_model_cortex.processCycle(visual_input, {}, {}, {}, {}, {}, {}, semantic_input);

    auto att = world_model_cortex.getAttentionState();
    float prediction_error = world_model_cortex.getLastPredictionError();

    if (n5_log.is_open()) {
      n5_log << i << "," << prediction_error << "," << att.semantic_weight << ","
             << att.visual_weight << "," << att.auditory_weight << ","
             << latent_norm(world_state) << "," << latent_entropy(world_state)
             << "\n";
      if (i % 10 == 0) {
        n5_log.flush();
      }
    }
  }

  return 0;
}

} // namespace Runtime
} // namespace NeuroForge

