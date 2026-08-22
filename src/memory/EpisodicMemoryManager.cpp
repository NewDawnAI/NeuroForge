#include "memory/EpisodicMemoryManager.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <numeric>
#include <random>

namespace NeuroForge {
namespace Memory {

// Helper: cosine similarity for vectors
static float local_cosine(const std::vector<float> &a,
                          const std::vector<float> &b) {
  if (a.empty() || b.empty())
    return 0.0f;
  size_t n = std::min(a.size(), b.size());
  float dot = 0, na = 0, nb = 0;
  for (size_t i = 0; i < n; ++i) {
    dot += a[i] * b[i];
    na += a[i] * a[i];
    nb += b[i] * b[i];
  }
  float denom = std::sqrt(na) * std::sqrt(nb);
  return (denom > 1e-8f) ? (dot / denom) : 0.0f;
}

EpisodicMemoryManager::EpisodicMemoryManager(const EpisodicConfig &config)
    : config_(config), pattern_dim_(config.pattern_dim) {}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  PATTERN-BASED API (primary)
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void EpisodicMemoryManager::encodePattern(const std::vector<float> &sensory,
                                          const std::vector<float> &context,
                                          const std::vector<float> &emotional,
                                          float salience) {
  std::lock_guard<std::shared_mutex> lock(episodes_mutex_);
  encodePatternLocked(sensory, context, emotional, salience);
}

void EpisodicMemoryManager::encodePatternLocked(
    const std::vector<float> &sensory, const std::vector<float> &context,
    const std::vector<float> &emotional, float salience) {
  EpisodicPattern pattern;
  pattern.sensory_state = sensory;
  pattern.context_state = context;
  pattern.emotional_state = emotional;
  pattern.salience = salience;
  pattern.consolidation_strength = salience * kDefaultSalience;
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  pattern.timestamp_ms = (now_ms < 0) ? 0ULL : static_cast<uint64_t>(now_ms);

  patterns_.push_back(std::move(pattern));

  // Enforce max
  if (patterns_.size() > config_.max_episodes) {
    forgetWeakPatternsLocked(kDefaultMinForgetStrength);
  }
}

std::vector<EpisodicPattern>
EpisodicMemoryManager::recallByCue(const std::vector<float> &cue,
                                   std::size_t max_results,
                                   float threshold) const {
  std::shared_lock<std::shared_mutex> lock(episodes_mutex_);
  float thresh =
      (threshold < 0) ? config_.pattern_completion_threshold : threshold;

  std::vector<std::pair<float, size_t>> scored;
  for (size_t i = 0; i < patterns_.size(); ++i) {
    // Match against sensory, context, and combined
    float sim_s = local_cosine(cue, patterns_[i].sensory_state);
    float sim_c = local_cosine(cue, patterns_[i].context_state);
    float best = std::max(sim_s, sim_c);
    if (best >= thresh) {
      scored.push_back({best * patterns_[i].salience, i});
    }
  }

  std::sort(scored.begin(), scored.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });

  std::vector<EpisodicPattern> results;
  for (size_t i = 0; i < std::min(scored.size(), max_results); ++i) {
    results.push_back(patterns_[scored[i].second]);
  }
  pattern_recalls_ += results.size();
  return results;
}

void EpisodicMemoryManager::loadPatterns(
    const std::vector<EpisodicPattern> &patterns) {
  std::lock_guard<std::shared_mutex> lock(episodes_mutex_);
  patterns_ = patterns;
}

void EpisodicMemoryManager::consolidatePatterns() {
  std::lock_guard<std::shared_mutex> lock(episodes_mutex_);
  consolidatePatternsLocked();
}

void EpisodicMemoryManager::forgetWeakPatterns(float min_strength) {
  std::lock_guard<std::shared_mutex> lock(episodes_mutex_);
  forgetWeakPatternsLocked(min_strength);
}

// â”€â”€â”€ Lock-free internal versions (caller must hold episodes_mutex_)
// â”€â”€â”€

void EpisodicMemoryManager::consolidatePatternsLocked() {
  for (auto &p : patterns_) {
    if (p.salience >= config_.consolidation_threshold && !p.consolidated) {
      p.consolidation_strength =
          std::min(1.0f, p.consolidation_strength + kConsolidationStep);
      if (p.consolidation_strength >= config_.consolidation_threshold) {
        p.consolidated = true;
      }
    }
  }
}

void EpisodicMemoryManager::forgetWeakPatternsLocked(float min_strength) {
  patterns_.erase(std::remove_if(patterns_.begin(), patterns_.end(),
                                 [min_strength](const EpisodicPattern &p) {
                                   return !p.consolidated &&
                                          p.consolidation_strength <
                                              min_strength;
                                 }),
                  patterns_.end());
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  UTILITY
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

float EpisodicMemoryManager::cosineSim(const std::vector<float> &a,
                                       const std::vector<float> &b) const {
  return local_cosine(a, b);
}

std::vector<float>
EpisodicMemoryManager::encodeString(const std::string &s) const {
  std::vector<float> vec(pattern_dim_, 0.0f);
  if (s.empty())
    return vec;
  std::hash<std::string> hasher;
  size_t h = hasher(s);
  std::mt19937 local_rng(static_cast<unsigned>(h));
  std::normal_distribution<float> dist(0.0f, 1.0f);
  for (size_t i = 0; i < pattern_dim_; ++i) {
    vec[i] = dist(local_rng);
  }
  float norm = 0;
  for (auto v : vec)
    norm += v * v;
  norm = std::sqrt(norm);
  if (norm > 1e-8f)
    for (auto &v : vec)
      v /= norm;
  return vec;
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  LEGACY API (adapters)
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
// Suppress deprecation warnings within this file since we define the
// deprecated methods here.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

std::uint64_t
EpisodicMemoryManager::storeEpisode(const std::string &context,
                                    const std::vector<float> &sensory_data,
                                    const std::vector<float> &emotional_state,
                                    const std::string & /*narrative*/) {

  std::lock_guard<std::shared_mutex> lock(episodes_mutex_);

  auto ep_id = next_episode_id_++;

  // Encode as vector pattern (primary storage — no strings)
  auto ctx_vec = encodeString(context);
  encodePatternLocked(sensory_data, ctx_vec, emotional_state, kDefaultSalience);

  // Enhanced episode for recent list (context_tag is a small label —
  // acceptable)
  EnhancedEpisode enhanced;
  enhanced.sensory_state = sensory_data;
  enhanced.action_state = {};
  enhanced.substrate_state = {};
  enhanced.context_tag = context;
  enhanced.emotional_weight =
      emotional_state.empty()
          ? 0.0f
          : emotional_state[0]; // Only first element used for legacy
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  enhanced.timestamp_ms = (now_ms < 0) ? 0ULL : static_cast<uint64_t>(now_ms);
  enhanced.consolidation_strength = kDefaultSalience;

  recent_episodes_.push_back(std::move(enhanced));
  if (recent_episodes_.size() > kMaxRecentEpisodes) {
    recent_episodes_.erase(recent_episodes_.begin(),
                           recent_episodes_.begin() +
                               static_cast<ptrdiff_t>(recent_episodes_.size() -
                                                      kMaxRecentEpisodes));
  }

  context_index_[context].push_back(recent_episodes_.size() - 1);
  episode_id_index_[ep_id] = recent_episodes_.size() - 1;
  total_episodes_recorded_++;

  return ep_id;
}

std::shared_ptr<Episode>
EpisodicMemoryManager::retrieveEpisode(std::uint64_t episode_id) {
  std::shared_lock<std::shared_mutex> lock(episodes_mutex_);
  total_retrievals_++;

  // Reconstruct from pattern storage on the fly (deprecated path)
  auto it = episode_id_index_.find(episode_id);
  if (it != episode_id_index_.end() && it->second < recent_episodes_.size()) {
    const auto &re = recent_episodes_[it->second];
    auto ep = std::make_shared<Episode>();
    ep->id = episode_id;
    ep->sensory_data = re.sensory_state;
    ep->context = re.context_tag;
    ep->salience = re.consolidation_strength;
    successful_retrievals_++;
    return ep;
  }
  return nullptr;
}

std::vector<std::shared_ptr<Episode>>
EpisodicMemoryManager::searchEpisodes(const std::string &query,
                                      std::size_t max_results) {
  // Delegate to pattern-based recall
  auto query_vec = encodeString(query);
  auto patterns = recallByCue(query_vec, max_results);

  std::vector<std::shared_ptr<Episode>> results;
  for (const auto &p : patterns) {
    auto ep = std::make_shared<Episode>();
    ep->id = 0;
    ep->sensory_data = p.sensory_state;
    ep->emotional_state = p.emotional_state;
    ep->salience = p.salience;
    results.push_back(std::move(ep));
  }
  total_retrievals_++;
  successful_retrievals_ += results.size();
  return results;
}

std::vector<std::shared_ptr<Episode>>
EpisodicMemoryManager::searchByVector(const std::vector<float> &query_vector,
                                      std::size_t max_results) {
  auto patterns = recallByCue(query_vector, max_results);

  std::vector<std::shared_ptr<Episode>> results;
  for (const auto &p : patterns) {
    auto ep = std::make_shared<Episode>();
    ep->id = 0;
    ep->sensory_data = p.sensory_state;
    ep->emotional_state = p.emotional_state;
    ep->salience = p.salience;
    results.push_back(std::move(ep));
  }
  total_retrievals_++;
  successful_retrievals_ += results.size();
  return results;
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

void EpisodicMemoryManager::consolidateMemories() {
  std::lock_guard<std::shared_mutex> lock(episodes_mutex_);
  total_consolidations_++;
  // Consolidate patterns only (no legacy episodes_ map anymore)
  consolidatePatternsLocked();
}

void EpisodicMemoryManager::updateSalience(std::uint64_t episode_id,
                                           float salience) {
  std::lock_guard<std::shared_mutex> lock(episodes_mutex_);
  // Update in episode_id_index_ -> recent_episodes_ if available
  auto it = episode_id_index_.find(episode_id);
  if (it != episode_id_index_.end() && it->second < recent_episodes_.size()) {
    recent_episodes_[it->second].consolidation_strength = salience;
  }
}

void EpisodicMemoryManager::forgetOldMemories() {
  if (!config_.enable_forgetting)
    return;
  std::lock_guard<std::shared_mutex> lock(episodes_mutex_);
  decayMemoryTracesLocked();
  pruneWeakMemoriesLocked();
}

std::size_t EpisodicMemoryManager::getConsolidatedCount() const {
  std::shared_lock<std::shared_mutex> lock(episodes_mutex_);
  std::size_t count = 0;
  for (const auto &p : patterns_) {
    if (p.consolidated)
      count++;
  }
  return count;
}

float EpisodicMemoryManager::getAverageActivation() const {
  std::shared_lock<std::shared_mutex> lock(episodes_mutex_);
  if (patterns_.empty())
    return 0.0f;
  float sum = 0.0f;
  for (const auto &p : patterns_) {
    sum += p.consolidation_strength;
  }
  return sum / static_cast<float>(patterns_.size());
}

void EpisodicMemoryManager::decayMemoryTraces() {
  std::lock_guard<std::shared_mutex> lock(episodes_mutex_);
  decayMemoryTracesLocked();
}

void EpisodicMemoryManager::decayMemoryTracesLocked() {
  for (auto &p : patterns_) {
    p.consolidation_strength *= (1.0f - config_.decay_rate);
    p.salience *= (1.0f - config_.decay_rate);
  }
}

void EpisodicMemoryManager::pruneWeakMemories() {
  std::lock_guard<std::shared_mutex> lock(episodes_mutex_);
  pruneWeakMemoriesLocked();
}

void EpisodicMemoryManager::pruneWeakMemoriesLocked() {
  // Prune weak patterns (delegate to existing pattern pruner)
  forgetWeakPatternsLocked(kPruneActivationThreshold);
}

std::vector<EnhancedEpisode>
EpisodicMemoryManager::getRecentEpisodes(std::uint64_t time_window_ms,
                                         std::size_t max_count) const {
  std::lock_guard<std::shared_mutex> lock(episodes_mutex_);
  auto now_ms_signed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch())
                           .count();
  uint64_t now_ms =
      (now_ms_signed < 0) ? 0ULL : static_cast<uint64_t>(now_ms_signed);
  uint64_t cutoff = (now_ms > time_window_ms) ? (now_ms - time_window_ms) : 0;

  std::vector<EnhancedEpisode> result;
  for (auto it = recent_episodes_.rbegin(); it != recent_episodes_.rend();
       ++it) {
    if (it->timestamp_ms >= cutoff) {
      result.push_back(*it);
      if (result.size() >= max_count)
        break;
    }
  }
  return result;
}

EpisodicMemoryManager::Statistics EpisodicMemoryManager::getStatistics() const {
  std::shared_lock<std::shared_mutex> lock(episodes_mutex_);
  Statistics stats;
  stats.total_episodes_recorded = total_episodes_recorded_.load();
  stats.recent_episodes_count = recent_episodes_.size();
  // Count consolidated from patterns
  stats.consolidated_episodes_count = 0;
  for (const auto &p : patterns_) {
    if (p.consolidated)
      stats.consolidated_episodes_count++;
  }
  stats.total_consolidations = total_consolidations_.load();
  stats.total_retrievals = total_retrievals_.load();
  stats.successful_retrievals = successful_retrievals_.load();
  stats.context_categories_count = context_index_.size();
  stats.retrieval_success_rate =
      (stats.total_retrievals > 0)
          ? static_cast<float>(stats.successful_retrievals) /
                static_cast<float>(stats.total_retrievals)
          : 0.0f;

  // Pattern stats
  stats.total_patterns = patterns_.size();
  float total_sal = 0;
  for (const auto &p : patterns_)
    total_sal += p.salience;
  stats.average_pattern_salience =
      patterns_.empty() ? 0.0f
                        : total_sal / static_cast<float>(patterns_.size());
  stats.pattern_recalls = pattern_recalls_.load();

  return stats;
}

} // namespace Memory
} // namespace NeuroForge
