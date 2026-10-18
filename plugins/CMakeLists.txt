# 自动发现插件：plugins/ 下每个包含 CMakeLists.txt 的子目录都是一个插件。
# 新增插件只需新建子目录（参考 docs/插件开发指南.md），无需修改本文件。
file(GLOB _plugin_dirs LIST_DIRECTORIES true RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)
list(SORT _plugin_dirs)
foreach(_dir ${_plugin_dirs})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${_dir} AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_dir}/CMakeLists.txt)
        message(STATUS "viewer3d plugin: ${_dir}")
        add_subdirectory(${_dir})
    endif()
endforeach()
