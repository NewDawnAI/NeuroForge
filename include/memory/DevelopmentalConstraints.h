#pragma once

#include <vector>
#include <string>
#include <functional>
#include <chrono>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <memory>

namespace NeuroForge {
namespace Memory {

// Simplified stage enum retained for potential external use
enum class DevelopmentalStage {
    Infant,
    Toddler,
    Child,
    Adolescent,
    Adult,
    Elderly
};

// Retain milestone types for forward compatibility (not used in .cpp)
struct DevelopmentalMilestone {
    std::string name;
    DevelopmentalStage required_stage{DevelopmentalStage::Adult};
    std::vector<std::string> prerequisites;
    bool achieved{false};
    std::chrono::steady_clock::time_point achievement_time{};
};

// Configuration aligned to .cpp usage
struct DevelopmentalConfig {
    bool enable_critical_periods{true};
    bool enable_age_dependent_decay{true};
    bool enable_pruning_periods{true};
    std::uint64_t update_interval_ms{1000};
    std::uint64_t maturation_time_ms{24ULL * 60ULL * 60ULL * 1000ULL};
    float base_learning_rate{1.0f};
    float global_plasticity_decay_rate{0.5f};
};

// CriticalPeriod aligned to .cpp implementation
struct CriticalPeriod {
    enum class PeriodType { Enhancement, Restriction, Specialization, Pruning, Stabilization };

    std::string period_name;
    std::string description;
    std::uint64_t start_time_ms{0};
    std::uint64_t end_time_ms{0};
    std::uint64_t peak_time_ms{0};
    float plasticity_multiplier{1.0f};
    float learning_rate_multiplier{1.0f};
    float consolidation_multiplier{1.0f};
    float sensitivity_curve{1.0f};
    PeriodType type{PeriodType::Enhancement};
    bool is_active{true};
    std::vector<std::string> affected_regions;
    std::vector<std::string> affected_modalities;
    std::vector<std::string> learning_types;

    CriticalPeriod() = default;
    CriticalPeriod(const std::string& name,
                   std::uint64_t start_ms,
                   std::uint64_t end_ms,
                   float plasticity_mult,
                   PeriodType period_type);

    bool isActiveAt(std::uint64_t current_time_ms) const;
    float getPlasticityMultiplierAt(std::uint64_t current_time_ms) const;
    float getLearningRateMultiplierAt(std::uint64_t current_time_ms) const;
    bool affectsRegion(const std::string& region_name) const;
    bool affectsModality(const std::string& modality_name) const;
    bool affectsLearningType(const std::string& learning_type) const;
};

// Learning modulation aligned to .cpp
struct LearningModulation {
    float plasticity_multiplier{1.0f};
    float learning_rate_multiplier{1.0f};
    float consolidation_multiplier{1.0f};
    bool is_enhanced{false};
    bool is_restricted{false};
};

class DevelopmentalConstraints {
public:
    using Config = DevelopmentalConfig;

    explicit DevelopmentalConstraints(const Config& config = Config{});
    ~DevelopmentalConstraints() = default;

    // Critical period management
    bool defineCriticalPeriod(const CriticalPeriod& period);
    bool removeCriticalPeriod(const std::string& period_name);
    std::unique_ptr<CriticalPeriod> getCriticalPeriod(const std::string& period_name) const;
    std::vector<CriticalPeriod> getActiveCriticalPeriods() const;
    std::vector<CriticalPeriod> getCriticalPeriods() const { return critical_periods_; }
    bool isInCriticalPeriod(const std::string& period_name) const;

    // Region multipliers
    float getCurrentPlasticityMultiplier(const std::string& region_name) const;
    float getCurrentLearningRateMultiplier(const std::string& region_name) const;
    float getCurrentConsolidationMultiplier(const std::string& region_name) const;

    // Learning modulation
    LearningModulation getLearningModulation(const std::string& learning_type,
                                             const std::string& region_name = "") const;

    // Age/maturation
    void advanceSystemAge(std::uint64_t time_ms);
    std::uint64_t getCurrentSystemAge() const;
    float getMaturationLevel() const;
    bool isSystemMature() const;

    // Updates/pruning
    void updateConstraints(bool force_update = false);
    float applyAgeBasedPlasticityDecay() const;
    float triggerSynapticPruning(const std::string& region_name);
    bool shouldPruneRegion(const std::string& region_name) const;

    // Static factory methods
    static CriticalPeriod createVisualCriticalPeriod(float start_hours,
                                                     float duration_hours,
                                                     float enhancement_factor);
    static CriticalPeriod createAuditoryCriticalPeriod(float start_hours,
                                                       float duration_hours,
                                                       float enhancement_factor);
    static CriticalPeriod createLanguageCriticalPeriod(float start_hours,
                                                       float duration_hours,
                                                       float enhancement_factor);
    static CriticalPeriod createMotorCriticalPeriod(float start_hours,
                                                    float duration_hours,
                                                    float enhancement_factor);
    static CriticalPeriod createPruningPeriod(float start_hours,
                                              float duration_hours,
                                              float pruning_strength);

    // Initialization and stats
    size_t initializeStandardDevelopment(bool enable_all_periods = true);

    struct Statistics {
        std::uint64_t system_age_ms{0};
        float maturation_level{0.0f};
        std::uint64_t total_period_activations{0};
        std::uint64_t total_constraint_updates{0};
        std::uint64_t active_periods_count{0};
        std::size_t total_periods_defined{0};
        bool system_mature{false};
        float current_global_plasticity_multiplier{1.0f};
        float current_global_learning_rate_multiplier{1.0f};
        std::vector<std::string> active_period_names;
    };

    Statistics getStatistics() const;

    // Config and lifecycle
    void setConfig(const Config& new_config);
    void clearAllPeriods();
    void resetSystemAge();
    bool isOperational() const;

private:
    // Cache recomputation
    float calculateModulationCurve(std::uint64_t current_time,
                                   std::uint64_t start_time,
                                   std::uint64_t end_time,
                                   std::uint64_t peak_time,
                                   float curve_shape) const;
    void updateModulationFactors();

    // Time helpers
    std::uint64_t getCurrentTimestamp() const;
    static std::uint64_t hoursToMilliseconds(float hours);

    // Validation
    bool validateCriticalPeriod(const CriticalPeriod& period) const;

private:
    Config config_{};

    // Periods
    std::vector<CriticalPeriod> critical_periods_;
    std::unordered_map<std::string, std::size_t> period_name_index_;

    // Cached multipliers
    mutable std::unordered_map<std::string, float> current_plasticity_multipliers_;
    mutable std::unordered_map<std::string, float> current_learning_rate_multipliers_;
    mutable std::unordered_map<std::string, float> current_consolidation_multipliers_;

    // Stats
    std::atomic<std::uint64_t> total_period_activations_{0};
    std::atomic<std::uint64_t> total_constraint_updates_{0};
    std::atomic<std::uint64_t> active_periods_count_{0};

    // Time state
    std::atomic<std::uint64_t> system_birth_time_{0};
    std::atomic<std::uint64_t> last_update_time_{0};

    // Synchronization
    mutable std::mutex constraints_mutex_;
};

} // namespace Memory
} // namespace NeuroForge