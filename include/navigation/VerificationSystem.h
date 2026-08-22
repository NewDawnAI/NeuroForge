#pragma once

#include "core/EpistemicValueModel.h"
#include "core/RelationGate.h"
#include "core/SourceClassifier.h"
#include "core/VerificationCostModel.h"


#include <algorithm>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Navigation {

/**
 * @brief Phase 18: A fact verification goal
 */
struct VerificationGoal {
  std::string subject;
  std::string predicate;
  std::string object;
  std::string original_source_url;
  std::set<Core::DomainClass> existing_classes;
  int attempt_count = 0;
  int max_attempts = 2;

  std::size_t subject_token_id = 0;
  std::size_t relation_token_id = 0;
  std::size_t object_token_id = 0;

  // Phase 20b: Cost-based scoring
  float score = 0.0f;

  bool isValid() const { return subject_token_id != 0; }
  bool hasAttemptsRemaining() const { return attempt_count < max_attempts; }
};

/**
 * @brief Phase 18: Verification Query Generator
 */
class VerificationQueryGenerator {
public:
  static std::string generateQuery(const VerificationGoal &goal) {
    std::ostringstream ss;
    if (!goal.subject.empty()) {
      ss << "\"" << goal.subject << "\" ";
    }
    if (!goal.predicate.empty()) {
      ss << "\"" << goal.predicate << "\" ";
    }
    if (!goal.object.empty()) {
      ss << "\"" << goal.object << "\"";
    }
    ss << " (site:arxiv.org OR site:britannica.com OR site:nature.com)";
    return ss.str();
  }
};

/**
 * @brief Phase 20b: Verification candidate with score
 */
struct ScoredCandidate {
  const Core::RelationTriple *triple;
  float score;
};

/**
 * @brief Phase 18 + 20b: Select a provisional fact for verification
 *
 * Phase 20b enhancement: Ranks by value/cost ratio instead of just confidence.
 */
class VerificationSelector {
public:
  /**
   * @brief Select the best provisional fact for verification (Phase 20b:
   * cost-aware)
   */
  static std::optional<VerificationGoal>
  selectForVerification(const std::vector<Core::RelationTriple> &relations) {

    std::vector<ScoredCandidate> candidates;

    for (const auto &rel : relations) {
      if (!rel.is_provisional)
        continue;
      if (rel.domain_classes.size() >= 2)
        continue;

      // Phase 20b: Calculate value/cost ratio
      auto cost_estimate = Core::VerificationCostModel::estimateCost(rel);
      float value = Core::EpistemicValueModel::estimateValue(rel);
      float score = value / std::max(cost_estimate.cost, 0.1f);

      candidates.push_back({&rel, score});
    }

    if (candidates.empty()) {
      return std::nullopt;
    }

    // Sort by score (highest first)
    std::sort(candidates.begin(), candidates.end(),
              [](const ScoredCandidate &a, const ScoredCandidate &b) {
                return a.score > b.score;
              });

    // Select best candidate
    const Core::RelationTriple *best = candidates[0].triple;
    float best_score = candidates[0].score;

    VerificationGoal goal;
    goal.subject_token_id = best->subject_token_id;
    goal.relation_token_id = best->relation_token_id;
    goal.object_token_id = best->object_token_id;
    goal.original_source_url = best->source_url;
    goal.existing_classes = best->domain_classes;
    goal.subject = "token_" + std::to_string(best->subject_token_id);
    goal.predicate = "token_" + std::to_string(best->relation_token_id);
    goal.object = "token_" + std::to_string(best->object_token_id);
    goal.score = best_score;

    return goal;
  }

  /**
   * @brief Select top N candidates for verification (Phase 20b)
   */
  static std::vector<VerificationGoal>
  selectTopCandidates(const std::vector<Core::RelationTriple> &relations,
                      std::size_t max_candidates = 3) {

    std::vector<ScoredCandidate> scored;

    for (const auto &rel : relations) {
      if (!rel.is_provisional)
        continue;
      if (rel.domain_classes.size() >= 2)
        continue;

      auto cost_estimate = Core::VerificationCostModel::estimateCost(rel);
      float value = Core::EpistemicValueModel::estimateValue(rel);
      float score = value / std::max(cost_estimate.cost, 0.1f);

      scored.push_back({&rel, score});
    }

    std::sort(scored.begin(), scored.end(),
              [](const ScoredCandidate &a, const ScoredCandidate &b) {
                return a.score > b.score;
              });

    if (scored.size() > max_candidates) {
      scored.resize(max_candidates);
    }

    std::vector<VerificationGoal> result;
    for (const auto &c : scored) {
      VerificationGoal goal;
      goal.subject_token_id = c.triple->subject_token_id;
      goal.relation_token_id = c.triple->relation_token_id;
      goal.object_token_id = c.triple->object_token_id;
      goal.original_source_url = c.triple->source_url;
      goal.existing_classes = c.triple->domain_classes;
      goal.subject = "token_" + std::to_string(c.triple->subject_token_id);
      goal.predicate = "token_" + std::to_string(c.triple->relation_token_id);
      goal.object = "token_" + std::to_string(c.triple->object_token_id);
      goal.score = c.score;
      result.push_back(goal);
    }

    return result;
  }
};

} // namespace Navigation
} // namespace NeuroForge
