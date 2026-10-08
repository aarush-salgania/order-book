#include <bits/stdc++.h>
using namespace std;


//the simple business logic - match a buyer to a seller based on the price.
//lets, for now, consider shares of a single entity only.
vector<pair<int,int>> asks;
vector<pair<int,int>> buys;    //stores {quantity, price};


int match_askorder(vector<pair<int,int>>& asks, vector<pair<int,int>>& buys, pair<int,int>& ask) {   //returns quantity unmatched
  int rem_quantity = ask.first;
  int price = ask.second;
  int i=0;
  while(rem_quantity) {
    if(buys.empty()) break;      //to do
    if(buys[i].second < price) break;
    if(rem_quantity - buys[i].first < 0) {
      rem_quantity=0;
      //update asks[i].first with unmatched quantity.
      break;
    }
    else {
      rem_quantity -= buys[i].first;
      //remove asks[i] from the book also;
    }
  }
  return rem_quantity;
}


int match_buyorder(vector<pair<int,int>>& asks, vector<pair<int,int>>& buys, pair<int,int>& buy) {
  int rem_quantity = buy.first;
  int price = buy.second;
  int i=0;
  while(rem_quantity) {
    if(asks.empty()) break;    //to do  
    if(asks[i].second > price) break;
    if(rem_quantity - asks[i].first < 0) {
      rem_quantity=0;
      //update asks[i].first with unmatched quantity.
      break;
    }
    else {
      rem_quantity -= asks[i].first;
      //remove asks[i] from the book also;
    }
  }
  return rem_quantity;
}


void add_ask(pair<int,int>& new_ask, vector<pair<int,int>>& asks) {
  int remaining_quantity = match_askorder(asks, buys, new_ask);
  if(remaining_quantity) {
    asks.push_back(new_ask);
  }
  sort(asks.begin(), asks.end());    //lowest ask first;
}


void add_buy(pair<int,int>& new_buy, vector<pair<int,int>>& buys) {
  int remaining_quantity = match_buyorder(asks, buys, new_buy);
  if(remaining_quantity) {
    buys.push_back(new_buy);
  }
}


int main() {
  
  return 0;
}


