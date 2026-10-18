#pragma once

#include <array>
#include <vector>

#include <QImage>
#include <QList>
#include <QString>
#include <QStringList>

#include "viewer_core_global.h"

class QOpenGLFunctions_3_3_Core;

// 纹理公共工具（内核导出，任务1/任务2演示程序与各插件共用）
namespace TextureUtils {

// ---------------- 纹理过滤方式 ----------------
// 对应 OpenGL 的 6 种缩小过滤（GL_TEXTURE_MIN_FILTER）
enum class Filter {
    Nearest,              // GL_NEAREST                ：取最近的 1 个纹素，不用 mipmap
    Linear,               // GL_LINEAR                 ：取最近 4 个纹素做双线性插值，不用 mipmap
    NearestMipmapNearest, // GL_NEAREST_MIPMAP_NEAREST ：选最接近的 1 层 mipmap，层内取最近纹素
    LinearMipmapNearest,  // GL_LINEAR_MIPMAP_NEAREST  ：选最接近的 1 层 mipmap，层内双线性
    NearestMipmapLinear,  // GL_NEAREST_MIPMAP_LINEAR  ：取相邻 2 层 mipmap，层内最近纹素，层间线性混合
    LinearMipmapLinear    // GL_LINEAR_MIPMAP_LINEAR   ：三线性过滤，2 层各做双线性再层间混合，效果最好
};

VIEWER_CORE_EXPORT QList<Filter> allFilters();
VIEWER_CORE_EXPORT QString filterName(Filter f);        // 如 "GL_LINEAR_MIPMAP_LINEAR"
VIEWER_CORE_EXPORT QString filterDescription(Filter f); // 中文说明
VIEWER_CORE_EXPORT unsigned int glMinFilter(Filter f);  // GL 缩小过滤枚举值
VIEWER_CORE_EXPORT unsigned int glMagFilter(Filter f);  // 与之搭配的放大过滤（放大只能 NEAREST/LINEAR）
VIEWER_CORE_EXPORT bool usesMipmap(Filter f);

// ---------------- 2D 纹理 ----------------
// 手动构建 mipmap 金字塔：第 0 层是原图，之后每层宽高减半直到 1x1。
// tintLevels=true 时给第 1 层起的每层叠加不同颜色，用来直观看出 GPU 正在使用哪一层。
VIEWER_CORE_EXPORT std::vector<QImage> buildMipChain(const QImage& image, bool tintLevels = false);

// 生成纹理对象并上传整条 mipmap 链，返回纹理 id（失败返回 0）。
VIEWER_CORE_EXPORT unsigned int createTexture2D(QOpenGLFunctions_3_3_Core* gl, const QImage& image,
                                                bool tintLevels = false);

// 给当前绑定到 target 的纹理设置过滤方式
VIEWER_CORE_EXPORT void applyFilter(QOpenGLFunctions_3_3_Core* gl, unsigned int target, Filter f);

// 程序生成的棋盘格（最容易观察走样与 mipmap 效果）
VIEWER_CORE_EXPORT QImage makeCheckerboard(int size = 512, int cell = 32);

// ---------------- 立方体贴图（环境映射） ----------------
// 把等距柱状投影全景图（宽:高 = 2:1）转换成立方体贴图的 6 个面，
// 顺序与 OpenGL 一致：+X, -X, +Y, -Y, +Z, -Z
VIEWER_CORE_EXPORT std::array<QImage, 6> equirectToCubeFaces(const QImage& panorama, int faceSize);

// 读取全景图、转换并上传为 GL_TEXTURE_CUBE_MAP（带 mipmap），返回纹理 id（失败返回 0）
VIEWER_CORE_EXPORT unsigned int createCubeMapFromPanorama(QOpenGLFunctions_3_3_Core* gl,
                                                          const QImage& panorama, int faceSize = 1024);

// ---------------- 资源路径 ----------------
VIEWER_CORE_EXPORT QString dataDir();                          // <exe 所在目录>/data
VIEWER_CORE_EXPORT QStringList listImages(const QString& dir); // 目录下 png/jpg/bmp 的绝对路径（按文件名排序）

} // namespace TextureUtils
