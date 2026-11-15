#pragma once

// reference_book.hpp
//
// StdMapBook: Canonical price-time priority reference order book.
// Implemented with std::map (bids descending, asks ascending) and std::list
// for resting orders at each price level, with an index map for O(1) cancel.
//
// Serves two critical functions:
// 1. Oracle for differential testing: validates BookCore's exact correctness
//    operation-by-operation across hundreds of thousands of random orders.
// 2. Performance baseline: measures BookCore's speedup over standard STL containers.

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <unordered_map>
#include <vector>

#include "core_shim.hpp"

namespace ob {

class StdMapBook {
 public:
  static constexpr const char* name = "stdmap";

  explicit StdMapBook(const PriceBand& /*band*/, std::size_t capacity = kCorePoolCapacity)
      : cap_(capacity) {
    index_.reserve(capacity);
  }

  ExecResult submit(const NewOrder& o) {
    ExecResult r{};
    r.remaining = o.qty;
    if (o.side == kBuy) {
      r.filled = match(asks_, o.qty, [&](Tick best) { return o.price >= best; });
    } else {
      r.filled = match(bids_, o.qty, [&](Tick best) { return o.price <= best; });
    }
    r.remaining -= r.filled;
    if (r.remaining > 0 && index_.size() < cap_) {
      if (o.side == kBuy) {
        rest(bids_, o, r.remaining);
      } else {
        rest(asks_, o, r.remaining);
      }
    }
    return r;
  }

  bool cancel(OrderId id) {
    auto it = index_.find(id);
    if (it == index_.end()) return false;
    const Loc loc = it->second;
    index_.erase(it);
    if (loc.side == kBuy) {
      remove(bids_, loc);
    } else {
      remove(asks_, loc);
    }
    return true;
  }

  Tick best_bid() const { return bids_.empty() ? kNoBid : bids_.begin()->first; }
  Tick best_ask() const { return asks_.empty() ? kNoAsk : asks_.begin()->first; }

  Quantity level_qty(Side s, Tick px) const {
    if (s == kBuy) {
      auto it = bids_.find(px);
      return it == bids_.end() ? 0 : it->second.qty;
    }
    auto it = asks_.find(px);
    return it == asks_.end() ? 0 : it->second.qty;
  }

  std::size_t live_orders() const { return index_.size(); }
  bool contains(OrderId id) const { return index_.count(id) != 0; }

  std::vector<OrderId> level_ids(Side s, Tick px) const {
    std::vector<OrderId> out;
    auto collect = [&](const auto& book) {
      auto it = book.find(px);
      if (it != book.end()) {
        for (const auto& ro : it->second.orders) out.push_back(ro.id);
      }
    };
    if (s == kBuy) collect(bids_);
    else collect(asks_);
    return out;
  }

  template <class Fn>
  void for_each_level(Side s, Fn fn) const {
    if (s == kBuy) {
      for (const auto& kv : bids_) fn(kv.first, kv.second.qty);
    } else {
      for (const auto& kv : asks_) fn(kv.first, kv.second.qty);
    }
  }

 private:
  struct Resting {
    OrderId id;
    Quantity qty;
  };

  struct Level {
    std::list<Resting> orders;
    Quantity qty = 0;
  };

  struct Loc {
    Side side;
    Tick px;
    std::list<Resting>::iterator it;
  };

  template <class Book, class CanMatch>
  Quantity match(Book& book, Quantity qty, CanMatch can_match) {
    Quantity filled = 0;
    while (qty > 0 && !book.empty() && can_match(book.begin()->first)) {
      Level& lvl = book.begin()->second;
      while (qty > 0 && !lvl.orders.empty()) {
        Resting& head = lvl.orders.front();
        const Quantity take = std::min(qty, head.qty);
        head.qty -= take;
        lvl.qty -= take;
        qty -= take;
        filled += take;
        if (head.qty == 0) {
          index_.erase(head.id);
          lvl.orders.pop_front();
        }
      }
      if (lvl.orders.empty()) {
        book.erase(book.begin());
      }
    }
    return filled;
  }

  template <class Book>
  void rest(Book& book, const NewOrder& o, Quantity qty) {
    Level& lvl = book[o.price];
    lvl.orders.push_back(Resting{o.id, qty});
    lvl.qty += qty;
    index_[o.id] = Loc{o.side, o.price, std::prev(lvl.orders.end())};
  }

  template <class Book>
  void remove(Book& book, const Loc& loc) {
    auto lit = book.find(loc.px);
    if (lit == book.end()) return;
    Level& lvl = lit->second;
    lvl.qty -= loc.it->qty;
    lvl.orders.erase(loc.it);
    if (lvl.orders.empty()) {
      book.erase(lit);
    }
  }

  std::size_t cap_;
  std::map<Tick, Level, std::greater<Tick>> bids_;
  std::map<Tick, Level> asks_;
  std::unordered_map<OrderId, Loc> index_;
};

}  // namespace ob

