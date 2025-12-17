#include "core/SelfModel.h"
#include <iostream>

namespace NeuroForge {
namespace Core {

SelfModel::SelfModel(MemoryDB& db) : db_(db) {}

void SelfModel::loadForRun(std::int64_t run_id) {
    run_id_ = run_id;
    loaded_ = false;

    // Load latest identity snapshot
    auto id_row = db_.getLatestSelfConcept(run_id_);
    if (id_row.has_value()) {
        identity_.ts_ms = id_row->ts_ms;
        identity_.step = id_row->step;
        identity_.identity_vector_json = id_row->identity_vector_json;
        identity_.confidence = id_row->confidence;
        identity_.notes = id_row->notes;
    } else {
        identity_ = IdentitySnapshot{};
    }

    // Load latest approved personality traits
    auto pers_row = db_.getLatestApprovedPersonality(run_id_);
    if (pers_row.has_value()) {
        personality_.ts_ms = pers_row->ts_ms;
        personality_.step = pers_row->step;
        personality_.trait_json = pers_row->trait_json;
        personality_.source_phase = pers_row->source_phase;
        personality_.revision_id = pers_row->revision_id;
        personality_.notes = pers_row->notes;
    } else {
        personality_ = PersonalityTraits{};
    }

    // Load latest social self snapshot
    auto soc_row = db_.getLatestSocialSelf(run_id_);
    if (soc_row.has_value()) {
        social_.ts_ms = soc_row->ts_ms;
        social_.step = soc_row->step;
        social_.role = soc_row->role;
        social_.norm_json = soc_row->norm_json;
        social_.reputation = soc_row->reputation;
        social_.confidence = soc_row->confidence;
        social_.notes = soc_row->notes;
    } else {
        social_ = SocialState{};
    }

    loaded_ = true;
    // Optional: emit a concise debug line to help confirm loading order
    std::cerr << "[SelfModel] loaded run_id=" << run_id_
              << " identity_ts=" << identity_.ts_ms
              << " personality_ts=" << personality_.ts_ms
              << " social_ts=" << social_.ts_ms << std::endl;
}

const IdentitySnapshot& SelfModel::identity() const noexcept { return identity_; }
const PersonalityTraits& SelfModel::personality() const noexcept { return personality_; }
const SocialState& SelfModel::social() const noexcept { return social_; }

bool SelfModel::isLoaded() const noexcept { return loaded_; }
std::int64_t SelfModel::runId() const noexcept { return run_id_; }

} // namespace Core
} // namespace NeuroForge

