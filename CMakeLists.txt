cmake_minimum_required(VERSION 3.1...3.16)
project(splitfits C)

set(CMAKE_C_STANDARD 99)

add_executable(splitfits splitfits.c)

file(GLOB TEST_FRAMEWORK "tests")
file(COPY "test.sh" DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${TEST_FRAMEWORK} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(check "${CMAKE_CURRENT_BINARY_DIR}/test.sh")

install(TARGETS splitfits
	RUNTIME DESTINATION bin)
