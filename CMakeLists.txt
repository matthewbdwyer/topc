cmake_minimum_required(VERSION 3.15)

project(topc)

# Include CMake modules
include(FetchContent)

# Required packages
find_package(PkgConfig REQUIRED)
pkg_search_module(UUID REQUIRED uuid)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

# compiler must be 17
set(CMAKE_CXX_STANDARD 17)

# compile for debug if no other type is set
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug)
endif()

# Apple's system linker (ld) cannot parse DWARF 5, which LLVM clang
# generates by default. Force DWARF 4 to suppress the linker warnings.
if(APPLE)
  add_compile_options(-gdwarf-4)
endif()

# cleanup the code with clang-tidy set(CMAKE_CXX_CLANG_TIDY clang-tidy
# -checks=-*,readability-*)

# Code Coverage rules ###########
add_library(coverage_config INTERFACE)

option(CODE_COVERAGE "Enable coverage reporting." ON)
if(CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  message(STATUS "Building with coverage reporting enabled")
  # Add required flags (GCC & LLVM/Clang)
  target_compile_options(
    coverage_config
    INTERFACE -O0 # no optimization
              -g # generate debug info
              --coverage # sets all required flags
  )
  target_link_options(coverage_config INTERFACE --coverage)
endif()

# ANTLR4 CPP target rules ###########

# Locate Java via find_program so any JDK 11+ install works.
find_program(JAVA_EXECUTABLE NAMES java)
if(NOT JAVA_EXECUTABLE)
  message(FATAL_ERROR
    "Java not found. Install a JDK 11+ or set JAVA_HOME manually.")
endif()
get_filename_component(JAVA_BIN_DIR "${JAVA_EXECUTABLE}" DIRECTORY)
get_filename_component(JAVA_HOME "${JAVA_BIN_DIR}/.." ABSOLUTE)

set(ANTLR4_WITH_STATIC_CRT OFF)
set(ANTLR_TAG 4.13.1)
set(DISABLE_WARNINGS ON)
set(ANTLR_BUILD_CPP_TESTS OFF)
set(ANTLR_BUILD_SHARED OFF)

set(ANTLR_EXECUTABLE "${CMAKE_BINARY_DIR}/include/antlr-${ANTLR_TAG}-complete.jar")
if(NOT EXISTS "${ANTLR_EXECUTABLE}")
  file(
          DOWNLOAD
          "https://www.antlr.org/download/antlr-${ANTLR_TAG}-complete.jar"
          "${ANTLR_EXECUTABLE}"
  )
endif()

FetchContent_Declare(
        antlr
        GIT_REPOSITORY https://github.com/antlr/antlr4
        GIT_TAG ${ANTLR_TAG}
        SOURCE_SUBDIR "runtime/Cpp"
)

FetchContent_MakeAvailable(antlr)
include(${antlr_SOURCE_DIR}/runtime/Cpp/cmake/FindANTLR.cmake)
include_directories(${antlr_SOURCE_DIR}/runtime/Cpp/runtime/src)

# LLVM rules for installed libraries ########### We want to select the latest
# stable release even if others are installed
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(LLVM REQUIRED CONFIG)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
  find_package(LLVM REQUIRED CONFIG)
else()
  message(FATAL_ERROR "${CMAKE_SYSTEM_NAME} is not supported")
endif()

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# TIP relies on opaque pointers and APIs from newer LLVM releases.
if(LLVM_PACKAGE_VERSION VERSION_LESS "22.0.0")
  message(FATAL_ERROR "LLVM 22+ is required, found ${LLVM_PACKAGE_VERSION}")
endif()

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Logging.
add_library(loguru)
target_sources(loguru PUBLIC ${CMAKE_SOURCE_DIR}/externals/loguru/loguru.cpp
                             ${CMAKE_SOURCE_DIR}/externals/loguru/loguru.hpp)
find_package(Threads REQUIRED)
target_link_libraries(loguru ${CMAKE_THREAD_LIBS_INIT})
if(NOT WIN32)
  target_link_libraries(loguru dl)
endif()
add_definitions(-DLOGURU_WITH_STREAMS=1 -DLOGURU_VERBOSE_SCOPE_ENDINGS=0)
include_directories(${CMAKE_SOURCE_DIR}/externals/loguru)

# build target including unit tests ###########
add_subdirectory(src)


FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.3.2)

FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")

option(BUILD_TESTING "Build the testing tree." ON)
# Only build tests if we are the top-level project Allows this to be used by
# super projects with `add_subdirectory`
if(BUILD_TESTING AND (PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR))
  enable_testing()
  add_subdirectory(test)
endif()
