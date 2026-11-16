//ring_buffer.hpp - lock-free SPSC queue between the matching engine (producer)
//and the Merkle worker (consumer). Header-only.
#pragma once
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
  #include <immintrin.h>
  inline void cpu_pause() noexcept { _mm_pause(); }
#elif defined(__aarch64__) || defined(_M_ARM64)
  inline void cpu_pause() noexcept { asm volatile("yield" ::: "memory"); }
#else
  inline void cpu_pause() noexcept {}
#endif

// Always zero-initialise (`ExecutionEvent e{};`): the pad bytes get hashed,
// so garbage there would make Merkle roots non-deterministic.
struct ExecutionEvent {
  uint64_t ingress_id;
  uint64_t trade_id;
  uint64_t maker_order_id;
  uint64_t taker_order_id;
  uint64_t price;       // fixed-point
  uint64_t quantity;
  uint8_t  side;        // 0 = buy, 1 = sell
  uint8_t  event_type;  // 0 = add, 1 = cancel, 2 = trade
  uint8_t  pad[6];
};
static_assert(sizeof(ExecutionEvent) == 56);

class RingBuffer {
 public:
  explicit RingBuffer(size_t capacity)  // must be a power of 2
      : mask_(capacity - 1), buf_(new ExecutionEvent[capacity]) {
    assert(capacity && (capacity & (capacity - 1)) == 0);
  }

  // Producer thread only. Returns false if full (caller decides: spin / drop).
  bool push(const ExecutionEvent& e) noexcept {
    const size_t t = tail_.load(std::memory_order_relaxed);
    if (t - cached_head_ > mask_) {  // looks full -> refresh consumer position
      cached_head_ = head_.load(std::memory_order_acquire);
      if (t - cached_head_ > mask_) return false;
    }
    buf_[t & mask_] = e;
    tail_.store(t + 1, std::memory_order_release);
    return true;
  }

  // Consumer thread only. Calls fn(const ExecutionEvent&) on up to max_n queued
  // events, then releases them all with a single store. Returns how many.
  template <class F>
  size_t consume(F&& fn, size_t max_n = 256) {
    const size_t h = head_.load(std::memory_order_relaxed);
    const size_t t = tail_.load(std::memory_order_acquire);
    if (t == h) return 0;
    const size_t end = h + std::min(t - h, max_n);
    for (size_t i = h; i != end; ++i) fn(buf_[i & mask_]);
    head_.store(end, std::memory_order_release);
    return end - h;
  }

 private:
  const size_t mask_;
  std::unique_ptr<ExecutionEvent[]> buf_;
  // Each side's hot variables live on their own cache line (no false sharing).
  alignas(64) std::atomic<size_t> tail_{0};  // written by producer
  size_t cached_head_{0};                    // producer's stale copy of head_
  alignas(64) std::atomic<size_t> head_{0};  // written by consumer
};