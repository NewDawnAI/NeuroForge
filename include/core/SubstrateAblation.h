#pragma once

/**
 * @file SubstrateAblation.h
 * @brief Runtime ablation of region activations, for causal-contribution testing.
 *
 * WHY THIS EXISTS
 *
 * Knocking a subsystem out with a CLI flag answers "does removing this change
 * anything?" — but a null there cannot distinguish "unused" from "redundantly
 * covered", because removal changes both the signal AND its magnitude.
 *
 * The sharper instrument corrupts the CONTENT while preserving everything else.
 * SHUFFLE is the strongest form: it permutes activations across the neurons of a
 * region, so the value distribution is bit-identical and only *which neuron carries
 * what* is destroyed. If downstream behaviour changes under shuffle, something
 * depends on the region's structure, and no argument about scale or gain explains
 * it away. If nothing changes, nothing downstream reads that structure.
 *
 * This mirrors the ablation that produced the one pre-registered, confirmed
 * substrate result in the sibling RDLNN project (shuffling reservoir output cost
 * ~12% of foraging efficiency, p=0.011 over 15 fresh seeds). Knockout flags in this
 * codebase returned a well-powered null at n=15; that is a weaker claim than a
 * shuffle null would be, and this header exists to close that gap.
 *
 * MODES
 *   None      unchanged
 *   Zero      activations set to 0      — removes the signal entirely
 *   Noise     resampled from the region's own mean/sd — destroys signal, keeps scale
 *   Shuffle   permuted across neurons   — keeps the distribution EXACTLY, kills structure
 *
 * READING A ZERO/SHUFFLE PAIR
 *   zero hurts, shuffle does not   -> only presence/magnitude is read downstream
 *   shuffle hurts, zero does not   -> a fallback covers absence but not corruption
 *   both hurt                      -> structured output is load-bearing
 *   neither hurts                  -> no measured dependence; confirm the region was
 *                                     actually exercised before concluding it is unused
 *
 * DETERMINISM
 * The permutation and noise draw from a private std::mt19937 seeded explicitly, so a
 * run is reproducible and the ablation never perturbs the global RNG stream. That
 * matters here: a sibling project spent a day tracing results that moved because a
 * constructor called the global seeder.
 *
 * USAGE
 *   SubstrateAblation::instance().configure(Mode::Shuffle, "cortical", 1234);
 *   // then, at the end of a region's process():
 *   SubstrateAblation::instance().apply(regionName(), neurons_);
 */

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Core {

class SubstrateAblation {
public:
  enum class Mode { None, Zero, Noise, Shuffle };

  static SubstrateAblation &instance() {
    static SubstrateAblation inst;
    return inst;
  }

  /**
   * @param mode        what to do to the activations
   * @param region_filter substring matched against the region name; empty = all
   *                      regions. Restricting to one region localises any effect.
   * @param seed        explicit seed; the generator is private to this object
   */
  void configure(Mode mode, const std::string &region_filter = "",
                 std::uint32_t seed = 12345u) {
    mode_ = mode;
    filter_ = region_filter;
    rng_.seed(seed);
    applications_ = 0;
    neurons_touched_ = 0;
  }

  static Mode parseMode(const std::string &s) {
    if (s == "zero") return Mode::Zero;
    if (s == "noise") return Mode::Noise;
    if (s == "shuffle") return Mode::Shuffle;
    return Mode::None;
  }

  static const char *modeName(Mode m) {
    switch (m) {
    case Mode::Zero: return "zero";
    case Mode::Noise: return "noise";
    case Mode::Shuffle: return "shuffle";
    default: return "none";
    }
  }

  bool active() const noexcept { return mode_ != Mode::None; }
  std::uint64_t applications() const noexcept { return applications_; }
  std::uint64_t neuronsTouched() const noexcept { return neurons_touched_; }
  Mode mode() const noexcept { return mode_; }

  /**
   * @brief Corrupt one region's activations in place.
   *
   * Templated on the container so this header needs no dependency on Neuron or
   * Region; it only requires elements exposing getActivation()/setActivation().
   * Call at the END of a region's process step, after activations are final.
   */
  template <typename NeuronPtrContainer>
  void apply(const std::string &region_name, NeuronPtrContainer &neurons) {
    if (mode_ == Mode::None || neurons.empty()) {
      return;
    }
    if (!filter_.empty() && region_name.find(filter_) == std::string::npos) {
      return;
    }

    const std::size_t n = neurons.size();
    std::vector<float> a;
    a.reserve(n);
    for (auto &p : neurons) {
      a.push_back(p ? static_cast<float>(p->getActivation()) : 0.0f);
    }

    switch (mode_) {
    case Mode::Zero:
      std::fill(a.begin(), a.end(), 0.0f);
      break;

    case Mode::Noise: {
      // Match the region's own first two moments so only the signal is destroyed,
      // not the scale downstream consumers see.
      const float mean =
          std::accumulate(a.begin(), a.end(), 0.0f) / static_cast<float>(n);
      float var = 0.0f;
      for (float v : a) {
        var += (v - mean) * (v - mean);
      }
      var /= static_cast<float>(n);
      std::normal_distribution<float> d(mean, std::sqrt(var));
      for (float &v : a) {
        v = d(rng_);
      }
      break;
    }

    case Mode::Shuffle:
      // The value multiset is preserved EXACTLY; only the assignment changes.
      std::shuffle(a.begin(), a.end(), rng_);
      break;

    default:
      break;
    }

    for (std::size_t i = 0; i < n; ++i) {
      if (neurons[i]) {
        neurons[i]->setActivation(a[i]);
      }
    }
    ++applications_;
    neurons_touched_ += n;
  }

private:
  SubstrateAblation() = default;

  Mode mode_ = Mode::None;
  std::string filter_;
  std::mt19937 rng_{12345u};
  std::uint64_t applications_ = 0;
  std::uint64_t neurons_touched_ = 0;
};

} // namespace Core
} // namespace NeuroForge
