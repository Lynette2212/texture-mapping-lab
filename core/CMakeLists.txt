add_library(viewer_core SHARED
    viewer_core_global.h
    PluginInterface.h
    ViewerContext.h
    ViewerContext.cpp
    Mesh.h
    Mesh.cpp
    TextureUtils.h
    TextureUtils.cpp
    GLViewport.h
    GLViewport.cpp
    MainWindow.h
    MainWindow.cpp
    PluginManager.h
    PluginManager.cpp
)

target_compile_definitions(viewer_core PRIVATE VIEWER_CORE_LIBRARY)

target_include_directories(viewer_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(viewer_core
    PUBLIC Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)

viewer3d_set_output(viewer_core)
