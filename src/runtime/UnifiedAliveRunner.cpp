#include "runtime/UnifiedAliveRunner.h"

#include "sandbox/GridWorld.h"
#include "sandbox/LinearActionModel.h"
#include "sandbox/LinearLatentDecoder.h"

#include "core/MemoryDB.h"
#include "perception/world/WorldModelCortex.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace NeuroForge {
namespace Runtime {

int runUnifiedAlive(
    NeuroForge::Perception::WorldModelCortex &world_model_cortex,
    NeuroForge::Core::MemoryDB *memdb, std::uint64_t memdb_run_id,
    bool log_json, const UnifiedAliveSinks &sinks,
    const UnifiedAliveConfig &cfg, const std::atomic<bool> &abort_signal) {

  (void)sinks;
  (void)memdb_run_id;
  (void)log_json;
  (void)memdb;

  NeuroForge::Sandbox::GridWorld env(7, 7, cfg.seed);
  const std::size_t obs_dim = env.observe().size();
  const std::size_t act_dim = cfg.action_dim;

  NeuroForge::Sandbox::LinearActionModel obs_model(obs_dim, act_dim);
  NeuroForge::Sandbox::LinearLatentDecoder decoder(
      world_model_cortex.getCurrentState().latent.size(), 3);

  std::ofstream log_csv(cfg.log_csv_path);
  if (log_csv.is_open()) {
    log_csv
        << "step,episode,mode,action,result,gate_decision,risk,alive_state\n";
  }

  std::uint64_t total_steps = 0;
  std::uint64_t steps_in_episode = 0;
  std::uint64_t episode_count = 0;
  std::uint32_t seed = cfg.seed;
  std::uint32_t collision_budget = cfg.collision_budget;

  // Drifter state
  bool needs_new_goal = true;
  bool is_sleeping = false;

  // Helper: Greedy Policy (The "Instinct")
  auto get_instinct_action = [&](const NeuroForge::Sandbox::GridWorld &w) {
    return w.greedyActionToGoal();
  };

  std::cout << "[Alive] System starting. Mode: "
            << (cfg.infinite_mode ? "INFINITE" : "BOUNDED") << "\n";

  while (!abort_signal.load()) {
    if (!cfg.infinite_mode &&
        total_steps >= static_cast<std::uint64_t>(cfg.max_steps)) {
      break;
    }

    // 1. Homeostasis / Goal Management
    if (needs_new_goal) {
      episode_count++;
      seed += 13; // Diverge seeds
      env.reset(seed);
      steps_in_episode = 0;
      needs_new_goal = false;
      is_sleeping = false;
      if (total_steps > 0) { // Don't spam on startup
        std::cout << "[Alive] New Intent (Episode " << episode_count << ")\n";
      }
    }

    // 2. Perceive
    std::vector<float> obs = env.observe();
    (void)world_model_cortex.observeCycle(obs, {}, {}, {}, {}, {}, {}, {});

    // 3. Plan (generate candidate)
    auto candidate_action = get_instinct_action(env);

    // 4. Gate (Safety Check)
    bool gate_open = true;
    float risk = 0.0f;

    // Simulate outcome using World Model
    {
      NeuroForge::Sandbox::GridWorld sim_env = env;
      NeuroForge::Perception::WorldState sim_state =
          world_model_cortex.getCurrentState();
      float max_col = 0.0f;

      for (int k = 0; k < cfg.horizon_steps; ++k) {
        if (sim_env.manhattanToGoal() == 0)
          break;

        // Instinctive next step in sim
        auto sim_act = get_instinct_action(sim_env);

        // WM Predict
        std::vector<float> a_oh(act_dim, 0.0f);
        a_oh[static_cast<std::size_t>(sim_act)] = 1.0f;
        auto next_state =
            world_model_cortex.getPredictor().predict(sim_state, a_oh);

        // Decode for safety
        auto pred_obs_vec = decoder.predict(next_state.latent);
        // Correct logic:
        // auto next_state is defined above.

        float p_col = (pred_obs_vec.size() > 6)
                          ? std::clamp(pred_obs_vec[6], 0.0f, 1.0f)
                          : 0.0f;
        if (p_col > max_col)
          max_col = p_col;

        sim_env.step(sim_act);
        sim_state = next_state;
      }
      risk = max_col;
      if (risk > cfg.max_collision_prob) {
        gate_open = false;
      }
    }

    // 5. Act or Sleep
    NeuroForge::Sandbox::GridAction final_action =
        NeuroForge::Sandbox::GridAction::Stay;
    std::string state_desc = "active";

    if (gate_open) {
      final_action = candidate_action;
      is_sleeping = false;
    } else {
      // High risk -> Sleep/Reflex Halt
      final_action = NeuroForge::Sandbox::GridAction::Stay;
      state_desc = "sleeping_risk";
      is_sleeping = true;
      // In a real drifter, we might eventually "give up" and pick a new goal if
      // we sleep too long. For v1, we just wait. The environment is static, so
      // we will wait forever unless we have logic to abort episode. Let's add
      // boredom:
      if (steps_in_episode > 50) {
        needs_new_goal = true; // Give up
        state_desc = "bored_reset";
      }
    }

    // execute
    std::vector<float> action_onehot(act_dim, 0.0f);
    action_onehot[static_cast<std::size_t>(final_action)] = 1.0f;
    world_model_cortex.predictNext(
        action_onehot); // WM commits to prediction of chosen action

    auto res = env.step(final_action);
    if (res.collision) {
      if (collision_budget > 0)
        collision_budget--;
      else {
        std::cout << "[Alive] CRITICAL: Collision budget exhausted. Dying.\n";
        break;
      }
    }

    // 6. Learn (N4 constraint: only if no semantic injection etc. - assumed
    // true here) Re-use standard learning loop from N7/N9
    if (world_model_cortex.hasLastObserved() &&
        !world_model_cortex.getLastObservedAction().empty()) {
      float e = world_model_cortex.getLastPredictionError();
      float e_norm = std::clamp(std::tanh(e), 0.0f, 1.0f);
      // Learn if error is significant but not total surprise, and we didn't
      // just crash Actually N7 logic:
      if (!res.collision && e_norm > 0.01f && e_norm < 0.95f) {
        float lr = 0.01f * std::clamp(e_norm, 0.1f, 1.0f);
        world_model_cortex.getPredictor().updateModel(
            world_model_cortex.getLastObservedPrediction(),
            world_model_cortex.getCurrentState(),
            world_model_cortex.getLastObservedAction(), lr);
      }
    }

    // Train decoder/obs model (Auxiliary)
    {
      std::vector<float> next_obs = env.observe();
      // Train decoder
      // ... (standard N7 decoder training lines omitted for brevity or can
      // copy-paste from N9) Copy-pasting N9 logic for completeness:
      std::vector<float> target(3, 0.0f);
      if (next_obs.size() > 6) {
        target[0] = next_obs[4];
        target[1] = next_obs[5];
        target[2] = next_obs[6];
      }
      decoder.update(
          world_model_cortex.getPredictor()
              .predict(world_model_cortex.getCurrentState(), action_onehot)
              .latent,
          target, 0.01f);
    }

    // Log
    if (log_csv.is_open()) {
      log_csv << total_steps << "," << episode_count << ",alive,"
              << NeuroForge::Sandbox::GridWorld::actionToString(final_action)
              << ","
              << (res.reached_goal ? "reached"
                                   : (res.collision ? "collision" : "step"))
              << "," << (gate_open ? "open" : "shunted") << "," << risk << ","
              << state_desc << "\n";
    }

    // Goal Check
    if (res.reached_goal) {
      needs_new_goal = true;
    }

    total_steps++;
    steps_in_episode++;

    // Safety break for testing so we don't freeze the universe if infinite_mode
    // is false
    if (!cfg.infinite_mode && total_steps > 100000)
      break;
  }

  std::cout << "[Alive] Shutting down after " << total_steps << " steps.\n";
  return 0;
}

} // namespace Runtime
} // namespace NeuroForge
