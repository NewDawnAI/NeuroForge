#include "runtime/N7BoundedRunner.h"

#include "perception/world/WorldModelCortex.h"
#include "sandbox/GridWorld.h"

#include <cstdint>
#include <fstream>
#include <vector>

namespace NeuroForge {
namespace Runtime {

int runN7Bounded(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                 const N7BoundedConfig &cfg) {
  NeuroForge::Sandbox::GridWorld env(7, 7, cfg.seed);
  std::ofstream n7_log(cfg.log_csv_path);
  if (n7_log.is_open() && n7_log.tellp() == 0) {
    n7_log << "step,episode,action,score,dist_before,dist_after,collision,"
              "reached,prediction_error,semantic_weight,visual_weight,"
              "attention_alpha,attention_lambda,lr_multiplier\n";
  }

  std::uint32_t episode = 0;
  std::uint32_t seed = cfg.seed;
  std::uint32_t collision_budget = cfg.collision_budget;
  int episode_steps = 0;

  for (int step = 0; step < cfg.max_steps; ++step) {
    std::vector<float> obs = env.observe();
    (void)world_model_cortex.processCycle(obs, {}, {}, {}, {}, {}, {}, {});

    int dist_before = env.manhattanToGoal();

    NeuroForge::Sandbox::GridAction best_action =
        NeuroForge::Sandbox::GridAction::Stay;
    float best_score = -1e9f;
    int best_dist_after = dist_before;
    for (NeuroForge::Sandbox::GridAction a :
         {NeuroForge::Sandbox::GridAction::Up, NeuroForge::Sandbox::GridAction::Down,
          NeuroForge::Sandbox::GridAction::Left, NeuroForge::Sandbox::GridAction::Right,
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
      if (score > best_score) {
        best_score = score;
        best_action = a;
        best_dist_after = dist_after;
      }
    }

    auto res = env.step(best_action);
    if (res.collision && collision_budget > 0) {
      collision_budget--;
    }

    auto att = world_model_cortex.getAttentionState();
    auto reg = world_model_cortex.getRegulationState();
    float pe = world_model_cortex.getLastPredictionError();

    if (n7_log.is_open()) {
      n7_log << step << "," << episode << ","
             << NeuroForge::Sandbox::GridWorld::actionToString(best_action) << ","
             << best_score << "," << dist_before << "," << best_dist_after << ","
             << (res.collision ? 1 : 0) << "," << (res.reached_goal ? 1 : 0)
             << "," << pe << "," << att.semantic_weight << "," << att.visual_weight
             << "," << reg.attention_alpha << "," << reg.attention_lambda << ","
             << reg.learning_rate_multiplier << "\n";
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

