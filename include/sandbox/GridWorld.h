#pragma once

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Sandbox {

enum class GridAction { Up = 0, Down = 1, Left = 2, Right = 3, Stay = 4 };

struct GridStepResult {
  bool moved{false};
  bool collision{false};
  bool reached_goal{false};
  int reward{0};
};

class GridWorld {
public:
  GridWorld(const GridWorld &) = default;
  GridWorld &operator=(const GridWorld &) = default;

  explicit GridWorld(int width = 7, int height = 7, std::uint32_t seed = 1)
      : width_(std::max(2, width)), height_(std::max(2, height)),
        rng_(seed) {
    reset(seed);
  }

  void reset(std::uint32_t seed) {
    rng_.seed(seed);
    last_collision_ = false;
    step_count_ = 0;
    std::uniform_int_distribution<int> xdist(0, width_ - 1);
    std::uniform_int_distribution<int> ydist(0, height_ - 1);
    ax_ = xdist(rng_);
    ay_ = ydist(rng_);
    do {
      gx_ = xdist(rng_);
      gy_ = ydist(rng_);
    } while (gx_ == ax_ && gy_ == ay_);
  }

  GridStepResult step(GridAction action) {
    GridStepResult res;
    int nx = ax_;
    int ny = ay_;
    switch (action) {
    case GridAction::Up:
      ny -= 1;
      break;
    case GridAction::Down:
      ny += 1;
      break;
    case GridAction::Left:
      nx -= 1;
      break;
    case GridAction::Right:
      nx += 1;
      break;
    case GridAction::Stay:
      break;
    }

    bool in_bounds = (nx >= 0 && nx < width_ && ny >= 0 && ny < height_);
    if (!in_bounds) {
      res.collision = true;
      res.moved = false;
      last_collision_ = true;
    } else {
      res.moved = (nx != ax_ || ny != ay_);
      ax_ = nx;
      ay_ = ny;
      res.collision = false;
      last_collision_ = false;
    }

    res.reached_goal = (ax_ == gx_ && ay_ == gy_);
    res.reward = res.reached_goal ? 1 : 0;
    step_count_++;
    return res;
  }

  std::vector<float> observe() const {
    float wx = (width_ > 1) ? (1.0f / static_cast<float>(width_ - 1)) : 1.0f;
    float wy =
        (height_ > 1) ? (1.0f / static_cast<float>(height_ - 1)) : 1.0f;
    float axn = static_cast<float>(ax_) * wx;
    float ayn = static_cast<float>(ay_) * wy;
    float gxn = static_cast<float>(gx_) * wx;
    float gyn = static_cast<float>(gy_) * wy;
    float dxn = static_cast<float>(gx_ - ax_) * wx;
    float dyn = static_cast<float>(gy_ - ay_) * wy;
    float col = last_collision_ ? 1.0f : 0.0f;
    return {axn, ayn, gxn, gyn, dxn, dyn, col, 1.0f};
  }

  int manhattanToGoal() const { return std::abs(gx_ - ax_) + std::abs(gy_ - ay_); }

  GridAction greedyActionToGoal() const {
    int best_d = manhattanToGoal();
    GridAction best_a = GridAction::Stay;
    for (GridAction a :
         {GridAction::Up, GridAction::Down, GridAction::Left, GridAction::Right,
          GridAction::Stay}) {
      int nx = ax_;
      int ny = ay_;
      switch (a) {
      case GridAction::Up:
        ny -= 1;
        break;
      case GridAction::Down:
        ny += 1;
        break;
      case GridAction::Left:
        nx -= 1;
        break;
      case GridAction::Right:
        nx += 1;
        break;
      case GridAction::Stay:
        break;
      }
      if (nx < 0 || nx >= width_ || ny < 0 || ny >= height_) {
        continue;
      }
      int d = std::abs(gx_ - nx) + std::abs(gy_ - ny);
      if (d < best_d) {
        best_d = d;
        best_a = a;
      }
    }
    return best_a;
  }

  static std::string actionToString(GridAction a) {
    switch (a) {
    case GridAction::Up:
      return "up";
    case GridAction::Down:
      return "down";
    case GridAction::Left:
      return "left";
    case GridAction::Right:
      return "right";
    case GridAction::Stay:
      return "stay";
    }
    return "unknown";
  }

  int width() const { return width_; }
  int height() const { return height_; }
  int agentX() const { return ax_; }
  int agentY() const { return ay_; }
  int goalX() const { return gx_; }
  int goalY() const { return gy_; }
  std::uint64_t stepCount() const { return step_count_; }

private:
  int width_{7};
  int height_{7};
  int ax_{0};
  int ay_{0};
  int gx_{0};
  int gy_{0};
  bool last_collision_{false};
  std::uint64_t step_count_{0};
  mutable std::mt19937 rng_;
};

} // namespace Sandbox
} // namespace NeuroForge
