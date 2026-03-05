#include "dreadnought/logger/spsc_queue.hpp"
#include <iostream>
#include <cassert>
#include <thread>

using namespace dreadnought;

void test_single_threaded() {
    SPSCLogQueue<16> queue;
    
    LogEntry entry1{100, 1, 0, 200, 300};
    assert(queue.try_push(entry1));
    
    LogEntry entry2;
    assert(queue.try_pop(entry2));
    assert(entry2.timestamp == 100);
    assert(entry2.event_id == 1);
    
    std::cout << "Single-threaded test passed\n";
}

void test_wraparound() {
    SPSCLogQueue<4> queue;
    
    for (int i = 0; i < 10; ++i) {
        LogEntry entry{static_cast<uint64_t>(i), static_cast<uint32_t>(i), 0, 0, 0};
        queue.try_push(entry);
        
        LogEntry popped;
        queue.try_pop(popped);
        assert(popped.timestamp == i);
    }
    
    std::cout << "Wraparound test passed\n";
}

void test_producer_consumer() {
    SPSCLogQueue<1024> queue;
    constexpr int COUNT = 10000;
    
    std::thread producer([&queue]() {
        for (int i = 0; i < COUNT; ++i) {
            LogEntry entry{static_cast<uint64_t>(i), static_cast<uint32_t>(i), 0, 0, 0};
            while (!queue.try_push(entry)) {
                std::this_thread::yield();
            }
        }
    });
    
    std::thread consumer([&queue]() {
        for (int i = 0; i < COUNT; ++i) {
            LogEntry entry;
            while (!queue.try_pop(entry)) {
                std::this_thread::yield();
            }
            assert(entry.timestamp == i);
        }
    });
    
    producer.join();
    consumer.join();
    
    std::cout << "Producer-consumer test passed\n";
}

int main() {
    std::cout << "=== SPSC Queue Tests ===\n";
    
    test_single_threaded();
    test_wraparound();
    test_producer_consumer();
    
    std::cout << "All tests passed!\n";
    return 0;
}