cmake_minimum_required(VERSION 3.15...4.0)

project(lbp
	VERSION 0.1
	DESCRIPTION "Headers and tools for the LightBurn Protocol"
	LANGUAGES CXX)

set (CMAKE_CXX_STANDARD 17)

add_library(lbp STATIC
	src/beio.cpp
	src/checksum.cpp
	src/payload.cpp

	include/lbp/beio.h
	include/lbp/checksum.h
	include/lbp/message.h
	include/lbp/parser.h
	include/lbp/payload.h
	include/lbp/queue.h
	include/lbp/ringbuffer.h
	include/lbp/spec.h
)

target_include_directories(lbp PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)

add_executable(TestLBP test/main.cpp)

target_link_libraries(TestLBP PRIVATE lbp)

install(TARGETS lbp
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
)
install(DIRECTORY include/lbp
	DESTINATION include
)
