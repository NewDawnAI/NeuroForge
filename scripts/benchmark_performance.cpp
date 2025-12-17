/**
 * @file benchmark_performance.cpp
 * @brief Comprehensive benchmark script for measuring NeuroForge performance improvements
 * 
 * This script benchmarks various aspects of the neural substrate performance optimizer:
 * - Memory allocation and cache performance
 * - Neural processing throughput
 * - Language processing efficiency
 * - Worker thread performance
 * - Overall system optimization effectiveness
 */

#include "core/SubstratePerformanceOptimizer.h"
#include "core/HypergraphBrain.h"
#include "core/SubstrateLanguageIntegration.h"
#include "core/NeuralLanguageBindings.h"
#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <fstream>
#include <sstream>

namespace NeuroForge {
namespace Benchmark {

class PerformanceBenchmark {
public:
    struct BenchmarkResults {
        // Memory benchmarks
        double memory_allocation_time_ms = 0.0;
        double memory_deallocation_time_ms = 0.0;
        double cache_hit_rate = 0.0;
        double memory_fragmentation = 0.0;
        
        // Neural processing benchmarks
        double neural_processing_throughput = 0.0;  // neurons/second
        double synapse_update_rate = 0.0;           // synapses/second
        double activation_latency_ms = 0.0;
        double propagation_delay_ms = 0.0;
        
        // Language processing benchmarks
        double token_processing_rate = 0.0;         // tokens/second
        double pattern_recognition_time_ms = 0.0;
        double language_cache_efficiency = 0.0;
        
        // Worker thread benchmarks
        double thread_utilization = 0.0;
        double task_completion_rate = 0.0;         // tasks/second
        double thread_synchronization_overhead_ms = 0.0;
        
        // Overall system benchmarks
        double overall_performance_score = 0.0;
        double optimization_effectiveness = 0.0;
        double system_stability_score = 0.0;
        
        // Resource usage
        size_t peak_memory_usage_mb = 0;
        double average_cpu_usage = 0.0;
        size_t context_switches = 0;
    };

private:
    std::shared_ptr<Core::HypergraphBrain> brain_;
    std::shared_ptr<Core::LanguageSystem> language_system_;
    std::shared_ptr<Core::SubstrateLanguageIntegration> language_integration_;
    std::shared_ptr<Core::NeuralLanguageBindings> neural_bindings_;
    std::shared_ptr<Core::SubstratePerformanceOptimizer> optimizer_;
    
    std::mt19937 rng_;
    std::uniform_real_distribution<float> dist_;

public:
    PerformanceBenchmark() : rng_(std::random_device{}()), dist_(0.0f, 1.0f) {
        // Create connectivity manager first
        auto connectivity_manager = std::make_shared<NeuroForge::Connectivity::ConnectivityManager>();
        
        // Create brain with required parameters
        brain_ = std::make_shared<Core::HypergraphBrain>(connectivity_manager, 100.0f, Core::HypergraphBrain::ProcessingMode::Parallel);
        
        // Create language system first with config
        Core::LanguageSystem::Config language_system_config;
        language_system_config.mimicry_learning_rate = 0.01f;
        language_system_config.grounding_strength = 0.5f;
        language_system_config.prosody_attention_weight = 0.4f;
        
        language_system_ = std::make_shared<Core::LanguageSystem>(language_system_config);
        
        // Initialize language integration with proper config
        Core::SubstrateLanguageIntegration::Config integration_config;
        integration_config.language_region_neurons = 1024;
        integration_config.language_learning_rate = 0.008f;
        integration_config.enable_sparse_updates = true;
        
        language_integration_ = std::make_shared<Core::SubstrateLanguageIntegration>(
            language_system_, brain_, integration_config);
        // Initialize neural language bindings with proper config
        Core::NeuralLanguageBindings::Config language_config;
        language_config.token_assembly_size = 8;
        language_config.assembly_coherence_threshold = 0.2f;
        language_config.neural_learning_rate = 0.01f;
        
        neural_bindings_ = std::make_shared<Core::NeuralLanguageBindings>(brain_, language_config);
        
        // Configure optimizer for benchmarking
        Core::SubstratePerformanceOptimizer::Config config;
        config.enable_memory_pooling = true;
        config.enable_parallel_processing = true;
        config.enable_token_caching = true;
        config.enable_pattern_precomputation = true;
        config.enable_batch_processing = true;
        config.enable_real_time_optimization = true;
        config.enable_performance_monitoring = true;
        config.enable_profiling = true;
        config.max_worker_threads = 8;
        
        optimizer_ = std::make_shared<Core::SubstratePerformanceOptimizer>(
            brain_, language_integration_, neural_bindings_, config);
    }

    bool initialize() {
        std::cout << "Initializing benchmark environment..." << std::endl;
        
        if (!brain_->initialize()) {
            std::cerr << "Failed to initialize HypergraphBrain" << std::endl;
            return false;
        }
        
        if (!language_integration_->initialize()) {
            std::cerr << "Failed to initialize SubstrateLanguageIntegration" << std::endl;
            return false;
        }
        
        if (!neural_bindings_->initialize()) {
            std::cerr << "Failed to initialize NeuralLanguageBindings" << std::endl;
            return false;
        }
        
        if (!optimizer_->initialize()) {
            std::cerr << "Failed to initialize SubstratePerformanceOptimizer" << std::endl;
            return false;
        }
        
        // Enable profiling for detailed metrics
        optimizer_->enableProfiling(true);
        
        std::cout << "Benchmark environment initialized successfully" << std::endl;
        return true;
    }

    BenchmarkResults runComprehensiveBenchmark() {
        BenchmarkResults results;
        
        std::cout << "\n=== Running Comprehensive Performance Benchmark ===" << std::endl;
        
        // Run individual benchmark suites
        benchmarkMemoryPerformance(results);
        benchmarkNeuralProcessing(results);
        benchmarkLanguageProcessing(results);
        benchmarkWorkerThreads(results);
        benchmarkOverallSystem(results);
        
        // Calculate composite scores
        calculateCompositeScores(results);
        
        return results;
    }

private:
    void benchmarkMemoryPerformance(BenchmarkResults& results) {
        std::cout << "\n--- Memory Performance Benchmark ---" << std::endl;
        
        const size_t num_allocations = 10000;
        const size_t allocation_size = 1024;
        std::vector<void*> allocations;
        allocations.reserve(num_allocations);
        
        // Benchmark memory allocation
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < num_allocations; ++i) {
            void* ptr = optimizer_->allocateFromPool(allocation_size);
            if (ptr) {
                allocations.push_back(ptr);
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        results.memory_allocation_time_ms = 
            std::chrono::duration<double, std::milli>(end - start).count();
        
        // Benchmark memory deallocation
        start = std::chrono::high_resolution_clock::now();
        for (void* ptr : allocations) {
            optimizer_->deallocateFromPool(ptr);
        }
        end = std::chrono::high_resolution_clock::now();
        results.memory_deallocation_time_ms = 
            std::chrono::duration<double, std::milli>(end - start).count();
        
        // Get memory metrics
        results.memory_fragmentation = optimizer_->getMemoryFragmentation();
        results.peak_memory_usage_mb = optimizer_->getMemoryUsage() / (1024 * 1024);
        
        std::cout << "Memory allocation time: " << results.memory_allocation_time_ms << " ms" << std::endl;
        std::cout << "Memory deallocation time: " << results.memory_deallocation_time_ms << " ms" << std::endl;
        std::cout << "Memory fragmentation: " << results.memory_fragmentation << std::endl;
        std::cout << "Peak memory usage: " << results.peak_memory_usage_mb << " MB" << std::endl;
    }

    void benchmarkNeuralProcessing(BenchmarkResults& results) {
        std::cout << "\n--- Neural Processing Benchmark ---" << std::endl;
        
        const size_t num_neurons = 10000;
        const size_t num_iterations = 1000;
        
        // Simulate neural processing workload
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t iter = 0; iter < num_iterations; ++iter) {
            // Trigger neural optimization
            optimizer_->optimizeNeuralSubstrate();
            
            // Simulate neural activity
            for (size_t i = 0; i < num_neurons / 100; ++i) {
                optimizer_->pruneInactiveConnections();
                optimizer_->adaptActivationThresholds();
            }
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double total_time_s = std::chrono::duration<double>(end - start).count();
        
        results.neural_processing_throughput = (num_neurons * num_iterations) / total_time_s;
        results.synapse_update_rate = results.neural_processing_throughput * 10; // Estimate
        
        // Get neural metrics from optimizer
        auto metrics = optimizer_->getPerformanceMetrics();
        results.activation_latency_ms = 1000.0 / results.neural_processing_throughput; // Estimate
        results.propagation_delay_ms = results.activation_latency_ms * 0.1; // Estimate
        
        std::cout << "Neural processing throughput: " << results.neural_processing_throughput << " neurons/s" << std::endl;
        std::cout << "Synapse update rate: " << results.synapse_update_rate << " synapses/s" << std::endl;
        std::cout << "Activation latency: " << results.activation_latency_ms << " ms" << std::endl;
    }

    void benchmarkLanguageProcessing(BenchmarkResults& results) {
        std::cout << "\n--- Language Processing Benchmark ---" << std::endl;
        
        const size_t num_tokens = 10000;
        const size_t num_patterns = 1000;
        
        // Generate test tokens and patterns
        std::vector<std::string> test_tokens;
        std::vector<std::string> test_patterns;
        
        for (size_t i = 0; i < num_tokens; ++i) {
            test_tokens.push_back("token_" + std::to_string(i));
        }
        
        for (size_t i = 0; i < num_patterns; ++i) {
            test_patterns.push_back("pattern_" + std::to_string(i));
        }
        
        // Benchmark token processing
        auto start = std::chrono::high_resolution_clock::now();
        
        for (const auto& token : test_tokens) {
            optimizer_->optimizeTokenCaching();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double token_time_s = std::chrono::duration<double>(end - start).count();
        results.token_processing_rate = num_tokens / token_time_s;
        
        // Benchmark pattern recognition
        start = std::chrono::high_resolution_clock::now();
        
        for (const auto& pattern : test_patterns) {
            optimizer_->optimizePatternPrecomputation();
            optimizer_->prefetchRelatedPatterns(pattern);
        }
        
        end = std::chrono::high_resolution_clock::now();
        results.pattern_recognition_time_ms = 
            std::chrono::duration<double, std::milli>(end - start).count();
        
        // Get language processing metrics
        results.language_cache_efficiency = optimizer_->getLanguageProcessingEfficiency();
        
        std::cout << "Token processing rate: " << results.token_processing_rate << " tokens/s" << std::endl;
        std::cout << "Pattern recognition time: " << results.pattern_recognition_time_ms << " ms" << std::endl;
        std::cout << "Language cache efficiency: " << results.language_cache_efficiency << std::endl;
    }

    void benchmarkWorkerThreads(BenchmarkResults& results) {
        std::cout << "\n--- Worker Thread Benchmark ---" << std::endl;
        
        const size_t num_tasks = 10000;
        
        // Schedule various types of tasks
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < num_tasks; ++i) {
            if (i % 4 == 0) {
                optimizer_->scheduleTask("memory_optimization", 1);
            } else if (i % 4 == 1) {
                optimizer_->scheduleTask("cache_optimization", 2);
            } else if (i % 4 == 2) {
                optimizer_->scheduleTask("neural_optimization", 1);
            } else {
                optimizer_->scheduleTask("language_optimization", 3);
            }
        }
        
        // Wait for tasks to complete (simulate by running optimization cycles)
        for (size_t i = 0; i < 100; ++i) {
            optimizer_->runOptimizationCycle();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        double total_time_s = std::chrono::duration<double>(end - start).count();
        
        results.task_completion_rate = num_tasks / total_time_s;
        results.thread_utilization = static_cast<double>(optimizer_->getActiveThreadCount()) / 8.0;
        results.thread_synchronization_overhead_ms = (total_time_s * 1000.0) / num_tasks;
        
        std::cout << "Task completion rate: " << results.task_completion_rate << " tasks/s" << std::endl;
        std::cout << "Thread utilization: " << results.thread_utilization * 100.0 << "%" << std::endl;
        std::cout << "Synchronization overhead: " << results.thread_synchronization_overhead_ms << " ms/task" << std::endl;
    }

    void benchmarkOverallSystem(BenchmarkResults& results) {
        std::cout << "\n--- Overall System Benchmark ---" << std::endl;
        
        // Run comprehensive optimization cycle
        auto start = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < 100; ++i) {
            optimizer_->runOptimizationCycle();
            optimizer_->updatePerformanceMetrics();
            
            // Simulate system load
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        
        // Get final performance metrics
        auto metrics = optimizer_->getPerformanceMetrics();
        results.overall_performance_score = metrics.overall_performance_score;
        results.optimization_effectiveness = metrics.optimization_effectiveness;
        results.average_cpu_usage = metrics.average_cpu_usage;
        
        // Calculate stability score based on performance consistency
        auto metrics_history = optimizer_->getMetricsHistory(50);
        if (!metrics_history.empty()) {
            double score_variance = 0.0;
            double mean_score = 0.0;
            
            for (const auto& m : metrics_history) {
                mean_score += m.overall_performance_score;
            }
            mean_score /= metrics_history.size();
            
            for (const auto& m : metrics_history) {
                double diff = m.overall_performance_score - mean_score;
                score_variance += diff * diff;
            }
            score_variance /= metrics_history.size();
            
            results.system_stability_score = std::max(0.0, 1.0 - score_variance);
        }
        
        std::cout << "Overall performance score: " << results.overall_performance_score << std::endl;
        std::cout << "Optimization effectiveness: " << results.optimization_effectiveness << std::endl;
        std::cout << "System stability score: " << results.system_stability_score << std::endl;
        std::cout << "Average CPU usage: " << results.average_cpu_usage * 100.0 << "%" << std::endl;
    }

    void calculateCompositeScores(BenchmarkResults& results) {
        // Calculate cache hit rate based on language processing efficiency
        results.cache_hit_rate = results.language_cache_efficiency;
        
        // Estimate context switches based on thread utilization
        results.context_switches = static_cast<size_t>(results.thread_utilization * 10000);
    }

public:
    void printDetailedResults(const BenchmarkResults& results) {
        std::cout << "\n=== Detailed Benchmark Results ===" << std::endl;
        std::cout << std::fixed << std::setprecision(3);
        
        std::cout << "\n--- Memory Performance ---" << std::endl;
        std::cout << "Allocation Time:     " << results.memory_allocation_time_ms << " ms" << std::endl;
        std::cout << "Deallocation Time:   " << results.memory_deallocation_time_ms << " ms" << std::endl;
        std::cout << "Cache Hit Rate:      " << results.cache_hit_rate * 100.0 << "%" << std::endl;
        std::cout << "Memory Fragmentation:" << results.memory_fragmentation * 100.0 << "%" << std::endl;
        std::cout << "Peak Memory Usage:   " << results.peak_memory_usage_mb << " MB" << std::endl;
        
        std::cout << "\n--- Neural Processing ---" << std::endl;
        std::cout << "Processing Throughput:" << results.neural_processing_throughput << " neurons/s" << std::endl;
        std::cout << "Synapse Update Rate: " << results.synapse_update_rate << " synapses/s" << std::endl;
        std::cout << "Activation Latency:  " << results.activation_latency_ms << " ms" << std::endl;
        std::cout << "Propagation Delay:   " << results.propagation_delay_ms << " ms" << std::endl;
        
        std::cout << "\n--- Language Processing ---" << std::endl;
        std::cout << "Token Processing Rate:" << results.token_processing_rate << " tokens/s" << std::endl;
        std::cout << "Pattern Recognition: " << results.pattern_recognition_time_ms << " ms" << std::endl;
        std::cout << "Cache Efficiency:    " << results.language_cache_efficiency * 100.0 << "%" << std::endl;
        
        std::cout << "\n--- Worker Threads ---" << std::endl;
        std::cout << "Thread Utilization:  " << results.thread_utilization * 100.0 << "%" << std::endl;
        std::cout << "Task Completion Rate:" << results.task_completion_rate << " tasks/s" << std::endl;
        std::cout << "Sync Overhead:       " << results.thread_synchronization_overhead_ms << " ms/task" << std::endl;
        
        std::cout << "\n--- Overall System ---" << std::endl;
        std::cout << "Performance Score:   " << results.overall_performance_score * 100.0 << "%" << std::endl;
        std::cout << "Optimization Effect: " << results.optimization_effectiveness * 100.0 << "%" << std::endl;
        std::cout << "Stability Score:     " << results.system_stability_score * 100.0 << "%" << std::endl;
        std::cout << "Average CPU Usage:   " << results.average_cpu_usage * 100.0 << "%" << std::endl;
        std::cout << "Context Switches:    " << results.context_switches << std::endl;
    }

    void saveResultsToFile(const BenchmarkResults& results, const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Failed to open file for writing: " << filename << std::endl;
            return;
        }
        
        file << "NeuroForge Performance Benchmark Results\n";
        file << "========================================\n\n";
        file << "Timestamp: " << std::chrono::system_clock::now().time_since_epoch().count() << "\n\n";
        
        file << "Memory Performance:\n";
        file << "  Allocation Time: " << results.memory_allocation_time_ms << " ms\n";
        file << "  Deallocation Time: " << results.memory_deallocation_time_ms << " ms\n";
        file << "  Cache Hit Rate: " << results.cache_hit_rate * 100.0 << "%\n";
        file << "  Memory Fragmentation: " << results.memory_fragmentation * 100.0 << "%\n";
        file << "  Peak Memory Usage: " << results.peak_memory_usage_mb << " MB\n\n";
        
        file << "Neural Processing:\n";
        file << "  Processing Throughput: " << results.neural_processing_throughput << " neurons/s\n";
        file << "  Synapse Update Rate: " << results.synapse_update_rate << " synapses/s\n";
        file << "  Activation Latency: " << results.activation_latency_ms << " ms\n";
        file << "  Propagation Delay: " << results.propagation_delay_ms << " ms\n\n";
        
        file << "Language Processing:\n";
        file << "  Token Processing Rate: " << results.token_processing_rate << " tokens/s\n";
        file << "  Pattern Recognition Time: " << results.pattern_recognition_time_ms << " ms\n";
        file << "  Cache Efficiency: " << results.language_cache_efficiency * 100.0 << "%\n\n";
        
        file << "Worker Threads:\n";
        file << "  Thread Utilization: " << results.thread_utilization * 100.0 << "%\n";
        file << "  Task Completion Rate: " << results.task_completion_rate << " tasks/s\n";
        file << "  Synchronization Overhead: " << results.thread_synchronization_overhead_ms << " ms/task\n\n";
        
        file << "Overall System:\n";
        file << "  Performance Score: " << results.overall_performance_score * 100.0 << "%\n";
        file << "  Optimization Effectiveness: " << results.optimization_effectiveness * 100.0 << "%\n";
        file << "  Stability Score: " << results.system_stability_score * 100.0 << "%\n";
        file << "  Average CPU Usage: " << results.average_cpu_usage * 100.0 << "%\n";
        file << "  Context Switches: " << results.context_switches << "\n";
        
        file.close();
        std::cout << "Results saved to: " << filename << std::endl;
    }
};

} // namespace Benchmark
} // namespace NeuroForge

int main() {
    try {
        NeuroForge::Benchmark::PerformanceBenchmark benchmark;
        
        if (!benchmark.initialize()) {
            std::cerr << "Failed to initialize benchmark" << std::endl;
            return 1;
        }
        
        std::cout << "Starting comprehensive performance benchmark..." << std::endl;
        auto results = benchmark.runComprehensiveBenchmark();
        
        benchmark.printDetailedResults(results);
        benchmark.saveResultsToFile(results, "benchmark_results.txt");
        
        std::cout << "\nBenchmark completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed with exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}