#include "regions/PhaseARegion.h"
#include "core/RegionRegistry.h"
#include "core/MemoryDB.h"
#include <algorithm>
#include <random>
#include <iostream>
#include <chrono>

namespace NeuroForge {
namespace Regions {

void PhaseAMimicryRegion::initialize() {
    // Create neurons if requested
    if (requested_neuron_count_ > 0 && getNeuronCount() == 0) {
        createNeurons(requested_neuron_count_);
    }

    // Initialize LanguageSystem (default config)
    if (!language_system_) {
        language_system_ = std::make_shared<Core::LanguageSystem>(Core::LanguageSystem::Config{});
        language_system_->initialize();
    }

    // Construct Phase A (no MemoryDB by default)
    if (!phase_a_) {
        auto cfg = Core::PhaseAMimicryFactory::createDefaultConfig();
        phase_a_ = Core::PhaseAMimicryFactory::create(language_system_, nullptr, cfg);
        phase_a_->initialize();
    }

    // Minimal persistence path: try to open MemoryDB at a default path
    if (!memory_db_) {
        // Use a local DB file in project root or working directory
        memory_db_ = std::make_shared<Core::MemoryDB>("phase_a_demo.db");
        memory_db_->setDebug(false);
        if (memory_db_->open()) {
            // If previous runs exist, log the fact for visibility
            auto runs = memory_db_->getRuns();
            if (!runs.empty()) {
                std::cerr << "[PhaseAMimicryRegion] Found " << runs.size() << " previous run(s) in DB. Last metadata: "
                          << (runs.back().metadata_json) << std::endl;
                // Read back last persisted tick (stored in reward_updates field) from the most recent run
                std::int64_t last_run_id = runs.back().id;
                std::uint64_t last_reward_updates = 0;
                if (memory_db_->getLatestRewardUpdates(last_run_id, last_reward_updates)) {
                    std::cerr << "[PhaseAMimicryRegion] Previous run (id=" << last_run_id
                              << ") latest persisted tick = " << last_reward_updates << std::endl;
                } else {
                    std::cerr << "[PhaseAMimicryRegion] Previous run (id=" << last_run_id
                              << ") has no persisted ticks yet" << std::endl;
                }
            }
            // Begin a new run with simple metadata containing region name
            std::string meta = std::string("{\"region\":\"") + getName() + "\",\"type\":\"phase_a\"}";
            if (!memory_db_->beginRun(meta, run_id_)) {
                std::cerr << "[PhaseAMimicryRegion] Failed to begin run in MemoryDB" << std::endl;
                run_id_ = 0;
            }
        } else {
            std::cerr << "[PhaseAMimicryRegion] MemoryDB open failed; persistence disabled" << std::endl;
            memory_db_.reset();
        }
    }

    setActive(true);
}

void PhaseAMimicryRegion::reset() {
    if (phase_a_) phase_a_->reset();
    if (language_system_) language_system_->reset();
    // Keep DB open across resets within same process; do not close here
}

void PhaseAMimicryRegion::processRegionSpecific(float delta_time) {
    // Advance language development a bit
    if (language_system_) {
        language_system_->updateDevelopment(delta_time);
    }

    // Tick counter for persistence demo
    ++ticks_;

    // Persist a very simple snapshot each step
    if (memory_db_ && run_id_ > 0) {
        Core::LearningSystem::Statistics s{}; // zeros ok; we just need a payload
        // Encode teacher type summary as part of metadata in the learning_stats row via numeric fields
        // We reuse reward_updates to store the ticks for quick retrieval in tests
        s.reward_updates = ticks_;
        (void)memory_db_->insertLearningStats(
            static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()),
            ticks_,
            /*processing_hz*/ 0.0,
            s,
            run_id_
        );
    }
}

// Allow external systems (main/brain) to inject a shared MemoryDB and optional run id
void PhaseAMimicryRegion::setMemoryDB(std::shared_ptr<Core::MemoryDB> db, std::int64_t run_id) {
    memory_db_ = std::move(db);
    run_id_ = run_id;
    // If Phase A already exists and expects a DB, wire it now
    // Current PhaseAMimicry interface keeps a memory_db_ internally; save/load calls use it.
    if (phase_a_) {
        // Recreate Phase A with same config but attached DB if available
        auto cfg = phase_a_->getConfig();
        phase_a_.reset();
        phase_a_ = Core::PhaseAMimicryFactory::create(language_system_, memory_db_, cfg);
        phase_a_->initialize();
    }
}

} // namespace Regions
} // namespace NeuroForge

namespace {
// Register factory and aliases for Phase A region
static const NeuroForge::Core::RegisterRegionFactory reg_phase_a(
    "phase_a",
    [](const std::string& name, std::size_t n){ return std::make_shared<NeuroForge::Regions::PhaseAMimicryRegion>(name, n); }
);
static const NeuroForge::Core::RegisterRegionAlias alias_pa("pa", "phase_a");
static const NeuroForge::Core::RegisterRegionAlias alias_mimicry("mimicry", "phase_a");

// Force-link symbol for this translation unit
extern "C" void NF_ForceLink_PhaseARegion() {}
}