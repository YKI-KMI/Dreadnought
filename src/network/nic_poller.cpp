#include "dreadnought/network/nic_poller.hpp"
#include <sys/mman.h>
#include <cstring>

namespace dreadnought {

NICPoller::NICPoller() noexcept 
    : initialized_(false), mmap_region_(nullptr) {}

NICPoller::~NICPoller() noexcept {
    if (mmap_region_ != nullptr && mmap_region_ != MAP_FAILED) {
        munmap(mmap_region_, RX_RING_SIZE * sizeof(Packet));
    }
}

bool NICPoller::init() noexcept {
    // In production: mmap NIC DMA region
    // mmap_region_ = mmap(nullptr, size, PROT_READ|PROT_WRITE, MAP_SHARED, nic_fd, 0);
    
    // Simulation: allocate aligned memory
    mmap_region_ = aligned_alloc(4096, RX_RING_SIZE * sizeof(Packet));
    if (mmap_region_ == nullptr) return false;
    
    std::memset(mmap_region_, 0, RX_RING_SIZE * sizeof(Packet));
    
    initialized_.store(true, std::memory_order_release);
    return true;
}

void NICPoller::simulate_packet_arrival(const Packet& pkt) noexcept {
    rx_ring_.try_push(pkt);
}

} // namespace dreadnought