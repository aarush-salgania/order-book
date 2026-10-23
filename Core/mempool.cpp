#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include "types.cpp"

template<class T>
class Pool {

  union Slot {
    Handle next;   //now, next is not a pointer/memory address, but an integer - an index.
    alignas(T) unsigned char data[sizeof(T)];
  };
  std::unique_ptr<Slot[]> slots_;  //slots_ is an array of Slot data type...special thing is the slots_[0] ptr - its a smart pointer!
  Handle free_ = NIL;  //initially, free_ points to nullptr OR here, free_ = NIL (an invalid location, meaning no free slots)
  std::size_t cap_ = 0;  //capacity
  std::size_t in_use_ = 0;

public:
  //create the array of Slots:
  explicit Pool(std::size_t n) : slots_(new Slot[n]), cap_(n) {  //initialize cap_ to n (the pool size).
    assert(n > 0 && n < NIL);  //for runtime debugging - ignore this, for now.
    //building the initial free-list:
    for (std::size_t i = 0; i < n; ++i) {
      slots_[i].next = Handle(i + 1);  //Handle(i+1) is just doing i+1 and storing it as a uint32;
    } //so slots_[0] points to slots_[1] and so on...
    slots_[n - 1].next = NIL;  //cuz slots_[n-1] must not point to slots_[n], but NIL
    free_ = 0;  //so after linking the slots, the head points to slots_[0]...see the alloc() below
  }

  Pool(const Pool&) = delete;
  Pool& operator=(const Pool&) = delete;

  Handle pointer_to_handle(const T* ptr) const noexcept {
    const auto* slot_ptr = reinterpret_cast<const Slot*>(ptr);
    std::ptrdiff_t diff = slot_ptr - slots_.get();
    assert(diff >= 0 && static_cast<std::size_t>(diff) < cap_);
    return static_cast<Handle>(diff);
  }

  Handle alloc() noexcept {  //noexcept guarantees that this function wont throw - again, reduces latency.
    if (free_ == NIL) return NIL;   //no slot is free
    Handle h = free_;   //h=0 effectively
    free_ = slots_[h].next;  //pop the head
    ++in_use_;
    return h;  //return the new head.
  }

  void dealloc(Handle h) noexcept {
    assert(h < cap_);
    slots_[h].next = free_;
    free_ = h;
    --in_use_;
  }

  //the below line is the bridge between index and data:
  T& operator[](Handle h) noexcept {  //means use the operator[] to return T* given h...basically, if i write pool[h], it will give me the data inside it!!!
    return *reinterpret_cast<T*>(slots_[h].data);
  }  //notice we are dereferencing the reinterpret_cast...cuz we want the data...the return type is also by reference...
  //so, for example, we write pool[0].price = 100.50, then it is modifying that slot_[0]'s data directly without any overheads!

  std::size_t in_use()   const noexcept { return in_use_; }
  std::size_t capacity() const noexcept { return cap_; }
};