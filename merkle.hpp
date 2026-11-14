// merkle.hpp - hashing, in-place Merkle root, and the worker thread that drains
// the ring buffer and emits one root per batch. Header-only.
#pragma once
#include <atomic>
#include <cstring>
#include <thread>
#include <type_traits>
#include <vector>
#include "ring_buffer.hpp"

struct Hash { uint64_t w[4]; };  // 256 bits
inline bool operator==(const Hash& a, const Hash& b) { return !std::memcmp(&a, &b, sizeof(Hash)); }

// ---- Hash function ---------------------------------------------------------
// FAST but NOT cryptographic. Swap the body of hash_words() for BLAKE3/SHA-256
// if roots must resist tampering; nothing else in this file needs to change.
inline uint64_t rotl(uint64_t x, int r) { return (x << r) | (x >> (64 - r)); }
inline uint64_t fmix(uint64_t x) {
  x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27; x *= 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}

// domain: 0 = leaf, 1 = internal node (prevents leaf/node confusion).
inline Hash hash_words(const uint64_t* d, size_t n, uint64_t domain) {
  constexpr uint64_t K1 = 0x9e3779b97f4a7c15ULL, K2 = 0xbf58476d1ce4e5b9ULL;
  uint64_t a = 0x243f6a8885a308d3ULL ^ domain, b = 0x13198a2e03707344ULL,
           c = 0xa4093822299f31d0ULL,          e = 0x082efa98ec4e6c89ULL;
  for (size_t i = 0; i < n; ++i) {  // 4 independent-ish lanes -> instruction-level parallelism
    a = rotl(a + d[i], 27) * K1;
    b = rotl(b ^ d[i], 31) * K2;
    c = rotl(c + d[i] * K2, 23) ^ a;
    e = rotl(e ^ d[i] * K1, 37) + b;
  }
  return {{fmix(a ^ n), fmix(b ^ a), fmix(c ^ b), fmix(e ^ c)}};
}

inline Hash hash_leaf(const ExecutionEvent& ev) {
  static_assert(std::is_trivially_copyable_v<ExecutionEvent> && sizeof(ExecutionEvent) == 7 * 8);
  uint64_t w[7];
  std::memcpy(w, &ev, sizeof w);
  return hash_words(w, 7, 0);
}

inline Hash hash_node(const Hash& l, const Hash& r) {
  uint64_t w[8];
  std::memcpy(w, &l, 32);
  std::memcpy(w + 4, &r, 32);
  return hash_words(w, 8, 1);
}

// ---- Merkle root -----------------------------------------------------------
// Reduces h[0..n) to a root IN PLACE (no allocation). h must have room for n+1
// entries (an odd level is padded with a zero hash). Overwrites h.
inline Hash merkle_root(Hash* h, size_t n) {
  if (n == 0) return Hash{};
  while (n > 1) {
    if (n & 1) h[n++] = Hash{};
    n /= 2;
    for (size_t i = 0; i < n; ++i) h[i] = hash_node(h[2 * i], h[2 * i + 1]);
  }
  return h[0];
}

// ---- Worker thread -----------------------------------------------------------
// Pops events, hashes them as leaves, and emits one root per `batch` events
// (plus one for the final partial batch on stop()).
class MerkleWorker {
 public:
  explicit MerkleWorker(RingBuffer& rb, size_t batch = 1024)
      : rb_(rb), batch_(batch), nodes_(batch + 1) {}
  ~MerkleWorker() { stop(); }

  void start() { thread_ = std::thread([this] { run(); }); }

  // Call after the producer has pushed its last event. Drains, then joins.
  void stop() {
    if (!thread_.joinable()) return;
    done_.store(true, std::memory_order_release);
    thread_.join();
  }

  // Safe to read only after stop().
  const std::vector<Hash>& roots() const { return roots_; }

 private:
  void flush() {
    roots_.push_back(merkle_root(nodes_.data(), n_));
    n_ = 0;
  }

  void run() {
    auto take = [this](const ExecutionEvent& e) {
      nodes_[n_++] = hash_leaf(e);
      if (n_ == batch_) flush();
    };
    for (;;) {
      const bool done = done_.load(std::memory_order_acquire);  // read BEFORE draining
      if (rb_.consume(take) == 0) {
        if (done) break;  // producer finished and queue is empty
        cpu_pause();
      }
    }
    if (n_) flush();
  }

  RingBuffer& rb_;
  const size_t batch_;
  std::vector<Hash> nodes_;  // reused scratch: leaves, reduced in place
  size_t n_ = 0;
  std::vector<Hash> roots_;
  std::atomic<bool> done_{false};
  std::thread thread_;
};