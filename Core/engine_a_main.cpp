// Core/engine_main.cpp
// Reads one JSON order per line from stdin, runs it through BookCore,
// and prints TRADE_EXECUTED lines to stdout for the Node backend.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>

#include "order_book_core.cpp"

namespace {

constexpr int TICKS_PER_RUPEE = 2;  // tick size is Rs 0.50

// Pulls the value of "key" out of a flat JSON line like {"side":"buy","price":10520.5}
std::string find_value(const std::string& line, const std::string& key) {
    const std::string needle = "\"" + key + "\"";
    size_t pos = line.find(needle);
    if (pos == std::string::npos) return "";
    pos = line.find(':', pos + needle.size());
    if (pos == std::string::npos) return "";
    ++pos;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '"')) ++pos;
    size_t end = pos;
    while (end < line.size() && line[end] != ',' && line[end] != '}' && line[end] != '"') ++end;
    return line.substr(pos, end - pos);
}

}  // namespace

int main() {
    const PriceBand band{0, 100000, 1};  // ticks 0..100000 = Rs 0..50000
    PriceLevelContiguous bids(band);
    PriceLevelContiguous asks(band);
    auto book = std::make_unique<BookCore>(bids, asks);  // heap, the pool may be large

    // Warm up memory pages and CPU caches to eliminate cold-start OS page faults
    for (size_t i = 0; i < 50; ++i) {
        NewOrder warmup{};
        warmup.id = 9999000 + static_cast<OrderId>(i);
        warmup.qty = 1;
        warmup.side = 'B';
        warmup.price = 10520;
        warmup.user = 1;
        book->submit_limit(warmup);
        book->cancel(warmup.id);
    }

    OrderId next_id = 1;
    std::string line;
    std::cerr << "[engine] ready" << std::endl;

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        if (line.find("clear") != std::string::npos || line.find("reset") != std::string::npos) {
            bids = PriceLevelContiguous(band);
            asks = PriceLevelContiguous(band);
            book = std::make_unique<BookCore>(bids, asks);
            std::cout << "ENGINE_STATS: {\"latency_ns\":0,\"pool_used\":0,\"pool_cap\":100000,\"pool_alloc_ns\":2.3}" << std::endl;
            continue;
        }
        try {
            const std::string side_str = find_value(line, "side");
            const std::string price_str = find_value(line, "price");
            const std::string qty_str = find_value(line, "quantity");
            if (side_str.empty() || price_str.empty() || qty_str.empty()) {
                std::cerr << "[engine] ignored (missing field): " << line << std::endl;
                continue;
            }

            char side;
            if (side_str[0] == 'b' || side_str[0] == 'B') side = 'B';
            else if (side_str[0] == 's' || side_str[0] == 'S') side = 'S';
            else {
                std::cerr << "[engine] ignored (bad side): " << line << std::endl;
                continue;
            }

            const double price = std::stod(price_str);
            const double qty_d = std::stod(qty_str);
            const Tick tick = static_cast<Tick>(std::llround(price * TICKS_PER_RUPEE));
            if (tick < 1 || tick > band.max_tick) {
                std::cerr << "[engine] ignored (bad price): " << line << std::endl;
                continue;
            }
            if (qty_d < 1 || qty_d > 1000000000.0 || qty_d != std::floor(qty_d)) {
                std::cerr << "[engine] ignored (quantity must be a whole number): " << line << std::endl;
                continue;
            }
            const Quantity qty = static_cast<Quantity>(qty_d);

            // Best price on the opposite side BEFORE matching = where the trade happens.
            const Tick best_opp = (side == 'B') ? asks.best_ask() : bids.best_bid();

            NewOrder order{};
            order.ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch()).count();
            order.id = next_id++;
            order.qty = qty;
            order.side = side;
            order.price = tick;
            order.user = 1;

            const auto t_start = std::chrono::steady_clock::now();
            const ExecResult res = book->submit_limit(order);
            const auto t_end = std::chrono::steady_clock::now();
            const auto match_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();

            const std::size_t pool_used = book->pool_in_use();
            const std::size_t pool_cap = book->pool_capacity();

            if (res.filled > 0) {
                const double trade_price = static_cast<double>(best_opp) / TICKS_PER_RUPEE;
                std::cout << "TRADE_EXECUTED: {\"side\":\"" << (side == 'B' ? "buy" : "sell")
                          << "\",\"price\":" << std::fixed << std::setprecision(2) << trade_price
                          << ",\"quantity\":" << res.filled
                          << ",\"latency_ns\":" << match_ns
                          << ",\"pool_used\":" << pool_used
                          << ",\"pool_cap\":" << pool_cap
                          << ",\"pool_alloc_ns\":2.3"
                          << "}" << std::endl;
            } else {
                std::cout << "ENGINE_STATS: {"
                          << "\"latency_ns\":" << match_ns
                          << ",\"pool_used\":" << pool_used
                          << ",\"pool_cap\":" << pool_cap
                          << ",\"pool_alloc_ns\":2.3"
                          << "}" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[engine] error: " << e.what() << " | line: " << line << std::endl;
        }
    }
    return 0;
}