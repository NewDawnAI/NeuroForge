#include "verification/GroundingVerifier.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <sstream>

namespace NeuroForge {
namespace Verification {

GroundingVerifier::GroundingVerifier(Core::LanguageSystem *language_system,
                                     Core::RelationGateManager *relation_gates,
                                     const VerifierConfig &config)
    : language_system_(language_system), relation_gates_(relation_gates),
      config_(config) {
  last_prune_time_ = std::chrono::system_clock::now();
}

GroundingVerifier::~GroundingVerifier() = default;

bool GroundingVerifier::initialize() {
  if (!language_system_) {
    std::cerr << "[GroundingVerifier] WARNING: LanguageSystem is null\n";
  }

  is_initialized_ = true;
  return true;
}

// ========== Hypothesis Management ==========

std::size_t GroundingVerifier::createHypothesis(
    const std::string &subject, const std::string &relation,
    const std::string &object, const RelationEvidence *initial_evidence) {

  // Get token IDs
  std::size_t subject_id = 0, relation_id = 0, object_id = 0;

  if (language_system_) {
    language_system_->getTokenId(subject, subject_id);
    language_system_->getTokenId(relation, relation_id);
    language_system_->getTokenId(object, object_id);
  }

  // Check if hypothesis already exists
  std::string key = makeTripleKey(subject_id, relation_id, object_id);

  {
    std::lock_guard<std::mutex> lock(hypothesis_mutex_);

    auto it = triple_to_hypothesis_.find(key);
    if (it != triple_to_hypothesis_.end()) {
      std::size_t existing_id = it->second;
      if (initial_evidence) {
        auto idx_it = hypothesis_index_.find(existing_id);
        if (idx_it != hypothesis_index_.end() &&
            idx_it->second < hypotheses_.size()) {
          auto &hypothesis = hypotheses_[idx_it->second];
          if (isNewSource(hypothesis, initial_evidence->source_url)) {
            int total_evidence =
                hypothesis.supportCount() + hypothesis.contradictCount();
            if (total_evidence < config_.max_evidence_per_hypothesis) {
              if (initial_evidence->supports) {
                hypothesis.supporting_evidence.push_back(*initial_evidence);
              } else {
                hypothesis.contradicting_evidence.push_back(*initial_evidence);
              }
              hypothesis.state = RelationHypothesis::State::Pending;
              hypothesis.last_checked = std::chrono::system_clock::now();
              updateHypothesisState(hypothesis);
              stats_.total_evidence_items++;

              if (evidence_callback_) {
                evidence_callback_(hypothesis, *initial_evidence);
              }
            }
          }
        }
      }

      return existing_id;
    }

    // Check capacity
    if (hypotheses_.size() >=
        static_cast<std::size_t>(config_.max_hypotheses)) {
      pruneStaleHypotheses();
      if (hypotheses_.size() >=
          static_cast<std::size_t>(config_.max_hypotheses)) {
        return 0; // Still full after pruning
      }
    }

    // Create new hypothesis
    RelationHypothesis hypothesis;
    hypothesis.subject_id = subject_id;
    hypothesis.relation_id = relation_id;
    hypothesis.object_id = object_id;
    hypothesis.subject_text = subject;
    hypothesis.relation_text = relation;
    hypothesis.object_text = object;
    hypothesis.state = RelationHypothesis::State::Unverified;
    hypothesis.min_sources_required = config_.min_sources_for_verification;
    hypothesis.created_at = std::chrono::system_clock::now();
    hypothesis.last_checked = hypothesis.created_at;

    if (initial_evidence) {
      if (initial_evidence->supports) {
        hypothesis.supporting_evidence.push_back(*initial_evidence);
      } else {
        hypothesis.contradicting_evidence.push_back(*initial_evidence);
      }
    }

    std::size_t id = next_hypothesis_id_++;
    hypothesis_index_[id] = hypotheses_.size();
    triple_to_hypothesis_[key] = id;
    hypotheses_.push_back(hypothesis);

    stats_.total_hypotheses++;

    return id;
  }
}

std::size_t GroundingVerifier::createHypothesis(
    std::size_t subject_id, std::size_t relation_id, std::size_t object_id,
    const RelationEvidence *initial_evidence) {

  std::string subject_text, relation_text, object_text;

  if (language_system_) {
    if (auto *token = language_system_->getToken(subject_id)) {
      subject_text = token->symbol;
    }
    if (auto *token = language_system_->getToken(relation_id)) {
      relation_text = token->symbol;
    }
    if (auto *token = language_system_->getToken(object_id)) {
      object_text = token->symbol;
    }
  }

  return createHypothesis(subject_text, relation_text, object_text,
                          initial_evidence);
}

std::optional<RelationHypothesis>
GroundingVerifier::getHypothesis(std::size_t id) const {
  std::lock_guard<std::mutex> lock(hypothesis_mutex_);

  auto it = hypothesis_index_.find(id);
  if (it != hypothesis_index_.end() && it->second < hypotheses_.size()) {
    return hypotheses_[it->second];
  }
  return std::nullopt;
}

std::optional<RelationHypothesis>
GroundingVerifier::getHypothesis(std::size_t subject_id,
                                 std::size_t relation_id,
                                 std::size_t object_id) const {

  std::string key = makeTripleKey(subject_id, relation_id, object_id);

  std::lock_guard<std::mutex> lock(hypothesis_mutex_);

  auto it = triple_to_hypothesis_.find(key);
  if (it != triple_to_hypothesis_.end()) {
    auto idx_it = hypothesis_index_.find(it->second);
    if (idx_it != hypothesis_index_.end() &&
        idx_it->second < hypotheses_.size()) {
      return hypotheses_[idx_it->second];
    }
  }
  return std::nullopt;
}

std::vector<RelationHypothesis>
GroundingVerifier::getHypothesesByState(RelationHypothesis::State state) const {

  std::lock_guard<std::mutex> lock(hypothesis_mutex_);

  std::vector<RelationHypothesis> result;
  for (const auto &h : hypotheses_) {
    if (h.state == state) {
      result.push_back(h);
    }
  }
  return result;
}

std::vector<RelationHypothesis>
GroundingVerifier::getPendingHypotheses(int limit) const {
  std::lock_guard<std::mutex> lock(hypothesis_mutex_);

  std::vector<RelationHypothesis> result;
  for (const auto &h : hypotheses_) {
    if (h.state == RelationHypothesis::State::Unverified ||
        h.state == RelationHypothesis::State::Pending) {
      result.push_back(h);
      if (static_cast<int>(result.size()) >= limit)
        break;
    }
  }
  return result;
}

// ========== Evidence Processing ==========

bool GroundingVerifier::addEvidence(std::size_t hypothesis_id,
                                    const RelationEvidence &evidence) {
  std::lock_guard<std::mutex> lock(hypothesis_mutex_);

  auto it = hypothesis_index_.find(hypothesis_id);
  if (it == hypothesis_index_.end() || it->second >= hypotheses_.size()) {
    return false;
  }

  auto &hypothesis = hypotheses_[it->second];

  // Check if from new source
  if (!isNewSource(hypothesis, evidence.source_url)) {
    return false; // Already have evidence from this source
  }

  // Check evidence limit
  int total_evidence = hypothesis.supportCount() + hypothesis.contradictCount();
  if (total_evidence >= config_.max_evidence_per_hypothesis) {
    return false;
  }

  // Add evidence
  if (evidence.supports) {
    hypothesis.supporting_evidence.push_back(evidence);
  } else {
    hypothesis.contradicting_evidence.push_back(evidence);
  }

  // Update state
  hypothesis.state = RelationHypothesis::State::Pending;
  hypothesis.last_checked = std::chrono::system_clock::now();
  updateHypothesisState(hypothesis);

  stats_.total_evidence_items++;

  if (evidence_callback_) {
    evidence_callback_(hypothesis, evidence);
  }

  return true;
}

bool GroundingVerifier::addEvidence(const std::string &subject,
                                    const std::string &relation,
                                    const std::string &object,
                                    const RelationEvidence &evidence) {
  // Get or create hypothesis
  std::size_t id = createHypothesis(subject, relation, object, nullptr);
  if (id == 0)
    return false;

  return addEvidence(id, evidence);
}

bool GroundingVerifier::addContradiction(std::size_t hypothesis_id,
                                         const RelationEvidence &evidence) {
  RelationEvidence contradiction = evidence;
  contradiction.supports = false;
  return addEvidence(hypothesis_id, contradiction);
}

int GroundingVerifier::processTextForEvidence(const std::string &text,
                                              const std::string &source_url) {
  int found = 0;

  std::lock_guard<std::mutex> lock(hypothesis_mutex_);

  for (auto &hypothesis : hypotheses_) {
    // Skip already verified or rejected
    if (hypothesis.state == RelationHypothesis::State::Verified ||
        hypothesis.state == RelationHypothesis::State::Rejected) {
      continue;
    }

    // Check if text contains this relation
    if (textContainsRelation(text, hypothesis.subject_text,
                             hypothesis.relation_text,
                             hypothesis.object_text)) {

      // Check if new source
      if (!isNewSource(hypothesis, source_url)) {
        continue;
      }

      // Create evidence
      RelationEvidence evidence;
      evidence.source_url = source_url;
      evidence.source_text = text.substr(0, std::min(text.size(), size_t(200)));
      evidence.confidence = 0.7f; // Base confidence for pattern match
      evidence.supports = true;
      evidence.found_at = std::chrono::system_clock::now();

      // Get TransE score if available
      if (language_system_) {
        evidence.transe_score = language_system_->scoreRelationTriple(
            hypothesis.subject_text, hypothesis.relation_text,
            hypothesis.object_text);
      }

      hypothesis.supporting_evidence.push_back(evidence);
      hypothesis.state = RelationHypothesis::State::Pending;
      hypothesis.last_checked = std::chrono::system_clock::now();
      updateHypothesisState(hypothesis);

      stats_.total_evidence_items++;
      found++;

      if (evidence_callback_) {
        evidence_callback_(hypothesis, evidence);
      }
    }
  }

  return found;
}

// ========== Verification ==========

RelationHypothesis::State
GroundingVerifier::verifyHypothesis(std::size_t hypothesis_id) {
  std::lock_guard<std::mutex> lock(hypothesis_mutex_);

  auto it = hypothesis_index_.find(hypothesis_id);
  if (it == hypothesis_index_.end() || it->second >= hypotheses_.size()) {
    return RelationHypothesis::State::Unverified;
  }

  auto &hypothesis = hypotheses_[it->second];
  auto old_state = hypothesis.state;

  updateHypothesisState(hypothesis);

  if (hypothesis.state != old_state && verification_callback_) {
    verification_callback_(hypothesis, hypothesis.isVerified());
  }

  return hypothesis.state;
}

int GroundingVerifier::verifyAllPending() {
  std::lock_guard<std::mutex> lock(hypothesis_mutex_);

  int changed = 0;

  for (auto &hypothesis : hypotheses_) {
    if (hypothesis.state == RelationHypothesis::State::Pending ||
        hypothesis.state == RelationHypothesis::State::Unverified) {

      auto old_state = hypothesis.state;
      updateHypothesisState(hypothesis);

      if (hypothesis.state != old_state) {
        changed++;

        if (verification_callback_) {
          verification_callback_(hypothesis, hypothesis.isVerified());
        }
      }
    }
  }

  updateStats();
  return changed;
}

float GroundingVerifier::computeVerificationScore(
    const RelationHypothesis &hypothesis) const {
  if (hypothesis.supporting_evidence.empty()) {
    return 0.0f;
  }

  float total_score = 0.0f;
  int unique_sources = countUniqueSources(hypothesis);

  for (const auto &evidence : hypothesis.supporting_evidence) {
    float source_trust = getSourceTrust(evidence.source_url);
    float evidence_score = config_.base_evidence_weight * source_trust;

    // Add TransE bonus
    evidence_score += config_.transe_weight * evidence.transe_score;

    // Add confidence factor
    evidence_score *= evidence.confidence;

    total_score += evidence_score;
  }

  // Normalize by number of evidence items
  total_score /= hypothesis.supporting_evidence.size();

  // Source diversity bonus
  if (unique_sources >= config_.min_sources_for_verification) {
    total_score += config_.source_diversity_bonus *
                   std::min(1.0f, (unique_sources -
                                   config_.min_sources_for_verification + 1) *
                                      0.2f);
  }

  // Penalty for contradictions
  float contradiction_ratio = 0.0f;
  if (!hypothesis.contradicting_evidence.empty()) {
    int total = hypothesis.supportCount() + hypothesis.contradictCount();
    contradiction_ratio =
        static_cast<float>(hypothesis.contradictCount()) / total;
    total_score *= (1.0f - contradiction_ratio);
  }

  return std::min(1.0f, std::max(0.0f, total_score));
}

float GroundingVerifier::computeConsistencyScore(
    const RelationHypothesis &hypothesis) const {
  int support = hypothesis.supportCount();
  int contradict = hypothesis.contradictCount();
  int total = support + contradict;

  if (total == 0)
    return 0.5f; // Neutral

  // Simple consistency = support ratio
  float consistency = static_cast<float>(support) / total;

  return consistency;
}

// ========== Relation Gate Integration ==========

void GroundingVerifier::updateRelationGate(
    const RelationHypothesis &hypothesis) {
  if (!relation_gates_)
    return;

  // Find or create the relation gate
  auto gate = relation_gates_->getRelation(
      hypothesis.subject_id, hypothesis.relation_id, hypothesis.object_id);

  if (hypothesis.isVerified()) {
    if (gate) {
      // Reinforce existing gate
      relation_gates_->reinforceGate(gate->gate_neuron_id,
                                     hypothesis.verification_score * 0.2f);
    } else {
      // Create new gate if verified
      relation_gates_->createRelationGateBySymbol(
          hypothesis.subject_text, hypothesis.relation_text,
          hypothesis.object_text,
          hypothesis.supporting_evidence.empty()
              ? ""
              : hypothesis.supporting_evidence[0].source_url);
    }
  } else if (hypothesis.state == RelationHypothesis::State::Rejected) {
    if (gate) {
      // Contradict the gate
      relation_gates_->contradictGate(gate->gate_neuron_id, 0.5f);
    }
  }
}

int GroundingVerifier::syncToRelationGates() {
  if (!relation_gates_)
    return 0;

  std::lock_guard<std::mutex> lock(hypothesis_mutex_);

  int updated = 0;

  for (const auto &hypothesis : hypotheses_) {
    if (hypothesis.isVerified() ||
        hypothesis.state == RelationHypothesis::State::Rejected) {
      updateRelationGate(hypothesis);
      updated++;
    }
  }

  return updated;
}

// ========== Maintenance ==========

std::size_t GroundingVerifier::pruneStaleHypotheses() {
  auto now = std::chrono::system_clock::now();

  std::lock_guard<std::mutex> lock(hypothesis_mutex_);

  std::vector<std::size_t> to_remove;

  for (std::size_t i = 0; i < hypotheses_.size(); ++i) {
    const auto &h = hypotheses_[i];

    // Keep verified and rejected
    if (h.isVerified() || h.state == RelationHypothesis::State::Rejected) {
      continue;
    }

    // Check age
    auto age =
        std::chrono::duration_cast<std::chrono::hours>(now - h.created_at)
            .count();

    if (age > config_.prune_interval_hours && h.supportCount() == 0) {
      to_remove.push_back(i);
    }
  }

  // Remove in reverse order
  std::sort(to_remove.rbegin(), to_remove.rend());

  for (std::size_t idx : to_remove) {
    // Update indices
    std::string key =
        makeTripleKey(hypotheses_[idx].subject_id, hypotheses_[idx].relation_id,
                      hypotheses_[idx].object_id);
    std::size_t removed_id = 0;
    if (auto it = triple_to_hypothesis_.find(key); it != triple_to_hypothesis_.end()) {
      removed_id = it->second;
      triple_to_hypothesis_.erase(it);
    }

    // Swap and pop
    if (idx < hypotheses_.size() - 1) {
      hypotheses_[idx] = hypotheses_.back();
      // Update index for moved hypothesis
      std::string moved_key = makeTripleKey(hypotheses_[idx].subject_id,
                                            hypotheses_[idx].relation_id,
                                            hypotheses_[idx].object_id);
      if (auto it = triple_to_hypothesis_.find(moved_key);
          it != triple_to_hypothesis_.end()) {
        hypothesis_index_[it->second] = idx;
      }
    }
    hypotheses_.pop_back();
    if (removed_id != 0) {
      hypothesis_index_.erase(removed_id);
    }
  }

  last_prune_time_ = now;

  return to_remove.size();
}

void GroundingVerifier::applyDecay(float delta_time) {
  std::lock_guard<std::mutex> lock(hypothesis_mutex_);

  for (auto &hypothesis : hypotheses_) {
    if (hypothesis.state == RelationHypothesis::State::Unverified ||
        hypothesis.state == RelationHypothesis::State::Pending) {

      hypothesis.verification_score *=
          (1.0f - config_.hypothesis_decay_rate * delta_time);
    }
  }
}

// ========== Statistics ==========

VerifierStats GroundingVerifier::getStatistics() const {
  std::lock_guard<std::mutex> lock(hypothesis_mutex_);

  VerifierStats stats;
  stats.total_hypotheses = hypotheses_.size();

  float total_score = 0.0f;
  float total_sources = 0.0f;
  int verified_count = 0;

  for (const auto &h : hypotheses_) {
    stats.total_evidence_items += h.supportCount() + h.contradictCount();

    switch (h.state) {
    case RelationHypothesis::State::Verified:
      stats.verified_hypotheses++;
      total_score += h.verification_score;
      total_sources += countUniqueSources(h);
      verified_count++;
      break;
    case RelationHypothesis::State::Contradicted:
      stats.contradicted_hypotheses++;
      break;
    case RelationHypothesis::State::Rejected:
      stats.rejected_hypotheses++;
      break;
    case RelationHypothesis::State::Pending:
    case RelationHypothesis::State::Unverified:
      stats.pending_hypotheses++;
      break;
    }
  }

  if (verified_count > 0) {
    stats.avg_verification_score = total_score / verified_count;
    stats.avg_sources_per_verified = total_sources / verified_count;
  }

  return stats;
}

void GroundingVerifier::resetStatistics() {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  stats_ = VerifierStats{};
}

// ========== Internal Methods ==========

std::string GroundingVerifier::makeTripleKey(std::size_t s, std::size_t r,
                                             std::size_t o) const {
  return std::to_string(s) + ":" + std::to_string(r) + ":" + std::to_string(o);
}

float GroundingVerifier::getSourceTrust(const std::string &url) const {
  for (const auto &[domain, trust] : config_.source_trust) {
    if (url.find(domain) != std::string::npos) {
      return trust;
    }
  }
  return config_.default_source_trust;
}

bool GroundingVerifier::isNewSource(const RelationHypothesis &hypothesis,
                                    const std::string &source_url) const {
  // Check supporting evidence
  for (const auto &e : hypothesis.supporting_evidence) {
    if (e.source_url == source_url)
      return false;
  }
  // Check contradicting evidence
  for (const auto &e : hypothesis.contradicting_evidence) {
    if (e.source_url == source_url)
      return false;
  }
  return true;
}

int GroundingVerifier::countUniqueSources(
    const RelationHypothesis &hypothesis) const {
  std::unordered_set<std::string> sources;

  for (const auto &e : hypothesis.supporting_evidence) {
    sources.insert(e.source_url);
  }
  for (const auto &e : hypothesis.contradicting_evidence) {
    sources.insert(e.source_url);
  }

  return static_cast<int>(sources.size());
}

void GroundingVerifier::updateHypothesisState(RelationHypothesis &hypothesis) {
  // Compute scores
  hypothesis.verification_score = computeVerificationScore(hypothesis);
  hypothesis.consistency_score = computeConsistencyScore(hypothesis);

  int unique_sources = countUniqueSources(hypothesis);
  float contradiction_ratio = 0.0f;

  int total = hypothesis.supportCount() + hypothesis.contradictCount();
  if (total > 0) {
    contradiction_ratio =
        static_cast<float>(hypothesis.contradictCount()) / total;
  }

  // Determine state
  if (unique_sources >= hypothesis.min_sources_required &&
      hypothesis.verification_score >= config_.min_verification_score &&
      hypothesis.consistency_score >=
          (1.0f - config_.contradiction_threshold)) {
    hypothesis.state = RelationHypothesis::State::Verified;
  } else if (contradiction_ratio >= config_.contradiction_threshold) {
    if (hypothesis.contradictCount() >= hypothesis.min_sources_required) {
      hypothesis.state = RelationHypothesis::State::Rejected;
    } else {
      hypothesis.state = RelationHypothesis::State::Contradicted;
    }
  } else if (hypothesis.supportCount() > 0 ||
             hypothesis.contradictCount() > 0) {
    hypothesis.state = RelationHypothesis::State::Pending;
  } else {
    hypothesis.state = RelationHypothesis::State::Unverified;
  }

  hypothesis.last_checked = std::chrono::system_clock::now();
}

void GroundingVerifier::updateStats() {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  stats_ = getStatistics();
}

std::vector<std::string>
GroundingVerifier::tokenize(const std::string &text) const {
  std::vector<std::string> tokens;
  std::string current;

  for (char c : text) {
    if (std::isalnum(static_cast<unsigned char>(c))) {
      current += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    } else if (!current.empty()) {
      tokens.push_back(current);
      current.clear();
    }
  }

  if (!current.empty()) {
    tokens.push_back(current);
  }

  return tokens;
}

bool GroundingVerifier::textContainsRelation(const std::string &text,
                                             const std::string &subject,
                                             const std::string &relation,
                                             const std::string &object) const {

  // Simple check: does text contain subject, relation indicator, and object?
  std::string lower_text = text;
  std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  std::string lower_subject = subject;
  std::string lower_object = object;
  std::transform(lower_subject.begin(), lower_subject.end(),
                 lower_subject.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  std::transform(lower_object.begin(), lower_object.end(), lower_object.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  // Check if both subject and object appear
  bool has_subject = lower_text.find(lower_subject) != std::string::npos;
  bool has_object = lower_text.find(lower_object) != std::string::npos;

  if (!has_subject || !has_object) {
    return false;
  }

  // Check for relation indicators based on relation type
  std::vector<std::string> indicators;

  if (relation == "is_a" || relation == "is a" || relation == "isa") {
    indicators = {"is a", "is an", "are", "was a", "type of", "kind of"};
  } else if (relation == "has" || relation == "have") {
    indicators = {"has", "have", "with", "contains", "includes"};
  } else if (relation == "part_of" || relation == "part of") {
    indicators = {"part of", "belongs to", "component of", "member of"};
  } else if (relation == "located_in" || relation == "located in") {
    indicators = {"in", "at", "located in", "found in"};
  } else if (relation == "made_of" || relation == "made of") {
    indicators = {"made of", "composed of", "consists of"};
  } else {
    // Generic - just check if relation word appears
    std::string lower_rel = relation;
    std::transform(lower_rel.begin(), lower_rel.end(), lower_rel.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    indicators = {lower_rel};
  }

  for (const auto &ind : indicators) {
    if (lower_text.find(ind) != std::string::npos) {
      return true;
    }
  }

  return false;
}

} // namespace Verification
} // namespace NeuroForge
