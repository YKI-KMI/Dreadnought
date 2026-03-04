#pragma once

#include "dreadnought/network/ring_buffer.hpp"
#include "dreadnought/network/packet.hpp"
#include "dreadnought/core/compiler.hpp"
#include <atomic>

namespace dreadnought {

// Simulated kernel-bypass NIC poller
// In production: Replace with DPDK/Solarflare/Mellanox APIs
class NICPoller {
public:
    static constexpr size_t RX_RING_SIZE = 4096;
    static constexpr size_t TX_RING_SIZE = 1024;
    
    NICPoller() noexcept;
    ~NICPoller() noexcept;
    
    // Initialize mmap region (simulated)
    bool init() noexcept;
    
    // Poll for received packets - busy wait, no blocking
    FORCE_INLINE int poll_rx(Packet* packets, int max_batch) noexcept {
        int count = 0;
        while (count < max_batch && rx_ring_.try_pop(packets[count])) {
            ++count;
        }
        return count;
    }
    
    // Enqueue packet for transmission - zero copy
    FORCE_INLINE bool send_packet(const OrderPacket& pkt) noexcept {
        // In real implementation: write directly to NIC TX ring
        return true; // Simulated
    }
    
    // Simulate packet arrival (for testing)
    void simulate_packet_arrival(const Packet& pkt) noexcept;
    
private:
    RingBuffer<RX_RING_SIZE> rx_ring_;
    std::atomic<bool> initialized_;
    
    // Simulated mmap region pointer
    void* mmap_region_;
};

} // namespace dreadnought