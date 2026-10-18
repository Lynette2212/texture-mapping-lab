#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QMatrix4x4>
#include <QVector3D>
#include <QPoint>
#include <QImage>

#include "TextureUtils.h"
#include "viewer_core_global.h"

class Mesh;
class QOpenGLShaderProgram;
class QOpenGLVertexArrayObject;
class QOpenGLBuffer;

// OpenGL 渲染后端（对应 Python 版 VTK 渲染器）。
// 插件通过 ctx->viewport 调用公共接口：显示模式、光照、纹理、纹理过滤、环境映射、相机复位。
class VIEWER_CORE_EXPORT GLViewport : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    enum RenderMode { Surface, Wireframe };

    explicit GLViewport(QWidget* parent = nullptr);
    ~GLViewport() override;

    void setMesh(Mesh* mesh);           // 不接管所有权，mesh 由主窗口持有

    void setRenderMode(RenderMode mode);
    RenderMode renderMode() const { return m_renderMode; }

    void setLightingEnabled(bool on);
    bool lightingEnabled() const { return m_lighting; }
    void setLightingCoeffs(float ambient, float diffuse, float specular);

    // ---------- 纹理 ----------
    void setTextureImage(const QImage& image); // 空图等价于 clearTexture()
    void clearTexture();
    bool hasTexture() const { return m_textureEnabled; }

    // 纹理过滤方式（mipmap 插件使用），默认三线性 GL_LINEAR_MIPMAP_LINEAR
    void setTextureFilter(TextureUtils::Filter filter);
    TextureUtils::Filter textureFilter() const { return m_filter; }
    // 纹理平铺次数：纹理坐标整体放大的倍数，越大越容易看出 mipmap 差异
    void setTextureTiling(float tiling);
    float textureTiling() const { return m_tiling; }
    // 给每层 mipmap 着色（调试用），直观看出 GPU 选了哪一层
    void setMipmapLevelTint(bool on);
    bool mipmapLevelTint() const { return m_mipTint; }

    // ---------- 环境映射（envmap 插件使用） ----------
    // 传入等距柱状全景图（2:1），内部转换为立方体贴图；空图表示关闭环境映射
    void setEnvironmentMap(const QImage& panorama);
    bool hasEnvironmentMap() const { return m_envEnabled; }
    void setSkyboxVisible(bool on);             // 是否把全景图画成背景天空盒
    bool skyboxVisible() const { return m_skyboxVisible; }
    void setReflectivity(float r);              // 模型表面反射环境的比例 0~1
    float reflectivity() const { return m_reflectivity; }

    void resetCamera();
    // 轨道相机：绕 target 旋转，yaw/pitch 为角度，distance 为到 target 的距离（插件可设定观察视角）
    void setCameraOrbit(float yawDeg, float pitchDeg, float distance, const QVector3D& target);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void uploadMesh();
    void rebuildTexture();
    void rebuildEnvironment();
    void drawSkybox(const QMatrix4x4& proj, const QMatrix4x4& view);
    void drawMesh(const QMatrix4x4& projView);
    void drawAxes(const QMatrix4x4& projView);
    QVector3D eyePosition() const;

    Mesh* m_mesh = nullptr;
    bool m_meshDirty = false;
    int m_indexCount = 0;

    QOpenGLShaderProgram* m_meshProg = nullptr;
    QOpenGLShaderProgram* m_axesProg = nullptr;
    QOpenGLShaderProgram* m_skyProg = nullptr;
    QOpenGLVertexArrayObject* m_vao = nullptr;
    QOpenGLBuffer* m_posVbo = nullptr;
    QOpenGLBuffer* m_nrmVbo = nullptr;
    QOpenGLBuffer* m_uvVbo = nullptr;
    QOpenGLBuffer* m_ibo = nullptr;
    QOpenGLVertexArrayObject* m_axesVao = nullptr;
    QOpenGLBuffer* m_axesVbo = nullptr;
    QOpenGLVertexArrayObject* m_skyVao = nullptr;
    QOpenGLBuffer* m_skyVbo = nullptr;

    // 2D 纹理
    GLuint m_texture = 0;
    QImage m_pendingTexture;
    bool m_textureDirty = false;
    bool m_textureEnabled = false;
    TextureUtils::Filter m_filter = TextureUtils::Filter::LinearMipmapLinear;
    float m_tiling = 1.0f;
    bool m_mipTint = false;

    // 环境映射
    GLuint m_envTexture = 0;
    QImage m_pendingEnv;
    bool m_envDirty = false;
    bool m_envEnabled = false;
    bool m_skyboxVisible = true;
    float m_reflectivity = 0.6f;

    RenderMode m_renderMode = Surface;
    bool m_lighting = true;
    float m_ambient = 0.2f;
    float m_diffuse = 0.8f;
    float m_specular = 0.4f;

    float m_yaw = 45.0f;
    float m_pitch = 25.0f;
    float m_distance = 60.0f;
    QVector3D m_target{0.0f, 0.0f, 0.0f};
    QPoint m_lastPos;
};
