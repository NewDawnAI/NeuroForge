cmake_minimum_required(VERSION 3.20)
project(NeuroForge VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Option to enable/disable tests (default ON for CI friendliness)
option(NEUROFORGE_ENABLE_TESTS "Build and enable NeuroForge tests" ON)

# New options for GPU stubs and vision demo defaults
option(NEUROFORGE_ENABLE_GPU "Enable GPU stubs and flags (no CUDA linkage yet)" OFF)
option(NEUROFORGE_ENABLE_VISION_DEMO "Enable vision demo code path by default" OFF)
option(NEUROFORGE_WITH_VIEWER "Build NeuroForge OpenGL viewer (requires GLFW + GLAD)" OFF)
option(ENABLE_CUDA "Enable CUDA acceleration (optional)" OFF)

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4 /permissive- /FS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Include directories
# Expose public headers under include/ and local module headers under src/
include_directories(include)
include_directories(src)

# Find required packages
find_package(Threads REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)

# Enable Cap'n Proto for boundary serialization (optional)
find_package(CapnProto CONFIG QUIET)
if(CapnProto_FOUND)
    message(STATUS "Cap'n Proto found: ${CapnProto_VERSION}")
else()
    message(STATUS "Cap'n Proto NOT found - substrate serialization disabled")
endif()

# Optionally find OpenCV for vision demo and GPU metrics (quietly)
# Hints for Windows users: try to auto-detect under C:\opencv and honor env vars before find_package
if(WIN32)
    # If user has OpenCV_DIR/OPENCV_DIR/OPENCV_ROOT in env, prefer those
    if(NOT DEFINED OpenCV_DIR)
        if(DEFINED ENV{OpenCV_DIR})
            set(OpenCV_DIR $ENV{OpenCV_DIR})
        elseif(DEFINED ENV{OPENCV_DIR})
            set(OpenCV_DIR $ENV{OPENCV_DIR})
        endif()
    endif()

    # If still not set, allow an overrideable root (defaults to C:/opencv)
    if(NOT DEFINED OpenCV_DIR)
        set(OPENCV_ROOT_HINT "C:/opencv" CACHE PATH "Root directory of your OpenCV installation")
        if(EXISTS "${OPENCV_ROOT_HINT}")
            set(_OpenCV_HINT_DIRS
                "${OPENCV_ROOT_HINT}"
                "${OPENCV_ROOT_HINT}/build"
                "${OPENCV_ROOT_HINT}/build/x64/vc17/lib"
                "${OPENCV_ROOT_HINT}/build/x64/vc16/lib"
                "${OPENCV_ROOT_HINT}/x64/vc17/lib"
                "${OPENCV_ROOT_HINT}/x64/vc16/lib"
            )
            foreach(_p IN LISTS _OpenCV_HINT_DIRS)
                if(EXISTS "${_p}/OpenCVConfig.cmake")
                    set(OpenCV_DIR "${_p}" CACHE PATH "Directory containing OpenCVConfig.cmake" FORCE)
                    break()
                elseif(EXISTS "${_p}/opencv4/OpenCVConfig.cmake")
                    set(OpenCV_DIR "${_p}/opencv4" CACHE PATH "Directory containing OpenCVConfig.cmake" FORCE)
                    break()
                endif()
            endforeach()
        endif()
    endif()
endif()

find_package(OpenCV QUIET)
if(OpenCV_FOUND)
    message(STATUS "Found OpenCV ${OpenCV_VERSION}")
endif()

# Core library sources
set(CORE_SOURCES
    src/core/Neuron.cpp
    src/core/Synapse.cpp
    src/core/Region.cpp
    src/core/SelfModel.cpp
    src/core/AutonomyEnvelope.cpp
    src/core/StageC_AutonomyGate.cpp
    src/core/HypergraphBrain.cpp
    src/core/LearningSystem.cpp
    src/core/Logger.cpp
    src/core/CUDAAccel.cpp
    src/core/MemoryDB.cpp
    src/core/RegionRegistry.cpp
    src/core/AutonomousScheduler.cpp
    src/core/SubstratePhaseC.cpp
    src/core/SubstratePhaseCAdapter.cpp
    src/core/SubstrateTaskGenerator.cpp
    src/core/SubstrateLanguageManager.cpp
    src/core/SubstrateLanguageAdapter.cpp
    src/core/SubstrateLanguageIntegration.cpp
    src/core/NeuralLanguageBindings.cpp
    src/core/SubstrateWorkingMemory.cpp
    src/core/SubstratePerformanceOptimizer.cpp
    src/core/PhaseC.cpp
    src/core/AutonomousLearningCycle.cpp
    src/core/IntrinsicMotivationSystem.cpp
    src/core/LanguageSystem.cpp
    src/core/LanguageSystem_Visual.cpp
    src/core/LanguageSystem_Acoustic.cpp
    src/core/LanguageSystem_SpeechProduction.cpp
    src/core/LanguageSystem_TokenTracker.cpp
    # src/core/LanguageSystem_TokenTracker_Simple.cpp  # Disabled to use full implementation
    src/core/PhaseAMimicry.cpp
    src/core/FirstPersonMazeRenderer.cpp
    src/core/ContextHooks.cpp
    src/core/RelationGate.cpp
    src/core/EpistemicGates.cpp
    src/core/EntityResolver.cpp
    src/core/BrainPersistence.cpp
    src/biases/AttachmentBias.cpp
    src/biases/ContrastEdgeBias.cpp
    src/biases/FaceDetectionBias.cpp
    src/biases/MotionBias.cpp
    src/biases/NoveltyBias.cpp
    src/biases/SurvivalBias.cpp
    src/biases/SocialPerceptionBias.cpp
    src/biases/SpatialNavigationBias.cpp
    src/biases/TemporalBias.cpp
    src/biases/VoiceBias.cpp
    src/audio_capture.cpp
    src/system_audio_capture.cpp
    src/core/Phase6Reasoner.cpp
    src/core/Phase7AffectiveState.cpp
    src/core/Phase7Reflection.cpp
    src/core/Phase8GoalSystem.cpp
    src/core/metrics/IdentityMonitor.cpp
    src/core/metrics/IntegrationHeuristics.cpp
    src/core/HardwareAdapter.cpp
    src/core/HiveManager.cpp
    # Newly added Phase 9–11 sources
    src/core/Phase9Metacognition.cpp
    src/core/Phase10SelfExplanation.cpp
    src/core/Phase11SelfRevision.cpp
    src/core/Phase12Consistency.cpp
    src/core/Phase13AutonomyEnvelope.cpp
    src/core/Phase14MetaReasoner.cpp
    src/core/Phase15EthicsRegulator.cpp
    src/core/MetaCognitionSystem.cpp
    src/core/ActionFilter.cpp
    src/core/UniversalLearningSignal.cpp
    src/core/CompositionMetrics.cpp
    src/core/Agent2EpistemicReviewer.cpp
    src/core/ReasoningStep.cpp
    src/core/ReasoningTrace.cpp
    src/core/Curiosity/ResolutionTracker.cpp
    # Sandbox implementation (Windows-only APIs guarded at compile-time)
    src/sandbox/WebSandbox.cpp
    # Perception pipeline for autonomous grounding
    src/perception/LivePerceptionLoop.cpp
    # Semantic projection for JEPA-safe language integration
    src/perception/world/SemanticProjection.cpp
    # Visual Perception for browsing
    src/vision/BrowserVision.cpp
    # Navigation for autonomous exploration
    src/navigation/CuriosityNavigator.cpp
    # Verification for anti-hallucination
    src/verification/GroundingVerifier.cpp
    # Language acquisition from web content
    src/language/LanguageAcquisitionLoop.cpp
    # Memory systems (episodic, semantic, procedural)
    src/memory/WorkingMemory.cpp
    src/memory/EpisodicMemoryManager.cpp
    src/memory/SemanticMemory.cpp
    src/memory/ProceduralMemory.cpp
    src/memory/MemoryIntegrator.cpp
    src/memory/SleepConsolidation.cpp
    src/memory/DreamProcessor.cpp
    src/memory/DevelopmentalConstraints.cpp
    src/runtime/UnifiedBoundedRunner.cpp
    src/runtime/N7ModelBasedRunner.cpp
    src/runtime/N7MinimalRunner.cpp
    src/runtime/N7BoundedRunner.cpp
    src/runtime/N8HumanGatedRunner.cpp
    src/runtime/N9AutomatedGatedRunner.cpp
    src/runtime/UnifiedAliveRunner.cpp
    src/runtime/WebSandboxRunner.cpp
    src/runtime/GrandUnifiedRunner.cpp
    src/encoders/TextEncoder.cpp
    src/runtime/SemanticN4Runner.cpp
    src/runtime/SemanticN5Runner.cpp
    src/runtime/SemanticN6Runner.cpp
    src/runtime/SemanticInjectionRunner.cpp
    src/runtime/SemanticN2Runner.cpp
    src/runtime/SemanticN3Runner.cpp
)

# Brain region sources
set(REGION_SOURCES
    src/regions/CorticalRegions.cpp
    src/regions/SubcorticalRegions.cpp
    src/regions/LimbicRegions.cpp
    src/regions/PhaseARegion.cpp
    src/regions/SomatosensoryCortex.cpp
)

# Phase 2 memory sources (ensure implementations are linked into core)
option(NEUROFORGE_SKIP_DEV_CONSTRAINTS "Skip compiling DevelopmentalConstraints.cpp" OFF)
set(MEMORY_SOURCES
    src/memory/WorkingMemory.cpp
    src/memory/ProceduralMemory.cpp
    src/memory/MemoryIntegrator.cpp
    src/memory/EpisodicMemoryManager.cpp
    src/memory/SemanticMemory.cpp
    src/memory/SleepConsolidation.cpp
)
if(NOT NEUROFORGE_SKIP_DEV_CONSTRAINTS)
    list(APPEND MEMORY_SOURCES src/memory/DevelopmentalConstraints.cpp)
else()
    message(WARNING "Skipping DevelopmentalConstraints.cpp for current build (NEUROFORGE_SKIP_DEV_CONSTRAINTS=ON)")
endif()

# Connectivity sources
set(CONNECTIVITY_SOURCES
    src/connectivity/ConnectivityManager.cpp
)

# Encoder sources
set(ENCODER_SOURCES
    src/encoders/VisionEncoder.cpp
    src/encoders/AudioEncoder.cpp
    src/encoders/EncoderAdapter.cpp
)

# Create core library (include Phase 2 memory sources)
add_library(neuroforge_core STATIC ${CORE_SOURCES} ${MEMORY_SOURCES})
# Generated headers are under the binary dir (and typically in a 'schemas' subdir); expose publicly
target_include_directories(neuroforge_core PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
                                                   ${CMAKE_CURRENT_BINARY_DIR}/schemas)
target_include_directories(neuroforge_core PUBLIC include)
target_link_libraries(neuroforge_core PUBLIC Threads::Threads Eigen3::Eigen)

# Cap'n Proto integration (enabled when found)
if(CapnProto_FOUND)
    target_include_directories(neuroforge_core PUBLIC ${CMAKE_SOURCE_DIR}/include/serialization)
    target_link_libraries(neuroforge_core PUBLIC CapnProto::capnp)
    target_compile_definitions(neuroforge_core PUBLIC NF_HAVE_CAPNP=1)
    message(STATUS "Cap'n Proto substrate serialization ENABLED")
endif()

# Platform-specific audio capture dependencies
if(UNIX AND NOT APPLE)
    # Linux: use ALSA (libasound2-dev required)
    find_package(ALSA QUIET)
    if(ALSA_FOUND)
        target_link_libraries(neuroforge_core PUBLIC ${ALSA_LIBRARIES})
        target_include_directories(neuroforge_core PRIVATE ${ALSA_INCLUDE_DIRS})
        message(STATUS "ALSA audio capture enabled")
    else()
        message(STATUS "ALSA not found - audio capture disabled on Linux")
    endif()
elseif(APPLE)
    # macOS: use CoreAudio framework
    find_library(AUDIO_TOOLBOX AudioToolbox)
    find_library(CORE_AUDIO CoreAudio)
    if(AUDIO_TOOLBOX AND CORE_AUDIO)
        target_link_libraries(neuroforge_core PUBLIC ${AUDIO_TOOLBOX} ${CORE_AUDIO})
        message(STATUS "CoreAudio capture enabled")
    else()
        message(STATUS "CoreAudio not found - audio capture disabled on macOS")
    endif()
endif()

# Optional CUDA kernels (enabled only when toolchain provides CUDA)
if(ENABLE_CUDA)
    # Try to locate NVCC if CMake didn't auto-detect it
    if(NOT CMAKE_CUDA_COMPILER)
        find_program(NVCC_EXECUTABLE nvcc
            HINTS ENV PATH
            PATHS
                "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.4/bin"
                "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8/bin"
        )
        if(NVCC_EXECUTABLE)
            set(CMAKE_CUDA_COMPILER ${NVCC_EXECUTABLE} CACHE FILEPATH "Path to NVCC" FORCE)
        else()
            message(WARNING "ENABLE_CUDA is ON but no CUDA toolset found (nvcc missing). Building CPU-only. Install CUDA Toolkit or pass -DCMAKE_CUDA_COMPILER=<path to nvcc>.")
            set(ENABLE_CUDA OFF)
        endif()
    endif()
    # On Windows, NVCC requires MSVC 'cl.exe' as host compiler. Detect it and fallback if absent.
    if(WIN32 AND ENABLE_CUDA)
        # If using a Visual Studio generator, VS provides the host toolchain; no PATH check needed.
        if(NOT CMAKE_GENERATOR MATCHES "Visual Studio")
            find_program(MSVC_CL cl HINTS ENV PATH)
            if(NOT MSVC_CL)
                message(WARNING "CUDA on Windows with non-VS generators requires MSVC 'cl.exe' in PATH. Building CPU-only. Install Visual Studio 2022 C++ Build Tools or use -G \"Visual Studio 17 2022\" -T cuda=<ver>.")
                set(ENABLE_CUDA OFF)
            endif()
        endif()
    endif()
endif()

if(ENABLE_CUDA)
    enable_language(CUDA)
    # Architectures: RTX 20xx/30xx (75,86). Extend as needed.
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_ARCHITECTURES "75;86")

    # Manually compile kernels.cu with nvcc to avoid problematic MSVC PDB flags
    set(NF_NVCC "${CMAKE_CUDA_COMPILER}")
    if(NOT EXISTS "${NF_NVCC}")
        find_program(NF_NVCC nvcc HINTS ENV CUDA_PATH PATHS "$ENV{CUDA_PATH}/bin")
    endif()
    if(NOT NF_NVCC)
        message(FATAL_ERROR "nvcc not found; set CMAKE_CUDA_COMPILER or CUDA_PATH")
    endif()
    set(NF_CUDA_INCLUDE "$ENV{CUDA_PATH}/include")
    set(NF_KERNEL_SRC "${CMAKE_CURRENT_SOURCE_DIR}/src/cuda/kernels.cu")
    set(NF_KERNEL_OBJ "${CMAKE_CURRENT_BINARY_DIR}/kernels.obj")
    add_custom_command(
        OUTPUT ${NF_KERNEL_OBJ}
        COMMAND "${NF_NVCC}" -c -std=c++17
                -I"${CMAKE_CURRENT_SOURCE_DIR}/include"
                -I"${CMAKE_CURRENT_SOURCE_DIR}/src"
                -I"${NF_CUDA_INCLUDE}"
                "--generate-code=arch=compute_75,code=sm_75"
                "--generate-code=arch=compute_86,code=sm_86"
                -Xcompiler="/EHsc /O2 /MD /W4 /permissive- /FS"
                -o "${NF_KERNEL_OBJ}" "${NF_KERNEL_SRC}"
        DEPENDS ${NF_KERNEL_SRC}
        COMMENT "Compiling CUDA kernels with nvcc"
    )
    add_library(neuroforge_cuda STATIC ${NF_KERNEL_OBJ})
    set_target_properties(neuroforge_cuda PROPERTIES LINKER_LANGUAGE CXX)
    target_include_directories(neuroforge_cuda PUBLIC include)
    # Propagate CUDA availability define
    target_compile_definitions(neuroforge_cuda PUBLIC NF_HAVE_CUDA=1)
    target_compile_definitions(neuroforge_core PUBLIC NF_HAVE_CUDA=1)
    # Link CUDA runtime if available, and make CUDA library transitively available to core
    find_package(CUDAToolkit QUIET)
    if(CUDAToolkit_FOUND)
        target_link_libraries(neuroforge_cuda PUBLIC CUDA::cudart)
        target_link_libraries(neuroforge_core PUBLIC CUDA::cudart neuroforge_cuda)
        if(WIN32)
            # Attempt to deploy cudart runtime DLL next to key executables
            if(DEFINED ENV{CUDA_PATH})
                set(_CUDA_BIN "$ENV{CUDA_PATH}/bin")
                # CUDA 13 runtime DLL name
                set(_CUDA_RT_DLL "${_CUDA_BIN}/cudart64_130.dll")
                if(EXISTS "${_CUDA_RT_DLL}")
                    foreach(_exe_target IN ITEMS neuroforge phase5_optimized_demo neuroforge_tests unified_substrate_demo)
                        if(TARGET ${_exe_target})
                            add_custom_command(TARGET ${_exe_target} POST_BUILD
                                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${_CUDA_RT_DLL}"
                                $<TARGET_FILE_DIR:${_exe_target}>
                                COMMENT "Deploying cudart64_130.dll for ${_exe_target}")
                        endif()
                    endforeach()
                else()
                    message(WARNING "CUDA runtime DLL not found at ${_CUDA_RT_DLL}; ensure CUDA_PATH is set. Runtime may rely on system PATH for cudart")
                endif()
            endif()
        endif()
    else()
        # Fallback: still link the CUDA kernels into executables explicitly
        message(STATUS "CUDAToolkit target not found; ensure executables link neuroforge_cuda when using GPU")
        target_link_libraries(neuroforge_core PUBLIC neuroforge_cuda)
    endif()
endif()

# If OpenCV is available, expose to core for optional GPU metrics via OpenCV CUDA APIs
if(OpenCV_FOUND)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_link_libraries(neuroforge_core PUBLIC ${OpenCV_LIBS})
        target_compile_definitions(neuroforge_core PUBLIC NF_HAVE_OPENCV=1)
    else()
        message(WARNING "OpenCV found but non-MSVC toolchain detected; skipping OpenCV linkage/defines for neuroforge_core to avoid ABI mismatch.")
    endif()
endif()

# Propagate GPU stub define to core (and regions) if enabled
if(NEUROFORGE_ENABLE_GPU)
    target_compile_definitions(neuroforge_core PUBLIC NF_ENABLE_GPU=1)
endif()

# Create regions library
add_library(neuroforge_regions STATIC ${REGION_SOURCES})
target_include_directories(neuroforge_regions PUBLIC include)
target_link_libraries(neuroforge_regions PUBLIC neuroforge_core)

# Create connectivity library
add_library(neuroforge_connectivity STATIC ${CONNECTIVITY_SOURCES})

# Create encoders library
add_library(neuroforge_encoders STATIC ${ENCODER_SOURCES})
target_include_directories(neuroforge_encoders PUBLIC include)
target_include_directories(neuroforge_connectivity PUBLIC include)
target_link_libraries(neuroforge_connectivity PUBLIC neuroforge_core neuroforge_regions)

# Main executable
add_executable(neuroforge src/main.cpp)

# Unified Perception Demo (WorldModelCortex + VocalMotorCortex)
add_executable(unified_perception_demo src/unified_perception_demo.cpp)
target_link_libraries(unified_perception_demo PRIVATE neuroforge_core neuroforge_regions neuroforge_connectivity neuroforge_encoders)
if(WIN32)
    target_link_libraries(unified_perception_demo PRIVATE winmm user32 gdi32 ole32)
endif()
# Link OpenCV to main too, if present (for vision capture/processing)
if(OpenCV_FOUND)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_link_libraries(neuroforge PRIVATE ${OpenCV_LIBS})
        target_compile_definitions(neuroforge PRIVATE NF_HAVE_OPENCV=1)
    endif()
endif()

target_link_libraries(neuroforge PRIVATE neuroforge_core neuroforge_regions neuroforge_connectivity neuroforge_encoders)
if(ENABLE_CUDA)
    target_link_libraries(neuroforge PRIVATE neuroforge_cuda)
endif()

# Link Windows multimedia library for microphone capture
if(WIN32)
    # Link necessary Windows system libraries for audio (winmm/WASAPI) and screen capture (GDI/User)
    # Include Shcore for DPI APIs used in the sandbox window
    target_link_libraries(neuroforge PRIVATE winmm user32 gdi32 ole32 avrt mmdevapi Shcore)
endif()

# Optional: Edge WebView2 (via vcpkg 'unofficial-webview2') for embedded browser sandbox
# NOTE: WebView2 is COM-based and only works with MSVC; skip when using GCC/MinGW
if(WIN32 AND MSVC)
    # Hint to find local vcpkg packages if toolchain is not used
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg/installed/x64-windows")
    
    find_package(unofficial-webview2 CONFIG QUIET)
    if(TARGET unofficial::webview2::webview2)
        # Propagate headers/defines to both core (where WebSandbox.cpp lives) and main
        target_link_libraries(neuroforge_core PRIVATE unofficial::webview2::webview2)
        target_link_libraries(neuroforge PRIVATE unofficial::webview2::webview2)
        target_compile_definitions(neuroforge_core PUBLIC NF_HAVE_WEBVIEW2=1)
        target_compile_definitions(neuroforge PRIVATE NF_HAVE_WEBVIEW2=1)
        message(STATUS "WebView2 found: enabling embedded browser sandbox")
    else()
        message(STATUS "WebView2 not found: sandbox will use plain window fallback")
    endif()
elseif(WIN32)
    message(STATUS "WebView2 skipped (MinGW build): sandbox will use plain window fallback")
endif()

# Default vision demo flag for the main executable
if(NEUROFORGE_ENABLE_VISION_DEMO)
    target_compile_definitions(neuroforge PRIVATE NF_ENABLE_VISION_DEMO=1)
endif()

# Also expose GPU stub flag to the main executable for conditional messages
if(NEUROFORGE_ENABLE_GPU)
    target_compile_definitions(neuroforge PRIVATE NF_ENABLE_GPU=1)
endif()

# Enable viewer by default for complete build
option(NEUROFORGE_WITH_VIEWER "Build 3D viewer" ON)

# Define NDEBUG in Release builds to strip assertions and reduce overhead
target_compile_definitions(neuroforge_core PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_compile_definitions(neuroforge_regions PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_compile_definitions(neuroforge_connectivity PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_compile_definitions(neuroforge_encoders PRIVATE $<$<CONFIG:Release>:NDEBUG>)
target_compile_definitions(neuroforge PRIVATE $<$<CONFIG:Release>:NDEBUG>)

# Detect if the OpenCV libraries include CUDA modules; if so, define NF_HAVE_OPENCV_CUDA
if(OpenCV_FOUND)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        # Collapse library list to a single string for simple pattern matching
        string(JOIN ";" _ocv_libs "${OpenCV_LIBS}")
        if(_ocv_libs MATCHES "cudaarithm|cudaimgproc|cudawarping|cudafilters")
            message(STATUS "OpenCV CUDA modules detected; enabling NF_HAVE_OPENCV_CUDA")
            target_compile_definitions(neuroforge_core PUBLIC NF_HAVE_OPENCV_CUDA=1)
            target_compile_definitions(neuroforge PRIVATE NF_HAVE_OPENCV_CUDA=1)
        endif()
    else()
        message(WARNING "Skipping OpenCV CUDA defines under non-MSVC toolchain.")
    endif()
endif()

# Pre-detect SQLite availability for gating SQLite-dependent tests
set(NF_SQLITE_AVAILABLE 0)
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    set(NF_SQLITE_AVAILABLE 1)
else()
    find_package(unofficial-sqlite3 CONFIG QUIET)
    if(TARGET unofficial::sqlite3::sqlite3)
        set(NF_SQLITE_AVAILABLE 1)
    endif()
endif()

# Optional: Wire SQLite3 into core for MemoryDB persistence (debugging/explainability)
if(NF_SQLITE_AVAILABLE)
    if(SQLite3_FOUND)
        target_link_libraries(neuroforge_core PUBLIC SQLite::SQLite3)
    elseif(TARGET unofficial::sqlite3::sqlite3)
        target_link_libraries(neuroforge_core PUBLIC unofficial::sqlite3::sqlite3)
    endif()
    target_compile_definitions(neuroforge_core PUBLIC NF_HAVE_SQLITE3=1)
    target_compile_definitions(neuroforge PRIVATE NF_HAVE_SQLITE3=1)
    message(STATUS "SQLite3 enabled: MemoryDB persistence available (optional — for debugging/explainability)")
endif()

# Tests (optional)
if(NEUROFORGE_ENABLE_TESTS)
    enable_testing()

    # Tests from 'tests' directory if present
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        # Build a focused minimal test runner to avoid multiple 'main' collisions
        set(FOCUSED_TEST_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_autonomous_scheduler.cpp)
        if(EXISTS ${FOCUSED_TEST_SOURCE})
            add_executable(neuroforge_tests ${FOCUSED_TEST_SOURCE})
            target_link_libraries(neuroforge_tests PRIVATE neuroforge_core neuroforge_regions neuroforge_connectivity neuroforge_encoders)
            # Ensure tests can include headers under src/ as some tests use e.g. biases/*.h living in src
            target_include_directories(neuroforge_tests PRIVATE include src)
            # Define minimal test framework to avoid hard dependency on GTest for files supporting it
            target_compile_definitions(neuroforge_tests PRIVATE MINIMAL_TEST_FRAMEWORK=1)
            add_test(NAME unit_tests COMMAND neuroforge_tests)
        else()
            message(STATUS "Skipping neuroforge_tests: focused test source not found")
        endif()
    endif()

    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_phase15_ethics.cpp)
        add_executable(test_phase15 tests/test_phase15_ethics.cpp)
        target_link_libraries(test_phase15 PRIVATE neuroforge_core neuroforge_regions neuroforge_connectivity neuroforge_encoders)
        target_include_directories(test_phase15 PRIVATE include src)
        target_compile_definitions(test_phase15 PRIVATE MINIMAL_TEST_FRAMEWORK=1)
        add_test(NAME phase15_ethics COMMAND test_phase15)
    endif()

    # Test executables
    add_executable(test_learning src/test_learning.cpp)
    target_link_libraries(test_learning 
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_learning PRIVATE include)
    target_compile_definitions(test_learning PRIVATE NF_LEARNING_TEST_MAIN)
    # Register test_learning with CTest
    add_test(NAME test_learning COMMAND $<TARGET_FILE:test_learning>)

# New: Integration Heuristics unit tests
add_executable(test_integration_heuristics tests/test_integration_heuristics.cpp)
target_link_libraries(test_integration_heuristics 
    neuroforge_core 
    neuroforge_regions 
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(test_integration_heuristics PRIVATE include)
add_test(NAME test_integration_heuristics COMMAND $<TARGET_FILE:test_integration_heuristics>)

# Referential continuity (entity normalization + coreference) tests
add_executable(test_referential_continuity tests/test_referential_continuity.cpp)
target_link_libraries(test_referential_continuity
    neuroforge_core
    neuroforge_regions
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(test_referential_continuity PRIVATE include src)
target_compile_definitions(test_referential_continuity PRIVATE MINIMAL_TEST_FRAMEWORK=1)
add_test(NAME test_referential_continuity COMMAND $<TARGET_FILE:test_referential_continuity>)

add_executable(test_worldmodel_regressions tests/test_worldmodel_regressions.cpp)
target_link_libraries(test_worldmodel_regressions
    neuroforge_core
    neuroforge_regions
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(test_worldmodel_regressions PRIVATE include src)
add_test(NAME test_worldmodel_regressions COMMAND $<TARGET_FILE:test_worldmodel_regressions>)

# Cross-page entity linking tests
add_executable(test_cross_page_entity_linking tests/test_cross_page_entity_linking.cpp)
target_link_libraries(test_cross_page_entity_linking
    neuroforge_core
    neuroforge_regions
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(test_cross_page_entity_linking PRIVATE include src)
target_compile_definitions(test_cross_page_entity_linking PRIVATE MINIMAL_TEST_FRAMEWORK=1)
add_test(NAME test_cross_page_entity_linking COMMAND $<TARGET_FILE:test_cross_page_entity_linking>)

# Agent-2 epistemic reviewer tests
add_executable(test_agent2_epistemic_reviewer tests/test_agent2_epistemic_reviewer.cpp)
target_link_libraries(test_agent2_epistemic_reviewer
    neuroforge_core
    neuroforge_regions
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(test_agent2_epistemic_reviewer PRIVATE include src)
target_compile_definitions(test_agent2_epistemic_reviewer PRIVATE MINIMAL_TEST_FRAMEWORK=1)
add_test(NAME test_agent2_epistemic_reviewer COMMAND $<TARGET_FILE:test_agent2_epistemic_reviewer>)

# Benchmarks
add_executable(bench_stage_c benchmarks/bench_stage_c.cpp)
target_link_libraries(bench_stage_c 
    neuroforge_core 
    neuroforge_regions 
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(bench_stage_c PRIVATE include)

    # New: Encoders unit tests executable
    add_executable(test_encoders src/test_encoders.cpp)
    target_link_libraries(test_encoders 
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_encoders PRIVATE include)
    add_test(NAME test_encoders COMMAND $<TARGET_FILE:test_encoders>)

    # MemoryDB smoke tests (only when SQLite is available)
    if(NF_SQLITE_AVAILABLE)
        add_executable(test_memorydb src/test_memorydb.cpp)
        target_link_libraries(test_memorydb 
            neuroforge_core 
            neuroforge_regions 
            neuroforge_connectivity
            neuroforge_encoders
        )
        target_include_directories(test_memorydb PRIVATE include)
        add_test(NAME test_memorydb COMMAND $<TARGET_FILE:test_memorydb>)
    else()
        message(STATUS "Skipping test_memorydb: SQLite3 not available")
    endif()

    # Add debug_sqlite_check executable only when SQLite is available
    if(NF_SQLITE_AVAILABLE)
        # add_executable(debug_sqlite_check debug_sqlite_check.cpp)
        # target_link_libraries(debug_sqlite_check neuroforge_core)
    else()
        message(STATUS "Skipping debug_sqlite_check: SQLite3 not available")
    endif()

    # Phase A demo executable
    add_executable(phase_a_demo src/phase_a_demo.cpp)
    target_link_libraries(phase_a_demo PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(phase_a_demo PRIVATE include)
    if(OpenCV_FOUND)
        target_link_libraries(phase_a_demo PRIVATE ${OpenCV_LIBS})
        target_compile_definitions(phase_a_demo PRIVATE NF_HAVE_OPENCV=1)
    endif()
    if(WIN32)
        target_link_libraries(phase_a_demo PRIVATE winmm)
    endif()

    # Phase C runner (temporary harness to validate DB persistence)
    add_executable(phase_c_runner src/phase_c_runner.cpp)
    target_link_libraries(phase_c_runner PRIVATE neuroforge_core)
    target_include_directories(phase_c_runner PRIVATE include)
    if(OpenCV_FOUND)
        target_compile_definitions(phase_c_runner PRIVATE NF_HAVE_OPENCV=1)
    endif()
    if(WIN32)
        target_link_libraries(phase_c_runner PRIVATE winmm)
    endif()

    # Language test executable
    add_executable(test_language src/test_language.cpp)
    target_link_libraries(test_language PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_language PRIVATE include)
    add_test(NAME test_language COMMAND $<TARGET_FILE:test_language>)

    # Phase A test executable
    add_executable(test_phase_a src/test_phase_a.cpp)
    target_link_libraries(test_phase_a PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_phase_a PRIVATE include)
    add_test(NAME test_phase_a COMMAND $<TARGET_FILE:test_phase_a>)

    # Additional test executables
    add_executable(test_acoustic_language src/test_acoustic_language.cpp)
    target_link_libraries(test_acoustic_language PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_acoustic_language PRIVATE include)
    add_test(NAME test_acoustic_language COMMAND $<TARGET_FILE:test_acoustic_language>)

    add_executable(test_speech_production src/test_speech_production.cpp)
    target_link_libraries(test_speech_production PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_speech_production PRIVATE include)
    add_test(NAME test_speech_production COMMAND $<TARGET_FILE:test_speech_production>)

    add_executable(test_babbling_stage src/test_babbling_stage.cpp)
    target_link_libraries(test_babbling_stage PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_babbling_stage PRIVATE include)
    add_test(NAME test_babbling_stage COMMAND $<TARGET_FILE:test_babbling_stage>)

    add_executable(test_developmental_tracking src/test_developmental_tracking.cpp)
    target_link_libraries(test_developmental_tracking PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_developmental_tracking PRIVATE include)
    add_test(NAME test_developmental_tracking COMMAND $<TARGET_FILE:test_developmental_tracking>)

    add_executable(test_phase2_integration src/test_phase2_integration.cpp)
    target_link_libraries(test_phase2_integration PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_phase2_integration PRIVATE include)
    add_test(NAME test_phase2_integration COMMAND $<TARGET_FILE:test_phase2_integration>)

    add_executable(test_babbling_stage_simple src/test_babbling_stage_simple.cpp)
    target_link_libraries(test_babbling_stage_simple PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_babbling_stage_simple PRIVATE include)
    add_test(NAME test_babbling_stage_simple COMMAND $<TARGET_FILE:test_babbling_stage_simple>)

    add_executable(test_substrate_language_integration src/test_substrate_language_integration.cpp)
    target_link_libraries(test_substrate_language_integration PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_substrate_language_integration PRIVATE include)
    add_test(NAME test_substrate_language_integration COMMAND $<TARGET_FILE:test_substrate_language_integration>)

    add_executable(test_visual_language_integration src/test_visual_language_integration.cpp)
    target_link_libraries(test_visual_language_integration PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_visual_language_integration PRIVATE include)
    add_test(NAME test_visual_language_integration COMMAND $<TARGET_FILE:test_visual_language_integration>)

    # SurvivalBias unit test
    add_executable(test_survival_bias src/test_survival_bias.cpp)
    target_link_libraries(test_survival_bias PRIVATE
        neuroforge_core
    )
    target_include_directories(test_survival_bias PRIVATE include)
    add_test(NAME test_survival_bias COMMAND $<TARGET_FILE:test_survival_bias>)

    # Phase 2 memory test suite (from tests directory)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_phase2_memory.cpp)
        add_executable(test_phase2_memory tests/test_phase2_memory.cpp)
        target_link_libraries(test_phase2_memory PRIVATE
            neuroforge_core 
            neuroforge_regions 
            neuroforge_connectivity
            neuroforge_encoders
        )
        target_include_directories(test_phase2_memory PRIVATE include src)
        add_test(NAME test_phase2_memory COMMAND $<TARGET_FILE:test_phase2_memory>)
    else()
        message(STATUS "Skipping test_phase2_memory: tests/test_phase2_memory.cpp not found")
    endif()

    # Neuron scaling test (Windows memory counters)
    add_executable(test_neuron_scaling src/test_neuron_scaling.cpp)
    target_link_libraries(test_neuron_scaling PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_neuron_scaling PRIVATE include)
    if (MSVC)
        # Link Psapi for PROCESS_MEMORY_COUNTERS_EX
        target_link_libraries(test_neuron_scaling PRIVATE psapi)
    endif()
    add_test(NAME test_neuron_scaling COMMAND $<TARGET_FILE:test_neuron_scaling>)

    # Demo executables
    add_executable(phase5_language_demo src/phase5_language_demo.cpp)
    target_link_libraries(phase5_language_demo PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(phase5_language_demo PRIVATE include)
    if(OpenCV_FOUND)
        target_link_libraries(phase5_language_demo PRIVATE ${OpenCV_LIBS})
        target_compile_definitions(phase5_language_demo PRIVATE NF_HAVE_OPENCV=1)
    endif()
    if(WIN32)
        target_link_libraries(phase5_language_demo PRIVATE winmm)
    endif()

    # Minimal test for Phase 5 initialization debugging
    add_executable(phase5_minimal_test src/phase5_minimal_test.cpp)
    target_link_libraries(phase5_minimal_test PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(phase5_minimal_test PRIVATE include)
    if(WIN32)
        target_link_libraries(phase5_minimal_test PRIVATE winmm)
    endif()

    # Fast test for Phase 5 performance validation
    # add_executable(phase5_fast_test phase5_fast_test.cpp)
    # target_link_libraries(phase5_fast_test PRIVATE
    #     neuroforge_core 
    #     neuroforge_regions 
    #     neuroforge_connectivity
    #     neuroforge_encoders
    # )
    # target_include_directories(phase5_fast_test PRIVATE include)
    # if(WIN32)
    #     target_link_libraries(phase5_fast_test PRIVATE winmm)
    # endif()

    # Phase 5 optimized demo executable
    # add_executable(phase5_optimized_demo phase5_optimized_demo.cpp)
    # target_link_libraries(phase5_optimized_demo PRIVATE
    #     neuroforge_core 
    #     neuroforge_regions 
    #     neuroforge_connectivity
    #     neuroforge_encoders
    # )
    # target_include_directories(phase5_optimized_demo PRIVATE include)
    # if(WIN32)
    #     target_link_libraries(phase5_optimized_demo PRIVATE winmm)
    # endif()

    add_executable(social_perception_demo src/social_perception_demo.cpp)
    target_link_libraries(social_perception_demo PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(social_perception_demo PRIVATE include)
    if(OpenCV_FOUND AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_link_libraries(social_perception_demo PRIVATE ${OpenCV_LIBS})
        target_compile_definitions(social_perception_demo PRIVATE NF_HAVE_OPENCV=1)
    endif()
    if(WIN32)
        # Ensure Windows GUI/GDI libs are linked for OpenCV HighGUI and drawing
        target_link_libraries(social_perception_demo PRIVATE winmm user32 gdi32)
    endif()

    add_executable(simple_social_demo src/simple_social_demo.cpp)
    target_link_libraries(simple_social_demo PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(simple_social_demo PRIVATE include)
    if(OpenCV_FOUND AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        target_link_libraries(simple_social_demo PRIVATE ${OpenCV_LIBS})
        target_compile_definitions(simple_social_demo PRIVATE NF_HAVE_OPENCV=1)
    endif()
    if(WIN32)
        target_link_libraries(simple_social_demo PRIVATE winmm user32 gdi32)
    endif()

    add_executable(production_substrate_deployment src/production_substrate_deployment.cpp)
    target_link_libraries(production_substrate_deployment PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(production_substrate_deployment PRIVATE include)
    if(OpenCV_FOUND)
        target_link_libraries(production_substrate_deployment PRIVATE ${OpenCV_LIBS})
        target_compile_definitions(production_substrate_deployment PRIVATE NF_HAVE_OPENCV=1)
    endif()
    if(WIN32)
        target_link_libraries(production_substrate_deployment PRIVATE winmm)
    endif()

    # Unified substrate demo (multi-phase concurrent run)
    add_executable(unified_substrate_demo src/unified_substrate_demo.cpp)
    target_link_libraries(unified_substrate_demo PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(unified_substrate_demo PRIVATE include)
    if(WIN32)
        target_link_libraries(unified_substrate_demo PRIVATE winmm)
    endif()

    # Cap'n Proto round-trip test (only when Cap'n Proto is available)
    if(CapnProto_FOUND)
        add_executable(test_capnp_roundtrip src/test_capnp_roundtrip.cpp)
        target_link_libraries(test_capnp_roundtrip PRIVATE
            neuroforge_core
            neuroforge_regions
            neuroforge_connectivity
            neuroforge_encoders
        )
        target_include_directories(test_capnp_roundtrip PRIVATE include)
        if(WIN32)
            target_link_libraries(test_capnp_roundtrip PRIVATE winmm)
        endif()
    endif()

    # Unified Training Demo (Nurture/Training Phase)
    add_executable(unified_training_demo src/unified_training_demo.cpp)
    target_link_libraries(unified_training_demo PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(unified_training_demo PRIVATE include)
    if(WIN32)
        target_link_libraries(unified_training_demo PRIVATE winmm)
    endif()

    # Unified substrate smoke test (CTest)
    add_executable(test_unified_smoke src/test_unified_smoke.cpp)
    target_link_libraries(test_unified_smoke PRIVATE
        neuroforge_core 
        neuroforge_regions 
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_unified_smoke PRIVATE include)
    add_test(NAME test_unified_smoke COMMAND $<TARGET_FILE:test_unified_smoke>)

    # Enhanced Auditory Cortex Test
    
# Test for Meta-Cognition System (Stage D)
add_executable(test_meta_cognition tests/test_meta_cognition.cpp)
target_link_libraries(test_meta_cognition PRIVATE
    neuroforge_core 
    neuroforge_regions 
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(test_meta_cognition PRIVATE include)
add_test(NAME test_meta_cognition COMMAND $<TARGET_FILE:test_meta_cognition>)

# Test for Relation Gates (Autonomous Grounding Phase 1)
add_executable(test_relation_gates tests/test_relation_gates.cpp)
target_link_libraries(test_relation_gates PRIVATE
    neuroforge_core 
    neuroforge_regions 
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(test_relation_gates PRIVATE include)
add_test(NAME test_relation_gates COMMAND $<TARGET_FILE:test_relation_gates>)

add_executable(test_auditory_cortex_enhanced tests/test_auditory_cortex_enhanced.cpp)
    target_link_libraries(test_auditory_cortex_enhanced PRIVATE
        neuroforge_core
        neuroforge_regions
        neuroforge_connectivity
        neuroforge_encoders
    )
    target_include_directories(test_auditory_cortex_enhanced PRIVATE include)
    add_test(NAME test_auditory_cortex_enhanced COMMAND $<TARGET_FILE:test_auditory_cortex_enhanced>)     

# Test for Cross-Modal Integration (Stage D/E)
add_executable(test_cross_modal tests/test_cross_modal.cpp)
target_link_libraries(test_cross_modal PRIVATE
    neuroforge_core
    neuroforge_regions
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(test_cross_modal PRIVATE include)
add_test(NAME test_cross_modal COMMAND $<TARGET_FILE:test_cross_modal>)

# CuriosityNavigator Unit Test (Phase 3)
add_executable(test_curiosity_navigator tests/test_curiosity_navigator.cpp)
target_link_libraries(test_curiosity_navigator PRIVATE
    neuroforge_core
    neuroforge_regions
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(test_curiosity_navigator PRIVATE include)
target_compile_definitions(test_curiosity_navigator PRIVATE MINIMAL_TEST_FRAMEWORK=1)
add_test(NAME test_curiosity_navigator COMMAND $<TARGET_FILE:test_curiosity_navigator>)

# Full Integration Test (Autonomous Grounding Phase 1-4)
add_executable(test_internet_grounding_full tests/test_internet_grounding_full.cpp)
target_link_libraries(test_internet_grounding_full PRIVATE
    neuroforge_core
    neuroforge_regions
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(test_internet_grounding_full PRIVATE include)
target_compile_definitions(test_internet_grounding_full PRIVATE MINIMAL_TEST_FRAMEWORK=1)
add_test(NAME IntegrationTest COMMAND $<TARGET_FILE:test_internet_grounding_full>)

# Language Acquisition Integration Test (Phase 5)
add_executable(test_language_acquisition tests/test_language_acquisition.cpp)
target_link_libraries(test_language_acquisition PRIVATE
    neuroforge_core
    neuroforge_regions
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(test_language_acquisition PRIVATE include)
target_compile_definitions(test_language_acquisition PRIVATE MINIMAL_TEST_FRAMEWORK=1)
add_test(NAME test_language_acquisition COMMAND $<TARGET_FILE:test_language_acquisition>)

endif()

# Language Learning Demo
add_executable(language_learning_demo demos/language_learning_demo.cpp)
target_link_libraries(language_learning_demo PRIVATE
    neuroforge_core
    neuroforge_regions
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(language_learning_demo PRIVATE include)

# Production Autonomous Learning Runner
add_executable(neuroforge_learn src/neuroforge_learn.cpp)
target_link_libraries(neuroforge_learn PRIVATE
    neuroforge_core
    neuroforge_regions
    neuroforge_connectivity
    neuroforge_encoders
)
target_include_directories(neuroforge_learn PRIVATE include)

# Installation
install(TARGETS neuroforge neuroforge_core neuroforge_regions neuroforge_connectivity
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/ DESTINATION include)

# Print build information
message(STATUS "NeuroForge Hypergraph Brain - Build Configuration:")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  GPU Stubs: ${NEUROFORGE_ENABLE_GPU}")
message(STATUS "  Vision Demo Default: ${NEUROFORGE_ENABLE_VISION_DEMO}")
if(OpenCV_FOUND)
    message(STATUS "  OpenCV: FOUND (${OpenCV_VERSION})")
else()
    message(STATUS "  OpenCV: NOT FOUND (vision demo will use synthetic input)")
endif()

# Viewer (optional OpenGL GLFW/GLAD)
if(NEUROFORGE_WITH_VIEWER)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        find_package(glfw3 CONFIG QUIET)
        find_package(glad CONFIG QUIET)
        find_package(OpenGL QUIET)

        set(_NF_GLFW_TARGET "")
        if(TARGET glfw)
            set(_NF_GLFW_TARGET glfw)
        elseif(TARGET glfw::glfw)
            set(_NF_GLFW_TARGET glfw::glfw)
        endif()

        if(_NF_GLFW_TARGET AND (TARGET glad::glad OR TARGET glad))
            add_executable(neuroforge_viewer
                src/viewer/ViewerMain.cpp
                src/viewer/Visualizer3D.cpp
            )
            target_include_directories(neuroforge_viewer PRIVATE include)
            target_link_libraries(neuroforge_viewer PRIVATE neuroforge_core neuroforge_regions neuroforge_connectivity neuroforge_encoders ${_NF_GLFW_TARGET})
            if(TARGET glad::glad)
                target_link_libraries(neuroforge_viewer PRIVATE glad::glad)
            else()
                target_link_libraries(neuroforge_viewer PRIVATE glad)
            endif()
            if(OpenGL_FOUND)
                target_link_libraries(neuroforge_viewer PRIVATE OpenGL::GL)
            elseif(WIN32)
                target_link_libraries(neuroforge_viewer PRIVATE opengl32)
            endif()
            target_compile_definitions(neuroforge_viewer PRIVATE NF_WITH_VIEWER=1)
        else()
            message(WARNING "NEUROFORGE_WITH_VIEWER is ON but GLFW/GLAD not found; skipping viewer target")
        endif()
    else()
        message(WARNING "Viewer is disabled for non-MSVC toolchains; skipping viewer target")
    endif()
endif()

# Require SQLite3 (official or vcpkg) for MemoryDB telemetry
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
    target_link_libraries(neuroforge_core PUBLIC SQLite::SQLite3)
    target_compile_definitions(neuroforge_core PUBLIC NF_HAVE_SQLITE3=1)
    target_link_libraries(neuroforge PRIVATE SQLite::SQLite3)
    target_compile_definitions(neuroforge PRIVATE NF_HAVE_SQLITE3=1)
    # Auto-deploy sqlite3.dll for Windows builds when using official SQLite3 package
    if(WIN32)
        # Try to locate the runtime DLL next to the imported library
        get_target_property(_sqlite3_location SQLite::SQLite3 IMPORTED_LOCATION)
        if(_sqlite3_location)
            get_filename_component(_sqlite3_dir "${_sqlite3_location}" DIRECTORY)
            # Common layout: lib directory with parallel bin directory
            string(REGEX REPLACE "/lib$" "/bin" _sqlite3_bin_dir "${_sqlite3_dir}")
            set(_sqlite3_dll "${_sqlite3_bin_dir}/sqlite3.dll")
            if(EXISTS "${_sqlite3_dll}")
                add_custom_command(TARGET neuroforge POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_sqlite3_dll}"
                    $<TARGET_FILE_DIR:neuroforge>
                    COMMENT "Deploying sqlite3.dll for NeuroForge")
                if(NEUROFORGE_ENABLE_TESTS)
                    add_custom_command(TARGET test_learning POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_sqlite3_dll}"
                        $<TARGET_FILE_DIR:test_learning>
                        COMMENT "Deploying sqlite3.dll for test_learning")
                    add_custom_command(TARGET test_encoders POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_sqlite3_dll}"
                        $<TARGET_FILE_DIR:test_encoders>
                        COMMENT "Deploying sqlite3.dll for test_encoders")
                    if(TARGET neuroforge_tests)
                        add_custom_command(TARGET neuroforge_tests POST_BUILD
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_sqlite3_dll}"
                            "$<TARGET_FILE_DIR:neuroforge_tests>"
                            VERBATIM
                            COMMENT "Deploying sqlite3.dll for neuroforge_tests")
                    endif()
                    if(TARGET test_memorydb)
                        add_custom_command(TARGET test_memorydb POST_BUILD
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_sqlite3_dll}"
                            "$<TARGET_FILE_DIR:test_memorydb>"
                            VERBATIM
                            COMMENT "Deploying sqlite3.dll for test_memorydb")
                    endif()
                    if(TARGET phase_c_runner)
                        add_custom_command(TARGET phase_c_runner POST_BUILD
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_sqlite3_dll}"
                            "$<TARGET_FILE_DIR:phase_c_runner>"
                            VERBATIM
                            COMMENT "Deploying sqlite3.dll for phase_c_runner")
                    endif()
                endif()
                if(TARGET neuroforge_viewer)
                    add_custom_command(TARGET neuroforge_viewer POST_BUILD
                        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${_sqlite3_dll}"
                        $<TARGET_FILE_DIR:neuroforge_viewer>
                        COMMENT "Deploying sqlite3.dll for neuroforge_viewer")
                endif()
            endif()
        endif()
    endif()
else()
    # Try vcpkg's unofficial sqlite3 config package
    find_package(unofficial-sqlite3 CONFIG QUIET)
    if(TARGET unofficial::sqlite3::sqlite3)
        target_link_libraries(neuroforge_core PUBLIC unofficial::sqlite3::sqlite3)
        target_compile_definitions(neuroforge_core PUBLIC NF_HAVE_SQLITE3=1)
        target_link_libraries(neuroforge PRIVATE unofficial::sqlite3::sqlite3)
        target_compile_definitions(neuroforge PRIVATE NF_HAVE_SQLITE3=1)
        
        # Auto-deploy sqlite3.dll for Windows builds
        if(WIN32)
            # Find sqlite3.dll in vcpkg installation
            get_target_property(_sqlite3_configs unofficial::sqlite3::sqlite3 IMPORTED_CONFIGURATIONS)
            if(_sqlite3_configs)
                list(GET _sqlite3_configs 0 _first_config)
                get_target_property(_sqlite3_location unofficial::sqlite3::sqlite3 IMPORTED_LOCATION_${_first_config})
                if(_sqlite3_location)
                    get_filename_component(_sqlite3_dir "${_sqlite3_location}" DIRECTORY)
                    # Look for sqlite3.dll in the bin directory (parallel to lib)
                    string(REGEX REPLACE "/lib$" "/bin" _sqlite3_bin_dir "${_sqlite3_dir}")
                    set(_sqlite3_dll "${_sqlite3_bin_dir}/sqlite3.dll")
                    if(EXISTS "${_sqlite3_dll}")
                        # Add post-build commands to copy sqlite3.dll next to executables
                        add_custom_command(TARGET neuroforge POST_BUILD
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            "${_sqlite3_dll}"
                            $<TARGET_FILE_DIR:neuroforge>
                            COMMENT "Deploying sqlite3.dll for NeuroForge")
                        
                        # If tests are enabled, also deploy for test executables
                        if(NEUROFORGE_ENABLE_TESTS)
                            add_custom_command(TARGET test_learning POST_BUILD
                                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${_sqlite3_dll}"
                                $<TARGET_FILE_DIR:test_learning>
                                COMMENT "Deploying sqlite3.dll for test_learning")
                            
                            add_custom_command(TARGET test_encoders POST_BUILD
                                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${_sqlite3_dll}"
                                $<TARGET_FILE_DIR:test_encoders>
                                COMMENT "Deploying sqlite3.dll for test_encoders")
                            
                            # Deploy for neuroforge_tests if it exists
                            if(TARGET neuroforge_tests)
                                add_custom_command(TARGET neuroforge_tests POST_BUILD
                                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                    "${_sqlite3_dll}"
                                    "$<TARGET_FILE_DIR:neuroforge_tests>"
                                    VERBATIM
                                    COMMENT "Deploying sqlite3.dll for neuroforge_tests")
                            endif()

                            # Deploy for test_memorydb if it exists
                            if(TARGET test_memorydb)
                                add_custom_command(TARGET test_memorydb POST_BUILD
                                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                    "${_sqlite3_dll}"
                                    "$<TARGET_FILE_DIR:test_memorydb>"
                                    VERBATIM
                                    COMMENT "Deploying sqlite3.dll for test_memorydb")
                            endif()
                            # Deploy for phase_c_runner if it exists
                            if(TARGET phase_c_runner)
                                add_custom_command(TARGET phase_c_runner POST_BUILD
                                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                    "${_sqlite3_dll}"
                                    "$<TARGET_FILE_DIR:phase_c_runner>"
                                    VERBATIM
                                    COMMENT "Deploying sqlite3.dll for phase_c_runner")
                            endif()
                        endif()
                        
                        # If viewer is built, deploy there too
                        if(TARGET neuroforge_viewer)
                            add_custom_command(TARGET neuroforge_viewer POST_BUILD
                                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                                "${_sqlite3_dll}"
                                $<TARGET_FILE_DIR:neuroforge_viewer>
                                COMMENT "Deploying sqlite3.dll for neuroforge_viewer")
                        endif()
                    endif()
                endif()
            endif()
        endif()
    else()
        message(WARNING "SQLite3 not found; MemoryDB telemetry disabled. Build will continue without SQLite.")
        # MemoryDB will compile in stub mode without NF_HAVE_SQLITE3.
    endif()
endif()

# Phase 28 stress test executable
add_executable(test_phase28_stress tests/test_phase28_stress.cpp)
target_include_directories(test_phase28_stress PRIVATE include)
target_link_libraries(test_phase28_stress PRIVATE neuroforge_core)

# Phase 29 stress test executable
add_executable(test_phase29_stress tests/test_phase29_stress.cpp)
target_include_directories(test_phase29_stress PRIVATE include)
target_link_libraries(test_phase29_stress PRIVATE neuroforge_core)

# Phase 30 stress test executable
add_executable(test_phase30_stress tests/test_phase30_stress.cpp)
target_include_directories(test_phase30_stress PRIVATE include)
target_link_libraries(test_phase30_stress PRIVATE neuroforge_core)

# Phase D: Developmental Runtime session (demo)
add_executable(run_developmental_session tests/run_developmental_session.cpp)
target_include_directories(run_developmental_session PRIVATE include)
target_link_libraries(run_developmental_session PRIVATE neuroforge_core)

# Phase D: REAL Developmental Session (with actual browsing)
add_executable(run_real_session tests/run_real_developmental_session.cpp)
target_include_directories(run_real_session PRIVATE include)
target_link_libraries(run_real_session PRIVATE neuroforge_core)

# Phase E1-E4: Expression and Vision Test
add_executable(test_expression_vision tests/test_expression_vision.cpp)
target_include_directories(test_expression_vision PRIVATE include)
target_link_libraries(test_expression_vision PRIVATE neuroforge_core)

# Integrated Developmental Session (D + E1-E4)
add_executable(run_integrated_session tests/run_integrated_session.cpp)
target_include_directories(run_integrated_session PRIVATE include)
target_link_libraries(run_integrated_session PRIVATE neuroforge_core)

# Curiosity-First Autonomous Session
add_executable(run_curiosity_session tests/run_curiosity_session.cpp)
target_include_directories(run_curiosity_session PRIVATE include)
target_link_libraries(run_curiosity_session PRIVATE neuroforge_core)

# Visual Browsing Session (See, Click, Scroll)
add_executable(run_visual_session tests/run_visual_session.cpp)
target_include_directories(run_visual_session PRIVATE include)
target_link_libraries(run_visual_session PRIVATE 
    neuroforge_core
    ${OpenCV_LIBS} # Link OpenCV directly for vision
)
if(WIN32)
    target_link_libraries(run_visual_session PRIVATE winmm user32 gdi32)
endif()
target_compile_definitions(run_visual_session PRIVATE NF_HAVE_OPENCV=1)

# Cap'n Proto Serialization Test
# add_executable(test_capnproto tests/test_capnproto.cpp src/serialization/neuroforge.capnp.cpp)
# target_include_directories(test_capnproto PRIVATE include ${CAPNP_INCLUDE_DIRS})
# target_link_libraries(test_capnproto PRIVATE CapnProto::capnp CapnProto::kj)
