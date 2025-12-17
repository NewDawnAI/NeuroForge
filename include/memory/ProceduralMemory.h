#pragma once

#include "core/Types.h"
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

namespace NeuroForge {
namespace Memory {

/**
 * @brief Procedural skill representation
 */
struct Skill {
    std::uint64_t id;
    std::string name;
    std::vector<std::string> action_sequence;
    std::vector<float> motor_pattern;
    float proficiency_level = 0.0f;
    std::uint32_t practice_count = 0;
    std::chrono::steady_clock::time_point last_practiced;
    bool automated = false;
};

/**
 * @brief Motor action representation
 */
struct MotorAction {
    std::string action_name;
    std::vector<float> motor_commands;
    float execution_time = 0.0f;
    float success_rate = 1.0f;
    std::vector<std::string> prerequisites;
};

/**
 * @brief Habit formation tracking
 */
struct Habit {
    std::uint64_t id;
    std::string trigger_context;
    std::string habitual_action;
    float strength = 0.0f;
    std::uint32_t repetition_count = 0;
    std::chrono::steady_clock::time_point formation_start;
};

/**
 * @brief Procedural memory configuration
 */
struct ProceduralConfig {
    std::size_t max_skills = 1000;
    std::size_t max_habits = 500;
    float learning_rate = 0.1f;
    float automation_threshold = 0.8f;
    float habit_formation_threshold = 0.7f;
    std::uint32_t min_repetitions_for_habit = 21;
};

/**
 * @brief Procedural memory statistics
 */
struct ProceduralStats {
    std::size_t total_skills = 0;
    std::size_t automated_skills = 0;
    std::size_t active_habits = 0;
    float average_proficiency = 0.0f;
    std::uint32_t total_practice_sessions = 0;
};

/**
 * @brief Manages procedural memory for skills, habits, and motor patterns
 */
class ProceduralMemory {
public:
    explicit ProceduralMemory(const ProceduralConfig& config = ProceduralConfig{});
    ~ProceduralMemory() = default;

    // Skill management
    std::uint64_t addSkill(const std::string& name, const std::vector<std::string>& action_sequence,
                          const std::vector<float>& motor_pattern = {});
    std::shared_ptr<Skill> getSkill(std::uint64_t skill_id);
    std::shared_ptr<Skill> findSkill(const std::string& name);
    bool removeSkill(std::uint64_t skill_id);
    
    // Skill practice and learning
    void practiceSkill(std::uint64_t skill_id, float performance_score);
    void practiceSkill(const std::string& skill_name, float performance_score);
    bool executeSkill(std::uint64_t skill_id, std::vector<float>& output_commands);
    
    // Motor action management
    void addMotorAction(const std::string& action_name, const std::vector<float>& commands,
                       float execution_time = 0.0f);
    std::shared_ptr<MotorAction> getMotorAction(const std::string& action_name);
    bool executeMotorAction(const std::string& action_name, std::vector<float>& commands);
    
    // Habit formation and management
    std::uint64_t startHabitFormation(const std::string& trigger_context, const std::string& action);
    void reinforceHabit(std::uint64_t habit_id);
    void reinforceHabit(const std::string& trigger_context);
    std::shared_ptr<Habit> getTriggeredHabit(const std::string& context);
    
    // Automation and chunking
    void checkForAutomation();
    void chunkActionSequence(std::uint64_t skill_id, const std::vector<std::size_t>& chunk_indices);
    std::vector<std::uint64_t> getAutomatedSkills() const;
    
    // Skill transfer and generalization
    void transferSkill(std::uint64_t source_skill_id, std::uint64_t target_skill_id, float transfer_amount = 0.1f);
    std::vector<std::uint64_t> findSimilarSkills(std::uint64_t skill_id, float similarity_threshold = 0.7f);
    
    // Memory maintenance
    void decayUnusedSkills(float decay_rate = 0.01f);
    void strengthenFrequentlyUsed();
    void consolidateMotorMemories();
    
    // Retrieval and search
    std::vector<std::shared_ptr<Skill>> getAllSkills() const;
    std::vector<std::shared_ptr<Skill>> getSkillsByProficiency(float min_proficiency = 0.0f) const;
    std::vector<std::shared_ptr<Habit>> getActiveHabits() const;
    
    // Statistics and monitoring
    const ProceduralStats& getStatistics() const { return statistics_; }
    void updateStatistics();
    float getOverallProficiency() const;
    
    // Configuration
    void updateConfig(const ProceduralConfig& config) { config_ = config; }
    const ProceduralConfig& getConfig() const { return config_; }

private:
    float calculateSimilarity(const Skill& a, const Skill& b) const;
    void updateProficiency(Skill& skill, float performance_score);
    bool shouldAutomateSkill(const Skill& skill) const;
    void processHabitFormation();

private:
    ProceduralConfig config_;
    std::unordered_map<std::uint64_t, std::shared_ptr<Skill>> skills_;
    std::unordered_map<std::string, std::uint64_t> skill_name_lookup_;
    std::unordered_map<std::string, std::shared_ptr<MotorAction>> motor_actions_;
    std::unordered_map<std::uint64_t, std::shared_ptr<Habit>> habits_;
    std::unordered_map<std::string, std::vector<std::uint64_t>> context_habits_;
    ProceduralStats statistics_;
    std::uint64_t next_skill_id_ = 1;
    std::uint64_t next_habit_id_ = 1;
};

} // namespace Memory
} // namespace NeuroForge