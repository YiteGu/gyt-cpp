cmake_minimum_required(VERSION 2.8)

project(messenger)

set(CMAKE_CXX_FLAGS -std=c++11)

add_executable(messenger
  ibv_test.cc
  Infiniband.cc
)

target_link_libraries(messenger -libverbs)
