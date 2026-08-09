cmake_minimum_required(VERSION 3.14)

project(tee_render LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_library(tee_render STATIC
  src/tee_anim.cpp
  src/tee_renderer.cpp
  src/tee_emoticon.cpp
)

target_include_directories(tee_render PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

option(TEE_RENDER_BUILD_EXAMPLE "Build the example application" ON)
if(TEE_RENDER_BUILD_EXAMPLE)
  add_subdirectory(example)
endif()
