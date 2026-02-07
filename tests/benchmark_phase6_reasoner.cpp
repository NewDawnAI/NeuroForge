#include "core/Phase6Reasoner.h"
#include "core/Phase8GoalSystem.h"
#include "core/MemoryDB.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <memory>
#include <filesystem>

using namespace NeuroForge::Core;

int main() {
    const std::string db_path = "benchmark_phase6.db";
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    auto memdb = std::make_shared<MemoryDB>(db_path);
    if (!memdb->open()) {
        std::cerr << "Failed to open MemoryDB" << std::endl;
        return 1;
    }
    memdb->ensureSchema();

    std::int64_t run_id = 0;
    if (!memdb->beginRun("{}", run_id)) {
        std::cerr << "Failed to begin run" << std::endl;
        return 1;
    }

    auto goal_system = std::make_shared<Phase8GoalSystem>(memdb, run_id);
    auto reasoner = std::make_unique<Phase6Reasoner>(memdb.get(), run_id);
    reasoner->setPhase8Components(goal_system.get());

    const int SUBGOAL_COUNT = 100;
    const std::string parent_desc = "parent_goal";
    std::int64_t parent_id = 0;
    if (!memdb->insertGoalNode(parent_desc, 0.8, 0.8, run_id, std::nullopt, parent_id)) {
        std::cerr << "Failed to insert parent goal" << std::endl;
        return 1;
    }

    std::cout << "Creating " << SUBGOAL_COUNT << " subgoals..." << std::endl;
    for (int i = 0; i < SUBGOAL_COUNT; ++i) {
        std::int64_t sub_id = 0;
        std::string sub_desc = "subgoal_" + std::to_string(i);
        if (!memdb->insertGoalNode(sub_desc, 0.5, 0.5, run_id, std::nullopt, sub_id)) {
            std::cerr << "Failed to insert subgoal " << i << std::endl;
            return 1;
        }
        if (!memdb->insertGoalEdge(parent_id, sub_id, 1.0)) {
            std::cerr << "Failed to link subgoal " << i << std::endl;
            return 1;
        }

        // Populate reasoner with some posterior means for these subgoals
        reasoner->applyOptionResult(0, sub_desc, 0.5, 0);
    }

    std::vector<ReasonOption> options;
    options.push_back({parent_desc, "test", "{}", 1.0, 0.0});

    // Warmup
    std::cout << "Warming up..." << std::endl;
    for (int i = 0; i < 10; ++i) {
        reasoner->scoreOptions(options);
    }

    const int ITERATIONS = 100;
    std::cout << "Benchmarking scoreOptions() with " << SUBGOAL_COUNT << " subgoals over " << ITERATIONS << " iterations..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ITERATIONS; ++i) {
        reasoner->scoreOptions(options);
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;
    std::cout << "Total time: " << elapsed.count() << " ms" << std::endl;
    std::cout << "Average time per call: " << elapsed.count() / ITERATIONS << " ms" << std::endl;

    memdb->close();
    if (std::filesystem::exists(db_path)) {
        std::filesystem::remove(db_path);
    }

    return 0;
}
