#pragma once

/**
 * @file LanguageAcquisitionLoop.h
 * @brief Autonomous language learning from web content
 *
 * Part of the Autonomous Internet Grounding system - Phase 5.
 * Connects Perception, Language, and Verification for true language learning.
 */

#include "core/LanguageSystem.h"
#include "core/RelationGate.h"
#include "navigation/CuriosityNavigator.h"
#include "perception/LivePerceptionLoop.h"
#include "verification/GroundingVerifier.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace NeuroForge {
namespace Language {

/**
 * @brief Statistics for language acquisition
 */
struct AcquisitionStats {
  std::size_t pages_processed = 0;
  std::size_t tokens_learned = 0;
  std::size_t relations_formed = 0;
  std::size_t relations_verified = 0;
  std::size_t contradictions_found = 0;
  std::size_t vocabulary_size = 0;
  float avg_token_confidence = 0.0f;
  float avg_relation_confidence = 0.0f;
  std::chrono::steady_clock::time_point start_time;
  std::chrono::steady_clock::time_point last_learn_time;
};

/**
 * @brief Configuration for LanguageAcquisitionLoop
 */
struct AcquisitionConfig {
  // Learning parameters
  float min_token_confidence = 0.3f; ///< Minimum confidence to create token
  float min_relation_confidence =
      0.5f;                         ///< Minimum confidence to store relation
  int min_occurrences_to_learn = 2; ///< Occurrences before learning
  int max_new_tokens_per_page = 2000;
  int max_tokenization_chars_per_page = 20000;
  bool enable_direct_tokenization = false;

  // Exploration parameters
  int max_pages_per_session = 50;   ///< Maximum pages per learning session
  float curiosity_threshold = 0.4f; ///< Minimum curiosity to explore

  // Verification parameters
  bool verify_before_storing = true;  ///< Verify relations before storing
  int min_sources_for_confidence = 2; ///< Sources needed for high confidence

  // Content filtering
  std::vector<std::string> seed_topics = {"animal", "science", "nature"};
  std::vector<std::string> allowed_domains = {
      "wikipedia.org", "simple.wikipedia.org", "britannica.com"};

  // Update cadence
  int update_interval_ms = 100; ///< Processing update interval
};

/**
 * @brief Orchestrates language learning from web content
 *
 * The LanguageAcquisitionLoop connects:
 * 1. CuriosityNavigator - decides what to explore
 * 2. LivePerceptionLoop - extracts text and entities
 * 3. LanguageSystem - learns tokens and vocabulary
 * 4. GroundingVerifier - verifies learned knowledge
 *
 * Flow: Navigate → Extract → Learn → Verify → Navigate (repeat)
 */
class LanguageAcquisitionLoop {
public:
  using LearnCallback =
      std::function<void(const std::string &token, float confidence)>;
  using VerifyCallback =
      std::function<void(const std::string &relation, bool verified)>;

  /**
   * @brief Construct the language acquisition loop
   *
   * @param language_system LanguageSystem for token/vocabulary management
   * @param perception LivePerceptionLoop for content extraction
   * @param navigator CuriosityNavigator for exploration (optional)
   * @param verifier GroundingVerifier for fact checking (optional)
   * @param config Configuration parameters
   */
  LanguageAcquisitionLoop(Core::LanguageSystem *language_system,
                          Perception::LivePerceptionLoop *perception,
                          Navigation::CuriosityNavigator *navigator = nullptr,
                          Verification::GroundingVerifier *verifier = nullptr,
                          const AcquisitionConfig &config = {});

  ~LanguageAcquisitionLoop();

  // ========== Lifecycle ==========

  /**
   * @brief Initialize the acquisition loop
   */
  bool initialize();

  /**
   * @brief Start autonomous learning
   */
  void start();

  /**
   * @brief Stop learning
   */
  void stop();

  /**
   * @brief Check if running
   */
  bool isRunning() const { return is_running_; }

  // ========== Manual Processing ==========

  /**
   * @brief Process web content manually (for testing/integration)
   *
   * @param text Raw text content
   * @param source_url Source URL for tracking
   */
  void processWebContent(const std::string &text,
                         const std::string &source_url);

  /**
   * @brief Learn from extracted entities
   *
   * @param entities Extracted entities from perception
   * @param source_url Source URL
   */
  void
  learnFromEntities(const std::vector<Perception::ExtractedEntity> &entities,
                    const std::string &source_url);

  /**
   * @brief Learn from candidate relations
   *
   * @param relations Candidate relations
   * @param source_url Source URL
   */
  void learnFromRelations(
      const std::vector<Perception::CandidateRelation> &relations,
      const std::string &source_url);

  /**
   * @brief Verify pending knowledge
   */
  void verifyPendingKnowledge();

  // ========== Exploration Goals ==========

  /**
   * @brief Add a topic to explore for language learning
   *
   * @param topic Topic string (e.g., "mammals", "physics")
   * @param priority Exploration priority
   */
  void addLearningTopic(const std::string &topic, float priority = 1.0f);

  /**
   * @brief Get topics with low knowledge confidence
   */
  std::vector<std::string> getLowConfidenceTopics(int top_k = 5) const;

  // ========== Callbacks ==========

  void setLearnCallback(LearnCallback callback) {
    learn_callback_ = std::move(callback);
  }

  void setVerifyCallback(VerifyCallback callback) {
    verify_callback_ = std::move(callback);
  }

  // ========== Configuration ==========

  void setConfig(const AcquisitionConfig &config) { config_ = config; }
  AcquisitionConfig getConfig() const { return config_; }

  // ========== Statistics ==========

  AcquisitionStats getStatistics() const;
  void resetStatistics();

  /**
   * @brief Get current vocabulary (learned tokens)
   */
  std::vector<std::string> getLearnedVocabulary() const;

  /**
   * @brief Get token confidence
   */
  float getTokenConfidence(const std::string &token) const;

private:
  // Core components
  Core::LanguageSystem *language_system_;
  Perception::LivePerceptionLoop *perception_;
  Navigation::CuriosityNavigator *navigator_;
  Verification::GroundingVerifier *verifier_;
  AcquisitionConfig config_;

  // State
  std::atomic<bool> is_running_{false};
  std::atomic<bool> is_initialized_{false};

  // Learning tracking
  std::unordered_map<std::string, int> token_occurrences_;
  std::unordered_map<std::string, float> token_confidence_;
  std::vector<Perception::CandidateRelation> pending_verifications_;
  mutable std::mutex learn_mutex_;

  // Statistics
  mutable AcquisitionStats stats_;
  mutable std::mutex stats_mutex_;

  // Callbacks
  LearnCallback learn_callback_;
  VerifyCallback verify_callback_;

  // ========== Internal Methods ==========

  /**
   * @brief Main learning loop (runs in background)
   */
  void learningLoop();

  /**
   * @brief Process a single page
   */
  void processPage(const std::string &url);

  /**
   * @brief Extract and learn tokens from text
   */
  void extractAndLearnTokens(const std::string &text,
                             const std::string &source_url);

  /**
   * @brief Convert entity to token in LanguageSystem
   */
  bool createToken(const Perception::ExtractedEntity &entity,
                   const std::string &source_url);

  /**
   * @brief Convert relation to structured knowledge
   */
  bool createRelation(const Perception::CandidateRelation &relation,
                      const std::string &source_url);

  /**
   * @brief Update token confidence based on occurrences
   */
  void updateTokenConfidence(const std::string &token);

  /**
   * @brief Check if token should be learned (meets threshold)
   */
  bool shouldLearnToken(const std::string &token) const;
};

} // namespace Language
} // namespace NeuroForge
