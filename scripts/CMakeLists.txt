# CMakeLists.txt for NeuroForge benchmark scripts

cmake_minimum_required(VERSION 3.20)

# Set the project name
project(NeuroForgeBenchmark)

# Set C++ standard to match main project
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler flags for optimization and debugging
if(MSVC)
    add_compile_options(/W4 /permissive- /FS)
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "/Od /Zi /DDEBUG")
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
endif()

# Find required packages
find_package(Threads REQUIRED)

# Optionally find SQLite3 (same as main project)
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
    find_package(unofficial-sqlite3 CONFIG QUIET)
endif()

# Include directories
include_directories(../include)

# Only include source files that actually exist
add_executable(benchmark_performance
    benchmark_performance.cpp
    ../src/core/Logger.cpp
    ../src/core/CUDAAccel.cpp
    ../src/core/SubstratePerformanceOptimizer.cpp
    ../src/core/HypergraphBrain.cpp
    ../src/core/LanguageSystem.cpp
    ../src/core/LanguageSystem_Acoustic.cpp
    ../src/core/LanguageSystem_Visual.cpp
    ../src/core/LanguageSystem_TokenTracker.cpp
    ../src/core/AutonomousScheduler.cpp
    ../src/core/SubstrateTaskGenerator.cpp
    ../src/core/AutonomousLearningCycle.cpp
    ../src/core/IntrinsicMotivationSystem.cpp
    ../src/core/LearningSystem.cpp
    ../src/core/Neuron.cpp
    ../src/core/Synapse.cpp
    ../src/core/Region.cpp
    ../src/core/SubstrateLanguageIntegration.cpp
    ../src/core/SubstrateLanguageManager.cpp
    ../src/core/SubstrateLanguageAdapter.cpp
    ../src/core/NeuralLanguageBindings.cpp
    ../src/core/RegionRegistry.cpp
    ../src/core/MemoryDB.cpp
    ../src/memory/DevelopmentalConstraints.cpp
    ../src/connectivity/ConnectivityManager.cpp
    ../src/regions/CorticalRegions.cpp
    ../src/regions/SomatosensoryCortex.cpp
    ../src/regions/LimbicRegions.cpp
)

# Link libraries
target_link_libraries(benchmark_performance 
    Threads::Threads
)

# Link SQLite3 if available
if(SQLite3_FOUND)
    target_link_libraries(benchmark_performance SQLite::SQLite3)
    target_compile_definitions(benchmark_performance PRIVATE NF_HAVE_SQLITE3=1)
elseif(TARGET unofficial::sqlite3::sqlite3)
    target_link_libraries(benchmark_performance unofficial::sqlite3::sqlite3)
    target_compile_definitions(benchmark_performance PRIVATE NF_HAVE_SQLITE3=1)
endif()

# Set output directory
set_target_properties(benchmark_performance PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add compile definitions
target_compile_definitions(benchmark_performance PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# Platform-specific settings
if(WIN32)
    target_compile_definitions(benchmark_performance PRIVATE WIN32_LEAN_AND_MEAN)
    # Link Windows multimedia library for microphone capture
    target_link_libraries(benchmark_performance winmm)
endif()

if(UNIX)
    target_link_libraries(benchmark_performance pthread)
endif()

# Add custom target for running benchmark
add_custom_target(run_benchmark
    COMMAND benchmark_performance
    DEPENDS benchmark_performance
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "Running NeuroForge performance benchmark"
)

# Installation rules
install(TARGETS benchmark_performance
    RUNTIME DESTINATION bin
)

# Add a target for building all benchmarks
add_custom_target(benchmarks ALL
    DEPENDS benchmark_performance
)
