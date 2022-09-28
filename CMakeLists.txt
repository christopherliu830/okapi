cmake_minimum_required(VERSION 3.20)
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
find_package(imgui CONFIG REQUIRED)
find_package(entt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

add_executable(${PROJECT_NAME} 
  src/main.cpp
)

add_subdirectory(src/graphics)
add_subdirectory(src/gui)
add_subdirectory(src/primitives)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main)
target_link_libraries(${PROJECT_NAME} PRIVATE EnTT::EnTT)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Vulkan)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui::imgui)
target_include_directories(${PROJECT_NAME} PUBLIC include)
target_include_directories(${PROJECT_NAME} PUBLIC lib)
target_include_directories(${PROJECT_NAME} PUBLIC src)
target_include_directories(${PROJECT_NAME} PUBLIC ${STAGING_DIR}/include) # spdlog
include(CPack)

# compile shaders

file(GLOB_RECURSE GLSL_SOURCE_FILES
  "assets/shaders/*.frag"
  "assets/shaders/*.vert"
)

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  get_filename_component(DIRECTORY_NAME ${GLSL} DIRECTORY)
  set(SPIRV "${DIRECTORY_NAME}/${FILE_NAME}.spv")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND glslc ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL}
    COMMENT "Compiling shader ${GLSL}... ${SPIRV}"
  )
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
  Shaders
  DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(${PROJECT_NAME} Shaders)
# end compile shaders

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