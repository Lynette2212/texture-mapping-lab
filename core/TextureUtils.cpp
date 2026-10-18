#include "TextureUtils.h"

#include <QCoreApplication>
#include <QDir>
#include <QOpenGLFunctions_3_3_Core>
#include <QtMath>

#include <algorithm>
#include <cmath>

#ifndef GL_TEXTURE_CUBE_MAP_SEAMLESS
#define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F
#endif

namespace TextureUtils {

// ============================================================
// 过滤方式
// ============================================================

QList<Filter> allFilters()
{
    return {Filter::Nearest, Filter::Linear,
            Filter::NearestMipmapNearest, Filter::LinearMipmapNearest,
            Filter::NearestMipmapLinear, Filter::LinearMipmapLinear};
}

QString filterName(Filter f)
{
    switch (f) {
    case Filter::Nearest:              return QStringLiteral("GL_NEAREST");
    case Filter::Linear:               return QStringLiteral("GL_LINEAR");
    case Filter::NearestMipmapNearest: return QStringLiteral("GL_NEAREST_MIPMAP_NEAREST");
    case Filter::LinearMipmapNearest:  return QStringLiteral("GL_LINEAR_MIPMAP_NEAREST");
    case Filter::NearestMipmapLinear:  return QStringLiteral("GL_NEAREST_MIPMAP_LINEAR");
    case Filter::LinearMipmapLinear:   return QStringLiteral("GL_LINEAR_MIPMAP_LINEAR");
    }
    return {};
}

QString filterDescription(Filter f)
{
    switch (f) {
    case Filter::Nearest:
        return QStringLiteral("最近点采样，不用 mipmap：远处闪烁、摩尔纹最严重");
    case Filter::Linear:
        return QStringLiteral("双线性采样，不用 mipmap：近处平滑，远处仍有摩尔纹");
    case Filter::NearestMipmapNearest:
        return QStringLiteral("选最近一层 mipmap + 最近点：远处不闪，但层与层之间有明显分界");
    case Filter::LinearMipmapNearest:
        return QStringLiteral("选最近一层 mipmap + 双线性：层内平滑，层间仍有分界线");
    case Filter::NearestMipmapLinear:
        return QStringLiteral("相邻两层 mipmap 线性混合 + 最近点：层间过渡平滑，层内略有颗粒");
    case Filter::LinearMipmapLinear:
        return QStringLiteral("三线性过滤：两层各双线性再混合，远近都平滑，画质最好");
    }
    return {};
}

unsigned int glMinFilter(Filter f)
{
    switch (f) {
    case Filter::Nearest:              return GL_NEAREST;
    case Filter::Linear:               return GL_LINEAR;
    case Filter::NearestMipmapNearest: return GL_NEAREST_MIPMAP_NEAREST;
    case Filter::LinearMipmapNearest:  return GL_LINEAR_MIPMAP_NEAREST;
    case Filter::NearestMipmapLinear:  return GL_NEAREST_MIPMAP_LINEAR;
    case Filter::LinearMipmapLinear:   return GL_LINEAR_MIPMAP_LINEAR;
    }
    return GL_LINEAR;
}

unsigned int glMagFilter(Filter f)
{
    // 放大（纹素比像素大）时不存在"更小的层"，只能用 NEAREST 或 LINEAR。
    // 这里让放大方式与缩小方式的"层内采样"保持一致。
    switch (f) {
    case Filter::Nearest:
    case Filter::NearestMipmapNearest:
    case Filter::NearestMipmapLinear:
        return GL_NEAREST;
    default:
        return GL_LINEAR;
    }
}

bool usesMipmap(Filter f)
{
    return f != Filter::Nearest && f != Filter::Linear;
}

// ============================================================
// 2D 纹理
// ============================================================

std::vector<QImage> buildMipChain(const QImage& image, bool tintLevels)
{
    // 每层叠加的调试颜色：第 0 层保持原色
    static const QColor kTints[] = {
        QColor(255, 255, 255), QColor(255, 60, 60), QColor(60, 220, 60), QColor(60, 120, 255),
        QColor(255, 230, 40), QColor(255, 60, 255), QColor(40, 230, 230), QColor(255, 140, 0),
    };
    constexpr int kTintCount = int(sizeof(kTints) / sizeof(kTints[0]));

    std::vector<QImage> levels;
    if (image.isNull())
        return levels;

    QImage current = image.convertToFormat(QImage::Format_RGBA8888);
    int level = 0;
    while (true) {
        QImage out = current;
        if (tintLevels && level > 0) {
            const QColor tint = kTints[level % kTintCount];
            for (int y = 0; y < out.height(); ++y) {
                uchar* p = out.scanLine(y);
                for (int x = 0; x < out.width(); ++x, p += 4) {
                    // 原图 40% + 调试色 60%
                    p[0] = uchar(p[0] * 0.4 + tint.red() * 0.6);
                    p[1] = uchar(p[1] * 0.4 + tint.green() * 0.6);
                    p[2] = uchar(p[2] * 0.4 + tint.blue() * 0.6);
                }
            }
        }
        levels.push_back(out);

        if (current.width() == 1 && current.height() == 1)
            break;
        // 宽高各减半（至少为 1），用平滑缩放得到下一层
        const int w = std::max(1, current.width() / 2);
        const int h = std::max(1, current.height() / 2);
        current = current.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        ++level;
    }
    return levels;
}

unsigned int createTexture2D(QOpenGLFunctions_3_3_Core* gl, const QImage& image, bool tintLevels)
{
    if (image.isNull())
        return 0;

    GLuint tex = 0;
    gl->glGenTextures(1, &tex);
    gl->glBindTexture(GL_TEXTURE_2D, tex);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    // QImage 第 0 行在图片顶部，而 OpenGL 纹理坐标 t=0 在底部，所以上下翻转后再上传
    const std::vector<QImage> chain = buildMipChain(image.mirrored(), tintLevels);
    for (int level = 0; level < int(chain.size()); ++level) {
        const QImage& img = chain[level];
        gl->glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA8, img.width(), img.height(), 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, img.constBits());
    }
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, int(chain.size()) - 1);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    applyFilter(gl, GL_TEXTURE_2D, Filter::LinearMipmapLinear);
    gl->glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void applyFilter(QOpenGLFunctions_3_3_Core* gl, unsigned int target, Filter f)
{
    gl->glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GLint(glMinFilter(f)));
    gl->glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GLint(glMagFilter(f)));
}

QImage makeCheckerboard(int size, int cell)
{
    QImage img(size, size, QImage::Format_RGBA8888);
    for (int y = 0; y < size; ++y) {
        uchar* p = img.scanLine(y);
        for (int x = 0; x < size; ++x, p += 4) {
            const bool white = ((x / cell) + (y / cell)) % 2 == 0;
            p[0] = white ? 235 : 30;
            p[1] = white ? 235 : 30;
            p[2] = white ? 235 : 120;
            p[3] = 255;
        }
    }
    return img;
}

// ============================================================
// 全景图 -> 立方体贴图
// ============================================================

namespace {

// 在全景图上按 (u,v) 做双线性采样；u 水平方向首尾相接，v 在上下边界截断
inline void sampleBilinear(const QImage& src, double u, double v, uchar out[4])
{
    const int w = src.width();
    const int h = src.height();
    const double x = u * w - 0.5;
    const double y = std::clamp(v * h - 0.5, 0.0, double(h - 1));
    const int x0 = int(std::floor(x));
    const int y0 = int(std::floor(y));
    const double fx = x - x0;
    const double fy = y - y0;
    const int xa = ((x0 % w) + w) % w;
    const int xb = (xa + 1) % w;
    const int ya = y0;
    const int yb = std::min(y0 + 1, h - 1);

    const uchar* r0 = src.constScanLine(ya);
    const uchar* r1 = src.constScanLine(yb);
    for (int c = 0; c < 4; ++c) {
        const double top = r0[xa * 4 + c] * (1 - fx) + r0[xb * 4 + c] * fx;
        const double bot = r1[xa * 4 + c] * (1 - fx) + r1[xb * 4 + c] * fx;
        out[c] = uchar(std::lround(top * (1 - fy) + bot * fy));
    }
}

} // namespace

std::array<QImage, 6> equirectToCubeFaces(const QImage& panorama, int faceSize)
{
    std::array<QImage, 6> faces;
    if (panorama.isNull())
        return faces;

    const QImage src = panorama.convertToFormat(QImage::Format_RGBA8888);

    for (int face = 0; face < 6; ++face) {
        QImage img(faceSize, faceSize, QImage::Format_RGBA8888);
        for (int y = 0; y < faceSize; ++y) {
            uchar* line = img.scanLine(y);
            // a、b ∈ [-1,1]：面内的横、纵坐标（b 向下为正，对应纹理 t 轴）
            const double b = 2.0 * (y + 0.5) / faceSize - 1.0;
            for (int x = 0; x < faceSize; ++x) {
                const double a = 2.0 * (x + 0.5) / faceSize - 1.0;
                // 按 OpenGL 立方体贴图规范，由 (面, a, b) 求出 3D 方向
                double dx = 0, dy = 0, dz = 0;
                switch (face) {
                case 0: dx =  1; dy = -b; dz = -a; break; // +X
                case 1: dx = -1; dy = -b; dz =  a; break; // -X
                case 2: dx =  a; dy =  1; dz =  b; break; // +Y
                case 3: dx =  a; dy = -1; dz = -b; break; // -Y
                case 4: dx =  a; dy = -b; dz =  1; break; // +Z
                case 5: dx = -a; dy = -b; dz = -1; break; // -Z
                }
                const double len = std::sqrt(dx * dx + dy * dy + dz * dz);
                dx /= len; dy /= len; dz /= len;

                // 方向 -> 经纬度 -> 全景图坐标
                const double lon = std::atan2(dx, -dz);  // [-π, π]，-Z 方向是全景图正中
                const double lat = std::asin(dy);        // [-π/2, π/2]
                const double u = 0.5 + lon / (2.0 * M_PI);
                const double v = 0.5 - lat / M_PI;
                sampleBilinear(src, u, v, line + x * 4);
            }
        }
        faces[face] = img;
    }
    return faces;
}

unsigned int createCubeMapFromPanorama(QOpenGLFunctions_3_3_Core* gl, const QImage& panorama, int faceSize)
{
    if (panorama.isNull())
        return 0;
    const std::array<QImage, 6> faces = equirectToCubeFaces(panorama, faceSize);

    GLuint tex = 0;
    gl->glGenTextures(1, &tex);
    gl->glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    gl->glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    for (int i = 0; i < 6; ++i) {
        gl->glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, faceSize, faceSize, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, faces[i].constBits());
    }
    gl->glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    gl->glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl->glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    gl->glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS); // 消除立方体面与面之间的接缝
    gl->glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    return tex;
}

// ============================================================
// 资源路径
// ============================================================

QString dataDir()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/data");
}

QStringList listImages(const QString& dir)
{
    const QDir d(dir);
    QStringList result;
    const QStringList names = d.entryList(
        {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.bmp")},
        QDir::Files, QDir::Name);
    for (const QString& n : names)
        result.append(d.absoluteFilePath(n));
    return result;
}

} // namespace TextureUtils
