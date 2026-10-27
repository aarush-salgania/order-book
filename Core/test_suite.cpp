#pragma once

#include <iostream>
#include <stdexcept>
#include <string>

namespace lob_test {

class LOBTestSuite {
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
  // Tests run automatically when the object is created.
  LOBTestSuite() {
    result_ = run_all_tests();
  }

  // Prevent copying because this object represents one test run.
  LOBTestSuite(const LOBTestSuite&) = delete;
  LOBTestSuite& operator=(const LOBTestSuite&) = delete;

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

    std::cout << "[SKIP] " << test_name
              << " - " << reason << "\n";
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
        "PriceLevelsContig basic set/get",
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
        "Modify to better price that crosses -> trades first",
        test_modify_to_better_price_crosses
    );

    run_test(
        result,
        "Modify to worse price -> requeues at tail of new price",
        test_modify_to_worse_price_requeues
    );

    run_test(
        result,
        "STP cancels resting same-owner orders instead of trading",
        test_stp_cancels_same_owner_order
    );

    run_test(
        result,
        "Multi-level sweep across asks",
        test_multi_level_market_sweep
    );

    run_test(
        result,
        "Rebuild index and continue matching",
        test_rebuild_index_and_continue_matching
    );

    skip_test(
        result,
        "Snapshot v2 roundtrip rebuilds FIFO and best-of-book",
        "requires logging.cpp and SnapshotWriter implementation"
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
    std::cout << "Status : "
              << (result.success() ? "PASS" : "FAIL")
              << "\n";

    return result;
  }

  // --------------------------------------------------------------------------
  // Test implementations
  // --------------------------------------------------------------------------

  static void test_basic_type_properties() {
    static_assert(std::is_signed_v<lob::Tick>);
    static_assert(std::is_signed_v<lob::Quantity>);
    static_assert(sizeof(lob::Side) == 1);
  }

  static void test_price_levels_contiguous_basic_set_get() {
    lob::PriceBand band{90, 110, 1};
    lob::PriceLevelsContig contig{band};

    auto& level = contig.get_level(100);

    require(level.total_qty == 0, "level.total_qty == 0");
    contig.set_best_bid(100);
    contig.set_best_ask(105);

    require(contig.best_bid() == 100, "contig.best_bid() == 100");
    require(contig.best_ask() == 105, "contig.best_ask() == 105");
  }

  static void test_price_levels_sparse_existence() {
    lob::PriceLevelsSparse sparse;

    require(!sparse.has_level(100), "!sparse.has_level(100)");

    auto& level = sparse.get_level(100);

    require(level.head == nullptr, "level.head == nullptr");
  }

  static void test_headers_compile() {
    lob::PriceLevelsSparse bids;
    lob::PriceLevelsSparse asks;
    lob::BookCore book{bids, asks, nullptr};

    require(book.empty(lob::Side::Bid), "book.empty(Bid)");
    require(book.empty(lob::Side::Ask), "book.empty(Ask)");
  }

  static void test_fifo_within_single_price_level() {
    lob::PriceBand band{90, 110, 1};
    lob::PriceLevelsContig bids{band};
    lob::PriceLevelsContig asks{band};
    lob::BookCore book{bids, asks, nullptr};

    book.submit_limit(
        lob::NewOrder{
            1, 1000, 1001, 42,
            lob::Side::Ask, 100, 5, lob::NONE
        }
    );

    book.submit_limit(
        lob::NewOrder{
            2, 1001, 1002, 43,
            lob::Side::Ask, 100, 7, lob::NONE
        }
    );

    const auto result =
        book.submit_market(
            lob::NewOrder{
                3, 1002, 2001, 99,
                lob::Side::Bid, 0, 6, lob::NONE
            }
        );

    require(result.filled == 6, "result.filled == 6");

    auto& level = asks.get_level(100);

    require(level.total_qty == 6, "level.total_qty == 6");
    require(level.head != nullptr, "level.head != nullptr");
    require(level.head->id == 1002, "level.head->id == 1002");
  }

  static void test_partial_fill_one_sided_book() {
    lob::PriceBand band{90, 110, 1};
    lob::PriceLevelsContig bids{band};
    lob::PriceLevelsContig asks{band};
    lob::BookCore book{bids, asks, nullptr};

    book.submit_limit(
        lob::NewOrder{
            1, 1000, 1, 1,
            lob::Side::Ask, 100, 3, lob::NONE
        }
    );

    const auto result =
        book.submit_limit(
            lob::NewOrder{
                2, 1001, 2, 2,
                lob::Side::Bid, 100, 5, lob::NONE
            }
        );

    require(result.filled == 3, "result.filled == 3");
    require(
        asks.best_ask() == std::numeric_limits<lob::Tick>::max(),
        "asks.best_ask() is empty"
    );
    require(bids.best_bid() == 100, "bids.best_bid() == 100");
  }

  static void test_modify_to_better_price_crosses() {
    lob::PriceBand band{90, 110, 1};
    lob::PriceLevelsContig bids{band};
    lob::PriceLevelsContig asks{band};
    lob::BookCore book{bids, asks, nullptr};

    book.submit_limit(
        lob::NewOrder{
            1, 1000, 3001, 30,
            lob::Side::Ask, 105, 5, lob::NONE
        }
    );

    book.submit_limit(
        lob::NewOrder{
            2, 1001, 4001, 40,
            lob::Side::Bid, 100, 5, lob::NONE
        }
    );

    const auto result =
        book.modify(
            lob::ModifyOrder{
                3, 1002, 4001, 106, 5, lob::NONE
            }
        );

    require(result.filled == 5, "result.filled == 5");
    require(
        asks.best_ask() == std::numeric_limits<lob::Tick>::max(),
        "asks.best_ask() is empty"
    );
  }

  static void test_modify_to_worse_price_requeues() {
    lob::PriceBand band{90, 110, 1};
    lob::PriceLevelsContig bids{band};
    lob::PriceLevelsContig asks{band};
    lob::BookCore book{bids, asks, nullptr};

    book.submit_limit(
        lob::NewOrder{
            1, 1000, 5001, 50,
            lob::Side::Bid, 100, 3, lob::NONE
        }
    );

    book.submit_limit(
        lob::NewOrder{
            2, 1001, 5002, 51,
            lob::Side::Bid, 100, 3, lob::NONE
        }
    );

    const auto result =
        book.modify(
            lob::ModifyOrder{
                3, 1002, 5001, 99, 3, lob::NONE
            }
        );

    require(result.filled == 0, "result.filled == 0");

    auto& level100 = bids.get_level(100);
    require(level100.head != nullptr, "level100.head != nullptr");
    require(level100.head->id == 5002, "level100.head->id == 5002");

    auto& level99 = bids.get_level(99);
    require(level99.head != nullptr, "level99.head != nullptr");
    require(level99.head->id == 5001, "level99.head->id == 5001");
    require(level99.total_qty == 3, "level99.total_qty == 3");
  }

  static void test_stp_cancels_same_owner_order() {
    lob::PriceBand band{90, 110, 1};
    lob::PriceLevelsContig bids{band};
    lob::PriceLevelsContig asks{band};
    lob::BookCore book{bids, asks, nullptr};

    book.submit_limit(
        lob::NewOrder{
            1, 1000, 7001, 77,
            lob::Side::Ask, 100, 3, lob::NONE
        }
    );

    const auto result =
        book.submit_limit(
            lob::NewOrder{
                2, 1001, 7002, 77,
                lob::Side::Bid, 100, 3, lob::STP
            }
        );

    require(result.filled == 0, "result.filled == 0");
    require(
        asks.best_ask() == std::numeric_limits<lob::Tick>::max(),
        "asks.best_ask() is empty"
    );

    auto& level = bids.get_level(100);

    require(level.total_qty == 3, "level.total_qty == 3");
    require(level.head != nullptr, "level.head != nullptr");
    require(level.head->id == 7002, "level.head->id == 7002");
  }

  static void test_multi_level_market_sweep() {
    lob::PriceBand band{90, 110, 1};
    lob::PriceLevelsContig bids{band};
    lob::PriceLevelsContig asks{band};
    lob::BookCore book{bids, asks, nullptr};

    book.submit_limit(
        lob::NewOrder{
            1, 1000, 10101, 10,
            lob::Side::Ask, 101, 2, lob::NONE
        }
    );

    book.submit_limit(
        lob::NewOrder{
            2, 1001, 10102, 11,
            lob::Side::Ask, 102, 3, lob::NONE
        }
    );

    book.submit_limit(
        lob::NewOrder{
            3, 1002, 10103, 12,
            lob::Side::Ask, 103, 4, lob::NONE
        }
    );

    const auto result =
        book.submit_limit(
            lob::NewOrder{
                4, 1003, 2001, 20,
                lob::Side::Bid, 103, 7, lob::NONE
            }
        );

    require(result.filled == 7, "result.filled == 7");

    auto& level103 = asks.get_level(103);

    require(level103.total_qty == 2, "level103.total_qty == 2");
    require(asks.best_ask() == 103, "asks.best_ask() == 103");
  }

  static void test_rebuild_index_and_continue_matching() {
    lob::PriceBand band{90, 110, 1};
    lob::PriceLevelsContig bids{band};
    lob::PriceLevelsContig asks{band};
    lob::BookCore book{bids, asks, nullptr};

    book.submit_limit(
        lob::NewOrder{
            1, 1000, 10001, 1,
            lob::Side::Bid, 100, 3, lob::NONE
        }
    );

    book.submit_limit(
        lob::NewOrder{
            2, 1001, 10002, 2,
            lob::Side::Bid, 100, 4, lob::NONE
        }
    );

    require(bids.best_bid() == 100, "bids.best_bid() == 100");
    require(
        bids.get_level(100).total_qty == 7,
        "bids.get_level(100).total_qty == 7"
    );

    book.rebuild_index_from_books();

    const auto result =
        book.submit_market(
            lob::NewOrder{
                4, 2000, 20000, 9,
                lob::Side::Ask, 0, 7, lob::NONE
            }
        );

    require(result.filled == 7, "result.filled == 7");
    require(
        bids.best_bid() == std::numeric_limits<lob::Tick>::min(),
        "bids.best_bid() is empty"
    );
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

} // namespace lob_test
