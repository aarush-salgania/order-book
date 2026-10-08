#include<bits/stdc++.h>

//atomic queue:
class atomicQueue {
  int q[10];
  std::atomic<int> front{0};   //atomic front is used to access non-atomic memory...
  public:
    void push(int x) {
      int my_slot = front.fetch_add(1);
      q[my_slot] = x;
    }
};

//lock-free/atomic list: 
struct node {
  int value;
  node* next;
};

class list {
  private:
    std::atomic<node*> head;   //the head pointer is atomic.
  public:
    void push_front(int x) {
      node* new_n = new node;
      new_n->value = x;   //the usual new node creation...

      node* old_h = head;  //save previous head...again as usual.

      do{new_n->next = head;}   //update the current head, whatever it is...
      while(!head.compare_exchange_strong(old_h, new_n));  //if some other thread has modified the head already, then use that as the current head. 
      //basically, don't do head=new_n until head==old_h...if head!=old_h, then set head=whatever the latest head is as updated by the other thread.
    }
};

//design a SPSC queue...use alignas, etc...release acquire.
// memory order and barriers to learn;

int main() {
  atomicQueue Q;
  Q.push(4);
  Q.push(3);
}