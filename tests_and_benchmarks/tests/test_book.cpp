#include "test_framework.hpp"
#include "reference_book.hpp"
#include "workload.hpp"

namespace {

using namespace ob;

constexpr PriceBand kBand{0, 999, 1};

template <class B>
class Book : public ::testing::Test {
 protected:
  std::unique_ptr<B> book{new B(kBand)};

  ExecResult buy(OrderId id, Tick px, Quantity q) { return book->submit(make_order(id, kBuy, px, q)); }
  ExecResult sell(OrderId id, Tick px, Quantity q) { return book->submit(make_order(id, kSell, px, q)); }
};

using Books = ::testing::Types<ContiguousBook, SparseBook, StdMapBook>;
class Names {
 public:
  template <class T>
  static std::string GetName(int) { return T::name; }
};
TYPED_TEST_SUITE(Book, Books, Names);

// ------------------------------------------------------- passive orders

TYPED_TEST(Book, EmptyBook) {
  EXPECT_EQ(this->book->best_bid(), kNoBid);
  EXPECT_EQ(this->book->best_ask(), kNoAsk);
}

TYPED_TEST(Book, NonCrossingOrdersRestAndSetBest) {
  EXPECT_EQ(this->buy(1, 500, 10).remaining, 10);
  EXPECT_EQ(this->buy(2, 498, 10).filled, 0);
  this->buy(3, 502, 10);
  this->sell(4, 510, 10);
  this->sell(5, 505, 10);
  EXPECT_EQ(this->book->best_bid(), 502);
  EXPECT_EQ(this->book->best_ask(), 505);
  EXPECT_EQ(this->book->level_qty(kBuy, 500), 10);
}

TYPED_TEST(Book, SamePriceOrdersAggregate) {
  this->buy(1, 400, 5);
  this->buy(2, 400, 7);
  this->buy(3, 400, 9);
  EXPECT_EQ(this->book->level_qty(kBuy, 400), 21);
}

// ------------------------------------------------------------ matching

TYPED_TEST(Book, CrossingBuyFillsAtRestingPrice) {
  this->sell(1, 505, 10);
  auto r = this->buy(2, 510, 4);
  EXPECT_EQ(r.filled, 4);
  EXPECT_EQ(r.remaining, 0);
  EXPECT_EQ(this->book->level_qty(kSell, 505), 6);
  EXPECT_EQ(this->book->best_ask(), 505);
  EXPECT_EQ(this->book->best_bid(), kNoBid) << "fully filled order must not rest";
}

TYPED_TEST(Book, CrossAtExactlyTheBestPrice) {
  this->sell(1, 505, 10);
  EXPECT_EQ(this->buy(2, 505, 10).filled, 10);
  EXPECT_EQ(this->book->best_ask(), kNoAsk);
  this->buy(3, 300, 10);
  EXPECT_EQ(this->sell(4, 300, 3).filled, 3);
  EXPECT_EQ(this->book->level_qty(kBuy, 300), 7);
}

TYPED_TEST(Book, OneTickAwayDoesNotCross) {
  this->sell(1, 505, 10);
  EXPECT_EQ(this->buy(2, 504, 10).filled, 0);
  EXPECT_EQ(this->book->best_bid(), 504);
  EXPECT_EQ(this->book->best_ask(), 505);
}

TYPED_TEST(Book, TimePriorityWithinALevel) {
  this->sell(1, 505, 10);
  this->sell(2, 505, 10);
  this->sell(3, 505, 10);
  EXPECT_EQ(this->buy(9, 505, 15).filled, 15);  // All of #1, half of #2
  EXPECT_FALSE(this->book->cancel(1)) << "#1 was first in the queue and must be fully filled";
  EXPECT_TRUE(this->book->cancel(2)) << "#2 partially filled, remainder still resting";
  EXPECT_EQ(this->book->level_qty(kSell, 505), 10);
  EXPECT_TRUE(this->book->cancel(3));
}

TYPED_TEST(Book, SweepsLevelsInPriceOrderAndStopsAtLimit) {
  this->sell(1, 503, 5);
  this->sell(2, 501, 5);
  this->sell(3, 502, 5);
  this->sell(4, 510, 5);
  auto r = this->buy(9, 503, 100);
  EXPECT_EQ(r.filled, 15);
  EXPECT_EQ(r.remaining, 85);
  EXPECT_EQ(this->book->best_ask(), 510);
  EXPECT_EQ(this->book->best_bid(), 503) << "remainder rests at limit price";
  EXPECT_EQ(this->book->level_qty(kBuy, 503), 85);
}

TYPED_TEST(Book, SellSideSweepIsSymmetric) {
  this->buy(1, 497, 5);
  this->buy(2, 499, 5);
  this->buy(3, 498, 5);
  this->buy(4, 490, 5);
  auto r = this->sell(9, 497, 12);
  EXPECT_EQ(r.filled, 12);
  EXPECT_EQ(this->book->best_bid(), 497);
  EXPECT_EQ(this->book->level_qty(kBuy, 497), 3);
  EXPECT_EQ(this->book->best_ask(), kNoAsk);
}

TYPED_TEST(Book, SweepingTheWholeSideLeavesItEmpty) {
  for (OrderId i = 1; i <= 20; ++i) this->sell(i, 600 + static_cast<Tick>(i), 3);
  auto r = this->buy(99, 999, 60);
  EXPECT_EQ(r.filled, 60);
  EXPECT_EQ(r.remaining, 0);
  EXPECT_EQ(this->book->best_ask(), kNoAsk);
  EXPECT_EQ(this->book->best_bid(), kNoBid);
}

TYPED_TEST(Book, BestRecoversAfterSweepAndNewOrders) {
  this->sell(1, 505, 5);
  this->buy(2, 505, 5);
  this->sell(3, 507, 1);
  this->sell(4, 506, 1);
  EXPECT_EQ(this->book->best_ask(), 506);
}

// ---------------------------------------------------------------- cancel

TYPED_TEST(Book, CancelUpdatesLevelQty) {
  for (OrderId id = 1; id <= 5; ++id) this->sell(id, 600, static_cast<Quantity>(id));
  EXPECT_TRUE(this->book->cancel(3));
  EXPECT_EQ(this->book->level_qty(kSell, 600), 1 + 2 + 4 + 5);
  EXPECT_TRUE(this->book->cancel(1));
  EXPECT_TRUE(this->book->cancel(5));
  EXPECT_EQ(this->book->level_qty(kSell, 600), 2 + 4);
  EXPECT_EQ(this->book->best_ask(), 600);
}

TYPED_TEST(Book, CancelKeepsFifoLinksIntact) {
  for (OrderId id = 1; id <= 5; ++id) this->sell(id, 600, 10);
  this->book->cancel(3);
  this->book->cancel(1);
  this->book->cancel(5);
  EXPECT_EQ(this->buy(9, 600, 10).filled, 10);
  EXPECT_FALSE(this->book->cancel(2)) << "#2 is now the oldest and must have filled";
  EXPECT_TRUE(this->book->cancel(4));
  EXPECT_EQ(this->book->best_ask(), kNoAsk);
}

TYPED_TEST(Book, CancellingBestLevelFallsBackToNext) {
  this->buy(1, 300, 1);
  this->buy(2, 290, 1);
  this->sell(3, 310, 1);
  this->sell(4, 320, 1);
  this->book->cancel(1);
  EXPECT_EQ(this->book->best_bid(), 290);
  this->book->cancel(3);
  EXPECT_EQ(this->book->best_ask(), 320);
  this->book->cancel(2);
  this->book->cancel(4);
  EXPECT_EQ(this->book->best_bid(), kNoBid);
  EXPECT_EQ(this->book->best_ask(), kNoAsk);
}

TYPED_TEST(Book, CancellingNonBestLevelLeavesBestAlone) {
  this->buy(1, 300, 1);
  this->buy(2, 250, 1);
  this->book->cancel(2);
  EXPECT_EQ(this->book->best_bid(), 300);
  EXPECT_EQ(this->book->level_qty(kBuy, 250), 0);
}

TYPED_TEST(Book, CancelMisses) {
  EXPECT_FALSE(this->book->cancel(7)) << "never existed";
  this->buy(7, 100, 1);
  EXPECT_TRUE(this->book->cancel(7));
  EXPECT_FALSE(this->book->cancel(7)) << "already cancelled";
  this->sell(8, 200, 5);
  this->buy(9, 200, 5);
  EXPECT_FALSE(this->book->cancel(8)) << "already filled";
  EXPECT_FALSE(this->book->cancel(9)) << "aggressor fully filled, never rested";
}

TYPED_TEST(Book, BandEdgesWork) {
  this->buy(1, kBand.min_tick, 5);
  this->sell(2, kBand.max_tick, 5);
  EXPECT_EQ(this->book->best_bid(), kBand.min_tick);
  EXPECT_EQ(this->book->best_ask(), kBand.max_tick);
  EXPECT_EQ(this->buy(3, kBand.max_tick, 5).filled, 5);
  EXPECT_EQ(this->sell(4, kBand.min_tick, 5).filled, 5);
  EXPECT_EQ(this->book->best_bid(), kNoBid);
  EXPECT_EQ(this->book->best_ask(), kNoAsk);
}

TYPED_TEST(Book, ZeroQuantityIsANoOp) {
  this->sell(1, 500, 5);
  auto r = this->buy(2, 500, 0);
  EXPECT_EQ(r.filled, 0);
  EXPECT_EQ(this->book->level_qty(kSell, 500), 5);
  EXPECT_EQ(this->book->best_bid(), kNoBid);
}

// ---------------------------------------------------- differential tests

template <class B>
void run_differential_test(const WorkloadConfig& cfg) {
  const Workload w = generate_workload(cfg);
  auto core = std::make_unique<B>(cfg.band);
  StdMapBook ref(cfg.band);

  for (std::size_t i = 0; i < w.ops.size(); ++i) {
    const Op& op = w.ops[i];
    if (op.type == OpType::Cancel) {
      ASSERT_EQ(core->cancel(op.cancel_id), ref.cancel(op.cancel_id)) << "op " << i << " cancel " << op.cancel_id;
    } else {
      const ExecResult a = core->submit(op.order);
      const ExecResult b = ref.submit(op.order);
      ASSERT_EQ(a.filled, b.filled) << "op " << i << " id " << op.order.id;
      ASSERT_EQ(a.remaining, b.remaining) << "op " << i << " id " << op.order.id;
    }
    ASSERT_EQ(core->best_bid(), ref.best_bid()) << "op " << i;
    ASSERT_EQ(core->best_ask(), ref.best_ask()) << "op " << i;
    if (i % 1000 == 0 && core->best_bid() != kNoBid) {
      for (Tick t = std::max(cfg.band.min_tick, core->best_bid() - 20); t <= core->best_bid(); ++t) {
        ASSERT_EQ(core->level_qty(kBuy, t), ref.level_qty(kBuy, t)) << "op " << i << " bid level " << t;
      }
    }
  }

  for (Tick t = cfg.band.min_tick; t <= cfg.band.max_tick; ++t) {
    ASSERT_EQ(core->level_qty(kBuy, t), ref.level_qty(kBuy, t)) << "bid level " << t;
    ASSERT_EQ(core->level_qty(kSell, t), ref.level_qty(kSell, t)) << "ask level " << t;
  }
}

TEST(Differential, ContiguousDefaultWorkload) {
  WorkloadConfig cfg;
  cfg.num_ops = 50000;
  run_differential_test<ContiguousBook>(cfg);
}

TEST(Differential, SparseDefaultWorkload) {
  WorkloadConfig cfg;
  cfg.num_ops = 20000;
  run_differential_test<SparseBook>(cfg);
}

TEST(Differential, ContiguousAggressiveHeavy) {
  WorkloadConfig cfg;
  cfg.num_ops = 50000;
  cfg.seed = 7;
  cfg.aggressive_permille = 350;
  cfg.passive_permille = 450;
  cfg.max_aggressive_qty = 2000;
  cfg.max_through = 20;
  run_differential_test<ContiguousBook>(cfg);
}

TEST(Differential, ContiguousCancelHeavy) {
  WorkloadConfig cfg;
  cfg.num_ops = 50000;
  cfg.seed = 23;
  cfg.passive_permille = 300;
  cfg.aggressive_permille = 50;
  run_differential_test<ContiguousBook>(cfg);
}

TEST(Differential, ContiguousFastMovingMid) {
  WorkloadConfig cfg;
  cfg.num_ops = 30000;
  cfg.seed = 11;
  cfg.band = PriceBand{0, 199, 1};
  cfg.start_mid = 100;
  cfg.max_depth = 40;
  cfg.mid_move_permille = 200;
  cfg.max_live = 2000;
  run_differential_test<ContiguousBook>(cfg);
}

TEST(Workload, IsDeterministicForASeed) {
  WorkloadConfig cfg;
  cfg.num_ops = 20000;
  auto a = generate_workload(cfg), b = generate_workload(cfg);
  for (std::size_t i = 0; i < a.ops.size(); ++i) {
    ASSERT_EQ(static_cast<int>(a.ops[i].type), static_cast<int>(b.ops[i].type));
    ASSERT_EQ(a.ops[i].order.price, b.ops[i].order.price);
    ASSERT_EQ(a.ops[i].order.id, b.ops[i].order.id);
    ASSERT_EQ(a.ops[i].cancel_id, b.ops[i].cancel_id);
  }
}

TEST(Workload, StaysInBandAndActuallyTrades) {
  WorkloadConfig cfg;
  cfg.num_ops = 50000;
  auto w = generate_workload(cfg);
  for (const auto& op : w.ops) {
    if (op.type == OpType::Cancel) continue;
    ASSERT_GE(op.order.price, cfg.band.min_tick);
    ASSERT_LE(op.order.price, cfg.band.max_tick);
  }
  StdMapBook ref(cfg.band);
  auto st = replay(ref, w.ops);
  EXPECT_GT(st.filled_qty, 0);
  EXPECT_GT(st.cancel_misses, 0u);
  EXPECT_LT(st.cancel_misses * 20, st.cancels) << "cancel misses should be a minority";
  EXPECT_GT(ref.live_orders(), 500u) << "book should build realistic depth";
  EXPECT_NE(ref.best_bid(), kNoBid);
  EXPECT_NE(ref.best_ask(), kNoAsk);
}

// ------------------------------------------------ characterisation tests

TEST(PoolExhaustion, ContiguousDropsSilently) {
  auto b = std::make_unique<ContiguousBook>(kBand);
  for (OrderId i = 1; i <= static_cast<OrderId>(kCorePoolCapacity); ++i) {
    ASSERT_EQ(b->submit(make_order(i, kBuy, 10 + static_cast<Tick>(i % 50), 1)).remaining, 1);
  }
  const ExecResult r = b->submit(make_order(999999, kBuy, 500, 7));
  EXPECT_EQ(r.filled, 0);
  EXPECT_EQ(r.remaining, 7);
  EXPECT_FALSE(b->cancel(999999)) << "unrested order cannot be cancelled";
}

// --------------------------------------------------------------- known bugs

template <class B>
void duplicate_id_test() {
  auto b = std::make_unique<B>(kBand);
  b->submit(make_order(1, kSell, 100, 10));
  b->submit(make_order(1, kSell, 101, 10));
  EXPECT_EQ(b->submit(make_order(2, kBuy, 100, 10)).filled, 10);
  if (b->level_qty(kSell, 101) > 0) {
    EXPECT_TRUE(b->cancel(1)) << "order resting at 101 is orphaned: it can never be cancelled";
  }
}

TEST(KnownBug, DISABLED_DuplicateIdOrphansResting_Contiguous) { duplicate_id_test<ContiguousBook>(); }
TEST(KnownBug, DISABLED_DuplicateIdOrphansResting_Sparse) { duplicate_id_test<SparseBook>(); }

TEST(KnownBug, DISABLED_NegativeQuantityIsNotRejected) {
  ContiguousBook b(kBand);
  const ExecResult r = b.submit(make_order(1, kBuy, 100, -5));
  EXPECT_GE(r.remaining, 0) << "invalid negative quantity should be rejected, not echoed back";
}

}  // namespace

