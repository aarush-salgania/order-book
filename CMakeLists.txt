cmake_minimum_required(VERSION 3.20)
project(order_book)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(.)

add_executable(order_book
        Core/order.cpp
        Core/main.cpp
        Core/types.cpp
        Core/order_book_core.cpp
        Core/mempool.cpp
        Tests/test_suite.cpp
)
# Interactive engine used by the Node backend (reads orders from stdin)
add_executable(order_book_engine Core/engine_a_main.cpp)
if (MINGW)
    target_link_options(order_book_engine PRIVATE -static)
endif()
set_target_properties(order_book_engine PROPERTIES
    OUTPUT_NAME order_book
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/backend)

# Asynchronous Verifiable Merkle Logger Child Process
add_executable(merkle_logger Core/merkle_logger_main.cpp)
if (MINGW)
    target_link_options(merkle_logger PRIVATE -static)
endif()
set_target_properties(merkle_logger PROPERTIES
    OUTPUT_NAME merkle_logger
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/backend)
    