#pragma once

#include "actuation/ReplayFrame.h"
#include "memory/EpisodicMemoryManager.h"

#include <cmath>
#include <functional>
#include <random>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Memory {

/**
 * @brief Phase 21: Bridge between ReplayFrames and EpisodicMemory
 *
 * Converts action+observation pairs into vector-only episodic patterns.
 * No narrative strings are generated or stored — the pattern IS the memory.
 */
class EpisodeAdapter {
public:
  explicit EpisodeAdapter(EpisodicMemoryManager &memory) : memory_(memory) {}

  /**
   * @brief Archive a replay frame as a vector pattern
   *
   * Encodes the frame's observation into a sensory vector, hashes the
   * context string into a deterministic context vector, and derives an
   * emotional vector from the success/failure signal. No strings stored.
   */
  void archiveReplayFrame(const Actuation::ReplayFrame &frame) {
    std::vector<float> sensory = extractFeatures(frame.observation);

    // Context = deterministic vector from frame metadata (no string stored)
    std::string ctx_seed = "verification_cycle_" +
                           std::to_string(frame.action.originating_frame_id);
    std::vector<float> context = hashToVector(ctx_seed, 64);

    // Emotional signal derived from success/failure (vector, not string)
    std::vector<float> emotional(16, 0.0f);
    emotional[0] = frame.success ? 1.0f : -0.5f;
    emotional[1] = frame.fact_confirmed ? 1.0f : 0.0f;
    emotional[2] = frame.observation.confidence;

    float salience =
        frame.success ? (frame.fact_confirmed ? 0.9f : 0.6f) : 0.3f;

    memory_.encodePattern(sensory, context, emotional, salience);
  }

  /**
   * @brief Archive multiple replay frames as vector patterns
   */
  std::size_t
  archiveSession(const std::vector<Actuation::ReplayFrame> &frames) {
    std::size_t count = 0;
    for (const auto &frame : frames) {
      if (frame.success) {
        archiveReplayFrame(frame);
        ++count;
      }
    }
    return count;
  }

private:
  /**
   * @brief Extract feature vector from observation (already vector-based)
   */
  std::vector<float> extractFeatures(const Actuation::Observation &obs) {
    std::vector<float> features;
    features.push_back(obs.confidence);
    features.push_back(static_cast<float>(obs.modality));
    features.push_back(static_cast<float>(obs.payload.length()) / 1000.0f);
    return features;
  }

  /**
   * @brief Hash a string seed into a deterministic unit vector
   *
   * Used to convert context labels into vector space without storing
   * the original string. Same seed always produces the same vector.
   */
  std::vector<float> hashToVector(const std::string &seed,
                                  std::size_t dim) const {
    std::vector<float> vec(dim, 0.0f);
    if (seed.empty())
      return vec;
    std::hash<std::string> hasher;
    std::mt19937 rng(static_cast<unsigned>(hasher(seed)));
    std::normal_distribution<float> dist(0.0f, 1.0f);
    for (auto &v : vec)
      v = dist(rng);
    // Normalize to unit vector
    float norm = 0;
    for (auto v : vec)
      norm += v * v;
    norm = std::sqrt(norm);
    if (norm > 1e-8f)
      for (auto &v : vec)
        v /= norm;
    return vec;
  }

  EpisodicMemoryManager &memory_;
};

} // namespace Memory
} // namespace NeuroForge
