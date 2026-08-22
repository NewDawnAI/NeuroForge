#pragma once

#include "core/ReasoningTrace.h"
#include "core/SourceClassifier.h"

#include <set>
#include <string>
#include <vector>

namespace NeuroForge {
namespace Navigation {

/**
 * @brief Phase 18: Search Link Filter
 *
 * Selects the best verification URL from search results that is
 * independent of the fact's existing sources.
 *
 * Prevents "Echo Chambers" - e.g., Wikipedia citing Wikipedia.
 */
class SearchLinkFilter {
public:
  /**
   * @brief Select the best independent candidate URL for verification
   *
   * @param search_results List of URLs from a search query
   * @param existing_classes Domain classes already seen for this fact
   * @return The first URL from a different domain class, or empty if none
   */
  static std::string
  selectBestCandidate(const std::vector<std::string> &search_results,
                      const std::set<Core::DomainClass> &existing_classes) {

    for (const auto &url : search_results) {
      // Classify the candidate URL
      Core::DomainClass candidate_class = Core::classifySource(url);

      // Skip unknown sources
      if (candidate_class == Core::DomainClass::Unknown) {
        continue;
      }

      // Check if this class is independent (not already seen)
      if (existing_classes.find(candidate_class) == existing_classes.end()) {
        return url; // Found an independent source!
      }
    }

    return ""; // No independent sources found
  }

  /**
   * @brief Prefer high-trust sources (Academic, Government) over others
   */
  static std::string selectHighTrustCandidate(
      const std::vector<std::string> &search_results,
      const std::set<Core::DomainClass> &existing_classes) {

    std::string best_candidate;
    bool found_high_trust = false;

    for (const auto &url : search_results) {
      Core::DomainClass candidate_class = Core::classifySource(url);

      if (candidate_class == Core::DomainClass::Unknown) {
        continue;
      }

      // Skip if already have this class
      if (existing_classes.find(candidate_class) != existing_classes.end()) {
        continue;
      }

      // Prefer high-trust sources
      if (Core::isHighTrustClass(candidate_class)) {
        return url; // Immediate return for high-trust
      }

      // Keep first independent as fallback
      if (best_candidate.empty()) {
        best_candidate = url;
      }
    }

    return best_candidate;
  }
};

} // namespace Navigation
} // namespace NeuroForge
