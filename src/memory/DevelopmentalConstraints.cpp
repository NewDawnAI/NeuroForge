#include "memory/DevelopmentalConstraints.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <unordered_set>

namespace NeuroForge {
namespace Memory {

// CriticalPeriod Implementation

CriticalPeriod::CriticalPeriod(const std::string& name,
                              std::uint64_t start_ms,
                              std::uint64_t end_ms,
                              float plasticity_mult,
                              PeriodType period_type)
    : period_name(name)
    , start_time_ms(start_ms)
    , end_time_ms(end_ms)
    , peak_time_ms((start_ms + end_ms) / 2)  // Default peak at middle
    , plasticity_multiplier(plasticity_mult)
    , learning_rate_multiplier(plasticity_mult)
    , consolidation_multiplier(plasticity_mult)
    , type(period_type) {
}

bool CriticalPeriod::isActiveAt(std::uint64_t current_time_ms) const {
    return is_active && current_time_ms >= start_time_ms && current_time_ms <= end_time_ms;
}

float CriticalPeriod::getPlasticityMultiplierAt(std::uint64_t current_time_ms) const {
    if (!isActiveAt(current_time_ms)) {
        return 1.0f;  // No modulation outside period
    }
    
    // Calculate position within period [0,1]
    float position = static_cast<float>(current_time_ms - start_time_ms) / 
                    static_cast<float>(end_time_ms - start_time_ms);
    
    // Apply sensitivity curve (Gaussian-like curve with peak in middle)
    float curve_factor = 1.0f;
    if (sensitivity_curve != 1.0f) {
        // Create bell curve centered at peak
        float peak_position = static_cast<float>(peak_time_ms - start_time_ms) / 
                             static_cast<float>(end_time_ms - start_time_ms);
        float distance_from_peak = std::abs(position - peak_position);
        curve_factor = std::exp(-distance_from_peak * distance_from_peak * sensitivity_curve);
    }
    
    // Apply type-specific modulation
    float base_multiplier = plasticity_multiplier;
    switch (type) {
        case PeriodType::Enhancement:
            return 1.0f + (base_multiplier - 1.0f) * curve_factor;
        case PeriodType::Restriction:
            return 1.0f - (1.0f - base_multiplier) * curve_factor;
        case PeriodType::Specialization:
            return base_multiplier * curve_factor;
        case PeriodType::Pruning:
            return std::max(0.1f, 1.0f - base_multiplier * curve_factor);
        case PeriodType::Stabilization:
            return std::max(0.5f, 1.0f - (1.0f - base_multiplier) * curve_factor);
    }
    
    return base_multiplier * curve_factor;
}

float CriticalPeriod::getLearningRateMultiplierAt(std::uint64_t current_time_ms) const {
    if (!isActiveAt(current_time_ms)) {
        return 1.0f;
    }
    
    // Similar calculation to plasticity multiplier but for learning rate
    float position = static_cast<float>(current_time_ms - start_time_ms) / 
                    static_cast<float>(end_time_ms - start_time_ms);
    
    float curve_factor = 1.0f;
    if (sensitivity_curve != 1.0f) {
        float peak_position = static_cast<float>(peak_time_ms - start_time_ms) / 
                             static_cast<float>(end_time_ms - start_time_ms);
        float distance_from_peak = std::abs(position - peak_position);
        curve_factor = std::exp(-distance_from_peak * distance_from_peak * sensitivity_curve);
    }
    
    return learning_rate_multiplier * curve_factor;
}

bool CriticalPeriod::affectsRegion(const std::string& region_name) const {
    if (affected_regions.empty()) {
        return true;  // Affects all regions if none specified
    }
    
    return std::find(affected_regions.begin(), affected_regions.end(), region_name) != 
           affected_regions.end();
}

bool CriticalPeriod::affectsModality(const std::string& modality_name) const {
    if (affected_modalities.empty()) {
        return true;  // Affects all modalities if none specified
    }
    
    return std::find(affected_modalities.begin(), affected_modalities.end(), modality_name) != 
           affected_modalities.end();
}

bool CriticalPeriod::affectsLearningType(const std::string& learning_type) const {
    if (learning_types.empty()) {
        return true;  // Affects all learning types if none specified
    }
    
    return std::find(learning_types.begin(), learning_types.end(), learning_type) != 
           learning_types.end();
}

// DevelopmentalConstraints Implementation

DevelopmentalConstraints::DevelopmentalConstraints(const Config& config)
    : config_(config) {
    
    // Initialize system birth time
    system_birth_time_.store(getCurrentTimestamp());
    last_update_time_.store(getCurrentTimestamp());
}

bool DevelopmentalConstraints::defineCriticalPeriod(const CriticalPeriod& period) {
    if (!validateCriticalPeriod(period)) {
        return false;
    }
    
    std::unique_lock<std::shared_mutex> lock(constraints_mutex_);
    
    // Check if period with same name already exists
    auto it = period_name_index_.find(period.period_name);
    if (it != period_name_index_.end()) {
        // Update existing period
        critical_periods_[it->second] = period;
    } else {
        // Add new period
        size_t index = critical_periods_.size();
        critical_periods_.push_back(period);
        period_name_index_[period.period_name] = index;
    }
    
    return true;
}

bool DevelopmentalConstraints::removeCriticalPeriod(const std::string& period_name) {
    std::unique_lock<std::shared_mutex> lock(constraints_mutex_);
    
    auto it = period_name_index_.find(period_name);
    if (it == period_name_index_.end()) {
        return false;
    }
    
    size_t index = it->second;
    
    // Remove from vector (swap with last element to avoid shifting)
    if (index < critical_periods_.size() - 1) {
        std::swap(critical_periods_[index], critical_periods_.back());
        // Update index for swapped element
        period_name_index_[critical_periods_[index].period_name] = index;
    }
    
    critical_periods_.pop_back();
    period_name_index_.erase(period_name);
    
    return true;
}

std::unique_ptr<CriticalPeriod> DevelopmentalConstraints::getCriticalPeriod(const std::string& period_name) const {
    std::shared_lock<std::shared_mutex> lock(constraints_mutex_);
    
    auto it = period_name_index_.find(period_name);
    if (it != period_name_index_.end() && it->second < critical_periods_.size()) {
        return std::make_unique<CriticalPeriod>(critical_periods_[it->second]);
    }
    
    return nullptr;
}

std::vector<CriticalPeriod> DevelopmentalConstraints::getActiveCriticalPeriods() const {
    std::shared_lock<std::shared_mutex> lock(constraints_mutex_);
    
    std::vector<CriticalPeriod> active_periods;
    std::uint64_t current_age = getCurrentSystemAge();
    
    for (const auto& period : critical_periods_) {
        if (period.isActiveAt(current_age)) {
            active_periods.push_back(period);
        }
    }
    
    return active_periods;
}

bool DevelopmentalConstraints::isInCriticalPeriod(const std::string& period_name) const {
    auto period = getCriticalPeriod(period_name);
    if (!period) {
        return false;
    }
    
    return period->isActiveAt(getCurrentSystemAge());
}

float DevelopmentalConstraints::getCurrentPlasticityMultiplier(const std::string& region_name) const {
    if (!config_.enable_critical_periods) {
        return config_.base_learning_rate;
    }
    
    std::shared_lock<std::shared_mutex> lock(constraints_mutex_);
    
    // Check cached value first
    auto it = current_plasticity_multipliers_.find(region_name);
    if (it != current_plasticity_multipliers_.end()) {
        return it->second;
    }
    
    // Calculate multiplier from active periods
    float total_multiplier = config_.base_learning_rate;
    std::uint64_t current_age = getCurrentSystemAge();
    
    for (const auto& period : critical_periods_) {
        if (period.affectsRegion(region_name) && period.isActiveAt(current_age)) {
            float period_multiplier = period.getPlasticityMultiplierAt(current_age);
            total_multiplier *= period_multiplier;
        }
    }
    
    // Apply age-based decay if enabled
    if (config_.enable_age_dependent_decay) {
        float maturation = getMaturationLevel();
        float decay_factor = 1.0f - (maturation * config_.global_plasticity_decay_rate);
        total_multiplier *= std::max(0.1f, decay_factor);
    }
    
    return total_multiplier;
}

float DevelopmentalConstraints::getCurrentLearningRateMultiplier(const std::string& region_name) const {
    if (!config_.enable_critical_periods) {
        return config_.base_learning_rate;
    }
    
    std::shared_lock<std::shared_mutex> lock(constraints_mutex_);
    
    // Check cached value first
    auto it = current_learning_rate_multipliers_.find(region_name);
    if (it != current_learning_rate_multipliers_.end()) {
        return it->second;
    }
    
    // Calculate multiplier from active periods
    float total_multiplier = config_.base_learning_rate;
    std::uint64_t current_age = getCurrentSystemAge();
    
    for (const auto& period : critical_periods_) {
        if (period.affectsRegion(region_name) && period.isActiveAt(current_age)) {
            float period_multiplier = period.getLearningRateMultiplierAt(current_age);
            total_multiplier *= period_multiplier;
        }
    }
    
    return total_multiplier;
}

float DevelopmentalConstraints::getCurrentConsolidationMultiplier(const std::string& region_name) const {
    if (!config_.enable_critical_periods) {
        return 1.0f;
    }
    
    std::shared_lock<std::shared_mutex> lock(constraints_mutex_);
    
    // Check cached value first
    auto it = current_consolidation_multipliers_.find(region_name);
    if (it != current_consolidation_multipliers_.end()) {
        return it->second;
    }
    
    // Calculate multiplier from active periods
    float total_multiplier = 1.0f;
    std::uint64_t current_age = getCurrentSystemAge();
    
    for (const auto& period : critical_periods_) {
        if (period.affectsRegion(region_name) && period.isActiveAt(current_age)) {
            total_multiplier *= period.consolidation_multiplier;
        }
    }
    
    return total_multiplier;
}

LearningModulation DevelopmentalConstraints::getLearningModulation(
    const std::string& learning_type,
    const std::string& region_name) const {
    
    LearningModulation modulation;
    
    if (!config_.enable_critical_periods) {
        return modulation;  // Default values
    }
    
    std::shared_lock<std::shared_mutex> lock(constraints_mutex_);
    std::uint64_t current_age = getCurrentSystemAge();
    
    for (const auto& period : critical_periods_) {
        if (period.affectsLearningType(learning_type) && 
            (region_name.empty() || period.affectsRegion(region_name)) &&
            period.isActiveAt(current_age)) {
            
            modulation.plasticity_multiplier *= period.getPlasticityMultiplierAt(current_age);
            modulation.learning_rate_multiplier *= period.getLearningRateMultiplierAt(current_age);
            modulation.consolidation_multiplier *= period.consolidation_multiplier;
            
            if (period.type == CriticalPeriod::PeriodType::Enhancement) {
                modulation.is_enhanced = true;
            } else if (period.type == CriticalPeriod::PeriodType::Restriction) {
                modulation.is_restricted = true;
            }
        }
    }
    
    return modulation;
}

void DevelopmentalConstraints::advanceSystemAge(std::uint64_t time_ms) {
    // Update system age (time_ms is absolute time, not relative)
    std::uint64_t birth_time = system_birth_time_.load();
    if (time_ms > birth_time) {
        // System age is time since birth
        // Note: We don't update birth time, just track current time
    }
    
    // Update constraints if enough time has passed
    std::uint64_t last_update = last_update_time_.load();
    if (time_ms - last_update >= config_.update_interval_ms) {
        updateConstraints();
        last_update_time_.store(time_ms);
    }
}

std::uint64_t DevelopmentalConstraints::getCurrentSystemAge() const {
    std::uint64_t current_time = getCurrentTimestamp();
    std::uint64_t birth_time = system_birth_time_.load();
    return current_time - birth_time;
}

float DevelopmentalConstraints::getMaturationLevel() const {
    std::uint64_t age = getCurrentSystemAge();
    return std::min(1.0f, static_cast<float>(age) / static_cast<float>(config_.maturation_time_ms));
}

bool DevelopmentalConstraints::isSystemMature() const {
    return getMaturationLevel() >= 1.0f;
}

void DevelopmentalConstraints::updateConstraints(bool force_update) {
    std::uint64_t current_time = getCurrentTimestamp();
    std::uint64_t last_update = last_update_time_.load();
    
    if (!force_update && (current_time - last_update) < config_.update_interval_ms) {
        return;  // Too soon to update
    }
    
    updateModulationFactors();
    
    // Count active periods
    size_t active_count = 0;
    std::uint64_t current_age = getCurrentSystemAge();
    
    {
        std::shared_lock<std::shared_mutex> lock(constraints_mutex_);
        for (const auto& period : critical_periods_) {
            if (period.isActiveAt(current_age)) {
                ++active_count;
            }
        }
    }
    
    active_periods_count_.store(active_count);
    total_constraint_updates_.fetch_add(1);
    last_update_time_.store(current_time);
}

float DevelopmentalConstraints::applyAgeBasedPlasticityDecay() const {
    if (!config_.enable_age_dependent_decay) {
        return 1.0f;
    }
    
    float maturation = getMaturationLevel();
    float decay_factor = 1.0f - (maturation * config_.global_plasticity_decay_rate);
    
    return std::max(0.1f, decay_factor);
}

float DevelopmentalConstraints::triggerSynapticPruning(const std::string& region_name) {
    if (!config_.enable_pruning_periods) {
        return 0.0f;
    }
    
    std::shared_lock<std::shared_mutex> lock(constraints_mutex_);
    std::uint64_t current_age = getCurrentSystemAge();
    
    float max_pruning_strength = 0.0f;
    
    for (const auto& period : critical_periods_) {
        if (period.type == CriticalPeriod::PeriodType::Pruning &&
            period.affectsRegion(region_name) &&
            period.isActiveAt(current_age)) {
            
            float pruning_strength = period.plasticity_multiplier;
            max_pruning_strength = std::max(max_pruning_strength, pruning_strength);
        }
    }
    
    return max_pruning_strength;
}

bool DevelopmentalConstraints::shouldPruneRegion(const std::string& region_name) const {
    if (!config_.enable_pruning_periods) {
        return false;
    }
    
    std::shared_lock<std::shared_mutex> lock(constraints_mutex_);
    std::uint64_t current_age = getCurrentSystemAge();
    
    for (const auto& period : critical_periods_) {
        if (period.type == CriticalPeriod::PeriodType::Pruning &&
            period.affectsRegion(region_name) &&
            period.isActiveAt(current_age)) {
            return true;
        }
    }
    
    return false;
}

// Static methods for creating standard critical periods

CriticalPeriod DevelopmentalConstraints::createVisualCriticalPeriod(float start_hours,
                                                                   float duration_hours,
                                                                   float enhancement_factor) {
    
    CriticalPeriod period("Visual_Critical_Period",
                         hoursToMilliseconds(start_hours),
                         hoursToMilliseconds(start_hours + duration_hours),
                         enhancement_factor,
                         CriticalPeriod::PeriodType::Enhancement);
    
    period.description = "Critical period for visual system development";
    period.affected_regions = {"VisualCortex", "Visual", "V1", "V2", "V4", "MT"};
    period.affected_modalities = {"Visual"};
    period.learning_types = {"hebbian", "stdp"};
    period.sensitivity_curve = 2.0f;  // Sharp peak
    
    return period;
}

CriticalPeriod DevelopmentalConstraints::createAuditoryCriticalPeriod(float start_hours,
                                                                     float duration_hours,
                                                                     float enhancement_factor) {
    
    CriticalPeriod period("Auditory_Critical_Period",
                         hoursToMilliseconds(start_hours),
                         hoursToMilliseconds(start_hours + duration_hours),
                         enhancement_factor,
                         CriticalPeriod::PeriodType::Enhancement);
    
    period.description = "Critical period for auditory system development";
    period.affected_regions = {"AuditoryCortex", "Audio", "A1", "A2"};
    period.affected_modalities = {"Audio"};
    period.learning_types = {"hebbian", "stdp"};
    period.sensitivity_curve = 1.5f;
    
    return period;
}

CriticalPeriod DevelopmentalConstraints::createLanguageCriticalPeriod(float start_hours,
                                                                     float duration_hours,
                                                                     float enhancement_factor) {
    
    CriticalPeriod period("Language_Critical_Period",
                         hoursToMilliseconds(start_hours),
                         hoursToMilliseconds(start_hours + duration_hours),
                         enhancement_factor,
                         CriticalPeriod::PeriodType::Enhancement);
    
    period.description = "Critical period for language acquisition";
    period.affected_regions = {"LanguageSystem", "Broca", "Wernicke", "Text"};
    period.affected_modalities = {"Text", "Audio"};
    period.learning_types = {"hebbian", "stdp", "assembly"};
    period.sensitivity_curve = 1.0f;  // Gradual curve
    
    return period;
}

CriticalPeriod DevelopmentalConstraints::createMotorCriticalPeriod(float start_hours,
                                                                  float duration_hours,
                                                                  float enhancement_factor) {
    
    CriticalPeriod period("Motor_Critical_Period",
                         hoursToMilliseconds(start_hours),
                         hoursToMilliseconds(start_hours + duration_hours),
                         enhancement_factor,
                         CriticalPeriod::PeriodType::Enhancement);
    
    period.description = "Critical period for motor skill development";
    period.affected_regions = {"MotorCortex", "Motor", "M1", "Cerebellum"};
    period.affected_modalities = {"Motor"};
    period.learning_types = {"hebbian", "stdp", "procedural"};
    period.sensitivity_curve = 1.5f;
    
    return period;
}

CriticalPeriod DevelopmentalConstraints::createPruningPeriod(float start_hours,
                                                            float duration_hours,
                                                            float pruning_strength) {
    
    CriticalPeriod period("Synaptic_Pruning_Period",
                         hoursToMilliseconds(start_hours),
                         hoursToMilliseconds(start_hours + duration_hours),
                         pruning_strength,
                         CriticalPeriod::PeriodType::Pruning);
    
    period.description = "Period of synaptic pruning and refinement";
    period.affected_regions = {};  // Affects all regions
    period.learning_types = {"hebbian", "stdp"};
    period.sensitivity_curve = 1.0f;
    
    return period;
}

size_t DevelopmentalConstraints::initializeStandardDevelopment(bool enable_all_periods) {
    size_t periods_created = 0;
    
    // Visual critical period (early, strong enhancement)
    if (defineCriticalPeriod(createVisualCriticalPeriod(0.5f, 12.0f, 3.0f))) {
        ++periods_created;
    }
    
    // Motor critical period (very early, moderate enhancement)
    if (defineCriticalPeriod(createMotorCriticalPeriod(0.25f, 6.0f, 2.0f))) {
        ++periods_created;
    }
    
    // Auditory critical period (early, strong enhancement)
    if (defineCriticalPeriod(createAuditoryCriticalPeriod(1.0f, 8.0f, 2.5f))) {
        ++periods_created;
    }
    
    // Language critical period (later, moderate enhancement)
    if (defineCriticalPeriod(createLanguageCriticalPeriod(2.0f, 16.0f, 2.0f))) {
        ++periods_created;
    }
    
    if (enable_all_periods) {
        // Synaptic pruning period (late development)
        if (defineCriticalPeriod(createPruningPeriod(18.0f, 4.0f, 0.3f))) {
            ++periods_created;
        }
        
        // Stabilization period (very late)
        CriticalPeriod stabilization("Stabilization_Period",
                                   hoursToMilliseconds(22.0f),
                                   hoursToMilliseconds(26.0f),
                                   0.5f,
                                   CriticalPeriod::PeriodType::Stabilization);
        stabilization.description = "Period of connection stabilization";
        
        if (defineCriticalPeriod(stabilization)) {
            ++periods_created;
        }
    }
    
    return periods_created;
}

DevelopmentalConstraints::Statistics DevelopmentalConstraints::getStatistics() const {
    std::shared_lock<std::shared_mutex> lock(constraints_mutex_);
    
    Statistics stats;
    stats.system_age_ms = getCurrentSystemAge();
    stats.maturation_level = getMaturationLevel();
    stats.total_period_activations = total_period_activations_.load();
    stats.total_constraint_updates = total_constraint_updates_.load();
    stats.active_periods_count = active_periods_count_.load();
    stats.total_periods_defined = critical_periods_.size();
    stats.system_mature = isSystemMature();
    
    // Calculate global multipliers
    stats.current_global_plasticity_multiplier = config_.base_learning_rate;
    stats.current_global_learning_rate_multiplier = config_.base_learning_rate;
    
    if (config_.enable_age_dependent_decay) {
        float decay_factor = applyAgeBasedPlasticityDecay();
        stats.current_global_plasticity_multiplier *= decay_factor;
        stats.current_global_learning_rate_multiplier *= decay_factor;
    }
    
    // Get active period names
    std::uint64_t current_age = getCurrentSystemAge();
    for (const auto& period : critical_periods_) {
        if (period.isActiveAt(current_age)) {
            stats.active_period_names.push_back(period.period_name);
        }
    }
    
    return stats;
}

void DevelopmentalConstraints::setConfig(const Config& new_config) {
    std::unique_lock<std::shared_mutex> lock(constraints_mutex_);
    config_ = new_config;
}

void DevelopmentalConstraints::clearAllPeriods() {
    std::unique_lock<std::shared_mutex> lock(constraints_mutex_);
    critical_periods_.clear();
    period_name_index_.clear();
    current_plasticity_multipliers_.clear();
    current_learning_rate_multipliers_.clear();
    current_consolidation_multipliers_.clear();
}

void DevelopmentalConstraints::resetSystemAge() {
    system_birth_time_.store(getCurrentTimestamp());
    last_update_time_.store(getCurrentTimestamp());
    
    // Reset statistics
    total_period_activations_.store(0);
    total_constraint_updates_.store(0);
    active_periods_count_.store(0);
}

bool DevelopmentalConstraints::isOperational() const {
    return true;  // Simple operational check
}

// Private Methods

float DevelopmentalConstraints::calculateModulationCurve(std::uint64_t current_time,
                                                       std::uint64_t start_time,
                                                       std::uint64_t end_time,
                                                       std::uint64_t peak_time,
                                                       float curve_shape) const {
    
    if (current_time < start_time || current_time > end_time) {
        return 0.0f;
    }
    
    // Normalize time to [0,1]
    float normalized_time = static_cast<float>(current_time - start_time) / 
                           static_cast<float>(end_time - start_time);
    
    float normalized_peak = static_cast<float>(peak_time - start_time) / 
                           static_cast<float>(end_time - start_time);
    
    // Calculate distance from peak
    float distance_from_peak = std::abs(normalized_time - normalized_peak);
    
    // Apply curve shape (Gaussian-like)
    return std::exp(-distance_from_peak * distance_from_peak * curve_shape);
}

void DevelopmentalConstraints::updateModulationFactors() {
    std::unique_lock<std::shared_mutex> lock(constraints_mutex_);
    
    // Clear cached values
    current_plasticity_multipliers_.clear();
    current_learning_rate_multipliers_.clear();
    current_consolidation_multipliers_.clear();
    
    // Recalculate for all regions mentioned in periods
    std::unordered_set<std::string> all_regions;
    for (const auto& period : critical_periods_) {
        for (const auto& region : period.affected_regions) {
            all_regions.insert(region);
        }
    }
    
    // Calculate multipliers for each region
    std::uint64_t current_age = getCurrentSystemAge();
    for (const auto& region : all_regions) {
        float plasticity_mult = config_.base_learning_rate;
        float learning_rate_mult = config_.base_learning_rate;
        float consolidation_mult = 1.0f;
        
        for (const auto& period : critical_periods_) {
            if (period.affectsRegion(region) && period.isActiveAt(current_age)) {
                plasticity_mult *= period.getPlasticityMultiplierAt(current_age);
                learning_rate_mult *= period.getLearningRateMultiplierAt(current_age);
                consolidation_mult *= period.consolidation_multiplier;
            }
        }
        
        // Apply age-based decay
        if (config_.enable_age_dependent_decay) {
            float decay_factor = applyAgeBasedPlasticityDecay();
            plasticity_mult *= decay_factor;
            learning_rate_mult *= decay_factor;
        }
        
        current_plasticity_multipliers_[region] = plasticity_mult;
        current_learning_rate_multipliers_[region] = learning_rate_mult;
        current_consolidation_multipliers_[region] = consolidation_mult;
    }
}

std::uint64_t DevelopmentalConstraints::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

std::uint64_t DevelopmentalConstraints::hoursToMilliseconds(float hours) {
    return static_cast<std::uint64_t>(hours * 60.0f * 60.0f * 1000.0f);
}

bool DevelopmentalConstraints::validateCriticalPeriod(const CriticalPeriod& period) const {
    // Basic validation
    return !period.period_name.empty() && 
           period.end_time_ms > period.start_time_ms &&
           period.plasticity_multiplier >= 0.0f &&
           period.learning_rate_multiplier >= 0.0f &&
           period.consolidation_multiplier >= 0.0f;
}

} // namespace Memory
} // namespace NeuroForge