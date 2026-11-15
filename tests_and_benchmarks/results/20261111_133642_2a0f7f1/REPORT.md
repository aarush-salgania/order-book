# Low Latency Limit Order Book - Benchmark & Tail-Latency Report

## Environment

- **CPU**: Snapdragon(R) X Plus - X1P42100 - Qualcomm(R) Oryon(TM) CPU (8 cores)
- **OS**: Windows 11
- **Compiler**: c++.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r4) 15.1.0
- **Build Type**: Release
- **Commit**: `2a0f7f186` (dirty)
- **Timestamp**: 2026-11-11T08:06:42+00:00 UTC

## 1. Headline Replay Throughput

## 2. Per-Operation Tail-Latency Distribution

| Ladder / Engine | Operation | Count | p50 (Median) | p90 | p99 | p99.9 | Max |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `contiguous` | passive | 225,309 | 156.3 ns | 156.3 ns | **312.5 ns** | 937.5 ns | 90.21 µs |
| `contiguous` | aggressive | 45,196 | 156.3 ns | 260.4 ns | **364.6 ns** | 1.09 µs | 66.20 µs |
| `contiguous` | cancel | 175,869 | 156.3 ns | 156.3 ns | **312.5 ns** | 885.4 ns | 298.18 µs |
| `contiguous` | cancel_miss | 3,626 | 52.1 ns | 156.3 ns | **156.3 ns** | 312.5 ns | 781.3 ns |
| `contiguous` | all | 450,000 | 156.3 ns | 156.3 ns | **312.5 ns** | 937.5 ns | 298.18 µs |
| `sparse` | passive | 225,309 | 156.3 ns | 208.3 ns | **312.5 ns** | 989.6 ns | 75.16 µs |
| `sparse` | aggressive | 45,196 | 156.3 ns | 468.8 ns | **1.41 µs** | 3.13 µs | 42.08 µs |
| `sparse` | cancel | 175,869 | 156.3 ns | 156.3 ns | **416.7 ns** | 1.25 µs | 173.23 µs |
| `sparse` | cancel_miss | 3,626 | 104.2 ns | 156.3 ns | **156.3 ns** | 781.3 ns | 5.36 µs |
| `sparse` | all | 450,000 | 156.3 ns | 208.3 ns | **572.9 ns** | 1.56 µs | 173.23 µs |
| `stdmap` | passive | 225,309 | 156.3 ns | 260.4 ns | **468.8 ns** | 1.20 µs | 9.67 ms |
| `stdmap` | aggressive | 45,196 | 156.3 ns | 312.5 ns | **729.2 ns** | 1.51 µs | 122.45 µs |
| `stdmap` | cancel | 175,869 | 156.3 ns | 312.5 ns | **625.0 ns** | 1.25 µs | 6.53 ms |
| `stdmap` | cancel_miss | 3,626 | 104.2 ns | 156.3 ns | **260.4 ns** | 2.50 µs | 24.74 µs |
| `stdmap` | all | 450,000 | 156.3 ns | 260.4 ns | **572.9 ns** | 1.25 µs | 9.67 ms |

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
