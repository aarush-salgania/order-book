#include "bench_common.hpp"

namespace {

using namespace obbench;

constexpr PriceBand kBand{0, 19999, 1};
constexpr Tick kMid = 10000;

const Workload& replay_workload() {
  static const Workload w = [] {
    WorkloadConfig cfg;
    cfg.num_ops = 50000;
    cfg.seed = 42;
    return generate_workload(cfg);
  }();
  return w;
}

template <class B>
void Replay(benchmark::State& state) {
  const Workload& w = replay_workload();
  ReplayStats last{};
  for (auto _ : state) {
    state.PauseTiming();
    auto book = std::make_unique<B>(w.cfg.band);
    state.ResumeTiming();
    last = replay(*book, w.ops);
    benchmark::DoNotOptimize(last);
    state.PauseTiming();
    book.reset();
    state.ResumeTiming();
  }
  state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations() * w.ops.size()));
  set_ns_per_op(state, static_cast<double>(w.ops.size()));
  if (last.cancels + last.cancel_misses > 0) {
    state.counters["cancel_miss%"] = 100.0 * last.cancel_misses / (last.cancels + last.cancel_misses);
  }
}

template <class B>
void PassiveAddCancel(benchmark::State& state) {
  const OrderId depth = std::max<OrderId>(1000, static_cast<OrderId>(state.range(0)));
  auto book = std::make_unique<B>(kBand);
  const auto ticks = near_touch_ticks(1 << 16, kMid, 2000, 7);
  const std::size_t mask = ticks.size() - 1;
  auto add = [&](OrderId id) {
    const Tick t = ticks[static_cast<std::size_t>(id) & mask];
    book->submit(make_order(id, t < kMid ? kBuy : kSell, t, 10));
  };
  for (OrderId id = 1; id <= depth; ++id) add(id);
  OrderId next = depth + 1;
  for (auto _ : state) {
    book->cancel(next - depth);
    add(next++);
  }
  set_ns_per_op(state, 2.0);
}

template <class B>
void MatchOne(benchmark::State& state) {
  auto book = std::make_unique<B>(kBand);
  OrderId id = 1;
  for (Tick t = kMid; t < kMid + 10; ++t) {
    for (int k = 0; k < 10; ++k) book->submit(make_order(id++, kSell, t, 10));
  }
  for (Tick t = kMid - 10; t < kMid; ++t) {
    for (int k = 0; k < 10; ++k) book->submit(make_order(id++, kBuy, t, 10));
  }
  for (auto _ : state) {
    ExecResult r = book->submit(make_order(id++, kBuy, kMid + 9, 10));
    benchmark::DoNotOptimize(r);
    book->submit(make_order(id++, kSell, kMid, 10));
  }
  set_ns_per_op(state, 2.0);
}

template <class B>
void Sweep(benchmark::State& state) {
  const Tick levels = std::max<Tick>(1, static_cast<Tick>(state.range(0)));
  auto book = std::make_unique<B>(kBand);
  OrderId id = 1;
  auto replenish = [&] {
    for (Tick t = kMid; t < kMid + levels; ++t) book->submit(make_order(id++, kSell, t, 1));
  };
  replenish();
  book->submit(make_order(id++, kSell, kMid + levels + 50, 1));
  for (Tick t = kMid - 20; t < kMid; ++t) book->submit(make_order(id++, kBuy, t, 5));
  for (auto _ : state) {
    ExecResult r = book->submit(make_order(id++, kBuy, kMid + levels - 1, levels));
    benchmark::DoNotOptimize(r);
    replenish();
  }
  set_ns_per_op(state, 1.0 + levels);
}

template <class B>
void CancelBest(benchmark::State& state) {
  const Tick gap = std::max<Tick>(1, static_cast<Tick>(state.range(0)));
  auto book = std::make_unique<B>(kBand);
  const Tick top = kMid - 1;
  book->submit(make_order(1, kBuy, top - gap, 10));
  book->submit(make_order(2, kSell, kMid + 5, 10));
  OrderId id = 3;
  book->submit(make_order(id, kBuy, top, 10));
  for (auto _ : state) {
    book->cancel(id);
    book->submit(make_order(++id, kBuy, top, 10));
  }
  set_ns_per_op(state, 2.0);
}

void CancelBest_SparseHistory(benchmark::State& state) {
  const Tick history = std::max<Tick>(10, static_cast<Tick>(state.range(0)));
  SparseBook book(kBand);
  OrderId id = 1;
  for (Tick t = 0; t < history; ++t) {
    book.submit(make_order(id, kBuy, 100 + t, 1));
    book.cancel(id++);
  }
  const Tick top = kMid - 1;
  book.submit(make_order(id++, kBuy, top - 1, 10));
  book.submit(make_order(id++, kSell, kMid + 5, 10));
  book.submit(make_order(id, kBuy, top, 10));
  for (auto _ : state) {
    book.cancel(id);
    book.submit(make_order(++id, kBuy, top, 10));
  }
  set_ns_per_op(state, 2.0);
}

void Book_Replay_Contiguous(benchmark::State& s) { Replay<ContiguousBook>(s); }
void Book_Replay_Sparse(benchmark::State& s) { Replay<SparseBook>(s); }
void Book_Replay_StdMap(benchmark::State& s) { Replay<StdMapBook>(s); }

void Book_PassiveAddCancel_Contiguous(benchmark::State& s) { PassiveAddCancel<ContiguousBook>(s); }
void Book_PassiveAddCancel_Sparse(benchmark::State& s) { PassiveAddCancel<SparseBook>(s); }
void Book_PassiveAddCancel_StdMap(benchmark::State& s) { PassiveAddCancel<StdMapBook>(s); }

void Book_MatchOne_Contiguous(benchmark::State& s) { MatchOne<ContiguousBook>(s); }
void Book_MatchOne_Sparse(benchmark::State& s) { MatchOne<SparseBook>(s); }
void Book_MatchOne_StdMap(benchmark::State& s) { MatchOne<StdMapBook>(s); }

void Book_Sweep_Contiguous(benchmark::State& s) { Sweep<ContiguousBook>(s); }
void Book_Sweep_Sparse(benchmark::State& s) { Sweep<SparseBook>(s); }
void Book_Sweep_StdMap(benchmark::State& s) { Sweep<StdMapBook>(s); }

void Book_CancelBest_Contiguous(benchmark::State& s) { CancelBest<ContiguousBook>(s); }
void Book_CancelBest_Sparse(benchmark::State& s) { CancelBest<SparseBook>(s); }
void Book_CancelBest_StdMap(benchmark::State& s) { CancelBest<StdMapBook>(s); }

BENCHMARK(Book_Replay_Contiguous);
BENCHMARK(Book_Replay_Sparse);
BENCHMARK(Book_Replay_StdMap);

BENCHMARK(Book_PassiveAddCancel_Contiguous)->Arg(1000)->Arg(10000);
BENCHMARK(Book_PassiveAddCancel_Sparse)->Arg(1000)->Arg(10000);
BENCHMARK(Book_PassiveAddCancel_StdMap)->Arg(1000)->Arg(10000);

BENCHMARK(Book_MatchOne_Contiguous);
BENCHMARK(Book_MatchOne_Sparse);
BENCHMARK(Book_MatchOne_StdMap);

BENCHMARK(Book_Sweep_Contiguous)->Arg(1)->Arg(5)->Arg(20);
BENCHMARK(Book_Sweep_Sparse)->Arg(1)->Arg(5)->Arg(20);
BENCHMARK(Book_Sweep_StdMap)->Arg(1)->Arg(5)->Arg(20);

BENCHMARK(Book_CancelBest_Contiguous)->Arg(1)->Arg(100)->Arg(1000);
BENCHMARK(Book_CancelBest_Sparse)->Arg(1)->Arg(100)->Arg(1000);
BENCHMARK(Book_CancelBest_StdMap)->Arg(1)->Arg(100)->Arg(1000);

BENCHMARK(CancelBest_SparseHistory)->Arg(100)->Arg(1000);

}  // namespace

