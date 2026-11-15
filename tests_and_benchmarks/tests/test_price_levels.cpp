#include "test_framework.hpp"
#include "core_shim.hpp"

namespace {

using ob::kNoAsk;
using ob::kNoBid;

constexpr PriceBand kBand{100, 1099, 1};

template <class L>
class PriceLevels : public ::testing::Test {
 protected:
  PriceLevels() : impl_(ob::Ladder<L>::make(kBand)), lv_(*impl_) {}

  void fill(Tick t) { lv_.get_level(t).head = 0; }
  void drain(Tick t) { lv_.get_level(t).head = NIL; }

  std::unique_ptr<L> impl_;
  IPriceLevels& lv_;
};

using Impls = ::testing::Types<PriceLevelContiguous, PriceLevelsSparse>;
class Names {
 public:
  template <class T>
  static std::string GetName(int) { return ob::Ladder<T>::name; }
};
TYPED_TEST_SUITE(PriceLevels, Impls, Names);

TYPED_TEST(PriceLevels, StartsEmptyWithSentinelBests) {
  for (Tick t = kBand.min_tick; t <= kBand.max_tick; ++t) {
    ASSERT_FALSE(this->lv_.has_level(t)) << t;
  }
  EXPECT_EQ(this->lv_.best_bid(), kNoBid);
  EXPECT_EQ(this->lv_.best_ask(), kNoAsk);
}

TYPED_TEST(PriceLevels, FreshLevelIsEmpty) {
  LevelFIFO& L = this->lv_.get_level(500);
  EXPECT_EQ(L.head, NIL);
  EXPECT_EQ(L.tail, NIL);
  EXPECT_EQ(L.qty, 0);
}

TYPED_TEST(PriceLevels, GetLevelIsStablePerTickAndDistinctAcrossTicks) {
  LevelFIFO* a = &this->lv_.get_level(kBand.min_tick);
  LevelFIFO* b = &this->lv_.get_level(kBand.max_tick);
  LevelFIFO* c = &this->lv_.get_level(600);
  EXPECT_NE(a, b);
  EXPECT_NE(a, c);
  EXPECT_NE(b, c);
  EXPECT_EQ(a, &this->lv_.get_level(kBand.min_tick));
  EXPECT_EQ(b, &this->lv_.get_level(kBand.max_tick));
}

TYPED_TEST(PriceLevels, HasLevelFollowsHead) {
  EXPECT_FALSE(this->lv_.has_level(700));
  this->fill(700);
  EXPECT_TRUE(this->lv_.has_level(700));
  EXPECT_FALSE(this->lv_.has_level(699));
  EXPECT_FALSE(this->lv_.has_level(701));
  this->drain(700);
  EXPECT_FALSE(this->lv_.has_level(700));
}

TYPED_TEST(PriceLevels, BestRoundTripIncludingSentinelsAndBandEdges) {
  this->lv_.set_best_bid(kBand.min_tick);
  this->lv_.set_best_ask(kBand.max_tick);
  EXPECT_EQ(this->lv_.best_bid(), kBand.min_tick);
  EXPECT_EQ(this->lv_.best_ask(), kBand.max_tick);
  this->lv_.set_best_bid(kNoBid);
  this->lv_.set_best_ask(kNoAsk);
  EXPECT_EQ(this->lv_.best_bid(), kNoBid);
  EXPECT_EQ(this->lv_.best_ask(), kNoAsk);
}

TYPED_TEST(PriceLevels, FindBestAskAfterBestLevelEmpties) {
  for (Tick t : {205, 210, 400}) this->fill(t);
  this->lv_.set_best_ask(205);
  this->drain(205);
  EXPECT_EQ(this->lv_.find_best_ask(), 210);
  this->drain(210);
  this->lv_.set_best_ask(210);
  EXPECT_EQ(this->lv_.find_best_ask(), 400);
}

TYPED_TEST(PriceLevels, FindBestBidAfterBestLevelEmpties) {
  for (Tick t : {150, 300, 305}) this->fill(t);
  this->lv_.set_best_bid(305);
  this->drain(305);
  EXPECT_EQ(this->lv_.find_best_bid(), 300);
  this->drain(300);
  this->lv_.set_best_bid(300);
  EXPECT_EQ(this->lv_.find_best_bid(), 150);
}

TYPED_TEST(PriceLevels, FindBestOnEmptySideReturnsSentinel) {
  this->fill(500);
  this->lv_.set_best_ask(500);
  this->lv_.set_best_bid(500);
  this->drain(500);
  EXPECT_EQ(this->lv_.find_best_ask(), kNoAsk);
  EXPECT_EQ(this->lv_.find_best_bid(), kNoBid);
}

TYPED_TEST(PriceLevels, FindBestFromSentinelScansWholeBand) {
  this->fill(kBand.max_tick);
  this->fill(kBand.min_tick);
  EXPECT_EQ(this->lv_.find_best_ask(), kBand.min_tick);
  EXPECT_EQ(this->lv_.find_best_bid(), kBand.max_tick);
}

TYPED_TEST(PriceLevels, FindBestFindsLevelsAtBandEdges) {
  this->fill(kBand.max_tick);
  this->lv_.set_best_ask(kBand.max_tick - 5);
  EXPECT_EQ(this->lv_.find_best_ask(), kBand.max_tick);
  this->drain(kBand.max_tick);
  this->fill(kBand.min_tick);
  this->lv_.set_best_bid(kBand.min_tick + 5);
  EXPECT_EQ(this->lv_.find_best_bid(), kBand.min_tick);
}

// ------------------------------------------------------ contiguous only

TEST(PriceLevelContiguous, BestLevelPtrTracksSetBest) {
  PriceLevelContiguous lv(kBand);
  EXPECT_EQ(lv.best_level_ptr('B'), nullptr);
  EXPECT_EQ(lv.best_level_ptr('S'), nullptr);
  lv.set_best_bid(300);
  lv.set_best_ask(301);
  EXPECT_EQ(lv.best_level_ptr('B'), &lv.get_level(300));
  EXPECT_EQ(lv.best_level_ptr('S'), &lv.get_level(301));
  lv.set_best_bid(kNoBid);
  lv.set_best_ask(kNoAsk);
  EXPECT_EQ(lv.best_level_ptr('B'), nullptr);
  EXPECT_EQ(lv.best_level_ptr('S'), nullptr);
}

TEST(PriceLevelContiguous, LevelsAreContiguousInTickOrder) {
  PriceLevelContiguous lv(kBand);
  EXPECT_EQ(&lv.get_level(kBand.min_tick + 1), &lv.get_level(kBand.min_tick) + 1);
  EXPECT_EQ(&lv.get_level(kBand.max_tick), &lv.get_level(kBand.min_tick) + (kBand.max_tick - kBand.min_tick));
}

// --------------------------------------------------------- sparse only

TEST(PriceLevelsSparse, LevelReferencesSurviveRehash) {
  PriceLevelsSparse lv;
  LevelFIFO* p = &lv.get_level(42);
  for (Tick t = 1000; t < 101000; ++t) lv.get_level(t);
  EXPECT_EQ(p, &lv.get_level(42));
}

TEST(PriceLevelsSparse, AcceptsTicksOutsideAnyBand) {
  PriceLevelsSparse lv;
  lv.get_level(-2'000'000'000).head = 0;
  EXPECT_TRUE(lv.has_level(-2'000'000'000));
  EXPECT_FALSE(lv.has_level(2'000'000'000));
  EXPECT_EQ(lv.find_best_ask(), -2'000'000'000);
}

}  // namespace

