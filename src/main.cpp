#include "dreadnought/core/dreadnought.hpp"
#include "dreadnought/strategy/mean_reversion.hpp"
#include "dreadnought/risk/risk_models.hpp"
#include "dreadnought/timing/rdtsc.hpp"
#include <iostream>
#include <csignal>
#include <atomic>

using namespace dreadnought;

// Global instance for signal handler
std::atomic<bool> g_shutdown_requested{false};

void signal_handler(int signal) {
    g_shutdown_requested.store(true, std::memory_order_release);
}

// Instantiate TSC calibrator
TSCCalibrator dreadnought::g_tsc_calibrator;

int main(int argc, char** argv) {
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << "=== Dreadnought Tick-to-Trade Engine ===\n";
    std::cout << "Initializing...\n";
    
    // Instantiate strategy: MeanReversion with StaticRiskModel
    using StrategyType = MeanReversionStrategy<StaticRiskModel>;
    Dreadnought<StrategyType> engine;
    
    if (!engine.init("/tmp/dreadnought.log")) {
        std::cerr << "Failed to initialize engine\n";
        return 1;
    }
    
    std::cout << "Engine initialized\n";
    std::cout << "TSC Frequency: " << g_tsc_calibrator.get_frequency_ghz() << " GHz\n";
    std::cout << "Starting main loop...\n";
    
    // Run in separate thread to allow clean shutdown
    std::thread engine_thread([&engine]() {
        engine.run();
    });
    
    // Wait for shutdown signal
    while (!g_shutdown_requested.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "\nShutdown requested...\n";
    engine.shutdown();
    engine_thread.join();
    
    // Print latency statistics
    const auto& tracker = engine.get_latency_tracker();
    std::cout << "\n=== Latency Statistics ===\n";
    std::cout << "Mean:  " << tracker.mean() << " ns\n";
    std::cout << "P50:   " << tracker.p50() << " ns\n";
    std::cout << "P99:   " << tracker.p99() << " ns\n";
    std::cout << "P99.9: " << tracker.p999() << " ns\n";
    std::cout << "Min:   " << tracker.min() << " ns\n";
    std::cout << "Max:   " << tracker.max() << " ns\n";
    
    return 0;
}