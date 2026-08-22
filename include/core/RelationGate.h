#pragma once

/**
 * @file RelationGate.h
 * @brief Hypergraph "Dendritic" Relation Gates for Autonomous Grounding
 *
 * Implements structured relational reasoning via coincidence-detection neurons.
 * A relation gate fires when subject, relation, and object token assemblies
 * co-activate within a time window (dendritic-like AND-gate).
 *
 * Part of the Autonomous Internet Grounding system.
 */

#include "core/HypergraphBrain.h"
#include "core/LanguageSystem.h"
#include "core/Region.h"
#include "core/Types.h"
#include <chrono>
#include <cmath>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/SourceClassifier.h" // Phase 16b: Domain classification

namespace NeuroForge {
namespace Core {

// Forward declarations
class NeuralLanguageBindings;

/**
 * @brief Represents a subject-relation-object triple with its neural gate
 */
struct RelationTriple {
  std::size_t subject_token_id;
  std::size_t relation_token_id;
  std::size_t object_token_id;

  NeuronID gate_neuron_id; ///< The coincidence-detection neuron
  RegionID gate_region_id; ///< Region containing the gate

  float confidence; ///< Current belief strength [0, 1]
  float activation; ///< Current gate activation level
  float transe_score = 0.0f;

  int verification_count;  ///< How many times verified
  int contradiction_count; ///< How many times contradicted

  std::string source_url; ///< Where this relation was learned
  std::chrono::system_clock::time_point created;
  std::chrono::system_clock::time_point last_activated;

  bool is_directed; ///< A→B ≠ B→A
  bool is_provisional = false;

  // Phase 15b: Evidence accumulation
  int support_count = 1;               ///< Independent sources seen
  std::set<std::string> source_hashes; ///< Hash of each unique source

  // Phase 16b: Domain class tracking for source independence
  std::set<DomainClass> domain_classes; ///< Domain classes seen

  // Compute verification ratio
  float verificationRatio() const {
    int total = verification_count + contradiction_count;
    return total > 0 ? static_cast<float>(verification_count) / total : 0.5f;
  }
};

/**
 * @brief Configuration for the RelationGateManager
 */
struct RelationGateConfig {
  // Gate creation
  float initial_gate_weight = 0.3f; ///< Initial synapse weight to gate
  float gate_threshold = 0.7f;      ///< Activation threshold for gate to fire
  int max_gates_per_relation = 100; ///< Limit gates per relation type
  int max_total_gates = 10000;      ///< Global gate limit

  // Co-activation window
  float coactivation_window_ms =
      50.0f; ///< Time window for coincidence detection

  // TransE scoring
  float transe_margin = 1.0f; ///< Margin for TransE loss
  int top_k_proposals = 5;    ///< Number of candidate relations to propose

  // Pruning
  float prune_threshold = 0.1f;        ///< Remove gates below this confidence
  int prune_after_contradictions = 5;  ///< Remove after N contradictions
  int min_age_before_prune_ms = 60000; ///< Don't prune fresh gates

  // Learning
  float reinforcement_rate = 0.1f; ///< How much to strengthen on verification
  float decay_rate = 0.01f;        ///< Natural confidence decay per step

  // Defaults
  bool default_directed = true; ///< Relations are directed by default
};

/**
 * @brief Manages relational knowledge via hypergraph coincidence-detection
 * gates
 *
 * Key concepts:
 * - **Relation Triple**: (subject, relation, object) e.g., (cat, is_a, mammal)
 * - **Gate Neuron**: A neuron that fires when subject+relation+object
 * co-activate
 * - **TransE Scoring**: Uses embedding geometry to propose candidate relations
 * - **Verification**: Confirms relations via repeated observation
 */
class RelationGateManager {
public:
  /**
   * @brief Construct the manager
   * @param brain The hypergraph brain (for creating neurons/synapses)
   * @param language_system The language system (for token embeddings)
   * @param config Configuration parameters
   */
  RelationGateManager(std::shared_ptr<HypergraphBrain> brain,
                      LanguageSystem *language_system,
                      const RelationGateConfig &config = RelationGateConfig{});

  ~RelationGateManager() = default;

  // ========== Gate Creation ==========

  /**
   * @brief Create a relation gate for (subject, relation, object)
   * @param subj_token_id Subject token ID from LanguageSystem
   * @param rel_token_id Relation token ID
   * @param obj_token_id Object token ID
   * @param source_url Optional URL where this was learned
   * @param transe_score Optional TransE confidence score
   * @param is_provisional Whether the gate is provisional
   * @return The gate neuron ID, or 0 if creation failed
   */
  NeuronID createRelationGate(std::size_t subj_token_id,
                              std::size_t rel_token_id,
                              std::size_t obj_token_id,
                              const std::string &source_url = "",
                              float transe_score = 0.0f,
                              bool is_provisional = false);

  /**
   * @brief Create a gate using token symbols (convenience)
   */
  NeuronID createRelationGateBySymbol(const std::string &subject,
                                      const std::string &relation,
                                      const std::string &object,
                                      const std::string &source_url = "",
                                      float transe_score = 0.0f,
                                      bool is_provisional = false);

  // ========== Gate Queries ==========

  /**
   * @brief Get all currently active (firing) relation gates
   * @param threshold Activation threshold
   * @return Vector of active triples
   */
  std::vector<RelationTriple> getActiveRelations(float threshold = 0.5f) const;

  /**
   * @brief Find relations involving a specific token
   * @param token_id Token to search for
   * @param as_subject Search as subject
   * @param as_object Search as object
   * @param as_relation Search as relation
   * @return Matching triples
   */
  std::vector<RelationTriple> findRelationsFor(std::size_t token_id,
                                               bool as_subject = true,
                                               bool as_object = true,
                                               bool as_relation = false) const;

  /**
   * @brief Check if a specific relation exists
   */
  std::optional<RelationTriple> getRelation(std::size_t subj_token_id,
                                            std::size_t rel_token_id,
                                            std::size_t obj_token_id) const;

  /**
   * @brief Get all relations of a given type (e.g., all "is_a" relations)
   */
  std::vector<RelationTriple>
  getRelationsByType(std::size_t rel_token_id) const;

  /**
   * @brief Get all stored relations (Phase 18: for verification target
   * selection)
   */
  const std::vector<RelationTriple> &getAllRelations() const { return gates_; }

  // ========== TransE Proposal ==========

  /**
   * @brief Propose candidate relations between subject and object
   *
   * Uses TransE scoring: score(s, r, o) = -||s + r - o||
   * Higher score = better fit
   *
   * @param subj_token_id Subject token
   * @param obj_token_id Object token
   * @param top_k Number of candidates to return
   * @return Vector of (relation_token_id, score) pairs, sorted descending
   */
  std::vector<std::pair<std::size_t, float>>
  proposeRelations(std::size_t subj_token_id, std::size_t obj_token_id,
                   int top_k = -1 // -1 = use config default
  ) const;

  /**
   * @brief Score a specific triple using TransE
   * @return Score in [0, 1], higher = more plausible
   */
  float scoreTriple(std::size_t subj_token_id, std::size_t rel_token_id,
                    std::size_t obj_token_id) const;

  // ========== Learning ==========

  /**
   * @brief Reinforce a gate (positive evidence)
   */
  void reinforceGate(NeuronID gate_neuron_id, float strength = -1.0f);

  /**
   * @brief Weaken a gate (negative evidence / contradiction)
   */
  void contradictGate(NeuronID gate_neuron_id, float strength = -1.0f);

  /**
   * @brief Update all gates based on current token activations
   * Called each processing step
   */
  void updateGates(float delta_time);

  /**
   * @brief Apply decay to all gate confidences
   */
  void applyDecay(float delta_time);

  // ========== Pruning ==========

  /**
   * @brief Remove low-confidence or highly-contradicted gates
   * @return Number of gates pruned
   */
  std::size_t pruneWeakGates();

  // ========== Statistics ==========

  struct Statistics {
    std::size_t total_gates;
    std::size_t active_gates;
    std::size_t relation_types;
    float avg_confidence;
    float avg_verification_ratio;
    std::size_t gates_created_this_session;
    std::size_t gates_pruned_this_session;
  };

  Statistics getStatistics() const;

  // ========== Configuration ==========

  void setConfig(const RelationGateConfig &config) { config_ = config; }
  const RelationGateConfig &getConfig() const { return config_; }

  // ========== Region Access ==========

  RegionID getGateRegionId() const { return gate_region_id_; }

private:
  std::shared_ptr<HypergraphBrain> brain_;
  LanguageSystem *language_system_;
  RelationGateConfig config_;

  RegionID gate_region_id_; ///< Dedicated region for gate neurons

  // Gate storage
  std::vector<RelationTriple> gates_;
  std::unordered_map<NeuronID, std::size_t>
      gate_index_; ///< NeuronID → gates_ index

  // Token → gate mappings for fast lookup
  std::unordered_multimap<std::size_t, std::size_t>
      subject_gates_; ///< subj_id → gate indices
  std::unordered_multimap<std::size_t, std::size_t>
      object_gates_; ///< obj_id → gate indices
  std::unordered_multimap<std::size_t, std::size_t>
      relation_gates_; ///< rel_id → gate indices

  // Statistics tracking
  std::size_t gates_created_ = 0;
  std::size_t gates_pruned_ = 0;

  // Helpers
  std::string makeGateKey(std::size_t s, std::size_t r, std::size_t o) const;
  std::unordered_map<std::string, std::size_t>
      gate_key_index_; ///< "s:r:o" → gate index

  // Get token embedding (returns nullptr if not found)
  const std::vector<float> *getTokenEmbedding(std::size_t token_id) const;

  // Cosine similarity
  static float cosineSimilarity(const std::vector<float> &a,
                                const std::vector<float> &b);

  // TransE distance: ||s + r - o||
  static float transeDistance(const std::vector<float> &subj,
                              const std::vector<float> &rel,
                              const std::vector<float> &obj);

  // Get all relation token IDs
  std::vector<std::size_t> getAllRelationTokenIds() const;

  // Phase 15b: Extract domain from URL as source identity
  std::string hashSource(const std::string &url) const;
};

} // namespace Core
} // namespace NeuroForge
