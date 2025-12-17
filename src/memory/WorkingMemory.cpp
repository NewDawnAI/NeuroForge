// Implementation aligned with include/memory/WorkingMemory.h
#include "memory/WorkingMemory.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace NeuroForge {
namespace Memory {

static float cosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.empty() || b.empty()) return 0.0f;
    const size_t n = std::min(a.size(), b.size());
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (size_t i = 0; i < n; ++i) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    if (na == 0.0f || nb == 0.0f) return 0.0f;
    float sim = dot / (std::sqrt(na) * std::sqrt(nb));
    return std::max(0.0f, std::min(1.0f, sim));
}

WorkingMemory::WorkingMemory(const WorkingMemoryConfig& config)
    : config_(config), next_item_id_(1), last_update_(std::chrono::steady_clock::now()) {}

std::uint64_t WorkingMemory::addItem(const std::string& content, const std::vector<float>& representation) {
    // Enforce capacity before adding
    enforceCapacityLimit();

    auto item = std::make_shared<WorkingMemoryItem>();
    item->id = next_item_id_++;
    item->content = content;
    item->representation = representation;
    item->activation_level = 1.0f;
    item->creation_time = std::chrono::steady_clock::now();
    item->last_access = item->creation_time;
    item->access_count = 0;
    item->rehearsed = false;

    items_.push_back(item);
    item_lookup_[item->id] = item;

    statistics_.total_items_processed += 1;
    updateStatistics();
    return item->id;
}

std::shared_ptr<WorkingMemoryItem> WorkingMemory::getItem(std::uint64_t item_id) {
    auto it = item_lookup_.find(item_id);
    if (it == item_lookup_.end()) return nullptr;
    auto item = it->second;
    item->last_access = std::chrono::steady_clock::now();
    item->access_count += 1;
    return item;
}

bool WorkingMemory::removeItem(std::uint64_t item_id) {
    auto it = item_lookup_.find(item_id);
    if (it == item_lookup_.end()) return false;
    auto ptr = it->second;
    item_lookup_.erase(it);
    // Remove from deque
    for (auto d_it = items_.begin(); d_it != items_.end(); ++d_it) {
        if ((*d_it)->id == item_id) {
            items_.erase(d_it);
            statistics_.items_forgotten += 1;
            updateStatistics();
            return true;
        }
    }
    return false;
}

void WorkingMemory::clear() {
    items_.clear();
    item_lookup_.clear();
    statistics_.current_load = 0;
    statistics_.capacity_utilization = 0.0f;
}

float WorkingMemory::getUtilization() const {
    if (config_.capacity == 0) return 0.0f;
    return static_cast<float>(items_.size()) / static_cast<float>(config_.capacity);
}

void WorkingMemory::updateActivations(float delta_time) {
    decayItems(delta_time);
    removeExpiredItems();
    updateStatistics();
    last_update_ = std::chrono::steady_clock::now();
}

void WorkingMemory::rehearseItems() {
    if (!config_.enable_rehearsal || items_.empty()) return;
    // Select up to max_rehearsal_items with lowest activation to boost
    std::vector<std::shared_ptr<WorkingMemoryItem>> candidates(items_.begin(), items_.end());
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b){
        return a->activation_level < b->activation_level;
    });
    size_t count = std::min(config_.max_rehearsal_items, candidates.size());
    for (size_t i = 0; i < count; ++i) {
        auto& item = candidates[i];
        item->activation_level = std::min(1.0f, item->activation_level + config_.rehearsal_boost);
        item->rehearsed = true;
        statistics_.items_rehearsed += 1;
    }
    updateStatistics();
}

void WorkingMemory::boostActivation(std::uint64_t item_id, float boost) {
    auto it = item_lookup_.find(item_id);
    if (it == item_lookup_.end()) return;
    auto& item = it->second;
    item->activation_level = std::max(0.0f, std::min(1.0f, item->activation_level + boost));
    item->last_access = std::chrono::steady_clock::now();
}

std::vector<std::shared_ptr<WorkingMemoryItem>> WorkingMemory::getAllItems() const {
    return std::vector<std::shared_ptr<WorkingMemoryItem>>(items_.begin(), items_.end());
}

std::vector<std::shared_ptr<WorkingMemoryItem>> WorkingMemory::getActiveItems(float threshold) const {
    std::vector<std::shared_ptr<WorkingMemoryItem>> result;
    for (const auto& item : items_) {
        if (item->activation_level >= threshold) result.push_back(item);
    }
    return result;
}

std::shared_ptr<WorkingMemoryItem> WorkingMemory::findMostActive() const {
    if (items_.empty()) return nullptr;
    return *std::max_element(items_.begin(), items_.end(), [](const auto& a, const auto& b){
        return a->activation_level < b->activation_level;
    });
}

std::shared_ptr<WorkingMemoryItem> WorkingMemory::findByContent(const std::string& content) const {
    for (const auto& item : items_) {
        if (item->content == content) return item;
    }
    return nullptr;
}

void WorkingMemory::consolidateToLongTerm() {
    // Placeholder: interface hook. No-op in this module.
}

void WorkingMemory::refreshItem(std::uint64_t item_id) {
    auto it = item_lookup_.find(item_id);
    if (it == item_lookup_.end()) return;
    auto& item = it->second;
    item->last_access = std::chrono::steady_clock::now();
    item->activation_level = std::min(1.0f, item->activation_level + (config_.rehearsal_boost * 0.5f));
}

void WorkingMemory::forgetWeakestItem() {
    if (items_.empty()) return;
    auto weakest_it = std::min_element(items_.begin(), items_.end(), [](const auto& a, const auto& b){
        return a->activation_level < b->activation_level;
    });
    if (weakest_it != items_.end()) {
        item_lookup_.erase((*weakest_it)->id);
        items_.erase(weakest_it);
        statistics_.items_forgotten += 1;
    }
}

std::uint64_t WorkingMemory::createChunk(const std::vector<std::uint64_t>& item_ids, const std::string& chunk_name) {
    if (item_ids.empty()) return 0;
    // Aggregate representations by averaging
    std::vector<float> agg;
    size_t count = 0;
    for (auto id : item_ids) {
        auto it = item_lookup_.find(id);
        if (it == item_lookup_.end()) continue;
        const auto& rep = it->second->representation;
        if (rep.empty()) continue;
        if (agg.size() < rep.size()) agg.resize(rep.size(), 0.0f);
        for (size_t i = 0; i < rep.size(); ++i) agg[i] += rep[i];
        count++;
    }
    if (count > 0) {
        for (auto& v : agg) v /= static_cast<float>(count);
    }
    // Activation as average of members
    float act = 0.0f;
    for (auto id : item_ids) {
        auto it = item_lookup_.find(id);
        if (it != item_lookup_.end()) act += it->second->activation_level;
    }
    if (!item_ids.empty()) act /= static_cast<float>(item_ids.size());

    enforceCapacityLimit();
    auto chunk = std::make_shared<WorkingMemoryItem>();
    chunk->id = next_item_id_++;
    chunk->content = chunk_name;
    chunk->representation = agg;
    chunk->activation_level = std::max(0.0f, std::min(1.0f, act));
    chunk->creation_time = std::chrono::steady_clock::now();
    chunk->last_access = chunk->creation_time;

    items_.push_back(chunk);
    item_lookup_[chunk->id] = chunk;
    updateStatistics();
    return chunk->id;
}

void WorkingMemory::expandChunk(std::uint64_t /*chunk_id*/) {
    // Placeholder: expansion logic would re-insert constituent items.
}

void WorkingMemory::updateStatistics() {
    statistics_.current_load = items_.size();
    statistics_.capacity_utilization = getUtilization();
    // Average retention time in seconds
    if (!items_.empty()) {
        auto now = std::chrono::steady_clock::now();
        double total_sec = 0.0;
        for (const auto& item : items_) {
            auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(now - item->creation_time).count();
            total_sec += static_cast<double>(dur) / 1000.0;
        }
        statistics_.average_retention_time = static_cast<float>(total_sec / static_cast<double>(items_.size()));
    } else {
        statistics_.average_retention_time = 0.0f;
    }
}

void WorkingMemory::enforceCapacityLimit() {
    while (items_.size() >= config_.capacity && !items_.empty()) {
        forgetWeakestItem();
    }
}

void WorkingMemory::decayItems(float delta_time) {
    if (delta_time <= 0.0f) return;
    const float factor = std::exp(-config_.decay_rate * delta_time);
    for (auto& item : items_) {
        item->activation_level *= factor;
    }
}

void WorkingMemory::removeExpiredItems() {
    if (items_.empty()) return;
    const auto now = std::chrono::steady_clock::now();
    const auto expire = config_.decay_time;
    auto it = items_.begin();
    while (it != items_.end()) {
        auto age = now - (*it)->last_access;
        if (age > expire && (*it)->activation_level < 0.1f) {
            item_lookup_.erase((*it)->id);
            it = items_.erase(it);
            statistics_.items_forgotten += 1;
        } else {
            ++it;
        }
    }
}

void WorkingMemory::selectItemsForRehearsal() {
    // Selection handled inline in rehearseItems() for simplicity.
}

float WorkingMemory::calculateRetentionProbability(const WorkingMemoryItem& item) const {
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - item.last_access).count();
    const double decay_ms = static_cast<double>(config_.decay_time.count());
    if (decay_ms <= 0.0) return 0.0f;
    double time_factor = std::exp(-static_cast<double>(elapsed_ms) / decay_ms);
    double p = static_cast<double>(item.activation_level) * time_factor;
    return static_cast<float>(std::max(0.0, std::min(1.0, p)));
}

// Phase 2 compatibility wrappers implementation
bool WorkingMemory::push(const std::vector<float>& representation, float activation, const std::string& name) {
    if (representation.empty()) return false;
    if (activation < config_.push_threshold) return false;
    if (isFull()) return false;
    
    std::string content = name.empty() ? ("item_" + std::to_string(next_item_id_)) : name;
    auto id = addItem(content, representation);
    auto item = getItem(id);
    if (item) {
        item->activation_level = activation;
    }
    return id > 0;
}

float WorkingMemory::getAverageActivation() const {
    if (items_.empty()) return 0.0f;
    float sum = 0.0f;
    for (const auto& item : items_) sum += item->activation_level;
    return sum / static_cast<float>(items_.size());
}

std::vector<float> WorkingMemory::getSlotContent(std::size_t slot_index) const {
    if (slot_index >= items_.size()) return {};
    return items_[slot_index]->representation;
}

void WorkingMemory::decay(float delta_time) {
    if (delta_time <= 0.0f) return;
    updateActivations(delta_time);
}

bool WorkingMemory::refresh(std::size_t slot_index, float new_activation) {
    if (slot_index >= items_.size()) return false;
    auto& item = items_[slot_index];
    item->activation_level = std::max(item->activation_level, new_activation);
    item->last_access = std::chrono::steady_clock::now();
    return true;
}

std::size_t WorkingMemory::refreshBySimilarity(const std::vector<float>& query,
                                               float similarity_threshold,
                                               float activation_boost) {
    if (items_.empty()) return 0;
    std::size_t refreshed = 0;
    for (auto& item : items_) {
        float sim = cosineSimilarity(query, item->representation);
        if (sim >= similarity_threshold) {
            item->activation_level = std::min(1.0f, item->activation_level + activation_boost);
            item->last_access = std::chrono::steady_clock::now();
            ++refreshed;
        }
    }
    if (refreshed > 0) updateStatistics();
    return refreshed;
}

std::vector<float> WorkingMemory::getActiveContent() const {
    if (items_.empty()) return {};
    size_t dim = items_.front()->representation.size();
    if (dim == 0) return {};
    std::vector<float> agg(dim, 0.0f);
    float total_weight = 0.0f;
    for (const auto& item : items_) {
        if (item->representation.size() != dim) continue;
        float w = std::max(0.0f, item->activation_level);
        total_weight += w;
        for (size_t i = 0; i < dim; ++i) agg[i] += w * item->representation[i];
    }
    if (total_weight > 0.0f) {
        for (auto& v : agg) v /= total_weight;
    }
    return agg;
}

std::vector<float> WorkingMemory::getMostActiveContent() const {
    auto most = findMostActive();
    if (!most) return {};
    return most->representation;
}

std::size_t WorkingMemory::findSimilarSlot(const std::vector<float>& query, float similarity_threshold) const {
    if (items_.empty()) return static_cast<std::size_t>(-1);
    std::size_t best_idx = static_cast<std::size_t>(-1);
    float best_sim = similarity_threshold;
    for (std::size_t i = 0; i < items_.size(); ++i) {
        float sim = cosineSimilarity(query, items_[i]->representation);
        if (sim >= best_sim) {
            best_sim = sim;
            best_idx = i;
        }
    }
    return best_idx;
}

// getConfig is defined inline in the header; no out-of-line definition needed.

} // namespace Memory
} // namespace NeuroForge