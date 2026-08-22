#include "connectivity/ConnectivityManager.h"
#include "core/HypergraphBrain.h"
#include "core/LanguageSystem.h"
#include "core/RelationGate.h"
#include <cassert>
#include <iostream>

using namespace NeuroForge::Core;
using NeuroForge::NeuronID;

/**
 * @brief Test suite for RelationGateManager
 *
 * Tests the hypergraph "dendritic" relation gates for autonomous grounding.
 */

void printHeader(const std::string &title) {
  std::cout << "\n========================================\n";
  std::cout << "  " << title << "\n";
  std::cout << "========================================\n";
}

// Helper to create a simple brain
std::shared_ptr<HypergraphBrain> createTestBrain() {
  auto connectivity_manager =
      std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
  return std::make_shared<HypergraphBrain>(connectivity_manager, 100.0f);
}

void testGateCreation() {
  printHeader("Test: Gate Creation");

  // Create minimal brain and language system
  auto brain = createTestBrain();

  LanguageSystem::Config lang_config;
  lang_config.embedding_dimension = 64;
  LanguageSystem lang_system(lang_config);
  lang_system.initialize();

  // Create basic relation tokens
  lang_system.createToken("cat", LanguageSystem::TokenType::Word);
  lang_system.createToken("mammal", LanguageSystem::TokenType::Word);
  lang_system.createToken("is_a", LanguageSystem::TokenType::Relation);
  lang_system.createToken("on", LanguageSystem::TokenType::Relation);
  lang_system.createToken("mat", LanguageSystem::TokenType::Word);

  // Create relation gate manager
  RelationGateConfig config;
  config.initial_gate_weight = 0.5f;

  RelationGateManager manager(brain, &lang_system, config);

  // Create a relation gate by symbol
  NeuronID gate_id = manager.createRelationGateBySymbol("cat", "is_a", "mammal",
                                                        "test_source");

  std::cout << "Created gate for (cat, is_a, mammal): " << gate_id << "\n";
  assert(gate_id != 0 && "Gate creation should succeed");

  // Create another gate
  NeuronID gate_id2 =
      manager.createRelationGateBySymbol("cat", "on", "mat", "test_source");
  std::cout << "Created gate for (cat, on, mat): " << gate_id2 << "\n";
  assert(gate_id2 != 0 && "Second gate creation should succeed");

  // Get statistics
  auto stats = manager.getStatistics();
  std::cout << "Total gates: " << stats.total_gates << "\n";
  std::cout << "Relation types: " << stats.relation_types << "\n";

  assert(stats.total_gates == 2 && "Should have 2 gates");

  std::cout << "[PASS] Gate creation works!\n";
}

void testGateQueries() {
  printHeader("Test: Gate Queries");

  auto brain = createTestBrain();

  LanguageSystem::Config lang_config;
  lang_config.embedding_dimension = 64;
  LanguageSystem lang_system(lang_config);
  lang_system.initialize();

  // Create tokens
  lang_system.createToken("dog", LanguageSystem::TokenType::Word);
  lang_system.createToken("cat", LanguageSystem::TokenType::Word);
  lang_system.createToken("animal", LanguageSystem::TokenType::Word);
  lang_system.createToken("is_a", LanguageSystem::TokenType::Relation);

  RelationGateManager manager(brain, &lang_system);

  // Create gates
  manager.createRelationGateBySymbol("dog", "is_a", "animal");
  manager.createRelationGateBySymbol("cat", "is_a", "animal");

  // Query by relation type
  std::size_t is_a_id = 0;
  lang_system.getTokenId("is_a", is_a_id);

  auto is_a_relations = manager.getRelationsByType(is_a_id);
  std::cout << "All 'is_a' relations: " << is_a_relations.size() << "\n";
  assert(is_a_relations.size() == 2 && "Should have 2 is_a relations");

  // Query by token
  std::size_t dog_id = 0;
  lang_system.getTokenId("dog", dog_id);

  auto dog_relations = manager.findRelationsFor(dog_id, true, false, false);
  std::cout << "Relations with 'dog' as subject: " << dog_relations.size()
            << "\n";
  assert(dog_relations.size() == 1 &&
         "Should have 1 relation with dog as subject");

  std::cout << "[PASS] Gate queries work!\n";
}

void testTransEScoring() {
  printHeader("Test: TransE Scoring");

  LanguageSystem::Config lang_config;
  lang_config.embedding_dimension = 64;
  LanguageSystem lang_system(lang_config);
  lang_system.initialize();

  // Create tokens with specific embeddings
  lang_system.createToken("king", LanguageSystem::TokenType::Word);
  lang_system.createToken("queen", LanguageSystem::TokenType::Word);
  lang_system.createToken("man", LanguageSystem::TokenType::Word);
  lang_system.createToken("woman", LanguageSystem::TokenType::Word);
  lang_system.createToken("is_similar_to", LanguageSystem::TokenType::Relation);
  lang_system.createToken("is_opposite_of",
                          LanguageSystem::TokenType::Relation);

  // Score relation triples
  float score1 =
      lang_system.scoreRelationTriple("king", "is_similar_to", "queen");
  float score2 =
      lang_system.scoreRelationTriple("king", "is_opposite_of", "man");

  std::cout << "Score (king, is_similar_to, queen): " << score1 << "\n";
  std::cout << "Score (king, is_opposite_of, man): " << score2 << "\n";

  // Scores should be positive
  assert(score1 > 0.0f && "Score should be positive");
  assert(score2 > 0.0f && "Score should be positive");

  // Get all relation tokens
  auto relations = lang_system.getAllRelationTokens();
  std::cout << "Relation tokens (" << relations.size() << "): ";
  for (const auto &r : relations) {
    std::cout << r << " ";
  }
  std::cout << "\n";

  // Propose relations
  auto proposals = lang_system.proposeRelationsFor("king", "queen", 3);
  std::cout << "Proposed relations for (king, queen):\n";
  for (const auto &[rel, score] : proposals) {
    std::cout << "  - " << rel << ": " << score << "\n";
  }

  std::cout << "[PASS] TransE scoring works!\n";
}

void testGateLearning() {
  printHeader("Test: Gate Learning");

  auto brain = createTestBrain();

  LanguageSystem::Config lang_config;
  lang_config.embedding_dimension = 64;
  LanguageSystem lang_system(lang_config);
  lang_system.initialize();

  lang_system.createToken("sun", LanguageSystem::TokenType::Word);
  lang_system.createToken("star", LanguageSystem::TokenType::Word);
  lang_system.createToken("is_a", LanguageSystem::TokenType::Relation);

  RelationGateManager manager(brain, &lang_system);

  NeuronID gate = manager.createRelationGateBySymbol("sun", "is_a", "star");

  std::cout << "Initial gate created: " << gate << "\n";

  // Reinforce the gate (positive evidence)
  manager.reinforceGate(gate, 0.2f);
  std::cout << "Gate reinforced with positive evidence\n";

  // Contradict the gate (negative evidence)
  manager.contradictGate(gate, 0.1f);
  std::cout << "Gate contradicted with negative evidence\n";

  // Get the relation
  std::size_t sun_id, is_a_id, star_id;
  lang_system.getTokenId("sun", sun_id);
  lang_system.getTokenId("is_a", is_a_id);
  lang_system.getTokenId("star", star_id);

  auto relation = manager.getRelation(sun_id, is_a_id, star_id);
  if (relation.has_value()) {
    std::cout << "Gate confidence: " << relation->confidence << "\n";
    std::cout << "Verification count: " << relation->verification_count << "\n";
    std::cout << "Contradiction count: " << relation->contradiction_count
              << "\n";

    assert(relation->verification_count == 1 && "Should have 1 verification");
    assert(relation->contradiction_count == 1 && "Should have 1 contradiction");
  } else {
    assert(false && "Relation should exist");
  }

  std::cout << "[PASS] Gate learning works!\n";
}

void testGatePruning() {
  printHeader("Test: Gate Pruning");

  auto brain = createTestBrain();

  LanguageSystem::Config lang_config;
  lang_config.embedding_dimension = 64;
  LanguageSystem lang_system(lang_config);
  lang_system.initialize();

  lang_system.createToken("unicorn", LanguageSystem::TokenType::Word);
  lang_system.createToken("horse", LanguageSystem::TokenType::Word);
  lang_system.createToken("is_a", LanguageSystem::TokenType::Relation);

  RelationGateConfig config;
  config.prune_threshold = 0.2f;
  config.min_age_before_prune_ms = 0; // Allow immediate pruning for test

  RelationGateManager manager(brain, &lang_system, config);

  NeuronID gate =
      manager.createRelationGateBySymbol("unicorn", "is_a", "horse");

  // Weaken the gate heavily
  for (int i = 0; i < 10; i++) {
    manager.contradictGate(gate, 0.1f);
  }

  auto stats_before = manager.getStatistics();
  std::cout << "Gates before pruning: " << stats_before.total_gates << "\n";

  std::size_t pruned = manager.pruneWeakGates();

  auto stats_after = manager.getStatistics();
  std::cout << "Gates after pruning: " << stats_after.total_gates << "\n";
  std::cout << "Gates pruned: " << pruned << "\n";

  // Gate should have been pruned (confidence went below threshold)
  assert(stats_after.total_gates < stats_before.total_gates || pruned > 0);

  std::cout << "[PASS] Gate pruning works!\n";
}

void testEvidenceAccumulation() {
  printHeader("Test: Phase 15b/16b Evidence Accumulation with Domain Classes");

  auto brain = createTestBrain();

  LanguageSystem::Config lang_config;
  lang_config.embedding_dimension = 64;
  LanguageSystem lang_system(lang_config);
  lang_system.initialize();

  lang_system.createToken("machine_learning", LanguageSystem::TokenType::Word);
  lang_system.createToken("prediction", LanguageSystem::TokenType::Word);
  lang_system.createToken("used_for", LanguageSystem::TokenType::Relation);

  RelationGateManager manager(brain, &lang_system);

  // Get token IDs for later queries
  std::size_t ml_id, uf_id, pred_id;
  lang_system.getTokenId("machine_learning", ml_id);
  lang_system.getTokenId("used_for", uf_id);
  lang_system.getTokenId("prediction", pred_id);

  // 1. Create gate from Source A (Wikipedia) - provisional
  NeuronID gate = manager.createRelationGateBySymbol(
      "machine_learning", "used_for", "prediction",
      "https://en.wikipedia.org/wiki/ML", 0.15f, true); // is_provisional=true

  auto rel = manager.getRelation(ml_id, uf_id, pred_id);
  assert(rel.has_value() && "Relation should exist");
  assert(rel->is_provisional && "Should be provisional");
  assert(rel->support_count == 1 && "Should have support_count=1");
  assert(rel->domain_classes.size() == 1 && "Should have 1 domain class");
  std::cout
      << "Step 1: Created provisional gate from Wikipedia (Encyclopedia)\n";
  std::cout << "  is_provisional=" << rel->is_provisional
            << " support_count=" << rel->support_count
            << " domain_classes=" << rel->domain_classes.size() << "\n";

  // 2. Same fact from another Wikipedia page → no promotion (same domain class)
  manager.createRelationGateBySymbol(
      "machine_learning", "used_for", "prediction",
      "https://en.wikipedia.org/wiki/Artificial_intelligence", 0.15f, true);

  rel = manager.getRelation(ml_id, uf_id, pred_id);
  assert(rel->is_provisional &&
         "Should still be provisional (same domain class)");
  assert(rel->domain_classes.size() == 1 &&
         "Still 1 domain class (Encyclopedia)");
  std::cout << "Step 2: Same fact from Wikipedia again - no change (same "
               "domain class)\n";
  std::cout << "  is_provisional=" << rel->is_provisional
            << " support_count=" << rel->support_count
            << " domain_classes=" << rel->domain_classes.size() << "\n";

  // 3. Same fact from Source B (arXiv - Academic) → PROMOTED (2 domain classes)
  manager.createRelationGateBySymbol(
      "machine_learning", "used_for", "prediction",
      "https://arxiv.org/abs/12345", 0.15f, true);

  rel = manager.getRelation(ml_id, uf_id, pred_id);
  assert(!rel->is_provisional && "Should now be CONFIRMED (2 domain classes)");
  assert(rel->support_count == 2 && "Should have support_count=2");
  assert(rel->domain_classes.size() == 2 && "Should have 2 domain classes");
  std::cout << "Step 3: Same fact from arXiv (Academic) - PROMOTED!\n";
  std::cout << "  is_provisional=" << rel->is_provisional
            << " support_count=" << rel->support_count
            << " domain_classes=" << rel->domain_classes.size() << "\n";

  std::cout << "[PASS] Evidence accumulation with domain classes works!\n";
}

int main() {
  std::cout << "\n======================================================\n";
  std::cout << "   RelationGate Test Suite                            \n";
  std::cout << "   Autonomous Internet Grounding - Phase 1            \n";
  std::cout << "======================================================\n";

  try {
    testGateCreation();
    testGateQueries();
    testTransEScoring();
    testGateLearning();
    testGatePruning();
    testEvidenceAccumulation();

    std::cout << "\n========================================\n";
    std::cout << "  ALL TESTS PASSED!\n";
    std::cout << "========================================\n\n";

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "\n[FAIL] Exception: " << e.what() << "\n";
    return 1;
  }
}
