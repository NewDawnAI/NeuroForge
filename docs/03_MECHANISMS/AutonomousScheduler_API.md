# Autonomous Scheduler API Documentation

## Overview

The Autonomous Scheduler is a core component of the NeuroForge HypergraphBrain that enables autonomous task execution, goal-oriented behavior, and self-directed learning. It integrates with brain regions to provide intelligent task scheduling, priority management, and adaptive behavior.

## Core Components

### AutonomousScheduler Class

The main scheduler class that manages task queues, execution priorities, and performance statistics.

#### Key Features
- **Priority-based task execution**: Tasks are executed based on their priority levels (High, Medium, Low)
- **Adaptive scheduling**: Dynamic adjustment of task execution based on performance metrics
- **Thread-safe operations**: Concurrent access protection for multi-threaded environments
- **Performance monitoring**: Comprehensive statistics tracking for optimization

### Task Types

#### GoalTask
Represents goal-oriented tasks that drive autonomous behavior.

```cpp
class GoalTask : public Task {
public:
    GoalTask(std::size_t id, const std::string& name, const std::string& goal_type);
    
    // Goal-specific methods
    void setGoalParameters(const std::vector<float>& parameters);
    void setSuccessThreshold(float threshold);
    std::string getGoalType() const;
    std::vector<float> getGoalParameters() const;
    float getSuccessThreshold() const;
};
```

**Goal Types:**
- `"exploration"`: Encourages exploration of new states and actions
- `"achievement"`: Focuses on achieving specific objectives
- `"maintenance"`: Maintains system stability and performance
- `"learning"`: Promotes learning and skill acquisition

#### ReflectionTask
Handles introspective and metacognitive processes.

```cpp
class ReflectionTask : public Task {
public:
    ReflectionTask(std::size_t id, const std::string& name, const std::string& reflection_type);
    
    // Reflection-specific methods
    std::string getReflectionType() const;
};
```

**Reflection Types:**
- `"simple"`: Basic self-assessment and state evaluation
- `"comprehensive"`: Deep introspection across multiple aspects
- `"metacognitive"`: Analysis of cognitive processes and strategies

## HypergraphBrain Integration

### Initialization

```cpp
// Initialize the autonomous scheduler
bool success = brain->initializeAutonomousScheduler();
if (!success) {
    // Handle initialization failure
}
```

### Configuration

```cpp
// Enable autonomous mode
brain->setAutonomousModeEnabled(true);

// Check autonomous mode status
bool is_enabled = brain->isAutonomousModeEnabled();
```

### Task Management

#### Adding Tasks

```cpp
// Create and add a goal task
auto goal_task = std::make_shared<GoalTask>(1, "exploration_goal", "exploration");
goal_task->setPriority(TaskPriority::High);
goal_task->setGoalParameters({0.5f, 0.8f, 0.3f});
goal_task->setSuccessThreshold(0.7f);
brain->addAutonomousTask(goal_task);

// Create and add a reflection task
auto reflection_task = std::make_shared<ReflectionTask>(2, "self_assessment", "comprehensive");
reflection_task->setPriority(TaskPriority::Medium);
brain->addAutonomousTask(reflection_task);
```

#### Executing Tasks

```cpp
// Execute a single autonomous cycle
float delta_time = 0.1f; // 100ms
brain->executeAutonomousCycle(delta_time);

// Run continuous autonomous loop
std::size_t max_iterations = 1000; // 0 for infinite
float target_frequency = 10.0f;    // 10 Hz
brain->runAutonomousLoop(max_iterations, target_frequency);
```

### Performance Monitoring

```cpp
// Get scheduler statistics
auto stats = brain->getAutonomousStatistics();

std::cout << "Total tasks executed: " << stats.total_tasks_executed << std::endl;
std::cout << "Active tasks: " << stats.active_tasks << std::endl;
std::cout << "Completed tasks: " << stats.completed_tasks << std::endl;
std::cout << "Failed tasks: " << stats.failed_tasks << std::endl;
std::cout << "Average execution time: " << stats.average_execution_time << "s" << std::endl;
std::cout << "Current load: " << stats.current_load << std::endl;
```

## Brain Region Integration

The autonomous scheduler integrates with key brain regions to enable intelligent behavior:

### SelfNode Integration
- **Goal Selection**: Initiates reflection processes for autonomous goal selection
- **Self-Awareness**: Updates self-representation based on task outcomes
- **Introspection**: Periodic comprehensive self-reflection across multiple aspects

```cpp
// Example: Triggering goal selection through SelfNode
auto self_node = brain->getRegion("SelfNode");
if (self_node) {
    auto self_node_limbic = std::dynamic_pointer_cast<Regions::SelfNode>(self_node);
    if (self_node_limbic) {
        self_node_limbic->initiateReflection("autonomous_goal_selection", 
            {Regions::SelfNode::SelfAspect::Cognitive, Regions::SelfNode::SelfAspect::Temporal});
    }
}
```

### PrefrontalCortex Integration
- **Decision Making**: Generates decisions based on current context and options
- **Planning**: Stores planning information in working memory
- **Executive Control**: Manages high-level cognitive processes

```cpp
// Example: Planning through PrefrontalCortex
auto prefrontal_cortex = brain->getRegion("PrefrontalCortex");
if (prefrontal_cortex) {
    auto pfc = std::dynamic_pointer_cast<Regions::PrefrontalCortex>(prefrontal_cortex);
    if (pfc) {
        std::vector<float> options = {0.2f, 0.5f, 0.8f, 0.3f};
        std::vector<float> values = {0.6f, 0.9f, 0.4f, 0.7f};
        auto decision = pfc->makeDecision(options, values);
        
        // Store planning context
        std::vector<float> planning_context = {
            temporal_context,
            current_activation,
            static_cast<float>(decision.selected_option),
            decision.confidence
        };
        pfc->storeInWorkingMemory(planning_context);
    }
}
```

### MotorCortex Integration
- **Action Execution**: Plans and executes motor actions based on goals
- **Movement Planning**: Coordinates body movements for goal achievement
- **Motor Commands**: Executes planned movements

```cpp
// Example: Action execution through MotorCortex
auto motor_cortex = brain->getRegion("MotorCortex");
if (motor_cortex) {
    auto mc = std::dynamic_pointer_cast<Regions::MotorCortex>(motor_cortex);
    if (mc) {
        std::vector<float> movement_vector = {0.1f, 0.0f, 0.2f};
        mc->planMovement(Regions::MotorCortex::BodyPart::Arms, movement_vector, 0.5f);
        mc->executeMotorCommands();
    }
}
```

## Autonomous Loop Architecture

The autonomous loop implements a complete cognitive cycle:

1. **Goal Selection** (SelfNode): Determine current objectives and priorities
2. **Planning** (PrefrontalCortex): Generate action plans and strategies
3. **Action Execution** (MotorCortex): Execute planned actions
4. **Scheduler Execution**: Process autonomous tasks
5. **Brain Processing**: Execute regular neural processing
6. **Periodic Introspection**: Comprehensive self-reflection and assessment
7. **Adaptive Goal Generation**: Create new goals based on performance

### Timing and Frequency

- **Target Frequency**: Configurable execution frequency (default: 10 Hz)
- **Introspection Interval**: Periodic deep reflection (default: every 10 seconds)
- **Adaptive Intervals**: Dynamic goal generation based on performance

## Best Practices

### Task Design
1. **Clear Objectives**: Define specific, measurable task goals
2. **Appropriate Priorities**: Use priority levels to manage execution order
3. **Resource Awareness**: Consider computational cost and resource usage
4. **Error Handling**: Implement robust error handling and recovery

### Performance Optimization
1. **Batch Operations**: Group related tasks for efficient execution
2. **Load Balancing**: Monitor system load and adjust task frequency
3. **Memory Management**: Use smart pointers and RAII for resource management
4. **Thread Safety**: Ensure thread-safe operations in concurrent environments

### Integration Patterns
1. **Region Coordination**: Coordinate between brain regions for coherent behavior
2. **State Management**: Maintain consistent state across components
3. **Event-Driven Architecture**: Use callbacks and events for loose coupling
4. **Monitoring and Logging**: Implement comprehensive monitoring for debugging

## Error Handling

The scheduler provides robust error handling:

```cpp
// Check scheduler availability
auto scheduler = brain->getAutonomousScheduler();
if (!scheduler) {
    // Scheduler not initialized
    return false;
}

// Handle task execution errors
try {
    brain->executeAutonomousCycle(delta_time);
} catch (const std::exception& e) {
    // Log error and continue
    std::cerr << "Autonomous cycle error: " << e.what() << std::endl;
}
```

## Thread Safety

All scheduler operations are thread-safe:

- **Mutex Protection**: Critical sections protected by mutexes
- **Atomic Operations**: Use of atomic variables for flags and counters
- **Lock-Free Queues**: Efficient task queue implementations
- **RAII**: Resource management through RAII principles

## Future Extensions

Planned enhancements include:

1. **Dynamic Priority Adjustment**: Automatic priority adjustment based on performance
2. **Task Dependencies**: Support for task dependency graphs
3. **Resource Constraints**: Resource-aware task scheduling
4. **Learning Integration**: Integration with learning systems for adaptive behavior
5. **Distributed Scheduling**: Support for distributed task execution

## Examples

See the test files for comprehensive usage examples:
- `tests/test_autonomous_scheduler.cpp`: Unit tests and usage patterns
- `tests/test_m7_acceptance.cpp`: Integration tests with brain regions

## API Reference

For detailed API documentation, see the header files:
- `include/core/AutonomousScheduler.h`: Core scheduler interface
- `include/core/HypergraphBrain.h`: Brain integration methods