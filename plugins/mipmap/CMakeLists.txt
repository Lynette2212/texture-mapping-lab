# Mipmap 纹理过滤插件：除内核外还复用 scenes 静态库里的走廊场景
viewer3d_add_plugin(mipmap_plugin
    MipmapPlugin.h
    MipmapPlugin.cpp
)
target_link_libraries(mipmap_plugin PRIVATE viewer_scenes)
