# Low-Latency Limit Order Book: Testing & Benchmarking Pipeline

A production-grade correctness testing, throughput benchmarking, and tail-latency profiling pipeline for the low-latency Limit Order Book (`core/`).

Directly integrates with the repository's core implementation without modifying any core source files. Fully self-contained with **zero external network dependencies**—runs out-of-the-box on Windows (PowerShell, Command Prompt), Linux, and macOS.

---

## Quick Start

### 1. Run Correctness Tests
Runs unit tests for the memory pool, price ladders, book matching engine, differential fuzzing against a reference book oracle, and the user's legacy test suite.

```powershell
# Windows PowerShell
.\tests_and_benchmarks\scripts\run_tests.ps1

# Windows Command Prompt
tests_and_benchmarks\scripts\run_tests.bat

# Linux / macOS / Git Bash
./tests_and_benchmarks/scripts/run_tests.sh
```

To run tests with specific filters or check the known bugs in `BookCore`:
```powershell
# Run the 3 tests that fail due to known bugs in BookCore:
.\tests_and_benchmarks\scripts\run_tests.ps1 -KnownBugs

# Filter specific tests:
.\tests_and_benchmarks\scripts\run_tests.ps1 -Filter "Pool"
```

---

### 2. Run Benchmarks & Tail-Latency Profiler
Builds an optimized `Release` binary (`-O3`), runs the throughput suite and the cycle-accurate RDTSC tail-latency harness, and generates the final submission report: `results/<timestamp>_<commit>/REPORT.md`.

```powershell
# Windows PowerShell
.\tests_and_benchmarks\scripts\run_benchmarks.ps1

# Windows Command Prompt
tests_and_benchmarks\scripts\run_benchmarks.bat

# Linux / macOS / Git Bash
./tests_and_benchmarks/scripts/run_benchmarks.sh
```

---

### 3. Compare Against a Baseline Run
Compare your current branch / optimizations against a previous run or the sample baseline to quantify speedups or detect regressions:

```powershell
# Windows PowerShell
.\tests_and_benchmarks\scripts\run_benchmarks.ps1 -Baseline "tests_and_benchmarks\results\sample"

# Using the Python summarizer directly on any directory:
python tests_and_benchmarks/scripts/summarize.py report tests_and_benchmarks/results/<your_run> --baseline tests_and_benchmarks/results/sample
```
The report annotates any performance improvements with `✅` and flags regressions exceeding 5% with `⚠️`.

---

## Pipeline Architecture

```
tests_and_benchmarks/
├── CMakeLists.txt              # Standard C++20 CMake build configuration
├── README.md                   # Complete documentation & benchmark analysis
├── harness/
│   ├── core_shim.hpp           # Bridges directly to core/ (order_book_core, mempool, order, types)
│   ├── reference_book.hpp      # Canonical StdMapBook oracle (std::map + std::list)
│   ├── workload.hpp            # Deterministic synthetic order flow generator (SplitMix64)
│   ├── test_framework.hpp      # Zero-dependency GTest-compatible unit test engine
│   └── bench_framework.hpp     # Zero-dependency Google Benchmark-compatible throughput engine
├── tests/
│   ├── main_tests.cpp          # Test runner entry point
│   ├── test_mempool.cpp        # Pool<T> allocation, capacity, LIFO reuse, pointer_to_handle
│   ├── test_price_levels.cpp   # Contiguous & Sparse ladder contracts, gap scans, bounds
│   ├── test_book.cpp           # BookCore matching, FIFO, sweeps, cancels, differential tests
│   └── test_user_suite.cpp     # Wrapper executing the existing Tests/test_suite.cpp
├── benchmarks/
│   ├── main_bench.cpp          # Throughput benchmark entry point
│   ├── bench_common.hpp        # Benchmark helpers (set_ns_per_op, near_touch_ticks, opaque)
│   ├── bench_mempool.cpp       # Mempool vs malloc/new/delete & IdIndex comparison
│   ├── bench_price_levels.cpp  # Contiguous vs Sparse level lookups & gap traversal
│   ├── bench_book.cpp          # 100K-op replay stream, add/cancel, match, sweep throughput
│   └── latency.cpp             # ob_latency tail-latency measurement tool (RDTSC + CPU pinning)
├── scripts/
│   ├── run_tests.bat           # Windows CMD test runner
│   ├── run_tests.ps1           # Windows PowerShell test runner
│   ├── run_tests.sh            # Linux / macOS / Git Bash test runner
│   ├── run_benchmarks.bat      # Windows CMD benchmark runner
│   ├── run_benchmarks.ps1      # Windows PowerShell benchmark runner
│   ├── run_benchmarks.sh       # Linux / macOS / Git Bash benchmark runner
│   └── summarize.py            # Python analysis tool generating REPORT.md and summary.csv
└── results/
    └── sample/                 # Pre-generated reference benchmark and latency report
```

---

## Detailed Tooling

### 1. Correctness Testing (`ob_tests`)
96 comprehensive test cases passing across all components:
- **`Pool<T>` Tests**: Initial capacity and in-use tracking, sequential allocation, handle stability, over-aligned types (`alignas(64)`), LIFO cache-line reuse, pointer-to-handle roundtripping, and 200,000-op churn without slot leaks.
- **`IPriceLevels` Contract**: Uniform behavior across both `PriceLevelContiguous` and `PriceLevelsSparse`. Tests verify sentinel best bid/ask values, head/tail linking, tick bounds, and best price recovery after level depletion.
- **`BookCore` Matching**:
  - Non-crossing limit orders rest and aggregate quantities at the same price level.
  - Crossing orders execute at resting prices with FIFO queue priority.
  - Multi-level sweeps consume liquidity across multiple price tiers in ascending/descending order.
  - Cancellations correctly update aggregate volume, preserve doubly-linked list integrity, and handle cancel misses (orders already filled or non-existent).
- **Differential Fuzzing**: Runs 50,000-order deterministic streams through both `BookCore` and the `StdMapBook` reference book simultaneously. Asserts exact op-by-op equality of filled volume, remaining quantity, best bid, best ask, and per-level aggregate liquidity across 5 distinct market regimes (balanced, aggressive-heavy, fast-moving fair value, deep book, and cancel-heavy).
- **User Test Suite**: Executes the original `TestSuite` in `Tests/test_suite.cpp`.

### 2. Throughput Benchmarks (`ob_bench`)
Calculates average operations per second and nanoseconds per operation:
- **`Book_Replay`**: Measures end-to-end matching throughput on realistic flow (50% passive, 10% aggressive, 40% cancel).
- **`Alloc` / `Pool`**: Quantifies raw allocation throughput under steady state and churn.
- **`PriceLevels`**: Evaluates cache locality and direct array indexing vs hash table lookup overhead.

### 3. Tail-Latency Profiler (`ob_latency`)
Averages disguise tail-risk. In high-frequency trading and matching engines, p99 and p99.9 latencies dictate whether an order gets filled or suffers adverse selection.

- **Hardware Timer**: Uses serialized `_mm_lfence()` + `__rdtsc()` / `__rdtscp()` on x86-64 (calibrated against `std::chrono::steady_clock`), with automatic measurement overhead subtraction.
- **CPU Affinity Pinning**: Eliminates OS thread scheduling jitter via `SetThreadAffinityMask` on Windows and `sched_setaffinity` on Linux (`--cpu=N`).
- **Percentile Breakdown**: Individually measures and reports p50, p90, p99, p99.9, p99.99, max, and mean latency for:
  - `passive`: Non-crossing limit order resting in the book.
  - `aggressive`: Crossing market / limit order matching against opposite resting liquidity.
  - `cancel`: Order cancellation hitting a resting order.
  - `cancel_miss`: Order cancellation missing due to prior fill or cancellation (race condition).
  - `all`: Aggregate latency across all order types.

---

## Benchmark Findings & Architectural Insights

| Component | Benchmark Metric | Observation / Performance Impact |
| :--- | :--- | :--- |
| **Memory Pool** | `Pool_HotPair` (2.1 ns) vs `NewDelete_HotPair` (52.9 ns) | `Pool<T>` is **24x faster** than system `new`/`delete` due to preallocated memory and LIFO free-list cache locality. Under high churn (64K live nodes), the pool is **146x faster** (20 ns vs 2.98 µs). |
| **Price Ladders** | `PriceLevelContiguous` (2.2 ns) vs `PriceLevelsSparse` (24–104 ns) | Contiguous ladder delivers deterministic O(1) direct indexing. Sparse ladder suffers hash computation, bucket chaining, and cache misses as active prices expand. |
| **Gap Scanning** | `FindBestAsk` (Contiguous vs Sparse across gaps) | When the best level empties, contiguous scans tick-by-tick (472 ns across 1,000 ticks). Sparse scans every bucket ever created (906 ns across 1,000 entries). |
| **Id Index** | `IdIndex_FlatRing` (1.1 ns) vs `IdIndex_UnorderedMap` (50.0 ns) | `std::unordered_map<OrderId, IdEntry>` allocates on insert. A flat ring index by `(order_id & mask)` eliminates allocation and speeds up lookups by **45x**. |

### Known Bugs Pinpointed by Test Suite (Run with `-KnownBugs`):
1. **Duplicate Order ID Index Corruption**: Re-using an existing order ID overwrites the `orders_` hash map entry. When the first order fills, it deletes the map entry, stranding the second order in the book forever (un-cancellable).
2. **Pool Saturation Silent Order Drop**: Once `order_pool_` reaches 100,000 orders, `alloc()` returns `NIL`. `submit_limit` returns `remaining = qty`, appearing as if the order was rested, but it is dropped from the book.
3. **Negative Order Quantity Accepted**: Orders with negative quantities are accepted and echoed back without input validation.

---

## Submission

The primary benchmark submission artifact is **`REPORT.md`**, generated automatically in:
```
tests_and_benchmarks/results/<timestamp>_<commit>/REPORT.md
```
A reference run is also archived at:
```
tests_and_benchmarks/results/sample/REPORT.md
```

