cmake_minimum_required(VERSION 3.25)
project(dreadnought CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler selection: Clang/LLVM strongly preferred
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "Clang is strongly recommended for PGO and deterministic codegen")
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ============================================================================
# COMPILER FLAGS - PRODUCTION GRADE
# ============================================================================

set(COMMON_FLAGS
    -march=native          # Use all CPU features available
    -mtune=native          # Optimize for this exact CPU
    -fno-exceptions        # No exception overhead
    -fno-rtti              # No RTTI vtables
    -fno-plt               # Avoid PLT indirection
    -fomit-frame-pointer   # Use RBP as general register
    -falign-functions=64   # Align hot functions to cache line
    -fstrict-aliasing      # Aggressive pointer aliasing
    -ffast-math            # Unsafe but faster FP
)

set(OPTIMIZATION_FLAGS
    -O3                    # Max optimization
    -flto=thin             # Thin LTO for faster linking
    -fwhole-program-vtables # LTO-based devirtualization
)

# PGO Support
option(PGO_GENERATE "Generate PGO profile" OFF)
option(PGO_USE "Use PGO profile" OFF)

if(PGO_GENERATE)
    list(APPEND COMMON_FLAGS -fprofile-generate)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-generate")
endif()

if(PGO_USE)
    list(APPEND COMMON_FLAGS -fprofile-use=${CMAKE_SOURCE_DIR}/default.profdata)
endif()

# Debug flags for latency profiling
set(DEBUG_FLAGS
    -g
    -fno-omit-frame-pointer
)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(${COMMON_FLAGS} ${OPTIMIZATION_FLAGS})
else()
    add_compile_options(${COMMON_FLAGS} ${DEBUG_FLAGS})
endif()

# ============================================================================
# DEPENDENCIES
# ============================================================================

find_package(Threads REQUIRED)

# ============================================================================
# LIBRARY
# ============================================================================

add_library(dreadnought_core INTERFACE)
target_include_directories(dreadnought_core INTERFACE include)
target_link_libraries(dreadnought_core INTERFACE Threads::Threads)

# ============================================================================
# EXECUTABLES
# ============================================================================

add_executable(dreadnought_main
    src/main.cpp
    src/network/nic_poller.cpp
    src/logger/binary_logger.cpp
    src/threading/affinity.cpp
    strategies/mean_reversion.cpp
)
target_link_libraries(dreadnought_main PRIVATE dreadnought_core)

# ============================================================================
# TESTS
# ============================================================================

enable_testing()

add_executable(test_order_book tests/test_order_book.cpp)
target_link_libraries(test_order_book PRIVATE dreadnought_core)
add_test(NAME test_order_book COMMAND test_order_book)

add_executable(test_spsc_queue tests/test_spsc_queue.cpp)
target_link_libraries(test_spsc_queue PRIVATE dreadnought_core)
add_test(NAME test_spsc_queue COMMAND test_spsc_queue)

add_executable(benchmark_latency tests/benchmark_latency.cpp)
target_link_libraries(benchmark_latency PRIVATE dreadnought_core)

# ============================================================================
# TOOLS
# ============================================================================

add_executable(replay_engine tools/replay_engine.cpp src/threading/affinity.cpp)
target_link_libraries(replay_engine PRIVATE dreadnought_core)

# ============================================================================
# INSTALL
# ============================================================================

install(TARGETS dreadnought_main DESTINATION bin)