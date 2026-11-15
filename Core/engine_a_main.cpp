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

    OrderId next_id = 1;
    std::string line;
    std::cerr << "[engine] ready" << std::endl;

    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
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
            if (price <= 0 || price * TICKS_PER_RUPEE > band.max_tick || qty_d < 1) {
                std::cerr << "[engine] ignored (out of range): " << line << std::endl;
                continue;
            }

            const Tick tick = static_cast<Tick>(std::llround(price * TICKS_PER_RUPEE));
            const Quantity qty = static_cast<Quantity>(std::llround(qty_d));

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

            const ExecResult res = book->submit_limit(order);

            if (res.filled > 0) {
                const double trade_price = static_cast<double>(best_opp) / TICKS_PER_RUPEE;
                std::cout << "TRADE_EXECUTED: {\"side\":\"" << (side == 'B' ? "buy" : "sell")
                          << "\",\"price\":" << std::fixed << std::setprecision(2) << trade_price
                          << ",\"quantity\":" << res.filled << "}" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[engine] error: " << e.what() << " | line: " << line << std::endl;
        }
    }
    return 0;
}