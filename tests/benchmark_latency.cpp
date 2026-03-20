#include "dreadnought/timing/rdtsc.hpp"
#include "dreadnought/timing/latency_tracker.hpp"
#include "dreadnought/market_data/order_book.hpp"
#include <iostream>
#include <vector>
#include <memory>

using namespace dreadnought;

void benchmark_rdtsc() {
    constexpr int ITERATIONS = 100000;
    auto tracker = std::make_unique<LatencyTracker<ITERATIONS>>();
    
    for (int i = 0; i < ITERATIONS; ++i) {
        uint64_t start = rdtsc();
        uint64_t end = rdtsc();
        tracker->record(end - start);
    }
    
    tracker->compute_percentiles();
    
    std::cout << "=== RDTSC Overhead ===\n";
    std::cout << "Mean:  " << tracker->mean() << " cycles\n";
    std::cout << "P50:   " << tracker->p50() << " cycles\n";
    std::cout << "P99:   " << tracker->p99() << " cycles\n";
    std::cout << "P99.9: " << tracker->p999() << " cycles\n";
}

void benchmark_order_book_update() {
    constexpr int ITERATIONS = 100000;
    auto tracker = std::make_unique<LatencyTracker<ITERATIONS>>();
    
    OrderBook book;
    
    // Pre-populate with fixed-point prices using PRICE_SCALER
    for (int i = 0; i < 10; ++i) {
        book.update_bid((100 * PRICE_SCALER) - i * 100, 100);
        book.update_ask((101 * PRICE_SCALER) + i * 100, 100);
    }
    
    for (int i = 0; i < ITERATIONS; ++i) {
        uint64_t start = rdtscp();
        book.update_bid(100 * PRICE_SCALER, 500 + i % 100);
        uint64_t end = rdtscp();
        tracker->record(end - start);
    }
    
    tracker->compute_percentiles();
    
    std::cout << "\n=== Order Book Update ===\n";
    std::cout << "Mean:  " << tracker->mean() << " cycles\n";
    std::cout << "P50:   " << tracker->p50() << " cycles\n";
    std::cout << "P99:   " << tracker->p99() << " cycles\n";
    std::cout << "P99.9: " << tracker->p999() << " cycles\n";
}

int main() {
    g_tsc_calibrator.calibrate();
    std::cout << "TSC Frequency: " << g_tsc_calibrator.get_frequency_ghz() << " GHz\n\n";
    
    benchmark_rdtsc();
    benchmark_order_book_update();
    
    return 0;
}