#pragma once
#include <cstddef>
#include <memory>
#include <iostream>

//basically, instead of repeatedly doing new/delete for individual objects (cuz that is slow), it allocates one big block and
//manages fixed sized slots itself...using the heap allocator requires expensive syscalls, and hence we do that only once,
//and then work within that pool using custom allocs/deallocs.

template<class T>
class Pool {
  
  union Slot {     //a slot is either a free-list link or contains data.
    Slot* next;
    alignas(T) unsigned char data[sizeof(T)];  //well, data is not an array really, but just contains a T.
    //note: int data[32] would give space for 32 actual int elements, while the above line allocates 32 raw bytes (assuming, sizeof(T)=32).
  };

  //remember, Pool is an array of Slots...and slots are of size T.
  //read more about unique_ptr...for now, its just a smart pointer that automatically frees the memory;
  std::unique_ptr<Slot[]> slots_;    //slots_ points to the start of a Slot array (i.e, the first Slot of the Pool)
  Slot* free_ = nullptr;             //head of the free list...initialized to nullptr.

  //so, we have two lists running in parallel inside the same pool...
  //one free slot stores the address to the next free node, but a filled slot contains only data and no 'next' pointer, unlike a linked-list.


  public:

    //explicit is just for safety (idk, some type-conversion shit);
    //essentially, the constructor below is like Pool(int n) {make the array of slots_ of size n and initially all of it is free.}
    
    explicit Pool(std::size_t n) : slots_(new Slot[n]) {   //initializer list...
      for (std::size_t i = 0; i < n; ++i) {   // chain every slot into the free list
        slots_[i].next = free_;  
        free_ = &slots_[i]; 
      }  //simple...we get smth like: `free_ -> slot[n-1] -> ... -> slot[1] -> slot[0] -> nullptr`
    }
 

    //the below lines disable copying of Pool
    Pool(const Pool&) = delete;  //dont allow Pool p1 = p2;  (copy constructor)
    Pool& operator=(const Pool&) = delete;   //dont allow p2 = p1;  (copy assignment)
    //=delete is used to tell the compiler that the action is forbidden;


    //now, how do we actually put data into this free list?
    //for that, we first need to take out a free slot from the free-list and mark it as not free...then we put data in it.
    //we define a function alloc() of return type of a pointer to T - the data type stored in slots.
    T* alloc() {        
      if (!free_) return nullptr;    //free_ -> nullptr
      Slot* s = free_;    //right now: free_,s-> slot[n-1] -> ... -> slot[1] -> slot[0] -> nullptr
      free_ = s->next;    //now: s->slot[n-1] and free_ -> slot[n-2] ... -> slot[0] -> nullptr   ... so we have popped the head.
      return reinterpret_cast<T*>(s);   //this just SOMEHOW converts a Slot* into a T* (idk how exactly, but we need a T* return type so we use this.)
    }


    //reverse of above operation...
    void dealloc(T* p) {        // push back onto the head
      Slot* s = reinterpret_cast<Slot*>(p);
      s->next = free_;
      free_ = s;
    }

    //both alloc() and dealloc() are O(1) since they occur at the head.

};



/*
the lifecycle:
pool.alloc() -> raw memory -> new (p) Person(...) -> Person exists -> use Person -> 
p->~Person() -> Person destroyed -> pool.dealloc(p) -> memory slot becomes available again
*/

class Person {
public:
  int age;

  Person(int age) : age(age) {
    std::cout << "Person(" << this->age << ") constructed\n";
  }

  ~Person() {
    std::cout << "Person(" << this->age << ") destroyed\n";
  }
};


int main() {

  Pool<Person> pool(3);   //so T is Person here.

  //Allocate raw memory
  Person* p1 = pool.alloc();
  Person* p2 = pool.alloc();
  Person* p3 = pool.alloc();

  //construct Person objects in that memory - this is called "placement new":
  new (p1) Person(18);  //we already have memory locations p1,p2,p3 from the above step;
  //but they dont store anything...now using the above line, i construct an object Person(18) at the memory location p1;
  new (p2) Person(20);
  new (p3) Person(25);

  std::cout << "\nAges:\n";
  std::cout << p1->age << '\n';
  std::cout << p2->age << '\n';
  std::cout << p3->age << '\n';

  // Pool is now full
  Person* p4 = pool.alloc();

  std::cout << "\np4 = " << p4 << '\n';

  // Destroy p2, then return its slot to the pool
  p2->~Person();
  pool.dealloc(p2);

  // This should reuse p2's slot
  Person* p5 = pool.alloc();

  std::cout << "p2 = " << p2 << '\n';
  std::cout << "p5 = " << p5 << '\n';

  new (p5) Person(30);

  // Clean up
  p1->~Person();
  p3->~Person();
  p5->~Person();

  pool.dealloc(p1);
  pool.dealloc(p3);
  pool.dealloc(p5);

  return 0;
}