#pragma once


#include <cstdint>
#include <new>
#include <cstdint>

// Branch prediction hints
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

// Force inline - critical path only
#define FORCE_INLINE __attribute__((always_inline)) inline

// Cold path - move out of instruction cache
#define COLD_PATH __attribute__((cold))

// Hot path - optimize aggressively
#define HOT_PATH __attribute__((hot))

// Prefetch hints
#define PREFETCH_READ(addr)  __builtin_prefetch((addr), 0, 3)
#define PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)

// Cache line size constants
namespace dreadnought {
    constexpr std::size_t CACHE_LINE_SIZE = 64;
    
    // C++17 hardware interference sizes
    #ifdef __cpp_lib_hardware_interference_size
        constexpr std::size_t DESTRUCTIVE_SIZE = std::hardware_destructive_interference_size;
        constexpr std::size_t CONSTRUCTIVE_SIZE = std::hardware_constructive_interference_size;
    #else
        constexpr std::size_t DESTRUCTIVE_SIZE = 64;
        constexpr std::size_t CONSTRUCTIVE_SIZE = 64;
    #endif
}

// No-op for CPU spin-wait
#define CPU_PAUSE() __builtin_ia32_pause()

// Memory barriers
#define COMPILER_BARRIER() __asm__ __volatile__("" ::: "memory")