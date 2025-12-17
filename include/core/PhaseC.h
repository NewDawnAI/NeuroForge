#pragma once
#include <string>
#include <vector>
#include <map>
#include <random>
#include <fstream>
#include <filesystem>
#include <functional>
#include <algorithm>

namespace NeuroForge {

struct Assembly {
    int id = -1;
    std::string symbol; // e.g., "color:red", "shape:square", or sequence token "A"
    float score = 0.0f;
};

struct BindingRow {
    int step = 0;
    std::string role;     // e.g., "color" | "shape"
    std::string filler;   // e.g., "red" | "square"
    float strength = 0.0f; // [0,1]
};

struct SequenceRow {
    int step = 0;
    std::string target;     // expected token
    std::string predicted;  // winner token
    int correct = 0;         // 0|1
};

// --- Minimal Working Memory (role–filler items with decaying strengths) ---
struct WorkingMemoryItem {
    std::string role;
    std::string filler;
    float strength = 0.0f; // [0,1]
};

class WorkingMemory {
public:
    explicit WorkingMemory(std::size_t capacity = 6, float decay = 0.90f)
        : capacity_(capacity), decay_(decay) {}

    void setCapacity(std::size_t c) { capacity_ = c; evictWeak(); }
    void setDecay(float d) { decay_ = d; }
    std::size_t capacity() const { return capacity_; }
    float decay() const { return decay_; }

    // Apply one decay step; drop near-zero items and evict to capacity
    void decayStep() {
        for (auto &it : items_) it.strength *= decay_;
        // Remove near-zero entries
        items_.erase(std::remove_if(items_.begin(), items_.end(), [](const WorkingMemoryItem &it){ return it.strength < 1e-4f; }), items_.end());
        evictWeak();
    }

    // Insert/update an item; if same role+filler exists, max the strength; if same role different filler exists, keep both (soft WM)
    void write(const std::string &role, const std::string &filler, float strength) {
        float s = std::clamp(strength, 0.0f, 1.0f);
        for (auto &it : items_) {
            if (it.role == role && it.filler == filler) {
                it.strength = std::max(it.strength, s);
                evictWeak();
                return;
            }
        }
        items_.push_back(WorkingMemoryItem{role, filler, s});
        evictWeak();
    }

    // Keep at most max_count items for a given role by strongest strength
    void pruneRoleCapacity(const std::string& role, std::size_t max_count) {
        if (max_count == 0) return; // 0 means do not keep any (but we treat 0 as disabled elsewhere)
        // Collect indices for role
        std::vector<std::size_t> idx;
        idx.reserve(items_.size());
        for (std::size_t i = 0; i < items_.size(); ++i) {
            if (items_[i].role == role) idx.push_back(i);
        }
        if (idx.size() <= max_count) return;
        // Sort indices by strength desc
        std::stable_sort(idx.begin(), idx.end(), [&](std::size_t a, std::size_t b){ return items_[a].strength > items_[b].strength; });
        // Mark extras for removal
        std::vector<char> remove(items_.size(), 0);
        for (std::size_t k = max_count; k < idx.size(); ++k) remove[idx[k]] = 1;
        // Rebuild items_ without removed ones
        std::vector<WorkingMemoryItem> kept;
        kept.reserve(items_.size());
        for (std::size_t i = 0; i < items_.size(); ++i) {
            if (remove[i]) continue;
            kept.push_back(std::move(items_[i]));
        }
        items_.swap(kept);
        evictWeak();
    }

    std::vector<WorkingMemoryItem> snapshot() const { return items_; }

private:
    void evictWeak() {
        // If over capacity, evict the weakest until within capacity
        if (items_.size() <= capacity_) return;
        // Partial sort to find threshold
        std::stable_sort(items_.begin(), items_.end(), [](const WorkingMemoryItem &a, const WorkingMemoryItem &b){ return a.strength > b.strength; });
        if (items_.size() > capacity_) items_.resize(capacity_);
    }

    std::size_t capacity_;
    float decay_;
    std::vector<WorkingMemoryItem> items_;
};

// Minimal CSV logger mirroring the Python prototype schema
class PhaseCCSVLogger {
public:
    explicit PhaseCCSVLogger(const std::filesystem::path& out_dir);
    ~PhaseCCSVLogger();

    void logTimeline(int step, const Assembly& winner);
    void logAssemblies(int step, const std::vector<Assembly>& assemblies);

    // Lazily create optional logs on first use
    void logBinding(const BindingRow& row);
    void logSequence(const SequenceRow& row);
    // New: snapshot of working memory (all items at a given step)
    void logWorkingMemory(int step, const std::vector<WorkingMemoryItem>& items);

    const std::filesystem::path& outDir() const { return out_dir_; }

    // Optional: set a JSON sink to mirror CSV logs as line-delimited JSON
    void setJsonSink(std::function<void(const std::string&)> sink) { json_sink_ = std::move(sink); }
    const std::function<void(const std::string&)>& getJsonSink() const { return json_sink_; }

    // Optional helper: emit survival modulation telemetry
    void logSurvivalMod(int step,
                        const std::string& symbol,
                        float base_coherence,
                        float modulated_coherence,
                        float hazard_probability,
                        float risk_score,
                        float arousal_level,
                        float avoidance_drive,
                        float approach_drive,
                        float weight);

private:
    void ensureBindings();
    void ensureSequence();
    void ensureWorkingMemory();

    std::filesystem::path out_dir_;
    std::ofstream timeline_csv_;
    std::ofstream assemblies_csv_;
    std::ofstream bindings_csv_;
    std::ofstream sequence_csv_;
    std::ofstream wm_csv_;
    bool bindings_ready_ = false;
    bool sequence_ready_ = false;
    bool wm_ready_ = false;
    std::function<void(const std::string&)> json_sink_;
};

class GlobalWorkspacePhaseC {
public:
    GlobalWorkspacePhaseC(std::mt19937::result_type seed, PhaseCCSVLogger& logger);

    // Per-step tasks
    void stepBinding(int step);
    void stepSequence(int step);

    // Allow runtime tuning of WM parameters
    void setWorkingMemoryParams(std::size_t capacity, float decay) {
        working_memory_.setCapacity(capacity);
        working_memory_.setDecay(decay);
    }
    // Optional: limit number of token entries kept in WM for sequence mode (0 = unlimited)
    void setSequenceWindow(std::size_t w) { seq_window_ = w; }

private:
    // Loop stages
    std::vector<Assembly> formAssembliesBinding(const std::map<std::string, std::string>& percept, int step);
    std::vector<Assembly> formAssembliesSequence(const std::map<std::string, std::string>& percept, int step);
    Assembly compete(const std::vector<Assembly>& assemblies);

    // Perception generators
    std::map<std::string, std::string> perceiveBinding(int step);
    std::map<std::string, std::string> perceiveSequence(int step);

    // Helpers
    float randUniform(float a, float b);

private:
    std::mt19937 rng_;
    PhaseCCSVLogger& logger_;

    // Toy symbol universes
    std::vector<std::string> colors_{"red","blue","green"};
    std::vector<std::string> shapes_{"triangle","square","circle"};
    std::vector<std::string> seq_tokens_{"A","B","C","D"};

    // New: simple working memory that tracks recent bindings/tokens
    WorkingMemory working_memory_{}; // defaults: capacity=6, decay=0.90
    std::size_t seq_window_ = 0; // 0 = unlimited
};

} // namespace NeuroForge