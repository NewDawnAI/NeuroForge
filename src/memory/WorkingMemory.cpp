#include "memory/WorkingMemory.h"
#include "memory/EpisodicMemoryManager.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace NeuroForge {
namespace Memory {

// ─── Utility ───
static float local_cosine_sim(const std::vector<float> &a,
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

// ─── Constructor ───
WorkingMemory::WorkingMemory(const WorkingMemoryConfig &config)
    : config_(config), slot_dim_(config.slot_dim),
      last_update_(std::chrono::steady_clock::now()) {
  // Initialize slots
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    slots_[i].resize(slot_dim_, 0.0f);
    activation_levels_[i] = 0.0f;
    slot_occupied_[i] = false;
  }

  // Initialize gating weights (learned attention gate)
  gating_weights_.resize(slot_dim_, 1.0f / static_cast<float>(slot_dim_));
}

// ═══════════════════════════════════════════════════════════════
//  ACTIVATION BUFFER API (primary)
// ═══════════════════════════════════════════════════════════════

bool WorkingMemory::push(const std::vector<float> &representation,
                         float activation, const std::string &name) {
  if (activation < config_.push_threshold)
    return false;

  // Compute gating score
  float gate_score = computeGatingScore(representation);
  if (gate_score < config_.push_threshold)
    return false;

  // Find empty slot or weakest slot
  size_t target_slot = MAX_SLOTS;

  // First: look for empty slot
  for (size_t i = 0; i < config_.capacity && i < MAX_SLOTS; ++i) {
    if (!slot_occupied_[i]) {
      target_slot = i;
      break;
    }
  }

  // If no empty slot, find weakest
  if (target_slot >= MAX_SLOTS) {
    float weakest = activation_levels_[0];
    target_slot = 0;
    for (size_t i = 1; i < config_.capacity && i < MAX_SLOTS; ++i) {
      if (activation_levels_[i] < weakest) {
        weakest = activation_levels_[i];
        target_slot = i;
      }
    }
    // Only displace if incoming is stronger
    if (activation <= activation_levels_[target_slot])
      return false;
    statistics_.items_forgotten++;
  }

  // Store pattern in slot
  slots_[target_slot].resize(slot_dim_, 0.0f);
  for (size_t i = 0; i < std::min(representation.size(), slot_dim_); ++i) {
    slots_[target_slot][i] = representation[i];
  }
  activation_levels_[target_slot] = activation;
  slot_occupied_[target_slot] = true;

  // Update gating weights (strengthen attention to patterns that get stored)
  for (size_t i = 0; i < std::min(representation.size(), slot_dim_); ++i) {
    gating_weights_[i] +=
        config_.gating_lr * activation * std::abs(representation[i]);
  }

  // Recount active
  active_count_ = 0;
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    if (slot_occupied_[i])
      active_count_++;
  }

  statistics_.total_pushes++;
  statistics_.total_items_processed++;

  // Also maintain legacy items_ for backward compat
  auto item = std::make_shared<WorkingMemoryItem>();
  item->id = next_item_id_++;
  item->content = name;
  item->representation = representation;
  item->activation_level = activation;
  item->creation_time = std::chrono::steady_clock::now();
  item->last_access = item->creation_time;
  items_.push_back(item);
  item_lookup_[item->id] = item;
  enforceCapacityLimit();

  return true;
}

std::vector<float> WorkingMemory::getSlotContent(std::size_t slot_index) const {
  if (slot_index >= MAX_SLOTS || !slot_occupied_[slot_index])
    return {};
  return slots_[slot_index];
}

std::vector<float> WorkingMemory::getActiveContent() const {
  // Sum all active slot patterns weighted by activation
  std::vector<float> combined(slot_dim_, 0.0f);
  float total_activation = 0.0f;
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    if (slot_occupied_[i] &&
        activation_levels_[i] > config_.refresh_threshold) {
      for (size_t j = 0; j < slot_dim_ && j < slots_[i].size(); ++j) {
        combined[j] += activation_levels_[i] * slots_[i][j];
      }
      total_activation += activation_levels_[i];
    }
  }
  if (total_activation > 1e-8f) {
    for (auto &v : combined)
      v /= total_activation;
  }
  return combined;
}

std::vector<float> WorkingMemory::getMostActiveContent() const {
  size_t best = 0;
  float best_act = -1.0f;
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    if (slot_occupied_[i] && activation_levels_[i] > best_act) {
      best_act = activation_levels_[i];
      best = i;
    }
  }
  if (best_act < 0)
    return {};
  return slots_[best];
}

void WorkingMemory::decay(float delta_time) {
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    if (slot_occupied_[i]) {
      activation_levels_[i] *= (1.0f - config_.decay_rate * delta_time);
      if (activation_levels_[i] < config_.refresh_threshold * 0.1f) {
        slot_occupied_[i] = false;
        std::fill(slots_[i].begin(), slots_[i].end(), 0.0f);
        activation_levels_[i] = 0.0f;
        statistics_.items_forgotten++;
      }
    }
  }
  // Recount
  active_count_ = 0;
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    if (slot_occupied_[i])
      active_count_++;
  }
  // Also decay legacy items
  decayItems(delta_time);
}

bool WorkingMemory::refresh(std::size_t slot_index, float new_activation) {
  if (slot_index >= MAX_SLOTS || !slot_occupied_[slot_index])
    return false;
  activation_levels_[slot_index] = new_activation;
  return true;
}

std::size_t WorkingMemory::refreshBySimilarity(const std::vector<float> &query,
                                               float similarity_threshold,
                                               float activation_boost) {
  size_t refreshed = 0;
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    if (!slot_occupied_[i])
      continue;
    float sim = local_cosine_sim(query, slots_[i]);
    if (sim >= similarity_threshold) {
      activation_levels_[i] =
          std::min(1.0f, activation_levels_[i] + activation_boost);
      refreshed++;
    }
  }

  // Also refresh legacy items
  for (auto &item : items_) {
    float sim = local_cosine_sim(query, item->representation);
    if (sim >= similarity_threshold) {
      item->activation_level =
          std::min(1.0f, item->activation_level + activation_boost);
    }
  }
  return refreshed;
}

std::size_t WorkingMemory::findSimilarSlot(const std::vector<float> &query,
                                           float similarity_threshold) const {
  float best_sim = -1.0f;
  size_t best_idx = MAX_SLOTS;
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    if (!slot_occupied_[i])
      continue;
    float sim = local_cosine_sim(query, slots_[i]);
    if (sim >= similarity_threshold && sim > best_sim) {
      best_sim = sim;
      best_idx = i;
    }
  }
  return best_idx;
}

void WorkingMemory::loadGatingWeights(const std::vector<float> &weights) {
  gating_weights_ = weights;
  if (gating_weights_.size() < slot_dim_) {
    gating_weights_.resize(slot_dim_, 1.0f / static_cast<float>(slot_dim_));
  }
}

float WorkingMemory::getAverageActivation() const {
  if (active_count_ == 0)
    return 0.0f;
  float sum = 0.0f;
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    if (slot_occupied_[i])
      sum += activation_levels_[i];
  }
  return sum / static_cast<float>(active_count_);
}

float WorkingMemory::computeGatingScore(
    const std::vector<float> &pattern) const {
  // Dot product with gating weights (learned attention filter)
  float score = 0.0f;
  for (size_t i = 0; i < std::min(pattern.size(), slot_dim_); ++i) {
    score += gating_weights_[i] * std::abs(pattern[i]);
  }
  // Normalize by dim
  return score / std::max(1.0f, static_cast<float>(slot_dim_));
}

float WorkingMemory::cosineSim(const std::vector<float> &a,
                               const std::vector<float> &b) const {
  return local_cosine_sim(a, b);
}

// ═══════════════════════════════════════════════════════════════
//  LEGACY API (adapters)
// ═══════════════════════════════════════════════════════════════

std::uint64_t WorkingMemory::addItem(const std::string &content,
                                     const std::vector<float> &representation) {
  // Use push() internally
  float default_activation = 1.0f;
  push(representation, default_activation, content);

  // Return the last inserted item's id
  if (!items_.empty())
    return items_.back()->id;
  return 0;
}

std::shared_ptr<WorkingMemoryItem>
WorkingMemory::getItem(std::uint64_t item_id) {
  auto it = item_lookup_.find(item_id);
  if (it != item_lookup_.end()) {
    it->second->last_access = std::chrono::steady_clock::now();
    it->second->access_count++;
    return it->second;
  }
  return nullptr;
}

bool WorkingMemory::removeItem(std::uint64_t item_id) {
  auto it = item_lookup_.find(item_id);
  if (it == item_lookup_.end())
    return false;
  auto item = it->second;
  item_lookup_.erase(it);
  items_.erase(
      std::remove_if(items_.begin(), items_.end(),
                     [item_id](const auto &i) { return i->id == item_id; }),
      items_.end());
  statistics_.items_forgotten++;

  // Also clear the corresponding slot
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    if (slot_occupied_[i]) {
      float sim = local_cosine_sim(item->representation, slots_[i]);
      if (sim > 0.9f) {
        slot_occupied_[i] = false;
        std::fill(slots_[i].begin(), slots_[i].end(), 0.0f);
        activation_levels_[i] = 0.0f;
        break;
      }
    }
  }
  active_count_ = 0;
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    if (slot_occupied_[i])
      active_count_++;
  }
  return true;
}

void WorkingMemory::clear() {
  items_.clear();
  item_lookup_.clear();
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    std::fill(slots_[i].begin(), slots_[i].end(), 0.0f);
    activation_levels_[i] = 0.0f;
    slot_occupied_[i] = false;
  }
  active_count_ = 0;
}

float WorkingMemory::getUtilization() const {
  if (config_.capacity == 0)
    return 0.0f;
  return static_cast<float>(active_count_) /
         static_cast<float>(config_.capacity);
}

void WorkingMemory::updateActivations(float delta_time) { decay(delta_time); }

void WorkingMemory::rehearseItems() {
  if (!config_.enable_rehearsal)
    return;
  // Boost the most active items
  std::vector<std::pair<float, size_t>> sorted_slots;
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    if (slot_occupied_[i]) {
      sorted_slots.push_back({activation_levels_[i], i});
    }
  }
  std::sort(sorted_slots.begin(), sorted_slots.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });

  size_t rehearsed = 0;
  for (const auto &[act, idx] : sorted_slots) {
    if (rehearsed >= config_.max_rehearsal_items)
      break;
    activation_levels_[idx] =
        std::min(1.0f, activation_levels_[idx] + config_.rehearsal_boost);
    rehearsed++;
    statistics_.items_rehearsed++;
  }

  // Also update legacy items
  for (auto &item : items_) {
    if (rehearsed > 0 && item->activation_level > 0.5f) {
      item->activation_level =
          std::min(1.0f, item->activation_level + config_.rehearsal_boost);
      item->rehearsed = true;
    }
  }
}

void WorkingMemory::boostActivation(std::uint64_t item_id, float boost) {
  auto item = getItem(item_id);
  if (!item)
    return;
  item->activation_level = std::min(1.0f, item->activation_level + boost);

  // Also boost corresponding slot
  for (size_t i = 0; i < MAX_SLOTS; ++i) {
    if (slot_occupied_[i]) {
      float sim = local_cosine_sim(item->representation, slots_[i]);
      if (sim > 0.8f) {
        activation_levels_[i] = std::min(1.0f, activation_levels_[i] + boost);
        break;
      }
    }
  }
}

std::vector<std::shared_ptr<WorkingMemoryItem>>
WorkingMemory::getAllItems() const {
  return {items_.begin(), items_.end()};
}

std::vector<std::shared_ptr<WorkingMemoryItem>>
WorkingMemory::getActiveItems(float threshold) const {
  std::vector<std::shared_ptr<WorkingMemoryItem>> result;
  for (const auto &item : items_) {
    if (item->activation_level >= threshold)
      result.push_back(item);
  }
  return result;
}

std::shared_ptr<WorkingMemoryItem> WorkingMemory::findMostActive() const {
  std::shared_ptr<WorkingMemoryItem> best;
  float max_act = -1.0f;
  for (const auto &item : items_) {
    if (item->activation_level > max_act) {
      max_act = item->activation_level;
      best = item;
    }
  }
  return best;
}

std::shared_ptr<WorkingMemoryItem>
WorkingMemory::findByContent(const std::string &content) const {
  for (const auto &item : items_) {
    if (item->content == content)
      return item;
  }
  return nullptr;
}

void WorkingMemory::consolidateToLongTerm() {
  if (!episodic_memory_)
    return;
  // Transfer high-activation items to episodic memory
  for (auto it = items_.begin(); it != items_.end();) {
    if ((*it)->activation_level >= config_.consolidation_threshold) {
      // Legacy call — will be migrated to encodePattern() in future cleanup
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      episodic_memory_->storeEpisode((*it)->content, (*it)->representation, {});
#pragma GCC diagnostic pop
      it = items_.erase(it);
    } else {
      ++it;
    }
  }
}

void WorkingMemory::refreshItem(std::uint64_t item_id) {
  auto item = getItem(item_id);
  if (item) {
    item->activation_level = 1.0f;
    item->last_access = std::chrono::steady_clock::now();
  }
}

void WorkingMemory::forgetWeakestItem() {
  if (items_.empty())
    return;
  auto weakest = std::min_element(
      items_.begin(), items_.end(), [](const auto &a, const auto &b) {
        return a->activation_level < b->activation_level;
      });
  if (weakest != items_.end()) {
    item_lookup_.erase((*weakest)->id);
    items_.erase(weakest);
    statistics_.items_forgotten++;
  }
}

std::uint64_t
WorkingMemory::createChunk(const std::vector<std::uint64_t> &item_ids,
                           const std::string &chunk_name) {
  std::vector<std::pair<std::string, std::vector<float>>> constituents;
  std::vector<float> combined_rep;
  float max_activation = 0.0f;

  for (auto id : item_ids) {
    auto item = getItem(id);
    if (!item)
      continue;
    constituents.push_back({item->content, item->representation});
    if (combined_rep.empty()) {
      combined_rep = item->representation;
    } else {
      for (size_t i = 0;
           i < std::min(combined_rep.size(), item->representation.size());
           ++i) {
        combined_rep[i] += item->representation[i];
      }
    }
    max_activation = std::max(max_activation, item->activation_level);
  }

  // Normalize
  float norm = 0;
  for (auto v : combined_rep)
    norm += v * v;
  norm = std::sqrt(norm);
  if (norm > 1e-8f) {
    for (auto &v : combined_rep)
      v /= norm;
  }

  // Remove old items
  for (auto id : item_ids)
    removeItem(id);

  // Create chunk item
  auto chunk_id = addItem(chunk_name, combined_rep);
  auto chunk_item = getItem(chunk_id);
  if (chunk_item)
    chunk_item->activation_level = max_activation;

  chunk_constituents_[chunk_id] = std::move(constituents);
  return chunk_id;
}

void WorkingMemory::expandChunk(std::uint64_t chunk_id) {
  auto it = chunk_constituents_.find(chunk_id);
  if (it == chunk_constituents_.end())
    return;

  auto chunk_item = getItem(chunk_id);
  float chunk_activation = chunk_item ? chunk_item->activation_level : 0.5f;

  removeItem(chunk_id);

  for (const auto &[content, rep] : it->second) {
    push(rep, chunk_activation, content);
  }
  chunk_constituents_.erase(it);
}

void WorkingMemory::updateStatistics() {
  statistics_.current_load = active_count_;
  statistics_.capacity_utilization = getUtilization();
  statistics_.average_slot_activation = getAverageActivation();

  // Compute gating weight norm
  float norm = 0;
  for (auto w : gating_weights_)
    norm += w * w;
  statistics_.gating_weight_norm = std::sqrt(norm);
}

void WorkingMemory::enforceCapacityLimit() {
  while (items_.size() > config_.capacity) {
    forgetWeakestItem();
  }
}

void WorkingMemory::decayItems(float delta_time) {
  for (auto &item : items_) {
    item->activation_level *= (1.0f - config_.decay_rate * delta_time);
  }
  removeExpiredItems();
}

void WorkingMemory::removeExpiredItems() {
  auto now = std::chrono::steady_clock::now();
  items_.erase(std::remove_if(items_.begin(), items_.end(),
                              [&](const auto &item) {
                                bool expired = item->activation_level <
                                               config_.refresh_threshold * 0.1f;
                                if (expired) {
                                  item_lookup_.erase(item->id);
                                  statistics_.items_forgotten++;
                                }
                                return expired;
                              }),
               items_.end());
}

void WorkingMemory::selectItemsForRehearsal() {
  // Handled in rehearseItems()
}

float WorkingMemory::calculateRetentionProbability(
    const WorkingMemoryItem &item) const {
  auto now = std::chrono::steady_clock::now();
  auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                 now - item.creation_time)
                 .count();
  float age_factor = std::exp(-static_cast<float>(age) /
                              static_cast<float>(config_.decay_time.count()));
  float access_factor = 1.0f + 0.1f * static_cast<float>(item.access_count);
  float rehearsal_factor = item.rehearsed ? 1.5f : 1.0f;
  return std::min(1.0f, item.activation_level * age_factor * access_factor *
                            rehearsal_factor);
}

} // namespace Memory
} // namespace NeuroForge