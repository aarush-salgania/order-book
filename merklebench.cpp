// bench.cpp - speed benchmark + correctness check.
// Correctness: the roots produced by the threaded pipeline must equal roots
// recomputed serially from the same events.
#include <chrono>
#include <cstdio>
#include "merkle.hpp"

constexpr size_t kRing = 1 << 16, kBatch = 1024, kTrades = 2'000'000;
using Clock = std::chrono::steady_clock;

static ExecutionEvent make_event(uint64_t i) {
  ExecutionEvent e{};
  e.ingress_id = e.trade_id = i;
  e.maker_order_id = 100000 + i;
  e.taker_order_id = 200000 + i;
  e.price = 1052500;
  e.quantity = 50;
  e.side = static_cast<uint8_t>(i & 1);
  e.event_type = 2;
  return e;
}

int main() {
  RingBuffer ring(kRing);
  MerkleWorker worker(ring, kBatch);
  worker.start();

  size_t spins = 0;
  const auto t0 = Clock::now();
  for (uint64_t i = 1; i <= kTrades; ++i) {
    const ExecutionEvent e = make_event(i);
    while (!ring.push(e)) { ++spins; cpu_pause(); }
  }
  const auto t1 = Clock::now();
  worker.stop();
  const auto t2 = Clock::now();

  // Serial reference (not timed).
  const auto& roots = worker.roots();
  std::vector<Hash> nodes(kBatch + 1);
  size_t n = 0, r = 0;
  bool ok = true;
  auto check = [&] {
    ok = ok && r < roots.size() && merkle_root(nodes.data(), n) == roots[r];
    ++r; n = 0;
  };
  for (uint64_t i = 1; i <= kTrades; ++i) {
    nodes[n++] = hash_leaf(make_event(i));
    if (n == kBatch) check();
  }
  if (n) check();
  ok = ok && r == roots.size();

  const double push_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kTrades;
  const double total_s = std::chrono::duration<double>(t2 - t0).count();
  std::printf("trades            : %zu\n", kTrades);
  std::printf("roots             : %zu (batch %zu)\n", roots.size(), kBatch);
  std::printf("full-ring spins   : %zu\n", spins);
  std::printf("push latency      : %.1f ns/trade (includes waiting if spins > 0)\n", push_ns);
  std::printf("end-to-end        : %.2f M trades/s (%.1f ms)\n", kTrades / total_s / 1e6, total_s * 1e3);
  std::printf("roots match serial: %s\n", ok ? "PASS" : "FAIL");
  return ok ? 0 : 1;
}