#pragma once

#include "core/Types.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <mutex>
#include <atomic>

namespace NeuroForge {
namespace Memory {

// Forward declarations
struct EnhancedEpisode;
class EpisodicMemoryManager;

struct ConceptNode {
    enum class ConceptType { Object, Action, Property, Relation, Abstract, Composite };
    std::string label;
    std::vector<float> feature_vector;
    std::string description;
    ConceptType type = ConceptType::Abstract;
    std::uint64_t creation_timestamp_ms = 0;
    std::uint64_t last_access_timestamp_ms = 0;
    float consolidation_strength = 0.0f;
    float episodic_support = 0.0f;
    float certainty = 0.0f;
    std::uint32_t access_count = 0;
    float abstraction_level = 0.0f;
    std::vector<int> related_concepts;
    std::unordered_map<int, float> relationship_strengths;
    std::vector<int> parent_concepts;
    std::vector<int> child_concepts;
    ConceptNode() = default;
    ConceptNode(const std::string& concept_label,
                const std::vector<float>& features,
                ConceptType concept_type,
                const std::string& desc = "");
    float calculateSimilarity(const ConceptNode& other) const;
    std::uint64_t getAge() const;
    bool shouldConsolidate(float consolidation_threshold) const;
    void updateWithEvidence(const std::vector<float>& new_features, float evidence_weight);
    void addRelationship(int concept_id, float strength, bool bidirectional);
};

struct SemanticStatistics {
    std::size_t total_concepts_created = 0;
    std::size_t active_concepts_count = 0;
    std::size_t total_consolidations = 0;
    std::size_t total_concept_accesses = 0;
    std::size_t concepts_merged = 0;
    std::size_t total_relationships = 0;
    float average_concept_age_ms = 0.0f;
    float average_consolidation_strength = 0.0f;
    float average_relationships_per_concept = 0.0f;
    std::size_t concept_types_count = 0;
    bool consolidation_active = false;
};

struct ConceptHierarchy {
    std::vector<ConceptNode> parents;
    std::vector<ConceptNode> children;
    std::vector<ConceptNode> siblings;
};

struct SemanticConfig {
    std::size_t max_concepts = 50000;
    float decay_rate = 0.005f;
    bool enable_concept_merging = true;
    float concept_merge_threshold = 0.85f;
    bool enable_hierarchy_formation = true;
    float concept_creation_threshold = 0.75f;
    std::uint64_t consolidation_interval_ms = 5000;
};

class SemanticMemory {
public:
    using Config = SemanticConfig;
    explicit SemanticMemory(const Config& config = Config{});
    ~SemanticMemory() = default;

    int createConcept(const std::string& label,
                      const std::vector<float>& features,
                      ConceptNode::ConceptType type,
                      const std::string& description = "");
    // Lightweight wrapper used by integrator: creates an Abstract concept
    int addConcept(const std::string& label,
                   const std::vector<float>& features);
    std::vector<int> extractConceptsFromEpisode(const EnhancedEpisode& episode,
                                                float extraction_threshold = -1.0f);

    std::unique_ptr<ConceptNode> retrieveConcept(int concept_id) const;
    std::unique_ptr<ConceptNode> retrieveConceptByLabel(const std::string& label) const;

    std::vector<std::pair<ConceptNode, float>> findSimilarConcepts(
        const std::vector<float>& query_features,
        std::size_t max_results = 10,
        float similarity_threshold = 0.0f) const;
    std::vector<ConceptNode> findConceptsByType(ConceptNode::ConceptType type,
                                                std::size_t max_results = 10) const;

    bool linkConcepts(int concept_id_1, int concept_id_2,
                      float strength = 1.0f, bool bidirectional = true);
    bool createHierarchicalRelationship(int child_id, int parent_id, float strength = 1.0f);
    std::vector<std::pair<ConceptNode, float>> getRelatedConcepts(
        int concept_id, std::size_t max_results = 10, float min_strength = 0.0f) const;
    ConceptHierarchy getConceptHierarchy(int concept_id, std::size_t max_depth = 2) const;

    std::size_t consolidateFromEpisodicMemory(const EpisodicMemoryManager& episodic_manager,
                                              std::size_t max_episodes = 10);

    std::size_t mergeSimilarConcepts(float merge_threshold = -1.0f);
    std::size_t formHierarchicalRelationships(float hierarchy_threshold = 0.8f);
    std::size_t applyConceptDecay(float decay_factor = -1.0f);
    std::size_t pruneWeakConcepts(float min_consolidation_strength = 0.2f,
                                  std::uint32_t min_access_count = 2);
    void clearAllConcepts();

    std::vector<ConceptNode> queryKnowledgeGraph(int query_concept,
                                                 const std::vector<std::string>& relationship_types,
                                                 std::size_t max_depth = 2) const;
    std::vector<int> findConceptualPath(int start_concept_id, int end_concept_id,
                                        std::size_t max_path_length = 6) const;
    std::vector<std::pair<ConceptNode, std::size_t>> getConceptNeighborhood(
        int concept_id, std::size_t radius = 2) const;

    SemanticStatistics getStatistics() const;
    void setConfig(const Config& new_config);
    std::size_t getTotalConceptCount() const;
    bool isOperational() const;

private:
    float calculateCosineSimilarity(const std::vector<float>& vec_a,
                                    const std::vector<float>& vec_b) const;
    std::vector<float> extractFeaturesFromEpisode(const EnhancedEpisode& episode) const;
    std::string generateConceptLabel(const std::vector<float>& features,
                                     ConceptNode::ConceptType type) const;
    void updateIndices(const ConceptNode& concept_node, int concept_id);
    void removeFromIndices(const ConceptNode& concept_node, int concept_id);
    std::uint64_t getCurrentTimestamp() const;
    bool validateConcept(const ConceptNode& concept_node) const;
    void performAutomaticConsolidation();
    bool shouldConsolidate() const;

private:
    Config config_;
    std::unordered_map<int, ConceptNode> concept_graph_;
    std::unordered_map<std::string, int> label_to_id_;
    std::unordered_map<ConceptNode::ConceptType, std::vector<int>> type_index_;
    std::unordered_map<std::string, std::vector<int>> keyword_index_;
    mutable std::mutex concepts_mutex_;
    std::atomic<int> next_concept_id_{1};
    std::atomic<std::size_t> total_concepts_created_{0};
    std::atomic<std::size_t> total_consolidations_{0};
    std::atomic<std::size_t> total_concept_accesses_{0};
    std::atomic<std::size_t> total_relationships_created_{0};
    std::atomic<std::size_t> concepts_merged_{0};
    std::atomic<std::uint64_t> last_consolidation_time_{0};
};

} // namespace Memory
} // namespace NeuroForge