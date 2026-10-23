#pragma once
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>
#include "types.cpp"

enum OrderType {
  Limit,
  Market
};

struct OrderNode
{

  OrderType orderType;
  OrderId id;
  char side;
  Price price;
  Quantity qty;

  Handle next {NIL};
  Handle prev {NIL};

  OrderNode() = default;

  OrderNode(OrderType order_type, char side, int32_t price, int32_t quantity)
    : orderType(order_type),
      side (side),
      price(price),
      qty(quantity) {
  }
};

struct LevelFIFO {
  Handle head {NIL};
  Handle tail {NIL};
  Quantity qty{};
  Price price{};
};

class IPriceLevels {
public:
  virtual ~IPriceLevels() = default;

  virtual LevelFIFO& get_level(Tick tx) = 0;
  virtual bool has_level(Tick tx) = 0;

  virtual Tick best_bid() = 0;
  virtual Tick best_ask() = 0;

  virtual void set_best_ask(Tick tx) = 0;
  virtual void set_best_bid(Tick tx) = 0;

  virtual void find_best_ask() = 0;
  virtual void find_best_bid() = 0;
};

class PriceLevelContiguous : public IPriceLevels {
private:
  int32_t idx(const Tick px) const { return (px - band_.min_tick); }

  PriceBand band_;
  std::vector<LevelFIFO> levels_;
  Tick best_bid_;
  Tick best_ask_;
  LevelFIFO* best_bid_ptr_;
  LevelFIFO* best_ask_ptr_;
public:
  explicit PriceLevelContiguous(const PriceBand &band) :
      band_(band),
      levels_(static_cast<size_t>(band.max_tick - band.min_tick + 1)),
      best_bid_(std::numeric_limits<Tick>::min()),
      best_ask_(std::numeric_limits<Tick>::max()),
      best_bid_ptr_(nullptr),
      best_ask_ptr_(nullptr) {}

  LevelFIFO& get_level(const Tick tx) override {
    return levels_[idx(tx)];
  }
  bool has_level(const Tick tx) override {
    auto &l = levels_[idx(tx)];
    // Updated null check to use Handle NIL
    return l.head != NIL;
  }
  LevelFIFO* best_level_ptr(const Side S) const {
    return S == 'B' ? best_bid_ptr_ : best_ask_ptr_;
  }

  Tick best_bid() override { return best_bid_; }
  Tick best_ask() override { return best_ask_; }

  void set_best_bid(const Tick tx) override{
    best_bid_ = tx;
    best_bid_ptr_ = tx == std::numeric_limits<Tick>::min() ? nullptr : &levels_[idx(tx)];
  }
  void set_best_ask(const Tick tx) override{
    best_ask_ = tx;
    best_ask_ptr_ = tx == std::numeric_limits<Tick>::max() ? nullptr : &levels_[idx(tx)];
  }

  void find_best_ask() override {
    int i = best_ask_ == std::numeric_limits<Tick>::max() ? idx(band_.min_tick) : idx(best_ask_);
    for (; i <= idx(band_.max_tick); i++) {
      if (levels_[i].head != NIL) {
        best_bid_ = i + band_.min_tick;
        return;
      }
    }
    best_ask_ = std::numeric_limits<Tick>::max();
  }

  void find_best_bid() override {
    int i = best_bid_ == std::numeric_limits<Tick>::min() ? idx(band_.max_tick) : idx(best_bid_);
    for (; i >= idx(band_.max_tick); i--) {
      if (levels_[i].head != NIL) {
        best_ask_ = i + band_.min_tick;
        return;
      }
    }
    best_bid_ = std::numeric_limits<Tick>::min();
  }

};

class PriceLevelsSparse : public IPriceLevels {

  std::unordered_map<Tick, LevelFIFO> map_;
  Tick best_bid_{std::numeric_limits<Tick>::min()};
  Tick best_ask_{std::numeric_limits<Tick>::max()};
  LevelFIFO* best_bid_ptr_{nullptr};
  LevelFIFO* best_ask_ptr_{nullptr};
public:
  LevelFIFO & get_level(const Tick tx) override {
    return map_[tx];
  }

  bool has_level(const Tick tx) override {
    const auto it  = map_.find(tx);
    // Updated null check to use Handle NIL
    return map_.end() != it && it->second.head != NIL;
  }

  Tick best_bid() override {
    return best_bid_;
  }

  Tick best_ask() override {
    return best_ask_;
  }

  void set_best_ask(const Tick tx) override {
    best_ask_ = tx;
    if (tx == std::numeric_limits<Tick>::max()) {
      best_ask_ptr_ = nullptr;
      return;
    }
    best_ask_ptr_ = &map_[tx];
  }

  void set_best_bid(const Tick tx) override {
    best_bid_ = tx;
    if (tx == std::numeric_limits<Tick>::min()) {
      best_bid_ptr_ = nullptr;
      return;
    }
    best_bid_ptr_ = &map_[tx];
  }

  void find_best_bid() override {
    for (auto & it : map_) {
      if (it.second.head != NIL) {
        best_bid_ = std::max(best_bid_, it.second.price);
      }
    }
  }

  void find_best_ask() override {
    for (auto & it : map_) {
      if (it.second.head != NIL) {
        best_ask_ = std::min(best_ask_, it.second.price);
      }
    }
  }
};