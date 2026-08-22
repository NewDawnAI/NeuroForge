#include "runtime/N7ModelBasedRunner.h"

#include "perception/world/WorldModelCortex.h"
#include "sandbox/GridWorld.h"
#include "sandbox/LinearActionModel.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <tuple>
#include <vector>

namespace NeuroForge {
namespace Runtime {

int runN7ModelBased(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                    const N7ModelBasedConfig &cfg) {
  NeuroForge::Sandbox::GridWorld env(7, 7, cfg.seed);
  const std::size_t obs_dim = env.observe().size();
  const std::size_t act_dim = cfg.action_dim;
  NeuroForge::Sandbox::LinearActionModel model(obs_dim, act_dim);

  std::ofstream n7_log(cfg.log_csv_path);
  if (n7_log.is_open() && n7_log.tellp() == 0) {
    n7_log << "step,episode,mode,action,score,dist_before,dist_after,"
              "pred_dist_after,pred_collision,collision,reached,"
              "pred_error,learn_applied,lr,delta_norm,"
              "prediction_error,visual_weight,attention_alpha,lr_multiplier\n";
  }

  std::uint32_t episode = 0;
  std::uint32_t seed = cfg.seed;
  std::uint32_t collision_budget = cfg.collision_budget;
  int episode_steps = 0;

  auto score_from_pred = [&](const std::vector<float> &pred_next) {
    float dx = (pred_next.size() > 4) ? pred_next[4] : 0.0f;
    float dy = (pred_next.size() > 5) ? pred_next[5] : 0.0f;
    float pred_dist = std::fabs(dx) + std::fabs(dy);
    float pred_collision = (pred_next.size() > 6) ? pred_next[6] : 0.0f;
    pred_collision = std::clamp(pred_collision, 0.0f, 1.0f);
    float reached = pred_dist < 0.01f ? 1.0f : 0.0f;

    float s = 0.0f;
    if (reached > 0.5f) {
      s += 1000.0f;
    }
    s -= pred_dist * 50.0f;
    s -= pred_collision * 100.0f;
    return std::tuple<float, float, float>(s, pred_dist, pred_collision);
  };

  for (int step = 0; step < cfg.max_steps; ++step) {
    std::vector<float> obs = env.observe();
    (void)world_model_cortex.processCycle(obs, {}, {}, {}, {}, {}, {}, {});

    int dist_before = env.manhattanToGoal();
    auto reg = world_model_cortex.getRegulationState();

    NeuroForge::Sandbox::GridAction model_action =
        NeuroForge::Sandbox::GridAction::Stay;
    float model_score = -1e9f;
    float model_pred_dist_after = static_cast<float>(dist_before);
    float model_pred_collision = 0.0f;

    for (std::size_t ai = 0; ai < act_dim; ++ai) {
      std::vector<float> pred_next = model.predict(obs, ai);
      auto [s, pred_dist, pred_col] = score_from_pred(pred_next);
      if (s > model_score) {
        model_score = s;
        model_action = static_cast<NeuroForge::Sandbox::GridAction>(ai);
        model_pred_dist_after = pred_dist;
        model_pred_collision = pred_col;
      }
    }

    NeuroForge::Sandbox::GridAction chosen_action = model_action;
    std::string mode = "model";
    if (step < cfg.warmup_steps) {
      mode = "warmup";
      float best_score_gt = -1e9f;
      for (NeuroForge::Sandbox::GridAction a :
           {NeuroForge::Sandbox::GridAction::Up,
            NeuroForge::Sandbox::GridAction::Down,
            NeuroForge::Sandbox::GridAction::Left,
            NeuroForge::Sandbox::GridAction::Right,
            NeuroForge::Sandbox::GridAction::Stay}) {
        NeuroForge::Sandbox::GridWorld sim = env;
        auto sim_res = sim.step(a);
        int dist_after = sim.manhattanToGoal();
        float score = 0.0f;
        if (sim_res.reached_goal) {
          score += 1000.0f;
        }
        score += static_cast<float>(dist_before - dist_after) * 10.0f;
        score -= static_cast<float>(dist_after) * 1.0f;
        if (sim_res.collision) {
          score -= 50.0f;
        }
        if (score > best_score_gt) {
          best_score_gt = score;
          chosen_action = a;
        }
      }
    }

    auto res = env.step(chosen_action);
    if (res.collision && collision_budget > 0) {
      collision_budget--;
    }

    std::vector<float> next_obs = env.observe();
    std::vector<float> pred_next =
        model.predict(obs, static_cast<std::size_t>(chosen_action));
    float pred_err = model.computeErrorNorm(pred_next, next_obs);

    bool learn_applied = false;
    float lr = 0.0f;
    float delta_norm = 0.0f;
    float pred_err_norm = std::clamp(std::tanh(pred_err), 0.0f, 1.0f);
    const float eps_min = 0.01f;
    const float eps_max = 0.95f;
    if (!res.collision && pred_err_norm > eps_min && pred_err_norm < eps_max) {
      lr = 0.01f * reg.learning_rate_multiplier *
           std::clamp(pred_err_norm, 0.1f, 1.0f);
      delta_norm =
          model.update(obs, static_cast<std::size_t>(chosen_action), next_obs, lr);
      learn_applied = true;
    }

    int dist_after = env.manhattanToGoal();
    float pe = world_model_cortex.getLastPredictionError();
    auto att = world_model_cortex.getAttentionState();

    if (n7_log.is_open()) {
      n7_log << step << "," << episode << "," << mode << ","
             << NeuroForge::Sandbox::GridWorld::actionToString(chosen_action)
             << "," << model_score << "," << dist_before << "," << dist_after
             << "," << model_pred_dist_after << "," << model_pred_collision << ","
             << (res.collision ? 1 : 0) << "," << (res.reached_goal ? 1 : 0)
             << "," << pred_err << "," << (learn_applied ? 1 : 0) << "," << lr
             << "," << delta_norm << "," << pe << "," << att.visual_weight << ","
             << reg.attention_alpha << "," << reg.learning_rate_multiplier << "\n";
      if (step % 10 == 0) {
        n7_log.flush();
      }
    }

    episode_steps++;
    if (res.reached_goal || episode_steps >= cfg.max_episode_steps) {
      episode++;
      seed += 17;
      env.reset(seed);
      episode_steps = 0;
    }

    if (collision_budget == 0) {
      break;
    }
  }

  return 0;
}

} // namespace Runtime
} // namespace NeuroForge

