#include "memory/WorkingMemory.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <chrono>
#include <thread>

using namespace NeuroForge::Memory;

void testBasicOperations() {
    std::cout << "Testing basic Working Memory operations..." << std::endl;
    
    WorkingMemory::Config config;
    WorkingMemory wm(config);
    
    // Test initial state
    assert(wm.getOccupiedSlots() == 0);
    assert(wm.getCapacityUtilization() == 0.0f);
    assert(wm.getAverageActivation() == 0.0f);
    
    // Test pushing items
    std::vector<float> item1 = {1.0f, 2.0f, 3.0f};
    std::vector<float> item2 = {4.0f, 5.0f, 6.0f};
    
    bool success1 = wm.push(item1, 0.8f, "test_item_1");
    bool success2 = wm.push(item2, 0.6f, "test_item_2");
    
    assert(success1);
    assert(success2);
    assert(wm.getOccupiedSlots() == 2);
    
    // Test retrieval
    auto retrieved = wm.getSlotContent(0);
    assert(!retrieved.empty());
    assert(retrieved.size() == 3);
    
    std::cout << "✓ Basic operations test passed" << std::endl;
}

void testCapacityLimits() {
    std::cout << "Testing Working Memory capacity limits..." << std::endl;
    
    WorkingMemory::Config config;
    WorkingMemory wm(config);
    
    // Fill all slots
    for (size_t i = 0; i < WorkingMemory::MILLER_CAPACITY; ++i) {
        std::vector<float> item = {static_cast<float>(i), static_cast<float>(i+1)};
        bool success = wm.push(item, 0.7f, "item_" + std::to_string(i));
        assert(success);
    }
    
    assert(wm.getOccupiedSlots() == WorkingMemory::MILLER_CAPACITY);
    assert(wm.getCapacityUtilization() == 1.0f);
    
    // Try to add one more (should replace least active)
    std::vector<float> overflow_item = {99.0f, 100.0f};
    bool overflow_success = wm.push(overflow_item, 0.9f, "overflow");
    assert(overflow_success);
    assert(wm.getOccupiedSlots() == WorkingMemory::MILLER_CAPACITY);
    
    std::cout << "✓ Capacity limits test passed" << std::endl;
}

void testDecayMechanism() {
    std::cout << "Testing Working Memory decay mechanism..." << std::endl;
    
    WorkingMemory::Config config;
    config.decay_rate = 1.0f; // Fast decay for testing
    config.refresh_threshold = 0.3f;
    config.push_threshold = 0.1f; // Lower threshold to allow low activation items
    
    WorkingMemory wm(config);
    
    // Add items with different activation levels
    std::vector<float> item1 = {1.0f, 2.0f};
    std::vector<float> item2 = {3.0f, 4.0f};
    
    bool push1_result = wm.push(item1, 0.8f, "high_activation");
    bool push2_result = wm.push(item2, 0.2f, "low_activation");
    
    std::cout << "Debug: push1_result = " << push1_result << ", push2_result = " << push2_result << std::endl;
    std::cout << "Debug: occupied slots after push = " << wm.getOccupiedSlots() << std::endl;

    assert(wm.getOccupiedSlots() == 2);
    
    // Apply decay - with decay_rate=1.0 and delta_time=1.0, decay_factor = exp(-1) ≈ 0.368
    // Item 1: 0.8 * 0.368 ≈ 0.294 (below 0.3 threshold, should be removed)
    // Item 2: 0.2 * 0.368 ≈ 0.074 (below 0.3 threshold, should be removed)
    wm.decay(1.0f); // 1 second of decay
    
    // Debug: Check what happened after decay
    std::cout << "Debug: After decay, occupied slots = " << wm.getOccupiedSlots() << std::endl;
    
    // Both items should be removed since both decay below the threshold
    assert(wm.getOccupiedSlots() == 0);
    
    std::cout << "✓ Decay mechanism test passed" << std::endl;
}

void testRefreshMechanism() {
    std::cout << "Testing Working Memory refresh mechanism..." << std::endl;
    
    WorkingMemory::Config config;
    WorkingMemory wm(config);
    
    std::vector<float> item = {1.0f, 2.0f, 3.0f};
    wm.push(item, 0.5f, "test_item");
    
    // Get initial activation
    auto stats_before = wm.getStatistics();
    float initial_activation = stats_before.average_activation;
    
    // Refresh the slot
    bool refresh_success = wm.refresh(0, 0.8f);
    assert(refresh_success);
    
    // Check activation increased
    auto stats_after = wm.getStatistics();
    assert(stats_after.average_activation > initial_activation);
    
    std::cout << "✓ Refresh mechanism test passed" << std::endl;
}

void testSimilarityBasedRefresh() {
    std::cout << "Testing similarity-based refresh..." << std::endl;
    
    WorkingMemory::Config config;
    WorkingMemory wm(config);
    
    std::vector<float> item1 = {1.0f, 0.0f, 0.0f};
    std::vector<float> item2 = {0.0f, 1.0f, 0.0f};
    std::vector<float> query = {0.9f, 0.1f, 0.0f}; // Similar to item1
    
    wm.push(item1, 0.5f, "similar_item");
    wm.push(item2, 0.5f, "different_item");
    
    // Refresh by similarity
    size_t refreshed_count = wm.refreshBySimilarity(query, 0.5f, 0.3f);
    assert(refreshed_count >= 1); // Should refresh at least the similar item
    
    std::cout << "✓ Similarity-based refresh test passed" << std::endl;
}

void testActiveContentRetrieval() {
    std::cout << "Testing active content retrieval..." << std::endl;
    
    WorkingMemory::Config config;
    WorkingMemory wm(config);
    
    std::vector<float> item1 = {1.0f, 2.0f};
    std::vector<float> item2 = {3.0f, 4.0f};
    
    wm.push(item1, 0.8f, "item1");
    wm.push(item2, 0.6f, "item2");
    
    // Get combined active content
    auto active_content = wm.getActiveContent();
    assert(!active_content.empty());
    
    // Get most active content
    auto most_active = wm.getMostActiveContent();
    assert(!most_active.empty());
    assert(most_active.size() == 2); // Should be item1 (higher activation)
    
    std::cout << "✓ Active content retrieval test passed" << std::endl;
}

void testSimilaritySearch() {
    std::cout << "Testing similarity search..." << std::endl;
    
    WorkingMemory::Config config;
    WorkingMemory wm(config);
    
    std::vector<float> item1 = {1.0f, 0.0f, 0.0f};
    std::vector<float> item2 = {0.0f, 1.0f, 0.0f};
    std::vector<float> item3 = {0.0f, 0.0f, 1.0f};
    
    wm.push(item1, 0.7f, "red");
    wm.push(item2, 0.6f, "green");
    wm.push(item3, 0.5f, "blue");
    
    // Search for similar item
    std::vector<float> query = {0.9f, 0.1f, 0.0f}; // Most similar to item1
    size_t similar_slot = wm.findSimilarSlot(query, 0.5f);
    
    assert(similar_slot < WorkingMemory::MILLER_CAPACITY);
    
    // Verify it found the right item
    auto found_content = wm.getSlotContent(similar_slot);
    assert(found_content[0] > 0.5f); // Should be the red item
    
    std::cout << "✓ Similarity search test passed" << std::endl;
}

void testStatistics() {
    std::cout << "Testing Working Memory statistics..." << std::endl;
    
    WorkingMemory::Config config;
    WorkingMemory wm(config);
    
    // Add some items
    for (int i = 0; i < 3; ++i) {
        std::vector<float> item = {static_cast<float>(i), static_cast<float>(i+1)};
        wm.push(item, 0.5f + i * 0.1f, "item_" + std::to_string(i));
    }
    
    auto stats = wm.getStatistics();
    
    assert(stats.occupied_slots == 3);
    assert(stats.capacity_utilization > 0.0f);
    assert(stats.average_activation > 0.0f);
    assert(stats.successful_pushes >= 3);
    
    std::cout << "✓ Statistics test passed" << std::endl;
}

void testConfiguration() {
    std::cout << "Testing Working Memory configuration..." << std::endl;
    
    WorkingMemory::Config config;
    config.decay_rate = 0.5f;
    config.refresh_threshold = 0.4f;
    config.push_threshold = 0.6f;
    
    WorkingMemory wm(config);
    
    // Test that configuration is applied
    auto retrieved_config = wm.getConfig();
    assert(retrieved_config.decay_rate == 0.5f);
    assert(retrieved_config.refresh_threshold == 0.4f);
    assert(retrieved_config.push_threshold == 0.6f);
    
    // Test push threshold enforcement
    std::vector<float> low_activation_item = {1.0f, 2.0f};
    bool should_fail = wm.push(low_activation_item, 0.3f); // Below threshold
    assert(!should_fail);
    
    bool should_succeed = wm.push(low_activation_item, 0.8f); // Above threshold
    assert(should_succeed);
    
    std::cout << "✓ Configuration test passed" << std::endl;
}

void testClearOperation() {
    std::cout << "Testing Working Memory clear operation..." << std::endl;
    
    WorkingMemory::Config config;
    WorkingMemory wm(config);
    
    // Add some items
    for (int i = 0; i < 3; ++i) {
        std::vector<float> item = {static_cast<float>(i)};
        wm.push(item, 0.7f);
    }
    
    assert(wm.getOccupiedSlots() == 3);
    
    // Clear all
    wm.clear();
    
    assert(wm.getOccupiedSlots() == 0);
    assert(wm.getCapacityUtilization() == 0.0f);
    assert(wm.getAverageActivation() == 0.0f);
    
    std::cout << "✓ Clear operation test passed" << std::endl;
}

int main() {
    std::cout << "=== Working Memory Module Test Suite ===" << std::endl;
    
    try {
        testBasicOperations();
        testCapacityLimits();
        testDecayMechanism();
        testRefreshMechanism();
        testSimilarityBasedRefresh();
        testActiveContentRetrieval();
        testSimilaritySearch();
        testStatistics();
        testConfiguration();
        testClearOperation();
        
        std::cout << "\n✅ All Working Memory tests passed!" << std::endl;
        std::cout << "Working Memory module is ready for integration." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}