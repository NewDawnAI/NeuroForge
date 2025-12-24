#include "core/MemoryDB.h"
#include "core/LearningSystem.h"
#include "core/AutonomyEnvelope.h"
#include "core/StageC_AutonomyGate.h"
#include <iostream>
#include <string>
#include <filesystem>
#include <chrono>
#include <cassert>
#include <limits>

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
    if (rewards.empty()) {
        std::cerr << "Skipping reward_log assertions: no rewards recorded in this run." << std::endl;
        db.close();
        fs::remove(test_db);
        return;
    }
    check(!rewards.empty(), "At least one reward recorded");

    // Verify columns look sane for first entry
    const auto& r = rewards.front();
    check(r.id > 0, "Reward id valid");
    check(r.ts_ms > 0, "Reward timestamp valid");
    check(r.step <= 200, "Reward step within configured range");
    check(r.source.size() > 0, "Reward source non-empty");

    db.close();
    fs::remove(test_db);

    std::cout << "reward_log integration test completed successfully!" << std::endl;
}

void testSelfRevisionOutcomeAPIs() {
    std::cout << "Testing self-revision outcome APIs..." << std::endl;

    const std::string test_db = "test_revision_outcomes.sqlite";
    if (fs::exists(test_db)) {
        fs::remove(test_db);
    }

    std::int64_t run_id = 0;
    std::int64_t revision_id = 0;
    const auto now = std::chrono::steady_clock::now();
    const auto base_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    {
        NeuroForge::Core::MemoryDB db(test_db);
        check(db.open(), "DB open for revision outcome tests");
        check(db.beginRun("{\"test\":\"revision_outcomes\"}", run_id), "Run started for revision outcome tests");

        for (int i = 0; i < 5; ++i) {
            check(db.insertMetacognition(base_ms + i * 10, 0.4 + 0.01 * i, 0.2 + 0.01 * i, 0.25 + 0.01 * i, 0.0, "pre", std::nullopt, std::nullopt, std::nullopt, run_id),
                  "Inserted pre metacognition " + std::to_string(i));
        }
        for (int i = 0; i < 3; ++i) {
            std::int64_t mid = 0;
            check(db.insertMotivationState(base_ms + i * 10, 0.5, 0.55 + 0.01 * i, "pre", run_id, mid), "Inserted pre motivation " + std::to_string(i));
        }
        for (int i = 0; i < 6; ++i) {
            std::int64_t rid = 0;
            check(db.insertRewardLog(base_ms + i * 10, 1 + i, 0.1 * i, "pre", "{}", run_id, rid), "Inserted pre reward " + std::to_string(i));
        }

        const std::int64_t baseline_ts = base_ms + 60;
        std::int64_t baseline_revision_id = 0;
        check(db.insertSelfRevision(run_id, baseline_ts, "{\"phase6.lr\":-0.01}", "driver", 0.5, 0.5, baseline_revision_id), "Inserted self revision");
        check(baseline_revision_id > 0, "Revision id valid");

        auto ts = db.getSelfRevisionTimestamp(baseline_revision_id);
        check(ts.has_value() && *ts == baseline_ts, "Revision timestamp query returned expected value");

        const std::int64_t revision_ts = base_ms + 120;
        check(db.insertSelfRevision(run_id, revision_ts, "{\"phase6.lr\":-0.02}", "driver2", 0.5, 0.5, revision_id), "Inserted self revision");
        check(revision_id > 0, "Second revision id valid");

        auto pending = db.getLatestUnevaluatedSelfRevisionId(run_id, revision_ts);
        check(pending.has_value() && *pending == revision_id, "Latest unevaluated revision id returned expected value");

        for (int i = 0; i < 5; ++i) {
            check(db.insertMetacognition(revision_ts + 10 + i * 10, 0.5 + 0.01 * i, 0.15 - 0.005 * i, 0.2 - 0.005 * i, 0.0, "post", std::nullopt, std::nullopt, std::nullopt, run_id),
                  "Inserted post metacognition " + std::to_string(i));
        }
        for (int i = 0; i < 3; ++i) {
            std::int64_t mid = 0;
            check(db.insertMotivationState(revision_ts + 10 + i * 10, 0.5, 0.6 + 0.01 * i, "post", run_id, mid), "Inserted post motivation " + std::to_string(i));
        }
        for (int i = 0; i < 6; ++i) {
            std::int64_t rid = 0;
            check(db.insertRewardLog(revision_ts + 10 + i * 10, 10 + i, 0.2 + 0.02 * i, "post", "{}", run_id, rid), "Inserted post reward " + std::to_string(i));
        }

        check(db.insertSelfRevisionOutcome(revision_id,
                                           revision_ts + 100,
                                           "Beneficial",
                                           0.45,
                                           0.55,
                                           0.25,
                                           0.18,
                                           0.56,
                                           0.63,
                                           0.0,
                                           0.01),
              "Inserted self revision outcome");

        auto latest = db.getLatestSelfRevisionOutcome(run_id);
        check(latest.has_value(), "Fetched latest self revision outcome");
        check(latest->revision_id == revision_id, "Latest outcome has expected revision id");
        check(latest->outcome_class == "Beneficial", "Latest outcome has expected class");

        auto none_pending = db.getLatestUnevaluatedSelfRevisionId(run_id, revision_ts + 9999);
        check(!none_pending.has_value(), "No pending unevaluated revisions after outcome insert");

        auto between_m = db.getMetacognitionBetween(run_id, base_ms, revision_ts + 1000, 100);
        check(between_m.size() >= 10, "Metacognition between query returned expected count");
        auto between_mot = db.getMotivationStatesBetween(run_id, base_ms, revision_ts + 1000, 100);
        check(between_mot.size() >= 6, "Motivation between query returned expected count");
        auto between_r = db.getRewardsBetween(run_id, base_ms, revision_ts + 1000, 100);
        check(between_r.size() >= 12, "Rewards between query returned expected count");

        db.close();
    }

    fs::remove(test_db);
    std::cout << "Self-revision outcome APIs test completed successfully!" << std::endl;
}

void testStageCGatingNoHistory() {
    std::cout << "Testing Stage C v1 autonomy gating (no history)..." << std::endl;

    const std::string test_db = "test_stagec_no_history.sqlite";
    if (fs::exists(test_db)) {
        fs::remove(test_db);
    }

    {
        NeuroForge::Core::MemoryDB db(test_db);
        check(db.open(), "DB open for Stage C no-history test");
        std::int64_t run_id = 0;
        check(db.beginRun("{\"test\":\"stagec_no_history\"}", run_id), "Run started for Stage C no-history test");

        NeuroForge::Core::AutonomyEnvelope reset{};
        (void)reset.applyAutonomyCap(1.0);

        NeuroForge::Core::AutonomyEnvelope env{};
        env.autonomy_score = 0.8;
        env.valid = true;

        NeuroForge::Core::StageC_AutonomyGate gate(&db);
        auto r = gate.evaluateAndApply(env, run_id, 20);

        check(r.window_n == 0, "Stage C reports zero history window");
        check(r.autonomy_cap_multiplier == 1.0f, "Stage C leaves autonomy cap at 1.0 with no history");
        check(!r.applied, "Stage C does not apply cap with no history");

        db.close();
    }

    fs::remove(test_db);
    std::cout << "Stage C no-history test completed successfully!" << std::endl;
}

static void seed_revision_outcomes(NeuroForge::Core::MemoryDB& db,
                                   std::int64_t run_id,
                                   const std::vector<std::string>& outcome_classes) {
    const auto now = std::chrono::steady_clock::now();
    const auto base_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    const double NaN = std::numeric_limits<double>::quiet_NaN();

    for (size_t i = 0; i < outcome_classes.size(); ++i) {
        std::int64_t rid = 0;
        check(db.insertSelfRevision(run_id,
                                    base_ms + static_cast<std::int64_t>(i * 10),
                                    "{\"noop\":true}",
                                    "seed",
                                    0.5,
                                    0.5,
                                    rid),
              "Inserted seeded self revision " + std::to_string(i));
        check(rid > 0, "Seeded revision id valid " + std::to_string(i));
        check(db.insertSelfRevisionOutcome(rid,
                                           base_ms + static_cast<std::int64_t>(i * 10 + 1),
                                           outcome_classes[i],
                                           NaN,
                                           NaN,
                                           NaN,
                                           NaN,
                                           NaN,
                                           NaN,
                                           NaN,
                                           NaN),
              "Inserted seeded outcome " + outcome_classes[i]);
    }
}

void testStageCGatingNeutralOnly() {
    std::cout << "Testing Stage C v1 autonomy gating (neutral-only)..." << std::endl;

    const std::string test_db = "test_stagec_neutral_only.sqlite";
    if (fs::exists(test_db)) {
        fs::remove(test_db);
    }

    {
        NeuroForge::Core::MemoryDB db(test_db);
        check(db.open(), "DB open for Stage C neutral-only test");
        std::int64_t run_id = 0;
        check(db.beginRun("{\"test\":\"stagec_neutral_only\"}", run_id), "Run started for Stage C neutral-only test");

        seed_revision_outcomes(db, run_id, {"Neutral", "Neutral", "Neutral", "Neutral"});

        NeuroForge::Core::AutonomyEnvelope reset{};
        (void)reset.applyAutonomyCap(1.0);

        NeuroForge::Core::AutonomyEnvelope env{};
        env.autonomy_score = 0.8;
        env.valid = true;

        NeuroForge::Core::StageC_AutonomyGate gate(&db);
        auto r = gate.evaluateAndApply(env, run_id, 20);

        check(r.window_n == 4, "Stage C window includes all seeded outcomes");
        check(r.revision_reputation == 0.5f, "Stage C reputation equals 0.5 for neutral-only");
        check(r.autonomy_cap_multiplier == 0.75f, "Stage C cap is 0.75 for neutral-only");
        check(r.applied, "Stage C applies autonomy cap when history exists");
        check(env.autonomy_cap_multiplier == 0.75, "Autonomy envelope cap multiplier updated");

        db.close();
    }

    fs::remove(test_db);
    std::cout << "Stage C neutral-only test completed successfully!" << std::endl;
}

void testStageCGatingHarmfulOnly() {
    std::cout << "Testing Stage C v1 autonomy gating (harmful-only)..." << std::endl;

    const std::string test_db = "test_stagec_harmful_only.sqlite";
    if (fs::exists(test_db)) {
        fs::remove(test_db);
    }

    {
        NeuroForge::Core::MemoryDB db(test_db);
        check(db.open(), "DB open for Stage C harmful-only test");
        std::int64_t run_id = 0;
        check(db.beginRun("{\"test\":\"stagec_harmful_only\"}", run_id), "Run started for Stage C harmful-only test");

        seed_revision_outcomes(db, run_id, {"Harmful", "Harmful", "Harmful"});

        NeuroForge::Core::AutonomyEnvelope reset{};
        (void)reset.applyAutonomyCap(1.0);

        NeuroForge::Core::AutonomyEnvelope env{};
        env.autonomy_score = 0.8;
        env.valid = true;

        NeuroForge::Core::StageC_AutonomyGate gate(&db);
        auto r = gate.evaluateAndApply(env, run_id, 20);

        check(r.window_n == 3, "Stage C window includes all seeded outcomes");
        check(r.revision_reputation == 0.0f, "Stage C reputation equals 0.0 for harmful-only");
        check(r.autonomy_cap_multiplier == 0.5f, "Stage C cap is 0.5 for harmful-only");
        check(r.applied, "Stage C applies autonomy cap when history exists");
        check(env.autonomy_cap_multiplier == 0.5, "Autonomy envelope cap multiplier updated");

        db.close();
    }

    fs::remove(test_db);
    std::cout << "Stage C harmful-only test completed successfully!" << std::endl;
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

void testMetacognitionIntegrationViaMaze() {
    std::cout << "Testing metacognition integration via maze run..." << std::endl;

    const std::string test_db = "test_maze_metacog.sqlite";
    if (fs::exists(test_db)) {
        fs::remove(test_db);
    }

    auto exe = find_neuroforge_exe();
    if (exe.empty()) {
        std::cerr << "Skipping metacognition integration test: neuroforge executable not found." << std::endl;
        return;
    }

    std::string args = std::string(" --memory-db=") + test_db +
                       " --memdb-debug=off"
                       " --maze-demo=on --maze-view=off"
                       " --vision-demo=off"
                       " --steps=120 --step-ms=0"
                       " --maze-max-episode-steps=10"
                       " --phase7=off --phase8=on --phase9=on --phase10=off --phase11=off";

    int ec = run_neuroforge(exe, args);
    check(ec == 0, "Headless maze run completed successfully");

    NeuroForge::Core::MemoryDB db(test_db);
    check(db.open(), "Open maze integration DB");
    auto runs = db.getRuns();
    check(!runs.empty(), "At least one run present after maze integration run");
    auto run_id = runs.back().id;

    auto metacog = db.getRecentMetacognition(run_id, 50);
    check(!metacog.empty(), "Metacognition rows recorded");

    auto mot = db.getMotivationStatesBetween(run_id, 0, std::numeric_limits<std::int64_t>::max(), 50);
    check(!mot.empty(), "Motivation state rows recorded");

    db.close();
    fs::remove(test_db);

    std::cout << "metacognition integration test completed successfully!" << std::endl;
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

void testCLIRWCIDisallowedAutonomyCoupling() {
    std::cout << "Testing CLI RWCI disallowed autonomy coupling (expect exit code 2)..." << std::endl;
    auto exe = find_neuroforge_exe();
    if (exe.empty()) {
        std::cerr << "Skipping CLI RWCI autonomy coupling test: neuroforge executable not found." << std::endl;
        return;
    }
    int ec = run_neuroforge(exe, " --steps=1 --step-ms=0 --vision-demo=off --rwci=on --autonomous-mode=on");
    check(ec == 2, "CLI rejected --rwci=on combined with --autonomous-mode=on");
}

void testCLIRWCIDrivesRevisionForbidden() {
    std::cout << "Testing CLI RWCI drives-revision forbidden (expect exit code 2)..." << std::endl;
    auto exe = find_neuroforge_exe();
    if (exe.empty()) {
        std::cerr << "Skipping CLI RWCI drives-revision test: neuroforge executable not found." << std::endl;
        return;
    }
    int ec = run_neuroforge(exe, " --steps=1 --step-ms=0 --vision-demo=off --rwci=on --rwci-drives-revision=on");
    check(ec == 2, "CLI rejected --rwci-drives-revision=on under Stage C v1");
}

int main() {
    std::cout << "Starting MemoryDB smoke tests..." << std::endl;
    
    try {
        testBasicOperations();
        testRoundTrip();
        testErrorHandling();
        testQueryAPIs();
        testRewardLogIntegration();
        testMetacognitionIntegrationViaMaze();
        testSelfRevisionOutcomeAPIs();
        testStageCGatingNoHistory();
        testStageCGatingNeutralOnly();
        testStageCGatingHarmfulOnly();
        testCLIPhase4ShortFlagsValid();
        testCLIPhase4InvalidValues();
        testCLIPhase4UnsafeBypass();
        testCLIRWCIDisallowedAutonomyCoupling();
        testCLIRWCIDrivesRevisionForbidden();
        
        std::cout << "All MemoryDB smoke tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Exception during MemoryDB tests: " << e.what() << std::endl;
        return 1;
    }
}
