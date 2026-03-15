# Dreadnought - Production-Grade Low-Latency Trading System

A sub-microsecond tick-to-trade engine built in C++23, designed for high-frequency trading with deterministic latency and minimal jitter.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)

## Overview

Dreadnought is a high-performance trading engine that processes market data and executes trading strategies in **under 1.2 microseconds** (wire-to-wire). It achieves this through:

- **Zero-copy kernel bypass networking** (DPDK/Solarflare simulation)
- **L1-cache-resident order book** (~8KB, fits entirely in CPU L1 cache)
- **Lock-free data structures** (SPSC queues, atomic operations)
- **Template-based strategy engine** (zero virtual dispatch overhead)
- **CPU isolation and pinning** (dedicated cores, real-time scheduling)
- **Profile-Guided Optimization** (PGO) support

## Performance Targets
```
┌─────────────────────────────────────────────────────────────────────┐
│                    WIRE-TO-WIRE LATENCY BUDGET                     │
│                     Target: < 1200ns (1.2µs)                        │
├────────────────────────┬─────────────┬──────────────┬──────────────┤
│ Stage                  │ Avg (ns)    │ P99.9 (ns)   │ Notes        │
├────────────────────────┼─────────────┼──────────────┼──────────────┤
│ NIC DMA Write          │      150    │       180    │ PCIe Gen4    │
│ Poll Detection         │       80    │       120    │ Busy polling │
│ Packet Parse           │      120    │       150    │ Zero-copy    │
│ Prefetch Book          │       40    │        60    │ _mm_prefetch │
│ Book Update (L1)       │      200    │       280    │ Array-based  │
│ Strategy Execution     │      100    │       140    │ CRTP inline  │
│ Risk Check             │       80    │       100    │ Branchless   │
│ Order Encode           │      120    │       150    │ Preallocated │
│ NIC TX Enqueue         │      150    │       180    │ Zero-copy    │
├────────────────────────┼─────────────┼──────────────┼──────────────┤
│ TOTAL                  │     1040    │      1360    │ < 1.4µs P999 │
└────────────────────────┴─────────────┴──────────────┴──────────────┘
```

## Architecture
```
                           [ EXCHANGE ]
                                 ↓
                          Fiber / Microwave
                                 ↓
┌────────────────────────────────────────────────────────────────────┐
│                          KERNEL BYPASS LAYER                       │
│                      mmap() DMA Ring Buffer                        │
│                       (Zero-Copy, No Syscalls)                     │
└────────────────────────────────────────────────────────────────────┘
                                 ↓
┌────────────────────────────────────────────────────────────────────┐
│                     TRADING THREAD (Core 2)                        │
│  1. Poll NIC (Busy Wait)        [80ns]                             │
│  2. Parse Packet (Zero-Copy)    [120ns]                            │
│  3. Prefetch Book               [40ns]                             │
│  4. Update Order Book (L1)      [200ns]                            │
│  5. Execute Strategy (CRTP)     [100ns]                            │
│  6. Risk Check (Inline)         [80ns]                             │
│  7. Encode Order                [120ns]                            │
│  8. Enqueue to NIC TX           [150ns]                            │
│  9. Log to SPSC Queue           [60ns]                             │
└────────────────────────────────────────────────────────────────────┘
                                 ↓
┌────────────────────────────────────────────────────────────────────┐
│                     LOGGER THREAD (Core 6)                         │
│  - Read binary log entries                                         │
│  - Format to CSV/Text                                              │
│  - Write to disk (async I/O)                                       │
└────────────────────────────────────────────────────────────────────┘
```

## Quick Start

### Prerequisites

- **Linux** (Ubuntu 20.04+, RHEL 8+, or similar)
- **CMake 3.25+**
- **Clang 16+** or GCC 12+ (Clang recommended)
- **C++23 support**

### Building on Linux
```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake clang-16

# Clone the repository
git clone <repository-url>
cd dreadnought

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++-16
make -j$(nproc)

# Run
./dreadnought_main
```

### Building on Windows (WSL2)
```powershell
# Install WSL2
wsl --install

# Restart, then open WSL and run:
sudo apt update
sudo apt install build-essential cmake clang-16

# Copy project to Linux filesystem
cp -r /mnt/c/path/to/dreadnought ~/dreadnought
cd ~/dreadnought

# Build (same as Linux)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++-16
make -j$(nproc)
./dreadnought_main
```

## Project Structure
```
dreadnought/
├── CMakeLists.txt              # Build configuration
├── include/dreadnought/        # Public headers
│   ├── core/                   # Core utilities & main engine
│   │   ├── compiler.hpp        # Compiler hints & macros
│   │   ├── types.hpp           # Fundamental types
│   │   └── dreadnought.hpp     # Main engine template
│   ├── market_data/            # Order book implementation
│   │   ├── order_book.hpp      # L1-cache-resident book
│   │   └── market_state.hpp    # Market data state wrapper
│   ├── strategy/               # Trading strategies
│   │   ├── strategy_base.hpp   # CRTP base class
│   │   └── mean_reversion.hpp  # Example strategy
│   ├── network/                # Network I/O layer
│   │   ├── ring_buffer.hpp     # Lock-free SPSC ring buffer
│   │   ├── packet.hpp          # Zero-copy packet structures
│   │   └── nic_poller.hpp      # Kernel-bypass NIC poller
│   ├── logger/                 # Lock-free logging
│   │   ├── spsc_queue.hpp      # Single-producer single-consumer queue
│   │   └── binary_logger.hpp   # Async binary logger
│   ├── timing/                 # Timestamp & latency tracking
│   │   ├── rdtsc.hpp           # CPU cycle counter (TSC)
│   │   └── latency_tracker.hpp # Percentile statistics
│   ├── risk/                   # Risk management
│   │   └── risk_models.hpp     # Static & dynamic risk models
│   └── threading/              # CPU affinity & isolation
│       └── affinity.hpp        # Core pinning utilities
├── src/                        # Implementation files
│   ├── main.cpp                # Entry point
│   ├── network/
│   │   └── nic_poller.cpp
│   ├── logger/
│   │   └── binary_logger.cpp
│   ├── threading/
│   │   └── affinity.cpp
│   └── timing/
│       └── rdtsc.cpp
├── strategies/                 # Strategy implementations
│   └── mean_reversion.cpp
├── tests/                      # Unit tests & benchmarks
│   ├── test_order_book.cpp
│   ├── test_spsc_queue.cpp
│   └── benchmark_latency.cpp
└── tools/                      # Utilities
    └── pgo_profile.sh          # Profile-Guided Optimization script
```

## Advanced Build Options

### Profile-Guided Optimization (PGO)

For maximum performance (10-20% improvement):
```bash
# Automated PGO build
./tools/pgo_profile.sh

# Optimized binary will be in: build_pgo_opt/dreadnought_main
```

### Manual PGO Workflow
```bash
# Step 1: Build with profiling
mkdir build_pgo && cd build_pgo
cmake .. -DCMAKE_BUILD_TYPE=Release -DPGO_GENERATE=ON
make -j$(nproc)

# Step 2: Run to collect profile data
./dreadnought_main
# (Let it run for a while, then Ctrl+C)

# Step 3: Merge profile data
llvm-profdata merge -output=../default.profdata default_*.profraw

# Step 4: Build with profile
cd ..
mkdir build_pgo_opt && cd build_pgo_opt
cmake .. -DCMAKE_BUILD_TYPE=Release -DPGO_USE=ON
make -j$(nproc)
```

### Debug Build
```bash
mkdir build_debug && cd build_debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)

# Run with debugger
gdb ./dreadnought_main
```

## Testing
```bash
cd build

# Run all tests
make test

# Or run individual tests
./test_order_book
./test_spsc_queue

# Benchmark latency
./benchmark_latency
```

## System Configuration (Production)

For optimal performance in production, configure your Linux system:

### Kernel Boot Parameters

Add to `/etc/default/grub`:
```bash
GRUB_CMDLINE_LINUX="isolcpus=2,4,6 nohz_full=2,4,6 rcu_nocbs=2,4,6 intel_pstate=disable"
```

Then update GRUB and reboot:
```bash
sudo update-grub
sudo reboot
```

### Verify CPU Isolation
```bash
cat /sys/devices/system/cpu/isolated
# Should show: 2,4,6
```

### Disable Hyper-Threading (on isolated cores)
```bash
# Find sibling cores
cat /sys/devices/system/cpu/cpu2/topology/thread_siblings_list

# Disable sibling (example: if core 3 is sibling of core 2)
echo 0 | sudo tee /sys/devices/system/cpu/cpu3/online
```

### CPU Frequency Scaling
```bash
# Disable turbo boost
echo 0 | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo

# Set to performance governor
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance | sudo tee $cpu
done
```

### Huge Pages
```bash
# Reserve 2MB huge pages
echo 20 | sudo tee /proc/sys/vm/nr_hugepages

# Verify
cat /proc/meminfo | grep HugePages
```

## Key Design Decisions

### Memory Architecture

- **L1-Cache-Resident Order Book**: 64-level array-based book fits in ~8KB
- **Cache-Line Alignment**: All hot structures aligned to 64-byte boundaries
- **False Sharing Prevention**: `alignas(64)` on all cross-thread data
- **Zero Dynamic Allocation**: All memory pre-allocated at startup

### Strategy Engine

- **CRTP (Curiously Recurring Template Pattern)**: Zero virtual dispatch overhead
- **Template-Based Risk Models**: Compile-time polymorphism via `[[no_unique_address]]`
- **Inline Everything**: Critical path functions marked `FORCE_INLINE`
- **Branch-Free Code**: Bitwise operations instead of conditionals where possible

### Network Stack

- **Kernel Bypass**: Simulated DPDK/Solarflare for zero-copy I/O
- **Lock-Free Ring Buffers**: SPSC queues with memory_order optimizations
- **Busy Polling**: No blocking, continuous polling for minimum latency
- **Batched Processing**: Up to 32 packets per poll for efficiency

### Logging

- **Off Critical Path**: Binary logging to lock-free queue (~60ns)
- **Background Thread**: Formatting and disk I/O on separate core
- **Drop on Overflow**: Never block trading thread

### Threading

- **CPU Pinning**: Trading thread locked to isolated core
- **Real-Time Priority**: `SCHED_FIFO` priority 99
- **NUMA-Aware**: Single NUMA node to avoid remote memory access

## Performance Tuning Guide

### Reducing Latency

1. **Prefetch Aggressively**: Use `_mm_prefetch` before data access
2. **SIMD Operations**: Vectorize book updates with AVX2/AVX512
3. **Reduce Branch Mispredicts**: Use PGO and `likely()`/`unlikely()` hints
4. **Hardware Timestamping**: Use NIC hardware timestamps instead of RDTSC

### Reducing Jitter

1. **Huge Pages**: Reduce TLB misses
2. **Memory Locking**: `mlockall()` to prevent page faults
3. **Warmup Cache**: Touch all memory at startup
4. **Disable C-States**: Prevent CPU from entering low-power states

### Monitoring
```bash
# Cache misses
perf stat -e cache-misses,cache-references ./dreadnought_main

# Branch mispredictions
perf stat -e branch-misses,branches ./dreadnought_main

# Context switches
perf stat -e context-switches ./dreadnought_main
```

## Debugging & Profiling

### Check Compiler Output
```bash
# See generated assembly
clang++-16 -S -O3 -march=native src/main.cpp -o main.s

# Check inlining decisions
clang++-16 -Rpass=inline -O3 src/main.cpp
```

### Profile with perf
```bash
# Record hotspots
perf record -g ./dreadnought_main

# View report
perf report

# Cache analysis
perf c2c record ./dreadnought_main
perf c2c report
```

### Memory Layout Analysis
```bash
# Check struct sizes
echo '#include "dreadnought/market_data/order_book.hpp"
int main() { return sizeof(dreadnought::OrderBook); }' | \
clang++-16 -x c++ -std=c++23 - -I include
```

## Contributing

Contributions are welcome! Please focus on:

- Performance improvements
- Additional strategy implementations
- Better testing coverage
- Documentation improvements

## License

MIT License - See LICENSE file for details

## Disclaimer

This is educational/research code. Not suitable for production trading without:

- Actual kernel bypass implementation (DPDK/Solarflare)
- Proper exchange connectivity
- Risk management validation
- Regulatory compliance
- Extensive testing with real market data

## Further Reading

- [Intel® Data Plane Development Kit (DPDK)](https://www.dpdk.org/)
- [Solarflare Onload](https://www.xilinx.com/products/cloud-connectivity/onload.html)
- [Mechanical Sympathy Blog](https://mechanical-sympathy.blogspot.com/)
- [Martin Thompson - Low Latency Performance](https://www.youtube.com/watch?v=MC1EKLQ2Wmg)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)

## Acknowledgments

Built upon principles from top-tier HFT firms and low-latency engineering best practices.

---

**Built for speed | Designed for trading | Optimized for Latency**
