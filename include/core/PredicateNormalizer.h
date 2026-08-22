#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

namespace NeuroForge {
namespace Core {

/**
 * @brief Phase 19: Predicate Normalizer
 *
 * Maps the messy variety of human language predicates into a clean set of
 * canonical predicates. This increases fact matching across different sources.
 *
 * Example:
 *   "enables" → "used_for"
 *   "refers to" → "is_a"
 *   "consists of" → "part_of"
 */
class PredicateNormalizer {
public:
  /**
   * @brief Normalize a raw predicate to canonical form
   * @param raw_predicate The predicate as extracted from text
   * @return Canonical predicate string
   */
  static std::string normalize(const std::string &raw_predicate) {
    // Canonical predicate alias map
    static const std::unordered_map<std::string, std::string> alias_map = {
        // IS_A group - identity/classification relations
        {"is", "is_a"},
        {"is a", "is_a"},
        {"is an", "is_a"},
        {"refers to", "is_a"},
        {"defined as", "is_a"},
        {"type of", "is_a"},
        {"kind of", "is_a"},
        {"form of", "is_a"},
        {"known as", "is_a"},
        {"called", "is_a"},
        {"represents", "is_a"},
        {"means", "is_a"},

        // USED_FOR group - purpose/application relations
        {"uses", "used_for"},
        {"used for", "used_for"},
        {"used in", "used_for"},
        {"used to", "used_for"},
        {"applied to", "used_for"},
        {"applied in", "used_for"},
        {"enables", "used_for"},
        {"helps", "used_for"},
        {"allows", "used_for"},
        {"supports", "used_for"},
        {"facilitates", "used_for"},
        {"performs", "used_for"},
        {"achieves", "used_for"},
        {"accomplishes", "used_for"},
        {"provides", "used_for"},
        {"useful for", "used_for"},
        {"employed in", "used_for"},
        {"employed for", "used_for"},

        // PART_OF group - composition/inclusion relations
        {"includes", "part_of"},
        {"include", "part_of"},
        {"consists of", "part_of"},
        {"contains", "part_of"},
        {"has", "has_property"},
        {"have", "has_property"},
        {"composed of", "part_of"},
        {"made of", "part_of"},
        {"component of", "part_of"},
        {"element of", "part_of"},
        {"subset of", "part_of"},
        {"branch of", "part_of"},
        {"subfield of", "part_of"},

        // LOCATED_IN group - spatial relations
        {"located in", "located_in"},
        {"found in", "located_in"},
        {"situated in", "located_in"},
        {"based in", "located_in"},

        // RELATED_TO group - general association
        {"related to", "related_to"},
        {"linked to", "related_to"},
        {"associated with", "related_to"},
        {"connected to", "related_to"},
        {"similar to", "related_to"},

        // CAUSES group - causal relations
        {"causes", "causes"},
        {"leads to", "causes"},
        {"results in", "causes"},
        {"produces", "causes"},
        {"creates", "causes"},

        // REQUIRES group - dependency relations
        {"requires", "requires"},
        {"needs", "requires"},
        {"depends on", "requires"},
        {"relies on", "requires"}};

    // Clean the predicate: lowercase and trim
    std::string cleaned = clean(raw_predicate);

    // Look up in alias map
    auto it = alias_map.find(cleaned);
    if (it != alias_map.end()) {
      return it->second;
    }

    // Fallback: return cleaned original if no alias found
    return cleaned;
  }

private:
  static std::string clean(const std::string &s) {
    std::string result;
    result.reserve(s.size());

    // Convert to lowercase and normalize whitespace
    bool last_was_space = true; // Trim leading spaces
    for (char c : s) {
      if (std::isspace(static_cast<unsigned char>(c))) {
        if (!last_was_space) {
          result += ' ';
          last_was_space = true;
        }
      } else {
        result +=
            static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        last_was_space = false;
      }
    }

    // Trim trailing space
    if (!result.empty() && result.back() == ' ') {
      result.pop_back();
    }

    return result;
  }
};

} // namespace Core
} // namespace NeuroForge
