#pragma once

#include "dreadnought/core/compiler.hpp"
#include "dreadnought/network/packet.hpp"
#include <atomic>
#include <array>

namespace dreadnought {

// Lock-free SPSC ring buffer for NIC RX/TX
// Producer = NIC DMA
// Consumer = Market data thread
template<size_t N>
class alignas(DESTRUCTIVE_SIZE) RingBuffer {
    static_assert((N & (N - 1)) == 0, "N must be power of 2");
    
public:
    RingBuffer() noexcept : head_(0), tail_(0) {}
    
    // Producer (NIC DMA) writes packet
    FORCE_INLINE bool try_push(const Packet& pkt) noexcept {
        const uint32_t current_tail = tail_.load(std::memory_order_relaxed);
        const uint32_t next_tail = (current_tail + 1) & (N - 1);
        
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Full
        }
        
        buffer_[current_tail] = pkt;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }
    
    // Consumer (market data thread) reads packet
    FORCE_INLINE bool try_pop(Packet& pkt) noexcept {
        const uint32_t current_head = head_.load(std::memory_order_relaxed);
        
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Empty
        }
        
        pkt = buffer_[current_head];
        head_.store((current_head + 1) & (N - 1), std::memory_order_release);
        return true;
    }
    
    FORCE_INLINE uint32_t size() const noexcept {
        return (tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_acquire)) & (N - 1);
    }
    
    FORCE_INLINE bool is_empty() const noexcept {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }
    
private:
    alignas(DESTRUCTIVE_SIZE) std::atomic<uint32_t> head_;
    alignas(DESTRUCTIVE_SIZE) std::atomic<uint32_t> tail_;
    std::array<Packet, N> buffer_;
};

} // namespace dreadnought