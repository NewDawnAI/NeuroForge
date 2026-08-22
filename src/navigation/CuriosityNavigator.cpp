#include "navigation/CuriosityNavigator.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <sstream>

namespace NeuroForge {
namespace Navigation {

CuriosityNavigator::CuriosityNavigator(
    Core::LanguageSystem *language_system,
    Core::RelationGateManager *relation_gates, const NavigatorConfig &config)
    : language_system_(language_system), relation_gates_(relation_gates),
      config_(config) {
  last_navigation_ = std::chrono::steady_clock::now();

  // Initialize novelty ring buffer
  recent_concepts_.resize(config_.novelty_window_size, "");
}

CuriosityNavigator::~CuriosityNavigator() { stop(); }

bool CuriosityNavigator::initialize() {
  if (!language_system_) {
    std::cerr << "[CuriosityNavigator] ERROR: LanguageSystem is null\n";
    return false;
  }

  is_initialized_ = true;
  return true;
}

void CuriosityNavigator::start() {
  if (!is_initialized_) {
    if (!initialize()) {
      return;
    }
  }
  is_running_ = true;
}

void CuriosityNavigator::stop() { is_running_ = false; }

static std::string authToString(Core::Curiosity::ExplorationAuthorization a) {
  using A = Core::Curiosity::ExplorationAuthorization;
  switch (a) {
  case A::None:
    return "NONE";
  case A::Diagnostic:
    return "DIAGNOSTIC";
  case A::ContradictionResolution:
    return "CONTRADICTION_RESOLUTION";
  }
  return "NONE";
}

void CuriosityNavigator::submitRequest(
    const Core::Curiosity::ExplorationRequest &req) {
  if (!req.isValid())
    return;
  {
    std::lock_guard<std::mutex> lock(request_mutex_);
    request_queue_.push(req);
  }
  std::cout << "[Curiosity] Authorized " << authToString(req.authorization)
            << " request: subject=" << req.goal.target_subject
            << " predicate=" << req.goal.target_predicate;
  if (!req.goal.object_hint.empty()) {
    std::cout << " object_hint=" << req.goal.object_hint;
  }
  std::cout << " trace_id=" << req.goal.originating_trace_id
            << " priority=" << req.goal.priority << "\n";
}

bool CuriosityNavigator::hasPendingRequests() const {
  std::lock_guard<std::mutex> lock(request_mutex_);
  return !request_queue_.empty();
}

Core::Curiosity::ExplorationRequest CuriosityNavigator::getNextRequest() {
  std::lock_guard<std::mutex> lock(request_mutex_);
  if (request_queue_.empty()) {
    return Core::Curiosity::ExplorationRequest{};
  }
  auto top = request_queue_.top();
  request_queue_.pop();
  return top;
}

std::string CuriosityNavigator::generateQueryFromGoal(
    const Core::Curiosity::CuriosityGoal &goal) const {
  std::string subject = goal.target_subject;
  std::string pred = goal.target_predicate;

  if (pred == "used_for") {
    return "applications of " + subject;
  }
  if (pred == "is") {
    if (goal.object_hint == "useful") {
      return "benefits of " + subject;
    }
    if (!goal.object_hint.empty()) {
      return subject + " " + goal.object_hint;
    }
    return subject + " is";
  }
  if (pred == "is_a") {
    return "definition of " + subject;
  }
  if (pred == "cause" || pred == "caused_by") {
    return "causes of " + subject;
  }

  std::string q = subject + " " + pred;
  if (!goal.object_hint.empty()) {
    q += " " + goal.object_hint;
  }
  return q;
}

// ========== Goal Management ==========

void CuriosityNavigator::addGoal(const std::string &topic, float priority,
                                 const std::vector<std::string> &seed_urls) {
  std::lock_guard<std::mutex> lock(goal_mutex_);

  ExplorationGoal goal;
  goal.topic = topic;
  goal.priority = priority;
  goal.seed_urls = seed_urls;
  goal.created_at = std::chrono::system_clock::now();

  // If no seed URLs provided, generate Wikipedia URL
  if (goal.seed_urls.empty()) {
    goal.seed_urls.push_back(generateWikipediaUrl(topic));
  }

  goals_.push_back(goal);

  // Sort by priority
  std::sort(goals_.begin(), goals_.end(), [](const auto &a, const auto &b) {
    return a.priority > b.priority;
  });
}

std::optional<ExplorationGoal> CuriosityNavigator::getCurrentGoal() const {
  std::lock_guard<std::mutex> lock(goal_mutex_);

  // Find first incomplete goal
  for (const auto &goal : goals_) {
    if (!goal.is_complete) {
      return goal;
    }
  }

  return std::nullopt;
}

std::vector<ExplorationGoal> CuriosityNavigator::getGoals() const {
  std::lock_guard<std::mutex> lock(goal_mutex_);
  return goals_;
}

void CuriosityNavigator::clearGoals() {
  std::lock_guard<std::mutex> lock(goal_mutex_);
  goals_.clear();
  current_goal_index_ = 0;
}

// ========== Link Processing ==========

void CuriosityNavigator::processLinks(
    const std::vector<std::pair<std::string, std::string>> &links,
    const std::string &source_url) {

  if (!is_running_)
    return;

  std::lock_guard<std::mutex> lock(queue_mutex_);

  // Get current depth
  int current_depth = 0;
  for (const auto &candidate : link_queue_) {
    if (candidate.url == source_url) {
      current_depth = candidate.depth;
      break;
    }
  }

  for (const auto &[url, anchor] : links) {
    // Skip if already visited or queued
    if (isVisited(url))
      continue;

    // Skip if not in allowed domains
    if (!isAllowedDomain(url))
      continue;

    // Check depth limit
    if (current_depth + 1 > config_.max_depth)
      continue;

    // Create candidate
    LinkCandidate candidate;
    candidate.url = url;
    candidate.anchor_text = anchor;
    candidate.depth = current_depth + 1;
    candidate.source_url = source_url;
    candidate.discovered_at = std::chrono::system_clock::now();

    // Calculate scores
    candidate.novelty_score = calculateNovelty(anchor);
    candidate.uncertainty_score = calculateUncertainty(url);
    candidate.knowledge_gain_potential = estimateKnowledgeGain(anchor);
    candidate.curiosity_score = calculateCuriosity(candidate);

    // Add to queue if above threshold
    if (candidate.curiosity_score >= config_.curiosity_threshold) {
      link_queue_.push_back(candidate);

      std::lock_guard<std::mutex> stats_lock(stats_mutex_);
      stats_.links_discovered++;

      if (link_callback_) {
        link_callback_(candidate);
      }
    }
  }

  // Trim queue if too large
  if (link_queue_.size() > static_cast<std::size_t>(config_.max_queue_size)) {
    resortQueue();
    link_queue_.resize(config_.max_queue_size);
  }
}

void CuriosityNavigator::recordPageVisit(
    const std::string &url, const std::vector<std::string> &concepts) {
  // Mark as visited
  {
    std::lock_guard<std::mutex> lock(history_mutex_);
    visited_urls_.insert(url);

    for (const auto &topic_concept : concepts) {
      updateNoveltyTracking(topic_concept);
    }
  }

  // Update stats
  {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.pages_visited++;
    stats_.concepts_encountered += concepts.size();
  }

  // Check if current goal is complete
  {
    std::lock_guard<std::mutex> lock(goal_mutex_);
    for (auto &goal : goals_) {
      if (!goal.is_complete && goal.learned_relations >= goal.min_relations) {
        goal.is_complete = true;

        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.goals_completed++;
      }
    }
  }
}

void CuriosityNavigator::recordRelationsLearned(int count) {
  std::lock_guard<std::mutex> lock(goal_mutex_);

  for (auto &goal : goals_) {
    if (!goal.is_complete) {
      goal.learned_relations += count;
      break;
    }
  }

  std::lock_guard<std::mutex> stats_lock(stats_mutex_);
  stats_.relations_learned += count;
}

// ========== Action Selection ==========

NavigationAction CuriosityNavigator::getNextAction() {
  NavigationAction action;

  if (!is_running_) {
    action.type = NavigationAction::Type::Wait;
    action.rationale = "Navigator not running";
    return action;
  }

  // Check rate limit
  if (!canNavigateNow()) {
    action.type = NavigationAction::Type::Wait;
    action.rationale = "Rate limited";
    return action;
  }

  // Check if we have a goal
  auto current_goal = getCurrentGoal();
  if (!current_goal) {
    action.type = NavigationAction::Type::Wait;
    action.rationale = "No exploration goal";
    return action;
  }

  // Check if we should start with seed URL
  {
    std::lock_guard<std::mutex> lock(history_mutex_);
    std::lock_guard<std::mutex> queue_lock(queue_mutex_);

    // If queue is empty and goal has unvisited seed URLs
    if (link_queue_.empty()) {
      for (const auto &seed : current_goal->seed_urls) {
        if (visited_urls_.count(seed) == 0) {
          action.type = NavigationAction::Type::Navigate;
          action.target = seed;
          action.confidence = 1.0f;
          action.rationale = "Starting goal: " + current_goal->topic;

          recordNavigation();

          if (action_callback_) {
            action_callback_(action);
          }

          return action;
        }
      }
    }
  }

  // Select link from queue
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    if (link_queue_.empty()) {
      action.type = NavigationAction::Type::NewGoal;
      action.rationale = "Link queue exhausted";
      return action;
    }

    auto selected = selectLink();

    action.type = NavigationAction::Type::Navigate;
    action.target = selected.url;
    action.confidence = selected.curiosity_score;
    action.rationale =
        "Curiosity: " + std::to_string(selected.curiosity_score) + " (" +
        selected.anchor_text + ")";

    // Remove from queue
    link_queue_.erase(
        std::remove_if(link_queue_.begin(), link_queue_.end(),
                       [&](const auto &c) { return c.url == selected.url; }),
        link_queue_.end());

    recordNavigation();

    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.links_explored++;
    stats_.avg_curiosity_score =
        (stats_.avg_curiosity_score * (stats_.links_explored - 1) +
         selected.curiosity_score) /
        stats_.links_explored;
  }

  if (action_callback_) {
    action_callback_(action);
  }

  return action;
}

std::vector<LinkCandidate> CuriosityNavigator::getTopCandidates(int k) const {
  std::lock_guard<std::mutex> lock(queue_mutex_);

  std::vector<LinkCandidate> candidates = link_queue_;

  // Sort by curiosity score
  std::sort(candidates.begin(), candidates.end(),
            [](const auto &a, const auto &b) {
              return a.curiosity_score > b.curiosity_score;
            });

  if (static_cast<int>(candidates.size()) > k) {
    candidates.resize(k);
  }

  return candidates;
}

// ========== Scoring ==========

float CuriosityNavigator::calculateNovelty(const std::string &text) const {
  auto words = tokenize(text);

  if (words.empty()) {
    return 0.5f; // Neutral novelty
  }

  float total_novelty = 0.0f;

  std::lock_guard<std::mutex> lock(history_mutex_);

  for (const auto &word : words) {
    auto it = concept_counts_.find(word);
    if (it == concept_counts_.end()) {
      // Never seen - high novelty
      total_novelty += 1.0f;
    } else {
      // Decay based on frequency
      float novelty = std::pow(config_.novelty_decay, it->second);
      total_novelty += novelty;
    }
  }

  return total_novelty / words.size();
}

float CuriosityNavigator::calculateUncertainty(const std::string &url) const {
  // Uncertainty is high for unseen URLs, low for familiar domains

  std::lock_guard<std::mutex> lock(history_mutex_);

  // Base uncertainty
  float uncertainty = 0.5f;

  // Lower if we've visited similar URLs
  int similar_count = 0;
  for (const auto &visited : visited_urls_) {
    // Check domain similarity
    if (url.find("wikipedia.org") != std::string::npos &&
        visited.find("wikipedia.org") != std::string::npos) {
      similar_count++;
    }
  }

  // More visits to similar = lower uncertainty
  uncertainty *= std::pow(0.9f, similar_count);

  // But never fully confident
  uncertainty = std::max(0.1f, uncertainty);

  return uncertainty;
}

float CuriosityNavigator::estimateKnowledgeGain(
    const std::string &anchor_text) const {
  auto words = tokenize(anchor_text);

  float gain = 0.0f;

  // Check how many words could form new relations
  for (const auto &word : words) {
    // Check if word is a known concept
    std::size_t token_id;
    if (language_system_ && language_system_->getTokenId(word, token_id)) {
      // Known concept - moderate gain
      gain += config_.concept_bonus;
    } else {
      // Unknown concept - high gain
      gain += config_.concept_bonus * 2.0f;
    }
  }

  // Estimate potential relations based on phrase structure
  // More words = more potential relations
  if (words.size() >= 2) {
    gain += config_.relation_bonus * (words.size() - 1);
  }

  return std::min(1.0f, gain);
}

float CuriosityNavigator::calculateCuriosity(
    const LinkCandidate &candidate) const {
  float curiosity =
      config_.novelty_weight * candidate.novelty_score +
      config_.uncertainty_weight * candidate.uncertainty_score +
      config_.knowledge_gain_weight * candidate.knowledge_gain_potential;

  // Depth penalty - prefer shallower exploration
  float depth_penalty = 1.0f - (candidate.depth * 0.1f);
  depth_penalty = std::max(0.5f, depth_penalty);

  return curiosity * depth_penalty;
}

// ========== Statistics ==========

NavigatorStats CuriosityNavigator::getStatistics() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  return stats_;
}

void CuriosityNavigator::resetStatistics() {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  stats_ = NavigatorStats{};
}

// ========== Internal Methods ==========

bool CuriosityNavigator::isAllowedDomain(const std::string &url) const {
  for (const auto &domain : config_.allowed_domains) {
    if (url.find(domain) != std::string::npos) {
      return true;
    }
  }
  return config_.allowed_domains.empty(); // Allow all if no filter
}

bool CuriosityNavigator::isVisited(const std::string &url) const {
  std::lock_guard<std::mutex> lock(history_mutex_);
  return visited_urls_.count(url) > 0;
}

bool CuriosityNavigator::canNavigateNow() const {
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                     now - last_navigation_)
                     .count();
  return elapsed >= config_.rate_limit_ms;
}

void CuriosityNavigator::recordNavigation() {
  last_navigation_ = std::chrono::steady_clock::now();
}

void CuriosityNavigator::updateNoveltyTracking(
    const std::string &topic_concept) {
  // Update count
  concept_counts_[topic_concept]++;

  // Add to ring buffer
  recent_concepts_[concept_ring_index_] = topic_concept;
  concept_ring_index_ = (concept_ring_index_ + 1) % recent_concepts_.size();
}

LinkCandidate CuriosityNavigator::selectLink() {
  if (link_queue_.empty()) {
    return LinkCandidate{};
  }

  // Epsilon-greedy selection
  std::uniform_real_distribution<float> dist(0.0f, 1.0f);

  if (dist(rng_) < config_.exploration_epsilon) {
    // Random exploration
    std::uniform_int_distribution<std::size_t> idx_dist(0,
                                                        link_queue_.size() - 1);
    return link_queue_[idx_dist(rng_)];
  }

  // Greedy selection - best curiosity score
  resortQueue();
  return link_queue_.front();
}

void CuriosityNavigator::resortQueue() {
  std::sort(link_queue_.begin(), link_queue_.end(),
            [](const auto &a, const auto &b) {
              return a.curiosity_score > b.curiosity_score;
            });
}

std::string
CuriosityNavigator::generateWikipediaUrl(const std::string &topic) const {
  // Convert topic to Wikipedia URL format
  std::string url = "https://en.wikipedia.org/wiki/";

  for (char c : topic) {
    if (c == ' ') {
      url += '_';
    } else if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
      url += c;
    }
  }

  return url;
}

std::vector<std::string>
CuriosityNavigator::tokenize(const std::string &text) const {
  std::vector<std::string> tokens;
  std::string current;

  for (char c : text) {
    if (std::isalnum(static_cast<unsigned char>(c))) {
      current += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    } else if (!current.empty()) {
      if (current.length() >= 3) { // Skip very short words
        tokens.push_back(current);
      }
      current.clear();
    }
  }

  if (!current.empty() && current.length() >= 3) {
    tokens.push_back(current);
  }

  return tokens;
}

void CuriosityNavigator::processVisuals(
    const Vision::VisualPageAnalysis &analysis) {
  if (!is_running_)
    return;

  std::lock_guard<std::mutex> lock(queue_mutex_);

  // Get current depth (approximate, assumed 0 or tracked elsewhere if needed)
  int current_depth = 0; // Simplified for visual elements

  for (const auto &element : analysis.elements) {
    if (element.text.empty())
      continue;

    // Skip low confidence
    if (element.confidence < 0.4f)
      continue;

    // Create candidate from visual element
    LinkCandidate candidate;
    // For visual elements without URL, we might use a special scheme or
    // selector
    candidate.url = "visual://" + element.type + "/" + element.text;

    std::stringstream ss;
    ss << "click:" << element.x + element.width / 2 << ","
       << element.y + element.height / 2;
    candidate.url = ss.str();

    candidate.anchor_text = element.text + " [" + element.type + "]";
    candidate.depth = current_depth;
    candidate.source_url = "visual_analysis"; // Context
    candidate.discovered_at = std::chrono::system_clock::now();

    // Calculate scores
    candidate.novelty_score = calculateNovelty(element.text);
    // Visual elements might have higher uncertainty
    candidate.uncertainty_score = 0.6f;
    candidate.knowledge_gain_potential = estimateKnowledgeGain(element.text);

    // Boost curiosity for interactive elements
    float type_boost = 0.0f;
    if (element.type == "button")
      type_boost = 0.2f;
    if (element.type == "input")
      type_boost = 0.1f;

    candidate.curiosity_score = calculateCuriosity(candidate) + type_boost;

    // Add to queue if interesting
    if (candidate.curiosity_score >= config_.curiosity_threshold) {
      link_queue_.push_back(candidate);

      std::lock_guard<std::mutex> stats_lock(stats_mutex_);
      stats_.links_discovered++;

      if (link_callback_) {
        link_callback_(candidate);
      }
    }
  }

  resortQueue();
}

} // namespace Navigation
} // namespace NeuroForge
