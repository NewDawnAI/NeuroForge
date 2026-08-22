#include "perception/LivePerceptionLoop.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>
#include <unordered_map>


namespace NeuroForge {
namespace Perception {

// Common English stopwords to filter out
static const std::unordered_set<std::string> STOPWORDS = {
    "a",          "an",
    "the",        "and",
    "or",         "but",
    "if",         "then",
    "else",       "when",
    "at",         "from",
    "by",         "on",
    "off",        "for",
    "in",         "out",
    "over",       "under",
    "to",         "into",
    "with",       "about",
    "against",    "between",
    "through",    "during",
    "before",     "after",
    "above",      "below",
    "of",         "up",
    "down",       "is",
    "are",        "was",
    "were",       "be",
    "been",       "being",
    "have",       "has",
    "had",        "having",
    "do",         "does",
    "did",        "doing",
    "would",      "could",
    "should",     "may",
    "might",      "must",
    "shall",      "will",
    "can",        "need",
    "dare",       "ought",
    "used",       "to",
    "a",          "able",
    "about",      "above",
    "according",  "accordingly",
    "across",     "actually",
    "after",      "afterwards",
    "again",      "against",
    "all",        "allow",
    "allows",     "almost",
    "alone",      "along",
    "already",    "also",
    "although",   "always",
    "am",         "among",
    "amongst",    "an",
    "and",        "another",
    "any",        "anybody",
    "anyhow",     "anyone",
    "anything",   "anyway",
    "anyways",    "anywhere",
    "apart",      "appear",
    "appreciate", "appropriate",
    "are",        "aren't",
    "around",     "as",
    "aside",      "ask",
    "asking",     "associated",
    "at",         "available",
    "away",       "awfully",
    "be",         "became",
    "because",    "become",
    "becomes",    "becoming",
    "been",       "before",
    "beforehand", "behind",
    "being",      "believe",
    "below",      "beside",
    "besides",    "best",
    "better",     "between",
    "beyond",     "both",
    "brief",      "but",
    "by",         "c'mon",
    "c's",        "came",
    "can",        "can't",
    "cannot",     "cant",
    "cause",      "causes",
    "certain",    "certainly",
    "changes",    "clearly",
    "co",         "com",
    "come",       "comes",
    "concerning", "consequently",
    "consider",   "considering",
    "contain",    "containing",
    "contains",   "corresponding",
    "could",      "couldn't",
    "course",     "currently",
    "definitely", "described",
    "despite",    "did",
    "didn't",     "different",
    "do",         "does",
    "doesn't",    "doing",
    "don't",      "done",
    "down",       "downwards",
    "during",     "each",
    "edu",        "eg",
    "eight",      "either",
    "else",       "elsewhere",
    "enough",     "entirely",
    "especially", "et",
    "etc",        "even",
    "ever",       "every",
    "everybody",  "everyone",
    "everything", "everywhere",
    "ex",         "exactly",
    "example",    "except",
    "far",        "few",
    "fifth",      "first",
    "this",       "that",
    "these",      "those",
    "it",         "its",
    "itself",     "i",
    "me",         "my",
    "myself",     "we",
    "our",        "ours",
    "ourselves",  "you",
    "your",       "yours",
    "yourself",   "yourselves",
    "he",         "him",
    "his",        "himself",
    "she",        "her",
    "hers",       "herself",
    "it",         "its",
    "they",       "them",
    "their",      "theirs",
    "themselves", "what",
    "which",      "who",
    "whom",       "whose",
    "how",        "where",
    "when",       "why"};

LivePerceptionLoop::LivePerceptionLoop(
    Core::LanguageSystem *language_system,
    Core::RelationGateManager *relation_gates, const PerceptionConfig &config)
    : language_system_(language_system), relation_gates_(relation_gates),
      config_(config) {
  minute_start_ = std::chrono::steady_clock::now();
  last_page_time_ = minute_start_;
}

LivePerceptionLoop::~LivePerceptionLoop() { stop(); }

bool LivePerceptionLoop::initialize() {
  if (!language_system_) {
    std::cerr << "[LivePerceptionLoop] ERROR: LanguageSystem is null\n";
    return false;
  }

  is_initialized_ = true;
  return true;
}

void LivePerceptionLoop::start() {
  if (!is_initialized_) {
    if (!initialize()) {
      return;
    }
  }
  is_running_ = true;
}

void LivePerceptionLoop::stop() { is_running_ = false; }

bool LivePerceptionLoop::isUrlAllowed(const std::string &url) const {
  // Check HTTPS requirement
  if (config_.require_https && url.substr(0, 8) != "https://") {
    return false;
  }

  // Check blocklist
  for (const auto &blocked : config_.url_blocklist) {
    if (url.find(blocked) != std::string::npos) {
      return false;
    }
  }

  // Check allowlist (if non-empty, URL must match one)
  if (!config_.url_allowlist.empty()) {
    bool allowed = false;
    for (const auto &allowed_domain : config_.url_allowlist) {
      if (url.find(allowed_domain) != std::string::npos) {
        allowed = true;
        break;
      }
    }
    if (!allowed) {
      return false;
    }
  }

  return true;
}

bool LivePerceptionLoop::canFetchNow() const {
  auto now = std::chrono::steady_clock::now();

  // Check minimum interval
  auto since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - last_page_time_)
                        .count();
  if (since_last < config_.min_page_interval_ms) {
    return false;
  }

  // Check per-minute limit
  auto since_minute =
      std::chrono::duration_cast<std::chrono::seconds>(now - minute_start_)
          .count();
  if (since_minute >= 60) {
    // Reset minute counter
    return true; // New minute, allow
  }

  return pages_this_minute_ < config_.max_pages_per_minute;
}

void LivePerceptionLoop::recordFetch() {
  auto now = std::chrono::steady_clock::now();

  // Check if we've entered a new minute
  auto since_minute =
      std::chrono::duration_cast<std::chrono::seconds>(now - minute_start_)
          .count();
  if (since_minute >= 60) {
    minute_start_ = now;
    pages_this_minute_ = 0;
  }

  last_page_time_ = now;
  pages_this_minute_++;

  std::lock_guard<std::mutex> lock(stats_mutex_);
  stats_.pages_fetched++;
}

bool LivePerceptionLoop::submitUrl(const std::string &url) {
  if (!isUrlAllowed(url)) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.pages_skipped++;
    return false;
  }

  // Check if already seen
  {
    std::lock_guard<std::mutex> lock(seen_mutex_);
    if (seen_urls_.count(url) > 0) {
      return false;
    }
  }

  if (!canFetchNow()) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.pages_skipped++;
    return false;
  }

  // Mark as seen
  {
    std::lock_guard<std::mutex> lock(seen_mutex_);
    seen_urls_.insert(url);
  }

  // For now, we just queue a placeholder snapshot
  // In full implementation, this would use WebSandbox to fetch
  PageSnapshot snapshot;
  snapshot.url = url;
  snapshot.captured_at = std::chrono::system_clock::now();

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (pending_snapshots_.size() >=
        static_cast<std::size_t>(config_.queue_max_size)) {
      return false;
    }
    pending_snapshots_.push(snapshot);
  }

  recordFetch();
  return true;
}

bool LivePerceptionLoop::submitHtml(const std::string &html,
                                    const std::string &url) {
  PageSnapshot snapshot;
  snapshot.url = url;
  snapshot.segments = extractTextFromHtml(html);
  snapshot.captured_at = std::chrono::system_clock::now();

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (pending_snapshots_.size() >=
        static_cast<std::size_t>(config_.queue_max_size)) {
      return false;
    }
    pending_snapshots_.push(snapshot);
  }

  recordFetch();
  return true;
}

bool LivePerceptionLoop::submitText(const std::string &text,
                                    const std::string &url) {
  PageSnapshot snapshot;
  snapshot.url = url;
  snapshot.segments = extractTextFromPlain(text);
  snapshot.captured_at = std::chrono::system_clock::now();

  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (pending_snapshots_.size() >=
        static_cast<std::size_t>(config_.queue_max_size)) {
      return false;
    }
    pending_snapshots_.push(snapshot);
  }

  recordFetch();
  return true;
}

std::size_t LivePerceptionLoop::getPendingCount() const {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  return pending_snapshots_.size();
}

void LivePerceptionLoop::update(float delta_time) {
  (void)delta_time;

  if (!is_running_ || !is_initialized_) {
    return;
  }

  // Process one snapshot per update
  PageSnapshot snapshot;
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (pending_snapshots_.empty()) {
      return;
    }
    snapshot = pending_snapshots_.front();
    pending_snapshots_.pop();
  }

  processSnapshot(snapshot);
}

void LivePerceptionLoop::processSnapshot(PageSnapshot &snapshot) {
  auto start_time = std::chrono::steady_clock::now();

  if (config_.debug_logging) {
    std::cout << "[Perception] processSnapshot url=" << snapshot.url << "\n";
  }

  // Extract entities if not already done
  if (snapshot.entities.empty() && !snapshot.segments.empty()) {
    if (config_.debug_logging) {
      std::cout << "[Perception] extractEntities begin segments="
                << snapshot.segments.size() << "\n";
    }
    snapshot.entities = extractEntities(snapshot.segments);
    if (config_.debug_logging) {
      std::cout << "[Perception] extractEntities done entities="
                << snapshot.entities.size() << "\n";
    }
  }

  accumulateEntityEvidence(snapshot);

  // Bind entities to tokens
  int bound_count = 0;
  for (auto &entity : snapshot.entities) {
    if (entity.token_id == 0) {
      auto token_id = bindEntityToToken(entity);
      if (token_id.has_value()) {
        entity.token_id = token_id.value();
        bound_count++;

        if (entity_callback_) {
          entity_callback_(entity);
        }
      }
    }
    if (config_.batch_size > 0 &&
        bound_count >= config_.batch_size) {
      break;
    }
  }
  if (config_.debug_logging) {
    std::cout << "[Perception] bindEntities done bound=" << bound_count << "\n";
  }

  bool has_unbound = false;
  for (const auto &entity : snapshot.entities) {
    if (entity.token_id == 0) {
      has_unbound = true;
      break;
    }
  }

  if (has_unbound && bound_count > 0) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (pending_snapshots_.size() <
        static_cast<std::size_t>(config_.queue_max_size)) {
      pending_snapshots_.push(snapshot);
    }
    return;
  }

  // Extract candidate relations
  if (snapshot.candidate_relations.empty()) {
    if (config_.debug_logging) {
      std::cout << "[Perception] extractRelations begin\n";
    }
    snapshot.candidate_relations =
        extractRelations(snapshot.segments, snapshot.entities, snapshot.url);
    if (config_.debug_logging) {
      std::cout << "[Perception] extractRelations done relations="
                << snapshot.candidate_relations.size() << "\n";
    }
  }

  // Create relation gates
  int gates_created = 0;
  for (auto &relation : snapshot.candidate_relations) {
    bool created = createRelationGate(relation);
    if (created) {
      gates_created++;
    }

    if (relation_callback_) {
      relation_callback_(relation, created);
    }
  }
  if (config_.debug_logging) {
    std::cout << "[Perception] createRelationGates done created=" << gates_created
              << "\n";
  }

  snapshot.is_processed = true;

  // Invoke page callback
  if (page_callback_) {
    if (config_.debug_logging) {
      std::cout << "[Perception] pageCallback begin\n";
    }
    page_callback_(snapshot);
    if (config_.debug_logging) {
      std::cout << "[Perception] pageCallback done\n";
    }
  }

  // Update stats
  auto end_time = std::chrono::steady_clock::now();
  float elapsed_ms =
      std::chrono::duration<float, std::milli>(end_time - start_time).count();

  {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.pages_processed++;
    stats_.segments_extracted += snapshot.segments.size();
    stats_.entities_extracted += snapshot.entities.size();
    stats_.entities_bound += bound_count;
    stats_.relations_proposed += snapshot.candidate_relations.size();
    stats_.relations_created += gates_created;

    // Running average
    float n = static_cast<float>(stats_.pages_processed);
    stats_.avg_processing_time_ms =
        (stats_.avg_processing_time_ms * (n - 1) + elapsed_ms) / n;
  }
}

std::vector<TextSegment>
LivePerceptionLoop::extractTextFromHtml(const std::string &html) const {
  std::vector<TextSegment> segments;

  // Simple HTML tag stripper (not a full parser)
  std::string text;
  text.reserve(html.size());

  bool in_tag = false;
  bool in_script = false;
  bool in_style = false;

  for (std::size_t i = 0; i < html.size(); ++i) {
    char c = html[i];

    if (c == '<') {
      // Check for script/style tags
      std::string lower_tag;
      for (std::size_t j = i + 1; j < html.size() && j < i + 10; ++j) {
        if (html[j] == ' ' || html[j] == '>')
          break;
        lower_tag += static_cast<char>(std::tolower(static_cast<unsigned char>(html[j])));
      }

      if (lower_tag.find("script") == 0)
        in_script = true;
      if (lower_tag.find("style") == 0)
        in_style = true;
      if (lower_tag.find("/script") == 0)
        in_script = false;
      if (lower_tag.find("/style") == 0)
        in_style = false;

      in_tag = true;

      // Add space for block elements
      if (!text.empty() &&
          !std::isspace(static_cast<unsigned char>(text.back()))) {
        text += ' ';
      }
    } else if (c == '>') {
      in_tag = false;
    } else if (!in_tag && !in_script && !in_style) {
      // Decode common HTML entities
      if (c == '&' && i + 1 < html.size()) {
        if (html.substr(i, 4) == "&lt;") {
          text += '<';
          i += 3;
        } else if (html.substr(i, 4) == "&gt;") {
          text += '>';
          i += 3;
        } else if (html.substr(i, 5) == "&amp;") {
          text += '&';
          i += 4;
        } else if (html.substr(i, 6) == "&nbsp;") {
          text += ' ';
          i += 5;
        } else if (html.substr(i, 6) == "&quot;") {
          text += '"';
          i += 5;
        } else
          text += c;
      } else {
        text += c;
      }
    }
  }

  // Split into segments by paragraph breaks
  return extractTextFromPlain(text);
}

std::vector<TextSegment>
LivePerceptionLoop::extractTextFromPlain(const std::string &text) const {
  std::vector<TextSegment> segments;

  std::istringstream stream(text);
  std::string line;
  std::string current_segment;

  while (std::getline(stream, line)) {
    // Trim
    std::size_t start = line.find_first_not_of(" \t\r\n");
    std::size_t end = line.find_last_not_of(" \t\r\n");

    if (start == std::string::npos) {
      // Empty line - could be paragraph break
      if (!current_segment.empty() &&
          current_segment.size() >=
              static_cast<std::size_t>(config_.min_segment_length)) {
        TextSegment seg;
        seg.text = current_segment;
        seg.element_type = "p";
        seg.extracted_at = std::chrono::system_clock::now();
        segments.push_back(seg);

        current_segment.clear();
      }
    } else {
      std::string trimmed = line.substr(start, end - start + 1);
      if (!current_segment.empty()) {
        current_segment += ' ';
      }
      current_segment += trimmed;
    }

    // Limit total text
    if (current_segment.size() >
        static_cast<std::size_t>(config_.max_text_length)) {
      break;
    }
  }

  // Add final segment
  if (!current_segment.empty() &&
      current_segment.size() >=
          static_cast<std::size_t>(config_.min_segment_length)) {
    TextSegment seg;
    seg.text = current_segment;
    seg.element_type = "p";
    seg.extracted_at = std::chrono::system_clock::now();
    segments.push_back(seg);
  }

  return segments;
}

std::vector<std::string>
LivePerceptionLoop::tokenize(const std::string &text) const {
  std::vector<std::string> tokens;
  std::string current;

  for (char c : text) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '\'') {
      current += c;
    } else {
      if (!current.empty()) {
        tokens.push_back(current);
        current.clear();
      }
    }
  }

  if (!current.empty()) {
    tokens.push_back(current);
  }

  return tokens;
}

std::string LivePerceptionLoop::normalizeWord(const std::string &word) const {
  std::string result;
  result.reserve(word.size());

  for (char c : word) {
    result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  // Remove leading/trailing punctuation
  while (!result.empty() &&
         !std::isalnum(static_cast<unsigned char>(result.front()))) {
    result.erase(result.begin());
  }
  while (!result.empty() &&
         !std::isalnum(static_cast<unsigned char>(result.back()))) {
    result.pop_back();
  }

  return result;
}

bool LivePerceptionLoop::isContentWord(const std::string &word) const {
  std::string lower = normalizeWord(word);

  if (lower.size() < static_cast<std::size_t>(config_.min_entity_length)) {
    return false;
  }

  return STOPWORDS.count(lower) == 0;
}

std::vector<ExtractedEntity> LivePerceptionLoop::extractEntities(
    const std::vector<TextSegment> &segments) const {

  std::unordered_map<std::string, ExtractedEntity> entity_map;

  for (const auto &segment : segments) {
    entity_resolver_.learnAliasesFromText(segment.text);

    auto tokens = tokenize(segment.text);
    std::vector<std::string> norm_tokens;
    norm_tokens.reserve(tokens.size());
    for (const auto &t : tokens) {
      auto n = normalizeWord(t);
      if (!n.empty()) norm_tokens.push_back(n);
    }

    auto upsert = [&](const std::string &surface, const std::string &type,
                      float base_salience) {
      auto canon = entity_resolver_.canonicalize(surface);
      if (!isContentWord(canon)) return;
      if (canon.size() > 64) return;

      auto it = entity_map.find(canon);
      if (it != entity_map.end()) {
        it->second.occurrence_count++;
        it->second.salience += 0.1f;
      } else {
        ExtractedEntity entity;
        entity.text = canon;
        entity.type = type;
        entity.salience = base_salience;
        entity.occurrence_count = 1;
        entity_map[canon] = entity;
      }
    };

    for (std::size_t i = 0; i < norm_tokens.size(); ++i) {
      if (!isContentWord(norm_tokens[i])) continue;

      upsert(norm_tokens[i], "WORD", 0.5f);

      if (i + 1 < norm_tokens.size() && isContentWord(norm_tokens[i + 1])) {
        std::string phrase2 = norm_tokens[i] + " " + norm_tokens[i + 1];
        std::string tail = norm_tokens[i + 1];
        auto tail_canon = entity_resolver_.canonicalize(tail);
        if (!tail_canon.empty() && tail_canon != tail && isContentWord(tail_canon)) {
          upsert(tail_canon, "PHRASE", 0.6f);
        } else {
          upsert(phrase2, "PHRASE", 0.6f);
        }
      }

      if (i + 2 < norm_tokens.size() && isContentWord(norm_tokens[i + 1]) &&
          isContentWord(norm_tokens[i + 2])) {
        std::string phrase2 = norm_tokens[i] + " " + norm_tokens[i + 1];
        std::string tail = norm_tokens[i + 2];
        auto tail_canon = entity_resolver_.canonicalize(tail);
        if (!tail_canon.empty() && tail_canon != tail && isContentWord(tail_canon)) {
          upsert(tail_canon, "PHRASE", 0.7f);
        } else if (entity_resolver_.canonicalize(tail) ==
                   entity_resolver_.canonicalize(phrase2)) {
          upsert(phrase2, "PHRASE", 0.7f);
        } else {
          upsert(phrase2 + " " + tail, "PHRASE", 0.7f);
        }
      }

      if (entity_map.size() >=
          static_cast<std::size_t>(config_.max_entities_per_page)) {
        break;
      }
    }

    if (entity_map.size() >=
        static_cast<std::size_t>(config_.max_entities_per_page)) {
      break;
    }
  }

  // Convert to vector and filter by salience
  std::vector<ExtractedEntity> entities;
  entities.reserve(entity_map.size());

  for (auto &[text, entity] : entity_map) {
    // Normalize salience based on occurrence
    entity.salience = std::min(1.0f, entity.salience);

    if (entity.salience >= config_.min_entity_salience) {
      entities.push_back(entity);
    }
  }

  // Sort by salience
  std::sort(entities.begin(), entities.end(), [](const auto &a, const auto &b) {
    return a.salience > b.salience;
  });

  return entities;
}

std::optional<std::size_t>
LivePerceptionLoop::bindEntityToToken(const ExtractedEntity &entity) {
  if (!language_system_) {
    return std::nullopt;
  }

  {
    std::lock_guard<std::mutex> lock(evidence_mutex_);
    auto it = canonical_token_cache_.find(entity.text);
    if (it != canonical_token_cache_.end() && it->second > 0) {
      return it->second;
    }
  }

  // Check if token already exists
  std::size_t existing_id;
  if (language_system_->getTokenId(entity.text, existing_id)) {
    {
      std::lock_guard<std::mutex> lock(evidence_mutex_);
      canonical_token_cache_[entity.text] = existing_id;
    }
    return existing_id;
  }

  // Create new token
  std::size_t new_id = language_system_->createToken(
      entity.text, Core::LanguageSystem::TokenType::Word);

  if (new_id > 0) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.entities_bound++;
    {
      std::lock_guard<std::mutex> lock2(evidence_mutex_);
      canonical_token_cache_[entity.text] = new_id;
    }
  }

  return new_id;
}

void LivePerceptionLoop::accumulateEntityEvidence(PageSnapshot &snapshot) {
  if (snapshot.entities.empty() || snapshot.evidence_accumulated) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(evidence_mutex_);
    for (const auto &e : snapshot.entities) {
      auto &ev = entity_evidence_[e.text];
      ev.pages_seen++;
      ev.total_occurrences += static_cast<std::size_t>(std::max(1, e.occurrence_count));
    }
  }

  for (auto &e : snapshot.entities) {
    std::size_t pages_seen = 0;
    {
      std::lock_guard<std::mutex> lock(evidence_mutex_);
      auto it = entity_evidence_.find(e.text);
      if (it != entity_evidence_.end()) {
        pages_seen = it->second.pages_seen;
      }
    }

    if (pages_seen > 1) {
      float boost = 0.05f * static_cast<float>(std::min<std::size_t>(10, pages_seen - 1));
      if (boost > 0.25f) boost = 0.25f;
      e.salience = std::min(1.0f, e.salience + boost);
    }
  }

  std::sort(snapshot.entities.begin(), snapshot.entities.end(),
            [](const auto &a, const auto &b) { return a.salience > b.salience; });

  snapshot.evidence_accumulated = true;
}

std::vector<CandidateRelation> LivePerceptionLoop::extractRelations(
    const std::vector<TextSegment> &segments,
    const std::vector<ExtractedEntity> &entities,
    const std::string &url) const {

  std::vector<CandidateRelation> relations;
  std::size_t raw_sentences = 0;
  std::size_t normalized_sentences = 0;
  std::size_t parsed_sentences = 0;
  std::size_t relation_candidates_generated = 0;
  std::size_t rejected_empty_sentence = 0;
  std::size_t rejected_unlinked_entity = 0;

  // Create entity lookup for quick access
  std::unordered_set<std::string> entity_set;
  std::unordered_map<std::string, float> entity_salience;
  for (const auto &entity : entities) {
    entity_set.insert(entity.text);
    entity_salience[entity.text] = entity.salience;
  }

  struct RecentEntity {
    std::string text;
    float salience = 0.0f;
  };
  std::vector<RecentEntity> recent;

  auto touchRecent = [&](const std::string &canon) {
    if (canon.empty()) return;
    if (entity_set.count(canon) == 0) return;

    float s = 0.5f;
    auto it = entity_salience.find(canon);
    if (it != entity_salience.end()) s = it->second;

    for (auto rit = recent.begin(); rit != recent.end(); ++rit) {
      if (rit->text == canon) {
        recent.erase(rit);
        break;
      }
    }
    recent.push_back({canon, s});
    if (recent.size() > 20) recent.erase(recent.begin());
  };

  auto resolveFromRecent = [&]() -> std::optional<std::string> {
    if (recent.empty()) return std::nullopt;
    float best = -1e9f;
    std::string best_text;
    for (std::size_t i = 0; i < recent.size(); ++i) {
      std::size_t dist = recent.size() - 1 - i;
      float score = recent[i].salience - static_cast<float>(dist) * 0.05f;
      if (score > best) {
        best = score;
        best_text = recent[i].text;
      }
    }
    if (best_text.empty()) return std::nullopt;
    return best_text;
  };

  auto normalizeLoose = [&](const std::string &s) {
    return entity_resolver_.normalizeSurface(s, false);
  };

  auto getAnaphorHead = [&](const std::string &raw) -> std::optional<std::string> {
    std::string loose = normalizeLoose(raw);
    static const std::unordered_set<std::string> heads = {
        "model",     "system",  "field",   "method", "approach",
        "algorithm", "technique", "framework", "process"};

    static const std::vector<std::string> det = {"the ", "this ", "that ",
                                                 "these ", "those "};
    bool had_det = false;
    for (const auto &d : det) {
      if (loose.rfind(d, 0) == 0 && loose.size() > d.size()) {
        loose.erase(0, d.size());
        while (!loose.empty() && loose.front() == ' ') loose.erase(loose.begin());
        had_det = true;
        break;
      }
    }
    if (!had_det) return std::nullopt;

    auto first_space = loose.find(' ');
    std::string head =
        first_space == std::string::npos ? loose : loose.substr(0, first_space);
    if (heads.count(head) == 0) return std::nullopt;
    return head;
  };

  auto resolveEntityText = [&](const std::string &raw)
      -> std::optional<std::string> {
    if (entity_resolver_.isPronoun(raw)) {
      return resolveFromRecent();
    }
    if (auto head = getAnaphorHead(raw); head.has_value()) {
      if (entity_set.count(head.value()) > 0) return head.value();
      return resolveFromRecent();
    }
    auto canon = entity_resolver_.canonicalize(raw);
    if (canon.empty()) return std::nullopt;
    if (entity_set.count(canon) > 0) return canon;

    auto raw_tokens = tokenize(raw);
    std::vector<std::string> content;
    content.reserve(raw_tokens.size());
    for (const auto &t : raw_tokens) {
      auto n = normalizeWord(t);
      if (n.empty()) continue;
      if (!isContentWord(n)) continue;
      content.push_back(n);
    }

    for (std::size_t i = 0; i < content.size(); ++i) {
      for (int n = 3; n >= 1; --n) {
        if (i + static_cast<std::size_t>(n) > content.size()) continue;
        std::string surface = content[i];
        for (int k = 1; k < n; ++k) {
          surface += " ";
          surface += content[i + static_cast<std::size_t>(k)];
        }
        auto c = entity_resolver_.canonicalize(surface);
        if (entity_set.count(c) > 0) return c;
      }
    }

    return std::nullopt;
  };

  // Search for relation patterns in each segment
  for (const auto &segment : segments) {
    // Split into sentences (simple approach)
    std::string sentence_delimiters = ".!?";
    std::size_t start = 0;

    while (start < segment.text.size()) {
      std::size_t end = segment.text.find_first_of(sentence_delimiters, start);
      if (end == std::string::npos) {
        end = segment.text.size();
      }

      std::string sentence = segment.text.substr(start, end - start + 1);
      start = end + 1;
      raw_sentences++;

      if (sentence.size() > 512) sentence.resize(512);

      entity_resolver_.learnAliasesFromText(sentence);

      auto sentence_tokens = tokenize(sentence);
      std::vector<std::string> stoks;
      stoks.reserve(sentence_tokens.size());
      for (const auto &t : sentence_tokens) {
        auto n = normalizeWord(t);
        if (!n.empty()) stoks.push_back(n);
      }
      if (stoks.empty()) {
        rejected_empty_sentence++;
        continue;
      }
      normalized_sentences++;

      struct Mention {
        std::size_t start = 0;
        std::size_t end = 0;
        std::string canon;
      };
      std::vector<Mention> mentions;
      mentions.reserve(stoks.size());

      for (std::size_t i = 0; i < stoks.size(); ++i) {
        for (int n = 3; n >= 1; --n) {
          if (i + static_cast<std::size_t>(n) > stoks.size()) continue;
          std::string surface = stoks[i];
          for (int k = 1; k < n; ++k) {
            surface += " ";
            surface += stoks[i + static_cast<std::size_t>(k)];
          }
          auto canon = entity_resolver_.canonicalize(surface);
          if (entity_set.count(canon) > 0) {
            touchRecent(canon);
            mentions.push_back({i, i + static_cast<std::size_t>(n), canon});
            break;
          }
        }
      }
      if (!mentions.empty()) parsed_sentences++;

      auto findPrevMention = [&](std::size_t pos) -> std::optional<Mention> {
        bool has = false;
        Mention best;
        for (const auto &m : mentions) {
          if (m.end > pos) continue;
          if (!has || m.end > best.end) {
            best = m;
            has = true;
          }
        }
        if (!has) return std::nullopt;
        return best;
      };

      auto findNextMention = [&](std::size_t pos) -> std::optional<Mention> {
        bool has = false;
        Mention best;
        for (const auto &m : mentions) {
          if (m.start < pos) continue;
          if (!has || m.start < best.start) {
            best = m;
            has = true;
          }
        }
        if (!has) return std::nullopt;
        return best;
      };

      auto emitRelation = [&](const std::string &subj, const std::string &pred,
                              const std::string &obj, float conf) {
        if (subj.empty() || obj.empty()) return;
        if (entity_set.count(subj) == 0 || entity_set.count(obj) == 0) {
          rejected_unlinked_entity++;
          return;
        }

        CandidateRelation rel;
        rel.subject.text = subj;
        rel.subject.type = "WORD";
        rel.predicate.text = pred;
        rel.predicate.type = "RELATION";
        rel.object.text = obj;
        rel.object.type = "WORD";
        rel.source_sentence = sentence;
        rel.source_url = url;
        rel.confidence = conf;

        if (language_system_) {
          rel.transe_score = language_system_->scoreRelationTriple(subj, pred, obj);
        }

        relations.push_back(rel);
        relation_candidates_generated++;
      };

      auto prevEntity = [&](std::size_t pos) -> std::optional<std::string> {
        if (auto m = findPrevMention(pos); m.has_value()) return m->canon;
        if (pos >= 1 && entity_resolver_.isPronoun(stoks[pos - 1])) {
          return resolveFromRecent();
        }
        if (pos >= 2 &&
            (stoks[pos - 1] == "is" || stoks[pos - 1] == "are" ||
             stoks[pos - 1] == "was" || stoks[pos - 1] == "were") &&
            entity_resolver_.isPronoun(stoks[pos - 2])) {
          return resolveFromRecent();
        }
        if (pos >= 1) {
          if (auto r = resolveEntityText(stoks[pos - 1]); r.has_value())
            return r.value();
        }
        if (pos >= 2) {
          std::string phrase = stoks[pos - 2] + " " + stoks[pos - 1];
          if (auto r = resolveEntityText(phrase); r.has_value()) return r.value();
        }
        return std::nullopt;
      };

      auto nextEntity = [&](std::size_t pos) -> std::optional<std::string> {
        if (auto m = findNextMention(pos); m.has_value()) return m->canon;
        if (pos < stoks.size()) {
          if (auto r = resolveEntityText(stoks[pos]); r.has_value()) return r.value();
        }
        if (pos + 1 < stoks.size()) {
          std::string phrase = stoks[pos] + " " + stoks[pos + 1];
          if (auto r = resolveEntityText(phrase); r.has_value()) return r.value();
        }
        return std::nullopt;
      };

      auto skipDets = [&](std::size_t idx) {
        while (idx < stoks.size()) {
          const auto &w = stoks[idx];
          if (w == "a" || w == "an" || w == "the" || w == "this" || w == "that" ||
              w == "these" || w == "those") {
            idx++;
            continue;
          }
          break;
        }
        return idx;
      };

      for (std::size_t i = 0; i < stoks.size(); ++i) {
        const auto &w = stoks[i];

        if (w == "is" && i + 2 < stoks.size() &&
            (stoks[i + 1] == "a" || stoks[i + 1] == "an")) {
          auto subj = prevEntity(i);
          auto obj = nextEntity(i + 2);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "is_a", obj.value(), 0.5f);
          }
        } else if (w == "are" && i + 1 < stoks.size()) {
          auto subj = prevEntity(i);
          auto obj = nextEntity(i + 1);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "is_a", obj.value(), 0.45f);
          }
        } else if ((w == "has" || w == "have") && i + 1 < stoks.size()) {
          auto subj = prevEntity(i);
          auto j = skipDets(i + 1);
          auto obj = nextEntity(j);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "has", obj.value(), 0.45f);
          }
        } else if ((w == "include" || w == "includes") && i + 1 < stoks.size()) {
          auto subj = prevEntity(i);
          auto obj = nextEntity(i + 1);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "includes", obj.value(), 0.45f);
          }
        } else if ((w == "contain" || w == "contains") && i + 1 < stoks.size()) {
          auto subj = prevEntity(i);
          auto obj = nextEntity(i + 1);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "contains", obj.value(), 0.45f);
          }
        } else if (w == "part" && i + 2 < stoks.size() && stoks[i + 1] == "of") {
          auto subj = prevEntity(i);
          auto j = skipDets(i + 2);
          auto obj = nextEntity(j);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "part_of", obj.value(), 0.45f);
          }
        } else if ((w == "belong" || w == "belongs") && i + 2 < stoks.size() &&
                   stoks[i + 1] == "to") {
          auto subj = prevEntity(i);
          auto j = skipDets(i + 2);
          auto obj = nextEntity(j);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "belongs_to", obj.value(), 0.45f);
          }
        } else if (w == "located" && i + 2 < stoks.size() && stoks[i + 1] == "in") {
          auto subj = prevEntity(i);
          auto j = skipDets(i + 2);
          auto obj = nextEntity(j);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "located_in", obj.value(), 0.45f);
          }
        } else if ((w == "live" || w == "lives") && i + 2 < stoks.size() &&
                   stoks[i + 1] == "in") {
          auto subj = prevEntity(i);
          auto j = skipDets(i + 2);
          auto obj = nextEntity(j);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "lives_in", obj.value(), 0.45f);
          }
        } else if (w == "made" && i + 2 < stoks.size() &&
                   (stoks[i + 1] == "of" || stoks[i + 1] == "from")) {
          auto subj = prevEntity(i);
          auto obj = nextEntity(i + 2);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "made_of", obj.value(), 0.45f);
          }
        } else if (w == "used" && i + 2 < stoks.size() &&
                   (stoks[i + 1] == "for" || stoks[i + 1] == "to")) {
          auto subj = prevEntity(i);
          auto obj = nextEntity(i + 2);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "used_for", obj.value(), 0.45f);
          }
        } else if ((w == "cause" || w == "causes") && i + 1 < stoks.size()) {
          auto subj = prevEntity(i);
          auto obj = nextEntity(i + 1);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "causes", obj.value(), 0.4f);
          }
        } else if ((w == "produce" || w == "produces") && i + 1 < stoks.size()) {
          auto subj = prevEntity(i);
          auto obj = nextEntity(i + 1);
          if (subj.has_value() && obj.has_value()) {
            emitRelation(subj.value(), "produces", obj.value(), 0.4f);
          }
        }

        if (relations.size() >=
            static_cast<std::size_t>(config_.max_relations_per_page)) {
          std::lock_guard<std::mutex> lock(stats_mutex_);
          stats_.raw_sentences += raw_sentences;
          stats_.normalized_sentences += normalized_sentences;
          stats_.parsed_sentences += parsed_sentences;
          stats_.relation_candidates_generated += relation_candidates_generated;
          stats_.rejected_empty_sentence += rejected_empty_sentence;
          stats_.rejected_unlinked_entity += rejected_unlinked_entity;
          return relations;
        }
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.raw_sentences += raw_sentences;
    stats_.normalized_sentences += normalized_sentences;
    stats_.parsed_sentences += parsed_sentences;
    stats_.relation_candidates_generated += relation_candidates_generated;
    stats_.rejected_empty_sentence += rejected_empty_sentence;
    stats_.rejected_unlinked_entity += rejected_unlinked_entity;
  }
  return relations;
}

bool LivePerceptionLoop::createRelationGate(const CandidateRelation &relation) {
  if (!relation_gates_ || !language_system_) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    if (stats_.transe_scored == 0) {
      stats_.transe_min = relation.transe_score;
      stats_.transe_max = relation.transe_score;
    } else {
      stats_.transe_min = std::min(stats_.transe_min, relation.transe_score);
      stats_.transe_max = std::max(stats_.transe_max, relation.transe_score);
    }
    stats_.transe_scored++;
    stats_.transe_sum += relation.transe_score;
  }

  // Check thresholds
  if (relation.confidence < config_.min_relation_confidence) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.rejected_low_confidence++;
    return false;
  }

  if (relation.transe_score < config_.min_transe_score_provisional) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_.rejected_low_transe_score++;
    return false;
  }

  bool is_provisional = relation.transe_score < config_.min_transe_score;

  // Ensure tokens exist
  std::size_t subj_id, rel_id, obj_id;
  if (!language_system_->getTokenId(relation.subject.text, subj_id)) {
    subj_id = language_system_->createToken(
        relation.subject.text, Core::LanguageSystem::TokenType::Word);
  }
  if (!language_system_->getTokenId(relation.predicate.text, rel_id)) {
    rel_id = language_system_->createToken(
        relation.predicate.text, Core::LanguageSystem::TokenType::Relation);
  }
  if (!language_system_->getTokenId(relation.object.text, obj_id)) {
    obj_id = language_system_->createToken(
        relation.object.text, Core::LanguageSystem::TokenType::Word);
  }

  // Create the gate
  auto gate_id = relation_gates_->createRelationGate(
      subj_id, rel_id, obj_id, relation.source_url, relation.transe_score,
      is_provisional);

  if (gate_id == 0) return false;
  {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    if (is_provisional) {
      stats_.accepted_provisional++;
    } else {
      stats_.accepted_confirmed++;
    }
  }
  return true;
}

PerceptionStats LivePerceptionLoop::getStatistics() const {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  return stats_;
}

void LivePerceptionLoop::resetStatistics() {
  std::lock_guard<std::mutex> lock(stats_mutex_);
  stats_ = PerceptionStats{};
}

std::vector<Core::EntityResolver::AliasRecord>
LivePerceptionLoop::dumpEntityAliases() const {
  return entity_resolver_.dumpAliases();
}

void LivePerceptionLoop::loadEntityAliases(
    const std::vector<Core::EntityResolver::AliasRecord> &records) {
  entity_resolver_.loadAliases(records);
}

} // namespace Perception
} // namespace NeuroForge
