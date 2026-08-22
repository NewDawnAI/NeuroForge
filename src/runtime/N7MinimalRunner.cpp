#include "runtime/N7MinimalRunner.h"

#include "perception/world/WorldModelCortex.h"
#include "sandbox/GridWorld.h"

#include <cstdint>
#include <fstream>
#include <vector>

namespace NeuroForge {
namespace Runtime {

int runN7Minimal(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                 const N7MinimalConfig &cfg) {
  NeuroForge::Sandbox::GridWorld env(7, 7, cfg.seed);
  std::ofstream n7_log(cfg.log_csv_path);
  if (n7_log.is_open() && n7_log.tellp() == 0) {
    n7_log << "step,episode,action,ax,ay,gx,gy,dist,collision,reached,"
              "prediction_error,semantic_weight,visual_weight,lr_multiplier\n";
  }

  std::uint32_t episode = 0;
  std::uint32_t seed = cfg.seed;
  std::uint32_t collision_budget = cfg.collision_budget;
  for (int step = 0; step < cfg.max_steps; ++step) {
    std::vector<float> obs = env.observe();
    (void)world_model_cortex.processCycle(obs, {}, {}, {}, {}, {}, {}, {});

    NeuroForge::Sandbox::GridAction act = env.greedyActionToGoal();
    auto res = env.step(act);
    if (res.collision && collision_budget > 0) {
      collision_budget--;
    }

    auto att = world_model_cortex.getAttentionState();
    auto reg = world_model_cortex.getRegulationState();
    float pe = world_model_cortex.getLastPredictionError();

    if (n7_log.is_open()) {
      n7_log << step << "," << episode << ","
             << NeuroForge::Sandbox::GridWorld::actionToString(act) << ","
             << env.agentX() << "," << env.agentY() << "," << env.goalX() << ","
             << env.goalY() << "," << env.manhattanToGoal() << ","
             << (res.collision ? 1 : 0) << "," << (res.reached_goal ? 1 : 0)
             << "," << pe << "," << att.semantic_weight << "," << att.visual_weight
             << "," << reg.learning_rate_multiplier << "\n";
      if (step % 10 == 0) {
        n7_log.flush();
      }
    }

    if (res.reached_goal) {
      episode++;
      seed += 17;
      env.reset(seed);
    }

    if (collision_budget == 0) {
      break;
    }
  }

  return 0;
}

} // namespace Runtime
} // namespace NeuroForge

