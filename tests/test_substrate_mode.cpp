#include <iostream>
#include <vector>
#include <functional>
#include <string>
#include <memory>
#include <cmath>

#include "core/HypergraphBrain.h"
#include "core/LearningSystem.h"
#include "connectivity/ConnectivityManager.h"

// Minimal test framework
struct TestCase {
    std::string name;
    std::function<void()> test_func;
};

static std::vector<TestCase> test_cases;

#define TEST_F(test_fixture, test_name) \
    void test_fixture##_##test_name(); \
    static bool test_fixture##_##test_name##_registered = []() { \
        test_cases.push_back({#test_fixture "." #test_name, test_fixture##_##test_name}); \
        return true; \
    }(); \
    void test_fixture##_##test_name()

#define EXPECT_TRUE(condition) \
    if (!(condition)) { \
        std::cerr << "EXPECT_TRUE failed: " << #condition << std::endl; \
        throw std::runtime_error("Assertion failed"); \
    }

#define EXPECT_FALSE(condition) \
    if (condition) { \
        std::cerr << "EXPECT_FALSE failed: " << #condition << std::endl; \
        throw std::runtime_error("Assertion failed"); \
    }

using namespace NeuroForge::Core;

class SubstrateModeTest {
public:
    void setUp() {
        auto connectivity_manager = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
        brain_ = std::make_unique<HypergraphBrain>(connectivity_manager);

        // Initialize learning system
        NeuroForge::Core::LearningSystem::Config config;
        brain_->initializeLearning(config);
    }

    void tearDown() {
        brain_.reset();
    }

    std::unique_ptr<HypergraphBrain> brain_;
};

TEST_F(SubstrateModeTest, MirrorModeConfiguration) {
    SubstrateModeTest test;
    test.setUp();

    // Initial state
    test.brain_->setSubstrateTaskGenerationEnabled(true);
    test.brain_->setAutonomousModeEnabled(true);
    test.brain_->getLearningSystem()->setMimicryEnabled(false);
    test.brain_->getLearningSystem()->setSubstrateTrainingMode(true);

    // Set Mirror Mode
    test.brain_->setSubstrateMode(HypergraphBrain::SubstrateMode::Mirror);

    // Verify
    EXPECT_FALSE(test.brain_->isSubstrateTaskGenerationEnabled());
    EXPECT_FALSE(test.brain_->isAutonomousModeEnabled());
    EXPECT_TRUE(test.brain_->getLearningSystem()->isMimicryEnabled());
    EXPECT_FALSE(test.brain_->getLearningSystem()->isSubstrateTrainingMode());

    test.tearDown();
}

TEST_F(SubstrateModeTest, OffModeConfiguration) {
    SubstrateModeTest test;
    test.setUp();

    test.brain_->setSubstrateMode(HypergraphBrain::SubstrateMode::Off);
    EXPECT_FALSE(test.brain_->isSubstrateTaskGenerationEnabled());

    test.tearDown();
}

TEST_F(SubstrateModeTest, TrainModeConfiguration) {
    SubstrateModeTest test;
    test.setUp();

    test.brain_->setSubstrateMode(HypergraphBrain::SubstrateMode::Train);

    EXPECT_TRUE(test.brain_->isSubstrateTaskGenerationEnabled());
    EXPECT_TRUE(test.brain_->getLearningSystem()->isSubstrateTrainingMode());

    test.tearDown();
}

TEST_F(SubstrateModeTest, NativeModeConfiguration) {
    SubstrateModeTest test;
    test.setUp();

    test.brain_->setSubstrateMode(HypergraphBrain::SubstrateMode::Native);

    EXPECT_TRUE(test.brain_->isSubstrateTaskGenerationEnabled());
    EXPECT_TRUE(test.brain_->isAutonomousModeEnabled());
    EXPECT_FALSE(test.brain_->getLearningSystem()->isSubstrateTrainingMode());

    test.tearDown();
}

int main() {
    int passed = 0;
    int failed = 0;

    for (const auto& test : test_cases) {
        std::cout << "Running " << test.name << "..." << std::endl;
        try {
            test.test_func();
            std::cout << "PASSED: " << test.name << std::endl;
            passed++;
        } catch (const std::exception& e) {
            std::cout << "FAILED: " << test.name << " - " << e.what() << std::endl;
            failed++;
        } catch (...) {
            std::cout << "FAILED: " << test.name << " - Unknown exception" << std::endl;
            failed++;
        }
    }

    std::cout << "\nTest Results: " << passed << " passed, " << failed << " failed" << std::endl;
    return failed > 0 ? 1 : 0;
}
