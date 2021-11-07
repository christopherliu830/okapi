
target_sources(${PROJECT_NAME} PRIVATE
  graphics.cpp
  graphics.h
  mesh.cpp
  mesh.h
  pipeline.cpp
  pipeline.h
  render_system.h
  render_system.cpp
  renderable.h
  types.h
  util.h
  vulkan.h
)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})