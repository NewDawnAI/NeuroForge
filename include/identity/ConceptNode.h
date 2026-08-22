#pragma once

#include <cstdint>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Identity {

/**
 * @brief Phase 23: Concept Node
 *
 * Not a word - a cluster of experiences.
 * Language attaches LATER as aliases.
 */
struct ConceptNode {
  std::uint64_t id = 0;

  /// Episodes that ground this concept
  std::vector<std::uint64_t> grounded_episode_ids;

  /// How stable is this concept?
  float stability = 0.0f;

  /// How well does it predict outcomes?
  float predictive_power = 0.0f;

  /// Concept label (emergent, not hard-coded)
  std::string emergent_label;
};

/**
 * @brief Phase 23: Lexical Binding
 *
 * Words are aliases for concepts, not the concept itself.
 */
struct LexicalBinding {
  std::uint64_t concept_id = 0;
  std::string token;
  float confidence = 0.0f;
  int usage_count = 0;
};

/**
 * @brief Phase 23: Concept Grounding Registry
 *
 * Maps experiences to concepts, and concepts to language.
 * This is how NeuroForge learns language autonomously.
 */
class ConceptRegistry {
public:
  ConceptRegistry() : next_concept_id_(1) {}

  /**
   * @brief Create a new concept from an episode cluster
   */
  std::uint64_t createConcept(const std::vector<std::uint64_t> &episode_ids) {
    ConceptNode node;
    node.id = next_concept_id_++;
    node.grounded_episode_ids = episode_ids;
    node.stability = 0.1f;
    node.predictive_power = 0.0f;
    concepts_.push_back(node);
    return node.id;
  }

  /**
   * @brief Bind a word to a concept
   */
  void bindLexeme(std::uint64_t concept_id, const std::string &token,
                  float confidence) {
    LexicalBinding binding;
    binding.concept_id = concept_id;
    binding.token = token;
    binding.confidence = confidence;
    binding.usage_count = 1;
    bindings_.push_back(binding);
  }

  /**
   * @brief Get all concepts
   */
  const std::vector<ConceptNode> &getConcepts() const { return concepts_; }

  /**
   * @brief Get all lexical bindings
   */
  const std::vector<LexicalBinding> &getBindings() const { return bindings_; }

private:
  std::vector<ConceptNode> concepts_;
  std::vector<LexicalBinding> bindings_;
  std::uint64_t next_concept_id_;
};

} // namespace Identity
} // namespace NeuroForge
