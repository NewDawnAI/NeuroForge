#include "memory/EpisodicMemoryManager.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <chrono>

namespace NeuroForge {
namespace Memory {

// Helper: cosine similarity for vectors
static float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || b.empty()) return 0.0f;
    const size_t n = std::min(a.size(), b.size());
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na <= 0.0f || nb <= 0.0f) return 0.0f;
    return dot / (std::sqrt(na) * std::sqrt(nb));
}

EpisodicMemoryManager::EpisodicMemoryManager(const EpisodicConfig& config)
    : config_(config) {}

std::uint64_t EpisodicMemoryManager::storeEpisode(const std::string& context,
                                                 const std::vector<float>& sensory_data,
                                                 const std::vector<float>& emotional_state,
                                                 const std::string& narrative) {
    const auto now = std::chrono::steady_clock::now();
    const std::uint64_t id = next_episode_id_++;

    auto ep = std::make_shared<Episode>();
    ep->id = id;
    ep->timestamp = now;
    ep->context = context;
    ep->sensory_data = sensory_data;
    ep->emotional_state = emotional_state;
    ep->narrative = narrative;
    // Initialize salience using emotional magnitude and simple sensory signal
    float emotional_mag = 0.0f;
    for (float v : emotional_state) emotional_mag += std::fabs(v);
    float sensory_mag = 0.0f;
    for (float v : sensory_data) sensory_mag += std::fabs(v);
    ep->salience = std::min(1.0f, (emotional_mag * 0.6f + sensory_mag * 0.4f) / 100.0f);
    ep->consolidated = false;

    episodes_[id] = ep;

    MemoryTrace trace;
    trace.episode_id = id;
    trace.activation_strength = ep->salience;
    trace.last_accessed = now;
    trace.access_count = 0;
    memory_traces_[id] = trace;

    // Enforce capacity if necessary
    if (episodes_.size() > config_.max_episodes) {
        pruneWeakMemories();
    }

    // Update extended statistics and recent tracking
    {
        std::lock_guard<std::mutex> lock(episodes_mutex_);
        // bump total episodes counter
        total_episodes_recorded_.fetch_add(1, std::memory_order_relaxed);
        // track recent episodes using EnhancedEpisode wrapper
        EnhancedEpisode ee;
        // Map available Episode fields into EnhancedEpisode structure
        ee.sensory_state = sensory_data;
        ee.action_state.clear();
        ee.substrate_state.clear();
        ee.context_tag = context;
        // Derive affect metrics from emotional_state
        float emotional_mag_tmp = 0.0f;
        for (float v : emotional_state) emotional_mag_tmp += std::fabs(v);
        ee.emotional_weight = emotional_mag_tmp;
        ee.reward_signal = 0.0f;
        // Use system clock for epoch-based timestamp
        const auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        ee.timestamp_ms = static_cast<std::uint64_t>(now_ms);
        ee.consolidation_strength = 0.0f;
        recent_episodes_.push_back(ee);
        // Index by episode id and context using recent_episodes_ position
        const size_t idx = recent_episodes_.size() - 1;
        episode_id_index_[id] = idx;
        context_index_[context].push_back(idx);
    }

    return id;
}

std::shared_ptr<Episode> EpisodicMemoryManager::retrieveEpisode(std::uint64_t episode_id) {
    auto it = episodes_.find(episode_id);
    if (it == episodes_.end()) return nullptr;
    // update trace
    auto mt = memory_traces_.find(episode_id);
    if (mt != memory_traces_.end()) {
        mt->second.access_count += 1;
        mt->second.last_accessed = std::chrono::steady_clock::now();
        // small reinforcement on access
        mt->second.activation_strength = std::min(1.0f, mt->second.activation_strength + 0.01f);
    }
    return it->second;
}

std::vector<std::shared_ptr<Episode>> EpisodicMemoryManager::searchEpisodes(const std::string& query,
                                                                            std::size_t max_results) {
    // naive search using context/narrative match and activation strength
    struct RankedEp { std::shared_ptr<Episode> ep; float score; };
    std::vector<RankedEp> ranked;
    const std::string q = query;
    for (const auto& kv : episodes_) {
        const auto& ep = kv.second;
        float text_score = 0.0f;
        if (!q.empty()) {
            if (ep->context.find(q) != std::string::npos) text_score += 0.6f;
            if (!ep->narrative.empty() && ep->narrative.find(q) != std::string::npos) text_score += 0.4f;
        }
        float act = 0.0f;
        auto it = memory_traces_.find(ep->id);
        if (it != memory_traces_.end()) act = it->second.activation_strength;
        float score = text_score * 0.7f + ep->salience * 0.2f + act * 0.1f;
        if (score > 0.0f) ranked.push_back({ep, score});
    }
    std::sort(ranked.begin(), ranked.end(), [](const RankedEp& a, const RankedEp& b){ return a.score > b.score; });
    if (ranked.size() > max_results) ranked.resize(max_results);
    std::vector<std::shared_ptr<Episode>> out;
    out.reserve(ranked.size());
    for (auto& r : ranked) out.push_back(r.ep);
    return out;
}

void EpisodicMemoryManager::consolidateMemories() {
    // Promote episodes exceeding threshold to consolidated
    for (auto& kv : episodes_) {
        auto& ep = kv.second;
        auto mt_it = memory_traces_.find(ep->id);
        float act = (mt_it != memory_traces_.end()) ? mt_it->second.activation_strength : 0.0f;
        float consolidation_score = ep->salience * 0.6f + act * 0.4f;
        if (consolidation_score >= config_.consolidation_threshold) {
            ep->consolidated = true;
            if (mt_it != memory_traces_.end()) {
                mt_it->second.activation_strength = std::min(1.0f, mt_it->second.activation_strength + 0.05f);
            }
        }
    }
    pruneWeakMemories();
}

void EpisodicMemoryManager::updateSalience(std::uint64_t episode_id, float salience) {
    auto it = episodes_.find(episode_id);
    if (it == episodes_.end()) return;
    it->second->salience = std::max(0.0f, std::min(1.0f, salience));
}

void EpisodicMemoryManager::forgetOldMemories() {
    if (!config_.enable_forgetting) return;
    decayMemoryTraces();
    pruneWeakMemories();
}

std::size_t EpisodicMemoryManager::getConsolidatedCount() const {
    std::size_t count = 0;
    for (const auto& kv : episodes_) {
        if (kv.second && kv.second->consolidated) ++count;
    }
    return count;
}

float EpisodicMemoryManager::getAverageActivation() const {
    if (memory_traces_.empty()) return 0.0f;
    float sum = 0.0f;
    for (const auto& kv : memory_traces_) sum += kv.second.activation_strength;
    return sum / static_cast<float>(memory_traces_.size());
}

void EpisodicMemoryManager::decayMemoryTraces() {
    for (auto& kv : memory_traces_) {
        kv.second.activation_strength = std::max(0.0f, kv.second.activation_strength - config_.decay_rate);
    }
}

float EpisodicMemoryManager::calculateSimilarity(const Episode& a, const Episode& b) const {
    // Combine sensory and emotional similarity with simple context bonus
    float s_sim = cosineSimilarity(a.sensory_data, b.sensory_data);
    float e_sim = cosineSimilarity(a.emotional_state, b.emotional_state);
    float c_bonus = (!a.context.empty() && a.context == b.context) ? 0.1f : 0.0f;
    float sim = 0.5f * s_sim + 0.5f * e_sim + c_bonus;
    return std::max(0.0f, std::min(1.0f, sim));
}

void EpisodicMemoryManager::pruneWeakMemories() {
    // If within capacity, nothing to do
    if (episodes_.size() <= config_.max_episodes) return;
    // Build list with score (lower score first)
    struct Scored { std::uint64_t id; float score; };
    std::vector<Scored> scores;
    scores.reserve(episodes_.size());
    for (const auto& kv : episodes_) {
        const auto& ep = kv.second;
        float act = 0.0f;
        auto it = memory_traces_.find(kv.first);
        if (it != memory_traces_.end()) act = it->second.activation_strength;
        float score = ep->salience * 0.6f + act * 0.4f;
        scores.push_back({kv.first, score});
    }
    std::sort(scores.begin(), scores.end(), [](const Scored& a, const Scored& b){ return a.score < b.score; });
    // Remove weakest until within capacity
    size_t to_remove = episodes_.size() - config_.max_episodes;
    for (size_t i = 0; i < to_remove && i < scores.size(); ++i) {
        episodes_.erase(scores[i].id);
        memory_traces_.erase(scores[i].id);
    }
}

EpisodicMemoryManager::Statistics EpisodicMemoryManager::getStatistics() const {
    std::lock_guard<std::mutex> lock(episodes_mutex_);
    Statistics stats;
    stats.total_episodes_recorded = total_episodes_recorded_.load();
    stats.recent_episodes_count = recent_episodes_.size();
    stats.consolidated_episodes_count = consolidated_episodes_.size();
    stats.total_consolidations = total_consolidations_.load();
    stats.total_retrievals = total_retrievals_.load();
    stats.successful_retrievals = successful_retrievals_.load();
    stats.context_categories_count = context_index_.size();
    // Calculate averages based on recent episodes if available
    if (!recent_episodes_.empty()) {
        std::uint64_t total_age = 0;
        float total_consolidation = 0.0f;
        for (const auto& episode : recent_episodes_) {
            total_age += episode.getAge();
            total_consolidation += episode.consolidation_strength;
        }
        stats.average_episode_age_ms = static_cast<float>(total_age) / recent_episodes_.size();
        stats.average_consolidation_strength = total_consolidation / recent_episodes_.size();
    }
    if (stats.total_retrievals > 0) {
        stats.retrieval_success_rate = static_cast<float>(stats.successful_retrievals) / stats.total_retrievals;
    }
    // Without interval tracking, expose consolidation activity as false
    stats.consolidation_active = false;
    return stats;
}

} // namespace Memory
} // namespace NeuroForge