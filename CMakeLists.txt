cmake_minimum_required(VERSION 3.14)
project(HuffmanCompression VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(huffcore
  src/BitIO.cpp
  src/HuffmanTree.cpp
  src/HuffmanCoder.cpp)
target_include_directories(huffcore PUBLIC include)
if(MSVC)
  target_compile_options(huffcore PRIVATE /W4)
else()
  target_compile_options(huffcore PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(huffman src/main.cpp)
target_link_libraries(huffman PRIVATE huffcore)

add_executable(gen_samples tools/gen_samples.cpp)

enable_testing()
add_executable(tests tests/test_huffman.cpp)
target_link_libraries(tests PRIVATE huffcore)
add_test(NAME huffman_tests COMMAND tests)
