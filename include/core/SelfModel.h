#pragma once

// SelfModel is a read-first, write-restricted mirror of the database-backed Self System.
// Step 2 requires a structural spine that loads identity, personality, and social snapshots
// without changing behavior or applying policies. This module provides const accessors only.

#include <string>
#include <optional>

#include "core/MemoryDB.h"

namespace NeuroForge {
namespace Core {

// Minimal identity snapshot cached in memory
struct IdentitySnapshot {
    std::int64_t ts_ms{0};
    std::uint64_t step{0};
    std::string identity_vector_json;
    std::optional<double> confidence;
    std::string notes;
};

// Minimal personality traits snapshot (latest approved row)
struct PersonalityTraits {
    std::int64_t ts_ms{0};
    std::uint64_t step{0};
    std::string trait_json;
    std::optional<int> source_phase;
    std::optional<std::int64_t> revision_id;
    std::string notes;
};

// Minimal social state snapshot
struct SocialState {
    std::int64_t ts_ms{0};
    std::uint64_t step{0};
    std::string role;
    std::string norm_json;
    std::optional<double> reputation;
    std::optional<double> confidence;
    std::string notes;
};

class SelfModel {
public:
    // Construct with a database reference; SelfModel does not own the DB
    explicit SelfModel(MemoryDB& db);

    // Load the latest snapshots for a given run (read-only)
    void loadForRun(std::int64_t run_id);

    // Read-only accessors
    const IdentitySnapshot& identity() const noexcept;
    const PersonalityTraits& personality() const noexcept;
    const SocialState& social() const noexcept;

    // Diagnostics
    bool isLoaded() const noexcept;
    std::int64_t runId() const noexcept;

private:
    MemoryDB& db_;
    std::int64_t run_id_{-1};
    bool loaded_{false};

    IdentitySnapshot identity_;
    PersonalityTraits personality_;
    SocialState social_;
};

} // namespace Core
} // namespace NeuroForge

