#include <bits/stdc++.h>

//differntiating by order types

enum class OrderType 
{
  GoodTillCancel,
  FillandKill
};

enum class Side
{
  Buy,
  Sell
};

using Price = std::int32_t;       //type alias named Price, for improved readibility.
using Quantity = std::uint32_t;   //uint means unsigned, cuz quantity can be negative.
using OrderId = std::uint64_t;    //64 bit instead of 32, cuz HFTs process millions of orders/sec.

struct LevelInfo            //struct means a custom datatype, for example, LevelInfo leeveel = {1000,50} means
{                           //50 units at 1000 rs unit price...so, a struct stores related datatypes (such as roll no. and name).
  Price price_;             //therefore, a level of the orderbook is just a (price,quantity) pair.
  Quantity quantity_;       //thus, there are Sell levels and Buy levels.
};                          //and this pair/level of the orderbook is stored in LevelInfo.
//for example, leeveel is an instance of this struct.

using LevelInfos  = std::vector<LevelInfo>;  //dynamic list of (price,quantity) pairs/levels.
//for example, LevelInfos bids_ is a vector of all Sell levels...such as: {{100,50},{20,300},{50,10}}.

class OrderBookLevelInfos   //class to store & access all Buy orders (bids) and Sell orders (asks).
{
  public:
  OrderBookLevelInfos (const LevelInfos& asks, const LevelInfos& bids)
  : asks_{ asks },  //constructor says, that when you create OrderBookLevelInfos, give me two lists...
    bids_{ bids }   //one for bids and one for asks ()
  { }
  const LevelInfos& GetBids() const { return bids_; }
  const LevelInfos& GetAsks() const { return asks_; }

  private:
  LevelInfos bids_;  //the vector of Buy levels.
  LevelInfos asks_;  //the vector of Sell levels.
};

class Order 
{
  public:
  Order(OrderType orderType, OrderId orderId, Side side, Price price, Quantity quantity) 
  : orderType_{ orderType }
  , orderId_{ orderId }
  , side_{ side }
  , price_{ price }
  , initialQuantity_{ quantity }
  , remainingQuantity_{ quantity }
  { }

  OrderType GetOrderType() const { return orderType_; }
  OrderId GetOrderId() const { return orderId_; }
  Side GetSide() const { return side_; }
  Price GetPrice() const { return price_; }
  Quantity GetInitialQuantity() const { return initialQuantity_; }
  Quantity GetRemainingQuantity() const { return remainingQuantity_; }
  Quantity GetFilledQuantity() const { return GetInitialQuantity() - GetRemainingQuantity(); } 

  void Fill(Quantity quantity)
  {
    if (quantity > GetRemainingQuantity()) 
    {
      std::ostringstream oss;
      oss << "Order (" << GetOrderId() << ") cannot be filled for more than its remaining quantity.";
      throw std::logic_error(oss.str());
    }

    remainingQuantity_ -= quantity;
  
  }

  private:
  OrderType orderType_;
  OrderId orderId_;
  Side side_;
  Price price_;
  Quantity initialQuantity_;
  Quantity remainingQuantity_;

};

using OrderPointer = std::shared_ptr<Order>; 
using Orderpointers = std::list<OrderPointer>;  //using a list for simplicity

int main() {
  
  return 0; 
}
