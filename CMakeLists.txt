cmake_minimum_required(VERSION 3.13)
project(okapi VERSION 0.1.0)

set(CXX_STANDARD C++17)

include(CTest)
enable_testing()

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(STAGING_DIR ${PROJECT_SOURCE_DIR}/staging)

find_package(SDL2 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(glm CONFIG REQUIRED)

# spdlog dependency
include(ExternalProject)
ExternalProject_Add(spdlog
    PREFIX spdlog
    SOURCE_DIR ${PROJECT_SOURCE_DIR}/lib/spdlog
    CMAKE_ARGS -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
    -DCMAKE_INSTALL_PREFIX=${STAGING_DIR}
    -DSPDLOG_BUILD_SHARED=OFF
)

add_executable(${PROJECT_NAME} 
  src/main.cpp
)

add_subdirectory(lib/entt)

add_subdirectory(src/graphics)

add_dependencies(${PROJECT_NAME} spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main)
target_link_libraries(${PROJECT_NAME} PRIVATE EnTT::EnTT)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PUBLIC lib)
target_include_directories(${PROJECT_NAME} PUBLIC src)
target_include_directories(${PROJECT_NAME} PUBLIC ${STAGING_DIR}/include) # spdlog
include(CPack)

# link to build
add_custom_command(
  TARGET okapi POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${PROJECT_SOURCE_DIR}/assets"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
  COMMENT "Copying assets..."
)

string(LENGTH "${CMAKE_SOURCE_DIR}/" ROOT_PATH_SIZE)
add_compile_definitions(ROOT_PATH_SIZE=${ROOT_PATH_SIZE})
add_compile_definitions(NOMINMAX)