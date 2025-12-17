#pragma once

#include "Types.h"
#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>
#include <string>
#include <chrono>
#include <functional>
#include <atomic>
#include <mutex>

namespace NeuroForge {
namespace Core {

// Forward declarations
class HypergraphBrain;

/**
 * @brief Task priority levels for autonomous execution
 */
enum class TaskPriority : std::uint8_t {
    Critical = 0,   // Immediate execution required (safety, survival)
    High = 1,       // Important tasks (goal achievement, learning)
    Medium = 2,     // Regular tasks (exploration, maintenance)
    Low = 3,        // Background tasks (introspection, cleanup)
    Deferred = 4    // Can be postponed indefinitely
};

/**
 * @brief Task execution status
 */
enum class TaskStatus : std::uint8_t {
    Pending,        // Task is waiting to be executed
    Running,        // Task is currently being executed
    Completed,      // Task completed successfully
    Failed,         // Task execution failed
    Cancelled,      // Task was cancelled
    Suspended       // Task execution suspended
};

/**
 * @brief Task execution context
 */
struct TaskContext {
    std::uint64_t timestamp_ms{0};
    std::uint64_t execution_cycle{0};
    float delta_time{0.0f};
    std::unordered_map<std::string, float> parameters;
    std::string context_tag;
    
    TaskContext() = default;
    TaskContext(std::uint64_t ts, std::uint64_t cycle, float dt, const std::string& tag = "")
        : timestamp_ms(ts), execution_cycle(cycle), delta_time(dt), context_tag(tag) {}
};

/**
 * @brief Base class for all autonomous tasks
 */
class AutonomousTask {
public:
    using TaskID = std::uint64_t;
    using ExecutionCallback = std::function<bool(const TaskContext&)>;
    using CompletionCallback = std::function<void(TaskID, TaskStatus, const std::string&)>;

protected:
    TaskID task_id_;
    std::string name_;
    std::string description_;
    TaskPriority priority_;
    TaskStatus status_;
    std::chrono::steady_clock::time_point created_time_;
    std::chrono::steady_clock::time_point scheduled_time_;
    std::chrono::steady_clock::time_point deadline_;
    std::uint32_t max_retries_;
    std::uint32_t retry_count_;
    float estimated_duration_ms_;
    std::vector<TaskID> dependencies_;
    std::vector<TaskID> dependents_;
    ExecutionCallback execution_callback_;
    CompletionCallback completion_callback_;
    std::string failure_reason_;

public:
    explicit AutonomousTask(TaskID id, const std::string& name, TaskPriority priority = TaskPriority::Medium);
    virtual ~AutonomousTask() = default;

    // Core task interface
    virtual bool execute(const TaskContext& context) = 0;
    virtual bool canExecute(const TaskContext& context) const;
    virtual void onComplete(TaskStatus status, const std::string& result = "");
    virtual void onFailure(const std::string& reason);
    virtual std::string getStatusString() const;

    // Getters
    TaskID getId() const noexcept { return task_id_; }
    const std::string& getName() const noexcept { return name_; }
    const std::string& getDescription() const noexcept { return description_; }
    TaskPriority getPriority() const noexcept { return priority_; }
    TaskStatus getStatus() const noexcept { return status_; }
    float getEstimatedDuration() const noexcept { return estimated_duration_ms_; }
    const std::vector<TaskID>& getDependencies() const noexcept { return dependencies_; }
    const std::vector<TaskID>& getDependents() const noexcept { return dependents_; }
    std::uint32_t getRetryCount() const noexcept { return retry_count_; }
    std::uint32_t getMaxRetries() const noexcept { return max_retries_; }
    const std::string& getFailureReason() const noexcept { return failure_reason_; }

    // Setters
    void setDescription(const std::string& desc) { description_ = desc; }
    void setPriority(TaskPriority priority) { priority_ = priority; }
    void setStatus(TaskStatus status) { status_ = status; }
    void setEstimatedDuration(float duration_ms) { estimated_duration_ms_ = duration_ms; }
    void setMaxRetries(std::uint32_t retries) { max_retries_ = retries; }
    void setScheduledTime(const std::chrono::steady_clock::time_point& time) { scheduled_time_ = time; }
    void setDeadline(const std::chrono::steady_clock::time_point& deadline) { deadline_ = deadline; }
    void setExecutionCallback(ExecutionCallback callback) { execution_callback_ = std::move(callback); }
    void setCompletionCallback(CompletionCallback callback) { completion_callback_ = std::move(callback); }

    // Dependencies
    void addDependency(TaskID dependency_id);
    void addDependent(TaskID dependent_id);
    void removeDependency(TaskID dependency_id);
    void removeDependent(TaskID dependent_id);
    bool hasDependencies() const noexcept { return !dependencies_.empty(); }
    bool hasDependents() const noexcept { return !dependents_.empty(); }

    // Timing
    bool isOverdue(const std::chrono::steady_clock::time_point& current_time) const;
    bool isScheduled(const std::chrono::steady_clock::time_point& current_time) const;
    std::chrono::milliseconds getAge(const std::chrono::steady_clock::time_point& current_time) const;
};

/**
 * @brief Goal-oriented task for high-level objectives
 */
class GoalTask : public AutonomousTask {
private:
    std::string goal_type_;
    std::vector<float> goal_parameters_;
    float success_threshold_;
    float current_progress_;
    std::vector<TaskID> sub_tasks_;

public:
    explicit GoalTask(TaskID id, const std::string& name, const std::string& goal_type);
    
    bool execute(const TaskContext& context) override;
    void setGoalParameters(const std::vector<float>& parameters) { goal_parameters_ = parameters; }
    void setSuccessThreshold(float threshold) { success_threshold_ = threshold; }
    void updateProgress(float progress) { current_progress_ = progress; }
    void addSubTask(TaskID sub_task_id) { sub_tasks_.push_back(sub_task_id); }
    
    const std::string& getGoalType() const noexcept { return goal_type_; }
    float getProgress() const noexcept { return current_progress_; }
    const std::vector<TaskID>& getSubTasks() const noexcept { return sub_tasks_; }
};

/**
 * @brief Planning task for generating action sequences
 */
class PlanTask : public AutonomousTask {
private:
    TaskID goal_id_;
    std::vector<TaskID> planned_actions_;
    std::string planning_strategy_;
    std::unordered_map<std::string, float> planning_parameters_;

public:
    explicit PlanTask(TaskID id, const std::string& name, TaskID goal_id);
    
    bool execute(const TaskContext& context) override;
    void setPlanningStrategy(const std::string& strategy) { planning_strategy_ = strategy; }
    void setPlanningParameters(const std::unordered_map<std::string, float>& params) { planning_parameters_ = params; }
    void addPlannedAction(TaskID action_id) { planned_actions_.push_back(action_id); }
    
    TaskID getGoalId() const noexcept { return goal_id_; }
    const std::vector<TaskID>& getPlannedActions() const noexcept { return planned_actions_; }
    const std::string& getPlanningStrategy() const noexcept { return planning_strategy_; }
};

/**
 * @brief Action execution task for concrete operations
 */
class ActionTask : public AutonomousTask {
private:
    std::string action_type_;
    std::vector<float> action_parameters_;
    std::string target_region_;
    std::unordered_map<std::string, float> execution_results_;

public:
    explicit ActionTask(TaskID id, const std::string& name, const std::string& action_type);
    
    bool execute(const TaskContext& context) override;
    void setActionParameters(const std::vector<float>& parameters) { action_parameters_ = parameters; }
    void setTargetRegion(const std::string& region) { target_region_ = region; }
    
    const std::string& getActionType() const noexcept { return action_type_; }
    const std::vector<float>& getActionParameters() const noexcept { return action_parameters_; }
    const std::string& getTargetRegion() const noexcept { return target_region_; }
    const std::unordered_map<std::string, float>& getExecutionResults() const noexcept { return execution_results_; }
};

/**
 * @brief Reflection task for introspection and learning
 */
class ReflectionTask : public AutonomousTask {
private:
    std::vector<TaskID> reflected_tasks_;
    std::string reflection_type_;
    std::unordered_map<std::string, float> insights_;
    std::string narrative_;

public:
    explicit ReflectionTask(TaskID id, const std::string& name, const std::string& reflection_type);
    
    bool execute(const TaskContext& context) override;
    void addReflectedTask(TaskID task_id) { reflected_tasks_.push_back(task_id); }
    void addInsight(const std::string& key, float value) { insights_[key] = value; }
    void setNarrative(const std::string& narrative) { narrative_ = narrative; }
    
    const std::vector<TaskID>& getReflectedTasks() const noexcept { return reflected_tasks_; }
    const std::string& getReflectionType() const noexcept { return reflection_type_; }
    const std::unordered_map<std::string, float>& getInsights() const noexcept { return insights_; }
    const std::string& getNarrative() const noexcept { return narrative_; }
};

/**
 * @brief Task scheduler for autonomous execution
 */
class AutonomousScheduler {
public:
    using TaskPtr = std::shared_ptr<AutonomousTask>;
    using TaskQueue = std::priority_queue<TaskPtr, std::vector<TaskPtr>, std::function<bool(const TaskPtr&, const TaskPtr&)>>;
    using TaskMap = std::unordered_map<AutonomousTask::TaskID, TaskPtr>;
    using SchedulerCallback = std::function<void(const AutonomousScheduler&, const TaskContext&)>;

    /**
     * @brief Scheduler configuration
     */
    struct Config {
        std::uint32_t max_concurrent_tasks{4};
        std::uint32_t max_queue_size{1000};
        float task_timeout_ms{5000.0f};
        float scheduling_frequency_hz{10.0f};
        bool enable_dependency_resolution{true};
        bool enable_priority_aging{true};
        bool enable_deadline_enforcement{true};
        float priority_aging_factor{0.1f};
        std::uint32_t max_retries_default{3};
    };

    /**
     * @brief Scheduler statistics
     */
    struct Statistics {
        std::uint64_t total_tasks_scheduled{0};
        std::uint64_t total_tasks_completed{0};
        std::uint64_t total_tasks_failed{0};
        std::uint64_t total_tasks_cancelled{0};
        std::uint32_t current_queue_size{0};
        std::uint32_t current_running_tasks{0};
        float average_execution_time_ms{0.0f};
        float scheduler_utilization{0.0f};
        std::chrono::steady_clock::time_point last_update_time;
    };

private:
    HypergraphBrain* brain_;
    Config config_;
    Statistics stats_;
    
    TaskQueue task_queue_;
    TaskMap all_tasks_;
    TaskMap running_tasks_;
    std::vector<AutonomousTask::TaskID> completed_tasks_;
    
    std::atomic<bool> is_running_{false};
    std::atomic<bool> is_paused_{false};
    std::atomic<bool> metrics_enabled_{false};
    std::atomic<AutonomousTask::TaskID> next_task_id_{1};
    
    mutable std::mutex scheduler_mutex_;
    mutable std::mutex queue_mutex_;
    mutable std::mutex stats_mutex_;
    
    std::vector<SchedulerCallback> pre_execution_callbacks_;
    std::vector<SchedulerCallback> post_execution_callbacks_;
    
    std::chrono::steady_clock::time_point last_scheduling_time_;

    // Internal methods
    bool canScheduleTask(const TaskPtr& task, const TaskContext& context) const;
    bool resolveDependencies(const TaskPtr& task) const;
    void updateTaskPriorities(const TaskContext& context);
    void enforceDeadlines(const TaskContext& context);
    void cleanupCompletedTasks();
    TaskPtr selectNextTask(const TaskContext& context);
    void executeTask(TaskPtr task, const TaskContext& context);
    void updateStatistics();

public:
    explicit AutonomousScheduler(HypergraphBrain* brain);
    ~AutonomousScheduler();

    // Lifecycle management
    bool initialize(const Config& config);
    void start();
    void pause();
    void resume();
    void stop();
    void reset();

    // Task management
    AutonomousTask::TaskID scheduleTask(TaskPtr task);
    bool cancelTask(AutonomousTask::TaskID task_id);
    bool suspendTask(AutonomousTask::TaskID task_id);
    bool resumeTask(AutonomousTask::TaskID task_id);
    TaskPtr getTask(AutonomousTask::TaskID task_id) const;
    std::vector<TaskPtr> getTasksByStatus(TaskStatus status) const;
    std::vector<TaskPtr> getTasksByPriority(TaskPriority priority) const;

    // Execution control
    void processScheduling(const TaskContext& context);
    void processExecution(const TaskContext& context);
    bool hasWork() const;
    std::uint32_t getQueueSize() const;
    std::uint32_t getRunningTaskCount() const;

    // Configuration and monitoring
    void setConfig(const Config& config) { config_ = config; }
    const Config& getConfig() const noexcept { return config_; }
    const Statistics& getStatistics() const;
    void addPreExecutionCallback(SchedulerCallback callback);
    void addPostExecutionCallback(SchedulerCallback callback);

    // M7: Metrics and monitoring
    void setMetricsEnabled(bool enabled);
    bool isMetricsEnabled() const;

    // Convenience methods for creating tasks
    AutonomousTask::TaskID scheduleGoal(const std::string& name, const std::string& goal_type, TaskPriority priority = TaskPriority::Medium);
    AutonomousTask::TaskID schedulePlan(const std::string& name, AutonomousTask::TaskID goal_id, TaskPriority priority = TaskPriority::Medium);
    AutonomousTask::TaskID scheduleAction(const std::string& name, const std::string& action_type, TaskPriority priority = TaskPriority::Medium);
    AutonomousTask::TaskID scheduleReflection(const std::string& name, const std::string& reflection_type, TaskPriority priority = TaskPriority::Low);
};

} // namespace Core
} // namespace NeuroForge