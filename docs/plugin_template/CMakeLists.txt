# 生成 bin/plugins/hello_plugin.dll，自动链接 viewer_core 与 Qt6::Widgets
viewer3d_add_plugin(hello_plugin
    HelloPlugin.h
    HelloPlugin.cpp
)
# 需要复用场景控件（走廊 / 环境映射）时再加：
# target_link_libraries(hello_plugin PRIVATE viewer_scenes)
