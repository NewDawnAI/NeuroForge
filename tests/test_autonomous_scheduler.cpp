#ifdef MINIMAL_TEST_FRAMEWORK
// Minimal test framework for systems without Google Test
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

#define TEST(test_case, test_name) \
    void test_case##_##test_name(); \
    static bool test_case##_##test_name##_registered = []() { \
        register_test(#test_case "." #test_name, test_case##_##test_name); \
        return true; \
    }(); \
    void test_case##_##test_name()

#define TEST_F(test_fixture, test_name) \
    void test_fixture##_##test_name(); \
    static bool test_fixture##_##test_name##_registered = []() { \
        register_test(#test_fixture "." #test_name, test_fixture##_##test_name); \
        return true; \
    }(); \
    void test_fixture##_##test_name()

#define EXPECT_TRUE(condition) \
    if (!(condition)) { \
        std::cerr << "EXPECT_TRUE failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        test_failed = true; \
    }

#define EXPECT_FALSE(condition) \
    if (condition) { \
        std::cerr << "EXPECT_FALSE failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        test_failed = true; \
    }

#define EXPECT_EQ(expected, actual) \
    if ((expected) != (actual)) { \
        std::cerr << "EXPECT_EQ failed: expected " << (expected) << ", got " << (actual) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        test_failed = true; \
    }

#define EXPECT_GT(val1, val2) \
    if (!((val1) > (val2))) { \
        std::cerr << "EXPECT_GT failed: " << (val1) << " > " << (val2) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        test_failed = true; \
    }

#define EXPECT_GE(val1, val2) \
    if (!((val1) >= (val2))) { \
        std::cerr << "EXPECT_GE failed: " << (val1) << " >= " << (val2) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        test_failed = true; \
    }

#define ASSERT_TRUE(condition) \
    if (!(condition)) { \
        std::cerr << "ASSERT_TRUE failed: " << #condition << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        test_failed = true; \
        return; \
    }

#define ASSERT_NE(val1, val2) \
    if ((val1) == (val2)) { \
        std::cerr << "ASSERT_NE failed: " << (val1) << " == " << (val2) << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
        test_failed = true; \
        return; \
    }

#define SUCCEED() \
    do { \
        std::cout << "Test succeeded explicitly" << std::endl; \
    } while(0)

static bool test_failed = false;
static std::vector<std::pair<std::string, void(*)()>> test_registry;

void register_test(const std::string& name, void(*func)()) {
    test_registry.push_back({name, func});
}

int main() {
    int passed = 0;
    int failed = 0;
    
    for (const auto& test : test_registry) {
        test_failed = false;
        std::cout << "Running " << test.first << "..." << std::endl;
        
        try {
            test.second();
            if (!test_failed) {
                std::cout << "  PASSED" << std::endl;
                passed++;
            } else {
                std::cout << "  FAILED" << std::endl;
                failed++;
            }
        } catch (const std::exception& e) {
            std::cout << "  FAILED with exception: " << e.what() << std::endl;
            failed++;
        }
    }
    
    std::cout << "\nTest Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed > 0 ? 1 : 0;
}

#else
#include <gtest/gtest.h>
#endif

#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

#include "core/HypergraphBrain.h"
#include "core/AutonomousScheduler.h"
#include "connectivity/ConnectivityManager.h"
#include "regions/CorticalRegions.h"
#include "regions/LimbicRegions.h"

using namespace NeuroForge::Core;
using namespace NeuroForge::Regions;

#ifdef MINIMAL_TEST_FRAMEWORK
// Global test variables for minimal framework
static std::unique_ptr<HypergraphBrain> brain_;

void setUp() {
    auto connectivity_manager = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
    brain_ = std::make_unique<HypergraphBrain>(connectivity_manager, 100.0f);
    
    // Add required regions for autonomous operation
    auto self_node = std::make_shared<SelfNode>("SelfNode");
    auto prefrontal_cortex = std::make_shared<PrefrontalCortex>("PrefrontalCortex");
    auto motor_cortex = std::make_shared<MotorCortex>("MotorCortex");
    
    brain_->addRegion(self_node);
    brain_->addRegion(prefrontal_cortex);
    brain_->addRegion(motor_cortex);
    
    // Initialize the autonomous scheduler
    ASSERT_TRUE(brain_->initializeAutonomousScheduler());
}

void tearDown() {
    if (brain_) {
        brain_->setAutonomousModeEnabled(false);
        brain_.reset();
    }
}

#else
class AutonomousSchedulerTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto connectivity_manager = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
        brain_ = std::make_unique<HypergraphBrain>(connectivity_manager, 100.0f);
        
        // Add required regions for autonomous operation
        auto self_node = std::make_shared<SelfNode>("SelfNode");
        auto prefrontal_cortex = std::make_shared<PrefrontalCortex>("PrefrontalCortex");
        auto motor_cortex = std::make_shared<MotorCortex>("MotorCortex");
        
        brain_->addRegion(self_node);
        brain_->addRegion(prefrontal_cortex);
        brain_->addRegion(motor_cortex);
        
        // Initialize the autonomous scheduler
        ASSERT_TRUE(brain_->initializeAutonomousScheduler());
    }
    
    void TearDown() override {
        brain_->setAutonomousModeEnabled(false);
        brain_.reset();
    }
    
    std::unique_ptr<HypergraphBrain> brain_;
};
#endif

// Test basic scheduler initialization
TEST_F(AutonomousSchedulerTest, InitializationTest) {
#ifdef MINIMAL_TEST_FRAMEWORK
    setUp();
#endif
    EXPECT_TRUE(brain_->getAutonomousScheduler() != nullptr);
    
    auto stats = brain_->getAutonomousStatistics();
    EXPECT_TRUE(stats.has_value());
    EXPECT_EQ(stats->total_tasks_scheduled, 0);
    EXPECT_EQ(stats->current_running_tasks, 0);
    EXPECT_EQ(stats->total_tasks_completed, 0);
    EXPECT_EQ(stats->total_tasks_failed, 0);
#ifdef MINIMAL_TEST_FRAMEWORK
    tearDown();
#endif
}

// Test autonomous mode enable/disable
TEST_F(AutonomousSchedulerTest, AutonomousModeToggle) {
#ifdef MINIMAL_TEST_FRAMEWORK
    setUp();
#endif
    EXPECT_FALSE(brain_->isAutonomousModeEnabled());
    
    brain_->setAutonomousModeEnabled(true);
    EXPECT_TRUE(brain_->isAutonomousModeEnabled());
    
    brain_->setAutonomousModeEnabled(false);
    EXPECT_FALSE(brain_->isAutonomousModeEnabled());
#ifdef MINIMAL_TEST_FRAMEWORK
    tearDown();
#endif
}

// Test adding and executing goal tasks
TEST_F(AutonomousSchedulerTest, GoalTaskExecution) {
#ifdef MINIMAL_TEST_FRAMEWORK
    setUp();
#endif
    auto goal_task = std::make_shared<GoalTask>(1, "test_goal", "exploration");
    goal_task->setPriority(TaskPriority::High);
    goal_task->setGoalParameters({0.5f, 0.8f, 0.3f});
    goal_task->setSuccessThreshold(0.7f);
    
    brain_->addAutonomousTask(goal_task);
    
    auto stats_before = brain_->getAutonomousStatistics();
    EXPECT_TRUE(stats_before.has_value());
    EXPECT_EQ(stats_before->current_queue_size, 1);
    
    // Execute one cycle
    brain_->executeAutonomousCycle(0.1f);
    
    auto stats_after = brain_->getAutonomousStatistics();
    EXPECT_TRUE(stats_after.has_value());
    EXPECT_GE(stats_after->total_tasks_scheduled, stats_before->total_tasks_scheduled);
#ifdef MINIMAL_TEST_FRAMEWORK
    tearDown();
#endif
}

// Test adding and executing reflection tasks
TEST_F(AutonomousSchedulerTest, ReflectionTaskExecution) {
#ifdef MINIMAL_TEST_FRAMEWORK
    setUp();
#endif
    auto reflection_task = std::make_shared<ReflectionTask>(2, "test_reflection", "comprehensive");
    reflection_task->setPriority(TaskPriority::Medium);
    
    brain_->addAutonomousTask(reflection_task);
    
    auto stats_before = brain_->getAutonomousStatistics();
    EXPECT_TRUE(stats_before.has_value());
    EXPECT_EQ(stats_before->current_queue_size, 1);
    
    // Execute one cycle
    brain_->executeAutonomousCycle(0.1f);
    
    auto stats_after = brain_->getAutonomousStatistics();
    EXPECT_TRUE(stats_after.has_value());
    EXPECT_GE(stats_after->total_tasks_scheduled, stats_before->total_tasks_scheduled);
#ifdef MINIMAL_TEST_FRAMEWORK
    tearDown();
#endif
}

// Test multiple task execution with different priorities
TEST_F(AutonomousSchedulerTest, MultipleTaskPriorities) {
#ifdef MINIMAL_TEST_FRAMEWORK
    setUp();
#endif
    // Add high priority goal task
    auto high_priority_task = std::make_shared<GoalTask>(3, "high_priority_goal", "achievement");
    high_priority_task->setPriority(TaskPriority::High);
    brain_->addAutonomousTask(high_priority_task);
    
    // Add low priority reflection task
    auto low_priority_task = std::make_shared<ReflectionTask>(4, "low_priority_reflection", "simple");
    low_priority_task->setPriority(TaskPriority::Low);
    brain_->addAutonomousTask(low_priority_task);
    
    // Add medium priority goal task
    auto medium_priority_task = std::make_shared<GoalTask>(5, "medium_priority_goal", "exploration");
    medium_priority_task->setPriority(TaskPriority::Medium);
    brain_->addAutonomousTask(medium_priority_task);
    
    auto stats_before = brain_->getAutonomousStatistics();
    EXPECT_TRUE(stats_before.has_value());
    EXPECT_EQ(stats_before->current_queue_size, 3);
    
    // Execute multiple cycles
    for (int i = 0; i < 5; ++i) {
        brain_->executeAutonomousCycle(0.1f);
    }
    
    auto stats_after = brain_->getAutonomousStatistics();
    EXPECT_TRUE(stats_after.has_value());
    EXPECT_GT(stats_after->total_tasks_scheduled, 0);
#ifdef MINIMAL_TEST_FRAMEWORK
    tearDown();
#endif
}

// Test autonomous loop execution
TEST_F(AutonomousSchedulerTest, AutonomousLoopExecution) {
#ifdef MINIMAL_TEST_FRAMEWORK
    setUp();
#endif
    brain_->setAutonomousModeEnabled(true);
    
    // Add some tasks to execute
    auto goal_task = std::make_shared<GoalTask>(6, "loop_test_goal", "exploration");
    goal_task->setPriority(TaskPriority::Medium);
    brain_->addAutonomousTask(goal_task);
    
    auto reflection_task = std::make_shared<ReflectionTask>(7, "loop_test_reflection", "simple");
    reflection_task->setPriority(TaskPriority::Low);
    brain_->addAutonomousTask(reflection_task);
    
    // Run autonomous loop in a separate thread for a short duration
    std::atomic<bool> loop_completed{false};
#ifdef MINIMAL_TEST_FRAMEWORK
    std::thread loop_thread([&loop_completed]() {
        brain_->runAutonomousLoop(10, 10.0f); // 10 iterations at 10 Hz
        loop_completed.store(true);
    });
#else
    std::thread loop_thread([this, &loop_completed]() {
        brain_->runAutonomousLoop(10, 10.0f); // 10 iterations at 10 Hz
        loop_completed.store(true);
    });
#endif
    
    // Wait for completion with timeout
    auto start_time = std::chrono::steady_clock::now();
    while (!loop_completed.load() && 
           std::chrono::steady_clock::now() - start_time < std::chrono::seconds(5)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Stop autonomous mode to terminate loop
    brain_->setAutonomousModeEnabled(false);
    
    if (loop_thread.joinable()) {
        loop_thread.join();
    }
    
    auto stats = brain_->getAutonomousStatistics();
    EXPECT_TRUE(stats.has_value());
    EXPECT_GT(stats->total_tasks_scheduled, 0);
#ifdef MINIMAL_TEST_FRAMEWORK
    tearDown();
#endif
}

// Test scheduler statistics tracking
TEST_F(AutonomousSchedulerTest, StatisticsTracking) {
#ifdef MINIMAL_TEST_FRAMEWORK
    setUp();
#endif
    auto initial_stats = brain_->getAutonomousStatistics();
    EXPECT_TRUE(initial_stats.has_value());
    
    // Add and execute several tasks
    for (int i = 0; i < 5; ++i) {
        auto task = std::make_shared<GoalTask>(10 + i, "stats_test_goal_" + std::to_string(i), "exploration");
        task->setPriority(TaskPriority::Medium);
        brain_->addAutonomousTask(task);
    }
    
    // Execute cycles
    for (int i = 0; i < 10; ++i) {
        brain_->executeAutonomousCycle(0.1f);
    }
    
    auto final_stats = brain_->getAutonomousStatistics();
    EXPECT_TRUE(final_stats.has_value());
    
    EXPECT_GE(final_stats->total_tasks_scheduled, initial_stats->total_tasks_scheduled);
    EXPECT_GE(final_stats->total_tasks_completed, initial_stats->total_tasks_completed);
    EXPECT_GE(final_stats->average_execution_time_ms, 0.0f);
#ifdef MINIMAL_TEST_FRAMEWORK
    tearDown();
#endif
}

// Test brain region integration during autonomous operation
TEST_F(AutonomousSchedulerTest, BrainRegionIntegration) {
#ifdef MINIMAL_TEST_FRAMEWORK
    setUp();
#endif
    brain_->setAutonomousModeEnabled(true);
    
    // Verify regions are accessible
    auto self_node = brain_->getRegion("SelfNode");
    auto prefrontal_cortex = brain_->getRegion("PrefrontalCortex");
    auto motor_cortex = brain_->getRegion("MotorCortex");
    
    ASSERT_NE(self_node, nullptr);
    ASSERT_NE(prefrontal_cortex, nullptr);
    ASSERT_NE(motor_cortex, nullptr);
    
    // Execute autonomous cycle and verify regions are engaged
    brain_->executeAutonomousCycle(0.1f);
    
    // The regions should have been processed during the cycle
    // This is verified by the fact that no exceptions were thrown
    SUCCEED();
#ifdef MINIMAL_TEST_FRAMEWORK
    tearDown();
#endif
}

// Test error handling and recovery
TEST_F(AutonomousSchedulerTest, ErrorHandlingAndRecovery) {
#ifdef MINIMAL_TEST_FRAMEWORK
    setUp();
#endif
    // Test with null task (should be handled gracefully)
    brain_->addAutonomousTask(nullptr);
    
    // Should not crash
    brain_->executeAutonomousCycle(0.1f);
    
    auto stats = brain_->getAutonomousStatistics();
    // Statistics should still be valid
    EXPECT_TRUE(stats.has_value());
    EXPECT_GE(stats->total_tasks_scheduled, 0);
#ifdef MINIMAL_TEST_FRAMEWORK
    tearDown();
#endif
}

// Test concurrent access to scheduler
TEST_F(AutonomousSchedulerTest, ConcurrentAccess) {
#ifdef MINIMAL_TEST_FRAMEWORK
    setUp();
#endif
    brain_->setAutonomousModeEnabled(true);
    
    std::atomic<int> tasks_added{0};
    std::vector<std::thread> threads;
    
    // Create multiple threads adding tasks concurrently
    for (int t = 0; t < 3; ++t) {
#ifdef MINIMAL_TEST_FRAMEWORK
        threads.emplace_back([&tasks_added, t]() {
#else
        threads.emplace_back([this, &tasks_added, t]() {
#endif
            for (int i = 0; i < 5; ++i) {
                auto task = std::make_shared<GoalTask>(
                    100 + t * 10 + i, 
                    "concurrent_task_" + std::to_string(t) + "_" + std::to_string(i), 
                    "exploration"
                );
                task->setPriority(TaskPriority::Medium);
                brain_->addAutonomousTask(task);
                tasks_added.fetch_add(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Execute cycles concurrently
#ifdef MINIMAL_TEST_FRAMEWORK
    std::thread execution_thread([]() {
#else
    std::thread execution_thread([this]() {
#endif
        for (int i = 0; i < 20; ++i) {
            brain_->executeAutonomousCycle(0.05f);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    execution_thread.join();
    
    EXPECT_EQ(tasks_added.load(), 15); // 3 threads * 5 tasks each
    
    auto stats = brain_->getAutonomousStatistics();
    EXPECT_TRUE(stats.has_value());
    EXPECT_GT(stats->total_tasks_scheduled, 0);
#ifdef MINIMAL_TEST_FRAMEWORK
    tearDown();
#endif
}