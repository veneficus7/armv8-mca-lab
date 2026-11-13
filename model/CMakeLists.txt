cmake_minimum_required(VERSION 3.20)
project(mca_check CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(mca-check src/parse.cpp src/model.cpp src/main.cpp)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(mca-check PRIVATE -Wall -Wextra -Wpedantic)
endif()
