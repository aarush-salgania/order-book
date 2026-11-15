cmake_minimum_required(VERSION 3.20)
project(tests_and_benchmarks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(OB_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(OB_NATIVE "Compile with -march=native" OFF)
option(OB_USE_EXTERNAL_LIBS "Fetch/Use external GoogleTest and Google Benchmark libraries" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Common include paths directly into the project's core/ and Tests/
add_library(ob_common INTERFACE)
target_include_directories(ob_common INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/harness
  ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks
  ${CMAKE_CURRENT_SOURCE_DIR}/../Core
  ${CMAKE_CURRENT_SOURCE_DIR}/../core
  ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(ob_common INTERFACE
    -Wall -Wextra -Wno-unused-parameter -Wno-pragma-once-outside-header
  )
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(ob_common INTERFACE -Wno-unknown-pragmas)
  else()
    target_compile_options(ob_common INTERFACE -Wno-unknown-warning-option)
  endif()
  if(OB_NATIVE)
    target_compile_options(ob_common INTERFACE -march=native)
  endif()
  if(OB_SANITIZE)
    target_compile_options(ob_common INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(ob_common INTERFACE -fsanitize=address,undefined)
  endif()
endif()

# ---------------------------------------------------------------- Tests
if(OB_USE_EXTERNAL_LIBS)
  include(FetchContent)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.17.0
    GIT_SHALLOW    TRUE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
  target_compile_definitions(ob_common INTERFACE OB_USE_EXTERNAL_GTEST)
endif()

add_executable(ob_tests
  tests/main_tests.cpp
  tests/test_mempool.cpp
  tests/test_price_levels.cpp
  tests/test_book.cpp
  tests/test_user_suite.cpp
)
target_link_libraries(ob_tests PRIVATE ob_common)
if(OB_USE_EXTERNAL_LIBS)
  target_link_libraries(ob_tests PRIVATE GTest::gtest)
endif()

enable_testing()
add_test(NAME ob_tests COMMAND ob_tests)

# ---------------------------------------------------------------- Benchmarks
if(OB_USE_EXTERNAL_LIBS)
  include(FetchContent)
  FetchContent_Declare(benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG        v1.10.0
    GIT_SHALLOW    TRUE)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(benchmark)
  target_compile_definitions(ob_common INTERFACE OB_USE_EXTERNAL_GBENCH)
endif()

add_executable(ob_bench
  benchmarks/main_bench.cpp
  benchmarks/bench_mempool.cpp
  benchmarks/bench_price_levels.cpp
  benchmarks/bench_book.cpp
)
target_link_libraries(ob_bench PRIVATE ob_common)
if(OB_USE_EXTERNAL_LIBS)
  target_link_libraries(ob_bench PRIVATE benchmark::benchmark)
endif()

# ---------------------------------------------------------------- Latency
add_executable(ob_latency
  benchmarks/latency.cpp
)
target_link_libraries(ob_latency PRIVATE ob_common)
