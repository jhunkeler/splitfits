cmake_minimum_required(VERSION 3.1...3.16)
project(splitfits C)
set(CMAKE_C_STANDARD 99)

find_package(Git)
if(GIT_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
	execute_process(COMMAND ${GIT_EXECUTABLE} describe --always --long --tags --dirty
		OUTPUT_VARIABLE GIT_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
	message(WARNING "git not found or directory is not a repository")
	set(GIT_VERSION, "unknown")
endif()

add_executable(splitfits splitfits.c)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/version.h" @ONLY)

file(GLOB TEST_RUNNER "test*.sh")
file(COPY ${TEST_RUNNER} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(GLOB TEST_FRAMEWORK "tests")
file(COPY ${TEST_FRAMEWORK} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})


add_custom_target(check "${CMAKE_CURRENT_BINARY_DIR}/test.sh")

install(TARGETS splitfits
	RUNTIME DESTINATION bin)
