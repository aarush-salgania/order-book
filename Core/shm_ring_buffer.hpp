// shm_ring_buffer.hpp - Lock-free SPSC queue mapped into OS Shared Memory
// Enables zero-syscall IPC between the matching engine process and the Merkle logger child process.
// Falls back seamlessly to in-process ring buffer or pipe if shared memory is unavailable.
#pragma once

#include "ring_buffer.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

namespace ob {

constexpr const wchar_t* kDefaultShmName = L"Local\\OrderBook_MerkleRingBuffer";
constexpr size_t kDefaultShmCapacity = 65536; // 64K events (Power of 2, ~3.6 MB RAM)
constexpr uint32_t kShmMagic = 0x4D45524B;    // 'MERK'

struct alignas(64) ShmHeader {
  uint32_t magic;
  uint32_t capacity;
  uint32_t mask;
  uint32_t version;
  alignas(64) std::atomic<size_t> tail{0};
  size_t cached_head{0};
  alignas(64) std::atomic<size_t> head{0};
  alignas(64) std::atomic<bool> producer_active{false};
  std::atomic<bool> consumer_active{false};
  std::atomic<bool> shutdown_requested{false};
};

inline size_t compute_shm_size(size_t capacity) {
  return sizeof(ShmHeader) + capacity * sizeof(ExecutionEvent);
}

// Producer interface (used by Core Matching Engine)
class ShmProducer {
 public:
  explicit ShmProducer(const wchar_t* name = kDefaultShmName, size_t capacity = kDefaultShmCapacity)
      : capacity_(capacity), mask_(capacity - 1) {
    assert(capacity && (capacity & (capacity - 1)) == 0);
#ifdef _WIN32
    const size_t total_size = compute_shm_size(capacity_);
    hMap_ = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                               static_cast<DWORD>(total_size), name);
    if (hMap_ != NULL) {
      void* ptr = MapViewOfFile(hMap_, FILE_MAP_ALL_ACCESS, 0, 0, total_size);
      if (ptr != nullptr) {
        header_ = static_cast<ShmHeader*>(ptr);
        header_->magic = kShmMagic;
        header_->capacity = static_cast<uint32_t>(capacity_);
        header_->mask = static_cast<uint32_t>(mask_);
        header_->version = 1;
        header_->tail.store(0, std::memory_order_relaxed);
        header_->head.store(0, std::memory_order_relaxed);
        header_->cached_head = 0;
        header_->producer_active.store(true, std::memory_order_release);
        header_->shutdown_requested.store(false, std::memory_order_release);

        events_ = reinterpret_cast<ExecutionEvent*>(static_cast<char*>(ptr) + sizeof(ShmHeader));
        is_shm_ = true;
        std::cerr << "[shm] Producer attached to shared memory ring buffer (" 
                  << (total_size / 1024) << " KB)" << std::endl;
        return;
      }
    }
#endif
    std::cerr << "[shm] Shared memory unavailable, using in-process fallback" << std::endl;
    fallback_ring_ = std::make_unique<RingBuffer>(capacity_);
    is_shm_ = false;
  }

  ~ShmProducer() {
#ifdef _WIN32
    if (header_) {
      header_->producer_active.store(false, std::memory_order_release);
      UnmapViewOfFile(header_);
    }
    if (hMap_) {
      CloseHandle(hMap_);
    }
#endif
  }

  bool is_shared_memory() const noexcept { return is_shm_; }

  bool push(const ExecutionEvent& e) noexcept {
    if (is_shm_ && header_) {
      const size_t t = header_->tail.load(std::memory_order_relaxed);
      if (t - header_->cached_head > mask_) {
        header_->cached_head = header_->head.load(std::memory_order_acquire);
        if (t - header_->cached_head > mask_) return false;
      }
      events_[t & mask_] = e;
      header_->tail.store(t + 1, std::memory_order_release);
      return true;
    } else if (fallback_ring_) {
      return fallback_ring_->push(e);
    }
    return false;
  }

  void signal_shutdown() noexcept {
    if (is_shm_ && header_) {
      header_->shutdown_requested.store(true, std::memory_order_release);
    }
  }

 private:
  size_t capacity_;
  size_t mask_;
  bool is_shm_{false};
  ShmHeader* header_{nullptr};
  ExecutionEvent* events_{nullptr};
  std::unique_ptr<RingBuffer> fallback_ring_{nullptr};
#ifdef _WIN32
  HANDLE hMap_{NULL};
#endif
};

// Consumer interface (used by Merkle Logger Child Process)
class ShmConsumer {
 public:
  explicit ShmConsumer(const wchar_t* name = kDefaultShmName, size_t capacity = kDefaultShmCapacity)
      : capacity_(capacity), mask_(capacity - 1) {
#ifdef _WIN32
    hMap_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (hMap_ != NULL) {
      const size_t total_size = compute_shm_size(capacity_);
      void* ptr = MapViewOfFile(hMap_, FILE_MAP_ALL_ACCESS, 0, 0, total_size);
      if (ptr != nullptr) {
        header_ = static_cast<ShmHeader*>(ptr);
        if (header_->magic == kShmMagic) {
          events_ = reinterpret_cast<ExecutionEvent*>(static_cast<char*>(ptr) + sizeof(ShmHeader));
          header_->consumer_active.store(true, std::memory_order_release);
          is_shm_ = true;
          std::cerr << "[shm] Consumer connected to shared memory ring buffer" << std::endl;
          return;
        }
        UnmapViewOfFile(ptr);
      }
      CloseHandle(hMap_);
      hMap_ = NULL;
    }
#endif
    std::cerr << "[shm] Shared memory not open yet, waiting or running pipe fallback" << std::endl;
    is_shm_ = false;
  }

  ~ShmConsumer() {
#ifdef _WIN32
    if (header_) {
      header_->consumer_active.store(false, std::memory_order_release);
      UnmapViewOfFile(header_);
    }
    if (hMap_) {
      CloseHandle(hMap_);
    }
#endif
  }

  bool is_shared_memory() const noexcept { return is_shm_; }

  bool try_connect(const wchar_t* name = kDefaultShmName) {
    if (is_shm_) return true;
#ifdef _WIN32
    hMap_ = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, name);
    if (hMap_ != NULL) {
      const size_t total_size = compute_shm_size(capacity_);
      void* ptr = MapViewOfFile(hMap_, FILE_MAP_ALL_ACCESS, 0, 0, total_size);
      if (ptr != nullptr) {
        header_ = static_cast<ShmHeader*>(ptr);
        if (header_->magic == kShmMagic) {
          events_ = reinterpret_cast<ExecutionEvent*>(static_cast<char*>(ptr) + sizeof(ShmHeader));
          header_->consumer_active.store(true, std::memory_order_release);
          is_shm_ = true;
          std::cerr << "[shm] Consumer connected to shared memory ring buffer" << std::endl;
          return true;
        }
        UnmapViewOfFile(ptr);
      }
      CloseHandle(hMap_);
      hMap_ = NULL;
    }
#endif
    return false;
  }

  bool is_shutdown_requested() const noexcept {
    if (is_shm_ && header_) {
      return header_->shutdown_requested.load(std::memory_order_acquire);
    }
    return false;
  }

  template <class F>
  size_t consume(F&& fn, size_t max_n = 256) {
    if (is_shm_ && header_) {
      const size_t h = header_->head.load(std::memory_order_relaxed);
      const size_t t = header_->tail.load(std::memory_order_acquire);
      if (t == h) return 0;
      const size_t end = h + std::min(t - h, max_n);
      for (size_t i = h; i != end; ++i) fn(events_[i & mask_]);
      header_->head.store(end, std::memory_order_release);
      return end - h;
    }
    return 0;
  }

 private:
  size_t capacity_;
  size_t mask_;
  bool is_shm_{false};
  ShmHeader* header_{nullptr};
  ExecutionEvent* events_{nullptr};
#ifdef _WIN32
  HANDLE hMap_{NULL};
#endif
};

} // namespace ob
