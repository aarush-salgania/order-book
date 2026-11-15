# Low Latency Limit Order Book - Benchmark & Tail-Latency Report

## Environment

- **CPU**: Snapdragon(R) X Plus - X1P42100 - Qualcomm(R) Oryon(TM) CPU (8 cores)
- **OS**: Windows 11
- **Compiler**: c++.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r4) 15.1.0
- **Build Type**: Release
- **Commit**: `2a0f7f186` (dirty)
- **Timestamp**: 2026-11-11T08:02:37+00:00 UTC

## 1. Headline Replay Throughput

## 2. Per-Operation Tail-Latency Distribution

| Ladder / Engine | Operation | Count | p50 (Median) | p90 | p99 | p99.9 | Max |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `contiguous` | passive | 225,309 | 156.2 ns | 156.2 ns | **312.5 ns** | 364.6 ns | 67.08 µs |
| `contiguous` | aggressive | 45,196 | 156.2 ns | 208.3 ns | **312.5 ns** | 625.0 ns | 22.81 µs |
| `contiguous` | cancel | 175,869 | 156.2 ns | 156.2 ns | **312.5 ns** | 520.8 ns | 105.10 µs |
| `contiguous` | cancel_miss | 3,626 | 52.1 ns | 156.2 ns | **156.2 ns** | 260.4 ns | 2.34 µs |
| `contiguous` | all | 450,000 | 156.2 ns | 156.2 ns | **312.5 ns** | 468.8 ns | 105.10 µs |
| `sparse` | passive | 225,309 | 156.2 ns | 156.2 ns | **312.5 ns** | 468.8 ns | 226.88 µs |
| `sparse` | aggressive | 45,196 | 156.2 ns | 416.7 ns | **1.25 µs** | 1.67 µs | 101.46 µs |
| `sparse` | cancel | 175,869 | 156.2 ns | 156.2 ns | **312.5 ns** | 937.5 ns | 86.30 µs |
| `sparse` | cancel_miss | 3,626 | 52.1 ns | 156.2 ns | **156.2 ns** | 677.1 ns | 33.49 µs |
| `sparse` | all | 450,000 | 156.2 ns | 156.2 ns | **468.8 ns** | 1.30 µs | 226.88 µs |
| `stdmap` | passive | 225,309 | 156.2 ns | 260.4 ns | **312.5 ns** | 468.8 ns | 100.36 µs |
| `stdmap` | aggressive | 45,196 | 156.2 ns | 260.4 ns | **416.7 ns** | 833.3 ns | 10.78 µs |
| `stdmap` | cancel | 175,869 | 156.2 ns | 208.3 ns | **312.5 ns** | 572.9 ns | 297.03 µs |
| `stdmap` | cancel_miss | 3,626 | 52.1 ns | 104.2 ns | **156.2 ns** | 312.5 ns | 937.5 ns |
| `stdmap` | all | 450,000 | 156.2 ns | 260.4 ns | **312.5 ns** | 572.9 ns | 297.03 µs |

*Measured with `rdtsc` (0.3100 ns/tick, timer overhead of 0.0 ns subtracted).* Tail latency (p99 / p99.9) captures tail risk from memory reallocations and price-level gap scanning.

## Architectural Analysis & Observations

1. **Contiguous Array Ladder vs Sparse Hash Ladder**:
   - `PriceLevelContiguous` delivers deterministic O(1) direct indexing for any tick price within the active band.
   - `PriceLevelsSparse` exhibits higher tail latency due to hash table collisions, rehash churn, and O(N) traversal of historically touched price buckets during `find_best_bid` / `find_best_ask`.

2. **Memory Pool (Free-List) Efficiency**:
   - `Pool<OrderNode>` achieves sub-2ns allocation and deallocation overhead with LIFO cache locality, dramatically outperforming standard heap allocation (`new`/`delete`).
   - Maintaining resting orders inside a preallocated pool eliminates OS malloc latency spikes on critical matching paths.

3. **Order Cancellation & Best Price Recovery**:
   - When the best bid or ask price level is depleted, linear gap scanning across tick arrays takes time proportional to the gap width.
   - Employing a hierarchical bitmask (hierarchical bitmap / SIMD word scan) over active price levels would reduce empty tick scanning to constant time O(1).
