#pragma once

// workload.hpp
//
// Deterministic synthetic order flow generator for correctness testing and benchmarks.
// Simulates realistic market micro-structure dynamics using integer-only PRNG (SplitMix64)
// so results are 100% reproducible across compilers, CPU architectures, and OS platforms.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "reference_book.hpp"

namespace ob {

enum class OpType : std::uint8_t { Passive = 0, Aggressive = 1, Cancel = 2 };

struct Op {
  OpType type;
  NewOrder order;  // For Passive / Aggressive
  OrderId cancel_id; // For Cancel
};

struct WorkloadConfig {
  std::size_t num_ops = 1000000;
  std::uint64_t seed = 42;
  PriceBand band{0, 19999, 1};
  Tick start_mid = 10000;
  // Per-mille distribution: cancel = 1000 - passive - aggressive
  unsigned passive_permille = 500;
  unsigned aggressive_permille = 100;
  unsigned depth_continue_permille = 850;  // Geometric decay depth
  Tick max_depth = 1500;
  Tick max_through = 3;                    // Aggressive pricing penetration through mid
  unsigned mid_move_permille = 20;         // Probability per op of mid-price drift (+-1)
  unsigned cancel_miss_permille = 20;      // Share of cancels aimed at already-filled/dead orders
  std::size_t max_live = 50000;            // Bound live resting orders to fit within pool
  Quantity max_qty = 100;
  Quantity max_aggressive_qty = 100;
};

struct Workload {
  WorkloadConfig cfg;
  std::vector<Op> ops;
};

class SplitMix64 {
 public:
  explicit SplitMix64(std::uint64_t seed) : s_(seed) {}
  std::uint64_t next() {
    std::uint64_t z = (s_ += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
  }
  std::uint64_t below(std::uint64_t n) { return n == 0 ? 0 : next() % n; }

 private:
  std::uint64_t s_;
};

inline Workload generate_workload(const WorkloadConfig& cfg) {
  Workload w;
  w.cfg = cfg;
  w.ops.reserve(cfg.num_ops);

  SplitMix64 rng(cfg.seed);
  StdMapBook sim(cfg.band);
  std::vector<OrderId> live;
  std::vector<OrderId> dead;
  constexpr std::size_t kDeadCap = 4096;

  auto remember_dead = [&](OrderId id) {
    if (dead.size() < kDeadCap) {
      dead.push_back(id);
    } else {
      dead[static_cast<std::size_t>(rng.below(kDeadCap))] = id;
    }
  };

  OrderId next_id = 1;
  Tick mid = cfg.start_mid;
  const Tick lo_mid = cfg.band.min_tick + cfg.max_depth + 2;
  const Tick hi_mid = cfg.band.max_tick - cfg.max_depth - 2;
  auto clamp_px = [&](Tick p) { return std::min(std::max(p, cfg.band.min_tick), cfg.band.max_tick); };

  for (std::size_t i = 0; i < cfg.num_ops; ++i) {
    if (rng.below(1000) < cfg.mid_move_permille) {
      mid += rng.below(2) ? 1 : -1;
      mid = std::min(std::max(mid, lo_mid), hi_mid);
    }

    const unsigned roll = static_cast<unsigned>(rng.below(1000));
    OpType type = (roll < cfg.passive_permille) ? OpType::Passive
                  : (roll < cfg.passive_permille + cfg.aggressive_permille) ? OpType::Aggressive
                  : OpType::Cancel;

    if (type == OpType::Cancel && live.empty()) type = OpType::Passive;
    if (type != OpType::Cancel && sim.live_orders() >= cfg.max_live && !live.empty()) type = OpType::Cancel;

    Op op{};
    op.type = type;

    if (type == OpType::Cancel) {
      OrderId id = 0;
      if (!dead.empty() && rng.below(1000) < cfg.cancel_miss_permille) {
        id = dead[static_cast<std::size_t>(rng.below(dead.size()))];
      } else {
        while (!live.empty()) {
          const std::size_t k = static_cast<std::size_t>(rng.below(live.size()));
          id = live[k];
          live[k] = live.back();
          live.pop_back();
          if (sim.contains(id)) break;
          remember_dead(id);
        }
      }

      if (id == 0) {
        type = op.type = OpType::Passive;
      } else {
        op.cancel_id = id;
        sim.cancel(id);
        remember_dead(id);
      }
    }

    if (type != OpType::Cancel) {
      const Side side = rng.below(2) ? kBuy : kSell;
      const Tick bb = sim.best_bid(), ba = sim.best_ask();
      Tick px;
      Quantity qty;

      if (type == OpType::Passive) {
        Tick d = 0;
        while (d < cfg.max_depth && rng.below(1000) < cfg.depth_continue_permille) ++d;
        if (side == kBuy) {
          const Tick top = (ba == kNoAsk) ? mid - 1 : std::min(mid - 1, ba - 1);
          px = top - d;
        } else {
          const Tick bottom = (bb == kNoBid) ? mid : std::max(mid, bb + 1);
          px = bottom + d;
        }
        qty = 1 + static_cast<Quantity>(rng.below(static_cast<std::uint64_t>(cfg.max_qty)));
      } else {
        const Tick k = static_cast<Tick>(rng.below(static_cast<std::uint64_t>(cfg.max_through) + 1));
        if (side == kBuy) {
          px = (ba == kNoAsk ? mid : ba) + k;
        } else {
          px = (bb == kNoBid ? mid - 1 : bb) - k;
        }
        qty = 1 + static_cast<Quantity>(rng.below(static_cast<std::uint64_t>(cfg.max_aggressive_qty)));
      }

      op.order = make_order(next_id, side, clamp_px(px), qty, static_cast<Timestamp>(i));
      sim.submit(op.order);
      if (sim.contains(next_id)) live.push_back(next_id);
      ++next_id;
    }

    w.ops.push_back(op);
  }

  return w;
}

struct ReplayStats {
  std::size_t passive = 0;
  std::size_t aggressive = 0;
  std::size_t cancels = 0;
  std::size_t cancel_misses = 0;
  std::int64_t filled_qty = 0;
};

template <class Book>
inline void apply(Book& book, const Op& op, ReplayStats& st) {
  switch (op.type) {
    case OpType::Passive:
      st.filled_qty += book.submit(op.order).filled;
      ++st.passive;
      break;
    case OpType::Aggressive:
      st.filled_qty += book.submit(op.order).filled;
      ++st.aggressive;
      break;
    case OpType::Cancel:
      if (book.cancel(op.cancel_id)) ++st.cancels;
      else ++st.cancel_misses;
      break;
  }
}

template <class Book>
inline ReplayStats replay(Book& book, const std::vector<Op>& ops) {
  ReplayStats st;
  for (const Op& op : ops) apply(book, op, st);
  return st;
}

}  // namespace ob

