#include "core/LanguageSystem.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace NeuroForge {
namespace Core {

// Simple trajectory tracking implementation
void LanguageSystem::enableTrajectoryTracking(const std::string& log_directory) {
    // Create log directory if it doesn't exist
    #ifdef _WIN32
    CreateDirectoryA(log_directory.c_str(), NULL);
    #endif
    
    std::cout << "📊 Trajectory tracking enabled in: " << log_directory << std::endl;
}

void LanguageSystem::captureTrajectorySnapshot() {
    // Simple snapshot capture - just log current state
    auto stats = getStatistics();
    auto current_stage = getCurrentStage();
    
    static int snapshot_count = 0;
    snapshot_count++;
    
    if (snapshot_count % 10 == 0) {
        std::cout << "📸 Snapshot " << snapshot_count 
                  << " - Stage: " << static_cast<int>(current_stage)
                  << ", Vocab: " << stats.active_vocabulary_size
                  << ", Generated: " << stats.total_tokens_generated << std::endl;
    }
}

void LanguageSystem::generateDevelopmentalReport() {
    auto stats = getStatistics();
    auto current_stage = getCurrentStage();
    
    std::cout << "\n🧠 NeuroForge Developmental Report" << std::endl;
    std::cout << "=================================" << std::endl;
    std::cout << "Current Stage: " << static_cast<int>(current_stage) << std::endl;
    std::cout << "Vocabulary Size: " << stats.active_vocabulary_size << std::endl;
    std::cout << "Total Tokens Generated: " << stats.total_tokens_generated << std::endl;
    std::cout << "Successful Mimicry: " << stats.successful_mimicry_attempts << std::endl;
    std::cout << "Grounding Associations: " << stats.grounding_associations_formed << std::endl;
    std::cout << "Average Activation: " << std::fixed << std::setprecision(3) 
              << stats.average_token_activation << std::endl;
    
    // Calculate developmental progress
    float progress = static_cast<float>(static_cast<int>(current_stage)) / 6.0f;
    std::cout << "Developmental Progress: " << std::fixed << std::setprecision(1) 
              << progress * 100 << "%" << std::endl;
    
    // Milestone achievements
    std::cout << "\n🏅 Milestone Achievements:" << std::endl;
    
    if (stats.total_tokens_generated > 0) {
        std::cout << "✅ Acoustic Babbling: System generating tokens" << std::endl;
    }
    
    if (stats.successful_mimicry_attempts > 0) {
        std::cout << "✅ Caregiver Response: System responding to teacher signals" << std::endl;
    }
    
    if (stats.grounding_associations_formed > 0) {
        std::cout << "✅ Sensory Grounding: Linking tokens to experiences" << std::endl;
    }
    
    if (stats.active_vocabulary_size >= 5) {
        std::cout << "✅ First Vocabulary: 5+ stable tokens achieved" << std::endl;
    }
    
    if (stats.average_token_activation > 0.5f) {
        std::cout << "✅ Strong Activation: High token engagement" << std::endl;
    }
    
    std::cout << "\n📈 Next Steps:" << std::endl;
    
    switch (current_stage) {
        case DevelopmentalStage::Chaos:
            std::cout << "- Continue acoustic babbling with varied patterns" << std::endl;
            std::cout << "- Increase teacher signal exposure" << std::endl;
            std::cout << "- Target: 10+ stable tokens for Babbling stage" << std::endl;
            break;
            
        case DevelopmentalStage::Babbling:
            std::cout << "- Focus on proto-word formation (mama, baba, dada)" << std::endl;
            std::cout << "- Enhance prosodic sensitivity" << std::endl;
            std::cout << "- Target: Consistent mimicry for Mimicry stage" << std::endl;
            break;
            
        case DevelopmentalStage::Mimicry:
            std::cout << "- Develop joint attention capabilities" << std::endl;
            std::cout << "- Strengthen cross-modal associations" << std::endl;
            std::cout << "- Target: Grounded word-object associations" << std::endl;
            break;
            
        default:
            std::cout << "- Continue advanced language development" << std::endl;
            std::cout << "- Explore complex communication patterns" << std::endl;
            break;
    }
    
    std::cout << "\n=================================" << std::endl;
    std::cout << "Report generated successfully! 🎉" << std::endl;
}

} // namespace Core
} // namespace NeuroForge