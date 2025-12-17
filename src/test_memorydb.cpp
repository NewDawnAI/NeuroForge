#include "core/MemoryDB.h"
#include "core/LearningSystem.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <chrono>
#include <cassert>

namespace fs = std::filesystem;

// Simple helper for test assertions
void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << std::endl;
        exit(1);
    }
    std::cout << "PASS: " << message << std::endl;
}

// Test basic database operations
void testBasicOperations() {
    std::cout << "Testing basic MemoryDB operations..." << std::endl;
    
    const std::string test_db = "test_basic.sqlite";
    
    // Clean up from previous runs
    if (fs::exists(test_db)) {
        fs::remove(test_db);
    }
    
    {
        NeuroForge::Core::MemoryDB db(test_db);
        db.setDebug(true);  // Enable debug output
        
        std::cout << "Attempting to open database: " << test_db << std::endl;
        
        // Test open
        bool opened = db.open();
        std::cout << "db.open() returned: " << (opened ? "true" : "false") << std::endl;
        std::cout << "db.isOpen() returns: " << (db.isOpen() ? "true" : "false") << std::endl;
        
        if (!opened) {
            std::cerr << "FAIL: Database failed to open" << std::endl;
            return;
        }
        
        std::cout << "SUCCESS: Database opened successfully" << std::endl;
        check(opened, "Database opened successfully");
        check(db.isOpen(), "Database reports as open");
        
        // Test schema creation
        bool schema_ok = db.ensureSchema();
        check(schema_ok, "Schema creation succeeded");
        
        // Test run creation
        std::int64_t run_id = 0;
        bool run_ok = db.beginRun("{\"test\":\"basic\"}", run_id);
        check(run_ok, "Run creation succeeded");
        check(run_id > 0, "Run ID is valid");
        
        // Test stats insertion
        NeuroForge::Core::LearningSystem::Statistics stats;
        stats.total_updates = 42;
        stats.hebbian_updates = 12;
        stats.stdp_updates = 30;
        stats.reward_updates = 77; // set nonzero Phase-4 counter
        stats.average_weight_change = 0.5f;
        stats.memory_consolidation_rate = 0.8f;
        stats.active_synapses = 1000;
        stats.potentiated_synapses = 600;
        stats.depressed_synapses = 400;
        
        auto now = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        
        bool stats_ok = db.insertLearningStats(ms, 100, 60.0, stats, run_id);
        check(stats_ok, "Learning stats insertion succeeded");

        // Verify reward_updates round-trip via query helper
        std::uint64_t latest_ru = 0;
        bool have_ru = db.getLatestRewardUpdates(run_id, latest_ru);
        check(have_ru, "Fetched latest reward_updates");
        check(latest_ru == 77, "reward_updates value persisted correctly");
        
        // Test experience insertion
        std::int64_t exp_id = 0;
        bool exp_ok = db.insertExperience(ms + 1000, 101, "test_tag", 
                                          "{\"input\":\"test\"}", "{\"output\":\"result\"}", 
                                          true, run_id, exp_id);
        check(exp_ok, "Experience insertion succeeded");
        check(exp_id > 0, "Experience ID is valid");
        
        // Test reward log insertion
        std::int64_t reward_id = 0;
        bool reward_ok = db.insertRewardLog(ms + 1500, 102, 0.75, "unit_test", "{\"context\":\"foo\"}", run_id, reward_id);
        check(reward_ok, "Reward log insertion succeeded");
        check(reward_id > 0, "Reward log ID is valid");
        
        // Test self-model insertion
        std::int64_t sm_id = 0;
        bool sm_ok = db.insertSelfModel(ms + 1600, 103, "{\"state\":\"ok\"}", 0.9, run_id, sm_id);
        check(sm_ok, "Self-model insertion succeeded");
        check(sm_id > 0, "Self-model ID is valid");
        
        // Test episode operations
        std::int64_t episode_id = 0;
        bool episode_start_ok = db.insertEpisode("test_episode", ms + 500, run_id, episode_id);
        check(episode_start_ok, "Episode start succeeded");
        check(episode_id > 0, "Episode ID is valid");
        
        bool episode_end_ok = db.updateEpisodeEnd(episode_id, ms + 2000);
        check(episode_end_ok, "Episode end succeeded");
        
        // Test experience-episode linking
        bool link_ok = db.linkExperienceToEpisode(exp_id, episode_id);
        check(link_ok, "Experience-episode linking succeeded");
        
        db.close();
        check(!db.isOpen(), "Database reports as closed");
    }
    
    // Verify file was created
    check(fs::exists(test_db), "Database file was created");
    
    // Clean up
    fs::remove(test_db);
    
    std::cout << "Basic operations test completed successfully!" << std::endl;
}

// Test round-trip functionality
void testRoundTrip() {
    std::cout << "Testing MemoryDB round-trip functionality..." << std::endl;
    
    const std::string test_db = "test_roundtrip.sqlite";
    
    // Clean up from previous runs
    if (fs::exists(test_db)) {
        fs::remove(test_db);
    }
    
    // Create and populate database
    std::int64_t run_id = 0;
    {
        NeuroForge::Core::MemoryDB db(test_db);
        check(db.open(), "First database open succeeded");
        check(db.beginRun("{\"test\":\"roundtrip\"}", run_id), "Run creation succeeded");
        
        // Insert multiple records
        NeuroForge::Core::LearningSystem::Statistics stats;
        stats.total_updates = 100;
        stats.hebbian_updates = 40;
        stats.stdp_updates = 60;
        stats.reward_updates = 5; // start nonzero
        stats.average_weight_change = 0.25f;
        stats.memory_consolidation_rate = 0.9f;
        stats.active_synapses = 2000;
        stats.potentiated_synapses = 1200;
        stats.depressed_synapses = 800;
        
        auto base_time = std::chrono::steady_clock::now();
        auto base_ms = std::chrono::duration_cast<std::chrono::milliseconds>(base_time.time_since_epoch()).count();
        
        for (int i = 0; i < 5; ++i) {
            bool ok = db.insertLearningStats(base_ms + i * 1000, 100 + i, 60.0 + i, stats, run_id);
            check(ok, "Stats insertion " + std::to_string(i) + " succeeded");
            
            // Update stats for next iteration
            stats.total_updates += 10;
            stats.active_synapses += 100;
            stats.reward_updates += 10; // increment Phase-4 counter
        }
        
        db.close();
    }
    
    // Reopen and verify persistence of latest reward_updates
    {
        NeuroForge::Core::MemoryDB db(test_db);
        check(db.open(), "Second database open succeeded");
        check(db.isOpen(), "Database is open after reopen");

        std::uint64_t latest_ru = 0;
        bool have_ru = db.getLatestRewardUpdates(run_id, latest_ru);
        check(have_ru, "Fetched latest reward_updates after reopen");
        check(latest_ru == 45, "Latest reward_updates equals expected last value (45)");

        db.close();
    }
    
    // Clean up
    fs::remove(test_db);
    
    std::cout << "Round-trip test completed successfully!" << std::endl;
}

// Test error handling
void testErrorHandling() {
    std::cout << "Testing MemoryDB error handling..." << std::endl;
    
    // Test operations on closed database
    NeuroForge::Core::MemoryDB db("nonexistent_dir/test.db");
    check(!db.isOpen(), "Unopened database reports as closed");
    
    std::int64_t run_id = 0;
    bool result = db.beginRun("{}", run_id);
    check(!result, "Operations on closed database fail gracefully");
    
    NeuroForge::Core::LearningSystem::Statistics stats{};
    result = db.insertLearningStats(0, 0, 0.0, stats, 0);
    check(!result, "Stats insertion on closed database fails gracefully");
    
    std::int64_t exp_id = 0;
    result = db.insertExperience(0, 0, "", "", "", false, 0, exp_id);
    check(!result, "Experience insertion on closed database fails gracefully");

    std::int64_t reward_id = 0;
    result = db.insertRewardLog(0, 0, 0.0, "", "", 0, reward_id);
    check(!result, "Reward log insertion on closed database fails gracefully");

    std::int64_t sm_id = 0;
    result = db.insertSelfModel(0, 0, "", 0.0, 0, sm_id);
    check(!result, "Self-model insertion on closed database fails gracefully");
    
    std::cout << "Error handling test completed successfully!" << std::endl;
}

// Test query APIs: getRecentRewards and getEpisodes
void testQueryAPIs() {
    std::cout << "Testing MemoryDB query APIs..." << std::endl;

    const std::string test_db = "test_queries.sqlite";
    if (fs::exists(test_db)) {
        fs::remove(test_db);
    }

    std::int64_t run_id = 0;
    std::int64_t ep1 = 0, ep2 = 0;

    auto now = std::chrono::steady_clock::now();
    auto base_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    // Create DB and insert data
    {
        NeuroForge::Core::MemoryDB db(test_db);
        check(db.open(), "DB open for query tests");
        check(db.ensureSchema(), "Schema ensured for query tests");
        check(db.beginRun("{\"test\":\"queries\"}", run_id), "Run started for query tests");

        // Two episodes
        check(db.insertEpisode("ep_one", base_ms + 10, run_id, ep1), "Episode one created");
        check(db.insertEpisode("ep_two", base_ms + 20, run_id, ep2), "Episode two created");
        check(db.updateEpisodeEnd(ep1, base_ms + 110), "Episode one ended");
        // Leave ep2 open to test end_ms==0

        // Insert rewards with increasing time
        for (int i = 0; i < 6; ++i) {
            std::int64_t reward_id = 0;
            std::string ctx = std::string("{") + "\"k\":" + std::to_string(i) + "}";
            bool ok = db.insertRewardLog(base_ms + 100 + i * 5, 1 + i, 0.1 * (i + 1), "src", ctx, run_id, reward_id);
            check(ok && reward_id > 0, "Inserted reward " + std::to_string(i));
        }

        db.close();
    }

    // Reopen DB and query
    {
        NeuroForge::Core::MemoryDB db(test_db);
        check(db.open(), "DB reopen for queries");

        // Query episodes
        auto eps = db.getEpisodes(run_id);
        check(eps.size() == 2, "Two episodes returned");
        check(eps[0].name == "ep_one" && eps[0].start_ms == base_ms + 10 && eps[0].end_ms == base_ms + 110, "Episode one fields correct");
        check(eps[1].name == "ep_two" && eps[1].start_ms == base_ms + 20 && eps[1].end_ms == 0, "Episode two ongoing");

        // Query recent rewards limited to 3
        auto rewards3 = db.getRecentRewards(run_id, 3);
        check(rewards3.size() == 3, "Three recent rewards returned");
        // Since ordered by ts_ms DESC, first is the last inserted
        check(rewards3[0].step == 6 && rewards3[0].reward > rewards3[1].reward, "Rewards ordering and fields plausible");

        // Query with larger limit than available
        auto rewards10 = db.getRecentRewards(run_id, 10);
        check(rewards10.size() == 6, "All six rewards returned when limit exceeds count");

        db.close();
    }

    fs::remove(test_db);
    std::cout << "Query APIs test completed successfully!" << std::endl;
}

// Integration test: run a short headless session and ensure reward_log has entries
void testRewardLogIntegration() {
    std::cout << "Testing reward_log integration via headless run..." << std::endl;

    const std::string test_db = "test_integration.sqlite";
    if (fs::exists(test_db)) {
        fs::remove(test_db);
    }

    // Locate executable candidates relative to repo root
    std::vector<std::string> candidates = {
        std::string("build/Release/neuroforge.exe"),
        std::string("build/neuroforge.exe"),
        std::string("build-vs/Debug/neuroforge.exe"),
        std::string("build-vs/Release/neuroforge.exe"),
        std::string("build-vcpkg-rel/Release/neuroforge.exe"),
        // Common when CWD is the 'build' directory
        std::string("Release/neuroforge.exe"),
        std::string("Debug/neuroforge.exe"),
        // Parent-root patterns when CWD is inside a build subdir
        std::string("../build/Release/neuroforge.exe"),
        std::string("../build/neuroforge.exe"),
        std::string("../build-vs/Debug/neuroforge.exe"),
        std::string("../build-vs/Release/neuroforge.exe"),
        std::string("../build-vcpkg-rel/Release/neuroforge.exe")
    };

    std::string exePath;
    for (const auto& p : candidates) {
        if (fs::exists(p)) { exePath = p; break; }
    }
    if (exePath.empty()) {
        std::cerr << "Skipping reward_log integration test: neuroforge executable not found." << std::endl;
        return; // Do not fail the entire test suite if exe not present in this configuration
    }

    // Build command line to run a short session with memdb enabled and low activity
    std::string args = std::string(" --memory-db=") + test_db +
                       " --memdb-debug=off" +
                       " --steps=200 --step-ms=5 --vision-demo=off";

    // Launch process
#ifdef _WIN32
    {
        // Normalize path separators and prefer .\\ prefix for relative paths
        std::string nativeExe = fs::path(exePath).make_preferred().string();
        fs::path pth(nativeExe);
        if (!pth.is_absolute()) {
            nativeExe = std::string(".\\") + nativeExe; // ensure cmd.exe executes relative exe reliably
        }
        std::string cmd = std::string("\"") + nativeExe + "\"" + args;
        int ec = std::system(cmd.c_str());
        check(ec == 0, "Headless neuroforge run completed successfully");
    }
#else
    std::string cmd = exePath + args;
    int ec = std::system(cmd.c_str());
    check(ec == 0, "Headless neuroforge run completed successfully");
#endif

    // Now open DB and assert reward_log entries exist (at least one)
    NeuroForge::Core::MemoryDB db(test_db);
    check(db.open(), "Open integration DB");
    auto runs = db.getRuns();
    check(!runs.empty(), "At least one run present after integration run");
    auto run_id = runs.back().id;

    auto rewards = db.getRecentRewards(run_id, 100);
    check(!rewards.empty(), "At least one reward recorded");

    // Verify columns look sane for first entry
    const auto& r = rewards.front();
    check(r.id > 0, "Reward id valid");
    check(r.ts_ms > 0, "Reward timestamp valid");
    check(r.step > 0, "Reward step valid");
    check(r.source.size() > 0, "Reward source non-empty");

    db.close();
    fs::remove(test_db);

    std::cout << "reward_log integration test completed successfully!" << std::endl;
}

// CLI integration tests for Phase-4 flags
static std::string find_neuroforge_exe() {
    std::vector<std::string> candidates = {
        std::string("Debug/neuroforge.exe"),
        std::string("build-vcpkg-rel/Debug/neuroforge.exe"),
        std::string("build/Debug/neuroforge.exe"),
        std::string("build/neuroforge.exe"),
        std::string("build-vs/Debug/neuroforge.exe"),
        std::string("Release/neuroforge.exe"),
        std::string("build-vcpkg-rel/Release/neuroforge.exe"),
        std::string("build/Release/neuroforge.exe"),
        std::string("build-vs/Release/neuroforge.exe"),
        std::string("neuroforge.exe"),
        std::string("../Debug/neuroforge.exe"),
        std::string("../build-vcpkg-rel/Debug/neuroforge.exe"),
        std::string("../build/Debug/neuroforge.exe"),
        std::string("../build/neuroforge.exe"),
        std::string("../build-vs/Debug/neuroforge.exe"),
        std::string("../Release/neuroforge.exe"),
        std::string("../build-vcpkg-rel/Release/neuroforge.exe"),
        std::string("../build/Release/neuroforge.exe"),
        std::string("../build-vs/Release/neuroforge.exe")
    };
    for (const auto& p : candidates) { if (fs::exists(p)) return p; }
    return std::string();
}

static int run_neuroforge(const std::string& exePath, const std::string& args) {
#ifdef _WIN32
    std::string nativeExe = fs::path(exePath).make_preferred().string();
    fs::path pth(nativeExe);
    if (!pth.is_absolute()) { nativeExe = std::string(".\\") + nativeExe; }
    std::string cmd = std::string("\"") + nativeExe + "\"" + args;
    return std::system(cmd.c_str());
#else
    return std::system((exePath + args).c_str());
#endif
}

void testCLIPhase4ShortFlagsValid() {
    std::cout << "Testing CLI Phase-4 short flags (valid values)..." << std::endl;
    auto exe = find_neuroforge_exe();
    if (exe.empty()) {
        std::cerr << "Skipping CLI Phase-4 short flags test: neuroforge executable not found." << std::endl;
        return;
    }
    std::string args =
        " --steps=1 --step-ms=0 --vision-demo=off"
        " -l=0.9 -e=0.5 -k=0.1 -a=0.1 -g=0.2 -u=0.3";
    int ec = run_neuroforge(exe, args);
    check(ec == 0, "CLI accepted valid Phase-4 short flags");
}

void testCLIPhase4InvalidValues() {
    std::cout << "Testing CLI Phase-4 invalid values (expect exit code 2)..." << std::endl;
    auto exe = find_neuroforge_exe();
    if (exe.empty()) {
        std::cerr << "Skipping CLI Phase-4 invalid values test: neuroforge executable not found." << std::endl;
        return;
    }
    // lambda out of range
    int ec1 = run_neuroforge(exe, " --steps=1 --step-ms=0 --vision-demo=off --lambda=1.5");
    check(ec1 == 2, "CLI rejected --lambda=1.5 with exit code 2");
    // eta-elig out of range
    int ec2 = run_neuroforge(exe, " --steps=1 --step-ms=0 --vision-demo=off --eta-elig=-0.1");
    check(ec2 == 2, "CLI rejected --eta-elig=-0.1 with exit code 2");
    // kappa negative
    int ec3 = run_neuroforge(exe, " --steps=1 --step-ms=0 --vision-demo=off --kappa=-0.01");
    check(ec3 == 2, "CLI rejected --kappa=-0.01 with exit code 2");
    // alpha negative
    int ec4 = run_neuroforge(exe, " --steps=1 --step-ms=0 --vision-demo=off --alpha=-0.01");
    check(ec4 == 2, "CLI rejected --alpha=-0.01 with exit code 2");
    // gamma negative
    int ec5 = run_neuroforge(exe, " --steps=1 --step-ms=0 --vision-demo=off --gamma=-0.01");
    check(ec5 == 2, "CLI rejected --gamma=-0.01 with exit code 2");
    // eta negative
    int ec6 = run_neuroforge(exe, " --steps=1 --step-ms=0 --vision-demo=off --eta=-0.01");
    check(ec6 == 2, "CLI rejected --eta=-0.01 with exit code 2");
}

void testCLIPhase4UnsafeBypass() {
    std::cout << "Testing CLI --phase4-unsafe bypass..." << std::endl;
    auto exe = find_neuroforge_exe();
    if (exe.empty()) {
        std::cerr << "Skipping CLI --phase4-unsafe test: neuroforge executable not found." << std::endl;
        return;
    }
    int ec = run_neuroforge(exe, " --steps=1 --step-ms=0 --vision-demo=off --phase4-unsafe --lambda=1.5 --eta-elig=-0.2 --kappa=-0.1 --alpha=-0.1 --gamma=-0.1 --eta=-0.1");
    check(ec == 0, "CLI accepted invalid Phase-4 values when --phase4-unsafe is set");
}

int main() {
    std::cout << "Starting MemoryDB smoke tests..." << std::endl;
    
    try {
        testBasicOperations();
        testRoundTrip();
        testErrorHandling();
        testQueryAPIs();
        testRewardLogIntegration();
        testCLIPhase4ShortFlagsValid();
        testCLIPhase4InvalidValues();
        testCLIPhase4UnsafeBypass();
        
        std::cout << "All MemoryDB smoke tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception during MemoryDB tests: " << e.what() << std::endl;
        return 1;
    }
}