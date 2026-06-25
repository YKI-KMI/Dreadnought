# ⚡ Dreadnought

**Ultra-low-latency tick-to-trade engine written in C++23**

Dreadnought is a high-frequency trading (HFT) infrastructure framework targeting single-digit microsecond tick-to-trade latency. It combines kernel-bypass networking, TSC-based nanosecond timing, cache-resident data structures, and a template-composable strategy layer into a single cohesive system.

> **Status:** Research / framework stage. NIC poller is simulated; swap in DPDK/Solarflare/Mellanox for production.

---

## Table of Contents

- [Architecture Overview](#architecture-overview)
- [Key Features](#key-features)
- [Performance Characteristics](#performance-characteristics)
- [Project Structure](#project-structure)
- [Prerequisites](#prerequisites)
- [Building](#building)
- [Running](#running)
- [Benchmarks](#benchmarks)
- [Tests](#tests)
- [Configuration](#configuration)
- [Writing a Custom Strategy](#writing-a-custom-strategy)
- [Kernel Tuning (Production)](#kernel-tuning-production)
- [Roadmap](#roadmap)
- [License](#license)

---

## Architecture Overview

```
Market Data (NIC / Multicast)
        │
        ▼
┌──────────────────────┐
│     NIC Poller       │  Kernel-bypass RX ring (simulated / DPDK drop-in)
│  rx_ring: 4096 pkts  │
└──────────┬───────────┘
           │  Packet batch (≤32)
           ▼
┌──────────────────────┐
│    Order Book        │  L1-resident, 8 KB, 64 price levels per side
│  update_bid / ask    │  Binary search + memmove, aligned to cache line
└──────────┬───────────┘
           │  best_bid / best_ask / VAMP
           ▼
┌──────────────────────┐
│  Strategy (template) │  MeanReversionStrategy<RiskModel>
│  on_tick → signal    │  EMA + variance, micro-price (VAMP), imbalance filter
└──────────┬───────────┘
           │  OrderPacket
           ▼
┌──────────────────────┐
│    NIC Poller TX     │  Zero-copy send to exchange
└──────────────────────┘
           │  async, non-blocking
           ▼
┌──────────────────────┐
│   Binary Logger      │  SPSC queue → background writer thread
│   65 536-entry queue │  Drops on overflow — hot path never blocks
└──────────────────────┘
```

Every stage is pinned to an isolated CPU core, aligned to cache-line boundaries (64 B), and measured end-to-end with RDTSC/RDTSCP.

---

## Key Features

| Feature | Detail |
|---|---|
| **Language** | C++23 (`-std=c++23`) |
| **Compiler** | Clang (strongly recommended); GCC supported |
| **Timing** | `RDTSC` / `RDTSCP` + `CLOCK_MONOTONIC` calibration — nanosecond precision |
| **Order Book** | L1-resident 8 KB struct, 64 levels/side, cache-aligned, O(1) TOB fast-path |
| **Strategy Layer** | CRTP policy template — zero virtual dispatch, zero heap allocation |
| **Risk Models** | `StaticRiskModel` (zero-overhead via `[[no_unique_address]]`) and `DynamicRiskModel` |
| **Networking** | Simulated kernel-bypass NIC with pluggable DPDK/Solarflare interface |
| **Logger** | Lock-free SPSC binary logger; background flush thread; never blocks hot path |
| **Thread Pinning** | `pthread_setaffinity_np` + `SCHED_FIFO` realtime priority |
| **Compiler Flags** | `-O3 -march=native -flto=thin -fno-exceptions -fno-rtti -ffast-math` |
| **PGO** | Profile-Guided Optimisation via CMake flags + `tools/pgo_profile.sh` |
| **Latency Tracking** | 1 000 000-sample `LatencyTracker<N>` — mean, P50, P99, P99.9, min, max |
| **Build System** | CMake ≥ 3.25, `compile_commands.json`, clangd LSP support |

---

## Performance Characteristics

All figures are CPU-cycle counts measured on the build host with RDTSCP.

| Operation | Typical P50 | P99 |
|---|---|---|
| RDTSC overhead | ~8 cycles | ~12 cycles |
| Order book top-of-book update | ~15 cycles | ~30 cycles |
| Full tick-to-trade (warm) | ~200–600 ns | < 1 µs |

Run `./build/benchmark_latency` on your hardware to get numbers specific to your CPU.

---

## Project Structure

```
Dreadnought/
├── include/dreadnought/
│   ├── core/
│   │   ├── compiler.hpp        # FORCE_INLINE, HOT_PATH, COLD_PATH, CPU_PAUSE, PREFETCH_*
│   │   ├── dreadnought.hpp     # Main engine template (Dreadnought<Strategy>)
│   │   └── types.hpp           # Price, Quantity, OrderID, Side, PriceLevel, etc.
│   ├── logger/
│   │   ├── binary_logger.hpp   # Hot-path SPSC → background file writer
│   │   └── spsc_queue.hpp      # Lock-free single-producer / single-consumer queue
│   ├── market_data/
│   │   ├── market_state.hpp    # MarketDataState wrapper
│   │   └── order_book.hpp      # L1-resident OrderBook (8 KB, 64 levels/side)
│   ├── network/
│   │   ├── nic_poller.hpp      # Kernel-bypass NIC abstraction
│   │   ├── packet.hpp          # Packet / OrderPacket wire formats
│   │   └── ring_buffer.hpp     # Lock-free RX/TX ring
│   ├── risk/
│   │   └── risk_models.hpp     # StaticRiskModel, DynamicRiskModel
│   ├── strategy/
│   │   ├── mean_reversion.hpp  # MeanReversionStrategy<RiskModel> (EMA + VAMP)
│   │   └── strategy_base.hpp   # CRTP StrategyBase<Derived>
│   ├── threading/
│   │   └── affinity.hpp        # CPU pinning + SCHED_FIFO
│   └── timing/
│       ├── latency_tracker.hpp # Preallocated percentile tracker
│       └── rdtsc.hpp           # rdtsc(), rdtscp(), TSCCalibrator
├── src/
│   ├── main.cpp                # Entry point — init, run loop, shutdown, stats
│   ├── logger/binary_logger.cpp
│   ├── network/nic_poller.cpp
│   ├── threading/affinity.cpp
│   └── timing/rdtsc.cpp
├── strategy/
│   └── mean_reversion.cpp      # Strategy implementation
├── tests/
│   ├── benchmark_latency.cpp   # RDTSC + order book microbenchmarks
│   ├── test_order_book.cpp     # Correctness tests for OrderBook
│   └── test_spsc_queue.cpp     # SPSC queue tests
├── tools/
│   └── pgo_profile.sh          # Profile-Guided Optimisation helper
└── CMakeLists.txt
```

---

## Prerequisites

| Dependency | Version | Notes |
|---|---|---|
| CMake | ≥ 3.25 | |
| Clang | ≥ 16 (recommended) | GCC ≥ 13 also works; PGO flags differ |
| Linux | Any modern kernel | `pthread`, `SCHED_FIFO`, `CLOCK_MONOTONIC` |
| x86-64 CPU | with `RDTSC` / `RDTSCP` | Required for timing primitives |

**Optional for production:**

- DPDK ≥ 23.x — replace `NICPoller` internals
- Isolated CPU cores via kernel boot params (`isolcpus`, `nohz_full`, `rcu_nocbs`)

---

## Building

### Standard Release Build

```bash
git clone https://github.com/your-org/dreadnought.git
cd dreadnought
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++
make -j$(nproc)
```

### Debug Build (with frame pointers for profiling)

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
make -j$(nproc)
```

### Profile-Guided Optimisation (PGO)

```bash
# Step 1 — instrument build
cmake .. -DPGO_GENERATE=ON -DCMAKE_CXX_COMPILER=clang++
make -j$(nproc)

# Step 2 — run representative workload
./dreadnought_main   # Ctrl+C after a warm run

# Step 3 — merge profiles and rebuild
bash ../tools/pgo_profile.sh

# Step 4 — optimised build using profile data
cmake .. -DPGO_USE=ON
make -j$(nproc)
```

> PGO typically yields an additional 5–15% latency reduction on hot paths.

---

## Running

```bash
./build/dreadnought_main
```

Expected output:

```
=== Dreadnought Tick-to-Trade Engine ===
Initializing...
Engine initialized
TSC Frequency: 3.80 GHz (Calibrated)
Starting main loop...

^C
Shutdown requested...

=== Latency Statistics ===
Mean:  234 ns
P50:   198 ns
P99:   512 ns
P99.9: 890 ns
Min:   142 ns
Max:   2341 ns
```

The engine logs all events to `/tmp/dreadnought.log` in binary format. Ctrl+C triggers a clean shutdown and prints full percentile statistics.

---

## Benchmarks

```bash
./build/benchmark_latency
```

Runs two independent microbenchmarks (100 000 iterations each):

- **RDTSC Overhead** — measures the raw cost of the timing primitive itself
- **Order Book Update** — measures a top-of-book bid update with a pre-populated 10-level book

Results are reported in CPU cycles. Divide by your TSC frequency (printed at startup) for nanoseconds.

---

## Tests

```bash
cd build
ctest --output-on-failure
```

Test suites:

| Suite | What it covers |
|---|---|
| `test_order_book` | Basic inserts, level ordering, removals, spread/mid-price calculations |
| `test_spsc_queue` | SPSC push/pop, overflow behaviour, producer–consumer ordering |

---

## Configuration

All compile-time constants live in the headers — no runtime config files.

| Constant | Location | Default | Description |
|---|---|---|---|
| `MAX_PRICE_LEVELS` | `types.hpp` | `64` | Price levels per side in `OrderBook` |
| `MAX_SYMBOL_COUNT` | `types.hpp` | `16` | Maximum simultaneous symbols |
| `PRICE_SCALER` | `types.hpp` | `10000` | Fixed-point scale (4 decimal places) |
| `RX_RING_SIZE` | `nic_poller.hpp` | `4096` | NIC receive ring depth |
| `TX_RING_SIZE` | `nic_poller.hpp` | `1024` | NIC transmit ring depth |
| `QUEUE_SIZE` | `binary_logger.hpp` | `65536` | SPSC logger queue depth |
| `MARKET_DATA_CORE` | `affinity.hpp` | `2` | CPU core for RX / market data |
| `STRATEGY_CORE` | `affinity.hpp` | `4` | CPU core for strategy execution |
| `LOGGER_CORE` | `affinity.hpp` | `6` | CPU core for logger background thread |
| `MeanReversionStrategy::ALPHA` | `mean_reversion.hpp` | `0.1` | EMA smoothing factor |
| `MeanReversionStrategy::ENTRY_THRESHOLD_SQ` | `mean_reversion.hpp` | `4.0` | Entry z-score² threshold |
| `MeanReversionStrategy::EXIT_THRESHOLD_SQ` | `mean_reversion.hpp` | `0.25` | Exit z-score² threshold |
| `StaticRiskModel::MAX_POSITION` | `risk_models.hpp` | `1000` | Max net position |
| `StaticRiskModel::MAX_ORDER_SIZE` | `risk_models.hpp` | `500` | Max single-order quantity |

---

## Writing a Custom Strategy

Strategies are CRTP structs injected at compile time — no virtual calls, no heap.

```cpp
// my_strategy.hpp
#include "dreadnought/strategy/strategy_base.hpp"
#include "dreadnought/risk/risk_models.hpp"

namespace dreadnought {

template<typename RiskModel>
struct MyStrategy : StrategyBase<MyStrategy<RiskModel>> {
    [[no_unique_address]] RiskModel risk_model;
    Signal current_signal;

    FORCE_INLINE void on_tick_impl(const OrderBook& book, Timestamp ts) noexcept {
        // Your signal logic here
        // book.best_bid(), book.best_ask(), book.mid_price(), book.spread() ...
    }

    FORCE_INLINE bool should_send_order_impl() const noexcept {
        return current_signal.valid &&
               risk_model.check_position_limit(/*...*/);
    }

    FORCE_INLINE Side     get_order_side_impl()  const noexcept { return current_signal.side; }
    FORCE_INLINE Price    get_order_price_impl() const noexcept { return current_signal.price; }
    FORCE_INLINE Quantity get_order_qty_impl()   const noexcept { return current_signal.qty; }
};

} // namespace dreadnought
```

Then wire it up in `main.cpp`:

```cpp
using StrategyType = MyStrategy<StaticRiskModel>;
auto engine = std::make_unique<Dreadnought<StrategyType>>();
```

That's it — no registration, no factory, no runtime dispatch.

---

## Kernel Tuning (Production)

For sub-microsecond latency, the OS must be configured to stay out of the way.

### Boot Parameters (`/etc/default/grub`)

```
GRUB_CMDLINE_LINUX="isolcpus=2,4,6 nohz_full=2,4,6 rcu_nocbs=2,4,6 \
  mitigations=off nosmt intel_idle.max_cstate=0 idle=poll"
```

Apply with `sudo update-grub && sudo reboot`.

### Runtime Tuning

```bash
# Disable CPU frequency scaling
for cpu in 2 4 6; do
  echo performance | sudo tee /sys/devices/system/cpu/cpu${cpu}/cpufreq/scaling_governor
done

# Disable NUMA balancing
sudo sh -c 'echo 0 > /proc/sys/kernel/numa_balancing'

# Increase socket buffer sizes (UDP multicast)
sudo sysctl -w net.core.rmem_max=134217728
sudo sysctl -w net.core.rmem_default=134217728

# Lock memory to prevent paging
ulimit -l unlimited
```

### Disable Hyper-Threading on isolated cores

```bash
# Disable the HT sibling of core 2 (find via /sys/devices/system/cpu/cpu*/topology/)
echo 0 | sudo tee /sys/devices/system/cpu/cpu3/online
```

---

## Roadmap

- [ ] DPDK / Solarflare / Mellanox NIC integration
- [ ] FIX / ITCH / OUCH protocol parsers
- [ ] Multi-symbol order book manager
- [ ] Replay engine for historical backtesting (`tools/replay_engine.cpp` stub exists)
- [ ] Market-making strategy template
- [ ] Risk model hot-reload without restart
- [ ] Prometheus metrics endpoint (off critical path)
- [ ] CI pipeline with latency regression tests

---

## License

MIT License — see [LICENSE](LICENSE) for details.

---

*Built for speed. Measured in nanoseconds.*