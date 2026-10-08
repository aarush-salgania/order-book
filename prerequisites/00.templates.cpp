#include<bits/stdc++.h>


//templates help making functions or classes GENERIC - more abstraction - more easy to plug-in.

template<typename T>   //writing this before the return type makes the function work for any type T...you could write class rather than typename... 
void print(T value) {   //T is just a placeholder...the same T is the type used inside the functions as well.
  std::cout << value << std::endl;
}
//the above code doesnt 'materialise' until its called...


//a template with two types - cuz the function can work with two different types:
template<typename T, typename U>   //we need two different types cuz we might be comparing a float and an int!!!
auto printmax(T a, U b) {    //return type 'auto' cuz we dont know whether its T or U.
  return (a > b) ? a : b;
}



//a template can also generate a class - note, i said a class, not an object!!

//suppose i have this class:
// class Box{
//   int value;
//   public:
//     Box(int value) {this->value = value;}
//     int getValue {
//       return value;
//     }
// };

//how to make the above work for double, float, etc?
template<typename T>    //just replace int with T in the class definition!!
class Box{
  T value;
  public:
    Box(T value) {this->value = value;}
    T getValue() {
      return value;
    }
}; //now i can just write Box<int> box1; or Box<double> box2; to create objects!!!
//notice this is just like STL containers - vector<int>, vector<float>, queue<int>, etc...


//a pair with two different datatypes:  so i want smth like: Pair<int,float> mypair;
template<typename T, typename U>
class Pair{
  public:
    T first;   //made these public so i can directly access them without member functions.
    U second;
    Pair(T first, U second) {
      this->first = first;
      this->second = second;
    }

    // T getfirst() {return this->first;}    //can i not use the member function names as first() and second() ?
    // U getsecond() {return this->second;}
};



//you can pass values to a template!!!
template<typename T, int n>   
class Array {
  private:
    T arr[n];  //so, we need an array of type T and the size is to be known at compile time, but we don't have that yet...
  
    public:
    void set(int index, T value) {
      if(index<0 || index>=n)
        throw std::out_of_range("Index out of range!!!");
      arr[index] = value;
    }

    T get(int index) {
      if(index<0 || index>=n)
        throw std::out_of_range("Index out of range!!!");
      return arr[index];
    }
};
//the above code will create a new class with the specified n...so, like we have a dynamic array using fixed length c-style arrays!!

//TO READ: specialized templates...like template<> 


//variadic templates:
template<typename... Args>
void printmany(Args... args) {
  ((std::cout << args << '\n'), ...);    //notice the syntax!!  
}


// //some other uses of templates:
// struct Lion {
//   std::string name;
//   std::string food;
// };

// struct Cow {
//   std::string name;
//   std::string food;
// };

// // template<class T>
// // void feedAnimal(T animalName) {   //T is 'Lion' or 'Cow'
// //   cout << "animal "
// // }


int main() {
  print(5);   //the template generates the function for an int
  print(5.5f);  //the template generates the function for a float...the function push is not even generated unless its called.
  print<std::string>("hello");    //you can specify the type explicitly...but cpp can auto-detect too.


  Pair<int, std::string> p(10, "Parth");
  std::cout << p.first << " " << p.second << std::endl;
  
  Array<int,10> nums;
  nums.set(0,2);
  nums.set(3,7);
  nums.set(9,10);
  std::cout << nums.get(3) << '\n';
  std::cout << nums.get(0) << '\n';
  // std::cout << nums.get(10) << '\n';  //throws exception and terminates the code here - the code above it runs as usual...
  std::cout << nums.get(1) << '\n';  //filled with garbage value;


  printmany(10);
  printmany("studytable", 20, 30.5f);
  

}