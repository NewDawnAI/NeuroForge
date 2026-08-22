#pragma once

#include <set>
#include <string>


namespace NeuroForge {
namespace Core {

/**
 * @brief Domain class taxonomy for Phase 16 source independence
 *
 * Prevents "Wikipedia citing Wikipedia" from becoming truth by
 * requiring corroboration from different domain classes.
 */
enum class DomainClass {
  Unknown = 0,
  Encyclopedia, // wikipedia.org, britannica.com
  Academic,     // arxiv.org, acm.org, ieee.org, springer.com
  Government,   // .gov domains
  Educational,  // .edu domains
  Blog,         // medium.com, substack.com, dev.to
  News,         // nytimes.com, bbc.com, reuters.com
  Forum,        // reddit.com, stackoverflow.com, quora.com
};

/**
 * @brief Classify a URL into a domain class
 * @param url The source URL to classify
 * @return The domain class for the URL
 */
inline DomainClass classifySource(const std::string &url) {
  // Encyclopedia
  if (url.find("wikipedia.org") != std::string::npos ||
      url.find("britannica.com") != std::string::npos ||
      url.find("encyclopedia.com") != std::string::npos) {
    return DomainClass::Encyclopedia;
  }

  // Academic
  if (url.find("arxiv.org") != std::string::npos ||
      url.find("acm.org") != std::string::npos ||
      url.find("ieee.org") != std::string::npos ||
      url.find("springer.com") != std::string::npos ||
      url.find("nature.com") != std::string::npos ||
      url.find("sciencedirect.com") != std::string::npos ||
      url.find("researchgate.net") != std::string::npos) {
    return DomainClass::Academic;
  }

  // Government
  if (url.find(".gov") != std::string::npos) {
    return DomainClass::Government;
  }

  // Educational
  if (url.find(".edu") != std::string::npos) {
    return DomainClass::Educational;
  }

  // Blog
  if (url.find("medium.com") != std::string::npos ||
      url.find("substack.com") != std::string::npos ||
      url.find("dev.to") != std::string::npos ||
      url.find("towardsdatascience.com") != std::string::npos) {
    return DomainClass::Blog;
  }

  // News
  if (url.find("nytimes.com") != std::string::npos ||
      url.find("bbc.com") != std::string::npos ||
      url.find("bbc.co.uk") != std::string::npos ||
      url.find("reuters.com") != std::string::npos ||
      url.find("apnews.com") != std::string::npos) {
    return DomainClass::News;
  }

  // Forum
  if (url.find("reddit.com") != std::string::npos ||
      url.find("stackoverflow.com") != std::string::npos ||
      url.find("quora.com") != std::string::npos) {
    return DomainClass::Forum;
  }

  return DomainClass::Unknown;
}

/**
 * @brief Check if a domain class is high-trust for single-source promotion
 * @param dc The domain class to check
 * @return true if the class is high-trust (Academic or Government)
 */
inline bool isHighTrustClass(DomainClass dc) {
  return dc == DomainClass::Academic || dc == DomainClass::Government;
}

/**
 * @brief Get a string representation of a domain class
 */
inline const char *domainClassToString(DomainClass dc) {
  switch (dc) {
  case DomainClass::Encyclopedia:
    return "encyclopedia";
  case DomainClass::Academic:
    return "academic";
  case DomainClass::Government:
    return "government";
  case DomainClass::Educational:
    return "educational";
  case DomainClass::Blog:
    return "blog";
  case DomainClass::News:
    return "news";
  case DomainClass::Forum:
    return "forum";
  default:
    return "unknown";
  }
}

} // namespace Core
} // namespace NeuroForge
