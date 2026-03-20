#include "dreadnought/timing/rdtsc.hpp"
#include <time.h>
#include <thread>
#include <chrono>

namespace dreadnought {

TSCCalibrator g_tsc_calibrator;

TSCCalibrator::TSCCalibrator() noexcept 
    : tsc_freq_ghz_(0.0), ns_per_tick_(0.0) {}

void TSCCalibrator::calibrate() noexcept {
    constexpr int SAMPLES = 10;
    constexpr int SLEEP_MS = 100;
    
    uint64_t tsc_deltas[SAMPLES];
    uint64_t ns_deltas[SAMPLES];
    
    for (int i = 0; i < SAMPLES; ++i) {
        struct timespec start_time, end_time;
        
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        uint64_t start_tsc = rdtsc();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
        
        uint64_t end_tsc = rdtsc();
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        
        uint64_t ns_elapsed = (end_time.tv_sec - start_time.tv_sec) * 1000000000ULL +
                              (end_time.tv_nsec - start_time.tv_nsec);
        
        tsc_deltas[i] = end_tsc - start_tsc;
        ns_deltas[i] = ns_elapsed;
    }
    
    // Sort samples to allow robust outlier rejection (actual median approach)
    std::sort(tsc_deltas, tsc_deltas + SAMPLES);
    std::sort(ns_deltas, ns_deltas + SAMPLES);
    
    uint64_t tsc_sum = 0, ns_sum = 0;
    // Discard 2 smallest and 2 largest samples
    for (int i = 2; i < SAMPLES - 2; ++i) {
        tsc_sum += tsc_deltas[i];
        ns_sum += ns_deltas[i];
    }
    
    double avg_tsc = static_cast<double>(tsc_sum) / (SAMPLES - 4);
    double avg_ns = static_cast<double>(ns_sum) / (SAMPLES - 4);
    
    tsc_freq_ghz_ = avg_tsc / avg_ns;
    ns_per_tick_ = avg_ns / avg_tsc;
}

} // namespace dreadnought