#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace NeuroForge {
namespace Core {

class EntityResolver {
public:
  EntityResolver();

  std::string normalizeSurface(std::string_view text,
                               bool strip_determiners = true) const;

  std::string canonicalize(std::string_view text) const;

  void addAlias(const std::string &alias, const std::string &canonical,
                float confidence = 1.0f);

  void learnAliasesFromText(const std::string &text, float confidence = 0.9f);

  bool isPronoun(std::string_view text) const;

  struct AliasRecord {
    std::string alias;
    std::string canonical;
    float confidence = 0.0f;
  };

  std::vector<AliasRecord> dumpAliases() const;

  void loadAliases(const std::vector<AliasRecord> &records);

private:
  struct AliasEntry {
    std::string canonical;
    float confidence = 0.0f;
  };

  static bool containsAlpha(std::string_view s);

  mutable std::mutex mutex_;
  std::unordered_map<std::string, AliasEntry> alias_to_canonical_;
  std::unordered_set<std::string> pronouns_;
};

} // namespace Core
} // namespace NeuroForge
