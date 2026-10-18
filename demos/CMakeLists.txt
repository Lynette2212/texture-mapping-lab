# 两个实验任务的独立演示程序，输出到 bin/
foreach(demo task1_mipmap task2_envmap)
    add_executable(${demo} WIN32 ${demo}.cpp)
    target_link_libraries(${demo} PRIVATE viewer_scenes)
    viewer3d_set_output(${demo})
    add_dependencies(${demo} viewer3d_runtime)
    viewer3d_deploy(${demo})
endforeach()
