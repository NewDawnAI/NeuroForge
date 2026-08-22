#pragma once

/**
 * @file GroundingVerifier.h
 * @brief Anti-hallucination system via cross-source verification
 *
 * Part of the Autonomous Internet Grounding system - Phase 4.
 * Verifies learned relations by checking consistency across multiple sources.
 */

#include "core/LanguageSystem.h"
#include "core/RelationGate.h"
#include "core/Types.h"
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace NeuroForge {
namespace Verification {

/**
 * @brief Evidence for a relation from a specific source
 */
struct RelationEvidence {
  std::string source_url;    ///< Where evidence was found
  std::string source_text;   ///< Original text containing relation
  float confidence = 0.0f;   ///< Extraction confidence
  float transe_score = 0.0f; ///< TransE plausibility
  bool supports = true;      ///< True if supports, false if contradicts
  std::chrono::system_clock::time_point found_at;
};

/**
 * @brief A hypothesis about a relation that needs verification
 */
struct RelationHypothesis {
  std::size_t subject_id;
  std::size_t relation_id;
  std::size_t object_id;
  std::string subject_text;
  std::string relation_text;
  std::string object_text;

  // Evidence tracking
  std::vector<RelationEvidence> supporting_evidence;
  std::vector<RelationEvidence> contradicting_evidence;

  // Verification state
  enum class State {
    Unverified,   ///< No cross-source evidence yet
    Pending,      ///< Actively seeking verification
    Verified,     ///< Multiple sources agree
    Contradicted, ///< Sources disagree
    Rejected      ///< Strong evidence against
  };
  State state = State::Unverified;

  // Scores
  float verification_score = 0.0f; ///< Aggregated verification confidence
  float consistency_score = 0.0f;  ///< Cross-source consistency
  int min_sources_required = 2;    ///< Sources needed for verification

  // Timestamps
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point last_checked;

  // Helper methods
  int supportCount() const {
    return static_cast<int>(supporting_evidence.size());
  }
  int contradictCount() const {
    return static_cast<int>(contradicting_evidence.size());
  }
  bool isVerified() const { return state == State::Verified; }
};

/**
 * @brief Configuration for GroundingVerifier
 */
struct VerifierConfig {
  // Verification thresholds
  int min_sources_for_verification = 2; ///< Minimum independent sources
  float min_verification_score = 0.6f;  ///< Minimum score to verify
  float contradiction_threshold = 0.4f; ///< Ratio to mark contradicted

  // Evidence scoring
  float base_evidence_weight = 1.0f;   ///< Base weight per evidence
  float transe_weight = 0.3f;          ///< Weight for TransE score
  float recency_weight = 0.1f;         ///< Weight for recent evidence
  float source_diversity_bonus = 0.2f; ///< Bonus for diverse sources

  // Source quality
  std::unordered_map<std::string, float> source_trust = {
      {"wikipedia.org", 0.9f},
      {"britannica.com", 0.95f},
      {"simple.wikipedia.org", 0.85f}};
  float default_source_trust = 0.5f;

  // Hypothesis management
  int max_hypotheses = 1000;            ///< Maximum tracked hypotheses
  int max_evidence_per_hypothesis = 20; ///< Max evidence items
  float hypothesis_decay_rate = 0.01f;  ///< Decay for unverified hypotheses
  int prune_interval_hours = 24;        ///< Hours between pruning

  // Active verification
  bool enable_active_verification = true; ///< Actively seek verification
  int verification_batch_size = 10;       ///< Hypotheses to verify per batch
};

/**
 * @brief Statistics for GroundingVerifier
 */
struct VerifierStats {
  std::size_t total_hypotheses = 0;
  std::size_t verified_hypotheses = 0;
  std::size_t contradicted_hypotheses = 0;
  std::size_t rejected_hypotheses = 0;
  std::size_t pending_hypotheses = 0;
  std::size_t total_evidence_items = 0;
  float avg_verification_score = 0.0f;
  float avg_sources_per_verified = 0.0f;
};

/**
 * @brief Grounding verification system to prevent hallucination
 *
 * Tracks hypotheses about relations and verifies them by:
 * 1. Collecting evidence from multiple sources
 * 2. Checking for contradictions
 * 3. Computing verification scores
 * 4. Updating relation gate confidence accordingly
 */
class GroundingVerifier {
public:
  using VerificationCallback =
      std::function<void(const RelationHypothesis &, bool verified)>;
  using EvidenceCallback =
      std::function<void(const RelationHypothesis &, const RelationEvidence &)>;

  /**
   * @brief Construct a new GroundingVerifier
   *
   * @param language_system LanguageSystem for token lookup
   * @param relation_gates RelationGateManager to update
   * @param config Configuration parameters
   */
  GroundingVerifier(Core::LanguageSystem *language_system,
                    Core::RelationGateManager *relation_gates = nullptr,
                    const VerifierConfig &config = {});

  ~GroundingVerifier();

  // ========== Lifecycle ==========

  /**
   * @brief Initialize the verifier
   */
  bool initialize();

  // ========== Hypothesis Management ==========

  /**
   * @brief Create a hypothesis for a relation
   * @param subject Subject token text
   * @param relation Relation type text
   * @param object Object token text
   * @param initial_evidence Optional initial evidence
   * @return Hypothesis ID (0 if failed)
   */
  std::size_t
  createHypothesis(const std::string &subject, const std::string &relation,
                   const std::string &object,
                   const RelationEvidence *initial_evidence = nullptr);

  /**
   * @brief Create hypothesis by token IDs
   */
  std::size_t
  createHypothesis(std::size_t subject_id, std::size_t relation_id,
                   std::size_t object_id,
                   const RelationEvidence *initial_evidence = nullptr);

  /**
   * @brief Get hypothesis by ID
   */
  std::optional<RelationHypothesis> getHypothesis(std::size_t id) const;

  /**
   * @brief Get hypothesis for a specific triple
   */
  std::optional<RelationHypothesis> getHypothesis(std::size_t subject_id,
                                                  std::size_t relation_id,
                                                  std::size_t object_id) const;

  /**
   * @brief Get all hypotheses in a state
   */
  std::vector<RelationHypothesis>
  getHypothesesByState(RelationHypothesis::State state) const;

  /**
   * @brief Get hypotheses needing verification
   */
  std::vector<RelationHypothesis> getPendingHypotheses(int limit = 10) const;

  // ========== Evidence Processing ==========

  /**
   * @brief Add supporting evidence for a hypothesis
   * @param hypothesis_id Hypothesis to support
   * @param evidence Evidence to add
   * @return true if successfully added
   */
  bool addEvidence(std::size_t hypothesis_id, const RelationEvidence &evidence);

  /**
   * @brief Add evidence by relation triple
   */
  bool addEvidence(const std::string &subject, const std::string &relation,
                   const std::string &object, const RelationEvidence &evidence);

  /**
   * @brief Add contradicting evidence
   */
  bool addContradiction(std::size_t hypothesis_id,
                        const RelationEvidence &evidence);

  /**
   * @brief Process text and extract evidence for all matching hypotheses
   * @param text Text to analyze
   * @param source_url Source URL
   * @return Number of evidence items found
   */
  int processTextForEvidence(const std::string &text,
                             const std::string &source_url);

  // ========== Verification ==========

  /**
   * @brief Check and update verification status for a hypothesis
   * @param hypothesis_id Hypothesis to check
   * @return Current verification state
   */
  RelationHypothesis::State verifyHypothesis(std::size_t hypothesis_id);

  /**
   * @brief Verify all pending hypotheses
   * @return Number of hypotheses that changed state
   */
  int verifyAllPending();

  /**
   * @brief Compute verification score for a hypothesis
   * @param hypothesis Hypothesis to score
   * @return Verification score [0, 1]
   */
  float computeVerificationScore(const RelationHypothesis &hypothesis) const;

  /**
   * @brief Compute consistency score across evidence
   * @param hypothesis Hypothesis to check
   * @return Consistency score [0, 1]
   */
  float computeConsistencyScore(const RelationHypothesis &hypothesis) const;

  // ========== Relation Gate Integration ==========

  /**
   * @brief Update relation gate based on verification result
   * @param hypothesis Verified hypothesis
   */
  void updateRelationGate(const RelationHypothesis &hypothesis);

  /**
   * @brief Sync all verified hypotheses to relation gates
   * @return Number of gates updated
   */
  int syncToRelationGates();

  // ========== Maintenance ==========

  /**
   * @brief Prune old unverified hypotheses
   * @return Number of hypotheses removed
   */
  std::size_t pruneStaleHypotheses();

  /**
   * @brief Apply decay to unverified hypotheses
   */
  void applyDecay(float delta_time);

  // ========== Callbacks ==========

  void setVerificationCallback(VerificationCallback callback) {
    verification_callback_ = std::move(callback);
  }

  void setEvidenceCallback(EvidenceCallback callback) {
    evidence_callback_ = std::move(callback);
  }

  // ========== Configuration ==========

  void setConfig(const VerifierConfig &config) { config_ = config; }
  VerifierConfig getConfig() const { return config_; }

  // ========== Statistics ==========

  VerifierStats getStatistics() const;
  void resetStatistics();

private:
  // Core references
  Core::LanguageSystem *language_system_;
  Core::RelationGateManager *relation_gates_;
  VerifierConfig config_;

  // State
  bool is_initialized_ = false;

  // Hypothesis storage
  std::vector<RelationHypothesis> hypotheses_;
  std::unordered_map<std::size_t, std::size_t> hypothesis_index_; // id -> index
  std::unordered_map<std::string, std::size_t>
      triple_to_hypothesis_; // "s:r:o" -> id
  std::size_t next_hypothesis_id_ = 1;
  mutable std::mutex hypothesis_mutex_;

  // Statistics
  mutable VerifierStats stats_;
  mutable std::mutex stats_mutex_;

  // Callbacks
  VerificationCallback verification_callback_;
  EvidenceCallback evidence_callback_;

  // Last prune time
  std::chrono::system_clock::time_point last_prune_time_;

  // ========== Internal Methods ==========

  /**
   * @brief Generate key for triple lookup
   */
  std::string makeTripleKey(std::size_t s, std::size_t r, std::size_t o) const;

  /**
   * @brief Get trust score for a source
   */
  float getSourceTrust(const std::string &url) const;

  /**
   * @brief Check if evidence is from a new source
   */
  bool isNewSource(const RelationHypothesis &hypothesis,
                   const std::string &source_url) const;

  /**
   * @brief Count unique sources in evidence
   */
  int countUniqueSources(const RelationHypothesis &hypothesis) const;

  /**
   * @brief Update hypothesis state based on evidence
   */
  void updateHypothesisState(RelationHypothesis &hypothesis);

  /**
   * @brief Update statistics after state change
   */
  void updateStats();

  /**
   * @brief Tokenize text for matching
   */
  std::vector<std::string> tokenize(const std::string &text) const;

  /**
   * @brief Check if text contains relation pattern
   */
  bool textContainsRelation(const std::string &text, const std::string &subject,
                            const std::string &relation,
                            const std::string &object) const;
};

} // namespace Verification
} // namespace NeuroForge
