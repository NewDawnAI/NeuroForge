#pragma once

#include "core/Types.h"
#include "core/HypergraphBrain.h"
#include "core/SubstrateLanguageIntegration.h"
#include "core/NeuralLanguageBindings.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>
#include <queue>
#include <condition_variable>

namespace NeuroForge {
namespace Core {

/**
 * @brief Performance optimizer for large-scale neural substrate operations
 * 
 * Provides comprehensive optimization strategies for neural substrate language
 * processing, including memory management, computational efficiency, and
 * scalability enhancements for production deployment.
 */
class SubstratePerformanceOptimizer {
public:
    /**
     * @brief Performance optimization configuration
     */
    struct Config {
        // Memory optimization
        bool enable_memory_pooling = true;                 ///< Enable memory pooling for neural objects
        bool enable_sparse_representations = true;         ///< Use sparse data structures
        std::size_t memory_pool_size = 1024 * 1024 * 100; ///< Memory pool size (100MB)
        float memory_usage_threshold = 0.85f;             ///< Memory usage threshold for cleanup
        
        // Computational optimization
        bool enable_parallel_processing = true;           ///< Enable parallel neural processing
        bool enable_vectorized_operations = true;         ///< Use vectorized operations
        std::size_t max_worker_threads = 8;               ///< Maximum worker threads
        float cpu_usage_threshold = 0.80f;                ///< CPU usage threshold
        
        // Neural substrate optimization
        bool enable_adaptive_thresholds = true;           ///< Adaptive activation thresholds
        bool enable_dynamic_pruning = true;               ///< Dynamic connection pruning
        float pruning_threshold = 0.01f;                  ///< Synaptic weight pruning threshold
        std::size_t pruning_interval_ms = 5000;           ///< Pruning interval in milliseconds
        
        // Language processing optimization
        bool enable_token_caching = true;                 ///< Cache frequently used tokens
        bool enable_pattern_precomputation = true;        ///< Precompute common patterns
        std::size_t token_cache_size = 1000;              ///< Token cache size
        std::size_t pattern_cache_size = 500;             ///< Pattern cache size
        
        // Batch processing optimization
        bool enable_batch_processing = true;              ///< Enable batch processing
        std::size_t batch_size = 64;                      ///< Batch size for operations
        std::size_t max_batch_queue_size = 256;           ///< Maximum batch queue size
        
        // Real-time optimization
        bool enable_real_time_optimization = true;        ///< Enable real-time optimization
        float target_frame_rate = 60.0f;                  ///< Target processing frame rate
        float optimization_interval_ms = 100.0f;          ///< Optimization interval
        
        // Scalability parameters
        std::size_t max_neural_assemblies = 10000;        ///< Maximum neural assemblies
        std::size_t max_proto_word_patterns = 5000;       ///< Maximum proto-word patterns
        std::size_t max_cross_modal_bindings = 2000;      ///< Maximum cross-modal bindings
        
        // Performance monitoring
        bool enable_performance_monitoring = true;        ///< Enable performance monitoring
        bool enable_profiling = false;                    ///< Enable detailed profiling
        std::size_t monitoring_window_ms = 1000;          ///< Monitoring window size
    };

    /**
     * @brief Performance metrics
     */
    struct PerformanceMetrics {
        // Memory metrics
        std::size_t total_memory_usage = 0;               ///< Total memory usage in bytes
        std::size_t peak_memory_usage = 0;                ///< Peak memory usage
        float memory_fragmentation = 0.0f;               ///< Memory fragmentation ratio
        std::size_t memory_allocations = 0;              ///< Number of memory allocations
        std::size_t memory_deallocations = 0;            ///< Number of memory deallocations
        
        // Computational metrics
        float cpu_usage = 0.0f;                          ///< Current CPU usage
        float average_cpu_usage = 0.0f;                  ///< Average CPU usage
        std::size_t active_threads = 0;                  ///< Number of active threads
        float processing_throughput = 0.0f;              ///< Operations per second
        
        // Neural substrate metrics
        std::size_t active_neurons = 0;                  ///< Number of active neurons
        std::size_t active_synapses = 0;                 ///< Number of active synapses
        float neural_utilization = 0.0f;                ///< Neural resource utilization
        std::size_t pruned_connections = 0;              ///< Number of pruned connections
        
        // Language processing metrics
        std::size_t token_cache_hits = 0;                ///< Token cache hits
        std::size_t token_cache_misses = 0;              ///< Token cache misses
        std::size_t pattern_cache_hits = 0;              ///< Pattern cache hits
        std::size_t pattern_cache_misses = 0;            ///< Pattern cache misses
        float language_processing_efficiency = 0.0f;     ///< Language processing efficiency
        
        // Batch processing metrics
        std::size_t batches_processed = 0;               ///< Number of batches processed
        float average_batch_size = 0.0f;                ///< Average batch size
        float batch_processing_time = 0.0f;             ///< Average batch processing time
        
        // Real-time metrics
        float current_frame_rate = 0.0f;                ///< Current processing frame rate
        float average_frame_time = 0.0f;                ///< Average frame processing time
        std::size_t dropped_frames = 0;                 ///< Number of dropped frames
        
        // Overall performance
        float overall_performance_score = 0.0f;         ///< Overall performance score (0-1)
        float optimization_effectiveness = 0.0f;        ///< Optimization effectiveness
        std::chrono::steady_clock::time_point last_update; ///< Last metrics update time
    };

    /**
     * @brief Optimization strategy enumeration
     */
    enum class OptimizationStrategy {
        Conservative,    ///< Conservative optimization (stability focused)
        Balanced,        ///< Balanced optimization (performance and stability)
        Aggressive,      ///< Aggressive optimization (maximum performance)
        Adaptive         ///< Adaptive optimization (dynamic strategy selection)
    };

    /**
     * @brief Performance bottleneck types
     */
    enum class BottleneckType {
        CPU,             ///< CPU bottleneck
        Memory,          ///< Memory bottleneck
        IO,              ///< I/O bottleneck
        Network,         ///< Network bottleneck
        Cache,           ///< Cache bottleneck
        Neural,          ///< Neural processing bottleneck
        Language         ///< Language processing bottleneck
    };

    /**
     * @brief Performance bottleneck information
     */
    struct BottleneckInfo {
        BottleneckType type;                              ///< Type of bottleneck
        std::string description;                          ///< Description of the bottleneck
        float severity;                                   ///< Severity level (0.0-1.0)
        std::string recommendation;                       ///< Optimization recommendation
    };

    /**
     * @brief Performance bottleneck analysis
     */
    struct BottleneckAnalysis {
        std::chrono::high_resolution_clock::time_point timestamp; ///< Analysis timestamp
        std::vector<BottleneckInfo> bottlenecks;          ///< Detected bottlenecks
    };

    /**
     * @brief Memory pool for neural objects
     */
    struct MemoryPool {
        std::vector<uint8_t> pool_memory;                ///< Pool memory buffer
        std::vector<bool> allocation_map;               ///< Allocation tracking
        std::size_t pool_size = 0;                     ///< Total pool size
        std::size_t allocated_size = 0;                ///< Currently allocated size
        std::size_t allocation_count = 0;              ///< Number of allocations
        mutable std::mutex pool_mutex;                 ///< Pool access mutex
    };

private:
    // System references
    std::shared_ptr<HypergraphBrain> hypergraph_brain_;
    std::shared_ptr<SubstrateLanguageIntegration> substrate_integration_;
    std::shared_ptr<NeuralLanguageBindings> neural_bindings_;
    
    // Configuration and state
    Config config_;
    OptimizationStrategy strategy_;
    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> is_optimizing_{false};
    
    // Performance monitoring
    PerformanceMetrics current_metrics_;
    std::vector<PerformanceMetrics> metrics_history_;
    mutable std::mutex metrics_mutex_;
    
    // Memory management
    std::unique_ptr<MemoryPool> memory_pool_;
    std::unordered_map<void*, std::size_t> allocation_sizes_;
    mutable std::mutex memory_mutex_;
    
    // Thread management
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> threads_active_{false};
    std::atomic<std::size_t> active_thread_count_{0};
    
    // Task management for worker threads
    struct Task {
        std::string type;
        int priority;
        std::chrono::steady_clock::time_point scheduled_time;
        std::chrono::steady_clock::time_point delay_until;
        bool is_delayed;
        
        Task(const std::string& t, int p = 0) 
            : type(t), priority(p), scheduled_time(std::chrono::steady_clock::now()), 
              delay_until(std::chrono::steady_clock::now()), is_delayed(false) {}
    };
    
    std::queue<Task> task_queue_;
    std::queue<Task> high_priority_queue_;
    std::queue<Task> delayed_task_queue_;
    mutable std::mutex task_mutex_;
    std::condition_variable task_condition_;
    std::atomic<bool> shutdown_requested_{false};
    
    // Caching systems
    std::unordered_map<std::string, std::vector<float>> token_cache_;
    std::unordered_map<std::string, float> pattern_cache_;
    mutable std::mutex cache_mutex_;
    
    // Batch processing
    struct BatchOperation {
        enum Type { TokenActivation, PatternReinforcement, BindingUpdate };
        Type type;
        std::vector<std::string> parameters;
        std::vector<float> values;
    };
    std::vector<BatchOperation> batch_queue_;
    mutable std::mutex batch_mutex_;
    
    // Real-time optimization
    std::chrono::steady_clock::time_point last_optimization_time_;
    std::chrono::steady_clock::time_point last_frame_time_;
    std::vector<float> frame_times_;
    
    // Profiling data structures
    struct PerformanceSnapshot {
        std::chrono::high_resolution_clock::time_point timestamp;
        float cpu_usage;
        std::size_t memory_usage;
        std::size_t thread_count;
        std::size_t context_switches;
        float cache_hit_rate;
        std::size_t page_faults;
        float neurons_per_second;
        float synapses_per_second;
        float activation_latency;
        float propagation_delay;
        float disk_reads_per_second;
        float disk_writes_per_second;
        float network_throughput;
    };
    
    std::vector<PerformanceSnapshot> performance_snapshots_;
    std::unordered_map<std::string, std::vector<float>> profiling_data_;
    std::vector<BottleneckAnalysis> bottleneck_history_;
    std::chrono::high_resolution_clock::time_point profiling_start_time_;
    mutable std::mutex profiling_mutex_;

public:
    /**
     * @brief Constructor
     * @param hypergraph_brain Shared pointer to hypergraph brain
     * @param substrate_integration Shared pointer to substrate integration
     * @param neural_bindings Shared pointer to neural bindings
     * @param config Optimization configuration
     */
    explicit SubstratePerformanceOptimizer(
        std::shared_ptr<HypergraphBrain> hypergraph_brain,
        std::shared_ptr<SubstrateLanguageIntegration> substrate_integration,
        std::shared_ptr<NeuralLanguageBindings> neural_bindings,
        const Config& config);

    /**
     * @brief Destructor
     */
    ~SubstratePerformanceOptimizer();

    // Core lifecycle
    bool initialize();
    void shutdown();
    void reset();

    // Configuration management
    void updateConfig(const Config& new_config);
    const Config& getConfig() const { return config_; }
    void setOptimizationStrategy(OptimizationStrategy strategy);
    OptimizationStrategy getOptimizationStrategy() const { return strategy_; }

    // Memory optimization
    bool initializeMemoryPool();
    void* allocateFromPool(std::size_t size);
    void deallocateFromPool(void* ptr);
    void optimizeMemoryUsage();
    void defragmentMemory();
    std::size_t getMemoryUsage() const;
    float getMemoryFragmentation() const;

    // Computational optimization
    void optimizeComputationalLoad();
    void balanceThreadLoad();
    void optimizeVectorizedOperations();
    void adaptProcessingStrategy();
    float getCPUUsage() const;
    std::size_t getActiveThreadCount() const;

    // Neural substrate optimization
    void optimizeNeuralSubstrate();
    void pruneInactiveConnections();
    void adaptActivationThresholds();
    void optimizeNeuralUtilization();
    void consolidateNeuralPatterns();
    float getNeuralUtilization() const;

    // Language processing optimization
    void optimizeLanguageProcessing();
    void optimizeTokenCaching();
    void optimizePatternPrecomputation();
    void optimizeCrossModalBindings();
    float getLanguageProcessingEfficiency() const;

    // Cache optimization
    void optimizeCacheLayout();
    void prefetchRelatedPatterns(const std::string& pattern);
    void optimizeMemoryAccessPatterns();
    void optimizeSequentialAccess();
    void implementPrefetching();
    void optimizeMemoryAlignment();
    void optimizeCacheLineUsage();
    void implementDataLocalityOptimization();
    void groupRelatedNeurons();
    void optimizeSynapseLayout();
    void separateHotColdData();

    // Batch processing optimization
    void enableBatchProcessing();
    void addToBatch(const BatchOperation& operation);
    void processBatch();
    void optimizeBatchSize();
    float getBatchProcessingEfficiency() const;

    // Real-time optimization
    void optimizeRealTimePerformance();
    void maintainTargetFrameRate();
    void adaptToProcessingLoad();
    void optimizeFrameTiming();
    float getCurrentFrameRate() const;
    float getAverageFrameTime() const;

    // Scalability optimization
    void optimizeForScale();
    void adaptToNeuralLoad();
    void optimizeResourceAllocation();
    void scaleProcessingCapacity();
    bool canHandleAdditionalLoad(std::size_t additional_neurons) const;

    // Performance monitoring
    void updatePerformanceMetrics();
    PerformanceMetrics getPerformanceMetrics() const;
    std::vector<PerformanceMetrics> getMetricsHistory(std::size_t count = 100) const;
    float calculateOverallPerformanceScore() const;
    std::string generatePerformanceReport() const;

    // Optimization execution
    void runOptimizationCycle();
    void runContinuousOptimization();
    void stopContinuousOptimization();
    bool isOptimizing() const { return is_optimizing_.load(); }

    // Worker thread management
    void scheduleTask(const std::string& task_type, int priority = 0);
    void scheduleHighPriorityTask(const std::string& task_type);
    void scheduleDelayedTask(const std::string& task_type, int delay_ms);

    // Profiling and analysis
    void enableProfiling(bool enable);
    void capturePerformanceSnapshot();
    void analyzePerformanceBottlenecks();
    std::vector<std::string> identifyOptimizationOpportunities() const;

    // State queries
    bool isInitialized() const { return is_initialized_.load(); }
    float getOptimizationEffectiveness() const;
    std::string getOptimizationStatus() const;

private:
    // Internal optimization methods
    void initializeWorkerThreads();
    void shutdownWorkerThreads();
    void workerThreadFunction(std::size_t thread_id);
    
    // Worker thread task management
    void scheduleBackgroundTasks();
    void performMemoryOptimization();
    void performCacheOptimization();
    void performNeuralOptimization();
    void performLanguageOptimization();
    void performSystemOptimization();
    void optimizeLanguageModelMemory();
    void adaptOptimizationStrategy();
    void adjustOptimizationFrequency();
    void processBatchTasks();
    std::string getNextTask();
    
    void updateMemoryMetrics();
    void updateComputationalMetrics();
    void updateNeuralMetrics();
    void updateLanguageMetrics();
    void updateBatchMetrics();
    void updateRealTimeMetrics();
    
    void applyConservativeOptimization();
    void applyBalancedOptimization();
    void applyAggressiveOptimization();
    void applyAdaptiveOptimization();
    
    OptimizationStrategy selectOptimalStrategy() const;
    void adjustOptimizationParameters();
    
    // Cache optimization helpers
    void evictLeastUsedPatterns();
    void updateTokenCache();
    void updatePatternCache();
    void evictLeastRecentlyUsed();
    
    // Memory pool management
    void expandMemoryPool();
    void compactMemoryPool();
    std::size_t findFreeBlock(std::size_t size);
    
    // Performance analysis
    float calculateMemoryEfficiency() const;
    float calculateComputationalEfficiency() const;
    float calculateNeuralEfficiency() const;
    float calculateLanguageEfficiency() const;
    
    // Profiling helper methods
    float getCurrentCPUUsage() const;
    std::size_t getContextSwitches() const;
    float calculateCacheHitRate() const;
    std::size_t getPageFaults() const;
    float getNeuronsProcessedPerSecond() const;
    float getSynapsesUpdatedPerSecond() const;
    float getAverageActivationLatency() const;
    float getAveragePropagationDelay() const;
    float getDiskReadsPerSecond() const;
    float getDiskWritesPerSecond() const;
    float getNetworkThroughput() const;

    // Utility methods
    std::chrono::steady_clock::time_point getCurrentTime() const;
    float calculateTimeDifference(const std::chrono::steady_clock::time_point& start,
                                 const std::chrono::steady_clock::time_point& end) const;
    void logOptimizationEvent(const std::string& event, const std::string& details = "");
};

} // namespace Core
} // namespace NeuroForge