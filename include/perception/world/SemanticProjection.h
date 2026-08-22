#pragma once

/**
 * @file SemanticProjection.h
 * @brief Projects grounded concept activations into WorldState semantic channel
 *
 * JEPA-Style Language Integration:
 * - Language may READ the world model
 * - Language must NOT DEFINE the world model
 *
 * This class provides a safe, one-way bridge from concept activations
 * to the WorldState semantic channel, ensuring language remains an
 * interpreter of the predictive world rather than its master.
 *
 * @invariant No raw tokens pass through - only grounded concept activations
 * @invariant Rate-limited to prevent language from dominating perception
 * @invariant Confidence-weighted to suppress uncertain concepts
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Perception {

// Forward declarations
struct WorldState;

/**
 * @brief Represents an active concept with its grounding strength
 */
struct ActiveConcept {
  std::string concept_id;       ///< Unique concept identifier
  std::vector<float> embedding; ///< Concept's learned latent embedding
  float activation;             ///< Current activation level [0,1]
  float grounding_confidence;   ///< How well-grounded this concept is [0,1]
  float predictive_power;       ///< How predictive this concept is [0,1]
  std::uint64_t
      last_grounded_ms; ///< When concept was last grounded perceptually
};

/**
 * @brief Configuration for semantic projection
 */
struct SemanticProjectionConfig {
  /// Dimensionality of semantic channel output
  std::size_t semantic_dim = 128;

  /// Minimum activation threshold for concept inclusion
  float activation_threshold = 0.1f;

  /// Minimum grounding confidence for projection
  float grounding_threshold = 0.3f;

  /// Maximum time since grounding before concept decays (ms)
  std::uint64_t max_grounding_age_ms = 10000;

  /// Rate limit: minimum ms between projections
  std::uint64_t min_projection_interval_ms = 50;

  /// Maximum number of concepts to project per update
  std::size_t max_concepts_per_projection = 20;

  /// Weight decay for weakly grounded concepts
  float weak_grounding_decay = 0.5f;

  /// Blend factor for temporal smoothing (0=instant, 1=frozen)
  float temporal_smoothing = 0.7f;
};

/**
 * @brief Projects grounded concepts into WorldState semantic channel
 *
 * This is the ONLY approved pathway for concept information to enter
 * the world model. It enforces:
 *
 * 1. Rate-limiting (prevents linguistic flooding)
 * 2. Grounding requirements (no ungrounded concepts)
 * 3. Confidence weighting (uncertain concepts suppressed)
 * 4. Temporal smoothing (prevents linguistic jitter)
 *
 * Usage:
 * @code
 * SemanticProjection projector(config);
 * std::vector<ActiveConcept> concepts = semanticMemory.getActive();
 * WorldState& state = worldModel.getCurrentState();
 * projector.projectInto(concepts, state);
 * @endcode
 */
class SemanticProjection {
public:
  /**
   * @brief Construct with configuration
   */
  explicit SemanticProjection(const SemanticProjectionConfig &config = {});

  /**
   * @brief Project active concepts into world state semantic channel
   *
   * CRITICAL: This is a one-way operation. Concepts read from world state
   * but cannot overwrite the core latent representation.
   *
   * @param active_concepts Currently active concepts from ConceptNodes
   * @param world_state Target world state (semantic channel updated)
   * @param current_time_ms Current timestamp for rate limiting
   * @return Number of concepts projected (may be 0 if rate-limited)
   */
  std::size_t projectInto(const std::vector<ActiveConcept> &active_concepts,
                          WorldState &world_state,
                          std::uint64_t current_time_ms);

  /**
   * @brief Get projection statistics
   */
  struct Statistics {
    std::uint64_t total_projections = 0;
    std::uint64_t rate_limited_count = 0;
    std::uint64_t concepts_projected = 0;
    std::uint64_t concepts_filtered = 0;
    float average_grounding = 0.0f;
    float average_activation = 0.0f;
  };

  Statistics getStatistics() const { return stats_; }

  /**
   * @brief Reset projection state (e.g., on context switch)
   */
  void reset();

  /**
   * @brief Project concepts without world state (for testing)
   * @return Semantic vector that would be produced
   */
  std::vector<float>
  projectFromConcepts(const std::vector<ActiveConcept> &active_concepts,
                      std::uint64_t current_time_ms);

private:
  SemanticProjectionConfig config_;
  Statistics stats_;

  /// Last projection timestamp for rate limiting
  std::uint64_t last_projection_ms_ = 0;

  /// Previous semantic output for temporal smoothing
  std::vector<float> previous_semantic_;

  /**
   * @brief Filter concepts by grounding and activation thresholds
   */
  std::vector<const ActiveConcept *>
  filterConcepts(const std::vector<ActiveConcept> &concepts,
                 std::uint64_t current_time_ms) const;

  /**
   * @brief Compute weighted sum of concept embeddings
   */
  std::vector<float>
  combineEmbeddings(const std::vector<const ActiveConcept *> &filtered,
                    std::uint64_t current_time_ms) const;

  /**
   * @brief Apply temporal smoothing between frames
   */
  void applySmoothing(std::vector<float> &current) const;

  /**
   * @brief Normalize output to prevent runaway
   */
  void normalizeOutput(std::vector<float> &output) const;
};

} // namespace Perception
} // namespace NeuroForge
