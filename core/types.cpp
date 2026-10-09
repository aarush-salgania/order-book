#include <cstdint>
using OrderId = int64_t;
using Quantity = int32_t;
using Price = int32_t;
using Tick = int64_t;
using Side = char;

struct PriceBand {
    Tick min_tick;   // inclusive
    Tick max_tick;   // inclusive
    Tick tick_size;
};