#pragma once

#include "dreadnought/market_data/market_state.hpp"
#include "dreadnought/strategy/strategy_base.hpp"
#include "dreadnought/network/nic_poller.hpp"
#include "dreadnought/network/packet.hpp"
#include "dreadnought/logger/binary_logger.hpp"
#include "dreadnought/timing/rdtsc.hpp"
#include "dreadnought/timing/latency_tracker.hpp"
#include "dreadnought/threading/affinity.hpp"
#include "dreadnought/core/compiler.hpp"
#include <atomic>
#include <array>

#ifdef __x86_64__
#include <immintrin.h>
#endif

namespace dreadnought {

// Main Dreadnought engine - template-based strategy instantiation
template<typename Strategy>
class Dreadnought {
public:
    Dreadnought() noexcept 
        : running_(false), warmup_complete_(false), packet_count_(0) {}
    
    ~Dreadnought() noexcept {
        shutdown();
    }
    
    bool init(const char* log_path) noexcept {
        if (!nic_.init()) return false;
        if (!logger_.init(log_path)) return false;
        
        g_tsc_calibrator.calibrate();
        
        return true;
    }
    
    void shutdown() noexcept {
        running_.store(false, std::memory_order_release);
        logger_.shutdown();
    }
    
    // Main execution loop
    HOT_PATH void run() noexcept {
        // Pin to dedicated core
        ThreadAffinity::pin_to_core(ThreadAffinity::MARKET_DATA_CORE);
        ThreadAffinity::set_realtime_priority();
        
        // Warmup phase - load everything into cache
        warmup();
        
        running_.store(true, std::memory_order_release);
        
        // Main loop - absolutely minimal latency
        while (likely(running_.load(std::memory_order_acquire))) {
            main_loop_iteration();
        }
        
        // Post-shutdown analysis
        latency_tracker_.compute_percentiles();
    }
    
    // Latency statistics
    const LatencyTracker<1000000>& get_latency_tracker() const noexcept {
        return latency_tracker_;
    }
    
private:
    FORCE_INLINE void main_loop_iteration() noexcept {
        // Poll NIC for packets - batch up to 32
        constexpr int BATCH_SIZE = 32;
        std::array<Packet, BATCH_SIZE> packets;
        
        int received = nic_.poll_rx(packets.data(), BATCH_SIZE);
        
        if (likely(received > 0)) {
            for (int i = 0; i < received; ++i) {
                process_packet(packets[i]);
            }
        } else {
            // No packets - spin wait
            CPU_PAUSE();
        }
    }
    
    FORCE_INLINE void process_packet(const Packet& pkt) noexcept {
        const uint64_t start_ts = rdtscp();
        
        // Log packet arrival
        logger_.log(LogEvent::PACKET_RECEIVED, pkt.header.sequence_num, start_ts);
        
        // Parse and update order book
        if (likely(pkt.is_market_data())) {
            update_order_book(pkt);
        }
        
        // Strategy execution
        strategy_.on_tick(md_state_.book, pkt.header.exchange_ts);
        
        // Check for order generation
        if (strategy_.should_send_order()) {
            send_order();
        }
        
        const uint64_t end_ts = rdtscp();
        const uint64_t latency_ticks = end_ts - start_ts;
        const uint64_t latency_ns = g_tsc_calibrator.ticks_to_ns(latency_ticks);
        
        latency_tracker_.record(latency_ns);
        ++packet_count_;
    }
    
    FORCE_INLINE void update_order_book(const Packet& pkt) noexcept {
        const auto& md = pkt.payload.market_data;
        
        // Prefetch book before access
        PREFETCH_WRITE(&md_state_.book);
        
        if (md.side == Side::BID) {
            md_state_.book.update_bid(md.price, md.qty);
        } else {
            md_state_.book.update_ask(md.price, md.qty);
        }
        
        md_state_.last_update_ts = pkt.header.exchange_ts;
        ++md_state_.sequence_num;
        
        logger_.log(LogEvent::BOOK_UPDATED, md_state_.sequence_num, md_state_.last_update_ts);
    }
    
    FORCE_INLINE void send_order() noexcept {
        OrderPacket order_pkt;
        
        order_pkt.header.sequence_num = packet_count_;
        order_pkt.header.exchange_ts = rdtsc();
        order_pkt.header.type = PacketType::MARKET_DATA_UPDATE;
        order_pkt.side = strategy_.get_order_side();
        order_pkt.price = strategy_.get_order_price();
        order_pkt.qty = strategy_.get_order_qty();
        order_pkt.client_order_id = packet_count_;
        
        nic_.send_packet(order_pkt);
        
        logger_.log(LogEvent::ORDER_SENT, order_pkt.client_order_id, order_pkt.header.exchange_ts);
    }
    
    COLD_PATH void warmup() noexcept {
        // Pre-touch all memory to avoid page faults
        ThreadAffinity::warmup_cache(&md_state_, sizeof(md_state_));
        ThreadAffinity::warmup_cache(&strategy_, sizeof(strategy_));
        
        // Populate cache with likely access patterns
        for (int i = 0; i < 1000; ++i) {
            volatile double x = md_state_.book.mid_price();
            (void)x;
        }
        
        warmup_complete_ = true;
    }
    
    // State - cache-line aligned and isolated
    alignas(DESTRUCTIVE_SIZE) MarketDataState md_state_;
    alignas(DESTRUCTIVE_SIZE) Strategy strategy_;
    alignas(DESTRUCTIVE_SIZE) NICPoller nic_;
    alignas(DESTRUCTIVE_SIZE) BinaryLogger logger_;
    alignas(DESTRUCTIVE_SIZE) LatencyTracker<1000000> latency_tracker_;
    
    std::atomic<bool> running_;
    bool warmup_complete_;
    uint64_t packet_count_;
};

} // namespace dreadnought