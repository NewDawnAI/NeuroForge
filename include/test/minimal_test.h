#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <sstream>

namespace NeuroForge {
namespace Test {

/**
 * @brief Simple test result tracking
 */
struct TestResult {
    std::string test_name;
    bool passed = false;
    std::string failure_message;
    double execution_time_ms = 0.0;
};

/**
 * @brief Test suite for organizing tests
 */
class TestSuite {
public:
    explicit TestSuite(const std::string& name) : suite_name_(name) {}
    
    void addTest(const std::string& name, std::function<bool()> test_func) {
        tests_.push_back({name, test_func});
    }
    
    void run() {
        std::cout << "Running test suite: " << suite_name_ << std::endl;
        
        for (const auto& test : tests_) {
            auto start = std::chrono::high_resolution_clock::now();
            
            try {
                bool result = test.second();
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration<double, std::milli>(end - start);
                
                TestResult test_result;
                test_result.test_name = test.first;
                test_result.passed = result;
                test_result.execution_time_ms = duration.count();
                
                results_.push_back(test_result);
                
                if (result) {
                    std::cout << "  [PASS] " << test.first << " (" << duration.count() << "ms)" << std::endl;
                } else {
                    std::cout << "  [FAIL] " << test.first << " (" << duration.count() << "ms)" << std::endl;
                }
            } catch (const std::exception& e) {
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration<double, std::milli>(end - start);
                
                TestResult test_result;
                test_result.test_name = test.first;
                test_result.passed = false;
                test_result.failure_message = e.what();
                test_result.execution_time_ms = duration.count();
                
                results_.push_back(test_result);
                std::cout << "  [ERROR] " << test.first << " - " << e.what() << std::endl;
            }
        }
        
        printSummary();
    }
    
    const std::vector<TestResult>& getResults() const { return results_; }
    
    bool allTestsPassed() const {
        for (const auto& result : results_) {
            if (!result.passed) return false;
        }
        return true;
    }

private:
    void printSummary() {
        int passed = 0;
        int failed = 0;
        double total_time = 0.0;
        
        for (const auto& result : results_) {
            if (result.passed) passed++;
            else failed++;
            total_time += result.execution_time_ms;
        }
        
        std::cout << "\nTest Summary for " << suite_name_ << ":" << std::endl;
        std::cout << "  Total: " << (passed + failed) << std::endl;
        std::cout << "  Passed: " << passed << std::endl;
        std::cout << "  Failed: " << failed << std::endl;
        std::cout << "  Total Time: " << total_time << "ms" << std::endl;
        
        if (failed == 0) {
            std::cout << "  Result: ALL TESTS PASSED!" << std::endl;
        } else {
            std::cout << "  Result: " << failed << " TESTS FAILED!" << std::endl;
        }
    }

private:
    std::string suite_name_;
    std::vector<std::pair<std::string, std::function<bool()>>> tests_;
    std::vector<TestResult> results_;
};

// Assertion macros
#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: " << #condition << " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    } while(0)

#define ASSERT_FALSE(condition) ASSERT_TRUE(!(condition))

#define ASSERT_EQ(expected, actual) \
    do { \
        if ((expected) != (actual)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: expected " << (expected) << " but got " << (actual) \
                << " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    } while(0)

#define ASSERT_NE(expected, actual) \
    do { \
        if ((expected) == (actual)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: expected " << (expected) << " to not equal " << (actual) \
                << " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    } while(0)

#define ASSERT_LT(a, b) ASSERT_TRUE((a) < (b))
#define ASSERT_LE(a, b) ASSERT_TRUE((a) <= (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))
#define ASSERT_GE(a, b) ASSERT_TRUE((a) >= (b))

#define ASSERT_NEAR(expected, actual, tolerance) \
    do { \
        auto diff = std::abs((expected) - (actual)); \
        if (diff > (tolerance)) { \
            std::ostringstream oss; \
            oss << "Assertion failed: expected " << (expected) << " but got " << (actual) \
                << " (difference " << diff << " > tolerance " << (tolerance) << ")" \
                << " at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(oss.str()); \
        } \
    } while(0)

// Test registration helper
#define TEST(suite_name, test_name) \
    bool test_##suite_name##_##test_name(); \
    static bool register_##suite_name##_##test_name = []() { \
        /* Test registration would go here if we had a global registry */ \
        return true; \
    }(); \
    bool test_##suite_name##_##test_name()

} // namespace Test
} // namespace NeuroForge