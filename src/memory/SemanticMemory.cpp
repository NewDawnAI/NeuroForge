#include "memory/SemanticMemory.h"
#include "memory/EnhancedEpisode.h"
#include "memory/EpisodicMemoryManager.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <chrono>
#include <random>
#include <queue>
#include <sstream>

namespace NeuroForge {
namespace Memory {

// ConceptNode Implementation

ConceptNode::ConceptNode(const std::string& concept_label,
                        const std::vector<float>& features,
                        ConceptType concept_type,
                        const std::string& desc)
    : label(concept_label)
    , feature_vector(features)
    , description(desc)
    , type(concept_type)
    , creation_timestamp_ms(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count())
    , last_access_timestamp_ms(creation_timestamp_ms) {
    
    // Set initial consolidation strength based on feature vector magnitude
    if (!features.empty()) {
        float magnitude = 0.0f;
        for (float f : features) {
            magnitude += f * f;
        }
        consolidation_strength = std::min(1.0f, std::sqrt(magnitude) / features.size());
    }
}

float ConceptNode::calculateSimilarity(const ConceptNode& other) const {
    if (feature_vector.empty() || other.feature_vector.empty()) {
        return 0.0f;
    }
    
    // Calculate cosine similarity between feature vectors
    float dot_product = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    size_t min_size = std::min(feature_vector.size(), other.feature_vector.size());
    
    for (size_t i = 0; i < min_size; ++i) {
        dot_product += feature_vector[i] * other.feature_vector[i];
        norm_a += feature_vector[i] * feature_vector[i];
        norm_b += other.feature_vector[i] * other.feature_vector[i];
    }
    
    float similarity = 0.0f;
    if (norm_a > 0.0f && norm_b > 0.0f) {
        similarity = dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
    }
    
    // Type similarity bonus
    if (type == other.type) {
        similarity += 0.1f;  // 10% bonus for same type
    }
    
    // Abstraction level similarity
    float abstraction_diff = std::abs(abstraction_level - other.abstraction_level);
    similarity += (1.0f - abstraction_diff) * 0.05f;  // 5% bonus for similar abstraction
    
    return std::max(0.0f, std::min(1.0f, similarity));
}

std::uint64_t ConceptNode::getAge() const {
    auto current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return static_cast<std::uint64_t>(current_time - creation_timestamp_ms);
}

bool ConceptNode::shouldConsolidate(float consolidation_threshold) const {
    // Consider multiple factors for consolidation
    float consolidation_score = consolidation_strength;
    
    // Access frequency contributes to consolidation
    consolidation_score += std::min(1.0f, access_count / 20.0f) * 0.3f;
    
    // Episodic support contributes
    consolidation_score += episodic_support * 0.2f;
    
    // Relationship count contributes (well-connected concepts are important)
    consolidation_score += std::min(1.0f, related_concepts.size() / 10.0f) * 0.2f;
    
    // Certainty contributes
    consolidation_score += certainty * 0.1f;
    
    return consolidation_score >= consolidation_threshold;
}

void ConceptNode::updateWithEvidence(const std::vector<float>& new_features, float evidence_weight) {
    if (new_features.empty() || feature_vector.empty()) {
        return;
    }
    
    // Update feature vector with weighted average
    size_t min_size = std::min(feature_vector.size(), new_features.size());
    for (size_t i = 0; i < min_size; ++i) {
        feature_vector[i] = (1.0f - evidence_weight) * feature_vector[i] + 
                           evidence_weight * new_features[i];
    }
    
    // Update consolidation strength
    consolidation_strength = std::min(1.0f, consolidation_strength + evidence_weight * 0.1f);
    
    // Update certainty
    certainty = std::min(1.0f, certainty + evidence_weight * 0.05f);
    
    // Update access timestamp
    last_access_timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    ++access_count;
}

void ConceptNode::addRelationship(int concept_id, float strength, bool bidirectional) {
    // Add to related concepts if not already present
    if (std::find(related_concepts.begin(), related_concepts.end(), concept_id) == related_concepts.end()) {
        related_concepts.push_back(concept_id);
    }
    
    // Update relationship strength
    relationship_strengths[concept_id] = strength;
}

// SemanticMemory Implementation

SemanticMemory::SemanticMemory(const Config& config)
    : config_(config) {
    // Initialize with current timestamp
    last_consolidation_time_.store(getCurrentTimestamp());
}

int SemanticMemory::createConcept(const std::string& label,
                                 const std::vector<float>& features,
                                 ConceptNode::ConceptType type,
                                 const std::string& description) {
    
    if (label.empty() || features.empty()) {
        return -1;  // Invalid concept data
    }
    
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    // Check if concept with this label already exists
    auto label_it = label_to_id_.find(label);
    if (label_it != label_to_id_.end()) {
        // Update existing concept with new evidence
        auto concept_it = concept_graph_.find(label_it->second);
        if (concept_it != concept_graph_.end()) {
            concept_it->second.updateWithEvidence(features, 0.2f);
            return label_it->second;
        }
    }
    
    // Create new concept
    int concept_id = next_concept_id_.fetch_add(1);
    ConceptNode new_concept(label, features, type, description);
    
    // Store concept
    concept_graph_[concept_id] = new_concept;
    label_to_id_[label] = concept_id;
    
    // Update indices
    updateIndices(new_concept, concept_id);
    
    // Maintain size limits
    if (concept_graph_.size() > config_.max_concepts) {
        // Remove oldest, weakest concept
        int oldest_id = -1;
        std::uint64_t oldest_time = std::numeric_limits<std::uint64_t>::max();
        float weakest_strength = 1.0f;
        
        for (const auto& [id, node] : concept_graph_) {
            if (node.creation_timestamp_ms < oldest_time && 
                node.consolidation_strength < weakest_strength) {
                oldest_id = id;
                oldest_time = node.creation_timestamp_ms;
                weakest_strength = node.consolidation_strength;
            }
        }
        
        if (oldest_id != -1) {
            auto& concept_to_remove = concept_graph_[oldest_id];
            removeFromIndices(concept_to_remove, oldest_id);
            label_to_id_.erase(concept_to_remove.label);
            concept_graph_.erase(oldest_id);
        }
    }
    
    // Update statistics
    total_concepts_created_.fetch_add(1);
    
    // Note: Automatic consolidation is deferred to avoid deadlock
    // It will be triggered by external consolidation cycles
    
    return concept_id;
}

int SemanticMemory::addConcept(const std::string& label,
                               const std::vector<float>& features) {
    // Wrapper to align with integrator API; defaults to Abstract type
    return createConcept(label, features, ConceptNode::ConceptType::Abstract, "");
}

std::vector<int> SemanticMemory::extractConceptsFromEpisode(const EnhancedEpisode& episode,
                                                          float extraction_threshold) {
    
    if (extraction_threshold < 0.0f) {
        extraction_threshold = config_.concept_creation_threshold;
    }
    
    std::vector<int> extracted_concepts;
    
    // Extract features from different episode components
    std::vector<float> combined_features;
    
    // Combine sensory, action, and substrate features
    combined_features.insert(combined_features.end(), 
                           episode.sensory_state.begin(), episode.sensory_state.end());
    combined_features.insert(combined_features.end(), 
                           episode.action_state.begin(), episode.action_state.end());
    
    // Subsample substrate state to avoid overwhelming feature vector
    if (!episode.substrate_state.empty()) {
        size_t subsample_size = std::min(episode.substrate_state.size(), size_t(50));
        for (size_t i = 0; i < subsample_size; ++i) {
            size_t idx = (i * episode.substrate_state.size()) / subsample_size;
            combined_features.push_back(episode.substrate_state[idx]);
        }
    }
    
    if (combined_features.empty()) {
        return extracted_concepts;
    }
    
    // Check if this represents a novel concept
    auto similar_concepts = findSimilarConcepts(combined_features, 5, extraction_threshold);
    
    if (similar_concepts.empty() || similar_concepts[0].second < extraction_threshold) {
        // Create new concept
        std::string concept_label = generateConceptLabel(combined_features, ConceptNode::ConceptType::Abstract);
        
        if (!episode.context_tag.empty()) {
            concept_label = episode.context_tag + "_" + concept_label;
        }
        
        int concept_id = createConcept(concept_label, combined_features, 
                                     ConceptNode::ConceptType::Abstract, 
                                     "Extracted from episode");
        
        if (concept_id != -1) {
            extracted_concepts.push_back(concept_id);
            
            // Link to episode
            std::lock_guard<std::mutex> lock(concepts_mutex_);
            auto concept_it = concept_graph_.find(concept_id);
            if (concept_it != concept_graph_.end()) {
                // Note: In a full implementation, we'd need episode IDs
                concept_it->second.episodic_support += 0.2f;
                concept_it->second.consolidation_strength += episode.emotional_weight * 0.1f;
            }
        }
    } else {
        // Update existing similar concept
        for (const auto& [candidate, similarity] : similar_concepts) {
            std::lock_guard<std::mutex> lock(concepts_mutex_);
            // Find concept by label (simplified approach)
            auto label_it = label_to_id_.find(candidate.label);
            if (label_it != label_to_id_.end()) {
                auto concept_it = concept_graph_.find(label_it->second);
                if (concept_it != concept_graph_.end()) {
                    concept_it->second.updateWithEvidence(combined_features, similarity * 0.1f);
                    extracted_concepts.push_back(label_it->second);
                }
            }
        }
    }
    
    return extracted_concepts;
}

std::unique_ptr<ConceptNode> SemanticMemory::retrieveConcept(int concept_id) const {
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    auto it = concept_graph_.find(concept_id);
    if (it != concept_graph_.end()) {
        // Update access statistics (const_cast for statistics)
        const_cast<SemanticMemory*>(this)->total_concept_accesses_.fetch_add(1);
        auto& node = const_cast<ConceptNode&>(it->second);
        node.last_access_timestamp_ms = getCurrentTimestamp();
        ++node.access_count;
        
        return std::make_unique<ConceptNode>(it->second);
    }
    
    return nullptr;
}

std::unique_ptr<ConceptNode> SemanticMemory::retrieveConceptByLabel(const std::string& label) const {
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    auto label_it = label_to_id_.find(label);
    if (label_it != label_to_id_.end()) {
        auto it = concept_graph_.find(label_it->second);
        if (it != concept_graph_.end()) {
            // Update access statistics (const_cast for statistics)
            const_cast<SemanticMemory*>(this)->total_concept_accesses_.fetch_add(1);
            auto& node = const_cast<ConceptNode&>(it->second);
            node.last_access_timestamp_ms = getCurrentTimestamp();
            ++node.access_count;
            
            return std::make_unique<ConceptNode>(it->second);
        }
    }
    
    return nullptr;
}

std::vector<std::pair<ConceptNode, float>> SemanticMemory::findSimilarConcepts(
    const std::vector<float>& query_features,
    size_t max_results,
    float similarity_threshold) const {
    
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    std::vector<std::pair<ConceptNode, float>> results;
    
    // Search all concepts
    for (const auto& [concept_id, node] : concept_graph_) {
        float similarity = calculateCosineSimilarity(query_features, node.feature_vector);
        results.emplace_back(node, similarity);
    }
    
    // Sort by similarity (descending)
    std::sort(results.begin(), results.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Limit results
    if (results.size() > max_results) {
        results.resize(max_results);
    }
    
    return results;
}

std::vector<ConceptNode> SemanticMemory::findConceptsByType(ConceptNode::ConceptType type,
                                                          size_t max_results) const {
    
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    std::vector<ConceptNode> results;
    
    // Use type index if available
    auto type_it = type_index_.find(type);
    if (type_it != type_index_.end()) {
        for (int concept_id : type_it->second) {
            auto concept_it = concept_graph_.find(concept_id);
            if (concept_it != concept_graph_.end()) {
                results.push_back(concept_it->second);
                if (results.size() >= max_results) {
                    break;
                }
            }
        }
    } else {
        // Fallback to linear search
        for (const auto& [concept_id, node] : concept_graph_) {
            if (node.type == type) {
                results.push_back(node);
                if (results.size() >= max_results) {
                    break;
                }
            }
        }
    }
    
    return results;
}

bool SemanticMemory::linkConcepts(int concept_id_1, int concept_id_2, 
                                 float strength, bool bidirectional) {
    
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    auto concept_1_it = concept_graph_.find(concept_id_1);
    auto concept_2_it = concept_graph_.find(concept_id_2);
    
    if (concept_1_it == concept_graph_.end() || concept_2_it == concept_graph_.end()) {
        return false;
    }
    
    // Add relationship from concept 1 to concept 2
    concept_1_it->second.addRelationship(concept_id_2, strength, false);
    
    if (bidirectional) {
        // Add relationship from concept 2 to concept 1
        concept_2_it->second.addRelationship(concept_id_1, strength, false);
    }
    
    total_relationships_created_.fetch_add(bidirectional ? 2 : 1);
    
    return true;
}

bool SemanticMemory::createHierarchicalRelationship(int child_id, int parent_id, float strength) {
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    auto child_it = concept_graph_.find(child_id);
    auto parent_it = concept_graph_.find(parent_id);
    
    if (child_it == concept_graph_.end() || parent_it == concept_graph_.end()) {
        return false;
    }
    
    // Add parent-child relationships
    child_it->second.parent_concepts.push_back(parent_id);
    parent_it->second.child_concepts.push_back(child_id);
    
    // Also add as general relationship
    child_it->second.addRelationship(parent_id, strength, false);
    parent_it->second.addRelationship(child_id, strength, false);
    
    return true;
}

std::vector<std::pair<ConceptNode, float>> SemanticMemory::getRelatedConcepts(
    int concept_id,
    size_t max_results,
    float min_strength) const {
    
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    std::vector<std::pair<ConceptNode, float>> results;
    
    auto concept_it = concept_graph_.find(concept_id);
    if (concept_it == concept_graph_.end()) {
        return results;
    }
    
    const auto& node = concept_it->second;
    
    // Get related concepts with their strengths
    for (int related_id : node.related_concepts) {
        auto related_it = concept_graph_.find(related_id);
        if (related_it != concept_graph_.end()) {
            float strength = 0.5f;  // Default strength
            
            auto strength_it = node.relationship_strengths.find(related_id);
            if (strength_it != node.relationship_strengths.end()) {
                strength = strength_it->second;
            }
            
            if (strength >= min_strength) {
                results.emplace_back(related_it->second, strength);
            }
        }
    }
    
    // Sort by relationship strength (descending)
    std::sort(results.begin(), results.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Limit results
    if (results.size() > max_results) {
        results.resize(max_results);
    }
    
    return results;
}

ConceptHierarchy SemanticMemory::getConceptHierarchy(int concept_id, size_t max_depth) const {
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    ConceptHierarchy hierarchy;
    
    auto concept_it = concept_graph_.find(concept_id);
    if (concept_it == concept_graph_.end()) {
        return hierarchy;
    }
    
    const auto& node = concept_it->second;
    
    // Get parents
    for (int parent_id : node.parent_concepts) {
        auto parent_it = concept_graph_.find(parent_id);
        if (parent_it != concept_graph_.end()) {
            hierarchy.parents.push_back(parent_it->second);
        }
    }
    
    // Get children
    for (int child_id : node.child_concepts) {
        auto child_it = concept_graph_.find(child_id);
        if (child_it != concept_graph_.end()) {
            hierarchy.children.push_back(child_it->second);
        }
    }
    
    // Get siblings (concepts with same parents)
    std::unordered_set<int> sibling_ids;
    for (int parent_id : node.parent_concepts) {
        auto parent_it = concept_graph_.find(parent_id);
        if (parent_it != concept_graph_.end()) {
            for (int sibling_id : parent_it->second.child_concepts) {
                if (sibling_id != concept_id) {
                    sibling_ids.insert(sibling_id);
                }
            }
        }
    }
    
    for (int sibling_id : sibling_ids) {
        auto sibling_it = concept_graph_.find(sibling_id);
        if (sibling_it != concept_graph_.end()) {
            hierarchy.siblings.push_back(sibling_it->second);
        }
    }
    
    return hierarchy;
}

size_t SemanticMemory::consolidateFromEpisodicMemory(const EpisodicMemoryManager& episodic_manager,
                                                   size_t max_episodes) {
    
    // Get recent episodes from episodic memory
    auto stats = episodic_manager.getStatistics();
    size_t episodes_to_process = std::min(max_episodes, static_cast<size_t>(stats.recent_episodes_count));
    
    size_t concepts_created = 0;
    
    // Note: This is a simplified implementation
    // In a full implementation, we'd need access to actual episodes
    
    return concepts_created;
}

size_t SemanticMemory::mergeSimilarConcepts(float merge_threshold) {
    if (!config_.enable_concept_merging) {
        return 0;
    }
    
    if (merge_threshold < 0.0f) {
        merge_threshold = config_.concept_merge_threshold;
    }
    
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    size_t merged_count = 0;
    std::vector<int> concepts_to_remove;
    
    // Find similar concepts to merge
    std::vector<int> concept_ids;
    for (const auto& [id, node] : concept_graph_) {
        concept_ids.push_back(id);
    }
    
    for (size_t i = 0; i < concept_ids.size(); ++i) {
        for (size_t j = i + 1; j < concept_ids.size(); ++j) {
            int id_1 = concept_ids[i], id_2 = concept_ids[j];
            
            auto concept_1_it = concept_graph_.find(id_1);
            auto concept_2_it = concept_graph_.find(id_2);
            
            if (concept_1_it == concept_graph_.end() || concept_2_it == concept_graph_.end()) {
                continue;
            }
            
            float similarity = concept_1_it->second.calculateSimilarity(concept_2_it->second);
            
            if (similarity >= merge_threshold) {
                // Merge concept 2 into concept 1
                auto& concept_1 = concept_1_it->second;
                const auto& concept_2 = concept_2_it->second;
                
                // Merge feature vectors (weighted average)
                float weight_1 = concept_1.consolidation_strength;
                float weight_2 = concept_2.consolidation_strength;
                float total_weight = weight_1 + weight_2;
                
                if (total_weight > 0.0f) {
                    for (size_t k = 0; k < std::min(concept_1.feature_vector.size(), 
                                                   concept_2.feature_vector.size()); ++k) {
                        concept_1.feature_vector[k] = (weight_1 * concept_1.feature_vector[k] + 
                                                      weight_2 * concept_2.feature_vector[k]) / total_weight;
                    }
                }
                
                // Merge other properties
                concept_1.consolidation_strength = std::max(concept_1.consolidation_strength, 
                                                          concept_2.consolidation_strength);
                concept_1.access_count += concept_2.access_count;
                concept_1.episodic_support = std::max(concept_1.episodic_support, 
                                                     concept_2.episodic_support);
                
                // Merge relationships
                for (int related_id : concept_2.related_concepts) {
                    if (std::find(concept_1.related_concepts.begin(), 
                                 concept_1.related_concepts.end(), related_id) == 
                        concept_1.related_concepts.end()) {
                        concept_1.related_concepts.push_back(related_id);
                    }
                }
                
                // Mark concept 2 for removal
                concepts_to_remove.push_back(id_2);
                ++merged_count;
            }
        }
    }
    
    // Remove merged concepts
    for (int id : concepts_to_remove) {
        auto concept_it = concept_graph_.find(id);
        if (concept_it != concept_graph_.end()) {
            removeFromIndices(concept_it->second, id);
            label_to_id_.erase(concept_it->second.label);
            concept_graph_.erase(id);
        }
    }
    
    concepts_merged_.fetch_add(merged_count);
    
    return merged_count;
}

size_t SemanticMemory::formHierarchicalRelationships(float hierarchy_threshold) {
    if (!config_.enable_hierarchy_formation) {
        return 0;
    }
    
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    size_t relationships_created = 0;
    
    // Simple hierarchy formation based on abstraction levels and similarity
    std::vector<int> concept_ids;
    for (const auto& [id, node] : concept_graph_) {
        concept_ids.push_back(id);
    }
    
    for (size_t i = 0; i < concept_ids.size(); ++i) {
        for (size_t j = 0; j < concept_ids.size(); ++j) {
            if (i == j) continue;
            
            int id_1 = concept_ids[i], id_2 = concept_ids[j];
            
            auto concept_1_it = concept_graph_.find(id_1);
            auto concept_2_it = concept_graph_.find(id_2);
            
            if (concept_1_it == concept_graph_.end() || concept_2_it == concept_graph_.end()) {
                continue;
            }
            
            const auto& concept_1 = concept_1_it->second;
            const auto& concept_2 = concept_2_it->second;
            
            // Check if concept_1 should be parent of concept_2
            if (concept_1.abstraction_level > concept_2.abstraction_level + 0.2f) {
                float similarity = concept_1.calculateSimilarity(concept_2);
                
                if (similarity >= hierarchy_threshold) {
                    // Create parent-child relationship directly (avoid deadlock)
                    // Add parent-child relationships
                    concept_2_it->second.parent_concepts.push_back(id_1);
                    concept_1_it->second.child_concepts.push_back(id_2);
                    
                    // Also add as general relationship
                    concept_2_it->second.addRelationship(id_1, similarity, false);
                    concept_1_it->second.addRelationship(id_2, similarity, false);
                    
                    ++relationships_created;
                }
            }
        }
    }
    
    return relationships_created;
}

size_t SemanticMemory::applyConceptDecay(float decay_factor) {
    if (decay_factor < 0.0f) {
        decay_factor = config_.decay_rate;
    }
    
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    size_t affected_count = 0;
    
    for (auto& [concept_id, node] : concept_graph_) {
        node.consolidation_strength *= (1.0f - decay_factor);
        node.episodic_support *= (1.0f - decay_factor * 0.5f);
        node.certainty *= (1.0f - decay_factor * 0.1f);
        ++affected_count;
    }
    
    return affected_count;
}

size_t SemanticMemory::pruneWeakConcepts(float min_consolidation_strength,
                                       std::uint32_t min_access_count) {
    
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    std::vector<int> concepts_to_remove;
    
    for (const auto& [concept_id, node] : concept_graph_) {
        if (node.consolidation_strength < min_consolidation_strength &&
            node.access_count < min_access_count) {
            concepts_to_remove.push_back(concept_id);
        }
    }
    
    // Remove weak concepts
    for (int id : concepts_to_remove) {
        auto concept_it = concept_graph_.find(id);
        if (concept_it != concept_graph_.end()) {
            removeFromIndices(concept_it->second, id);
            label_to_id_.erase(concept_it->second.label);
            concept_graph_.erase(id);
        }
    }
    
    return concepts_to_remove.size();
}

std::vector<ConceptNode> SemanticMemory::queryKnowledgeGraph(
    int query_concept,
    const std::vector<std::string>& relationship_types,
    size_t max_depth) const {
    
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    std::vector<ConceptNode> results;
    std::unordered_set<int> visited;
    std::queue<std::pair<int, size_t>> to_visit;  // concept_id, depth
    
    to_visit.push({query_concept, 0});
    visited.insert(query_concept);
    
    while (!to_visit.empty()) {
        auto [current_id, depth] = to_visit.front();
        to_visit.pop();
        
        if (depth >= max_depth) {
            continue;
        }
        
        auto concept_it = concept_graph_.find(current_id);
        if (concept_it == concept_graph_.end()) {
            continue;
        }
        
        const auto& node = concept_it->second;
        results.push_back(node);
        
        // Follow relationships
        for (int related_id : node.related_concepts) {
            if (visited.find(related_id) == visited.end()) {
                visited.insert(related_id);
                to_visit.push({related_id, depth + 1});
            }
        }
    }
    
    return results;
}

std::vector<int> SemanticMemory::findConceptualPath(int start_concept_id, int end_concept_id,
                                                  size_t max_path_length) const {
    
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    // Simple breadth-first search for shortest path
    std::queue<std::vector<int>> paths;
    std::unordered_set<int> visited;
    
    paths.push({start_concept_id});
    visited.insert(start_concept_id);
    
    while (!paths.empty()) {
        auto current_path = paths.front();
        paths.pop();
        
        if (current_path.size() > max_path_length) {
            continue;
        }
        
        int current_concept = current_path.back();
        
        if (current_concept == end_concept_id) {
            return current_path;  // Found path
        }
        
        auto concept_it = concept_graph_.find(current_concept);
        if (concept_it == concept_graph_.end()) {
            continue;
        }
        
        // Explore related concepts
        for (int related_id : concept_it->second.related_concepts) {
            if (visited.find(related_id) == visited.end()) {
                visited.insert(related_id);
                auto new_path = current_path;
                new_path.push_back(related_id);
                paths.push(new_path);
            }
        }
    }
    
    return {};  // No path found
}

std::vector<std::pair<ConceptNode, size_t>> SemanticMemory::getConceptNeighborhood(
    int concept_id, size_t radius) const {
    
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    std::vector<std::pair<ConceptNode, size_t>> results;
    std::unordered_map<int, size_t> distances;
    std::queue<std::pair<int, size_t>> to_visit;
    
    to_visit.push({concept_id, 0});
    distances[concept_id] = 0;
    
    while (!to_visit.empty()) {
        auto [current_id, distance] = to_visit.front();
        to_visit.pop();
        
        if (distance > radius) {
            continue;
        }
        
        auto concept_it = concept_graph_.find(current_id);
        if (concept_it == concept_graph_.end()) {
            continue;
        }
        
        results.emplace_back(concept_it->second, distance);
        
        // Explore neighbors
        for (int related_id : concept_it->second.related_concepts) {
            if (distances.find(related_id) == distances.end() || 
                distances[related_id] > distance + 1) {
                distances[related_id] = distance + 1;
                to_visit.push({related_id, distance + 1});
            }
        }
    }
    
    return results;
}

SemanticStatistics SemanticMemory::getStatistics() const {
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    
    SemanticStatistics stats;
    stats.total_concepts_created = total_concepts_created_.load();
    stats.active_concepts_count = concept_graph_.size();
    stats.total_consolidations = total_consolidations_.load();
    stats.total_concept_accesses = total_concept_accesses_.load();
    stats.concepts_merged = concepts_merged_.load();
    
    // Calculate relationship count
    for (const auto& [concept_id, node] : concept_graph_) {
        stats.total_relationships += node.related_concepts.size();
    }
    
    // Calculate averages
    if (!concept_graph_.empty()) {
        std::uint64_t total_age = 0;
        float total_consolidation = 0.0f;
        
        for (const auto& [concept_id, node] : concept_graph_) {
            total_age += node.getAge();
            total_consolidation += node.consolidation_strength;
        }
        
        stats.average_concept_age_ms = static_cast<float>(total_age) / concept_graph_.size();
        stats.average_consolidation_strength = total_consolidation / concept_graph_.size();
        stats.average_relationships_per_concept = static_cast<float>(stats.total_relationships) / concept_graph_.size();
    }
    
    stats.concept_types_count = type_index_.size();
    stats.consolidation_active = shouldConsolidate();
    
    return stats;
}

void SemanticMemory::setConfig(const Config& new_config) {
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    config_ = new_config;
}

void SemanticMemory::clearAllConcepts() {
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    concept_graph_.clear();
    label_to_id_.clear();
    type_index_.clear();
    keyword_index_.clear();
    
    // Reset statistics
    total_concepts_created_.store(0);
    total_consolidations_.store(0);
    total_concept_accesses_.store(0);
    total_relationships_created_.store(0);
    concepts_merged_.store(0);
}

size_t SemanticMemory::getTotalConceptCount() const {
    std::lock_guard<std::mutex> lock(concepts_mutex_);
    return concept_graph_.size();
}

bool SemanticMemory::isOperational() const {
    return true;  // Simple operational check
}

// Private Methods

float SemanticMemory::calculateCosineSimilarity(const std::vector<float>& vec_a,
                                              const std::vector<float>& vec_b) const {
    
    if (vec_a.empty() || vec_b.empty()) {
        return 0.0f;
    }
    
    float dot_product = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    size_t min_size = std::min(vec_a.size(), vec_b.size());
    
    for (size_t i = 0; i < min_size; ++i) {
        dot_product += vec_a[i] * vec_b[i];
        norm_a += vec_a[i] * vec_a[i];
        norm_b += vec_b[i] * vec_b[i];
    }
    
    if (norm_a > 0.0f && norm_b > 0.0f) {
        return dot_product / (std::sqrt(norm_a) * std::sqrt(norm_b));
    }
    
    return 0.0f;
}

std::vector<float> SemanticMemory::extractFeaturesFromEpisode(const EnhancedEpisode& episode) const {
    std::vector<float> features;
    
    // Combine different episode components
    features.insert(features.end(), episode.sensory_state.begin(), episode.sensory_state.end());
    features.insert(features.end(), episode.action_state.begin(), episode.action_state.end());
    
    // Add novelty metrics as features
    features.push_back(episode.novelty_metrics.prediction_error);
    features.push_back(episode.novelty_metrics.information_gain);
    features.push_back(episode.novelty_metrics.surprise_level);
    features.push_back(episode.novelty_metrics.attention_level);
    
    // Add contextual features
    features.push_back(episode.emotional_weight);
    features.push_back(episode.reward_signal);
    
    return features;
}

std::string SemanticMemory::generateConceptLabel(const std::vector<float>& features,
                                               ConceptNode::ConceptType type) const {
    
    std::ostringstream label;
    
    // Generate label based on type
    switch (type) {
        case ConceptNode::ConceptType::Object:
            label << "Object_";
            break;
        case ConceptNode::ConceptType::Action:
            label << "Action_";
            break;
        case ConceptNode::ConceptType::Property:
            label << "Property_";
            break;
        case ConceptNode::ConceptType::Relation:
            label << "Relation_";
            break;
        case ConceptNode::ConceptType::Abstract:
            label << "Concept_";
            break;
        case ConceptNode::ConceptType::Composite:
            label << "Composite_";
            break;
    }
    
    // Add feature-based identifier
    if (!features.empty()) {
        float feature_sum = std::accumulate(features.begin(), features.end(), 0.0f);
        label << std::abs(static_cast<int>(feature_sum * 1000)) % 10000;
    } else {
        label << "Unknown";
    }
    
    return label.str();
}

void SemanticMemory::updateIndices(const ConceptNode& concept_node, int concept_id) {
    // Update type index
    type_index_[concept_node.type].push_back(concept_id);
    
    // Update keyword index (simplified)
    if (!concept_node.label.empty()) {
        keyword_index_[concept_node.label].push_back(concept_id);
    }
}

void SemanticMemory::removeFromIndices(const ConceptNode& concept_node, int concept_id) {
    // Remove from type index
    auto& type_vec = type_index_[concept_node.type];
    type_vec.erase(std::remove(type_vec.begin(), type_vec.end(), concept_id), type_vec.end());
    
    if (type_vec.empty()) {
        type_index_.erase(concept_node.type);
    }
    
    // Remove from keyword index
    if (!concept_node.label.empty()) {
        auto& keyword_vec = keyword_index_[concept_node.label];
        keyword_vec.erase(std::remove(keyword_vec.begin(), keyword_vec.end(), concept_id), keyword_vec.end());
        
        if (keyword_vec.empty()) {
            keyword_index_.erase(concept_node.label);
        }
    }
}

std::uint64_t SemanticMemory::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool SemanticMemory::validateConcept(const ConceptNode& concept_node) const {
    return !concept_node.label.empty() && !concept_node.feature_vector.empty();
}

void SemanticMemory::performAutomaticConsolidation() {
    if (shouldConsolidate()) {
        // Perform various consolidation operations without holding the main mutex
        // Each operation will acquire its own lock
        mergeSimilarConcepts();
        formHierarchicalRelationships();
        applyConceptDecay();
        
        last_consolidation_time_.store(getCurrentTimestamp());
        total_consolidations_.fetch_add(1);
    }
}

bool SemanticMemory::shouldConsolidate() const {
    auto current_time = getCurrentTimestamp();
    auto time_since_last = current_time - last_consolidation_time_.load();
    return time_since_last >= config_.consolidation_interval_ms;
}

} // namespace Memory
} // namespace NeuroForge