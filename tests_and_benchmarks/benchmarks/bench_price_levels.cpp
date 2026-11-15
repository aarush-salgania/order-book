#include "bench_common.hpp"

namespace {

using namespace obbench;

constexpr std::size_t kSamples = 1 << 16;
constexpr std::size_t kMask = kSamples - 1;
constexpr Tick kMid = 10000;
constexpr PriceBand kBand{0, 20000, 1};

template <class L>
std::unique_ptr<L> make_populated(Tick lo, Tick hi) {
  auto lv = Ladder<L>::make(kBand);
  for (Tick t = lo; t <= hi; ++t) {
    auto& lvl = lv->get_level(t);
    lvl.head = lvl.tail = 0;
    lvl.qty = static_cast<Quantity>(t & 0xff);
  }
  return lv;
}

template <class L, class Iface>
void GetLevel_NearTouch(benchmark::State& state) {
  auto lv = make_populated<L>(kMid - 2000, kMid + 2000);
  Iface* api = opaque<Iface>(lv.get());
  const auto ticks = near_touch_ticks(kSamples, kMid, 2000, 1);
  std::size_t i = 0;
  std::int64_t acc = 0;
  for (auto _ : state) {
    acc += api->get_level(ticks[i++ & kMask]).qty;
    benchmark::DoNotOptimize(acc);
  }
}

template <class L>
void GetLevel_Uniform(benchmark::State& state) {
  const Tick w = std::max<Tick>(100, static_cast<Tick>(state.range(0)));
  const Tick lo = kMid - w / 2, hi = lo + w - 1;
  auto lv = make_populated<L>(lo, hi);
  L* api = opaque<L>(lv.get());
  const auto ticks = uniform_ticks(kSamples, lo, hi, 2);
  std::size_t i = 0;
  std::int64_t acc = 0;
  for (auto _ : state) {
    acc += api->get_level(ticks[i++ & kMask]).qty;
    benchmark::DoNotOptimize(acc);
  }
}

template <class L>
void HasLevel_Miss(benchmark::State& state) {
  const Tick w = std::max<Tick>(100, static_cast<Tick>(state.range(0)));
  auto lv = Ladder<L>::make(kBand);
  for (Tick t = kMid - w; t < kMid + w; t += 2) lv->get_level(t).head = 0;
  L* api = opaque<L>(lv.get());
  auto ticks = uniform_ticks(kSamples, kMid - w, kMid + w - 1, 3);
  for (auto& t : ticks) t |= 1;
  std::size_t i = 0;
  int hits = 0;
  for (auto _ : state) {
    hits += api->has_level(ticks[i++ & kMask]);
    benchmark::DoNotOptimize(hits);
  }
}

template <class L, class Iface>
void SetBest(benchmark::State& state) {
  auto lv = Ladder<L>::make(kBand);
  const auto ticks = near_touch_ticks(kSamples, kMid, 2000, 4);
  for (Tick t : ticks) lv->get_level(t);
  Iface* api = opaque<Iface>(lv.get());
  std::size_t i = 0;
  for (auto _ : state) {
    const Tick t = ticks[i++ & kMask];
    if (t < kMid) api->set_best_bid(t);
    else api->set_best_ask(t);
    benchmark::ClobberMemory();
  }
}

void FindBestAsk_Contiguous(benchmark::State& state) {
  const Tick gap = std::max<Tick>(1, static_cast<Tick>(state.range(0)));
  PriceLevelContiguous lv(kBand);
  lv.get_level(kMid + gap).head = 0;
  lv.set_best_ask(kMid);
  IPriceLevels* api = opaque<IPriceLevels>(&lv);
  for (auto _ : state) benchmark::DoNotOptimize(api->find_best_ask());
}

void FindBestAsk_Sparse(benchmark::State& state) {
  const Tick entries = std::max<Tick>(10, static_cast<Tick>(state.range(0)));
  PriceLevelsSparse lv;
  for (Tick t = 0; t < entries; ++t) lv.get_level(kMid + t);
  lv.get_level(kMid + entries / 2).head = 0;
  lv.set_best_ask(kMid);
  IPriceLevels* api = opaque<IPriceLevels>(&lv);
  for (auto _ : state) benchmark::DoNotOptimize(api->find_best_ask());
}

using C = PriceLevelContiguous;
using S = PriceLevelsSparse;
using I = IPriceLevels;

void PriceLevels_GetLevel_NearTouch_Contiguous(benchmark::State& s) { GetLevel_NearTouch<C, C>(s); }
void PriceLevels_GetLevel_NearTouch_Contiguous_Virtual(benchmark::State& s) { GetLevel_NearTouch<C, I>(s); }
void PriceLevels_GetLevel_NearTouch_Sparse(benchmark::State& s) { GetLevel_NearTouch<S, S>(s); }
void PriceLevels_GetLevel_NearTouch_Sparse_Virtual(benchmark::State& s) { GetLevel_NearTouch<S, I>(s); }

void PriceLevels_GetLevel_Uniform_Contiguous(benchmark::State& s) { GetLevel_Uniform<C>(s); }
void PriceLevels_GetLevel_Uniform_Sparse(benchmark::State& s) { GetLevel_Uniform<S>(s); }

void PriceLevels_HasLevel_Miss_Contiguous(benchmark::State& s) { HasLevel_Miss<C>(s); }
void PriceLevels_HasLevel_Miss_Sparse(benchmark::State& s) { HasLevel_Miss<S>(s); }

void PriceLevels_SetBest_Contiguous(benchmark::State& s) { SetBest<C, C>(s); }
void PriceLevels_SetBest_Contiguous_Virtual(benchmark::State& s) { SetBest<C, I>(s); }
void PriceLevels_SetBest_Sparse(benchmark::State& s) { SetBest<S, S>(s); }
void PriceLevels_SetBest_Sparse_Virtual(benchmark::State& s) { SetBest<S, I>(s); }

BENCHMARK(PriceLevels_GetLevel_NearTouch_Contiguous);
BENCHMARK(PriceLevels_GetLevel_NearTouch_Contiguous_Virtual);
BENCHMARK(PriceLevels_GetLevel_NearTouch_Sparse);
BENCHMARK(PriceLevels_GetLevel_NearTouch_Sparse_Virtual);

BENCHMARK(PriceLevels_GetLevel_Uniform_Contiguous)->Arg(1000)->Arg(10000);
BENCHMARK(PriceLevels_GetLevel_Uniform_Sparse)->Arg(1000)->Arg(10000);

BENCHMARK(PriceLevels_HasLevel_Miss_Contiguous)->Arg(1000)->Arg(10000);
BENCHMARK(PriceLevels_HasLevel_Miss_Sparse)->Arg(1000)->Arg(10000);

BENCHMARK(PriceLevels_SetBest_Contiguous);
BENCHMARK(PriceLevels_SetBest_Contiguous_Virtual);
BENCHMARK(PriceLevels_SetBest_Sparse);
BENCHMARK(PriceLevels_SetBest_Sparse_Virtual);

BENCHMARK(FindBestAsk_Contiguous)->Arg(1)->Arg(10)->Arg(100)->Arg(1000);
BENCHMARK(FindBestAsk_Sparse)->Arg(10)->Arg(100)->Arg(1000);

}  // namespace

