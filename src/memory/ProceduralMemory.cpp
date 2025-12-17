#include "memory/ProceduralMemory.h"
#include <algorithm>
#include <numeric>
#include <cmath>

namespace NeuroForge {
namespace Memory {

ProceduralMemory::ProceduralMemory(const ProceduralConfig& config) : config_(config) {}

// Skill management
std::uint64_t ProceduralMemory::addSkill(const std::string& name,
                                         const std::vector<std::string>& action_sequence,
                                         const std::vector<float>& motor_pattern) {
    std::uint64_t id = next_skill_id_++;
    auto skill = std::make_shared<Skill>();
    skill->id = id;
    skill->name = name;
    skill->action_sequence = action_sequence;
    skill->motor_pattern = motor_pattern;
    skills_[id] = skill;
    skill_name_lookup_[name] = id;
    updateStatistics();
    return id;
}

std::shared_ptr<Skill> ProceduralMemory::getSkill(std::uint64_t skill_id) {
    auto it = skills_.find(skill_id);
    if (it != skills_.end()) return it->second;
    return nullptr;
}

std::shared_ptr<Skill> ProceduralMemory::findSkill(const std::string& name) {
    auto it = skill_name_lookup_.find(name);
    if (it == skill_name_lookup_.end()) return nullptr;
    return getSkill(it->second);
}

bool ProceduralMemory::removeSkill(std::uint64_t skill_id) {
    auto it = skills_.find(skill_id);
    if (it == skills_.end()) return false;
    skill_name_lookup_.erase(it->second->name);
    skills_.erase(it);
    updateStatistics();
    return true;
}

// Skill practice and learning
void ProceduralMemory::practiceSkill(std::uint64_t skill_id, float performance_score) {
    auto skill = getSkill(skill_id);
    if (!skill) return;
    updateProficiency(*skill, performance_score);
    statistics_.total_practice_sessions++;
}

void ProceduralMemory::practiceSkill(const std::string& skill_name, float performance_score) {
    auto skill = findSkill(skill_name);
    if (!skill) return;
    practiceSkill(skill->id, performance_score);
}

bool ProceduralMemory::executeSkill(std::uint64_t skill_id, std::vector<float>& output_commands) {
    auto skill = getSkill(skill_id);
    if (!skill) return false;
    // Simple execution: copy motor pattern
    output_commands = skill->motor_pattern;
    return true;
}

// Motor action management
void ProceduralMemory::addMotorAction(const std::string& action_name,
                                      const std::vector<float>& commands,
                                      float execution_time) {
    auto action = std::make_shared<MotorAction>();
    action->action_name = action_name;
    action->motor_commands = commands;
    action->execution_time = execution_time;
    motor_actions_[action_name] = action;
}

std::shared_ptr<MotorAction> ProceduralMemory::getMotorAction(const std::string& action_name) {
    auto it = motor_actions_.find(action_name);
    if (it != motor_actions_.end()) return it->second;
    return nullptr;
}

bool ProceduralMemory::executeMotorAction(const std::string& action_name, std::vector<float>& commands) {
    auto action = getMotorAction(action_name);
    if (!action) return false;
    commands = action->motor_commands;
    return true;
}

// Habit formation and management
std::uint64_t ProceduralMemory::startHabitFormation(const std::string& trigger_context,
                                                    const std::string& action) {
    std::uint64_t id = next_habit_id_++;
    auto habit = std::make_shared<Habit>();
    habit->id = id;
    habit->trigger_context = trigger_context;
    habit->habitual_action = action;
    habits_[id] = habit;
    context_habits_[trigger_context].push_back(id);
    updateStatistics();
    return id;
}

void ProceduralMemory::reinforceHabit(std::uint64_t habit_id) {
    auto it = habits_.find(habit_id);
    if (it == habits_.end()) return;
    auto& habit = it->second;
    habit->repetition_count++;
    habit->strength = std::min(1.0f, habit->strength + config_.learning_rate * 0.1f);
    updateStatistics();
}

void ProceduralMemory::reinforceHabit(const std::string& trigger_context) {
    auto vec_it = context_habits_.find(trigger_context);
    if (vec_it == context_habits_.end()) return;
    for (auto id : vec_it->second) reinforceHabit(id);
}

std::shared_ptr<Habit> ProceduralMemory::getTriggeredHabit(const std::string& context) {
    auto vec_it = context_habits_.find(context);
    if (vec_it == context_habits_.end()) return nullptr;
    for (auto id : vec_it->second) {
        auto habit = habits_[id];
        if (habit && habit->strength >= config_.habit_formation_threshold) return habit;
    }
    return nullptr;
}

// Automation and chunking
void ProceduralMemory::checkForAutomation() {
    std::vector<std::uint64_t> automated;
    for (const auto& kv : skills_) {
        const auto& s = kv.second;
        if (shouldAutomateSkill(*s)) automated.push_back(kv.first);
    }
    for (auto id : automated) {
        auto s = skills_[id];
        if (s) s->automated = true;
    }
    updateStatistics();
}

void ProceduralMemory::chunkActionSequence(std::uint64_t skill_id, const std::vector<std::size_t>& chunk_indices) {
    auto skill = getSkill(skill_id);
    if (!skill) return;
    // No-op placeholder; chunking could rearrange action_sequence if implemented.
    (void)chunk_indices;
}

std::vector<std::uint64_t> ProceduralMemory::getAutomatedSkills() const {
    std::vector<std::uint64_t> ids;
    for (const auto& kv : skills_) if (kv.second && kv.second->automated) ids.push_back(kv.first);
    return ids;
}

// Skill transfer and generalization
void ProceduralMemory::transferSkill(std::uint64_t source_skill_id, std::uint64_t target_skill_id, float transfer_amount) {
    auto src = getSkill(source_skill_id);
    auto dst = getSkill(target_skill_id);
    if (!src || !dst) return;
    dst->proficiency_level = std::min(1.0f, dst->proficiency_level + src->proficiency_level * transfer_amount);
}

std::vector<std::uint64_t> ProceduralMemory::findSimilarSkills(std::uint64_t skill_id, float similarity_threshold) {
    std::vector<std::uint64_t> result;
    auto ref = getSkill(skill_id);
    if (!ref) return result;
    for (const auto& kv : skills_) {
        if (kv.first == skill_id) continue;
        if (calculateSimilarity(*ref, *kv.second) >= similarity_threshold) result.push_back(kv.first);
    }
    return result;
}

// Memory maintenance
void ProceduralMemory::decayUnusedSkills(float decay_rate) {
    for (auto& kv : skills_) {
        auto& s = kv.second;
        s->proficiency_level = std::max(0.0f, s->proficiency_level - decay_rate);
    }
    updateStatistics();
}

void ProceduralMemory::strengthenFrequentlyUsed() {
    for (auto& kv : skills_) {
        auto& s = kv.second;
        if (s->practice_count > 10) s->proficiency_level = std::min(1.0f, s->proficiency_level + 0.01f);
    }
    updateStatistics();
}

void ProceduralMemory::consolidateMotorMemories() {
    // Placeholder consolidation: mark automated if high proficiency
    for (auto& kv : skills_) {
        auto& s = kv.second;
        if (s->proficiency_level >= config_.automation_threshold) s->automated = true;
    }
    updateStatistics();
}

// Retrieval and search
std::vector<std::shared_ptr<Skill>> ProceduralMemory::getAllSkills() const {
    std::vector<std::shared_ptr<Skill>> out;
    out.reserve(skills_.size());
    for (const auto& kv : skills_) out.push_back(kv.second);
    return out;
}

std::vector<std::shared_ptr<Skill>> ProceduralMemory::getSkillsByProficiency(float min_proficiency) const {
    std::vector<std::shared_ptr<Skill>> out;
    for (const auto& kv : skills_) if (kv.second && kv.second->proficiency_level >= min_proficiency) out.push_back(kv.second);
    return out;
}

std::vector<std::shared_ptr<Habit>> ProceduralMemory::getActiveHabits() const {
    std::vector<std::shared_ptr<Habit>> out;
    for (const auto& kv : habits_) if (kv.second && kv.second->strength >= config_.habit_formation_threshold) out.push_back(kv.second);
    return out;
}

// Statistics and monitoring
void ProceduralMemory::updateStatistics() {
    statistics_.total_skills = skills_.size();
    statistics_.automated_skills = 0;
    statistics_.active_habits = 0;
    float total_prof = 0.0f;
    for (const auto& kv : skills_) {
        if (kv.second) {
            total_prof += kv.second->proficiency_level;
            if (kv.second->automated) statistics_.automated_skills++;
        }
    }
    for (const auto& kv : habits_) if (kv.second && kv.second->strength >= config_.habit_formation_threshold) statistics_.active_habits++;
    statistics_.average_proficiency = statistics_.total_skills ? (total_prof / static_cast<float>(statistics_.total_skills)) : 0.0f;
}

float ProceduralMemory::getOverallProficiency() const {
    return statistics_.average_proficiency;
}

// Private helpers
float ProceduralMemory::calculateSimilarity(const Skill& a, const Skill& b) const {
    if (a.action_sequence.empty() || b.action_sequence.empty()) return 0.0f;
    size_t match = 0;
    size_t max_len = std::min(a.action_sequence.size(), b.action_sequence.size());
    for (size_t i = 0; i < max_len; ++i) if (a.action_sequence[i] == b.action_sequence[i]) match++;
    return static_cast<float>(match) / static_cast<float>(std::max(a.action_sequence.size(), b.action_sequence.size()));
}

void ProceduralMemory::updateProficiency(Skill& skill, float performance_score) {
    skill.practice_count++;
    skill.proficiency_level = std::min(1.0f, std::max(0.0f, skill.proficiency_level + (performance_score - 0.5f) * config_.learning_rate));
}

bool ProceduralMemory::shouldAutomateSkill(const Skill& skill) const {
    return skill.proficiency_level >= config_.automation_threshold && skill.practice_count >= config_.min_repetitions_for_habit;
}

void ProceduralMemory::processHabitFormation() {
    // Placeholder: reinforce all habits slightly over time
    for (auto& kv : habits_) if (kv.second) kv.second->strength = std::min(1.0f, kv.second->strength + 0.001f);
}

} // namespace Memory
} // namespace NeuroForge