cmake_minimum_required(VERSION 3.20)
project(nicx VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-g -O0)
else()
  add_compile_options(-O2)
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/cmake/version.hpp.in
  ${CMAKE_BINARY_DIR}/generated/nicx/version.hpp
)

file(GLOB_RECURSE SOURCES
  src/core/*.cpp
  src/output/*.cpp
  src/system/*.cpp
  src/commands/*.cpp
  src/pkg/*.cpp
  src/pkg/managers/*.cpp
)

add_executable(nicx src/main.cpp ${SOURCES})

target_include_directories(nicx PRIVATE
  include
  ${CMAKE_BINARY_DIR}/generated
)

install(TARGETS nicx DESTINATION bin)
install(FILES docs/nicx.1 DESTINATION share/man/man1)
