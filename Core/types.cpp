#pragma once

#include <cstdint>
using OrderId = int64_t;
using Quantity = int32_t;
using Price = int32_t;
using Tick = int32_t;
using Timestamp = int64_t;
using UserId = int64_t;
using Side = char;
using Handle = std::uint32_t;  //using a 32 bit int index instead of 64 bit pointer - less space...

static constexpr Handle NIL = 0xFFFFFFFFu;   //reserve the biggest uint32 value as NIL to mean invalid/no slot.
//constexpr cuz its a compile time const and static cuz this NIL is shared by all objects of Pool.

struct PriceBand {
    Tick min_tick;   // inclusive
    Tick max_tick;   // inclusive
    Tick tick_size;
};

struct NewOrder {
    Timestamp ts;
    OrderId id;
    Quantity qty;
    Side side;
    Tick price;
    UserId user;
};