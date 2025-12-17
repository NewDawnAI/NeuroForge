#include "core/SubstrateTaskGenerator.h"
#include "core/HypergraphBrain.h"
#include "core/LearningSystem.h"
#include <algorithm>
#include <random>
#include <sstream>
#include <cmath>

namespace NeuroForge {
namespace Core {

SubstrateTaskGenerator::SubstrateTaskGenerator(std::shared_ptr<HypergraphBrain> brain,
                                             std::shared_ptr<AutonomousScheduler> scheduler,
                                             const Config& config)
    : brain_(brain)
    , scheduler_(scheduler)
    , config_(config)
    , last_generation_time_(std::chrono::steady_clock::now()) {
}

bool SubstrateTaskGenerator::initialize() {
    if (!brain_ || !scheduler_) {
        return false;
    }
    
    is_active_.store(true);
    generation_cycle_.store(0);
    last_generation_time_ = std::chrono::steady_clock::now();
    
    // Initialize substrate context
    updateSubstrateContext();
    scheduler_->addPostExecutionCallback([this](const AutonomousScheduler& s, const TaskContext&) {
        auto completed = s.getTasksByStatus(TaskStatus::Completed);
        for (const auto& task : completed) {
            auto id = task->getId();
            {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                if (active_tasks_.find(id) == active_tasks_.end()) {
                    continue;
                }
            }
            float performance = 0.5f;
            bool success = task->getStatus() == TaskStatus::Completed;
            if (auto goal = std::dynamic_pointer_cast<GoalTask>(task)) {
                performance = std::clamp(goal->getProgress(), 0.0f, 1.0f);
            } else if (auto plan = std::dynamic_pointer_cast<PlanTask>(task)) {
                performance = plan->getPlannedActions().empty() ? 0.5f : 1.0f;
            } else if (auto action = std::dynamic_pointer_cast<ActionTask>(task)) {
                const auto& res = action->getExecutionResults();
                if (!res.empty()) {
                    float sum = 0.0f;
                    int count = 0;
                    for (const auto& kv : res) { sum += kv.second; count++; }
                    if (count > 0) performance = std::clamp(sum / static_cast<float>(count), 0.0f, 1.0f);
                } else {
                    performance = 0.8f;
                }
            } else if (auto refl = std::dynamic_pointer_cast<ReflectionTask>(task)) {
                const auto& ins = refl->getInsights();
                if (!ins.empty()) {
                    float sum = 0.0f;
                    int count = 0;
                    for (const auto& kv : ins) { sum += kv.second; count++; }
                    if (count > 0) performance = std::clamp(sum / static_cast<float>(count), 0.0f, 1.0f);
                } else {
                    performance = 0.7f;
                }
            }
            evaluateTaskOutcome(id, success, performance);
        }
    });
    
    return true;
}

void SubstrateTaskGenerator::shutdown() {
    is_active_.store(false);
    
    std::lock_guard<std::mutex> lock(stats_mutex_);
    active_tasks_.clear();
}

std::size_t SubstrateTaskGenerator::generateTasks(float delta_time) {
    if (!is_active_.load() || !shouldGenerateTasks()) {
        return 0;
    }
    
    // Update substrate context with current brain state
    updateSubstrateContext();
    
    std::size_t tasks_generated = 0;
    
    // Check resource constraints
    if (!checkResourceConstraints()) {
        return 0;
    }
    
    // Generate tasks based on substrate state and intrinsic motivation
    auto current_time = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
        current_time - last_generation_time_).count();
    
    if (time_since_last >= config_.task_generation_interval_ms) {
        // Determine task types to generate based on substrate context
        std::vector<SubstrateTaskType> candidate_tasks;
        
        // Curiosity-driven exploration
        if (current_context_.intrinsic_motivation > config_.curiosity_threshold) {
            candidate_tasks.push_back(SubstrateTaskType::Exploration);
        }
        
        // Uncertainty-based learning
        if (current_context_.uncertainty_level > config_.uncertainty_threshold) {
            candidate_tasks.push_back(SubstrateTaskType::PredictionImprovement);
        }
        
        // Prediction error correction
        if (current_context_.prediction_error > config_.prediction_error_threshold) {
            candidate_tasks.push_back(SubstrateTaskType::Optimization);
        }
        
        // Memory consolidation based on processing cycles
        if (current_context_.processing_cycles % 1000 == 0) {
            candidate_tasks.push_back(SubstrateTaskType::Consolidation);
        }
        
        // Self-reflection based on configuration
        if (config_.enable_self_reflection && 
            std::fmod(current_context_.processing_cycles, 500.0) < 1.0) {
            candidate_tasks.push_back(SubstrateTaskType::SelfReflection);
        }
        
        // Adaptive goal generation
        if (config_.enable_adaptive_goals) {
            auto avg_performance = 0.0f;
            if (!current_context_.performance_metrics.empty()) {
                for (const auto& metric : current_context_.performance_metrics) {
                    avg_performance += metric.second;
                }
                avg_performance /= current_context_.performance_metrics.size();
                
                if (avg_performance < config_.performance_threshold) {
                    candidate_tasks.push_back(SubstrateTaskType::AdaptiveGoal);
                }
            }
        }
        
        // Generate tasks from candidates
        std::random_device rd;
        std::mt19937 gen(rd());
        std::shuffle(candidate_tasks.begin(), candidate_tasks.end(), gen);
        
        for (auto task_type : candidate_tasks) {
            if (tasks_generated >= config_.max_concurrent_tasks) {
                break;
            }
            
            AutonomousTask::TaskID task_id = 0;
            
            switch (task_type) {
                case SubstrateTaskType::Exploration:
                    task_id = generateExplorationTask();
                    break;
                case SubstrateTaskType::Consolidation:
                    task_id = generateConsolidationTask();
                    break;
                case SubstrateTaskType::SelfReflection:
                    task_id = generateSelfReflectionTask();
                    break;
                case SubstrateTaskType::AdaptiveGoal:
                    task_id = generateAdaptiveGoal();
                    break;
                case SubstrateTaskType::PredictionImprovement:
                case SubstrateTaskType::Optimization:
                    // Generate optimization task
                    task_id = scheduler_->scheduleGoal(
                        generateTaskName(task_type),
                        "optimization",
                        TaskPriority::Medium
                    );
                    break;
                default:
                    break;
            }
            
            if (task_id != 0) {
                std::lock_guard<std::mutex> lock(stats_mutex_);
                active_tasks_[task_id] = task_type;
                stats_.total_tasks_generated++;
                stats_.task_type_counts[task_type]++;
                tasks_generated++;
            }
        }
        
        last_generation_time_ = current_time;
        generation_cycle_.fetch_add(1);
    }
    
    return tasks_generated;
}

void SubstrateTaskGenerator::updateSubstrateContext() {
    std::lock_guard<std::mutex> lock(context_mutex_);
    
    current_context_.timestamp = std::chrono::steady_clock::now();
    
    if (brain_) {
        // Get global activation from brain (use public method)
        current_context_.global_activation = 0.0f; // Will be calculated from regions
        current_context_.processing_cycles = brain_->getProcessingCycles();
        
        // Calculate global activation from regions
        const auto& regions = brain_->getRegionsMap();
        float total_activation = 0.0f;
        std::size_t region_count = 0;
        for (const auto& region_pair : regions) {
            if (region_pair.second) {
                total_activation += region_pair.second->getGlobalActivation();
                region_count++;
            }
        }
        if (region_count > 0) {
            current_context_.global_activation = total_activation / region_count;
        }
        
        // Get learning system metrics
        if (auto* learning_system = brain_->getLearningSystem()) {
            current_context_.learning_rate = learning_system->getConfig().global_learning_rate;
            current_context_.intrinsic_motivation = calculateIntrinsicMotivation();
            current_context_.uncertainty_level = calculateUncertaintyLevel();
            current_context_.prediction_error = calculatePredictionError();
            
            // Get performance metrics
            current_context_.performance_metrics.clear();
            current_context_.performance_metrics["competence"] = learning_system->getCompetenceLevel();
            current_context_.performance_metrics["learning_rate"] = current_context_.learning_rate;
            current_context_.performance_metrics["motivation"] = current_context_.intrinsic_motivation;
        }
        
        // Get region activations
        current_context_.region_activations.clear();
        for (const auto& region_pair : regions) {
            if (region_pair.second) {
                float avg_activation = region_pair.second->getGlobalActivation();
                current_context_.region_activations.push_back(avg_activation);
            }
        }
    }
}

bool SubstrateTaskGenerator::shouldGenerateTasks() const {
    if (!is_active_.load()) {
        return false;
    }
    
    // Check if enough time has passed since last generation
    auto current_time = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
        current_time - last_generation_time_).count();
    
    return time_since_last >= config_.task_generation_interval_ms;
}

AutonomousTask::TaskID SubstrateTaskGenerator::generateExplorationTask() {
    std::string task_name = generateTaskName(SubstrateTaskType::Exploration);
    return scheduler_->scheduleGoal(task_name, "exploration", TaskPriority::High);
}

AutonomousTask::TaskID SubstrateTaskGenerator::generateConsolidationTask() {
    std::string task_name = generateTaskName(SubstrateTaskType::Consolidation);
    return scheduler_->scheduleGoal(task_name, "consolidation", TaskPriority::Medium);
}

AutonomousTask::TaskID SubstrateTaskGenerator::generateSelfReflectionTask() {
    std::string task_name = generateTaskName(SubstrateTaskType::SelfReflection);
    return scheduler_->scheduleReflection(task_name, "comprehensive", TaskPriority::Low);
}

AutonomousTask::TaskID SubstrateTaskGenerator::generateAdaptiveGoal() {
    std::string task_name = generateTaskName(SubstrateTaskType::AdaptiveGoal);
    return scheduler_->scheduleGoal(task_name, "adaptive", TaskPriority::High);
}

float SubstrateTaskGenerator::calculateIntrinsicMotivation() const {
    if (!brain_) return 0.0f;
    
    auto* learning_system = brain_->getLearningSystem();
    if (!learning_system) return 0.0f;
    
    // Combine uncertainty, novelty, and prediction error for intrinsic motivation
    float uncertainty = calculateUncertaintyLevel();
    float prediction_error = calculatePredictionError();
    float novelty = learning_system->getLastSubstrateNovelty();
    
    // Weighted combination
    float motivation = 0.4f * uncertainty + 0.3f * prediction_error + 0.3f * novelty;
    return std::clamp(motivation, 0.0f, 1.0f);
}

float SubstrateTaskGenerator::calculateUncertaintyLevel() const {
    if (!brain_) return 0.0f;
    
    // Calculate uncertainty based on activation variance across regions
    if (current_context_.region_activations.empty()) {
        return 0.0f;
    }
    
    float mean = 0.0f;
    for (float activation : current_context_.region_activations) {
        mean += activation;
    }
    mean /= current_context_.region_activations.size();
    
    float variance = 0.0f;
    for (float activation : current_context_.region_activations) {
        float diff = activation - mean;
        variance += diff * diff;
    }
    variance /= current_context_.region_activations.size();
    
    // Normalize variance to [0, 1] range
    return std::clamp(std::sqrt(variance), 0.0f, 1.0f);
}

float SubstrateTaskGenerator::calculatePredictionError() const {
    if (!brain_) return 0.0f;
    
    auto* learning_system = brain_->getLearningSystem();
    if (!learning_system) return 0.0f;
    
    // Use learning system's prediction error metrics
    float competence = learning_system->getCompetenceLevel();
    float prediction_error = 1.0f - competence; // Inverse of competence
    
    return std::clamp(prediction_error, 0.0f, 1.0f);
}

void SubstrateTaskGenerator::evaluateTaskOutcome(AutonomousTask::TaskID task_id, 
                                               bool success, 
                                               float performance) {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto it = active_tasks_.find(task_id);
    if (it != active_tasks_.end()) {
        if (success) {
            stats_.successful_tasks++;
        } else {
            stats_.failed_tasks++;
        }
        
        // Update average performance
        float total_tasks = stats_.successful_tasks + stats_.failed_tasks;
        stats_.average_performance = 
            (stats_.average_performance * (total_tasks - 1) + performance) / total_tasks;
        
        active_tasks_.erase(it);
    }
    
    // Update adaptive parameters based on performance
    updateAdaptiveParameters();
}

SubstrateTaskGenerator::SubstrateTaskType SubstrateTaskGenerator::selectTaskType() const {
    // Select task type based on current substrate context and priorities
    std::vector<std::pair<SubstrateTaskType, float>> priorities;
    
    priorities.emplace_back(SubstrateTaskType::Exploration, current_context_.intrinsic_motivation);
    priorities.emplace_back(SubstrateTaskType::PredictionImprovement, current_context_.prediction_error);
    priorities.emplace_back(SubstrateTaskType::Optimization, current_context_.uncertainty_level);
    priorities.emplace_back(SubstrateTaskType::SelfReflection, 0.3f); // Base priority
    
    // Sort by priority
    std::sort(priorities.begin(), priorities.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return priorities.empty() ? SubstrateTaskType::SelfReflection : priorities[0].first;
}

std::string SubstrateTaskGenerator::generateTaskName(SubstrateTaskType type) const {
    std::ostringstream name;
    name << "substrate_";
    
    switch (type) {
        case SubstrateTaskType::Exploration:
            name << "exploration_" << generation_cycle_.load();
            break;
        case SubstrateTaskType::Consolidation:
            name << "consolidation_" << generation_cycle_.load();
            break;
        case SubstrateTaskType::Optimization:
            name << "optimization_" << generation_cycle_.load();
            break;
        case SubstrateTaskType::SelfReflection:
            name << "reflection_" << generation_cycle_.load();
            break;
        case SubstrateTaskType::PredictionImprovement:
            name << "prediction_" << generation_cycle_.load();
            break;
        case SubstrateTaskType::AdaptiveGoal:
            name << "adaptive_goal_" << generation_cycle_.load();
            break;
        default:
            name << "unknown_" << generation_cycle_.load();
            break;
    }
    
    return name.str();
}

void SubstrateTaskGenerator::updateAdaptiveParameters() {
    // Adapt generation parameters based on task success rates
    float success_rate = 0.0f;
    if (stats_.successful_tasks + stats_.failed_tasks > 0) {
        success_rate = static_cast<float>(stats_.successful_tasks) / 
                      (stats_.successful_tasks + stats_.failed_tasks);
    }
    
    // Adjust thresholds based on success rate
    if (success_rate < 0.5f) {
        // Lower thresholds to generate more tasks
        config_.curiosity_threshold *= 0.95f;
        config_.uncertainty_threshold *= 0.95f;
        config_.prediction_error_threshold *= 0.95f;
    } else if (success_rate > 0.8f) {
        // Raise thresholds to be more selective
        config_.curiosity_threshold *= 1.05f;
        config_.uncertainty_threshold *= 1.05f;
        config_.prediction_error_threshold *= 1.05f;
    }
    
    // Clamp thresholds to reasonable ranges
    config_.curiosity_threshold = std::clamp(config_.curiosity_threshold, 0.1f, 0.8f);
    config_.uncertainty_threshold = std::clamp(config_.uncertainty_threshold, 0.1f, 0.8f);
    config_.prediction_error_threshold = std::clamp(config_.prediction_error_threshold, 0.1f, 0.8f);
}

bool SubstrateTaskGenerator::checkResourceConstraints() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    // Check if we're at the maximum concurrent task limit
    if (active_tasks_.size() >= config_.max_concurrent_tasks) {
        return false;
    }
    
    // Check brain processing load
    if (brain_ && current_context_.global_activation > 0.9f) {
        return false; // Brain is too active, avoid adding more tasks
    }
    
    return true;
}

void SubstrateTaskGenerator::setActive(bool active) {
    is_active_.store(active, std::memory_order_relaxed);
}

bool SubstrateTaskGenerator::isActive() const {
    return is_active_.load(std::memory_order_relaxed);
}

} // namespace Core
} // namespace NeuroForge
