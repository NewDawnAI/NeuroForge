#include "core/EntityResolver.h"
#include <algorithm>
#include <cctype>

namespace NeuroForge {
namespace Core {

static std::string toLower(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (unsigned char c : std::string(s)) {
    out.push_back(static_cast<char>(std::tolower(static_cast<int>(c))));
  }
  return out;
}

static void trimInPlace(std::string &s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
    s.erase(s.begin());
  }
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }
}

static void collapseSpacesInPlace(std::string &s) {
  std::string out;
  out.reserve(s.size());
  bool prev_space = false;
  for (char c : s) {
    bool space = std::isspace(static_cast<unsigned char>(c)) != 0;
    if (space) {
      if (!prev_space) out.push_back(' ');
      prev_space = true;
    } else {
      out.push_back(c);
      prev_space = false;
    }
  }
  s.swap(out);
  trimInPlace(s);
}

static void stripOuterPunctInPlace(std::string &s) {
  while (!s.empty() && !std::isalnum(static_cast<unsigned char>(s.front()))) {
    s.erase(s.begin());
  }
  while (!s.empty() && !std::isalnum(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }
}

EntityResolver::EntityResolver() {
  pronouns_ = {"it",   "this", "that", "these", "those", "they", "them",
               "its",  "their", "theirs", "itself", "themselves",
               "he",   "him", "his", "himself", "she", "her", "hers",
               "herself"};
}

bool EntityResolver::containsAlpha(std::string_view s) {
  for (unsigned char c : std::string(s)) {
    if (std::isalpha(static_cast<int>(c)) != 0) return true;
  }
  return false;
}

std::string EntityResolver::normalizeSurface(std::string_view text,
                                             bool strip_determiners) const {
  std::string s = toLower(text);
  for (auto &c : s) {
    if (c == '-' || c == '_' || c == '/') c = ' ';
  }
  collapseSpacesInPlace(s);
  stripOuterPunctInPlace(s);
  collapseSpacesInPlace(s);

  if (s.empty()) return s;

  if (pronouns_.count(s) > 0) return s;

  if (strip_determiners) {
    static const std::vector<std::string> prefixes = {
        "the ", "a ", "an ", "this ", "that ", "these ", "those "};
    for (const auto &p : prefixes) {
      if (s.rfind(p, 0) == 0 && s.size() > p.size()) {
        s.erase(0, p.size());
        collapseSpacesInPlace(s);
        break;
      }
    }
  }

  return s;
}

bool EntityResolver::isPronoun(std::string_view text) const {
  std::string key = toLower(text);
  for (auto &c : key) {
    if (c == '-' || c == '_' || c == '/') c = ' ';
  }
  collapseSpacesInPlace(key);
  stripOuterPunctInPlace(key);
  collapseSpacesInPlace(key);
  if (key.empty()) return false;
  return pronouns_.count(key) > 0;
}

std::vector<EntityResolver::AliasRecord> EntityResolver::dumpAliases() const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<AliasRecord> out;
  out.reserve(alias_to_canonical_.size());
  for (const auto &[alias, entry] : alias_to_canonical_) {
    AliasRecord r;
    r.alias = alias;
    r.canonical = entry.canonical;
    r.confidence = entry.confidence;
    out.push_back(std::move(r));
  }
  return out;
}

void EntityResolver::loadAliases(const std::vector<AliasRecord> &records) {
  std::lock_guard<std::mutex> lock(mutex_);
  for (const auto &r : records) {
    if (r.alias.empty() || r.canonical.empty()) continue;
    auto &entry = alias_to_canonical_[r.alias];
    if (r.confidence >= entry.confidence) {
      entry.canonical = r.canonical;
      entry.confidence = r.confidence;
    }
  }
}

std::string EntityResolver::canonicalize(std::string_view text) const {
  std::string norm = normalizeSurface(text, true);
  if (norm.empty()) return norm;

  std::lock_guard<std::mutex> lock(mutex_);
  auto it = alias_to_canonical_.find(norm);
  if (it == alias_to_canonical_.end()) return norm;
  return it->second.canonical;
}

void EntityResolver::addAlias(const std::string &alias,
                              const std::string &canonical,
                              float confidence) {
  std::string a = normalizeSurface(alias, true);
  std::string c = normalizeSurface(canonical, true);
  if (a.empty() || c.empty()) return;
  if (a == c) return;
  if (!containsAlpha(a) || !containsAlpha(c)) return;

  std::lock_guard<std::mutex> lock(mutex_);
  auto &entry = alias_to_canonical_[a];
  if (confidence >= entry.confidence) {
    entry.canonical = c;
    entry.confidence = confidence;
  }
}

void EntityResolver::learnAliasesFromText(const std::string &text,
                                         float confidence) {
  std::string s = text;
  if (s.size() > 4096) s.resize(4096);

  auto isUpper = [](unsigned char c) { return std::isupper(static_cast<int>(c)) != 0; };
  auto isAlpha = [](unsigned char c) { return std::isalpha(static_cast<int>(c)) != 0; };

  auto trimCopy = [](std::string v) {
    trimInPlace(v);
    return v;
  };

  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] != '(') continue;

    std::size_t j = i + 1;
    while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) j++;
    std::size_t ac_start = j;
    while (j < s.size() && isUpper(static_cast<unsigned char>(s[j]))) j++;
    std::size_t ac_end = j;
    while (j < s.size() && std::isspace(static_cast<unsigned char>(s[j]))) j++;
    if (j >= s.size() || s[j] != ')') continue;
    if (ac_end <= ac_start) continue;

    std::string acronym = s.substr(ac_start, ac_end - ac_start);
    if (acronym.size() < 2 || acronym.size() > 8) continue;

    std::size_t phrase_end = i;
    while (phrase_end > 0 &&
           std::isspace(static_cast<unsigned char>(s[phrase_end - 1]))) {
      phrase_end--;
    }
    if (phrase_end == 0) continue;

    std::size_t phrase_start = phrase_end;
    std::size_t scanned = 0;
    while (phrase_start > 0 && scanned < 90) {
      unsigned char c = static_cast<unsigned char>(s[phrase_start - 1]);
      if (isAlpha(c) || c == ' ' || c == '-' || c == '\'') {
        phrase_start--;
        scanned++;
        continue;
      }
      break;
    }

    std::string phrase = trimCopy(s.substr(phrase_start, phrase_end - phrase_start));
    if (phrase.size() < 3) continue;
    addAlias(acronym, phrase, confidence);
  }

  std::string lower = toLower(s);
  const std::string needle = " stands for ";
  std::size_t pos = 0;
  while ((pos = lower.find(needle, pos)) != std::string::npos) {
    std::size_t left = pos;
    while (left > 0 && std::isspace(static_cast<unsigned char>(s[left - 1]))) left--;
    std::size_t ac_end = left;
    std::size_t ac_start = ac_end;
    while (ac_start > 0 && isUpper(static_cast<unsigned char>(s[ac_start - 1]))) {
      ac_start--;
    }

    std::string acronym = s.substr(ac_start, ac_end - ac_start);
    if (acronym.size() < 2 || acronym.size() > 8) {
      pos += needle.size();
      continue;
    }

    std::size_t right = pos + needle.size();
    while (right < s.size() && std::isspace(static_cast<unsigned char>(s[right]))) right++;
    std::size_t phrase_start = right;
    std::size_t phrase_end = phrase_start;
    while (phrase_end < s.size() && phrase_end - phrase_start < 90) {
      char c = s[phrase_end];
      if (c == '.' || c == ',' || c == ';' || c == '\n' || c == '\r') break;
      phrase_end++;
    }
    std::string phrase = trimCopy(s.substr(phrase_start, phrase_end - phrase_start));
    if (phrase.size() >= 3) {
      addAlias(acronym, phrase, confidence);
    }

    pos += needle.size();
  }
}

} // namespace Core
} // namespace NeuroForge
