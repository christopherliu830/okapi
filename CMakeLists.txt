cmake_minimum_required(VERSION 3.20)
project(okapi VERSION 0.1.0)

set(CXX_STANDARD C++20)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(STAGING_DIR ${PROJECT_SOURCE_DIR}/staging)

add_executable(${PROJECT_NAME} 
  src/main.cpp
)
add_subdirectory(src)

target_include_directories(${PROJECT_NAME} PUBLIC include)

include(CPack)

# link to build
add_custom_command(
  TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/assets"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
  COMMENT "Copying assets..."
)

string(LENGTH "${CMAKE_SOURCE_DIR}/" ROOT_PATH_SIZE)
add_compile_definitions(ROOT_PATH_SIZE=${ROOT_PATH_SIZE})
add_compile_definitions(NOMINMAX)