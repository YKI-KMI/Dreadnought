#include "dreadnought/logger/binary_logger.hpp"
#include "dreadnought/threading/affinity.hpp"
#include <iostream>

namespace dreadnought {

BinaryLogger::BinaryLogger() noexcept 
    : running_(false) {}

BinaryLogger::~BinaryLogger() noexcept {
    shutdown();
}

bool BinaryLogger::init(const char* log_path) noexcept {
    log_file_.open(log_path, std::ios::binary | std::ios::out);
    if (!log_file_.is_open()) return false;
    
    running_.store(true, std::memory_order_release);
    worker_thread_ = std::thread(&BinaryLogger::background_worker, this);
    
    return true;
}

void BinaryLogger::shutdown() noexcept {
    if (running_.load(std::memory_order_acquire)) {
        running_.store(false, std::memory_order_release);
        
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        
        if (log_file_.is_open()) {
            log_file_.close();
        }
    }
}

void BinaryLogger::background_worker() noexcept {
    // Pin to dedicated core
    ThreadAffinity::pin_to_core(ThreadAffinity::LOGGER_CORE);
    
    LogEntry entry;
    
    while (running_.load(std::memory_order_acquire) || !log_queue_.is_empty()) {
        if (log_queue_.try_pop(entry)) {
            // Write binary entry to file
            log_file_.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
            
            // Optional: format to text for debugging
            // std::cout << entry.timestamp << "," << entry.event_id << "," 
            //           << entry.value1 << "," << entry.value2 << "\n";
        } else {
            // Queue empty - yield CPU
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

} // namespace dreadnought