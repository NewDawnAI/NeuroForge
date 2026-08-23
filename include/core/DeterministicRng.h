#pragma once

/**
 * @file DeterministicRng.h
 * @brief One reproducible seed source, replacing scattered std::random_device.
 *
 * WHY
 *
 * Fifteen sites across the executing code seeded generators from
 * `std::random_device`, so a run could not be reproduced from any command-line
 * seed. Measured 2026-08-23 before this change: identical invocations
 * (`--steps=150 --enable-learning --phase-c-seed=401 --sequential`) produced
 * 44,660 / 49,064 / 47,718 / 47,252 Total Updates. That ~4,400-unit spread on a
 * ~47,000 metric swamped the effects a subsystem-knockout study was trying to
 * detect, and produced a null that had to be withdrawn.
 *
 * A separate wall-clock seed in Region::growSynapses was fixed first and cut the
 * spread to ~650. This header closes the rest.
 *
 * HOW
 *
 * `seedFor("SiteName")` returns a seed derived from a process-wide base mixed
 * with a hash of the site name. Every site therefore keeps an INDEPENDENT stream
 * — nothing is forced to share a sequence, which would couple unrelated
 * subsystems — while the whole program is reproducible from one number.
 *
 * `setBaseSeed()` is called once from main() with --phase-c-seed. Passing 0
 * restores nondeterministic behaviour by seeding the base from random_device,
 * for the rare case where that is wanted deliberately.
 *
 * USAGE
 *   // was: std::mt19937 rng_{std::random_device{}()};
 *   std::mt19937 rng_{NeuroForge::Core::DeterministicRng::seedFor("LearningSystem")};
 *
 * NOTE ON GLOBAL STATE
 * The base lives in a function-local static, so there is no static-init-order
 * problem: a generator constructed during static initialisation still gets a
 * well-defined value. Set the base before constructing the brain; sites
 * constructed earlier take the default base and stay reproducible, just not
 * responsive to --phase-c-seed.
 */

#include <cstdint>
#include <random>
#include <string>

namespace NeuroForge {
namespace Core {

class DeterministicRng {
public:
  /// Default base when main() has not set one. Any fixed value works; this keeps
  /// runs reproducible out of the box rather than requiring a flag.
  static constexpr std::uint32_t kDefaultBase = 0x5EEDu;

  /**
   * @brief Set the process-wide base. Call once, early, from main().
   * @param base 0 selects a nondeterministic base (explicit opt-in only).
   */
  static void setBaseSeed(std::uint32_t base) {
    if (base == 0u) {
      std::random_device rd;
      baseRef() = rd();
      nondeterministicRef() = true;
    } else {
      baseRef() = base;
      nondeterministicRef() = false;
    }
  }

  static std::uint32_t baseSeed() { return baseRef(); }
  static bool isNondeterministic() { return nondeterministicRef(); }

  /**
   * @brief A stable, site-specific seed.
   *
   * Same base + same site name -> same seed, every run, on every machine.
   * Different sites get well-separated streams.
   */
  static std::uint32_t seedFor(const char *site) {
    std::uint32_t h = 2166136261u; // FNV-1a
    for (const char *p = site; p && *p; ++p) {
      h ^= static_cast<unsigned char>(*p);
      h *= 16777619u;
    }
    // Mix so neighbouring bases do not produce neighbouring streams.
    std::uint32_t x = h ^ (baseRef() * 2654435761u);
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x ? x : 1u; // mt19937 tolerates 0, but avoid the degenerate case
  }

  /// Convenience: a ready-made generator for a site.
  static std::mt19937 generatorFor(const char *site) {
    return std::mt19937(seedFor(site));
  }

private:
  static std::uint32_t &baseRef() {
    static std::uint32_t base = kDefaultBase;
    return base;
  }
  static bool &nondeterministicRef() {
    static bool nd = false;
    return nd;
  }
};

} // namespace Core
} // namespace NeuroForge
