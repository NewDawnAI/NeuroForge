#include "perception/world/SemanticProjection.h"
#include "perception/world/WorldState.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace NeuroForge {
namespace Perception {

SemanticProjection::SemanticProjection(const SemanticProjectionConfig &config)
    : config_(config), previous_semantic_(config.semantic_dim, 0.0f) {}

std::size_t SemanticProjection::projectInto(
    const std::vector<ActiveConcept> &active_concepts, WorldState &world_state,
    std::uint64_t current_time_ms) {
  // Rate limiting check
  if (current_time_ms - last_projection_ms_ <
      config_.min_projection_interval_ms) {
    stats_.rate_limited_count++;
    return 0;
  }

  // Filter concepts by grounding and activation
  auto filtered = filterConcepts(active_concepts, current_time_ms);

  if (filtered.empty()) {
    // No concepts to project, but still update with smoothed previous
    if (!previous_semantic_.empty()) {
      world_state.sources.semantic = previous_semantic_;
    }
    return 0;
  }

  // Combine concept embeddings weighted by activation and grounding
  std::vector<float> semantic = combineEmbeddings(filtered, current_time_ms);

  // Apply temporal smoothing
  applySmoothing(semantic);

  // Normalize to prevent runaway
  normalizeOutput(semantic);

  // Update world state semantic channel
  world_state.sources.semantic = semantic;
  previous_semantic_ = semantic;

  // Update statistics
  last_projection_ms_ = current_time_ms;
  stats_.total_projections++;
  stats_.concepts_projected += filtered.size();
  stats_.concepts_filtered += active_concepts.size() - filtered.size();

  // Compute averages
  float total_grounding = 0.0f;
  float total_activation = 0.0f;
  for (const auto *c : filtered) {
    total_grounding += c->grounding_confidence;
    total_activation += c->activation;
  }
  if (!filtered.empty()) {
    stats_.average_grounding = total_grounding / filtered.size();
    stats_.average_activation = total_activation / filtered.size();
  }

  return filtered.size();
}

std::vector<float> SemanticProjection::projectFromConcepts(
    const std::vector<ActiveConcept> &active_concepts,
    std::uint64_t current_time_ms) {
  auto filtered = filterConcepts(active_concepts, current_time_ms);
  if (filtered.empty()) {
    return std::vector<float>(config_.semantic_dim, 0.0f);
  }

  auto semantic = combineEmbeddings(filtered, current_time_ms);
  normalizeOutput(semantic);
  return semantic;
}

void SemanticProjection::reset() {
  previous_semantic_.assign(config_.semantic_dim, 0.0f);
  last_projection_ms_ = 0;
  stats_ = Statistics{};
}

std::vector<const ActiveConcept *>
SemanticProjection::filterConcepts(const std::vector<ActiveConcept> &concepts,
                                   std::uint64_t current_time_ms) const {
  std::vector<const ActiveConcept *> filtered;
  filtered.reserve(
      std::min(concepts.size(), config_.max_concepts_per_projection));

  for (const auto &node : concepts) {
    // Check activation threshold
    if (node.activation < config_.activation_threshold) {
      continue;
    }

    // Check grounding confidence threshold
    if (node.grounding_confidence < config_.grounding_threshold) {
      continue;
    }

    // Check grounding age (decay ungrounded concepts)
    std::uint64_t age = current_time_ms - node.last_grounded_ms;
    if (age > config_.max_grounding_age_ms) {
      continue;
    }

    filtered.push_back(&node);
  }

  // Sort by activation * grounding (most salient first)
  std::sort(filtered.begin(), filtered.end(),
            [](const ActiveConcept *a, const ActiveConcept *b) {
              float score_a = a->activation * a->grounding_confidence;
              float score_b = b->activation * b->grounding_confidence;
              return score_a > score_b;
            });

  // Limit to max concepts
  if (filtered.size() > config_.max_concepts_per_projection) {
    filtered.resize(config_.max_concepts_per_projection);
  }

  return filtered;
}

std::vector<float> SemanticProjection::combineEmbeddings(
    const std::vector<const ActiveConcept *> &filtered,
    std::uint64_t current_time_ms) const {
  std::vector<float> result(config_.semantic_dim, 0.0f);
  float total_weight = 0.0f;

  for (const auto *node : filtered) {
    // Compute age-based decay
    std::uint64_t age = current_time_ms - node->last_grounded_ms;
    float age_factor =
        1.0f - (static_cast<float>(age) / config_.max_grounding_age_ms);
    age_factor = std::max(0.0f, age_factor);

    // Compute weight: activation * grounding * age_factor * predictive_power
    float weight = node->activation * node->grounding_confidence * age_factor *
                   (0.5f + 0.5f * node->predictive_power);

    // Add weighted embedding
    std::size_t dim = std::min(node->embedding.size(), config_.semantic_dim);
    for (std::size_t i = 0; i < dim; ++i) {
      result[i] += node->embedding[i] * weight;
    }
    total_weight += weight;
  }

  // Normalize by total weight
  if (total_weight > 1e-8f) {
    for (float &v : result) {
      v /= total_weight;
    }
  }

  return result;
}

void SemanticProjection::applySmoothing(std::vector<float> &current) const {
  if (previous_semantic_.size() != current.size()) {
    return; // First projection, no smoothing
  }

  float alpha = config_.temporal_smoothing;
  for (std::size_t i = 0; i < current.size(); ++i) {
    current[i] = alpha * previous_semantic_[i] + (1.0f - alpha) * current[i];
  }
}

void SemanticProjection::normalizeOutput(std::vector<float> &output) const {
  float norm = 0.0f;
  for (float v : output) {
    norm += v * v;
  }
  norm = std::sqrt(norm);

  if (norm > 1e-8f) {
    for (float &v : output) {
      v /= norm;
    }
  }
}

} // namespace Perception
} // namespace NeuroForge
