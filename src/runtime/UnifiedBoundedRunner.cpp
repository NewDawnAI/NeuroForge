#include "runtime/UnifiedBoundedRunner.h"

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

int runUnifiedBounded(
    NeuroForge::Perception::WorldModelCortex &world_model_cortex,
    NeuroForge::Core::MemoryDB *memdb, std::uint64_t memdb_run_id,
    bool log_json, const UnifiedBoundedSinks &sinks,
    const UnifiedBoundedConfig &cfg) {
  NeuroForge::Sandbox::GridWorld env(7, 7, cfg.seed);
  const std::size_t obs_dim = env.observe().size();
  const std::size_t act_dim = cfg.action_dim;

  NeuroForge::Sandbox::LinearActionModel obs_model(obs_dim, act_dim);
  NeuroForge::Sandbox::LinearLatentDecoder decoder(
      world_model_cortex.getCurrentState().latent.size(), 3);

  std::ofstream log_csv(cfg.log_csv_path);
  if (log_csv.is_open() && log_csv.tellp() == 0) {
    log_csv << "step,episode,mode,action,score,dist_before,dist_after,"
               "pred_dist_after,pred_collision,collision,reached,"
               "model_pred_error,model_learn_applied,model_lr,model_delta_norm,"
               "wm_prediction_error,wm_visual_weight,wm_attention_alpha,wm_lr_"
               "multiplier,"
               "wm_suggest_action,wm_suggest_score,wm_suggest_pred_dist,wm_"
               "suggest_pred_collision,wm_action_match,"
               "wm_learn_applied,wm_lr,wm_delta_norm,driver,shadow_match_rate,"
               "wm_drove,"
               "gate_window_ready,gate_match_ok,gate_col_ok,gate_dist_ok,gate_"
               "allow_wm,gate_block_reason,"
               "gate_advantage,gate_adv_ok,gate_pred_dist_advantage,gate_dist_"
               "vs_obs_ok\n";
  }

  std::uint32_t episode = 0;
  std::uint32_t seed = cfg.seed;
  std::uint32_t collision_budget = cfg.collision_budget;
  int episode_steps = 0;

  std::uint64_t total_steps = 0;
  std::uint64_t total_episodes = 0;
  std::uint64_t reached_count = 0;
  std::uint64_t collision_count = 0;
  std::uint64_t obs_model_learn_steps = 0;
  std::uint64_t wm_learn_steps = 0;
  std::uint64_t wm_action_match_count = 0;
  std::uint64_t wm_drive_steps = 0;
  double sum_steps_to_goal = 0.0;
  double sum_obs_model_pred_error = 0.0;
  double sum_wm_pred_error = 0.0;
  double sum_lr_multiplier = 0.0;
  double sum_wm_delta_norm = 0.0;

  std::deque<int> shadow_match_hist;

  auto action_allowed = [&](NeuroForge::Sandbox::GridAction a) {
    int x = env.agentX();
    int y = env.agentY();
    if (a == NeuroForge::Sandbox::GridAction::Left) {
      return x > 0;
    }
    if (a == NeuroForge::Sandbox::GridAction::Right) {
      return x < env.width() - 1;
    }
    if (a == NeuroForge::Sandbox::GridAction::Up) {
      return y > 0;
    }
    if (a == NeuroForge::Sandbox::GridAction::Down) {
      return y < env.height() - 1;
    }
    return true;
  };

  auto score_from_obs_pred = [&](const std::vector<float> &pred_next) {
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
    (void)world_model_cortex.observeCycle(obs, {}, {}, {}, {}, {}, {}, {});
    int dist_before = env.manhattanToGoal();
    auto reg = world_model_cortex.getRegulationState();
    const auto &cur_state = world_model_cortex.getCurrentState();

    bool wm_learn_applied = false;
    float wm_lr = 0.0f;
    float wm_delta_norm = 0.0f;
    if (world_model_cortex.hasLastObserved() &&
        !world_model_cortex.getLastObservedAction().empty()) {
      float e = world_model_cortex.getLastPredictionError();
      float e_norm = std::clamp(std::tanh(e), 0.0f, 1.0f);
      float col = (obs.size() > 6) ? obs[6] : 0.0f;
      const float eps_min = 0.01f;
      const float eps_max = 0.95f;
      if (col < 0.5f && e_norm > eps_min && e_norm < eps_max) {
        wm_lr = 0.01f * reg.learning_rate_multiplier *
                std::clamp(e_norm, 0.1f, 1.0f);
        wm_delta_norm = world_model_cortex.getPredictor().updateModel(
            world_model_cortex.getLastObservedPrediction(),
            world_model_cortex.getCurrentState(),
            world_model_cortex.getLastObservedAction(), wm_lr);
        wm_learn_applied = true;
      }
    }

    NeuroForge::Sandbox::GridAction obs_model_action =
        NeuroForge::Sandbox::GridAction::Stay;
    float obs_model_score = -1e9f;
    float obs_model_pred_dist_after = static_cast<float>(dist_before);
    float obs_model_pred_collision = 0.0f;
    std::string mode = (step < cfg.warmup_steps) ? "warmup" : "obs_model";

    NeuroForge::Sandbox::GridAction wm_suggest_action =
        NeuroForge::Sandbox::GridAction::Stay;
    float wm_suggest_score = -1e9f;
    float wm_suggest_pred_dist = 0.0f;
    float wm_suggest_pred_collision = 0.0f;

    std::string driver = "warmup";
    float shadow_match_rate = 0.0f;
    int wm_drove = 0;
    int gate_window_ready = 0;
    int gate_match_ok = 0;
    int gate_col_ok = 0;
    int gate_dist_ok = 0;
    int gate_allow_wm = 0;
    std::string gate_block_reason = "warmup";
    float gate_advantage = 0.0f;
    int gate_adv_ok = 0;
    float gate_pred_dist_advantage = 0.0f;
    int gate_dist_vs_obs_ok = 0;

    NeuroForge::Sandbox::GridAction chosen_action =
        NeuroForge::Sandbox::GridAction::Stay;
    float best_score = -1e9f;
    float best_pred_dist_after = static_cast<float>(dist_before);
    float best_pred_collision = 0.0f;

    if (step < cfg.warmup_steps) {
      for (NeuroForge::Sandbox::GridAction a :
           {NeuroForge::Sandbox::GridAction::Up,
            NeuroForge::Sandbox::GridAction::Down,
            NeuroForge::Sandbox::GridAction::Left,
            NeuroForge::Sandbox::GridAction::Right,
            NeuroForge::Sandbox::GridAction::Stay}) {
        if (!action_allowed(a)) {
          continue;
        }
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
          obs_model_action = a;
          best_pred_dist_after = static_cast<float>(dist_after);
          best_pred_collision = sim_res.collision ? 1.0f : 0.0f;
        }
      }
      chosen_action = obs_model_action;
    } else {
      for (std::size_t ai = 0; ai < act_dim; ++ai) {
        auto a_enum = static_cast<NeuroForge::Sandbox::GridAction>(ai);
        if (!action_allowed(a_enum)) {
          continue;
        }
        std::vector<float> a_onehot(act_dim, 0.0f);
        a_onehot[ai] = 1.0f;
        NeuroForge::Perception::WorldState pred_state =
            world_model_cortex.getPredictor().predict(cur_state, a_onehot);
        std::vector<float> out = decoder.predict(pred_state.latent);
        float dx = out.size() > 0 ? out[0] : 0.0f;
        float dy = out.size() > 1 ? out[1] : 0.0f;
        float pred_dist = std::fabs(dx) + std::fabs(dy);
        float pred_col = out.size() > 2 ? out[2] : 0.0f;
        pred_col = std::clamp(pred_col, 0.0f, 1.0f);
        float s = -(pred_dist * 50.0f) - (pred_col * 100.0f);
        if (pred_dist < 0.01f) {
          s += 1000.0f;
        }
        if (s > wm_suggest_score) {
          wm_suggest_score = s;
          wm_suggest_action = a_enum;
          wm_suggest_pred_dist = pred_dist;
          wm_suggest_pred_collision = pred_col;
        }
      }

      for (std::size_t ai = 0; ai < act_dim; ++ai) {
        auto a_enum = static_cast<NeuroForge::Sandbox::GridAction>(ai);
        if (!action_allowed(a_enum)) {
          continue;
        }
        std::vector<float> pred_next = obs_model.predict(obs, ai);
        auto [s, pred_dist, pred_col] = score_from_obs_pred(pred_next);
        if (s > obs_model_score) {
          obs_model_score = s;
          obs_model_action = a_enum;
          obs_model_pred_dist_after = pred_dist;
          obs_model_pred_collision = pred_col;
        }
      }

      int shadow_match = (obs_model_action == wm_suggest_action) ? 1 : 0;
      shadow_match_hist.push_back(shadow_match);
      if (cfg.stage_d3_match_window > 0 &&
          static_cast<int>(shadow_match_hist.size()) >
              cfg.stage_d3_match_window) {
        shadow_match_hist.pop_front();
      }
      if (cfg.stage_d3_match_window > 0 &&
          static_cast<int>(shadow_match_hist.size()) >=
              cfg.stage_d3_match_window) {
        gate_window_ready = 1;
        int s = 0;
        for (int v : shadow_match_hist) {
          s += v;
        }
        shadow_match_rate = static_cast<float>(s) /
                            static_cast<float>(shadow_match_hist.size());
      }

      bool allow_wm = false;
      if (!cfg.stage_d3_live) {
        gate_block_reason = "stage_off";
      } else if (cfg.stage_d3_live_mode == "match") {
        if (!gate_window_ready) {
          gate_block_reason = "window";
        } else {
          bool match_ok = static_cast<double>(shadow_match_rate) >=
                          cfg.stage_d3_min_match_rate;
          bool col_ok = static_cast<double>(wm_suggest_pred_collision) <=
                        cfg.stage_d3_max_pred_collision;
          double cur_dist = 0.0;
          if (obs.size() > 5) {
            cur_dist =
                static_cast<double>(std::fabs(obs[4]) + std::fabs(obs[5]));
          }
          bool dist_ok =
              (cur_dist - static_cast<double>(wm_suggest_pred_dist)) >=
              cfg.stage_d3_min_dist_improve;
          gate_match_ok = match_ok ? 1 : 0;
          gate_col_ok = col_ok ? 1 : 0;
          gate_dist_ok = dist_ok ? 1 : 0;
          allow_wm = match_ok && col_ok && dist_ok;
          if (!allow_wm) {
            if (!match_ok) {
              gate_block_reason = "match";
            } else if (!col_ok) {
              gate_block_reason = "collision";
            } else if (!dist_ok) {
              gate_block_reason = "dist";
            } else {
              gate_block_reason = "unknown";
            }
          }
        }
      } else if (cfg.stage_d3_live_mode == "advantage") {
        bool col_ok = static_cast<double>(wm_suggest_pred_collision) <=
                      cfg.stage_d3_max_pred_collision;
        double cur_dist = 0.0;
        if (obs.size() > 5) {
          cur_dist = static_cast<double>(std::fabs(obs[4]) + std::fabs(obs[5]));
        }
        bool dist_ok = (cur_dist - static_cast<double>(wm_suggest_pred_dist)) >=
                       cfg.stage_d3_min_dist_improve;
        gate_col_ok = col_ok ? 1 : 0;
        gate_dist_ok = dist_ok ? 1 : 0;
        gate_advantage = wm_suggest_score - obs_model_score;
        gate_adv_ok =
            (static_cast<double>(gate_advantage) >= cfg.stage_d3_min_advantage)
                ? 1
                : 0;
        gate_pred_dist_advantage =
            obs_model_pred_dist_after - wm_suggest_pred_dist;
        gate_dist_vs_obs_ok = (static_cast<double>(gate_pred_dist_advantage) >=
                               cfg.stage_d3_min_pred_dist_advantage)
                                  ? 1
                                  : 0;
        allow_wm = (gate_adv_ok == 1) && col_ok && dist_ok &&
                   (gate_dist_vs_obs_ok == 1);
        if (!allow_wm) {
          if (!gate_adv_ok) {
            gate_block_reason = "advantage";
          } else if (!col_ok) {
            gate_block_reason = "collision";
          } else if (!dist_ok) {
            gate_block_reason = "dist";
          } else if (!gate_dist_vs_obs_ok) {
            gate_block_reason = "dist_vs_obs";
          } else {
            gate_block_reason = "unknown";
          }
        }
      } else {
        gate_block_reason = "mode";
      }
      gate_allow_wm = allow_wm ? 1 : 0;

      if (allow_wm) {
        chosen_action = wm_suggest_action;
        best_score = wm_suggest_score;
        best_pred_dist_after = wm_suggest_pred_dist;
        best_pred_collision = wm_suggest_pred_collision;
        driver = "world_model";
        wm_drove = 1;
        gate_block_reason = "allow";
      } else {
        chosen_action = obs_model_action;
        best_score = obs_model_score;
        best_pred_dist_after = obs_model_pred_dist_after;
        best_pred_collision = obs_model_pred_collision;
        driver = "obs_model";
      }
    }

    std::vector<float> action_onehot(act_dim, 0.0f);
    action_onehot[static_cast<std::size_t>(chosen_action)] = 1.0f;
    world_model_cortex.predictNext(action_onehot);

    auto res = env.step(chosen_action);
    if (res.collision && collision_budget > 0) {
      collision_budget--;
    }

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
      (void)decoder.update(pred_state.latent, target, lr);
    }

    bool obs_model_learn_applied = false;
    float obs_model_lr = 0.0f;
    float obs_model_delta_norm = 0.0f;
    float obs_model_pred_error_norm =
        std::clamp(std::tanh(obs_model_pred_error), 0.0f, 1.0f);
    const float eps_min = 0.01f;
    const float eps_max = 0.95f;
    if (!res.collision && obs_model_pred_error_norm > eps_min &&
        obs_model_pred_error_norm < eps_max) {
      obs_model_lr = 0.01f * reg.learning_rate_multiplier *
                     std::clamp(obs_model_pred_error_norm, 0.1f, 1.0f);
      obs_model_delta_norm = obs_model.update(
          obs, static_cast<std::size_t>(chosen_action), next_obs, obs_model_lr);
      obs_model_learn_applied = true;
    }

    int dist_after = env.manhattanToGoal();
    float wm_pe = world_model_cortex.getLastPredictionError();
    auto att = world_model_cortex.getAttentionState();

    total_steps++;
    sum_obs_model_pred_error += static_cast<double>(obs_model_pred_error);
    sum_wm_pred_error += static_cast<double>(wm_pe);
    sum_lr_multiplier += static_cast<double>(reg.learning_rate_multiplier);
    if (wm_learn_applied) {
      wm_learn_steps++;
      sum_wm_delta_norm += static_cast<double>(wm_delta_norm);
    }
    if (res.collision) {
      collision_count++;
    }
    if (obs_model_learn_applied) {
      obs_model_learn_steps++;
    }
    if (res.reached_goal) {
      reached_count++;
      sum_steps_to_goal += static_cast<double>(episode_steps + 1);
    }

    int wm_action_match = 0;
    if (step >= cfg.warmup_steps && obs_model_action == wm_suggest_action) {
      wm_action_match = 1;
      wm_action_match_count++;
    }
    if (wm_drove) {
      wm_drive_steps++;
    }

    if (log_csv.is_open()) {
      log_csv << step << "," << episode << "," << mode << ","
              << NeuroForge::Sandbox::GridWorld::actionToString(chosen_action)
              << "," << best_score << "," << dist_before << "," << dist_after
              << "," << best_pred_dist_after << "," << best_pred_collision
              << "," << (res.collision ? 1 : 0) << ","
              << (res.reached_goal ? 1 : 0) << "," << obs_model_pred_error
              << "," << (obs_model_learn_applied ? 1 : 0) << "," << obs_model_lr
              << "," << obs_model_delta_norm << "," << wm_pe << ","
              << att.visual_weight << "," << reg.attention_alpha << ","
              << reg.learning_rate_multiplier << ","
              << NeuroForge::Sandbox::GridWorld::actionToString(
                     wm_suggest_action)
              << "," << wm_suggest_score << "," << wm_suggest_pred_dist << ","
              << wm_suggest_pred_collision << "," << wm_action_match << ","
              << (wm_learn_applied ? 1 : 0) << "," << wm_lr << ","
              << wm_delta_norm << "," << driver << "," << shadow_match_rate
              << "," << wm_drove << "," << gate_window_ready << ","
              << gate_match_ok << "," << gate_col_ok << "," << gate_dist_ok
              << "," << gate_allow_wm << "," << gate_block_reason << ","
              << gate_advantage << "," << gate_adv_ok << ","
              << gate_pred_dist_advantage << "," << gate_dist_vs_obs_ok << "\n";
      if (step % 10 == 0) {
        log_csv.flush();
      }
    }

    if (log_json && sinks.emit_json_line) {
      std::ostringstream js;
      js << "{\"version\":1,\"phase\":\"N7\",\"event\":\"unified_bounded_"
            "step\","
            "\"step\":"
         << step << ",\"episode\":" << episode << "}";
      sinks.emit_json_line(js.str());
    }

    if (memdb && memdb_run_id > 0 && sinks.insert_experience) {
      std::ostringstream p;
      p << "{\"episode\":" << episode << ",\"mode\":\"" << mode
        << "\",\"action\":\""
        << NeuroForge::Sandbox::GridWorld::actionToString(chosen_action)
        << "\",\"driver\":\"" << driver << "\"}";
      sinks.insert_experience(unix_ms_now(), static_cast<std::uint64_t>(step),
                              "n7_unified_bounded", p.str());
    }

    episode_steps++;
    if (res.reached_goal || episode_steps >= cfg.max_episode_steps) {
      total_episodes++;
      episode++;
      seed += 17;
      env.reset(seed);
      episode_steps = 0;
    }

    if (collision_budget == 0) {
      break;
    }
  }

  {
    std::ofstream summary(cfg.summary_csv_path);
    if (summary.is_open()) {
      summary << "total_steps,total_episodes,successes,success_rate,"
                 "avg_steps_to_goal,collisions,collision_rate,"
                 "mean_model_pred_error,mean_wm_pred_error,"
                 "mean_lr_multiplier,model_learn_step_rate,"
                 "wm_learn_step_rate,mean_wm_delta_norm,wm_action_match_rate,"
                 "wm_drive_step_rate\n";
      double success_rate = (total_episodes > 0)
                                ? (static_cast<double>(reached_count) /
                                   static_cast<double>(total_episodes))
                                : 0.0;
      double avg_steps_to_goal =
          (reached_count > 0)
              ? (sum_steps_to_goal / static_cast<double>(reached_count))
              : 0.0;
      double collision_rate = (total_steps > 0)
                                  ? (static_cast<double>(collision_count) /
                                     static_cast<double>(total_steps))
                                  : 0.0;
      double mean_model_pe =
          (total_steps > 0) ? (sum_obs_model_pred_error / total_steps) : 0.0;
      double mean_wm_pe =
          (total_steps > 0) ? (sum_wm_pred_error / total_steps) : 0.0;
      double mean_lr_mult =
          (total_steps > 0) ? (sum_lr_multiplier / total_steps) : 0.0;
      double learn_rate = (total_steps > 0)
                              ? (static_cast<double>(obs_model_learn_steps) /
                                 static_cast<double>(total_steps))
                              : 0.0;
      double wm_learn_rate = (total_steps > 0)
                                 ? (static_cast<double>(wm_learn_steps) /
                                    static_cast<double>(total_steps))
                                 : 0.0;
      double mean_wm_delta =
          (wm_learn_steps > 0)
              ? (sum_wm_delta_norm / static_cast<double>(wm_learn_steps))
              : 0.0;
      double wm_match_rate =
          (total_steps > static_cast<std::uint64_t>(cfg.warmup_steps))
              ? (static_cast<double>(wm_action_match_count) /
                 static_cast<double>(total_steps - static_cast<std::uint64_t>(
                                                       cfg.warmup_steps)))
              : 0.0;
      double wm_drive_rate =
          (total_steps > static_cast<std::uint64_t>(cfg.warmup_steps))
              ? (static_cast<double>(wm_drive_steps) /
                 static_cast<double>(total_steps - static_cast<std::uint64_t>(
                                                       cfg.warmup_steps)))
              : 0.0;

      summary << total_steps << "," << total_episodes << "," << reached_count
              << "," << success_rate << "," << avg_steps_to_goal << ","
              << collision_count << "," << collision_rate << ","
              << mean_model_pe << "," << mean_wm_pe << "," << mean_lr_mult
              << "," << learn_rate << "," << wm_learn_rate << ","
              << mean_wm_delta << "," << wm_match_rate << "," << wm_drive_rate
              << "\n";
    }
  }

  std::cout << "[UnifiedBounded] steps=" << total_steps
            << " episodes=" << total_episodes << " successes=" << reached_count
            << " collisions=" << collision_count << std::endl;
  return 0;
}

} // namespace Runtime
} // namespace NeuroForge
