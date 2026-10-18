cmake_minimum_required(VERSION 3.21)
project(viewer3d_qt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)

set(VIEWER3D_BIN_DIR    ${CMAKE_BINARY_DIR}/bin)
set(VIEWER3D_PLUGIN_DIR ${VIEWER3D_BIN_DIR}/plugins)

# Linux：exe 与 libviewer_core.so 同在 bin/，插件在 bin/plugins/，
# 用相对 RPATH 让整个 bin/ 目录拷到别处也能直接运行
if(UNIX AND NOT APPLE)
    set(CMAKE_BUILD_RPATH "\$ORIGIN;\$ORIGIN/..")
endif()

# 普通目标：exe/dll 输出到 bin/
function(viewer3d_set_output target)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${VIEWER3D_BIN_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${VIEWER3D_BIN_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        string(TOUPPER ${cfg} CFG)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${CFG} ${VIEWER3D_BIN_DIR}
            LIBRARY_OUTPUT_DIRECTORY_${CFG} ${VIEWER3D_BIN_DIR}
            ARCHIVE_OUTPUT_DIRECTORY_${CFG} ${CMAKE_BINARY_DIR}/lib)
    endforeach()
endfunction()

# 插件目标：DLL 输出到 bin/plugins/，链接核心库
function(viewer3d_add_plugin target)
    add_library(${target} SHARED ${ARGN})
    target_link_libraries(${target} PRIVATE viewer_core Qt6::Widgets)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${VIEWER3D_PLUGIN_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${VIEWER3D_PLUGIN_DIR})
    foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        string(TOUPPER ${cfg} CFG)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${CFG} ${VIEWER3D_PLUGIN_DIR}
            LIBRARY_OUTPUT_DIRECTORY_${CFG} ${VIEWER3D_PLUGIN_DIR})
    endforeach()
endfunction()

# 可执行程序：构建后自动部署 Qt 运行库到 bin/（同时扫描核心库，否则会漏 OpenGL 模块）
function(viewer3d_deploy target)
    if(WIN32)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND Qt6::windeployqt --no-translations --no-system-d3d-compiler --no-opengl-sw
                    $<TARGET_FILE:${target}> $<TARGET_FILE:viewer_core>
            COMMENT "windeployqt ${target}")
    endif()
endfunction()

# 运行时文件（插件配置 + data 素材）统一拷贝到 bin/，只拷贝有变化的文件
# copy_directory_if_different 需要 CMake 3.26+；Ubuntu 22.04 自带 3.22，退回 copy_directory
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
    set(_copy_dir_cmd copy_directory_if_different)
else()
    set(_copy_dir_cmd copy_directory)
endif()
add_custom_target(viewer3d_runtime ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${VIEWER3D_BIN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/plugin_config.json ${VIEWER3D_BIN_DIR}/plugin_config.json
    COMMAND ${CMAKE_COMMAND} -E ${_copy_dir_cmd}
            ${CMAKE_SOURCE_DIR}/data ${VIEWER3D_BIN_DIR}/data
    COMMENT "Copy runtime data to bin/")

add_subdirectory(core)      # 内核：主窗口、渲染视口、插件管理、纹理工具（DLL）
add_subdirectory(scenes)    # 场景库：任务1走廊、任务2环境映射控件（静态库）
add_subdirectory(app)       # 主程序 viewer3d.exe（任务3：插件化 3D 模型查看器）
add_subdirectory(demos)     # 独立演示：task1_mipmap.exe、task2_envmap.exe
add_subdirectory(plugins)   # 插件：自动扫描 plugins/ 下每个含 CMakeLists.txt 的子目录
