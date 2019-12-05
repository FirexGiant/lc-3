cmake_minimum_required(VERSION 3.6)

project(lc-3 LANGUAGES CXX)

add_executable(lc3 src/lc3.cpp)
target_include_directories(lc3 PRIVATE include)

add_executable(lc3al src/lc3al.cpp)
target_include_directories(lc3al PRIVATE include)
