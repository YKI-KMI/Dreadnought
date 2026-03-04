#pragma once

#include "dreadnought/core/compiler.hpp"
#include "dreadnought/core/types.hpp"
#include <atomic>
#include <array>

namespace dreadnought {

// Log entry - binary only
struct alignas(32) LogEntry {
    Timestamp timestamp;
    uint32_t event_id;
    uint32_t _pad1;
    uint64_t value1;
    uint64_t value2;
};

static_assert(sizeof(LogEntry) == 32, "LogEntry must be 32 bytes");

// SPSC queue for lock-free logging
template<size_t N>
class alignas(DESTRUCTIVE_SIZE) SPSCLogQueue {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");
    
public:
    SPSCLogQueue() noexcept : head_(0), tail_(0) {}
    
    // Producer (hot path) - write binary log entry
    FORCE_INLINE bool try_push(const LogEntry& entry) noexcept {
        const uint32_t current_tail = tail_.load(std::memory_order_relaxed);
        const uint32_t next_tail = (current_tail + 1) & (N - 1);
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Full - drop log entry
        }
        
        buffer_[current_tail] = entry;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    // Consumer (background thread) - read log entry
    FORCE_INLINE bool try_pop(LogEntry& entry) noexcept {
        const uint32_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Empty
        }
        
        entry = buffer_[current_head];
        head_.store((current_head + 1) & (N - 1), std::memory_order_release);
        return true;
    }
    
    FORCE_INLINE bool is_empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }
    
private:
    alignas(DESTRUCTIVE_SIZE) std::atomic<uint32_t> head_;
    alignas(DESTRUCTIVE_SIZE) std::atomic<uint32_t> tail_;
    std::array<LogEntry, N> buffer_;
};

} // namespace dreadnought