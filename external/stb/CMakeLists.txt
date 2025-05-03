cmake_minimum_required(VERSION 3.18)

project(stb)

file(GLOB SOURCES stb/*.h)

add_library(stb ${SOURCES} stb.cpp)

target_include_directories(stb PUBLIC stb/)