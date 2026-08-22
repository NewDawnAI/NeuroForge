#include "runtime/SemanticN4Runner.h"

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

static float vec_mag(const std::vector<float> &v) {
  float s = 0.0f;
  for (float x : v) {
    s += x * x;
  }
  return std::sqrt(s);
}

int runSemanticN4(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                  NeuroForge::Perception::SemanticProjection &semantic_projector,
                  const SemanticN4Config &cfg) {
  std::ofstream n4_log(cfg.log_csv_path);
  if (n4_log.is_open() && n4_log.tellp() == 0) {
    n4_log << "step,timestamp,phase,latent_norm,latent_entropy,"
              "prediction_error,prediction_error_norm,"
              "learning_allowed,learning_rate,model_confidence,"
              "parameter_delta_norm,semantic_active\n";
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

  auto make_drifting_visual = [&](int step_index) {
    std::vector<float> out = base_visual;
    std::size_t shift =
        static_cast<std::size_t>((step_index / 5) % out.size());
    std::rotate(out.begin(), out.begin() + shift, out.end());
    for (std::size_t vi = 0; vi < out.size(); ++vi) {
      out[vi] += 0.02f * std::sin(0.03f * step_index +
                                  static_cast<float>(vi) * 0.1f);
    }
    return out;
  };

  for (int i = 0; i < cfg.steps; ++i) {
    std::uint64_t now_ms = steady_ms_now();

    std::string phase = "none";
    if (i < 200) {
      phase = "baseline";
    } else if (i < 400) {
      phase = "stable_world";
    } else if (i < 500) {
      phase = "semantic_injection";
    } else if (i < 700) {
      phase = "post_semantic";
    } else if (i < 800) {
      phase = "noise_burst";
    } else {
      phase = "recovery";
    }

    bool injection_active = (phase == "semantic_injection");
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
    if (phase == "noise_burst") {
      visual_input = make_incoherent_visual(i);
    } else if (phase == "stable_world" || phase == "post_semantic" ||
               phase == "recovery") {
      visual_input = make_drifting_visual(i);
    } else {
      visual_input = base_visual;
    }

    std::vector<float> semantic_input =
        semantic_projector.projectFromConcepts(active_concepts, now_ms);

    bool has_pred = world_model_cortex.hasPendingPrediction();
    NeuroForge::Perception::WorldState predicted_state;
    if (has_pred) {
      predicted_state = world_model_cortex.getPendingPrediction();
    }

    NeuroForge::Perception::WorldState observed =
        world_model_cortex.processCycle(visual_input, {}, {}, {}, {}, {}, {}, semantic_input);

    float pred_err = 0.0f;
    float pred_err_norm = 0.0f;
    if (has_pred) {
      pred_err = std::clamp(predicted_state.distanceTo(observed), 0.001f, 10.0f);
      pred_err_norm = std::clamp(std::tanh(pred_err), 0.0f, 1.0f);
    }

    bool semantic_active = vec_mag(semantic_input) > 1e-6f;
    bool learning_window =
        (phase == "stable_world") || (phase == "post_semantic") || (phase == "recovery");
    bool learning_allowed = false;
    float learning_rate = 0.0f;
    float delta_norm = 0.0f;
    const float epsilon_min = 0.0005f;
    const float epsilon_max = 0.8f;
    const float base_lr = 0.01f;
    if (learning_window && has_pred && !semantic_active &&
        pred_err_norm > epsilon_min && pred_err_norm < epsilon_max) {
      learning_allowed = true;
      learning_rate = base_lr * std::clamp(pred_err_norm, 0.1f, 1.0f);
      delta_norm = world_model_cortex.getPredictor().updateModel(predicted_state, observed,
                                                                learning_rate);
    }

    float model_confidence = 1.0f - pred_err_norm;

    if (n4_log.is_open()) {
      n4_log << i << "," << now_ms << "," << phase << "," << latent_norm(observed)
             << "," << latent_entropy(observed) << "," << pred_err << ","
             << pred_err_norm << "," << (learning_allowed ? 1 : 0) << ","
             << learning_rate << "," << model_confidence << "," << delta_norm
             << "," << (semantic_active ? 1 : 0) << "\n";
      if (i % 10 == 0) {
        n4_log.flush();
      }
    }
  }

  return 0;
}

} // namespace Runtime
} // namespace NeuroForge

