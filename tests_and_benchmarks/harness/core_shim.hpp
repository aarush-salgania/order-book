#pragma once

// core_shim.hpp
//
// Single inclusion shim that bridges tests and benchmarks directly to the real
// source files in core/ (order_book_core.cpp, order.cpp, mempool.cpp, types.cpp).
//
// Includes necessary standard library headers first, especially <algorithm>
// for std::min/std::max which order.cpp and order_book_core.cpp utilize.

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "order_book_core.cpp"  // Directly pulls core/order_book_core.cpp, order.cpp, mempool.cpp, types.cpp

namespace ob {

constexpr Tick kNoBid = std::numeric_limits<Tick>::min();
constexpr Tick kNoAsk = std::numeric_limits<Tick>::max();
constexpr Side kBuy = 'B';
constexpr Side kSell = 'S';

// BookCore pool size is 100,000 nodes (order_book_core.cpp).
constexpr std::size_t kCorePoolCapacity = 100000;

template <class L>
struct Ladder;

template <>
struct Ladder<PriceLevelContiguous> {
  static std::unique_ptr<PriceLevelContiguous> make(const PriceBand& b) {
    return std::unique_ptr<PriceLevelContiguous>(new PriceLevelContiguous(b));
  }
  static constexpr const char* name = "contiguous";
};

template <>
struct Ladder<PriceLevelsSparse> {
  static std::unique_ptr<PriceLevelsSparse> make(const PriceBand&) {
    return std::unique_ptr<PriceLevelsSparse>(new PriceLevelsSparse());
  }
  static constexpr const char* name = "sparse";
};

inline NewOrder make_order(OrderId id, Side side, Tick px, Quantity qty, Timestamp ts = 0, UserId user = 0) {
  NewOrder o{};
  o.ts = ts;
  o.id = id;
  o.qty = qty;
  o.side = side;
  o.price = px;
  o.user = user;
  return o;
}

// CoreBook owns two ladders (bids, asks) and a BookCore matching engine,
// presenting a unified, ergonomic test/bench API.
template <class L>
class CoreBook {
 public:
  static constexpr const char* name = Ladder<L>::name;

  explicit CoreBook(const PriceBand& band)
      : band_(band),
        bids_(Ladder<L>::make(band)),
        asks_(Ladder<L>::make(band)),
        core_(new BookCore(*bids_, *asks_)) {}

  ExecResult submit(const NewOrder& o) { return core_->submit_limit(o); }
  bool cancel(OrderId id) { return core_->cancel(id); }

  Tick best_bid() { return bids_->best_bid(); }
  Tick best_ask() { return asks_->best_ask(); }

  Quantity level_qty(Side s, Tick px) {
    if (px < band_.min_tick || px > band_.max_tick) return 0;
    IPriceLevels& lv = (s == kBuy) ? static_cast<IPriceLevels&>(*bids_) : *asks_;
    return lv.has_level(px) ? lv.get_level(px).qty : 0;
  }

  BookCore& core() { return *core_; }
  L& bids() { return *bids_; }
  L& asks() { return *asks_; }

 private:
  PriceBand band_;
  std::unique_ptr<L> bids_;
  std::unique_ptr<L> asks_;
  std::unique_ptr<BookCore> core_;
};

using ContiguousBook = CoreBook<PriceLevelContiguous>;
using SparseBook = CoreBook<PriceLevelsSparse>;

}  // namespace ob

