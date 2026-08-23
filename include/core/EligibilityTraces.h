#pragma once

/**
 * @file EligibilityTraces.h
 * @brief Per-synapse eligibility, stored flat and updated without a global lock.
 *
 * WHY
 *
 * Eligibility was a `std::unordered_map<SynapseID, SynState>` guarded by one
 * process-wide mutex, where `SynState` held a single float. It is written from
 * `LearningSystem::onNeuronSpike`, which runs once per spike and touches every
 * synapse attached to the spiking neuron -- roughly 128 of them in the anatomical
 * brain. Every spike from every neuron therefore serialised on one lock and paid
 * ~128 hash lookups.
 *
 * Measured 2026-08-24, at --steps=20 on the 12-region anatomical brain:
 *
 *   closed loop, eligibility off   3 s
 *   eligibility on, no reward    200 s (killed)
 *   both                         200 s (killed)
 *
 * The reward path was not the cost; accumulation was, on its own. Three earlier
 * optimisations (an O(1) getNeuron index, removing two shared_ptr vector copies
 * per spike, caching findSynapseById) each helped and none was sufficient,
 * because the shape of the problem was the map and the lock rather than the work
 * around them.
 *
 * HOW
 *
 * SynapseIDs come from a monotonic `next_id_.fetch_add(1)`, so they are dense.
 * That allows a flat array indexed directly by id: no hashing, and one
 * `std::atomic<float>` per synapse that can be updated with relaxed loads and
 * stores instead of a mutex.
 *
 * Storage is a fixed table of block pointers rather than a `std::vector`, for
 * two reasons: `std::atomic` is neither copyable nor movable, so a vector cannot
 * reallocate one; and a fixed table means readers never observe the container
 * itself changing. A block is allocated once, under a mutex, and published with
 * a release store; readers acquire-load the pointer and then work lock-free.
 *
 * CONCURRENCY
 *
 * `bump()` is a relaxed read-modify-write and is NOT atomic as a whole. Two
 * spikes hitting the same synapse in the same instant can lose one increment.
 * That is deliberate: eligibility is a decaying trace that saturates at a cap,
 * an occasional lost increment is indistinguishable from slightly different
 * spike timing, and making it a CAS loop would reintroduce contention on exactly
 * the hot path this class exists to relieve.
 */

#include "Types.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>

namespace NeuroForge {
namespace Core {

class EligibilityTraces {
public:
    /// Synapses per block. 4096 floats = 16 KB, so one block is a handful of pages.
    static constexpr std::size_t kBlockSize = 4096;
    /// Table size. 8192 blocks covers ~33.5M synapses, well past fly scale.
    static constexpr std::size_t kMaxBlocks = 8192;

    EligibilityTraces() {
        for (auto &b : blocks_) {
            b.store(nullptr, std::memory_order_relaxed);
        }
    }

    ~EligibilityTraces() {
        for (auto &b : blocks_) {
            delete[] b.load(std::memory_order_relaxed);
        }
    }

    EligibilityTraces(const EligibilityTraces &) = delete;
    EligibilityTraces &operator=(const EligibilityTraces &) = delete;

    /// Add to a synapse's trace, saturating at `cap`. Lock-free once the block
    /// exists. See the concurrency note above for why this is not a CAS loop.
    void bump(NeuroForge::SynapseID sid, float amount, float cap = 1.0f) {
        std::atomic<float> *slot = slotFor(sid, true);
        if (!slot) {
            return;
        }
        const float cur = slot->load(std::memory_order_relaxed);
        slot->store(std::min(cap, cur + amount), std::memory_order_relaxed);
        noteHighest(sid);
    }

    /// Overwrite a synapse's trace. Used by notePrePost, which computes the new
    /// value from the old one and its own decay term.
    void set(NeuroForge::SynapseID sid, float value) {
        std::atomic<float> *slot = slotFor(sid, true);
        if (!slot) {
            return;
        }
        slot->store(value, std::memory_order_relaxed);
        noteHighest(sid);
    }

    float get(NeuroForge::SynapseID sid) const {
        const std::atomic<float> *slot =
            const_cast<EligibilityTraces *>(this)->slotFor(sid, false);
        return slot ? slot->load(std::memory_order_relaxed) : 0.0f;
    }

    /// Call fn(sid, eligibility) for every synapse whose trace is non-zero.
    ///
    /// Scans only up to the highest id ever touched, so an empty or sparsely
    /// used table costs almost nothing. This replaces iterating a map under a
    /// lock while the spike path is trying to write to it.
    template <typename Fn>
    void forEachActive(Fn &&fn) const {
        const std::size_t high = highest_.load(std::memory_order_acquire);
        if (high == 0 && get(0) == 0.0f) {
            return;
        }
        const std::size_t last_block = std::min(high / kBlockSize, kMaxBlocks - 1);
        for (std::size_t b = 0; b <= last_block; ++b) {
            std::atomic<float> *block = blocks_[b].load(std::memory_order_acquire);
            if (!block) {
                continue;
            }
            const std::size_t base = b * kBlockSize;
            for (std::size_t i = 0; i < kBlockSize; ++i) {
                const std::size_t sid = base + i;
                if (sid > high) {
                    break;
                }
                const float e = block[i].load(std::memory_order_relaxed);
                if (e != 0.0f) {
                    fn(static_cast<NeuroForge::SynapseID>(sid), e);
                }
            }
        }
    }

    /// Zero every trace. Blocks are kept so the memory is not churned.
    void clear() {
        const std::size_t high = highest_.load(std::memory_order_acquire);
        const std::size_t last_block = std::min(high / kBlockSize, kMaxBlocks - 1);
        for (std::size_t b = 0; b <= last_block; ++b) {
            std::atomic<float> *block = blocks_[b].load(std::memory_order_acquire);
            if (!block) {
                continue;
            }
            for (std::size_t i = 0; i < kBlockSize; ++i) {
                block[i].store(0.0f, std::memory_order_relaxed);
            }
        }
    }

    /// Highest synapse id that has ever been written.
    std::size_t highestId() const {
        return highest_.load(std::memory_order_acquire);
    }

private:
    std::atomic<float> *slotFor(NeuroForge::SynapseID sid, bool create) {
        const std::size_t idx = static_cast<std::size_t>(sid);
        const std::size_t b = idx / kBlockSize;
        if (b >= kMaxBlocks) {
            return nullptr; // beyond the table; ignore rather than corrupt
        }
        std::atomic<float> *block = blocks_[b].load(std::memory_order_acquire);
        if (!block) {
            if (!create) {
                return nullptr;
            }
            std::lock_guard<std::mutex> lock(grow_mutex_);
            block = blocks_[b].load(std::memory_order_relaxed);
            if (!block) {
                auto *fresh = new std::atomic<float>[kBlockSize];
                for (std::size_t i = 0; i < kBlockSize; ++i) {
                    fresh[i].store(0.0f, std::memory_order_relaxed);
                }
                blocks_[b].store(fresh, std::memory_order_release);
                block = fresh;
            }
        }
        return block + (idx % kBlockSize);
    }

    void noteHighest(NeuroForge::SynapseID sid) {
        const std::size_t idx = static_cast<std::size_t>(sid);
        std::size_t cur = highest_.load(std::memory_order_relaxed);
        while (idx > cur &&
               !highest_.compare_exchange_weak(cur, idx, std::memory_order_release,
                                               std::memory_order_relaxed)) {
        }
    }

    std::array<std::atomic<std::atomic<float> *>, kMaxBlocks> blocks_;
    std::atomic<std::size_t> highest_{0};
    std::mutex grow_mutex_; ///< block allocation only, never the update path
};

} // namespace Core
} // namespace NeuroForge
