#include "test_framework.hpp"
#include "core_shim.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

namespace {

struct alignas(64) CacheLineObj {
  std::uint64_t a[8];
};

using P = Pool<std::uint64_t>;

TEST(Pool, ReportsCapacityAndUsage) {
  P pool(10);
  EXPECT_EQ(pool.capacity(), 10u);
  EXPECT_EQ(pool.in_use(), 0u);
  Handle h = pool.alloc();
  EXPECT_EQ(pool.in_use(), 1u);
  pool.dealloc(h);
  EXPECT_EQ(pool.in_use(), 0u);
}

TEST(Pool, HandsOutEveryHandleOnceThenNil) {
  P pool(32);
  std::set<Handle> seen;
  for (int i = 0; i < 32; ++i) {
    Handle h = pool.alloc();
    ASSERT_NE(h, NIL);
    EXPECT_LT(h, 32u);
    EXPECT_TRUE(seen.insert(h).second) << "handle handed out twice";
  }
  EXPECT_EQ(pool.alloc(), NIL) << "must report exhaustion, not overrun";
  EXPECT_EQ(pool.in_use(), 32u);
}

TEST(Pool, FreshPoolAllocatesSequentially) {
  P pool(8);
  for (Handle i = 0; i < 8; ++i) {
    EXPECT_EQ(pool.alloc(), i);
  }
}

TEST(Pool, ReusesMostRecentlyFreedFirst) {
  // LIFO reuse keeps the hottest cache line at the top of the free list
  P pool(4);
  Handle a = pool.alloc();
  Handle b = pool.alloc();
  pool.dealloc(a);
  pool.dealloc(b);
  EXPECT_EQ(pool.alloc(), b);
  EXPECT_EQ(pool.alloc(), a);
}

TEST(Pool, StorageIsStableAndIndependent) {
  P pool(64);
  std::vector<Handle> hs;
  for (std::uint64_t i = 0; i < 64; ++i) {
    hs.push_back(pool.alloc());
    pool[hs.back()] = i * 0x0101010101010101ull;
  }
  for (std::uint64_t i = 0; i < 64; ++i) {
    EXPECT_EQ(pool[hs[i]], i * 0x0101010101010101ull);
  }
}

TEST(Pool, RespectsOverAlignedTypes) {
  Pool<CacheLineObj> pool(8);
  for (int i = 0; i < 8; ++i) {
    Handle h = pool.alloc();
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(&pool[h]) % alignof(CacheLineObj), 0u);
  }
}

TEST(Pool, PointerToHandleRoundTrips) {
  Pool<CacheLineObj> pool(16);
  for (int i = 0; i < 16; ++i) {
    Handle h = pool.alloc();
    EXPECT_EQ(pool.pointer_to_handle(&pool[h]), h);
  }
}

TEST(Pool, CapacityOfOne) {
  P pool(1);
  Handle h = pool.alloc();
  EXPECT_EQ(h, 0u);
  EXPECT_EQ(pool.alloc(), NIL);
  pool.dealloc(h);
  EXPECT_EQ(pool.alloc(), 0u);
}

TEST(Pool, LongRandomChurnNeverLosesASlot) {
  constexpr std::size_t N = 1024;
  P pool(N);
  std::vector<Handle> held;
  std::uint64_t s = 7;
  for (int step = 0; step < 200000; ++step) {
    s = s * 6364136223846793005ull + 1442695040888963407ull;
    const bool do_alloc = held.empty() || (held.size() < N && (s >> 63));
    if (do_alloc) {
      Handle h = pool.alloc();
      ASSERT_NE(h, NIL);
      held.push_back(h);
    } else {
      const std::size_t k = (s >> 20) % held.size();
      pool.dealloc(held[k]);
      held[k] = held.back();
      held.pop_back();
    }
    ASSERT_EQ(pool.in_use(), held.size());
  }
  while (held.size() < N) {
    Handle h = pool.alloc();
    ASSERT_NE(h, NIL) << "free list lost a slot";
    held.push_back(h);
  }
  EXPECT_EQ(pool.alloc(), NIL);
  std::sort(held.begin(), held.end());
  EXPECT_EQ(std::adjacent_find(held.begin(), held.end()), held.end()) << "duplicate handle";
}

}  // namespace

