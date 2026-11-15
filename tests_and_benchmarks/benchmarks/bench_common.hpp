#pragma once

// bench_common.hpp
//
// Shared helpers and utilities for BookCore microbenchmarks.

#include "bench_framework.hpp"
#include <cstdint>
#include <vector>

#include "reference_book.hpp"
#include "workload.hpp"

namespace obbench {

using namespace ob;

// Ticks where real flow lands: geometric distance from the touch.
inline std::vector<Tick> near_touch_ticks(std::size_t n, Tick mid, Tick max_depth, std::uint64_t seed = 7) {
  SplitMix64 rng(seed);
  std::vector<Tick> out(n);
  for (auto& t : out) {
    Tick d = 0;
    while (d < max_depth && rng.below(1000) < 850) ++d;
    t = (rng.below(2) == 0) ? (mid - 1 - d) : (mid + d);
  }
  return out;
}

inline std::vector<Tick> uniform_ticks(std::size_t n, Tick lo, Tick hi, std::uint64_t seed = 2) {
  SplitMix64 rng(seed);
  std::vector<Tick> out(n);
  for (auto& t : out) {
    t = lo + static_cast<Tick>(rng.below(static_cast<std::uint64_t>(hi - lo + 1)));
  }
  return out;
}

inline void set_ns_per_op(benchmark::State& state, double ops_per_iteration = 1.0) {
  if (state.iterations() > 0 && ops_per_iteration > 0) {
    double total_ops = state.iterations() * ops_per_iteration;
    double sec_per_op = (state.compute_elapsed().count() * 1e-9) / total_ops;
    state.counters["ns/op"] = sec_per_op;
    state.SetItemsProcessed(static_cast<int64_t>(total_ops));
  }
}

// Returns p through a volatile so optimizer doesn't devirtualize
template <class T>
inline T* opaque(T* p) {
  T* volatile v = p;
  return v;
}

}  // namespace obbench
