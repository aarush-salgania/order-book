#include <cstdint>
#include <unordered_map>
#include <vector>
#include "types.cpp"

enum OrderType {
  Limit,
  Market
};

struct OrderNode
{
  OrderType _orderType;
  OrderId id;
  char side;
  Price _price;
  Quantity qty;
  OrderNode* next {nullptr};
  OrderNode* prev {nullptr};

  OrderNode(OrderType order_type, char side, int32_t price, int32_t quantity)
    : _orderType(order_type),
      side (side),
      _price(price),
      qty(quantity) {
  }
};

struct LevelFIFO {
  OrderNode *head;
  OrderNode *tail;
  Quantity qty;
  Price price;
};

class IPriceLevels {
public:
  virtual ~IPriceLevels() = default;

  virtual LevelFIFO& get_level(Tick tx) = 0;
  virtual bool has_level(Tick tx) = 0;

  virtual Tick best_bid(Tick tx) = 0;
  virtual Tick best_ask(Tick tx) = 0;

  virtual void set_best_ask(Tick tx) = 0;
  virtual void set_best_bid(Tick tx) = 0;
};

class PriceLevelCongiguous final : public IPriceLevels {
private:
  size_t idx(Tick px) const { return (px - band_.min_tick); }

  PriceBand band_;
  std::vector<LevelFIFO> levels_;
  Tick best_bid_;
  Tick best_ask_;
  LevelFIFO* best_bid_ptr_;
  LevelFIFO* best_ask_ptr_;
public:
  PriceLevelCongiguous(PriceBand band) :
  band_(band),
  levels_(static_cast<size_t>(band.max_tick - band.min_tick + 1)),
      best_bid_(std::numeric_limits<Tick>::min()),
      best_ask_(std::numeric_limits<Tick>::max()),
      best_bid_ptr_(nullptr),
      best_ask_ptr_(nullptr) {}

  LevelFIFO& get_level(Tick tx) override {
    return levels_[idx(tx)];
  }
  bool has_level(Tick tx) override {
    auto &l = levels_[idx(tx)];
    return l.head != nullptr;
  }
  LevelFIFO* best_level_ptr(Side S) {
    return S == 'B' ? best_bid_ptr_ : best_ask_ptr_;
  }
  void set_best_bid(Tick tx) override{
    best_bid_ = tx;
    best_bid_ptr_ = tx == std::numeric_limits<Tick>::min() ? nullptr : &levels_[idx(tx)];
  }
  void set_best_ask(Tick tx) override{
    best_ask_ = tx;
    best_ask_ptr_ = tx == std::numeric_limits<Tick>::max() ? nullptr : &levels_[idx(tx)];
  }
  void for_each_level(Tick tx) {

  }
};

class PriceLevelsSparse final : public IPriceLevels {
  private:
  std::unordered_map<Tick, LevelFIFO> map_;
  Tick best_bid_{std::numeric_limits<Tick>::min()};
  Tick best_ask_{std::numeric_limits<Tick>::max()};
  LevelFIFO* best_bid_ptr_{nullptr};
  LevelFIFO* best_ask_ptr_{nullptr};
public:
  LevelFIFO & get_level(Tick tx) override {
    return map_[tx];
  }

  bool has_level(Tick tx) override {
    auto it  = map_.find(tx);
    return map_.end() != it && it->second.head != nullptr;
  }

  Tick best_bid(Tick tx) override {
    return best_bid_;
  }

  Tick best_ask(Tick tx) override {
    return best_ask_;
  }

  void set_best_ask(Tick tx) override {
    best_ask_ = tx;
    if (tx == std::numeric_limits<Tick>::max()) {
      best_ask_ptr_ = nullptr;
      return;
    }
    best_ask_ptr_ = &map_[tx];
  }

  void set_best_bid(Tick tx) override {
    best_bid_ = tx;
    if (tx == std::numeric_limits<Tick>::min()) {
      best_bid_ptr_ = nullptr;
      return;
    }
    best_bid_ptr_ = &map_[tx];
  }
};