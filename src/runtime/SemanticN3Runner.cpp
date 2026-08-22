#include "runtime/SemanticN3Runner.h"

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

int runSemanticN3(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                  NeuroForge::Perception::SemanticProjection &semantic_projector,
                  const SemanticN3Config &cfg) {
  std::ofstream n3_log(cfg.log_csv_path);
  if (n3_log.is_open() && n3_log.tellp() == 0) {
    n3_log << "step,timestamp,phase,latent_norm,latent_entropy,"
              "prediction_error,semantic_gain,effective_activation,"
              "dominance_ratio,injection_active\n";
  }

  float last_prediction_error_norm = 0.0f;
  world_model_cortex.setSurpriseCallback(
      [&](float surprise_level, float /*prediction_error*/) {
        last_prediction_error_norm = std::clamp(surprise_level, 0.0f, 1.0f);
      });

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
    bool injection_active = false;
    if (i < 100) {
      phase = "baseline";
      injection_active = false;
    } else if (i < 200) {
      phase = "injection";
      injection_active = true;
    } else if (i < 300) {
      phase = "prediction_aligned";
      injection_active = true;
    } else if (i < 400) {
      phase = "prediction_violated";
      injection_active = true;
    } else if (i < 500) {
      phase = "recovery";
      injection_active = false;
    } else if (i < 700) {
      bool phase_a = ((i - 500) / 10) % 2 == 0;
      phase = phase_a ? "osc_aligned" : "osc_violated";
      injection_active = true;
    } else {
      phase = "cooldown";
      injection_active = false;
    }

    const float k = 0.5f;
    float semantic_gain =
        std::clamp(1.0f + k * std::clamp(last_prediction_error_norm, 0.0f, 1.0f),
                   0.5f, 1.5f);
    float effective_activation = 0.0f;
    float dominance_ratio = 0.0f;

    std::vector<NeuroForge::Perception::ActiveConcept> active_concepts;
    if (injection_active) {
      NeuroForge::Perception::ActiveConcept ac;
      ac.concept_id = "biology";
      ac.embedding.assign(64, 0.0f);
      std::fill(ac.embedding.begin(),
                ac.embedding.begin() + std::min<std::size_t>(20, ac.embedding.size()),
                1.0f);
      float base_activation = 1.0f;
      effective_activation = base_activation * semantic_gain;
      ac.activation = effective_activation;
      ac.grounding_confidence = 1.0f;
      ac.predictive_power = 1.0f;
      ac.last_grounded_ms = now_ms;
      active_concepts.push_back(ac);
      dominance_ratio = 1.0f;
    }

    bool violated = (phase == "prediction_violated") || (phase == "osc_violated");
    std::vector<float> visual_input =
        violated ? make_incoherent_visual(i) : base_visual;

    std::vector<float> semantic_input =
        semantic_projector.projectFromConcepts(active_concepts, now_ms);

    auto world_state =
        world_model_cortex.processCycle(visual_input, {}, {}, {}, {}, {}, {}, semantic_input);

    if (n3_log.is_open()) {
      n3_log << i << "," << now_ms << "," << phase << "," << latent_norm(world_state)
             << "," << latent_entropy(world_state) << ","
             << last_prediction_error_norm << "," << semantic_gain << ","
             << effective_activation << "," << dominance_ratio << ","
             << (injection_active ? 1 : 0) << "\n";
      if (i % 10 == 0) {
        n3_log.flush();
      }
    }
  }

  return 0;
}

} // namespace Runtime
} // namespace NeuroForge

