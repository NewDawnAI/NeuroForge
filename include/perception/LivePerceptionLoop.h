#pragma once

/**
 * @file LivePerceptionLoop.h
 * @brief Real-time web content perception pipeline for autonomous grounding
 *
 * Part of the Autonomous Internet Grounding system - Phase 2.
 * Bridges WebSandbox browser frames to the NeuroForge cognitive architecture.
 */

#include "core/LanguageSystem.h"
#include "core/EntityResolver.h"
#include "core/RelationGate.h"
#include "core/Types.h"
#include "sandbox/WebSandbox.h"
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>


namespace NeuroForge {
namespace Perception {

/**
 * @brief Extracted text segment from web page
 */
struct TextSegment {
  std::string text;         ///< Raw text content
  std::string element_type; ///< HTML element type (p, h1, span, etc.)
  float confidence = 1.0f;  ///< Extraction confidence
  int depth = 0;            ///< DOM depth
  std::chrono::system_clock::time_point extracted_at;
};

/**
 * @brief Extracted entity/concept from text
 */
struct ExtractedEntity {
  std::string text;         ///< Entity text
  std::string type;         ///< Entity type (NOUN, VERB, ADJ, etc.)
  std::size_t token_id = 0; ///< Token ID in LanguageSystem (0 if not bound)
  float salience = 0.0f;    ///< Importance score
  int occurrence_count = 1; ///< Times seen on page
};

/**
 * @brief Candidate relation extracted from text
 */
struct CandidateRelation {
  ExtractedEntity subject;
  ExtractedEntity predicate;
  ExtractedEntity object;
  float confidence = 0.0f;     ///< Extraction confidence
  float transe_score = 0.0f;   ///< TransE plausibility score
  std::string source_sentence; ///< Original sentence
  std::string source_url;      ///< Page URL
};

/**
 * @brief Page content snapshot
 */
struct PageSnapshot {
  std::string url;
  std::string title;
  std::vector<TextSegment> segments;
  std::vector<ExtractedEntity> entities;
  std::vector<CandidateRelation> candidate_relations;
  std::chrono::system_clock::time_point captured_at;
  bool is_processed = false;
  bool evidence_accumulated = false;
};

/**
 * @brief Configuration for LivePerceptionLoop
 */
struct PerceptionConfig {
  // Rate limiting
  int max_pages_per_minute = 30;   ///< Maximum pages to process per minute
  int min_page_interval_ms = 2000; ///< Minimum time between page fetches

  // Text extraction
  int max_text_length = 50000; ///< Maximum text to extract per page
  int min_segment_length = 10; ///< Minimum segment length to keep
  std::vector<std::string> skip_elements = {"script", "style", "nav", "footer"};

  // Entity extraction
  int min_entity_length = 2;        ///< Minimum entity length
  int max_entities_per_page = 200;  ///< Maximum entities per page
  float min_entity_salience = 0.1f; ///< Minimum salience to keep

  // Relation extraction
  int max_relations_per_page = 50;      ///< Maximum relations per page
  float min_relation_confidence = 0.3f; ///< Minimum extraction confidence
  float min_transe_score = 0.2f;        ///< Minimum TransE score to create gate
  float min_transe_score_provisional = 0.05f;

  // Safety
  std::vector<std::string> url_allowlist = {
      "wikipedia.org", "britannica.com", "simple.wikipedia.org",
      "dictionary.com", "merriam-webster.com"};
  std::vector<std::string> url_blocklist = {};
  bool require_https = true;

  // Processing
  int queue_max_size = 100; ///< Maximum pending snapshots
  int batch_size = 5;       ///< Entities to bind per cycle
  bool debug_logging = false;
};

/**
 * @brief Statistics for LivePerceptionLoop
 */
struct PerceptionStats {
  std::size_t pages_fetched = 0;
  std::size_t pages_processed = 0;
  std::size_t pages_skipped = 0; ///< Skipped due to rate limit or filter
  std::size_t segments_extracted = 0;
  std::size_t entities_extracted = 0;
  std::size_t entities_bound = 0; ///< Entities bound to tokens
  std::size_t relations_proposed = 0;
  std::size_t relations_created = 0; ///< Gates created
  std::size_t raw_sentences = 0;
  std::size_t normalized_sentences = 0;
  std::size_t parsed_sentences = 0;
  std::size_t relation_candidates_generated = 0;
  std::size_t rejected_empty_sentence = 0;
  std::size_t rejected_unlinked_entity = 0;
  std::size_t rejected_low_confidence = 0;
  std::size_t rejected_low_transe_score = 0;
  std::size_t accepted_provisional = 0;
  std::size_t accepted_confirmed = 0;
  std::size_t transe_scored = 0;
  float transe_min = 0.0f;
  float transe_max = 0.0f;
  float transe_sum = 0.0f;
  std::size_t errors = 0;
  float avg_processing_time_ms = 0.0f;
};

/**
 * @brief Real-time web content perception for autonomous grounding
 *
 * Connects WebSandbox browser to the cognitive architecture:
 * 1. Observes web page content via WebSandbox
 * 2. Extracts text segments and entities
 * 3. Proposes candidate relations
 * 4. Creates relation gates in RelationGateManager
 */
class LivePerceptionLoop {
public:
  using PageCallback = std::function<void(const PageSnapshot &)>;
  using EntityCallback = std::function<void(const ExtractedEntity &)>;
  using RelationCallback =
      std::function<void(const CandidateRelation &, bool created)>;

  /**
   * @brief Construct a new LivePerceptionLoop
   *
   * @param language_system LanguageSystem for token binding
   * @param relation_gates RelationGateManager for creating gates (optional)
   * @param config Configuration parameters
   */
  LivePerceptionLoop(Core::LanguageSystem *language_system,
                     Core::RelationGateManager *relation_gates = nullptr,
                     const PerceptionConfig &config = {});

  ~LivePerceptionLoop();

  // ========== Lifecycle ==========

  /**
   * @brief Initialize the perception loop
   * @return true if successful
   */
  bool initialize();

  /**
   * @brief Start processing (call update() periodically)
   */
  void start();

  /**
   * @brief Stop processing
   */
  void stop();

  /**
   * @brief Check if loop is running
   */
  bool isRunning() const { return is_running_.load(); }

  // ========== Page Ingestion ==========

  /**
   * @brief Submit a URL for fetching (will be rate-limited)
   * @param url URL to fetch
   * @return true if queued, false if rejected
   */
  bool submitUrl(const std::string &url);

  /**
   * @brief Submit raw HTML content directly
   * @param html HTML content
   * @param url Source URL for reference
   * @return true if queued
   */
  bool submitHtml(const std::string &html, const std::string &url);

  /**
   * @brief Submit a pre-extracted text (bypass HTML parsing)
   * @param text Plain text content
   * @param url Source URL for reference
   * @return true if queued
   */
  bool submitText(const std::string &text, const std::string &url);

  // ========== Processing ==========

  /**
   * @brief Process pending snapshots (call periodically)
   * @param delta_time Time since last update
   */
  void update(float delta_time);

  /**
   * @brief Get number of pending snapshots
   */
  std::size_t getPendingCount() const;

  // ========== Callbacks ==========

  void setPageCallback(PageCallback callback) {
    page_callback_ = std::move(callback);
  }
  void setEntityCallback(EntityCallback callback) {
    entity_callback_ = std::move(callback);
  }
  void setRelationCallback(RelationCallback callback) {
    relation_callback_ = std::move(callback);
  }

  // ========== Configuration ==========

  void setConfig(const PerceptionConfig &config) { config_ = config; }
  PerceptionConfig getConfig() const { return config_; }

  // ========== Statistics ==========

  PerceptionStats getStatistics() const;
  void resetStatistics();

  std::vector<Core::EntityResolver::AliasRecord> dumpEntityAliases() const;
  void loadEntityAliases(const std::vector<Core::EntityResolver::AliasRecord> &records);

private:
  struct EntityEvidence {
    std::size_t pages_seen = 0;
    std::size_t total_occurrences = 0;
  };

  // Core references
  Core::LanguageSystem *language_system_;
  Core::RelationGateManager *relation_gates_;
  PerceptionConfig config_;
  mutable Core::EntityResolver entity_resolver_;

  // State
  std::atomic<bool> is_running_{false};
  std::atomic<bool> is_initialized_{false};

  // Rate limiting
  std::chrono::steady_clock::time_point last_page_time_;
  int pages_this_minute_ = 0;
  std::chrono::steady_clock::time_point minute_start_;

  // Queues
  std::queue<PageSnapshot> pending_snapshots_;
  mutable std::mutex queue_mutex_;

  // Statistics
  mutable PerceptionStats stats_;
  mutable std::mutex stats_mutex_;

  // Cross-page entity evidence + stable canonical token cache
  std::unordered_map<std::string, EntityEvidence> entity_evidence_;
  std::unordered_map<std::string, std::size_t> canonical_token_cache_;
  mutable std::mutex evidence_mutex_;

  // Callbacks
  PageCallback page_callback_;
  EntityCallback entity_callback_;
  RelationCallback relation_callback_;

  // Seen URLs (avoid duplicates)
  std::unordered_set<std::string> seen_urls_;
  mutable std::mutex seen_mutex_;

  // ========== Internal Methods ==========

  /**
   * @brief Check if URL is allowed by filters
   */
  bool isUrlAllowed(const std::string &url) const;

  /**
   * @brief Check if rate limit allows fetching
   */
  bool canFetchNow() const;

  /**
   * @brief Record a fetch for rate limiting
   */
  void recordFetch();

  /**
   * @brief Extract text segments from HTML
   */
  std::vector<TextSegment> extractTextFromHtml(const std::string &html) const;

  /**
   * @brief Extract text segments from plain text
   */
  std::vector<TextSegment> extractTextFromPlain(const std::string &text) const;

  /**
   * @brief Extract entities from text segments
   */
  std::vector<ExtractedEntity>
  extractEntities(const std::vector<TextSegment> &segments) const;

  /**
   * @brief Bind entity to LanguageSystem token
   * @return Token ID if bound, std::nullopt otherwise
   */
  std::optional<std::size_t> bindEntityToToken(const ExtractedEntity &entity);

  /**
   * @brief Extract candidate relations from entities and text
   */
  std::vector<CandidateRelation>
  extractRelations(const std::vector<TextSegment> &segments,
                   const std::vector<ExtractedEntity> &entities,
                   const std::string &url) const;

  /**
   * @brief Create relation gate if score is high enough
   * @return true if gate was created
   */
  bool createRelationGate(const CandidateRelation &relation);

  /**
   * @brief Process a single snapshot
   */
  void processSnapshot(PageSnapshot &snapshot);

  void accumulateEntityEvidence(PageSnapshot &snapshot);

  /**
   * @brief Simple tokenization (split on whitespace/punctuation)
   */
  std::vector<std::string> tokenize(const std::string &text) const;

  /**
   * @brief Check if word looks like a content word (not stopword)
   */
  bool isContentWord(const std::string &word) const;

  /**
   * @brief Normalize word (lowercase, trim)
   */
  std::string normalizeWord(const std::string &word) const;
};

} // namespace Perception
} // namespace NeuroForge
