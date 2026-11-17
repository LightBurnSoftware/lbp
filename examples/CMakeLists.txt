cmake_minimum_required(VERSION 3.15...4.0)

# Example code for firmware authors. Not part of the lbp library: nothing here is installed.
# Expects the lbp target to exist already (add ../lbp first).

project(lbp-examples LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)

add_library(lbpcor_example STATIC
	lbpcor.cpp
	lbpcor.h
)
target_include_directories(lbpcor_example PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(lbpcor_example PUBLIC lbp)

add_executable(TestLbpCor lbpcor_test.cpp)
target_link_libraries(TestLbpCor PRIVATE lbpcor_example)
