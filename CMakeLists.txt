cmake_minimum_required(VERSION 3.24)
project(FaeScript)


add_executable(fae main.cpp compiler.cpp)
target_compile_features(fae PUBLIC cxx_std_20)

