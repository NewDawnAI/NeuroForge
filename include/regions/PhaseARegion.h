#pragma once

#include "core/Region.h"
#include "core/LanguageSystem.h"
#include "core/PhaseAMimicry.h"
#include "core/MemoryDB.h"
#include <memory>
#include <string>

namespace NeuroForge {
namespace Regions {

class PhaseAMimicryRegion : public Core::Region {
public:
    PhaseAMimicryRegion(const std::string& name, std::size_t neuron_count)
        : Core::Region(Core::RegionFactory::getNextId(), name,
                       Core::Region::Type::Special,
                       Core::Region::ActivationPattern::Asynchronous)
        , requested_neuron_count_(neuron_count) {}

    ~PhaseAMimicryRegion() override = default;

    void initialize() override;
    void reset() override;

    // New configuration setters (wired from CLI)
    void setTeacherEnabled(bool enabled) noexcept { teacher_enabled_ = enabled; }
    void setTrainingEnabled(bool enabled) noexcept { training_enabled_ = enabled; }
    void setTeacherAndTraining(bool teacher_enabled, bool training_enabled) noexcept {
        teacher_enabled_ = teacher_enabled;
        training_enabled_ = training_enabled;
    }

    // Allow external systems (main/brain) to inject a shared MemoryDB and optional run id
    void setMemoryDB(std::shared_ptr<Core::MemoryDB> db, std::int64_t run_id) override;

protected:
    void processRegionSpecific(float delta_time) override;

private:
    std::size_t requested_neuron_count_ = 0;
    std::shared_ptr<Core::LanguageSystem> language_system_;
    std::unique_ptr<Core::PhaseAMimicry> phase_a_;

    // Minimal persistence path (M3-full Step 1)
    std::shared_ptr<Core::MemoryDB> memory_db_;
    std::int64_t run_id_ = 0;
    std::uint64_t ticks_ = 0;

    // Flags controlled via CLI
    bool teacher_enabled_ = false;
    bool training_enabled_ = true;
};

} // namespace Regions
} // namespace NeuroForge