cmake_minimum_required(VERSION 4.3)
project(order_book)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(order_book
        Core/order.cpp
        Core/main.cpp
        Core/types.cpp
        Core/order_book_core.cpp
        Core/mempool.cpp
        Core/test_suite.cpp
)
