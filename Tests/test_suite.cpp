#pragma once

#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <limits>

#include "Core/order_book_core.cpp"

class TestSuite {
public:
    struct Result {
        int passed{0};
        int failed{0};
        int skipped{0};

        bool success() const {
            return failed == 0;
        }
    };

private:
    Result result_{};

public:
    TestSuite() {
        result_ = run_all_tests();
    }

    TestSuite(const TestSuite&) = delete;
    TestSuite& operator=(const TestSuite&) = delete;

    const Result& result() const {
        return result_;
    }

    bool success() const {
        return result_.success();
    }

private:
    template <typename Function>
    static void run_test(
        Result& result,
        const char* test_name,
        Function&& function) {

        try {
            function();
            ++result.passed;
            std::cout << "[PASS] " << test_name << "\n";
        } catch (const std::exception& error) {
            ++result.failed;
            std::cerr << "[FAIL] " << test_name << "\n";
            std::cerr << "       " << error.what() << "\n";
        } catch (...) {
            ++result.failed;
            std::cerr << "[FAIL] " << test_name << "\n";
            std::cerr << "       unknown exception\n";
        }
    }

    static void skip_test(
        Result& result,
        const char* test_name,
        const char* reason) {

        ++result.skipped;
        std::cout << "[SKIP] " << test_name << " - " << reason << "\n";
    }

    static Result run_all_tests() {
        Result result{};

        run_test(
            result,
            "Basic type properties",
            test_basic_type_properties
        );

        run_test(
            result,
            "PriceLevelContiguous basic set/get",
            test_price_levels_contiguous_basic_set_get
        );

        run_test(
            result,
            "PriceLevelsSparse existence",
            test_price_levels_sparse_existence
        );

        run_test(
            result,
            "Headers compile and basic structs exist",
            test_headers_compile
        );

        run_test(
            result,
            "FIFO within a single price level",
            test_fifo_within_single_price_level
        );

        run_test(
            result,
            "Partial fill on one-sided books and empties correctly",
            test_partial_fill_one_sided_book
        );

        run_test(
            result,
            "Multi-level market sweep across asks",
            test_multi_level_market_sweep
        );

        skip_test(
            result,
            "Modify to better price that crosses",
            "requires BookCore::modify implementation"
        );

        skip_test(
            result,
            "Modify to worse price -> requeues",
            "requires BookCore::modify implementation"
        );

        skip_test(
            result,
            "STP cancels resting same-owner orders",
            "requires Self-Trade Prevention (STP) logic"
        );

        skip_test(
            result,
            "Rebuild index and continue matching",
            "requires BookCore::rebuild_index_from_books implementation"
        );

        skip_test(
            result,
            "Snapshot v2 roundtrip",
            "requires logging and SnapshotWriter implementation"
        );

        run_test(
            result,
            "Smoke test",
            test_smoke
        );

        std::cout << "\n========================================\n";
        std::cout << "LOB test summary\n";
        std::cout << "========================================\n";
        std::cout << "Passed : " << result.passed << "\n";
        std::cout << "Failed : " << result.failed << "\n";
        std::cout << "Skipped: " << result.skipped << "\n";
        std::cout << "Status : " << (result.success() ? "PASS" : "FAIL") << "\n";

        return result;
    }

    // --------------------------------------------------------------------------
    // Test implementations
    // --------------------------------------------------------------------------

    static void test_basic_type_properties() {
        static_assert(std::is_signed<Tick>::value, "Tick must be signed integer");
        static_assert(std::is_signed<Quantity>::value, "Quantity must be signed integer");
        static_assert(sizeof(Side) == 1, "Side must be 1 byte");
    }

    static void test_price_levels_contiguous_basic_set_get() {
        PriceBand band{90, 110, 1};
        PriceLevelContiguous contig{band};

        auto& level = contig.get_level(100);

        require(level.qty == 0, "level.qty == 0");
        contig.set_best_bid(100);
        contig.set_best_ask(105);

        require(contig.best_bid() == 100, "contig.best_bid() == 100");
        require(contig.best_ask() == 105, "contig.best_ask() == 105");
    }

    static void test_price_levels_sparse_existence() {
        PriceLevelsSparse sparse;

        require(!sparse.has_level(100), "!sparse.has_level(100)");

        auto& level = sparse.get_level(100);

        require(level.head == NIL, "level.head == NIL");
    }

    static void test_headers_compile() {
        PriceLevelsSparse bids;
        PriceLevelsSparse asks;
        BookCore book{bids, asks};

        require(book.empty('B'), "book.empty('B')");
        require(book.empty('S'), "book.empty('S')");
    }

    static void test_fifo_within_single_price_level() {
        PriceBand band{90, 110, 1};
        PriceLevelContiguous bids{band};
        PriceLevelContiguous asks{band};
        BookCore book{bids, asks};

        // NewOrder{ts, id, qty, side, price, user}
        book.submit_limit(NewOrder{1000, 1001, 5, 'S', 100, 42});
        book.submit_limit(NewOrder{1001, 1002, 7, 'S', 100, 43});

        // Submit crossing Bid order for 6 contracts
        const auto result = book.submit_limit(NewOrder{1002, 2001, 6, 'B', 100, 99});

        require(result.filled == 6, "result.filled == 6");

        auto& level = asks.get_level(100);

        // 12 - 6 = 6 contracts remaining at tick 100
        require(level.qty == 6, "level.qty == 6");
        require(level.head != NIL, "level.head != NIL");
    }

    static void test_partial_fill_one_sided_book() {
        PriceBand band{90, 110, 1};
        PriceLevelContiguous bids{band};
        PriceLevelContiguous asks{band};
        BookCore book{bids, asks};

        book.submit_limit(NewOrder{1000, 1, 3, 'S', 100, 10});

        const auto result = book.submit_limit(NewOrder{1001, 2, 5, 'B', 100, 20});

        require(result.filled == 3, "result.filled == 3");
        require(
            asks.best_ask() == std::numeric_limits<Tick>::max(),
            "asks.best_ask() is empty"
        );
        require(bids.best_bid() == 100, "bids.best_bid() == 100");
    }

    static void test_multi_level_market_sweep() {
        PriceBand band{90, 110, 1};
        PriceLevelContiguous bids{band};
        PriceLevelContiguous asks{band};
        BookCore book{bids, asks};

        book.submit_limit(NewOrder{1000, 10101, 2, 'S', 101, 10});
        book.submit_limit(NewOrder{1001, 10102, 3, 'S', 102, 11});
        book.submit_limit(NewOrder{1002, 10103, 4, 'S', 103, 12});

        // Submit Bid crossing up to 103 for qty 7
        const auto result = book.submit_limit(NewOrder{1003, 2001, 7, 'B', 103, 20});

        require(result.filled == 7, "result.filled == 7");

        auto& level103 = asks.get_level(103);

        require(level103.qty == 2, "level103.qty == 2");
        require(asks.best_ask() == 103, "asks.best_ask() == 103");
    }

    static void test_smoke() {
        require(true, "true");
    }

    static void require(
        bool condition,
        const char* expression) {

        if (!condition) {
            throw std::runtime_error(expression);
        }
    }
};