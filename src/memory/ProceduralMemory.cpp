#include "memory/ProceduralMemory.h"
// This file implements the deprecated Skill/MotorAction/Habit legacy API.
// Suppress deprecation warnings — these are known deprecated paths.
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <algorithm>
#include <cmath>
#include <functional>
#include <numeric>
#include <stdexcept>

namespace NeuroForge {
namespace Memory {

// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
//  Constructor
// â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
ProceduralMemory::ProceduralMemory(const ProceduralConfig &config)
    : config_(config), context_dim_(config.context_dim),
      action_dim_(config.action_dim), rng_(std::random_device{}()) {
  // Initialize weight matrix to small random values (Xavier init)
  weights_.resize(context_dim_ * action_dim_);
  float scale =
      std::sqrt(2.0f / static_cast<float>(context_dim_ + action_dim_));
  std::normal_distribution<float> dist(0.0f, scale);
  for (auto &w : weights_) {
    w = dist(rng_);
  }
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  WEIGHT-BASED API  (the real memory)
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

void ProceduralMemory::reinforce(const std::vector<float> &context,
                                 const std::vector<float> &action,
                                 float reward) {
  // Ensure dimensions match (pad/truncate if needed)
  std::vector<float> ctx(context_dim_, 0.0f);
  std::vector<float> act(action_dim_, 0.0f);
  for (size_t i = 0; i < std::min(context.size(), context_dim_); ++i)
    ctx[i] = context[i];
  for (size_t i = 0; i < std::min(action.size(), action_dim_); ++i)
    act[i] = action[i];

  // Hebbian-like weight update: Î”W = Î· Â· reward Â· context âŠ— action
  float eta = config_.learning_rate;
  {
    std::lock_guard<std::shared_mutex> lock(weights_mutex_);
    for (size_t c = 0; c < context_dim_; ++c) {
      for (size_t a = 0; a < action_dim_; ++a) {
        weights_[c * action_dim_ + a] += eta * reward * ctx[c] * act[a];
      }
    }
  }

  // Record trace
  ProceduralTrace trace;
  trace.context_pattern = ctx;
  trace.action_pattern = act;
  trace.strength = std::abs(reward);
  trace.practice_count = 1;
  trace.last_practiced = std::chrono::steady_clock::now();
  trace.automated = false;

  // Check if similar trace already exists â€” merge instead of adding
  int closest = findClosestTrace(ctx, 0.9f);
  if (closest >= 0) {
    auto &existing = traces_[static_cast<size_t>(closest)];
    existing.strength += std::abs(reward) * 0.1f;
    existing.practice_count++;
    existing.last_practiced = std::chrono::steady_clock::now();
    // Blend action patterns
    float blend = 1.0f / static_cast<float>(existing.practice_count + 1);
    for (size_t i = 0; i < action_dim_; ++i) {
      existing.action_pattern[i] =
          (1.0f - blend) * existing.action_pattern[i] + blend * act[i];
    }
    if (existing.strength >= config_.automation_threshold) {
      existing.automated = true;
    }
  } else {
    traces_.push_back(std::move(trace));
  }

  statistics_.total_reinforcements++;
  statistics_.total_practice_sessions++;
}

std::vector<float>
ProceduralMemory::recall(const std::vector<float> &context) const {
  std::vector<float> ctx(context_dim_, 0.0f);
  for (size_t i = 0; i < std::min(context.size(), context_dim_); ++i)
    ctx[i] = context[i];

  // Matrix multiply: action = context Â· W
  std::vector<float> action(action_dim_, 0.0f);
  {
    std::lock_guard<std::shared_mutex> lock(weights_mutex_);
    for (size_t c = 0; c < context_dim_; ++c) {
      for (size_t a = 0; a < action_dim_; ++a) {
        action[a] += ctx[c] * weights_[c * action_dim_ + a];
      }
    }
  }

  // Apply tanh activation for bounded output
  for (auto &v : action) {
    v = std::tanh(v);
  }

  statistics_.total_recalls++;
  return action;
}

void ProceduralMemory::loadTraces(const std::vector<ProceduralTrace> &traces) {
  traces_ = traces;
  // Reconstruct weight matrix from traces via replayed reinforcement
  std::fill(weights_.begin(), weights_.end(), 0.0f);
  for (const auto &trace : traces_) {
    float eta = config_.learning_rate;
    for (size_t c = 0; c < context_dim_ && c < trace.context_pattern.size();
         ++c) {
      for (size_t a = 0; a < action_dim_ && a < trace.action_pattern.size();
           ++a) {
        weights_[c * action_dim_ + a] += eta * trace.strength *
                                         trace.context_pattern[c] *
                                         trace.action_pattern[a];
      }
    }
  }
}

void ProceduralMemory::loadWeights(const std::vector<float> &weights,
                                   std::size_t ctx_dim, std::size_t act_dim) {
  if (weights.size() != ctx_dim * act_dim) {
    throw std::invalid_argument("Weight matrix size mismatch");
  }
  std::lock_guard<std::shared_mutex> lock(weights_mutex_);
  weights_ = weights;
  context_dim_ = ctx_dim;
  action_dim_ = act_dim;
}

void ProceduralMemory::applyWeightDecay(float decay_rate) {
  float rate = (decay_rate < 0) ? config_.weight_decay : decay_rate;
  std::lock_guard<std::shared_mutex> lock(weights_mutex_);
  for (auto &w : weights_) {
    w *= (1.0f - rate);
  }
}

int ProceduralMemory::findClosestTrace(const std::vector<float> &context,
                                       float similarity_threshold) const {
  int best_idx = -1;
  float best_sim = similarity_threshold;

  for (size_t i = 0; i < traces_.size(); ++i) {
    float sim = cosineSimilarity(context, traces_[i].context_pattern);
    if (sim > best_sim) {
      best_sim = sim;
      best_idx = static_cast<int>(i);
    }
  }
  return best_idx;
}

float ProceduralMemory::getWeightNorm() const {
  std::shared_lock<std::shared_mutex> lock(weights_mutex_);
  float norm = 0.0f;
  for (auto w : weights_)
    norm += w * w;
  return std::sqrt(norm);
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  UTILITY
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

float ProceduralMemory::cosineSimilarity(const std::vector<float> &a,
                                         const std::vector<float> &b) const {
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

/// Deterministic hash-based encoding: string â†’ float vector
std::vector<float> ProceduralMemory::encodeString(const std::string &s) const {
  std::vector<float> vec(context_dim_, 0.0f);
  if (s.empty())
    return vec;

  // Distribute characters across dimensions using hash
  std::hash<std::string> hasher;
  size_t h = hasher(s);
  std::mt19937 local_rng(static_cast<unsigned>(h));
  std::normal_distribution<float> dist(0.0f, 1.0f);
  for (size_t i = 0; i < context_dim_; ++i) {
    vec[i] = dist(local_rng);
  }
  // Normalize to unit vector
  float norm = 0;
  for (auto v : vec)
    norm += v * v;
  norm = std::sqrt(norm);
  if (norm > 1e-8f) {
    for (auto &v : vec)
      v /= norm;
  }
  return vec;
}

std::vector<float> ProceduralMemory::encodeActionSequence(
    const std::vector<std::string> &actions) const {
  std::vector<float> vec(action_dim_, 0.0f);
  for (const auto &act : actions) {
    auto encoded = encodeString(act);
    // Resize/truncate to action_dim
    for (size_t i = 0; i < action_dim_ && i < encoded.size(); ++i) {
      vec[i] += encoded[i];
    }
  }
  // Normalize
  float norm = 0;
  for (auto v : vec)
    norm += v * v;
  norm = std::sqrt(norm);
  if (norm > 1e-8f) {
    for (auto &v : vec)
      v /= norm;
  }
  return vec;
}

// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
//  LEGACY SKILL API  (adapters to weight-based operations)
// â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•

std::uint64_t
ProceduralMemory::addSkill(const std::string &name,
                           const std::vector<std::string> &action_sequence,
                           const std::vector<float> &motor_pattern) {
  auto skill = std::make_shared<Skill>();
  skill->id = next_skill_id_++;
  skill->name = name;
  skill->action_sequence = action_sequence;
  skill->motor_pattern = motor_pattern;
  skill->last_practiced = std::chrono::steady_clock::now();

  skills_[skill->id] = skill;
  skill_name_lookup_[name] = skill->id;

  // â”€â”€â”€ ALSO reinforce weight matrix â”€â”€â”€
  auto ctx = encodeString(name);
  auto act = motor_pattern.empty() ? encodeActionSequence(action_sequence)
                                   : motor_pattern;
  reinforce(ctx, act, 1.0f);

  statistics_.total_skills = skills_.size();
  return skill->id;
}

std::shared_ptr<Skill> ProceduralMemory::getSkill(std::uint64_t skill_id) {
  auto it = skills_.find(skill_id);
  return (it != skills_.end()) ? it->second : nullptr;
}

std::shared_ptr<Skill> ProceduralMemory::findSkill(const std::string &name) {
  auto it = skill_name_lookup_.find(name);
  return (it != skill_name_lookup_.end()) ? getSkill(it->second) : nullptr;
}

bool ProceduralMemory::removeSkill(std::uint64_t skill_id) {
  auto it = skills_.find(skill_id);
  if (it == skills_.end())
    return false;
  skill_name_lookup_.erase(it->second->name);
  skills_.erase(it);
  statistics_.total_skills = skills_.size();
  return true;
}

void ProceduralMemory::practiceSkill(std::uint64_t skill_id,
                                     float performance_score) {
  auto skill = getSkill(skill_id);
  if (!skill)
    return;
  updateProficiency(*skill, performance_score);
  skill->practice_count++;
  skill->last_practiced = std::chrono::steady_clock::now();

  // â”€â”€â”€ ALSO reinforce weight matrix â”€â”€â”€
  auto ctx = encodeString(skill->name);
  auto act = skill->motor_pattern.empty()
                 ? encodeActionSequence(skill->action_sequence)
                 : skill->motor_pattern;
  reinforce(ctx, act, performance_score);

  statistics_.total_practice_sessions++;
}

void ProceduralMemory::practiceSkill(const std::string &skill_name,
                                     float performance_score) {
  auto skill = findSkill(skill_name);
  if (skill)
    practiceSkill(skill->id, performance_score);
}

bool ProceduralMemory::executeSkill(std::uint64_t skill_id,
                                    std::vector<float> &output_commands) {
  auto skill = getSkill(skill_id);
  if (!skill)
    return false;

  // Use weight-based recall for the skill
  auto ctx = encodeString(skill->name);
  output_commands = recall(ctx);

  skill->last_practiced = std::chrono::steady_clock::now();
  return true;
}

// â”€â”€â”€ Motor action management (legacy, unchanged) â”€â”€â”€

void ProceduralMemory::addMotorAction(const std::string &action_name,
                                      const std::vector<float> &commands,
                                      float execution_time) {
  auto action = std::make_shared<MotorAction>();
  action->action_name = action_name;
  action->motor_commands = commands;
  action->execution_time = execution_time;
  motor_actions_[action_name] = action;
}

std::shared_ptr<MotorAction>
ProceduralMemory::getMotorAction(const std::string &action_name) {
  auto it = motor_actions_.find(action_name);
  return (it != motor_actions_.end()) ? it->second : nullptr;
}

bool ProceduralMemory::executeMotorAction(const std::string &action_name,
                                          std::vector<float> &commands) {
  auto action = getMotorAction(action_name);
  if (!action)
    return false;
  commands = action->motor_commands;
  return true;
}

// â”€â”€â”€ Habit formation (legacy, unchanged) â”€â”€â”€

std::uint64_t
ProceduralMemory::startHabitFormation(const std::string &trigger_context,
                                      const std::string &action) {
  auto habit = std::make_shared<Habit>();
  habit->id = next_habit_id_++;
  habit->trigger_context = trigger_context;
  habit->habitual_action = action;
  habit->formation_start = std::chrono::steady_clock::now();
  habits_[habit->id] = habit;
  context_habits_[trigger_context].push_back(habit->id);
  return habit->id;
}

void ProceduralMemory::reinforceHabit(std::uint64_t habit_id) {
  auto it = habits_.find(habit_id);
  if (it == habits_.end())
    return;
  auto &habit = it->second;
  habit->repetition_count++;
  habit->strength =
      std::min(1.0f, habit->strength + config_.learning_rate * 0.1f);

  // â”€â”€â”€ ALSO reinforce weight matrix â”€â”€â”€
  auto ctx = encodeString(habit->trigger_context);
  auto act = encodeString(habit->habitual_action);
  // Truncate act to action_dim
  act.resize(action_dim_, 0.0f);
  reinforce(ctx, act, 0.5f);
}

void ProceduralMemory::reinforceHabit(const std::string &trigger_context) {
  auto it = context_habits_.find(trigger_context);
  if (it == context_habits_.end())
    return;
  for (auto id : it->second)
    reinforceHabit(id);
}

std::shared_ptr<Habit>
ProceduralMemory::getTriggeredHabit(const std::string &context) {
  auto it = context_habits_.find(context);
  if (it == context_habits_.end() || it->second.empty())
    return nullptr;

  std::shared_ptr<Habit> best;
  float best_strength = -1.0f;
  for (auto id : it->second) {
    auto hit = habits_.find(id);
    if (hit != habits_.end() && hit->second->strength > best_strength) {
      best_strength = hit->second->strength;
      best = hit->second;
    }
  }
  return best;
}

// â”€â”€â”€ Automation and chunking â”€â”€â”€

void ProceduralMemory::checkForAutomation() {
  for (auto &[id, skill] : skills_) {
    if (!skill->automated && shouldAutomateSkill(*skill)) {
      skill->automated = true;
      statistics_.automated_skills++;
    }
  }
  // Also check traces
  for (auto &trace : traces_) {
    if (!trace.automated && trace.strength >= config_.automation_threshold) {
      trace.automated = true;
    }
  }
}

void ProceduralMemory::chunkActionSequence(
    std::uint64_t skill_id, const std::vector<std::size_t> &chunk_indices) {
  auto skill = getSkill(skill_id);
  if (!skill || chunk_indices.size() < 2)
    return;

  // Build chunked action sequence
  std::vector<std::string> chunked_actions;
  std::string chunk;
  std::size_t chunk_start = 0;

  for (auto idx : chunk_indices) {
    if (idx >= skill->action_sequence.size())
      continue;
    // Concatenate actions up to this index
    for (std::size_t i = chunk_start;
         i <= idx && i < skill->action_sequence.size(); ++i) {
      if (!chunk.empty())
        chunk += "+";
      chunk += skill->action_sequence[i];
    }
    chunked_actions.push_back(chunk);
    chunk.clear();
    chunk_start = idx + 1;
  }
  // Remaining
  for (std::size_t i = chunk_start; i < skill->action_sequence.size(); ++i) {
    chunked_actions.push_back(skill->action_sequence[i]);
  }

  skill->action_sequence = chunked_actions;
}

std::vector<std::uint64_t> ProceduralMemory::getAutomatedSkills() const {
  std::vector<std::uint64_t> result;
  for (const auto &[id, skill] : skills_) {
    if (skill->automated)
      result.push_back(id);
  }
  return result;
}

// â”€â”€â”€ Skill transfer (weight-space transfer) â”€â”€â”€

void ProceduralMemory::transferSkill(std::uint64_t source_skill_id,
                                     std::uint64_t target_skill_id,
                                     float transfer_amount) {
  auto src = getSkill(source_skill_id);
  auto tgt = getSkill(target_skill_id);
  if (!src || !tgt)
    return;

  // Weight-based transfer: blend source's recalled action into target's
  // reinforcement
  auto src_ctx = encodeString(src->name);
  auto src_action = recall(src_ctx);
  auto tgt_ctx = encodeString(tgt->name);
  reinforce(tgt_ctx, src_action, transfer_amount);

  tgt->proficiency_level += transfer_amount * src->proficiency_level;
  tgt->proficiency_level = std::min(1.0f, tgt->proficiency_level);
}

std::vector<std::uint64_t>
ProceduralMemory::findSimilarSkills(std::uint64_t skill_id,
                                    float similarity_threshold) {
  auto skill = getSkill(skill_id);
  if (!skill)
    return {};

  auto ctx = encodeString(skill->name);
  std::vector<std::uint64_t> result;
  for (const auto &[id, other] : skills_) {
    if (id == skill_id)
      continue;
    auto other_ctx = encodeString(other->name);
    if (cosineSimilarity(ctx, other_ctx) >= similarity_threshold) {
      result.push_back(id);
    }
  }
  return result;
}

// â”€â”€â”€ Memory maintenance â”€â”€â”€

void ProceduralMemory::decayUnusedSkills(float decay_rate) {
  for (auto &[id, skill] : skills_) {
    skill->proficiency_level *= (1.0f - decay_rate);
  }
  // Also decay weight matrix
  applyWeightDecay(decay_rate);
}

void ProceduralMemory::strengthenFrequentlyUsed() {
  for (auto &[id, skill] : skills_) {
    if (skill->practice_count > 5) {
      float boost =
          std::min(0.05f, static_cast<float>(skill->practice_count) * 0.001f);
      skill->proficiency_level =
          std::min(1.0f, skill->proficiency_level + boost);
    }
  }
}

void ProceduralMemory::consolidateMotorMemories() {
  for (auto &[id, skill] : skills_) {
    if (skill->proficiency_level > 0.5f && !skill->motor_pattern.empty()) {
      // Reinforce consolidated skills in weight matrix
      auto ctx = encodeString(skill->name);
      reinforce(ctx, skill->motor_pattern, skill->proficiency_level * 0.1f);
    }
  }
}

// â”€â”€â”€ Retrieval and search â”€â”€â”€

std::vector<std::shared_ptr<Skill>> ProceduralMemory::getAllSkills() const {
  std::vector<std::shared_ptr<Skill>> result;
  result.reserve(skills_.size());
  for (const auto &[id, skill] : skills_) {
    result.push_back(skill);
  }
  return result;
}

std::vector<std::shared_ptr<Skill>>
ProceduralMemory::getSkillsByProficiency(float min_proficiency) const {
  std::vector<std::shared_ptr<Skill>> result;
  for (const auto &[id, skill] : skills_) {
    if (skill->proficiency_level >= min_proficiency) {
      result.push_back(skill);
    }
  }
  return result;
}

std::vector<std::shared_ptr<Habit>> ProceduralMemory::getActiveHabits() const {
  std::vector<std::shared_ptr<Habit>> result;
  for (const auto &[id, habit] : habits_) {
    if (habit->strength >= config_.habit_formation_threshold) {
      result.push_back(habit);
    }
  }
  return result;
}

// â”€â”€â”€ Statistics â”€â”€â”€

void ProceduralMemory::updateStatistics() {
  statistics_.total_skills = skills_.size();
  statistics_.automated_skills = 0;
  float total_proficiency = 0.0f;
  for (const auto &[id, skill] : skills_) {
    if (skill->automated)
      statistics_.automated_skills++;
    total_proficiency += skill->proficiency_level;
  }
  statistics_.average_proficiency =
      skills_.empty() ? 0.0f
                      : total_proficiency / static_cast<float>(skills_.size());

  statistics_.active_habits = 0;
  for (const auto &[id, habit] : habits_) {
    if (habit->strength >= config_.habit_formation_threshold)
      statistics_.active_habits++;
  }

  // Weight-based stats
  statistics_.total_traces = traces_.size();
  float total_strength = 0.0f;
  for (const auto &t : traces_)
    total_strength += t.strength;
  statistics_.average_trace_strength =
      traces_.empty() ? 0.0f
                      : total_strength / static_cast<float>(traces_.size());
  statistics_.weight_matrix_norm = getWeightNorm();
}

float ProceduralMemory::getOverallProficiency() const {
  if (skills_.empty())
    return 0.0f;
  float total = 0.0f;
  for (const auto &[id, skill] : skills_) {
    total += skill->proficiency_level;
  }
  return total / static_cast<float>(skills_.size());
}

// â”€â”€â”€ Legacy private helpers â”€â”€â”€

float ProceduralMemory::calculateSimilarity(const Skill &a,
                                            const Skill &b) const {
  if (a.motor_pattern.empty() || b.motor_pattern.empty())
    return 0.0f;
  return cosineSimilarity(a.motor_pattern, b.motor_pattern);
}

void ProceduralMemory::updateProficiency(Skill &skill,
                                         float performance_score) {
  float alpha = config_.learning_rate;
  skill.proficiency_level =
      (1.0f - alpha) * skill.proficiency_level + alpha * performance_score;
  skill.proficiency_level = std::clamp(skill.proficiency_level, 0.0f, 1.0f);
}

bool ProceduralMemory::shouldAutomateSkill(const Skill &skill) const {
  return skill.proficiency_level >= config_.automation_threshold &&
         skill.practice_count >= config_.min_repetitions_for_habit;
}

void ProceduralMemory::processHabitFormation() {
  for (auto &[id, habit] : habits_) {
    if (habit->repetition_count >= config_.min_repetitions_for_habit) {
      habit->strength =
          std::max(habit->strength, config_.habit_formation_threshold);
    }
  }
}

} // namespace Memory
} // namespace NeuroForge
