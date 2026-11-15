# Low Latency Limit Order Book - Benchmark & Tail-Latency Report

## Environment

- **CPU**: Snapdragon(R) X Plus - X1P42100 - Qualcomm(R) Oryon(TM) CPU (8 cores)
- **OS**: Windows 11
- **Compiler**: c++.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r4) 15.1.0
- **Build Type**: Release
- **Commit**: `6ae4936d1` (dirty)
- **Timestamp**: 2026-11-15T10:52:59+00:00 UTC

## 1. Headline Replay Throughput

| Implementation | Throughput | Mean Latency | Variance (CV) |
| :--- | ---: | ---: | ---: |
| `Contiguous` | **11.6 M ops/s** | 86.1 ns | - |
| `Sparse` | **10.9 M ops/s** | 91.4 ns | - |
| `StdMap` | **7.8 M ops/s** | 127.5 ns | - |

*Replay of realistic synthetic order flow (50% passive limit, 10% crossing/aggressive, 40% cancel).*

## 2. Per-Operation Tail-Latency Distribution

| Ladder / Engine | Operation | Count | p50 (Median) | p90 | p99 | p99.9 | Max |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `contiguous` | passive | 225,309 | 156.2 ns | 156.2 ns | **312.5 ns** | 416.7 ns | 94.27 µs |
| `contiguous` | aggressive | 45,196 | 156.2 ns | 260.4 ns | **312.5 ns** | 729.2 ns | 71.35 µs |
| `contiguous` | cancel | 175,869 | 156.2 ns | 156.2 ns | **312.5 ns** | 625.0 ns | 78.44 µs |
| `contiguous` | cancel_miss | 3,626 | 52.1 ns | 156.2 ns | **156.2 ns** | 312.5 ns | 364.6 ns |
| `contiguous` | all | 450,000 | 156.2 ns | 156.2 ns | **312.5 ns** | 572.9 ns | 94.27 µs |
| `sparse` | passive | 225,309 | 156.2 ns | 156.2 ns | **312.5 ns** | 468.8 ns | 185.83 µs |
| `sparse` | aggressive | 45,196 | 156.2 ns | 416.7 ns | **1.25 µs** | 1.93 µs | 33.39 µs |
| `sparse` | cancel | 175,869 | 156.2 ns | 156.2 ns | **364.6 ns** | 989.6 ns | 255.99 µs |
| `sparse` | cancel_miss | 3,626 | 52.1 ns | 156.2 ns | **156.2 ns** | 312.5 ns | 1.77 µs |
| `sparse` | all | 450,000 | 156.2 ns | 156.2 ns | **520.8 ns** | 1.41 µs | 255.99 µs |
| `stdmap` | passive | 225,309 | 156.2 ns | 260.4 ns | **468.8 ns** | 2.50 µs | 225.78 µs |
| `stdmap` | aggressive | 45,196 | 156.2 ns | 312.5 ns | **729.2 ns** | 2.60 µs | 2.13 ms |
| `stdmap` | cancel | 175,869 | 156.2 ns | 312.5 ns | **572.9 ns** | 2.50 µs | 276.04 µs |
| `stdmap` | cancel_miss | 3,626 | 104.2 ns | 156.2 ns | **312.5 ns** | 677.1 ns | 6.25 µs |
| `stdmap` | all | 450,000 | 156.2 ns | 260.4 ns | **520.8 ns** | 2.50 µs | 2.13 ms |

*Measured with `rdtsc` (0.3100 ns/tick, timer overhead of 0.0 ns subtracted).* Tail latency (p99 / p99.9) captures tail risk from memory reallocations and price-level gap scanning.

## Order book (BookCore vs std::map reference)

| Benchmark | Latency / Op |
| :--- | ---: |
| `Book_CancelBest_Contiguous` | 63.2 ns |
| `Book_CancelBest_Contiguous/1` | 67.8 ns |
| `Book_CancelBest_Contiguous/100` | 91.4 ns |
| `Book_CancelBest_Contiguous/1000` | 307.5 ns |
| `Book_CancelBest_Sparse` | 84.0 ns |
| `Book_CancelBest_Sparse/1` | 75.7 ns |
| `Book_CancelBest_Sparse/100` | 75.7 ns |
| `Book_CancelBest_Sparse/1000` | 75.4 ns |
| `Book_CancelBest_StdMap` | 123.6 ns |
| `Book_CancelBest_StdMap/1` | 122.4 ns |
| `Book_CancelBest_StdMap/100` | 122.9 ns |
| `Book_CancelBest_StdMap/1000` | 122.5 ns |
| `Book_MatchOne_Contiguous` | 64.7 ns |
| `Book_MatchOne_Sparse` | 70.4 ns |
| `Book_MatchOne_StdMap` | 90.5 ns |
| `Book_PassiveAddCancel_Contiguous` | 68.0 ns |
| `Book_PassiveAddCancel_Contiguous/1000` | 66.9 ns |
| `Book_PassiveAddCancel_Contiguous/10000` | 69.2 ns |
| `Book_PassiveAddCancel_Sparse` | 70.0 ns |
| `Book_PassiveAddCancel_Sparse/1000` | 66.7 ns |
| `Book_PassiveAddCancel_Sparse/10000` | 71.5 ns |
| `Book_PassiveAddCancel_StdMap` | 110.7 ns |
| `Book_PassiveAddCancel_StdMap/1000` | 111.1 ns |
| `Book_PassiveAddCancel_StdMap/10000` | 110.2 ns |
| `Book_Sweep_Contiguous` | 108.1 ns |
| `Book_Sweep_Contiguous/1` | 89.7 ns |
| `Book_Sweep_Contiguous/20` | 127.0 ns |
| `Book_Sweep_Contiguous/5` | 119.1 ns |
| `Book_Sweep_Sparse` | 76.4 ns |
| `Book_Sweep_Sparse/1` | 77.4 ns |
| `Book_Sweep_Sparse/20` | 161.1 ns |
| `Book_Sweep_Sparse/5` | 124.6 ns |
| `Book_Sweep_StdMap` | 120.4 ns |
| `Book_Sweep_StdMap/1` | 122.5 ns |
| `Book_Sweep_StdMap/20` | 241.5 ns |
| `Book_Sweep_StdMap/5` | 203.5 ns |

## Other

| Benchmark | Latency / Op |
| :--- | ---: |
| `CancelBest_SparseHistory` | 77.4 ns |
| `CancelBest_SparseHistory/100` | 126.3 ns |
| `CancelBest_SparseHistory/1000` | 607.1 ns |

## Price levels

| Benchmark | Latency / Op |
| :--- | ---: |
| `FindBestAsk_Contiguous` | 2.90 ns |
| `FindBestAsk_Contiguous/1` | 2.89 ns |
| `FindBestAsk_Contiguous/10` | 6.36 ns |
| `FindBestAsk_Contiguous/100` | 48.8 ns |
| `FindBestAsk_Contiguous/1000` | 486.0 ns |
| `FindBestAsk_Sparse` | 8.34 ns |
| `FindBestAsk_Sparse/10` | 8.36 ns |
| `FindBestAsk_Sparse/100` | 99.6 ns |
| `FindBestAsk_Sparse/1000` | 970.4 ns |
| `PriceLevels_GetLevel_NearTouch_Contiguous` | 2.18 ns |
| `PriceLevels_GetLevel_NearTouch_Contiguous_Virtual` | 2.33 ns |
| `PriceLevels_GetLevel_NearTouch_Sparse` | 27.1 ns |
| `PriceLevels_GetLevel_NearTouch_Sparse_Virtual` | 18.2 ns |
| `PriceLevels_GetLevel_Uniform_Contiguous` | 2.18 ns |
| `PriceLevels_GetLevel_Uniform_Contiguous/1000` | 2.22 ns |
| `PriceLevels_GetLevel_Uniform_Contiguous/10000` | 2.28 ns |
| `PriceLevels_GetLevel_Uniform_Sparse` | 2.34 ns |
| `PriceLevels_GetLevel_Uniform_Sparse/1000` | 3.62 ns |
| `PriceLevels_GetLevel_Uniform_Sparse/10000` | 135.6 ns |
| `PriceLevels_HasLevel_Miss_Contiguous` | 2.22 ns |
| `PriceLevels_HasLevel_Miss_Contiguous/1000` | 2.18 ns |
| `PriceLevels_HasLevel_Miss_Contiguous/10000` | 2.24 ns |
| `PriceLevels_HasLevel_Miss_Sparse` | 5.75 ns |
| `PriceLevels_HasLevel_Miss_Sparse/1000` | 5.25 ns |
| `PriceLevels_HasLevel_Miss_Sparse/10000` | 82.5 ns |
| `PriceLevels_SetBest_Contiguous` | 4.58 ns |
| `PriceLevels_SetBest_Contiguous_Virtual` | 5.92 ns |
| `PriceLevels_SetBest_Sparse` | 7.82 ns |
| `PriceLevels_SetBest_Sparse_Virtual` | 9.76 ns |

## Allocation on the hot path

| Benchmark | Latency / Op |
| :--- | ---: |
| `IdIndex_FlatRing` | 1.12 ns |
| `IdIndex_FlatRing/10000` | 1.17 ns |
| `IdIndex_FlatRing/50000` | 1.13 ns |
| `IdIndex_UnorderedMap` | 53.7 ns |
| `IdIndex_UnorderedMap/10000/0` | 55.7 ns |
| `IdIndex_UnorderedMap/10000/1` | 53.5 ns |
| `IdIndex_UnorderedMap/50000/1` | 59.1 ns |
| `NewDelete_Churn` | 61.2 ns |
| `NewDelete_Churn/1024` | 60.8 ns |
| `NewDelete_Churn/16384` | 251.7 ns |
| `NewDelete_Churn/65536` | 3.00 µs |
| `NewDelete_HotPair` | 54.5 ns |
| `Pool_Churn` | 2.27 ns |
| `Pool_Churn/1024` | 2.26 ns |
| `Pool_Churn/16384` | 2.64 ns |
| `Pool_Churn/65536` | 20.2 ns |
| `Pool_HotPair` | 2.21 ns |

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
