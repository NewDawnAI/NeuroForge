#include "core/LanguageSystem.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <filesystem>
#include <map>
#include <unordered_map>
#include <iostream>

namespace NeuroForge {
namespace Core {

// TokenTrajectoryLogger Implementation
LanguageSystem::TokenTrajectoryLogger::TokenTrajectoryLogger(const std::string& log_dir, std::size_t snapshot_interval)
    : log_directory_(log_dir), snapshot_interval_(snapshot_interval), current_step_(0) {
    std::filesystem::create_directories(log_directory_);
}

void LanguageSystem::TokenTrajectoryLogger::captureSnapshot(const LanguageSystem& lang_system, std::size_t token_id) {
    if (current_step_ % snapshot_interval_ != 0) {
        current_step_++;
        return;
    }
    
    // Get token information
    const auto* token = lang_system.getToken(token_id);
    if (!token) {
        return; // Invalid token ID
    }

    // Get current statistics
    auto stats = lang_system.getStatistics();
    
    // Create snapshot
    TokenAssociationSnapshot snapshot;
    snapshot.timestamp = std::chrono::steady_clock::now();
    snapshot.token_id = token_id;
    snapshot.symbol = token->symbol;
    snapshot.activation_strength = token->activation_strength;
    snapshot.usage_count = token->usage_count;
    snapshot.embedding = token->embedding;
    
    // Calculate cross-modal strength
    snapshot.cross_modal_strength = calculateCrossModalStrength(lang_system, token_id);
    
    // Get associated tokens (simplified - could be enhanced)
    snapshot.associated_tokens = {}; // TODO: Implement token association analysis
    
    // Calculate cluster stability using simple history-based statistics
    // Tokens that appear frequently with consistent activation are treated as more stable.
    float stability = 0.2f;
    if (!trajectory_log_.empty()) {
        std::vector<const TokenAssociationSnapshot*> history;
        history.reserve(trajectory_log_.size());
        for (const auto& past : trajectory_log_) {
            if (past.symbol == snapshot.symbol) {
                history.push_back(&past);
            }
        }

        if (!history.empty()) {
            float sum_activation = 0.0f;
            for (const auto* h : history) {
                sum_activation += h->activation_strength;
            }
            const float count = static_cast<float>(history.size());
            const float mean_activation = sum_activation / std::max(1.0f, count);

            float sum_abs_dev = 0.0f;
            for (const auto* h : history) {
                sum_abs_dev += std::fabs(h->activation_strength - mean_activation);
            }
            const float mean_abs_dev = sum_abs_dev / std::max(1.0f, count);

            const float usage_factor = count / (count + 5.0f);
            const float variability_factor = 1.0f - std::min(1.0f, mean_abs_dev);
            stability = 0.2f + 0.5f * usage_factor + 0.3f * variability_factor;
            stability = std::clamp(stability, 0.0f, 1.0f);
        }
    }
    snapshot.cluster_stability = stability;
    
    // Record current developmental stage
    snapshot.stage_at_snapshot = stats.current_stage;
    
    // Store snapshot
    trajectory_log_.push_back(snapshot);
    current_step_++;
}

float LanguageSystem::TokenTrajectoryLogger::calculateCrossModalStrength(const LanguageSystem& lang_system, std::size_t token_id) {
    // Implementation for calculating cross-modal binding strength
    auto stats = lang_system.getStatistics();
    
    // Simple heuristic: combine visual-linguistic and auditory-linguistic associations
    float visual_strength = 0.0f;
    float auditory_strength = 0.0f;
    
    // Get cross-modal associations from language system
    auto cross_modal_data = lang_system.getCrossModalAssociations(token_id);
    if (!cross_modal_data.empty()) {
        for (const auto& association : cross_modal_data) {
            if (association.modality == "vision") {
                visual_strength = association.association_strength;
            } else if (association.modality == "audio") {
                auditory_strength = association.association_strength;
            }
        }
    }
    
    return (visual_strength + auditory_strength) / 2.0f;
}

void LanguageSystem::TokenTrajectoryLogger::writeTrajectoryLog() {
    std::string log_path = log_directory_ + "/token_trajectory_log.csv";
    std::ofstream log_file(log_path);
    
    if (!log_file.is_open()) {
        return;
    }
    
    // Write CSV header
    log_file << "timestamp,token_id,symbol,activation_strength,usage_count,cluster_stability,cross_modal_strength,stage,associated_tokens\n";
    
    for (const auto& snapshot : trajectory_log_) {
        // Convert steady_clock timestamp to system_clock for time_t conversion
        auto now_steady = std::chrono::steady_clock::now();
        auto now_system = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::system_clock::duration>(
            snapshot.timestamp - now_steady);
        auto system_time = now_system + elapsed;
        auto time_t = std::chrono::system_clock::to_time_t(system_time);
        
        log_file << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << ","
                 << snapshot.token_id << ",\""
                 << snapshot.symbol << "\","
                 << snapshot.activation_strength << ","
                 << snapshot.usage_count << ","
                 << snapshot.cluster_stability << ","
                 << snapshot.cross_modal_strength << ","
                 << static_cast<int>(snapshot.stage_at_snapshot) << ",\"";
        
        // Write associated tokens as semicolon-separated list
        for (size_t i = 0; i < snapshot.associated_tokens.size(); ++i) {
            if (i > 0) log_file << ";";
            log_file << snapshot.associated_tokens[i];
        }
        log_file << "\"\n";
    }
    
    log_file.close();
    
    // Also write cluster evolution data
    writeClusterEvolutionLog();
}

void LanguageSystem::TokenTrajectoryLogger::writeClusterEvolutionLog() {
    std::string cluster_path = log_directory_ + "/cluster_evolution.csv";
    std::ofstream cluster_file(cluster_path);
    
    if (!cluster_file.is_open()) {
        return;
    }
    
    // Write CSV header
    cluster_file << "formation_step,cluster_name,member_count,cohesion_score,is_proto_word,members\n";
    
    // Generate cluster evolution data based on trajectory snapshots
    std::map<std::string, std::vector<std::string>> clusters;
    std::map<std::string, float> cluster_cohesion;
    std::map<std::string, int> cluster_formation_step;
    
    int step = 0;
    for (const auto& snapshot : trajectory_log_) {
        step++;
        
        // Group tokens by similar symbols (simplified clustering)
        std::string cluster_key;
        if (snapshot.symbol.length() >= 2) {
            cluster_key = snapshot.symbol.substr(0, 2) + "_cluster";
        } else {
            cluster_key = snapshot.symbol + "_cluster";
        }
        
        // Add token to cluster if not already present
        auto& cluster_members = clusters[cluster_key];
        if (std::find(cluster_members.begin(), cluster_members.end(), snapshot.symbol) == cluster_members.end()) {
            cluster_members.push_back(snapshot.symbol);
            cluster_formation_step[cluster_key] = step;
        }
        
        // Update cluster cohesion (average of member stabilities)
        cluster_cohesion[cluster_key] = (cluster_cohesion[cluster_key] + snapshot.cluster_stability) / 2.0f;
    }
    
    // Write cluster data
    for (const auto& cluster_pair : clusters) {
        const std::string& cluster_name = cluster_pair.first;
        const std::vector<std::string>& members = cluster_pair.second;
        
        if (members.size() >= 2) { // Only write clusters with multiple members
            bool is_proto_word = (cluster_cohesion[cluster_name] > 0.6f && members.size() >= 2);
            
            cluster_file << cluster_formation_step[cluster_name] << ","
                        << cluster_name << ","
                        << members.size() << ","
                        << std::fixed << std::setprecision(2) << cluster_cohesion[cluster_name] << ","
                        << (is_proto_word ? "true" : "false") << ",\"";
            
            // Write members as semicolon-separated list
            for (size_t i = 0; i < members.size(); ++i) {
                if (i > 0) cluster_file << ";";
                cluster_file << members[i];
            }
            cluster_file << "\"\n";
        }
    }
    
    cluster_file.close();
}

std::string LanguageSystem::TokenTrajectoryLogger::generateDevelopmentalReport(const LanguageSystem& lang_system) {
    std::ostringstream report;
    
    report << "# Language Development Trajectory Report\n\n";
    report << "**Generated**: " << std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count() << " seconds since epoch\n\n";
    
    report << "## Current System State\n\n";
    auto stats = lang_system.getStatistics();
    report << "- **Current Stage**: " << lang_system.stageToString(lang_system.getCurrentStage()) << "\n";
    report << "- **Total Tokens Generated**: " << stats.total_tokens_generated << "\n";
    report << "- **Active Vocabulary Size**: " << stats.active_vocabulary_size << "\n";
    report << "- **Total Vocabulary Size**: " << stats.total_vocabulary_size << "\n";
    report << "- **Successful Mimicry Attempts**: " << stats.successful_mimicry_attempts << "\n";
    report << "- **Grounding Associations Formed**: " << stats.grounding_associations_formed << "\n\n";
    
    report << "## Trajectory Analysis\n\n";
    analyzeTrajectoryProgression(report, lang_system);
    analyzeCrossModalBinding(report, lang_system);
    generateStagePredictions(report, lang_system);
    
    return report.str();
}

void LanguageSystem::TokenTrajectoryLogger::analyzeTrajectoryProgression(std::ostringstream& report, const LanguageSystem& language_system) {
    if (trajectory_log_.empty()) {
        report << "## Trajectory Analysis\n\nNo trajectory data available.\n\n";
        return;
    }
    
    report << "## Trajectory Analysis\n\n";
    
    // Calculate progression metrics
    std::map<LanguageSystem::DevelopmentalStage, std::size_t> stage_counts;
    std::map<LanguageSystem::DevelopmentalStage, float> avg_activation_by_stage;
    
    for (const auto& snapshot : trajectory_log_) {
        stage_counts[snapshot.stage_at_snapshot]++;
        avg_activation_by_stage[snapshot.stage_at_snapshot] += snapshot.activation_strength;
    }
    
    report << "### Stage Distribution\n\n";
    for (const auto& stage_count_pair : stage_counts) {
        const auto& stage = stage_count_pair.first;
        const auto& count = stage_count_pair.second;
        float avg_activation = avg_activation_by_stage[stage] / count;
        
        report << "- **" << language_system.stageToString(stage) << "**: " 
               << count << " snapshots, " 
               << std::fixed << std::setprecision(3) << avg_activation 
               << " avg activation\n";
    }
    
    report << "\n";
}

void LanguageSystem::TokenTrajectoryLogger::analyzeCrossModalBinding(std::ostringstream& report, const LanguageSystem& language_system) {
    if (trajectory_log_.empty()) {
        report << "## Cross-Modal Binding Analysis\n\nNo trajectory data available.\n\n";
        return;
    }
    
    report << "## Cross-Modal Binding Analysis\n\n";
    
    // Calculate average cross-modal strength over time
    std::map<LanguageSystem::DevelopmentalStage, std::vector<float>> cross_modal_by_stage;
    
    for (const auto& snapshot : trajectory_log_) {
        cross_modal_by_stage[snapshot.stage_at_snapshot].push_back(snapshot.cross_modal_strength);
    }
    
    for (const auto& stage_strengths_pair : cross_modal_by_stage) {
        const auto& stage = stage_strengths_pair.first;
        const auto& strengths = stage_strengths_pair.second;
        if (strengths.empty()) continue;
        
        float avg_strength = std::accumulate(strengths.begin(), strengths.end(), 0.0f) / strengths.size();
        
        report << "- **" << language_system.stageToString(stage) << "**: " 
               << std::fixed << std::setprecision(3) << avg_strength << " average binding strength\n";
    }
    
    report << "\n";
}

void LanguageSystem::TokenTrajectoryLogger::generateStagePredictions(std::ostringstream& report, const LanguageSystem& language_system) {
    auto stats = language_system.getStatistics();
    auto current_stage = language_system.getCurrentStage();
    
    report << "### Stage Predictions\n\n";
    report << "Current Stage: " << language_system.stageToString(current_stage) << "\n\n";
    
    if (current_stage == LanguageSystem::DevelopmentalStage::Chaos) {
        report << "**Transition to Babbling Stage Expected When**:\n";
        report << "- Vocabulary size reaches 10+ stable tokens\n";
        report << "- Cluster stability scores > 0.5\n";
        report << "- Proto-word formations detected\n";
        report << "- Prosodic salience tests pass consistently\n\n";
        
        report << "**Current Progress**:\n";
        report << "- Vocabulary: " << stats.total_vocabulary_size << "/10 tokens\n";
        report << "- Stability: " << std::fixed << std::setprecision(3) << stats.average_cluster_stability << "/0.5\n\n";
    }
    // Add more stage-specific predictions as needed
}

// LanguageSystem trajectory tracking methods implementation
void LanguageSystem::enableTrajectoryTracking(const std::string& log_directory) {
    // Initialize the trajectory logger if not already done
    if (!trajectory_logger_) {
        trajectory_logger_ = std::make_unique<TokenTrajectoryLogger>(log_directory);
    }
    
    std::cout << "📊 Full trajectory tracking enabled in: " << log_directory << std::endl;
    std::cout << "   Data will be written to CSV files for analysis" << std::endl;
}

void LanguageSystem::captureTrajectorySnapshot() {
    if (!trajectory_logger_) {
        std::cout << "⚠️ Trajectory tracking not enabled. Call enableTrajectoryTracking() first." << std::endl;
        return;
    }
    
    // Capture snapshots for all active tokens
    auto stats = getStatistics();
    auto current_stage = getCurrentStage();
    
    static int snapshot_count = 0;
    snapshot_count++;
    
    // Capture detailed snapshots for analysis
    for (std::size_t token_id = 0; token_id < stats.total_vocabulary_size && token_id < 100; ++token_id) {
        const auto* token = getToken(token_id);
        if (token && token->usage_count > 0) {
            trajectory_logger_->captureSnapshot(*this, token_id);
        }
    }
    
    if (snapshot_count % 10 == 0) {
        std::cout << "📸 Snapshot " << snapshot_count 
                  << " - Stage: " << stageToString(current_stage)
                  << ", Vocab: " << stats.active_vocabulary_size
                  << ", Generated: " << stats.total_tokens_generated << std::endl;
        
        // Write trajectory data to files
        trajectory_logger_->writeTrajectoryLog();
    }
}

void LanguageSystem::generateDevelopmentalReport() {
    if (!trajectory_logger_) {
        std::cout << "⚠️ Trajectory tracking not enabled. Call enableTrajectoryTracking() first." << std::endl;
        return;
    }
    
    std::cout << "\n🧠 NeuroForge Comprehensive Developmental Report" << std::endl;
    std::cout << "=================================================" << std::endl;
    
    // Generate and display the full report
    std::string report = trajectory_logger_->generateDevelopmentalReport(*this);
    std::cout << report << std::endl;
    
    // Also write the report to a file
    std::string report_path = trajectory_logger_->log_directory_ + "/developmental_report.md";
    std::ofstream report_file(report_path);
    if (report_file.is_open()) {
        report_file << report;
        report_file.close();
        std::cout << "📄 Report saved to: " << report_path << std::endl;
    }
}

} // namespace Core
} // namespace NeuroForge
