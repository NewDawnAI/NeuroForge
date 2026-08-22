#include "runtime/N8HumanGatedRunner.h"

#include "sandbox/GridWorld.h"
#include "sandbox/LinearActionModel.h"
#include "sandbox/LinearLatentDecoder.h"

#include "core/MemoryDB.h"
#include "perception/world/WorldModelCortex.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Runtime {

static std::int64_t unix_ms_now() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
      .count();
}

int runN8HumanGated(
    NeuroForge::Perception::WorldModelCortex &world_model_cortex,
    NeuroForge::Core::MemoryDB *memdb, std::uint64_t memdb_run_id,
    bool log_json, const N8HumanGatedSinks &sinks,
    const N8HumanGatedConfig &cfg) {
  // Use N7 boosted params for learning
  NeuroForge::Sandbox::GridWorld env(7, 7, cfg.seed);
  const std::size_t obs_dim = env.observe().size();
  const std::size_t act_dim = cfg.action_dim;

  NeuroForge::Sandbox::LinearActionModel obs_model(obs_dim, act_dim);
  NeuroForge::Sandbox::LinearLatentDecoder decoder(
      world_model_cortex.getCurrentState().latent.size(), 3);

  std::ofstream log_csv(cfg.log_csv_path);
  if (log_csv.is_open()) {
    log_csv << "step,episode,mode,action,result\n";
  }

  std::uint32_t episode = 0;
  std::uint32_t seed = cfg.seed;
  std::uint32_t collision_budget = cfg.collision_budget;
  int episode_steps = 0;

  std::uint64_t total_steps = 0;
  std::uint64_t total_episodes = 0;
  std::uint64_t reached_count = 0;
  std::uint64_t collision_count = 0;
  std::uint64_t skipped_episodes = 0;

  auto action_allowed = [&](NeuroForge::Sandbox::GridAction a) {
    int x = env.agentX();
    int y = env.agentY();
    if (a == NeuroForge::Sandbox::GridAction::Left)
      return x > 0;
    if (a == NeuroForge::Sandbox::GridAction::Right)
      return x < env.width() - 1;
    if (a == NeuroForge::Sandbox::GridAction::Up)
      return y > 0;
    if (a == NeuroForge::Sandbox::GridAction::Down)
      return y < env.height() - 1;
    return true;
  };

  for (int step = 0; step < cfg.max_steps; ++step) {
    // START OF EPISODE INTERACTIVE CHECK
    if (episode_steps == 0) {
      std::cout << "\n[N8 Human Gate] Episode " << episode << " Ready.\n";
      std::cout << "Agent: (" << env.agentX() << "," << env.agentY() << ")  ";
      std::cout << "Goal: (" << env.goalX() << "," << env.goalY() << ")\n";
      std::cout << "Estimated Distance: " << env.manhattanToGoal() << "\n";
      std::cout << "Authorize goal pursuit? (y/n) > " << std::flush;

      char c;
      std::cin >> c;
      if (c == 'n' || c == 'N') {
        std::cout << "Goal execution DENIED by human. Skipping episode.\n";
        skipped_episodes++;
        episode++;
        seed += 17;
        env.reset(seed);
        continue; // Skip this episode loop
      }
      std::cout << "Goal execution AUTHORIZED. Proceeding...\n";
    }

    // --- STANDARD N7 EXECUTION LOGIC FOR AUTHORIZED EPISODE ---
    std::vector<float> obs = env.observe();
    (void)world_model_cortex.observeCycle(obs, {}, {}, {}, {}, {}, {}, {});
    const auto &cur_state = world_model_cortex.getCurrentState();
    auto reg = world_model_cortex.getRegulationState();

    // Learning (Copying N7 logic)
    if (world_model_cortex.hasLastObserved() &&
        !world_model_cortex.getLastObservedAction().empty()) {
      float e = world_model_cortex.getLastPredictionError();
      float e_norm = std::clamp(std::tanh(e), 0.0f, 1.0f);
      float col = (obs.size() > 6) ? obs[6] : 0.0f;
      if (col < 0.5f && e_norm > 0.01f && e_norm < 0.95f) {
        float wm_lr = 0.01f * reg.learning_rate_multiplier *
                      std::clamp(e_norm, 0.1f, 1.0f);
        world_model_cortex.getPredictor().updateModel(
            world_model_cortex.getLastObservedPrediction(),
            world_model_cortex.getCurrentState(),
            world_model_cortex.getLastObservedAction(), wm_lr);
      }
    }

    // Decision Logic: Default to ObsModel (Safe Policy) as per plan
    NeuroForge::Sandbox::GridAction obs_model_action =
        NeuroForge::Sandbox::GridAction::Stay;
    float best_score = -1e9f;

    // Simple greedy lookahead (Safe Policy)
    for (NeuroForge::Sandbox::GridAction a :
         {NeuroForge::Sandbox::GridAction::Up,
          NeuroForge::Sandbox::GridAction::Down,
          NeuroForge::Sandbox::GridAction::Left,
          NeuroForge::Sandbox::GridAction::Right,
          NeuroForge::Sandbox::GridAction::Stay}) {
      if (!action_allowed(a))
        continue;
      NeuroForge::Sandbox::GridWorld sim = env;
      auto sim_res = sim.step(a);
      int dist_after = sim.manhattanToGoal();
      float score = -static_cast<float>(dist_after);
      if (sim_res.reached_goal)
        score += 100.0f;
      if (sim_res.collision)
        score -= 100.0f;

      if (score > best_score) {
        best_score = score;
        obs_model_action = a;
      }
    }

    NeuroForge::Sandbox::GridAction chosen_action = obs_model_action;

    // Execute
    std::vector<float> action_onehot(act_dim, 0.0f);
    action_onehot[static_cast<std::size_t>(chosen_action)] = 1.0f;
    world_model_cortex.predictNext(action_onehot);

    auto res = env.step(chosen_action);
    if (res.collision && collision_budget > 0)
      collision_budget--;

    // Train ObsModel/Decoder (N7 logic)
    std::vector<float> next_obs = env.observe();
    std::vector<float> obs_model_pred_next =
        obs_model.predict(obs, static_cast<std::size_t>(chosen_action));
    float obs_model_pred_error =
        obs_model.computeErrorNorm(obs_model_pred_next, next_obs);

    {
      float lr = 0.05f * reg.learning_rate_multiplier;
      std::vector<float> target(3, 0.0f);
      if (next_obs.size() > 6) {
        target[0] = next_obs[4];
        target[1] = next_obs[5];
        target[2] = next_obs[6];
      }
      NeuroForge::Perception::WorldState pred_state =
          world_model_cortex.getPredictor().predict(cur_state, action_onehot);
      decoder.update(pred_state.latent, target, lr);
    }

    float obs_model_pred_error_norm =
        std::clamp(std::tanh(obs_model_pred_error), 0.0f, 1.0f);
    if (!res.collision && obs_model_pred_error_norm > 0.01f &&
        obs_model_pred_error_norm < 0.95f) {
      float obs_model_lr = 0.01f * reg.learning_rate_multiplier *
                           std::clamp(obs_model_pred_error_norm, 0.1f, 1.0f);
      obs_model.update(obs, static_cast<std::size_t>(chosen_action), next_obs,
                       obs_model_lr);
    }

    total_steps++;
    if (res.collision)
      collision_count++;

    if (log_csv.is_open()) {
      log_csv << step << "," << episode << ",n8_human,"
              << NeuroForge::Sandbox::GridWorld::actionToString(chosen_action)
              << ","
              << (res.reached_goal ? "reached"
                                   : (res.collision ? "collision" : "step"))
              << "\n";
    }

    episode_steps++;
    if (res.reached_goal || episode_steps >= cfg.max_episode_steps) {
      if (res.reached_goal)
        reached_count++;
      total_episodes++;
      episode++;
      seed += 17;
      env.reset(seed);
      episode_steps = 0;
    }

    if (collision_budget == 0) {
      std::cout << "[N8] Collision budget exhausted. Stopping.\n";
      break;
    }
  }

  std::ofstream summary(cfg.summary_csv_path);
  if (summary.is_open()) {
    summary << "total_steps,total_episodes,skipped_episodes,reached_count,"
               "collisions\n";
    summary << total_steps << "," << total_episodes << "," << skipped_episodes
            << "," << reached_count << "," << collision_count << "\n";
  }

  std::cout << "[N8 Human Gated] steps=" << total_steps
            << " episodes=" << total_episodes << " skipped=" << skipped_episodes
            << " successes=" << reached_count
            << " collisions=" << collision_count << std::endl;

  return 0;
}

} // namespace Runtime
} // namespace NeuroForge
