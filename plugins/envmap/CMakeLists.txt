# 环境映射插件：除内核外还复用 scenes 静态库里的环境映射演示控件
viewer3d_add_plugin(envmap_plugin
    EnvMapPlugin.h
    EnvMapPlugin.cpp
)
target_link_libraries(envmap_plugin PRIVATE viewer_scenes)
