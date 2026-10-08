#include <cstdint>
#include <vector>

enum OrderType {
  Limit,
  Market
};

class Order
{
private:
  static int64_t nextOrderId;
  OrderType _orderType;
  int64_t _orderId;
  char _side;
  int32_t _price;
  int32_t _quantity;

public:
  Order(OrderType order_type, char side, int32_t price, int32_t quantity)
    : _orderType(order_type),
      _side(side),
      _price(price),
      _quantity(quantity) {
    _orderId = nextOrderId++;
  }

  OrderType order_type() const {
    return _orderType;
  }

  int64_t order_id() const {
    return _orderId;
  }

  char side() const {
    return _side;
  }

  int32_t price() const {
    return _price;
  }

  int32_t quantity() const {
    return _quantity;
  }
};