cmake_minimum_required(VERSION 4.3)
project(order_book)

set(CMAKE_CXX_STANDARD 14)

include_directories(..)

add_executable(order_book
        order.cpp
        main.cpp
        types.cpp
        order_book_core.cpp
        mempool.cpp
)
