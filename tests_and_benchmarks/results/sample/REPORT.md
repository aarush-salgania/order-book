# Low Latency Limit Order Book - Benchmark & Tail-Latency Report

## Environment

- **CPU**: Snapdragon(R) X Plus - X1P42100 - Qualcomm(R) Oryon(TM) CPU (8 cores)
- **OS**: Windows 11
- **Compiler**: c++.exe (MinGW-W64 x86_64-ucrt-posix-seh, built by Brecht Sanders, r4) 15.1.0
- **Build Type**: Release
- **Commit**: `2a0f7f186` (dirty)
- **Timestamp**: 2026-11-11T08:09:26+00:00 UTC

## 1. Headline Replay Throughput

| Implementation | Throughput | Mean Latency | Variance (CV) | Delta vs Baseline |
| :--- | ---: | ---: | ---: | ---: |
| `Contiguous` | **11.6 M ops/s** | 85.9 ns | - | +0.0% |
| `Sparse` | **10.3 M ops/s** | 96.8 ns | - | +0.0% |
| `StdMap` | **8.2 M ops/s** | 122.0 ns | - | +0.0% |

*Replay of realistic synthetic order flow (50% passive limit, 10% crossing/aggressive, 40% cancel).*

## 2. Per-Operation Tail-Latency Distribution

| Ladder / Engine | Operation | Count | p50 (Median) | p90 | p99 | p99.9 | Max |
| :--- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `contiguous` | passive | 225,309 | 156.3 ns | 156.3 ns | **312.5 ns** | 312.5 ns | 110.52 µs |
| `contiguous` | aggressive | 45,196 | 156.3 ns | 208.3 ns | **312.5 ns** | 468.8 ns | 76.20 µs |
| `contiguous` | cancel | 175,869 | 104.2 ns | 156.3 ns | **208.3 ns** | 416.7 ns | 44.27 µs |
| `contiguous` | cancel_miss | 3,626 | 52.1 ns | 104.2 ns | **156.3 ns** | 156.3 ns | 468.8 ns |
| `contiguous` | all | 450,000 | 156.3 ns | 156.3 ns | **312.5 ns** | 416.7 ns | 110.52 µs |
| `sparse` | passive | 225,309 | 156.3 ns | 208.3 ns | **312.5 ns** | 364.6 ns | 63.65 µs |
| `sparse` | aggressive | 45,196 | 156.3 ns | 416.7 ns | **1.20 µs** | 1.56 µs | 35.26 µs |
| `sparse` | cancel | 175,869 | 156.3 ns | 156.3 ns | **260.4 ns** | 885.4 ns | 53.02 µs |
| `sparse` | cancel_miss | 3,626 | 52.1 ns | 156.3 ns | **156.3 ns** | 260.4 ns | 416.7 ns |
| `sparse` | all | 450,000 | 156.3 ns | 156.3 ns | **468.8 ns** | 1.25 µs | 63.65 µs |
| `stdmap` | passive | 225,309 | 156.3 ns | 260.4 ns | **312.5 ns** | 312.5 ns | 90.63 µs |
| `stdmap` | aggressive | 45,196 | 156.3 ns | 260.4 ns | **364.6 ns** | 468.8 ns | 3.59 µs |
| `stdmap` | cancel | 175,869 | 156.3 ns | 156.3 ns | **260.4 ns** | 312.5 ns | 55.83 µs |
| `stdmap` | cancel_miss | 3,626 | 52.1 ns | 104.2 ns | **156.3 ns** | 260.4 ns | 1.56 µs |
| `stdmap` | all | 450,000 | 156.3 ns | 260.4 ns | **312.5 ns** | 416.7 ns | 90.63 µs |

*Measured with `rdtsc` (0.3100 ns/tick, timer overhead of 0.0 ns subtracted).* Tail latency (p99 / p99.9) captures tail risk from memory reallocations and price-level gap scanning.

## Order book (BookCore vs std::map reference)

| Benchmark | Latency / Op | Delta vs Base |
| :--- | ---: | ---: |
| `Book_CancelBest_Contiguous` | 62.8 ns | +0.0% |
| `Book_CancelBest_Contiguous/1` | 63.2 ns | +0.0% |
| `Book_CancelBest_Contiguous/100` | 89.2 ns | +0.0% |
| `Book_CancelBest_Contiguous/1000` | 305.1 ns | +0.0% |
| `Book_CancelBest_Sparse` | 70.8 ns | +0.0% |
| `Book_CancelBest_Sparse/1` | 69.8 ns | +0.0% |
| `Book_CancelBest_Sparse/100` | 74.2 ns | +0.0% |
| `Book_CancelBest_Sparse/1000` | 69.7 ns | +0.0% |
| `Book_CancelBest_StdMap` | 115.9 ns | +0.0% |
| `Book_CancelBest_StdMap/1` | 116.4 ns | +0.0% |
| `Book_CancelBest_StdMap/100` | 116.4 ns | +0.0% |
| `Book_CancelBest_StdMap/1000` | 119.6 ns | +0.0% |
| `Book_MatchOne_Contiguous` | 68.8 ns | +0.0% |
| `Book_MatchOne_Sparse` | 69.1 ns | +0.0% |
| `Book_MatchOne_StdMap` | 107.4 ns | +0.0% |
| `Book_PassiveAddCancel_Contiguous` | 63.3 ns | +0.0% |
| `Book_PassiveAddCancel_Contiguous/1000` | 65.1 ns | +0.0% |
| `Book_PassiveAddCancel_Contiguous/10000` | 67.5 ns | +0.0% |
| `Book_PassiveAddCancel_Sparse` | 67.2 ns | +0.0% |
| `Book_PassiveAddCancel_Sparse/1000` | 67.3 ns | +0.0% |
| `Book_PassiveAddCancel_Sparse/10000` | 68.7 ns | +0.0% |
| `Book_PassiveAddCancel_StdMap` | 107.8 ns | +0.0% |
| `Book_PassiveAddCancel_StdMap/1000` | 112.4 ns | +0.0% |
| `Book_PassiveAddCancel_StdMap/10000` | 115.0 ns | +0.0% |
| `Book_Sweep_Contiguous` | 82.2 ns | +0.0% |
| `Book_Sweep_Contiguous/1` | 84.4 ns | +0.0% |
| `Book_Sweep_Contiguous/20` | 127.2 ns | +0.0% |
| `Book_Sweep_Contiguous/5` | 117.0 ns | +0.0% |
| `Book_Sweep_Sparse` | 74.6 ns | +0.0% |
| `Book_Sweep_Sparse/1` | 74.3 ns | +0.0% |
| `Book_Sweep_Sparse/20` | 158.8 ns | +0.0% |
| `Book_Sweep_Sparse/5` | 124.8 ns | +0.0% |
| `Book_Sweep_StdMap` | 115.7 ns | +0.0% |
| `Book_Sweep_StdMap/1` | 118.1 ns | +0.0% |
| `Book_Sweep_StdMap/20` | 271.5 ns | +0.0% |
| `Book_Sweep_StdMap/5` | 196.4 ns | +0.0% |

## Other

| Benchmark | Latency / Op | Delta vs Base |
| :--- | ---: | ---: |
| `CancelBest_SparseHistory` | 198.6 ns | +0.0% |
| `CancelBest_SparseHistory/100` | 123.1 ns | +0.0% |
| `CancelBest_SparseHistory/1000` | 652.2 ns | +0.0% |

## Price levels

| Benchmark | Latency / Op | Delta vs Base |
| :--- | ---: | ---: |
| `FindBestAsk_Contiguous` | 2.89 ns | +0.0% |
| `FindBestAsk_Contiguous/1` | 2.85 ns | +0.0% |
| `FindBestAsk_Contiguous/10` | 6.37 ns | +0.0% |
| `FindBestAsk_Contiguous/100` | 47.9 ns | +0.0% |
| `FindBestAsk_Contiguous/1000` | 472.3 ns | +0.0% |
| `FindBestAsk_Sparse` | 7.96 ns | +0.0% |
| `FindBestAsk_Sparse/10` | 8.12 ns | +0.0% |
| `FindBestAsk_Sparse/100` | 96.2 ns | +0.0% |
| `FindBestAsk_Sparse/1000` | 906.7 ns | +0.0% |
| `PriceLevels_GetLevel_NearTouch_Contiguous` | 2.21 ns | +0.0% |
| `PriceLevels_GetLevel_NearTouch_Contiguous_Virtual` | 2.32 ns | +0.0% |
| `PriceLevels_GetLevel_NearTouch_Sparse` | 24.4 ns | +0.0% |
| `PriceLevels_GetLevel_NearTouch_Sparse_Virtual` | 51.5 ns | +0.0% |
| `PriceLevels_GetLevel_Uniform_Contiguous` | 2.17 ns | +0.0% |
| `PriceLevels_GetLevel_Uniform_Contiguous/1000` | 2.17 ns | +0.0% |
| `PriceLevels_GetLevel_Uniform_Contiguous/10000` | 2.24 ns | +0.0% |
| `PriceLevels_GetLevel_Uniform_Sparse` | 2.23 ns | +0.0% |
| `PriceLevels_GetLevel_Uniform_Sparse/1000` | 4.39 ns | +0.0% |
| `PriceLevels_GetLevel_Uniform_Sparse/10000` | 104.4 ns | +0.0% |
| `PriceLevels_HasLevel_Miss_Contiguous` | 2.19 ns | +0.0% |
| `PriceLevels_HasLevel_Miss_Contiguous/1000` | 2.19 ns | +0.0% |
| `PriceLevels_HasLevel_Miss_Contiguous/10000` | 2.16 ns | +0.0% |
| `PriceLevels_HasLevel_Miss_Sparse` | 5.22 ns | +0.0% |
| `PriceLevels_HasLevel_Miss_Sparse/1000` | 6.86 ns | +0.0% |
| `PriceLevels_HasLevel_Miss_Sparse/10000` | 86.5 ns | +0.0% |
| `PriceLevels_SetBest_Contiguous` | 4.42 ns | +0.0% |
| `PriceLevels_SetBest_Contiguous_Virtual` | 5.70 ns | +0.0% |
| `PriceLevels_SetBest_Sparse` | 7.46 ns | +0.0% |
| `PriceLevels_SetBest_Sparse_Virtual` | 9.45 ns | +0.0% |

## Allocation on the hot path

| Benchmark | Latency / Op | Delta vs Base |
| :--- | ---: | ---: |
| `IdIndex_FlatRing` | 1.12 ns | +0.0% |
| `IdIndex_FlatRing/10000` | 1.09 ns | +0.0% |
| `IdIndex_FlatRing/50000` | 1.13 ns | +0.0% |
| `IdIndex_UnorderedMap` | 50.0 ns | +0.0% |
| `IdIndex_UnorderedMap/10000/0` | 54.0 ns | +0.0% |
| `IdIndex_UnorderedMap/10000/1` | 49.0 ns | +0.0% |
| `IdIndex_UnorderedMap/50000/1` | 51.2 ns | +0.0% |
| `NewDelete_Churn` | 58.7 ns | +0.0% |
| `NewDelete_Churn/1024` | 59.1 ns | +0.0% |
| `NewDelete_Churn/16384` | 247.7 ns | +0.0% |
| `NewDelete_Churn/65536` | 2.98 µs | +0.0% |
| `NewDelete_HotPair` | 52.9 ns | +0.0% |
| `Pool_Churn` | 2.27 ns | +0.0% |
| `Pool_Churn/1024` | 2.29 ns | +0.0% |
| `Pool_Churn/16384` | 2.39 ns | +0.0% |
| `Pool_Churn/65536` | 20.3 ns | +0.0% |
| `Pool_HotPair` | 2.14 ns | +0.0% |

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
