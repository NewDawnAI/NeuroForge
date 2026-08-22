#include "core/RelationGate.h"
#include "core/NeuralLanguageBindings.h"
#include "core/Neuron.h"
#include "core/PredicateNormalizer.h"
#include "core/Synapse.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numeric>
#include <sstream>

namespace NeuroForge {
namespace Core {

RelationGateManager::RelationGateManager(std::shared_ptr<HypergraphBrain> brain,
                                         LanguageSystem *language_system,
                                         const RelationGateConfig &config)
    : brain_(brain), language_system_(language_system), config_(config),
      gate_region_id_(0) {
  // Create dedicated region for relation gates
  if (brain_) {
    auto gate_region =
        brain_->createRegion("RelationGateRegion", Region::Type::Cortical,
                             Region::ActivationPattern::Asynchronous);
    if (gate_region) {
      gate_region_id_ = gate_region->getId();
    }
  }

  // Reserve space
  gates_.reserve(config_.max_total_gates);
}

// ========== Gate Creation ==========

NeuronID RelationGateManager::createRelationGate(std::size_t subj_token_id,
                                                 std::size_t rel_token_id,
                                                 std::size_t obj_token_id,
                                                 const std::string &source_url,
                                                 float transe_score,
                                                 bool is_provisional) {
  // Check limits
  if (gates_.size() >= static_cast<std::size_t>(config_.max_total_gates)) {
    std::cerr << "[RelationGate] Max gate limit reached: "
              << config_.max_total_gates << std::endl;
    return 0;
  }

  // Check if gate already exists
  std::string key = makeGateKey(subj_token_id, rel_token_id, obj_token_id);
  auto it = gate_key_index_.find(key);
  if (it != gate_key_index_.end()) {
    // Gate exists - check for Phase 15b/16b evidence accumulation
    RelationTriple &existing = gates_[it->second];

    // Compute source hash (domain from URL)
    std::string source_hash = hashSource(source_url);

    // Phase 16b: Classify the source domain
    DomainClass new_class = classifySource(source_url);

    // Only count if this is a NEW source (domain hash)
    bool new_source =
        !source_hash.empty() && existing.source_hashes.find(source_hash) ==
                                    existing.source_hashes.end();

    // Only count if this is a NEW domain class
    bool new_class_seen = new_class != DomainClass::Unknown &&
                          existing.domain_classes.find(new_class) ==
                              existing.domain_classes.end();

    if (new_source) {
      existing.source_hashes.insert(source_hash);
      existing.support_count++;
    }

    if (new_class_seen) {
      existing.domain_classes.insert(new_class);
    }

    // Phase 16b: Enhanced promotion rule
    // - 2+ different domain classes → CONFIRMED
    // - OR 1 high-trust class (Academic/Government) with 2+ sources → CONFIRMED
    if (existing.is_provisional) {
      bool has_two_classes = existing.domain_classes.size() >= 2;
      bool has_high_trust = false;
      for (const auto &dc : existing.domain_classes) {
        if (isHighTrustClass(dc)) {
          has_high_trust = true;
          break;
        }
      }
      bool can_promote =
          has_two_classes || (has_high_trust && existing.support_count >= 2);

      if (can_promote) {
        existing.is_provisional = false; // Promote to CONFIRMED

        std::cout << "[RelationGate] PROMOTED → CONFIRMED: "
                  << existing.subject_token_id << " → "
                  << existing.relation_token_id << " → "
                  << existing.object_token_id
                  << " (sources=" << existing.support_count
                  << ", classes=" << existing.domain_classes.size() << ")\n";
      }
    }

    return existing.gate_neuron_id;
  }

  // Get the gate region
  auto gate_region = brain_->getRegion(gate_region_id_);
  if (!gate_region) {
    std::cerr << "[RelationGate] Gate region not found" << std::endl;
    return 0;
  }

  // Create the gate neuron (createNeurons returns a vector)
  auto neurons = gate_region->createNeurons(1);
  if (neurons.empty()) {
    std::cerr << "[RelationGate] Failed to create gate neuron" << std::endl;
    return 0;
  }

  auto gate_neuron = neurons[0];

  // Set gate threshold higher (needs multiple inputs to fire)
  gate_neuron->setThreshold(config_.gate_threshold);

  NeuronID gate_id = gate_neuron->getId();

  // Create the relation triple
  RelationTriple triple;
  triple.subject_token_id = subj_token_id;
  triple.relation_token_id = rel_token_id;
  triple.object_token_id = obj_token_id;
  triple.gate_neuron_id = gate_id;
  triple.gate_region_id = gate_region_id_;
  triple.confidence = config_.initial_gate_weight;
  triple.activation = 0.0f;
  triple.transe_score = transe_score;
  triple.verification_count = 0;
  triple.contradiction_count = 0;
  triple.source_url = source_url;
  triple.created = std::chrono::system_clock::now();
  triple.last_activated = triple.created;
  triple.is_directed = config_.default_directed;
  triple.is_provisional = is_provisional;

  // Phase 15b: Initialize source tracking
  triple.support_count = 1;
  std::string initial_hash = hashSource(source_url);
  if (!initial_hash.empty()) {
    triple.source_hashes.insert(initial_hash);
  }

  // Phase 16b: Initialize domain class tracking
  DomainClass initial_class = classifySource(source_url);
  if (initial_class != DomainClass::Unknown) {
    triple.domain_classes.insert(initial_class);
  }

  // Store the triple
  std::size_t gate_index = gates_.size();
  gates_.push_back(triple);

  // Update indices
  gate_index_[gate_id] = gate_index;
  gate_key_index_[key] = gate_index;
  subject_gates_.insert({subj_token_id, gate_index});
  object_gates_.insert({obj_token_id, gate_index});
  relation_gates_.insert({rel_token_id, gate_index});

  gates_created_++;

  // Note: Synapses from token assemblies to gate neuron would be created
  // when NeuralLanguageBindings is integrated. For now, the gate exists
  // and can be updated via updateGates().

  return gate_id;
}

NeuronID RelationGateManager::createRelationGateBySymbol(
    const std::string &subject, const std::string &relation,
    const std::string &object, const std::string &source_url,
    float transe_score, bool is_provisional) {
  if (!language_system_) {
    return 0;
  }

  // Phase 19: Normalize the predicate before lookup
  std::string normalized_relation = PredicateNormalizer::normalize(relation);

  auto *subj_token = language_system_->getToken(subject);
  auto *rel_token = language_system_->getToken(normalized_relation);
  auto *obj_token = language_system_->getToken(object);

  if (!subj_token || !rel_token || !obj_token) {
    std::cerr << "[RelationGate] Token not found: "
              << (!subj_token ? subject
                              : (!rel_token ? normalized_relation : object))
              << std::endl;
    return 0;
  }

  // Look up token IDs
  std::size_t subj_id, rel_id, obj_id;
  if (!language_system_->getTokenId(subject, subj_id) ||
      !language_system_->getTokenId(normalized_relation, rel_id) ||
      !language_system_->getTokenId(object, obj_id)) {
    std::cerr << "[RelationGate] Failed to get token IDs" << std::endl;
    return 0;
  }

  return createRelationGate(subj_id, rel_id, obj_id, source_url, transe_score,
                            is_provisional);
}

// ========== Gate Queries ==========

std::vector<RelationTriple>
RelationGateManager::getActiveRelations(float threshold) const {
  std::vector<RelationTriple> active;
  active.reserve(gates_.size() / 4); // Estimate 25% active

  for (const auto &gate : gates_) {
    if (gate.activation >= threshold) {
      active.push_back(gate);
    }
  }

  return active;
}

std::vector<RelationTriple>
RelationGateManager::findRelationsFor(std::size_t token_id, bool as_subject,
                                      bool as_object, bool as_relation) const {
  std::vector<RelationTriple> results;
  std::unordered_set<std::size_t> seen;

  if (as_subject) {
    auto range = subject_gates_.equal_range(token_id);
    for (auto it = range.first; it != range.second; ++it) {
      if (seen.insert(it->second).second) {
        results.push_back(gates_[it->second]);
      }
    }
  }

  if (as_object) {
    auto range = object_gates_.equal_range(token_id);
    for (auto it = range.first; it != range.second; ++it) {
      if (seen.insert(it->second).second) {
        results.push_back(gates_[it->second]);
      }
    }
  }

  if (as_relation) {
    auto range = relation_gates_.equal_range(token_id);
    for (auto it = range.first; it != range.second; ++it) {
      if (seen.insert(it->second).second) {
        results.push_back(gates_[it->second]);
      }
    }
  }

  return results;
}

std::optional<RelationTriple>
RelationGateManager::getRelation(std::size_t subj_token_id,
                                 std::size_t rel_token_id,
                                 std::size_t obj_token_id) const {
  std::string key = makeGateKey(subj_token_id, rel_token_id, obj_token_id);
  auto it = gate_key_index_.find(key);
  if (it != gate_key_index_.end()) {
    return gates_[it->second];
  }
  return std::nullopt;
}

std::vector<RelationTriple>
RelationGateManager::getRelationsByType(std::size_t rel_token_id) const {
  std::vector<RelationTriple> results;
  auto range = relation_gates_.equal_range(rel_token_id);
  for (auto it = range.first; it != range.second; ++it) {
    results.push_back(gates_[it->second]);
  }
  return results;
}

// ========== TransE Proposal ==========

std::vector<std::pair<std::size_t, float>>
RelationGateManager::proposeRelations(std::size_t subj_token_id,
                                      std::size_t obj_token_id,
                                      int top_k) const {
  if (top_k < 0) {
    top_k = config_.top_k_proposals;
  }

  const auto *subj_emb = getTokenEmbedding(subj_token_id);
  const auto *obj_emb = getTokenEmbedding(obj_token_id);

  if (!subj_emb || !obj_emb) {
    return {};
  }

  // Get all relation tokens
  std::vector<std::size_t> rel_ids = getAllRelationTokenIds();

  // Score each relation
  std::vector<std::pair<std::size_t, float>> scored;
  scored.reserve(rel_ids.size());

  for (std::size_t rel_id : rel_ids) {
    const auto *rel_emb = getTokenEmbedding(rel_id);
    if (!rel_emb)
      continue;

    float dist = transeDistance(*subj_emb, *rel_emb, *obj_emb);
    // Convert distance to score (lower distance = higher score)
    float score = 1.0f / (1.0f + dist);
    scored.push_back({rel_id, score});
  }

  // Sort by score descending
  std::sort(scored.begin(), scored.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });

  // Return top-k
  if (static_cast<int>(scored.size()) > top_k) {
    scored.resize(top_k);
  }

  return scored;
}

float RelationGateManager::scoreTriple(std::size_t subj_token_id,
                                       std::size_t rel_token_id,
                                       std::size_t obj_token_id) const {
  const auto *subj_emb = getTokenEmbedding(subj_token_id);
  const auto *rel_emb = getTokenEmbedding(rel_token_id);
  const auto *obj_emb = getTokenEmbedding(obj_token_id);

  if (!subj_emb || !rel_emb || !obj_emb) {
    return 0.0f;
  }

  float dist = transeDistance(*subj_emb, *rel_emb, *obj_emb);
  return 1.0f / (1.0f + dist);
}

// ========== Learning ==========

void RelationGateManager::reinforceGate(NeuronID gate_neuron_id,
                                        float strength) {
  if (strength < 0.0f) {
    strength = config_.reinforcement_rate;
  }

  auto it = gate_index_.find(gate_neuron_id);
  if (it == gate_index_.end())
    return;

  auto &gate = gates_[it->second];
  gate.confidence = std::min(1.0f, gate.confidence + strength);
  gate.verification_count++;
  gate.last_activated = std::chrono::system_clock::now();
}

void RelationGateManager::contradictGate(NeuronID gate_neuron_id,
                                         float strength) {
  if (strength < 0.0f) {
    strength = config_.reinforcement_rate;
  }

  auto it = gate_index_.find(gate_neuron_id);
  if (it == gate_index_.end())
    return;

  auto &gate = gates_[it->second];
  gate.confidence = std::max(0.0f, gate.confidence - strength);
  gate.contradiction_count++;
}

void RelationGateManager::updateGates(float delta_time) {
  if (!brain_)
    return;

  auto gate_region = brain_->getRegion(gate_region_id_);
  if (!gate_region)
    return;

  // Update activation levels from gate neurons
  for (auto &gate : gates_) {
    auto neuron = gate_region->getNeuron(gate.gate_neuron_id);
    if (neuron) {
      gate.activation = neuron->getActivation();

      if (gate.activation > config_.gate_threshold) {
        gate.last_activated = std::chrono::system_clock::now();
      }
    }
  }

  // Apply decay
  applyDecay(delta_time);
}

void RelationGateManager::applyDecay(float delta_time) {
  float decay = config_.decay_rate * delta_time;

  for (auto &gate : gates_) {
    gate.confidence = std::max(0.0f, gate.confidence - decay);
  }
}

// ========== Pruning ==========

std::size_t RelationGateManager::pruneWeakGates() {
  auto now = std::chrono::system_clock::now();
  std::size_t pruned = 0;

  // Mark gates for removal (can't remove while iterating)
  std::vector<std::size_t> to_remove;

  for (std::size_t i = 0; i < gates_.size(); ++i) {
    const auto &gate = gates_[i];

    // Check age
    auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now - gate.created)
                      .count();

    if (age_ms < config_.min_age_before_prune_ms) {
      continue; // Too young to prune
    }

    // Check for pruning conditions
    bool should_prune = false;

    if (gate.confidence < config_.prune_threshold) {
      should_prune = true;
    }

    if (gate.contradiction_count >= config_.prune_after_contradictions &&
        gate.verificationRatio() < 0.3f) {
      should_prune = true;
    }

    if (should_prune) {
      to_remove.push_back(i);
    }
  }

  // Remove in reverse order to maintain indices
  std::sort(to_remove.rbegin(), to_remove.rend());

  for (std::size_t idx : to_remove) {
    const auto &gate = gates_[idx];

    // Remove from indices
    gate_index_.erase(gate.gate_neuron_id);
    gate_key_index_.erase(makeGateKey(
        gate.subject_token_id, gate.relation_token_id, gate.object_token_id));

    // Note: multimap removal is more complex, we'll rebuild if needed

    // Swap and pop
    if (idx < gates_.size() - 1) {
      gates_[idx] = gates_.back();
      // Update index for moved gate
      gate_index_[gates_[idx].gate_neuron_id] = idx;
      gate_key_index_[makeGateKey(gates_[idx].subject_token_id,
                                  gates_[idx].relation_token_id,
                                  gates_[idx].object_token_id)] = idx;
    }
    gates_.pop_back();

    pruned++;
  }

  gates_pruned_ += pruned;

  // Rebuild multimaps if we pruned anything
  if (pruned > 0) {
    subject_gates_.clear();
    object_gates_.clear();
    relation_gates_.clear();

    for (std::size_t i = 0; i < gates_.size(); ++i) {
      subject_gates_.insert({gates_[i].subject_token_id, i});
      object_gates_.insert({gates_[i].object_token_id, i});
      relation_gates_.insert({gates_[i].relation_token_id, i});
    }
  }

  return pruned;
}

// ========== Statistics ==========

RelationGateManager::Statistics RelationGateManager::getStatistics() const {
  Statistics stats;
  stats.total_gates = gates_.size();
  stats.active_gates = 0;
  stats.relation_types = 0;
  stats.avg_confidence = 0.0f;
  stats.avg_verification_ratio = 0.0f;
  stats.gates_created_this_session = gates_created_;
  stats.gates_pruned_this_session = gates_pruned_;

  if (gates_.empty()) {
    return stats;
  }

  std::unordered_set<std::size_t> rel_types;
  float total_conf = 0.0f;
  float total_ratio = 0.0f;

  for (const auto &gate : gates_) {
    if (gate.activation >= config_.gate_threshold) {
      stats.active_gates++;
    }
    rel_types.insert(gate.relation_token_id);
    total_conf += gate.confidence;
    total_ratio += gate.verificationRatio();
  }

  stats.relation_types = rel_types.size();
  stats.avg_confidence = total_conf / gates_.size();
  stats.avg_verification_ratio = total_ratio / gates_.size();

  return stats;
}

// ========== Helpers ==========

std::string RelationGateManager::makeGateKey(std::size_t s, std::size_t r,
                                             std::size_t o) const {
  std::ostringstream oss;
  oss << s << ":" << r << ":" << o;
  return oss.str();
}

const std::vector<float> *
RelationGateManager::getTokenEmbedding(std::size_t token_id) const {
  if (!language_system_)
    return nullptr;

  auto *token = language_system_->getToken(token_id);
  if (!token)
    return nullptr;

  return &token->embedding;
}

float RelationGateManager::cosineSimilarity(const std::vector<float> &a,
                                            const std::vector<float> &b) {
  if (a.size() != b.size() || a.empty())
    return 0.0f;

  float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
  for (std::size_t i = 0; i < a.size(); ++i) {
    dot += a[i] * b[i];
    norm_a += a[i] * a[i];
    norm_b += b[i] * b[i];
  }

  float denom = std::sqrt(norm_a) * std::sqrt(norm_b);
  return denom > 1e-8f ? dot / denom : 0.0f;
}

float RelationGateManager::transeDistance(const std::vector<float> &subj,
                                          const std::vector<float> &rel,
                                          const std::vector<float> &obj) {
  if (subj.size() != rel.size() || rel.size() != obj.size() || subj.empty()) {
    return 1e6f; // Large distance for invalid inputs
  }

  // TransE: ||s + r - o||
  float sum_sq = 0.0f;
  for (std::size_t i = 0; i < subj.size(); ++i) {
    float diff = subj[i] + rel[i] - obj[i];
    sum_sq += diff * diff;
  }

  return std::sqrt(sum_sq);
}

std::vector<std::size_t> RelationGateManager::getAllRelationTokenIds() const {
  if (!language_system_)
    return {};

  std::vector<std::size_t> result;

  // Get all tokens of type Relation
  // Note: This requires iterating through LanguageSystem's vocabulary
  // For now, return relation tokens we've seen in existing gates
  std::unordered_set<std::size_t> seen;
  for (const auto &gate : gates_) {
    if (seen.insert(gate.relation_token_id).second) {
      result.push_back(gate.relation_token_id);
    }
  }

  return result;
}

// Phase 15b: Extract domain from URL as source identity
std::string RelationGateManager::hashSource(const std::string &url) const {
  if (url.empty())
    return "";

  // Extract domain as source identity
  // e.g., "https://en.wikipedia.org/wiki/ML" → "en.wikipedia.org"
  auto pos = url.find("://");
  if (pos == std::string::npos)
    return url; // No protocol, use as-is
  std::size_t start = pos + 3;
  auto end = url.find('/', start);
  if (end == std::string::npos)
    end = url.size();
  return url.substr(start, end - start);
}

} // namespace Core
} // namespace NeuroForge
