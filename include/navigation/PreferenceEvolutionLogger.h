#pragma once

/**
 * @file PreferenceEvolutionLogger.h
 * @brief Log preference evolution during autonomous browsing
 *
 * Tracks what kind of mind NeuroForge is becoming:
 * - Topic frequency over time
 * - Preference vector drift
 * - Exploration entropy
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Navigation {

/**
 * @brief Topic visit record
 */
struct TopicVisit {
  std::string topic;
  std::string domain;
  std::uint64_t timestamp_ms = 0;
  float novelty_score = 0.0f;
  float curiosity_score = 0.0f;
};

/**
 * @brief Preference evolution snapshot
 */
struct PreferenceSnapshot {
  std::uint64_t timestamp_ms = 0;
  int pages_visited = 0;
  std::map<std::string, int> topic_counts;
  std::map<std::string, int> domain_counts;
  float exploration_entropy = 0.0f;
  float avg_curiosity_score = 0.0f;
};

/**
 * @brief Preference Evolution Logger
 */
class PreferenceEvolutionLogger {
public:
  PreferenceEvolutionLogger() = default;

  /**
   * @brief Log a topic visit
   */
  void logVisit(const std::string &topic, const std::string &domain,
                float novelty = 0.0f, float curiosity = 0.0f) {
    TopicVisit visit;
    visit.topic = topic;
    visit.domain = domain;
    visit.timestamp_ms = getCurrentTimeMs();
    visit.novelty_score = novelty;
    visit.curiosity_score = curiosity;

    visits_.push_back(visit);
    topic_counts_[topic]++;
    domain_counts_[domain]++;

    total_curiosity_ += curiosity;
    pages_visited_++;
  }

  /**
   * @brief Take a preference snapshot
   */
  PreferenceSnapshot snapshot() {
    PreferenceSnapshot snap;
    snap.timestamp_ms = getCurrentTimeMs();
    snap.pages_visited = pages_visited_;
    snap.topic_counts = topic_counts_;
    snap.domain_counts = domain_counts_;
    snap.exploration_entropy = calculateEntropy();
    snap.avg_curiosity_score =
        pages_visited_ > 0 ? total_curiosity_ / pages_visited_ : 0.0f;

    snapshots_.push_back(snap);
    return snap;
  }

  /**
   * @brief Get top topics by frequency
   */
  std::vector<std::pair<std::string, int>> topTopics(int n = 10) const {
    std::vector<std::pair<std::string, int>> sorted;
    for (const auto &p : topic_counts_) {
      sorted.push_back(p);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });
    if (sorted.size() > static_cast<size_t>(n)) {
      sorted.resize(n);
    }
    return sorted;
  }

  /**
   * @brief Get top domains by frequency
   */
  std::vector<std::pair<std::string, int>> topDomains(int n = 10) const {
    std::vector<std::pair<std::string, int>> sorted;
    for (const auto &p : domain_counts_) {
      sorted.push_back(p);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });
    if (sorted.size() > static_cast<size_t>(n)) {
      sorted.resize(n);
    }
    return sorted;
  }

  /**
   * @brief Calculate exploration entropy (higher = more diverse)
   */
  float calculateEntropy() const {
    if (pages_visited_ == 0)
      return 0.0f;

    float entropy = 0.0f;
    for (const auto &p : topic_counts_) {
      float prob = static_cast<float>(p.second) / pages_visited_;
      if (prob > 0) {
        entropy -= prob * std::log2(prob);
      }
    }
    return entropy;
  }

  /**
   * @brief Print summary to console
   */
  void printSummary() const {
    std::cout << "\n=== Preference Evolution Summary ===" << std::endl;
    std::cout << "Pages visited: " << pages_visited_ << std::endl;
    std::cout << "Unique topics: " << topic_counts_.size() << std::endl;
    std::cout << "Unique domains: " << domain_counts_.size() << std::endl;
    std::cout << "Exploration entropy: " << calculateEntropy() << std::endl;
    std::cout << "Avg curiosity score: "
              << (pages_visited_ > 0 ? total_curiosity_ / pages_visited_ : 0)
              << std::endl;

    std::cout << "\nTop Topics:" << std::endl;
    for (const auto &p : topTopics(5)) {
      std::cout << "  " << p.first << ": " << p.second << std::endl;
    }

    std::cout << "\nTop Domains:" << std::endl;
    for (const auto &p : topDomains(5)) {
      std::cout << "  " << p.first << ": " << p.second << std::endl;
    }
  }

  /**
   * @brief Export to CSV
   */
  void exportCSV(const std::string &filename) const {
    std::ofstream f(filename);
    if (!f)
      return;

    f << "timestamp_ms,topic,domain,novelty,curiosity\n";
    for (const auto &v : visits_) {
      f << v.timestamp_ms << "," << v.topic << "," << v.domain << ","
        << v.novelty_score << "," << v.curiosity_score << "\n";
    }
  }

  int pagesVisited() const { return pages_visited_; }
  std::size_t uniqueTopics() const { return topic_counts_.size(); }
  std::size_t uniqueDomains() const { return domain_counts_.size(); }

private:
  std::uint64_t getCurrentTimeMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
  }

  std::vector<TopicVisit> visits_;
  std::vector<PreferenceSnapshot> snapshots_;
  std::map<std::string, int> topic_counts_;
  std::map<std::string, int> domain_counts_;
  int pages_visited_ = 0;
  float total_curiosity_ = 0.0f;
};

} // namespace Navigation
} // namespace NeuroForge
