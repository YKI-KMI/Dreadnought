#pragma once

#include "dreadnought/timing/rdtsc.hpp"
#include "dreadnought/core/compiler.hpp"
#include <array>
#include <algorithm>

namespace dreadnought {

// Percentile latency tracker - preallocated storage
template<size_t N>
class LatencyTracker {
public:
    LatencyTracker() noexcept : count_(0) {
        samples_.fill(0);
    }
    
    FORCE_INLINE void record(uint64_t latency_ns) noexcept {
        if (count_ < N) {
            samples_[count_++] = latency_ns;
        }
    }
    
    void compute_percentiles() noexcept {
        if (count_ == 0) return;
        
        std::sort(samples_.begin(), samples_.begin() + count_);
        
        p50_ = samples_[count_ * 50 / 100];
        p99_ = samples_[count_ * 99 / 100];
        p999_ = samples_[count_ * 999 / 1000];
        
        uint64_t sum = 0;
        for (size_t i = 0; i < count_; ++i) {
            sum += samples_[i];
        }
        mean_ = sum / count_;
    }
    
    uint64_t p50() const noexcept { return p50_; }
    uint64_t p99() const noexcept { return p99_; }
    uint64_t p999() const noexcept { return p999_; }
    uint64_t mean() const noexcept { return mean_; }
    uint64_t min() const noexcept { return count_ > 0 ? samples_[0] : 0; }
    uint64_t max() const noexcept { return count_ > 0 ? samples_[count_ - 1] : 0; }
    
private:
    std::array<uint64_t, N> samples_;
    size_t count_;
    uint64_t mean_;
    uint64_t p50_;
    uint64_t p99_;
    uint64_t p999_;
};

} // namespace dreadnought