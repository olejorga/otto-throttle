project(otto_throttle CXX)
cmake_minimum_required(VERSION 3.2)

add_subdirectory(xplane_sdk bin/)
add_xplane_plugin(otto_throttle 301 otto_throttle.cpp)