#pragma once

/**
 * @file CuriosityNavigator.h
 * @brief Curiosity-driven autonomous navigation for web exploration
 *
 * Part of the Autonomous Internet Grounding system - Phase 3.
 * Implements intrinsic motivation-based navigation policy.
 */

#include "core/Curiosity/ExplorationRequest.h"
#include "core/LanguageSystem.h"
#include "core/RelationGate.h"
#include "core/Types.h"
#include "perception/LivePerceptionLoop.h"
#include "vision/BrowserVision.h"
#include <chrono>
#include <functional>
#include <optional>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "core/DeterministicRng.h"

namespace NeuroForge {
namespace Navigation {

/**
 * @brief A link candidate for exploration
 */
struct LinkCandidate {
  std::string url;                       ///< Target URL
  std::string anchor_text;               ///< Link text
  float novelty_score = 0.0f;            ///< How novel is this link
  float uncertainty_score = 0.0f;        ///< How uncertain are we about it
  float curiosity_score = 0.0f;          ///< Combined curiosity score
  float knowledge_gain_potential = 0.0f; ///< Expected information gain
  int depth = 0;                         ///< Depth from starting page
  std::string source_url;                ///< Page where link was found
  std::chrono::system_clock::time_point discovered_at;
};

/**
 * @brief A concept/topic for exploration
 */
struct ExplorationGoal {
  std::string topic;                  ///< Topic to explore
  float priority = 1.0f;              ///< Goal priority
  int min_relations = 5;              ///< Minimum relations to learn
  int learned_relations = 0;          ///< Relations learned so far
  std::vector<std::string> seed_urls; ///< Starting URLs
  std::chrono::system_clock::time_point created_at;
  bool is_complete = false;
};

/**
 * @brief Navigation action to take
 */
struct NavigationAction {
  enum class Type {
    Navigate, ///< Navigate to a URL
    Click,    ///< Click on an element
    Scroll,   ///< Scroll the page
    Extract,  ///< Extract content
    Wait,     ///< Wait/observe
    Back,     ///< Go back
    NewGoal   ///< Switch to new exploration goal
  };

  Type type;
  std::string target;      ///< URL or element selector
  float confidence = 0.0f; ///< Action confidence
  std::string rationale;   ///< Why this action
};

/**
 * @brief Configuration for CuriosityNavigator
 */
struct NavigatorConfig {
  // Curiosity parameters
  float novelty_weight = 0.4f;        ///< Weight for novelty in curiosity
  float uncertainty_weight = 0.3f;    ///< Weight for uncertainty
  float knowledge_gain_weight = 0.3f; ///< Weight for knowledge gain
  float curiosity_threshold = 0.3f;   ///< Minimum curiosity to explore

  // Exploration parameters
  int max_depth = 3;                ///< Maximum link depth
  int max_queue_size = 100;         ///< Maximum links to queue
  int max_pages_per_goal = 20;      ///< Max pages per goal
  float exploration_epsilon = 0.1f; ///< Random exploration probability

  // Novelty calculation
  float novelty_decay = 0.95f;   ///< Decay for seen concepts
  int novelty_window_size = 100; ///< Recent concepts to track

  // Knowledge gain estimation
  float relation_bonus = 0.1f; ///< Score bonus per potential relation
  float concept_bonus = 0.05f; ///< Score bonus per new concept

  // Safety
  std::vector<std::string> allowed_domains = {"wikipedia.org", "britannica.com",
                                              "simple.wikipedia.org"};
  int rate_limit_ms = 2000; ///< Minimum ms between navigations
};

/**
 * @brief Statistics for CuriosityNavigator
 */
struct NavigatorStats {
  std::size_t pages_visited = 0;
  std::size_t links_discovered = 0;
  std::size_t links_explored = 0;
  std::size_t links_skipped = 0;
  std::size_t concepts_encountered = 0;
  std::size_t relations_learned = 0;
  std::size_t goals_completed = 0;
  float avg_curiosity_score = 0.0f;
  float avg_knowledge_gain = 0.0f;
};

/**
 * @brief Curiosity-driven navigation policy for autonomous web exploration
 *
 * Uses intrinsic motivation signals to:
 * 1. Score links by novelty and uncertainty
 * 2. Estimate potential knowledge gain
 * 3. Select actions that maximize learning
 */
class CuriosityNavigator {
public:
  using ActionCallback = std::function<void(const NavigationAction &)>;
  using LinkCallback = std::function<void(const LinkCandidate &)>;

  /**
   * @brief Construct a new CuriosityNavigator
   *
   * @param language_system LanguageSystem for concept tracking
   * @param relation_gates RelationGateManager for knowledge state
   * @param config Configuration parameters
   */
  CuriosityNavigator(Core::LanguageSystem *language_system,
                     Core::RelationGateManager *relation_gates = nullptr,
                     const NavigatorConfig &config = {});

  ~CuriosityNavigator();

  // ========== Lifecycle ==========

  /**
   * @brief Initialize the navigator
   */
  bool initialize();

  /**
   * @brief Start autonomous navigation
   */
  void start();

  /**
   * @brief Stop navigation
   */
  void stop();

  /**
   * @brief Check if running
   */
  bool isRunning() const { return is_running_; }

  // ========== Goal Management ==========

  /**
   * @brief Add an exploration goal
   * @param topic Topic to explore
   * @param priority Goal priority
   * @param seed_urls Starting URLs (optional)
   */
  void addGoal(const std::string &topic, float priority = 1.0f,
               const std::vector<std::string> &seed_urls = {});

  /**
   * @brief Get current active goal
   */
  std::optional<ExplorationGoal> getCurrentGoal() const;

  /**
   * @brief Get all goals
   */
  std::vector<ExplorationGoal> getGoals() const;

  /**
   * @brief Clear all goals
   */
  void clearGoals();

  // ========== Link Processing ==========

  /**
   * @brief Process discovered links from a page
   * @param links Vector of (url, anchor_text) pairs
   * @param source_url The page where links were found
   */
  void
  processLinks(const std::vector<std::pair<std::string, std::string>> &links,
               const std::string &source_url);

  /**
   * @brief Record that a page was visited with its concepts
   * @param url Visited URL
   * @param concepts Concepts found on page
   */
  void recordPageVisit(const std::string &url,
                       const std::vector<std::string> &concepts);

  /**
   * @brief Process visual analysis of the page
   * @param analysis Visual elements found
   */
  void processVisuals(const Vision::VisualPageAnalysis &analysis);

  /**
   * @brief Record learned relations
   * @param count Number of relations learned
   */
  void recordRelationsLearned(int count);

  // ========== Action Selection ==========

  /**
   * @brief Get next navigation action
   * @return Next action to take
   */
  NavigationAction getNextAction();

  /**
   * @brief Get top-k link candidates by curiosity score
   * @param k Number of candidates
   * @return Vector of top candidates
   */
  std::vector<LinkCandidate> getTopCandidates(int k = 5) const;

  void submitRequest(const Core::Curiosity::ExplorationRequest &req);
  bool hasPendingRequests() const;
  Core::Curiosity::ExplorationRequest getNextRequest();
  std::string
  generateQueryFromGoal(const Core::Curiosity::CuriosityGoal &goal) const;

  // ========== Scoring ==========

  /**
   * @brief Calculate novelty score for text/concept
   * @param text Text to score
   * @return Novelty score [0, 1]
   */
  float calculateNovelty(const std::string &text) const;

  /**
   * @brief Calculate uncertainty score for a URL
   * @param url URL to score
   * @return Uncertainty score [0, 1]
   */
  float calculateUncertainty(const std::string &url) const;

  /**
   * @brief Estimate knowledge gain potential
   * @param anchor_text Link text
   * @return Knowledge gain estimate [0, 1]
   */
  float estimateKnowledgeGain(const std::string &anchor_text) const;

  /**
   * @brief Calculate combined curiosity score
   * @param candidate Link candidate
   * @return Curiosity score [0, 1]
   */
  float calculateCuriosity(const LinkCandidate &candidate) const;

  // ========== Callbacks ==========

  void setActionCallback(ActionCallback callback) {
    action_callback_ = std::move(callback);
  }
  void setLinkCallback(LinkCallback callback) {
    link_callback_ = std::move(callback);
  }

  // ========== Configuration ==========

  void setConfig(const NavigatorConfig &config) { config_ = config; }
  NavigatorConfig getConfig() const { return config_; }

  // ========== Statistics ==========

  NavigatorStats getStatistics() const;
  void resetStatistics();

private:
  // Core references
  Core::LanguageSystem *language_system_;
  Core::RelationGateManager *relation_gates_;
  NavigatorConfig config_;

  // State
  bool is_running_ = false;
  bool is_initialized_ = false;

  // Goal management
  std::vector<ExplorationGoal> goals_;
  std::size_t current_goal_index_ = 0;
  mutable std::mutex goal_mutex_;

  // Link queue (priority queue by curiosity score)
  std::vector<LinkCandidate> link_queue_;
  mutable std::mutex queue_mutex_;

  struct RequestComparator {
    bool operator()(const Core::Curiosity::ExplorationRequest &a,
                    const Core::Curiosity::ExplorationRequest &b) const {
      return a.goal.priority < b.goal.priority;
    }
  };

  std::priority_queue<Core::Curiosity::ExplorationRequest,
                      std::vector<Core::Curiosity::ExplorationRequest>,
                      RequestComparator>
      request_queue_;
  mutable std::mutex request_mutex_;

  // Visited tracking
  std::unordered_set<std::string> visited_urls_;
  std::unordered_map<std::string, int> concept_counts_;
  std::vector<std::string> recent_concepts_; // Ring buffer
  std::size_t concept_ring_index_ = 0;
  mutable std::mutex history_mutex_;

  // Rate limiting
  std::chrono::steady_clock::time_point last_navigation_;

  // Random number generator
  mutable std::mt19937 rng_{NeuroForge::Core::DeterministicRng::seedFor("CuriosityNavigator")};

  // Statistics
  mutable NavigatorStats stats_;
  mutable std::mutex stats_mutex_;

  // Callbacks
  ActionCallback action_callback_;
  LinkCallback link_callback_;

  // ========== Internal Methods ==========

  /**
   * @brief Check if URL is in allowed domains
   */
  bool isAllowedDomain(const std::string &url) const;

  /**
   * @brief Check if we've visited this URL
   */
  bool isVisited(const std::string &url) const;

  /**
   * @brief Check if rate limit allows navigation
   */
  bool canNavigateNow() const;

  /**
   * @brief Record navigation for rate limiting
   */
  void recordNavigation();

  /**
   * @brief Update novelty tracking with new concept
   */
  void updateNoveltyTracking(const std::string &topic_concept);

  /**
   * @brief Select link with epsilon-greedy policy
   */
  LinkCandidate selectLink();

  /**
   * @brief Re-sort link queue by curiosity score
   */
  void resortQueue();

  /**
   * @brief Generate Wikipedia URL for topic
   */
  std::string generateWikipediaUrl(const std::string &topic) const;

  /**
   * @brief Tokenize text into words
   */
  std::vector<std::string> tokenize(const std::string &text) const;
};

} // namespace Navigation
} // namespace NeuroForge
