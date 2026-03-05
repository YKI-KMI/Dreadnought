#include "dreadnought/timing/rdtsc.hpp"
#include "dreadnought/timing/latency_tracker.hpp"
#include "dreadnought/market_data/order_book.hpp"
#include <iostream>
#include <vector>

using namespace dreadnought;

void benchmark_rdtsc() {
    constexpr int ITERATIONS = 1000000;
    LatencyTracker<ITERATIONS> tracker;
    
    for (int i = 0; i < ITERATIONS; ++i) {
        uint64_t start = rdtsc();
        uint64_t end = rdtsc();
        tracker.record(end - start);
    }
    
    tracker.compute_percentiles();
    
    std::cout << "=== RDTSC Overhead ===\n";
    std::cout << "Mean:  " << tracker.mean() << " cycles\n";
    std::cout << "P50:   " << tracker.p50() << " cycles\n";
    std::cout << "P99:   " << tracker.p99() << " cycles\n";
    std::cout << "P99.9: " << tracker.p999() << " cycles\n";
}

void benchmark_order_book_update() {
    constexpr int ITERATIONS = 100000;
    LatencyTracker<ITERATIONS> tracker;
    
    OrderBook book;
    
    // Pre-populate
    for (int i = 0; i < 10; ++i) {
        book.update_bid(100.0 - i * 0.01, 100);
        book.update_ask(101.0 + i * 0.01, 100);
    }
    
    for (int i = 0; i < ITERATIONS; ++i) {
        uint64_t start = rdtsc();
        book.update_bid(100.0, 500 + i % 100);
        uint64_t end = rdtsc();
        tracker.record(end - start);
    }
    
    tracker.compute_percentiles();
    
    std::cout << "\n=== Order Book Update ===\n";
    std::cout << "Mean:  " << tracker.mean() << " cycles\n";
    std::cout << "P50:   " << tracker.p50() << " cycles\n";
    std::cout << "P99:   " << tracker.p99() << " cycles\n";
    std::cout << "P99.9: " << tracker.p999() << " cycles\n";
}

int main() {
    g_tsc_calibrator.calibrate();
    std::cout << "TSC Frequency: " << g_tsc_calibrator.get_frequency_ghz() << " GHz\n\n";
    
    benchmark_rdtsc();
    benchmark_order_book_update();
    
    return 0;
}