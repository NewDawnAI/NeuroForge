#include "runtime/N9AutomatedGatedRunner.h"

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

int runN9AutomatedGated(
    NeuroForge::Perception::WorldModelCortex &world_model_cortex,
    NeuroForge::Core::MemoryDB *memdb, std::uint64_t memdb_run_id,
    bool log_json, const N9AutomatedGatedSinks &sinks,
    const N9AutomatedGatedConfig &cfg) {

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
    log_csv << "step,episode,mode,action,result,gate_decision,max_col,max_"
               "unc\n";
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

  auto action_allowed = [&](const NeuroForge::Sandbox::GridWorld &world,
                            NeuroForge::Sandbox::GridAction a) {
    int x = world.agentX();
    int y = world.agentY();
    if (a == NeuroForge::Sandbox::GridAction::Left)
      return x > 0;
    if (a == NeuroForge::Sandbox::GridAction::Right)
      return x < world.width() - 1;
    if (a == NeuroForge::Sandbox::GridAction::Up)
      return y > 0;
    if (a == NeuroForge::Sandbox::GridAction::Down)
      return y < world.height() - 1;
    return true;
  };

  // Helper to choose safe action (greedy obs model)
  auto choose_safe_action = [&](const NeuroForge::Sandbox::GridWorld &sim_env)
      -> NeuroForge::Sandbox::GridAction {
    NeuroForge::Sandbox::GridAction chosen =
        NeuroForge::Sandbox::GridAction::Stay;
    float best_score = -1e9f;
    for (NeuroForge::Sandbox::GridAction a :
         {NeuroForge::Sandbox::GridAction::Up,
          NeuroForge::Sandbox::GridAction::Down,
          NeuroForge::Sandbox::GridAction::Left,
          NeuroForge::Sandbox::GridAction::Right,
          NeuroForge::Sandbox::GridAction::Stay}) {
      if (!action_allowed(sim_env, a))
        continue;
      NeuroForge::Sandbox::GridWorld sim_next = sim_env;
      auto sim_res = sim_next.step(a);
      int dist_after = sim_next.manhattanToGoal();
      float score = -static_cast<float>(dist_after);
      if (sim_res.reached_goal)
        score += 100.0f;
      if (sim_res.collision)
        score -= 100.0f;

      if (score > best_score) {
        best_score = score;
        chosen = a;
      }
    }
    return chosen;
  };

  for (int step = 0; step < cfg.max_steps; ++step) {
    // START OF EPISODE AUTOMATED GATE CHECK
    if (episode_steps == 0) {
      // Simulate N steps into the future using World Model
      // We simulate using the Safe Policy (ObsModel / Greedy) to see if the
      // World Model trusts it.

      NeuroForge::Sandbox::GridWorld sim_env =
          env; // Copy current state for planning
      NeuroForge::Perception::WorldState sim_state =
          world_model_cortex.getCurrentState();

      float max_predicted_collision = 0.0f;
      float max_uncertainty = 0.0f;
      bool collision_predicted = false;
      (void)max_uncertainty;
      (void)collision_predicted;

      for (int k = 0; k < cfg.horizon_steps; ++k) {
        if (sim_env.manhattanToGoal() == 0)
          break;

        // 1. Choose action via Safe Policy (Ground Truth simulation for
        // policy selection) Ideally we would predict the policy action
        // using the ObsModel neural net, but N7 logic uses analytical
        // greedy for the "Safe" path. We stick to the analytical greedy
        // here to represent "What the agent intends to do".
        auto intended_action = choose_safe_action(sim_env);

        // 2. Predict Outcome via World Model
        std::vector<float> action_onehot(act_dim, 0.0f);
        action_onehot[static_cast<std::size_t>(intended_action)] = 1.0f;

        auto next_sim_state =
            world_model_cortex.getPredictor().predict(sim_state, action_onehot);

        // Decode predicted observation to check for collision bit (index 6
        // typically) We don't have the decoder here? We instanced it above.
        // Wait, decoder maps Latent -> Obs.
        std::vector<float> predicted_obs =
            decoder.predict(next_sim_state.latent);

        float p_col = 0.0f;
        // In GridWorld obs: [x,y,gx,gy, dist_x, dist_y, collision_flag]
        // Index 6 is collision.
        if (predicted_obs.size() > 6) {
          p_col = std::clamp(predicted_obs[6], 0.0f, 1.0f);
        }

        // Uncertainty (Prediction Error of the transition? No, that
        // requires ground truth). We need intrinsic uncertainty (ensemble
        // variance or similar). The current SimplePredictor might not
        // expose uncertainty directly unless it's an ensemble. N7 uses
        // `prediction_error` (vs ground truth) for learning. For gating, we
        // usually need an uncertainty estimate. If the predictor doesn't
        // support it, we fall back to: A: If we had an ensemble, variance.
        // B: We assume uncertainty is low if we have visited this state?
        // No.
        //
        // Check WorldModelCortex.h or Predictor interface.
        // If `predict` only returns state, we might not have uncertainty.
        // Checking N8 code: `float e_norm =
        // world_model_cortex.getLastPredictionError();` - this is pos-hoc.

        // WORKAROUND: If we can't get real uncertainty, we can only gate on
        // SAFETY (collision). OR we check if the state is "familiar" (e.g.
        // Decoder reconstruction error of the latent? No). Let's assume for
        // this "Stage D3" proof of concept, we rely heavily on the
        // COLLISION prediction. If the model PREDICTS a collision, we stop.

        if (p_col > max_predicted_collision)
          max_predicted_collision = p_col;

        // Move simulation forward (Assumption: World Model is accurate
        // enough that we can chain latents? Or do we step the real env copy
        // to get the next state for policy? We must use the *simulated*
        // state for the World Model chain, but we need the *simulated* env
        // for the Policy (action selection). This creates a divergence if
        // the World Model is wrong. But we are testing the World Model's
        // confidence in its OWN dream. So we should feed the *World
        // Model's* predicted state back into... the Policy? The Policy
        // (choose_safe_action) needs a GridWorld object. We can't easily
        // reconstruct GridWorld from Latent. So we step the `sim_env`
        // (Ground Truth) to generate the query for the Policy, but we chain
        // the `sim_state` (Latent) to check for accumulation of bad states?
        // Actually, independent checking is safer. We step `sim_env` to
        // know what we *would* do. We feed that action to `sim_state` to
        // see if the World Model predicts doom.

        sim_env.step(
            intended_action);       // Ground truth update for policy generation
        sim_state = next_sim_state; // Latent chain update
      }

      bool safe = (max_predicted_collision < cfg.max_collision_prob);
      // Uncertainty check mocked as true since we lack ensemble currently
      bool confident = true;

      if (!safe || !confident) {
        if (log_csv.is_open()) {
          log_csv << step << "," << episode << ",n9_auto,SKIP,skipped,"
                  << (safe ? "UNSAFE" : "UNSAFE_COL") << ","
                  << max_predicted_collision << ",0.0\n";
        }
        std::cout << "[N9 Gate] Episode " << episode
                  << " Skipped. Risk=" << max_predicted_collision << "\n";
        skipped_episodes++;
        episode++;
        seed += 17;
        env.reset(seed);
        episode_steps = 0;
        continue;
      }
      std::cout << "[N9 Gate] Episode " << episode
                << " Authorized. Risk=" << max_predicted_collision << "\n";
    }

    // --- STANDARD EXECUTION LOGIC ---
    std::vector<float> obs = env.observe();
    (void)world_model_cortex.observeCycle(obs, {}, {}, {}, {}, {}, {}, {});
    const auto &cur_state = world_model_cortex.getCurrentState();
    auto reg = world_model_cortex.getRegulationState();

    // Learning
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

    // Decision Logic: Safe Policy
    auto chosen_action = choose_safe_action(env);

    // Execute
    std::vector<float> action_onehot(act_dim, 0.0f);
    action_onehot[static_cast<std::size_t>(chosen_action)] = 1.0f;
    world_model_cortex.predictNext(action_onehot);

    auto res = env.step(chosen_action);
    if (res.collision && collision_budget > 0)
      collision_budget--;

    // Train ObsModel/Decoder
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
      log_csv << step << "," << episode << ",n9_auto,"
              << NeuroForge::Sandbox::GridWorld::actionToString(chosen_action)
              << ","
              << (res.reached_goal ? "reached"
                                   : (res.collision ? "collision" : "step"))
              << ",authorized,0.0,0.0\n";
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
      std::cout << "[N9] Collision budget exhausted. Stopping.\n";
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

  std::cout << "[N9 Automated Gated] steps=" << total_steps
            << " episodes=" << total_episodes << " skipped=" << skipped_episodes
            << " successes=" << reached_count
            << " collisions=" << collision_count << std::endl;

  return 0;
}

} // namespace Runtime
} // namespace NeuroForge
