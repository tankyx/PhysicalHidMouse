cmake_minimum_required(VERSION 3.16)
project(physical_hid_mouse LANGUAGES CXX)

add_library(physical_hid_mouse INTERFACE)
target_include_directories(physical_hid_mouse INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(physical_hid_mouse INTERFACE cxx_std_17)

# Only build the self-check when this is the top-level project.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_executable(phm_selftest tests/selftest.cpp)
    target_link_libraries(phm_selftest PRIVATE physical_hid_mouse)
    add_test(NAME phm_selftest COMMAND phm_selftest)
endif()
