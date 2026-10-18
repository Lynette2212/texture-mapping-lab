add_executable(viewer3d main.cpp)

target_link_libraries(viewer3d PRIVATE viewer_core Qt6::Widgets)

viewer3d_set_output(viewer3d)
add_dependencies(viewer3d viewer3d_runtime)
viewer3d_deploy(viewer3d)
