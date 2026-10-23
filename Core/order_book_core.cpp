#pragma once
#include "order.cpp"
#include "../types.cpp"
#include "mempool.cpp"

struct ExecResult {
    Quantity filled{0};
    Quantity remaining{0};
};

class BookCore {
private:
    struct IdEntry {
        char side;
        Tick price;
        Handle idx;
    };
    IPriceLevels & asks_;
    IPriceLevels & bids_;
    std::unordered_map<OrderId, IdEntry> orders_;
    Pool<OrderNode> order_pool_{100000};

    void free_node(const OrderNode *node) {
        Handle idx = order_pool_.pointer_to_handle(node);
        order_pool_.dealloc(idx);
    }

    void free_node(const Handle h) {
        order_pool_.dealloc(h);
    }

    void enqueue(LevelFIFO& L, const Handle idx) {
        OrderNode& node = order_pool_[idx];
        node.next = NIL;
        node.prev = L.tail;
        if (L.tail != NIL) {
            order_pool_[L.tail].next = idx;
        } else {
            L.head = idx;
        }
        L.tail = idx;
        L.qty += node.qty;
    }

    void erase(LevelFIFO& L, const Handle idx) {
        OrderNode& node = order_pool_[idx];
        if (node.prev != NIL) {
            order_pool_[node.prev].next = node.next;
        } else {
            L.head = node.next;
        }

        if (node.next != NIL) {
            order_pool_[node.next].prev = node.prev;
        } else {
            L.tail = node.prev;
        }

        node.prev = NIL;
        node.next = NIL;
        L.qty -= node.qty;
    }

    template<bool isBid>
    ExecResult submit_limit_side(const NewOrder& new_order) {
        ExecResult res{0, new_order.qty};
        Tick opp_best = isBid ? asks_.best_ask() : bids_.best_bid();
        bool crosses = isBid ? new_order.price >= opp_best : new_order.price <= opp_best;

        if (crosses) {
            Quantity matched = match_against_side<isBid>(new_order.qty, new_order.price);
            res.filled += matched;
            res.remaining -= matched;
        }

        if (res.remaining  > 0) {
            IPriceLevels& same_side = isBid ? bids_ : asks_;
            LevelFIFO& L = same_side.get_level(new_order.price);

            Handle h = order_pool_.alloc();
            OrderNode& node = order_pool_[h];
            node.id = new_order.id;
            node.side = new_order.side;
            node.price = new_order.price;
            node.qty = new_order.qty;

            enqueue(L, h);
            orders_[new_order.id] = IdEntry{isBid ? 'B' : 'S', new_order.price, h};

            if (isBid) {
                if (new_order.price > bids_.best_bid()) bids_.set_best_bid(new_order.price);
            } else {
                if (new_order.price < asks_.best_ask()) asks_.set_best_ask(new_order.price);
            }
        }

        return res;
    }
    template<bool isBid>
    Quantity match_against_side(Quantity qty, const Tick px_limit) {
        Quantity total_matched = 0;
        IPriceLevels &opp_side = isBid ? asks_ : bids_;
        while (qty > 0) {
            Tick best_px = isBid ? asks_.best_ask() : bids_.best_bid();
            bool can_match = isBid ? (px_limit >= best_px) : (best_px >= px_limit);
            if (!can_match) break;

            LevelFIFO& L = opp_side.get_level(best_px);
            if (L.head == NIL) {
                refresh_best_after_depletion(isBid ? 'S' : 'B');
                continue;
            }
            Handle curr_idx = L.head;
            while (curr_idx != NIL && qty > 0) {
                OrderNode& curr = order_pool_[curr_idx];
                Handle next_idx = curr.next;

                Quantity fill_qty = std::min(qty, curr.qty);
                qty -= fill_qty;
                L.qty -= fill_qty;
                curr.qty -= fill_qty;
                total_matched += curr.qty;

                if (curr.qty == 0) {
                    erase(L, curr_idx);
                    free_node(&curr);
                    orders_.erase(curr.id);
                }
            }
            if (L.head == NIL) {
                refresh_best_after_depletion(isBid ? 'S' : 'B');
            }
        }

        return total_matched;
    }
    void refresh_best_after_depletion(Side side) {
        if (side == 'B') {
            bids_.find_best_ask();
        } else {
            asks_.find_best_ask();
        }
    }
public:
    BookCore(IPriceLevels& bids, IPriceLevels& asks) : bids_(bids), asks_(asks) {
        order_pool_.alloc();
    }

    bool empty(const Side side) const {
        return (side == 'B') ?
        bids_.best_bid() == std::numeric_limits<Tick>::min() :
        asks_.best_bid() == std::numeric_limits<Tick>::min();
    }

    bool cancel(OrderId id) {
        auto it = orders_.find(id);
        if (it == orders_.end()) return false;

        const IdEntry& entry = it->second;
        IPriceLevels& side = entry.side == 'B' ? bids_ : asks_;
        LevelFIFO& L = side.get_level(entry.price);

        erase(L, entry.idx);
        free_node(entry.idx);
        orders_.erase(it);

        if (L.head == NIL) {
            refresh_best_after_depletion('B');
        }

        return true;
    }

    ExecResult submit_limit(const NewOrder& new_order) {
        if (new_order.side == 'B') {
            return submit_limit_side<true>(new_order);
        }
        return submit_limit_side<false>(new_order);
    }
};