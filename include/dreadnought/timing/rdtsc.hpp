#pragma once

#include "dreadnought/core/compiler.hpp"
#include <cstdint>

namespace dreadnought {

// Read Time Stamp Counter - fastest timestamp source
// Latency: ~20-30 CPU cycles
FORCE_INLINE uint64_t rdtsc() noexcept {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// Serializing RDTSC - ensures all prior instructions complete
FORCE_INLINE uint64_t rdtscp() noexcept {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi) :: "rcx");
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// TSC frequency calibration
class TSCCalibrator {
public:
    TSCCalibrator() noexcept;
    
    // Calibrate TSC frequency against CLOCK_MONOTONIC
    void calibrate() noexcept;
    
    // Convert TSC ticks to nanoseconds
    FORCE_INLINE uint64_t ticks_to_ns(uint64_t ticks) const noexcept {
        return ticks * ns_per_tick_;
    }
    
    // Convert nanoseconds to TSC ticks
    FORCE_INLINE uint64_t ns_to_ticks(uint64_t ns) const noexcept {
        return ns / ns_per_tick_;
    }
    
    double get_frequency_ghz() const noexcept { return tsc_freq_ghz_; }
    
private:
    double tsc_freq_ghz_;
    double ns_per_tick_;
};

// Global TSC calibrator instance
extern TSCCalibrator g_tsc_calibrator;

} // namespace dreadnought