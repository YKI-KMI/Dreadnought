#pragma once

#include <cstdint>

namespace dreadnought {

// CPU affinity management
class ThreadAffinity {
public:
    // Pin current thread to specific physical core
    static bool pin_to_core(int core_id) noexcept;
    
    // Set thread priority to realtime
    static bool set_realtime_priority() noexcept;
    
    // Isolate cores: isolcpus=2,4,6 nohz_full=2,4,6 rcu_nocbs=2,4,6
    // Core assignment strategy:
    // - Core 2: Market data thread (RX path)
    // - Core 4: Strategy + Order execution thread
    // - Core 6: Logger background thread
    // - Disable HyperThreading on these cores
    static constexpr int MARKET_DATA_CORE = 2;
    static constexpr int STRATEGY_CORE = 4;
    static constexpr int LOGGER_CORE = 6;
    
    // Prefetch next cache line for better pipelining
    static void warmup_cache(void* addr, size_t size) noexcept;
};

}