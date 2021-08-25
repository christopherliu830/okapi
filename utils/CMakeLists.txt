cmake_minimum_required(VERSION 3.0.0)
project(utils)

set(HEADERS
  vulkan_utils.hpp
)

set(SOURCES
  vulkan_utils.cpp
)

source_group(headers FILES ${HEADERS})
source_group(sources FILES ${SOURCES})

add_library(utils 
  ${SOURCES}
  ${HEADERS}
)

target_link_libraries(utils PUBLIC glm)
target_compile_definitions(utils PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1)