#include "core/Phase8GoalSystem.h"
#include "core/MemoryDB.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <filesystem>

using namespace NeuroForge::Core;
namespace fs = std::filesystem;

int main() {
    const std::string db_path = "benchmark_goals.sqlite";
    if (fs::exists(db_path)) {
        fs::remove(db_path);
    }

    auto db = std::make_shared<MemoryDB>(db_path);
    if (!db->open()) {
        std::cerr << "Failed to open DB" << std::endl;
        return 1;
    }
    db->ensureSchema();

    std::int64_t run_id = 0;
    db->beginRun("{}", run_id);

    Phase8GoalSystem goal_system(db, run_id);

    const int GOAL_COUNT = 500;
    std::cout << "Creating " << GOAL_COUNT << " goals..." << std::endl;
    for (int i = 0; i < GOAL_COUNT; ++i) {
        goal_system.createGoal("Goal " + std::to_string(i), 0.5, 0.8);
    }

    const int ITERATIONS = 10;
    std::cout << "Benchmarking decayStability() over " << ITERATIONS << " iterations with " << GOAL_COUNT << " goals..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        goal_system.decayStability(0.01);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> elapsed = end - start;

    std::cout << "Total time: " << elapsed.count() << " ms" << std::endl;
    std::cout << "Average time per call: " << elapsed.count() / ITERATIONS << " ms" << std::endl;

    db->close();
    if (fs::exists(db_path)) {
        fs::remove(db_path);
    }

    return 0;
}
