# 场景库：任务1走廊、任务2环境映射两个可复用的 OpenGL 控件。
# 被 demos/ 下的独立演示程序和 plugins/ 下的插件共同链接。
add_library(viewer_scenes STATIC
    Primitives.h
    Primitives.cpp
    CorridorWidget.h
    CorridorWidget.cpp
    EnvMapWidget.h
    EnvMapWidget.cpp
)

target_include_directories(viewer_scenes PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(viewer_scenes PUBLIC viewer_core Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)
set_target_properties(viewer_scenes PROPERTIES POSITION_INDEPENDENT_CODE ON)
viewer3d_set_output(viewer_scenes)
