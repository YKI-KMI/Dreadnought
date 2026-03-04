#pragma once

#include "dreadnought/logger/spsc_queue.hpp"
#include "dreadnought/timing/rdtsc.hpp"
#include <thread>
#include <atomic>
#include <fstream>

namespace dreadnought {

// Binary logger with background thread
// Hot path: write to SPSC queue
// Cold path: background thread formats and writes to disk
class BinaryLogger {
public:
    static constexpr size_t QUEUE_SIZE = 65536;
    
    BinaryLogger() noexcept;
    ~BinaryLogger() noexcept;
    
    bool init(const char* log_path) noexcept;
    void shutdown() noexcept;
    
    // Hot path - called from trading thread
    FORCE_INLINE void log(uint32_t event_id, uint64_t value1, uint64_t value2) noexcept {
        LogEntry entry;
        entry.timestamp = rdtsc();
        entry.event_id = event_id;
        entry.value1 = value1;
        entry.value2 = value2;
        
        // Drop on overflow - never block hot path
        log_queue_.try_push(entry);
    }
    
private:
    void background_worker() noexcept;
    
    SPSCLogQueue<QUEUE_SIZE> log_queue_;
    std::thread worker_thread_;
    std::atomic<bool> running_;
    std::ofstream log_file_;
};

// Log event IDs
namespace LogEvent {
    constexpr uint32_t PACKET_RECEIVED = 1;
    constexpr uint32_t BOOK_UPDATED = 2;
    constexpr uint32_t STRATEGY_SIGNAL = 3;
    constexpr uint32_t ORDER_SENT = 4;
    constexpr uint32_t ORDER_FILLED = 5;
}

} // namespace dreadnought