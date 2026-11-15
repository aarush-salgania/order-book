#include "bench_common.hpp"

namespace {

using namespace obbench;
using Node = OrderNode;

void NewDelete_HotPair(benchmark::State& state) {
  for (auto _ : state) {
    auto* p = new Node();
    benchmark::DoNotOptimize(p);
    delete p;
  }
}

void Pool_HotPair(benchmark::State& state) {
  Pool<Node> pool(1024);
  for (auto _ : state) {
    Handle h = pool.alloc();
    benchmark::DoNotOptimize(h);
    pool.dealloc(h);
    benchmark::ClobberMemory();
  }
}

constexpr std::size_t kIdxMask = (1u << 16) - 1;

std::vector<std::uint32_t> random_indices(std::size_t n) {
  SplitMix64 rng(99);
  std::vector<std::uint32_t> v(kIdxMask + 1);
  for (auto& x : v) x = static_cast<std::uint32_t>(rng.below(n));
  return v;
}

void NewDelete_Churn(benchmark::State& state) {
  const std::size_t n = std::max<std::size_t>(1024, static_cast<std::size_t>(state.range(0)));
  const auto idx = random_indices(n);
  std::vector<Node*> live(n);
  for (auto& p : live) p = new Node();
  std::size_t i = 0;
  for (auto _ : state) {
    Node*& slot = live[idx[i++ & kIdxMask]];
    delete slot;
    slot = new Node(1, Limit, 'S', 101, 2);
    slot->qty += 1;
    benchmark::ClobberMemory();
  }
  for (auto* p : live) delete p;
}

void Pool_Churn(benchmark::State& state) {
  const std::size_t n = std::max<std::size_t>(1024, static_cast<std::size_t>(state.range(0)));
  const auto idx = random_indices(n);
  Pool<Node> pool(n);
  std::vector<Handle> live(n);
  for (auto& h : live) h = pool.alloc();
  std::size_t i = 0;
  for (auto _ : state) {
    Handle& slot = live[idx[i++ & kIdxMask]];
    pool.dealloc(slot);
    slot = pool.alloc();
    Node& nd = pool[slot];
    nd.id = static_cast<OrderId>(i);
    nd.qty += 1;
    benchmark::ClobberMemory();
  }
}

// --- id -> handle index, as BookCore::orders_ does it -------------------

struct IdEntry {
  char side;
  Tick price;
  Handle idx;
};

void IdIndex_UnorderedMap(benchmark::State& state) {
  const std::size_t n = std::max<std::size_t>(1000, static_cast<std::size_t>(state.range(0)));
  std::unordered_map<OrderId, IdEntry> m;
  if (state.range(1)) m.reserve(2 * n);
  OrderId next = 0;
  for (; next < static_cast<OrderId>(n); ++next) m[next] = IdEntry{'B', 100, static_cast<Handle>(next)};
  for (auto _ : state) {
    m.erase(next - static_cast<OrderId>(n));
    m[next] = IdEntry{'B', 100, static_cast<Handle>(next)};
    ++next;
    benchmark::ClobberMemory();
  }
  set_ns_per_op(state, 2.0);
}

void IdIndex_FlatRing(benchmark::State& state) {
  const std::size_t n = std::max<std::size_t>(1000, static_cast<std::size_t>(state.range(0)));
  std::size_t cap = 1;
  while (cap < 2 * n) cap <<= 1;
  std::vector<IdEntry> ring(cap, IdEntry{0, 0, NIL});
  const std::size_t mask = cap - 1;
  OrderId next = 0;
  for (; next < static_cast<OrderId>(n); ++next) ring[next & mask] = IdEntry{'B', 100, static_cast<Handle>(next)};
  for (auto _ : state) {
    ring[(next - static_cast<OrderId>(n)) & mask].idx = NIL;
    ring[next & mask] = IdEntry{'B', 100, static_cast<Handle>(next)};
    ++next;
    benchmark::ClobberMemory();
  }
  set_ns_per_op(state, 2.0);
}

BENCHMARK(NewDelete_HotPair);
BENCHMARK(Pool_HotPair);
BENCHMARK(NewDelete_Churn)->Arg(1024)->Arg(16384)->Arg(65536);
BENCHMARK(Pool_Churn)->Arg(1024)->Arg(16384)->Arg(65536);

BENCHMARK(IdIndex_UnorderedMap)->Args({10000, 0})->Args({10000, 1})->Args({50000, 1});
BENCHMARK(IdIndex_FlatRing)->Arg(10000)->Arg(50000);

}  // namespace

