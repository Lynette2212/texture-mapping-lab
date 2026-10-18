#include "GLViewport.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QtMath>

#include "Mesh.h"

namespace {

// ---------------- 模型着色器 ----------------
// uTiling：纹理坐标缩放倍数（>1 时纹理重复平铺，便于观察 mipmap 的缩小过滤效果）
const char* kMeshVert = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
uniform mat4 uMvp;
uniform float uTiling;
out vec3 vNormal;
out vec3 vWorldPos;
out vec2 vUv;
void main()
{
    vNormal = aNormal;
    vWorldPos = aPos;
    vUv = aUv * uTiling;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)GLSL";

// Phong 光照 + 可选 2D 纹理 + 可选环境映射（立方体贴图反射）
const char* kMeshFrag = R"GLSL(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
in vec2 vUv;
uniform vec3 uLightPos;
uniform vec3 uEyePos;
uniform vec3 uBaseColor;
uniform float uAmbient;
uniform float uDiffuse;
uniform float uSpecular;
uniform bool uLighting;
uniform bool uUseTexture;
uniform sampler2D uTex;
uniform bool uUseEnv;
uniform float uReflectivity;
uniform samplerCube uEnv;
out vec4 fragColor;
void main()
{
    vec3 base = uUseTexture ? texture(uTex, vUv).rgb : uBaseColor;
    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;

    vec3 color = base;
    if (uLighting) {
        vec3 l = normalize(uLightPos - vWorldPos);
        vec3 v = normalize(uEyePos - vWorldPos);
        vec3 r = reflect(-l, n);
        float diff = max(dot(n, l), 0.0);
        float spec = pow(max(dot(r, v), 0.0), 32.0);
        color = uAmbient * base + uDiffuse * diff * base + uSpecular * spec * vec3(1.0);
    }

    if (uUseEnv) {
        // 环境映射：视线 I 关于法线 N 的反射方向 R 作为立方体贴图的采样方向
        vec3 I = normalize(vWorldPos - uEyePos);
        vec3 R = reflect(I, n);
        color = mix(color, texture(uEnv, R).rgb, uReflectivity);
    }
    fragColor = vec4(color, 1.0);
}
)GLSL";

const char* kAxesVert = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMvp;
out vec3 vColor;
void main()
{
    vColor = aColor;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)GLSL";

const char* kAxesFrag = R"GLSL(
#version 330 core
in vec3 vColor;
out vec4 fragColor;
void main() { fragColor = vec4(vColor, 1.0); }
)GLSL";

// ---------------- 天空盒着色器 ----------------
// 去掉平移的视图矩阵让天空盒始终包围相机；xyww 使深度恒为 1（最远处）
const char* kSkyVert = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uProj;
uniform mat4 uViewRot;
out vec3 vDir;
void main()
{
    vDir = aPos;
    vec4 p = uProj * uViewRot * vec4(aPos, 1.0);
    gl_Position = p.xyww;
}
)GLSL";

const char* kSkyFrag = R"GLSL(
#version 330 core
in vec3 vDir;
uniform samplerCube uEnv;
out vec4 fragColor;
void main() { fragColor = vec4(texture(uEnv, vDir).rgb, 1.0); }
)GLSL";

// 单位立方体 36 个顶点（天空盒）
const float kSkyCube[] = {
    -1,  1, -1,  -1, -1, -1,   1, -1, -1,   1, -1, -1,   1,  1, -1,  -1,  1, -1,
    -1, -1,  1,  -1, -1, -1,  -1,  1, -1,  -1,  1, -1,  -1,  1,  1,  -1, -1,  1,
     1, -1, -1,   1, -1,  1,   1,  1,  1,   1,  1,  1,   1,  1, -1,   1, -1, -1,
    -1, -1,  1,  -1,  1,  1,   1,  1,  1,   1,  1,  1,   1, -1,  1,  -1, -1,  1,
    -1,  1, -1,   1,  1, -1,   1,  1,  1,   1,  1,  1,  -1,  1,  1,  -1,  1, -1,
    -1, -1, -1,  -1, -1,  1,   1, -1, -1,   1, -1, -1,  -1, -1,  1,   1, -1,  1,
};

QOpenGLShaderProgram* makeProgram(const char* vertSrc, const char* fragSrc)
{
    auto* prog = new QOpenGLShaderProgram;
    prog->addShaderFromSourceCode(QOpenGLShader::Vertex, vertSrc);
    prog->addShaderFromSourceCode(QOpenGLShader::Fragment, fragSrc);
    prog->link();
    return prog;
}

} // namespace

GLViewport::GLViewport(QWidget* parent) : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

GLViewport::~GLViewport()
{
    makeCurrent();
    if (m_texture) glDeleteTextures(1, &m_texture);
    if (m_envTexture) glDeleteTextures(1, &m_envTexture);
    delete m_ibo; delete m_uvVbo; delete m_nrmVbo; delete m_posVbo;
    delete m_vao;
    delete m_axesVbo; delete m_axesVao;
    delete m_skyVbo; delete m_skyVao;
    delete m_meshProg; delete m_axesProg; delete m_skyProg;
    doneCurrent();
}

void GLViewport::setMesh(Mesh* mesh)
{
    m_mesh = mesh;
    m_meshDirty = true;
    resetCamera();
    update();
}

void GLViewport::setRenderMode(RenderMode mode)
{
    m_renderMode = mode;
    update();
}

void GLViewport::setLightingEnabled(bool on)
{
    m_lighting = on;
    update();
}

void GLViewport::setLightingCoeffs(float ambient, float diffuse, float specular)
{
    m_ambient = ambient;
    m_diffuse = diffuse;
    m_specular = specular;
    update();
}

void GLViewport::setTextureImage(const QImage& image)
{
    m_pendingTexture = image;
    m_textureEnabled = !image.isNull();
    m_textureDirty = true;
    update();
}

void GLViewport::clearTexture()
{
    setTextureImage(QImage());
}

void GLViewport::setTextureFilter(TextureUtils::Filter filter)
{
    m_filter = filter;
    // 只改纹理参数，不必重新上传图像
    if (m_texture && isValid()) {
        makeCurrent();
        glBindTexture(GL_TEXTURE_2D, m_texture);
        TextureUtils::applyFilter(this, GL_TEXTURE_2D, m_filter);
        glBindTexture(GL_TEXTURE_2D, 0);
        doneCurrent();
    }
    update();
}

void GLViewport::setTextureTiling(float tiling)
{
    m_tiling = tiling;
    update();
}

void GLViewport::setMipmapLevelTint(bool on)
{
    if (m_mipTint == on)
        return;
    m_mipTint = on;
    // 着色版与原版 mipmap 链内容不同，需要重新生成纹理
    if (m_textureEnabled)
        m_textureDirty = true;
    update();
}

void GLViewport::setEnvironmentMap(const QImage& panorama)
{
    m_pendingEnv = panorama;
    m_envEnabled = !panorama.isNull();
    m_envDirty = true;
    update();
}

void GLViewport::setSkyboxVisible(bool on)
{
    m_skyboxVisible = on;
    update();
}

void GLViewport::setReflectivity(float r)
{
    m_reflectivity = qBound(0.0f, r, 1.0f);
    update();
}

void GLViewport::resetCamera()
{
    m_target = m_mesh ? m_mesh->center() : QVector3D(0, 0, 0);
    m_distance = m_mesh ? m_mesh->boundingRadius() * 3.0f : 60.0f;
    update();
}

void GLViewport::setCameraOrbit(float yawDeg, float pitchDeg, float distance, const QVector3D& target)
{
    m_yaw = yawDeg;
    m_pitch = qBound(-89.0f, pitchDeg, 89.0f);
    m_distance = qBound(0.01f, distance, 1e6f);
    m_target = target;
    update();
}

void GLViewport::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.15f, 0.15f, 0.17f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    m_meshProg = makeProgram(kMeshVert, kMeshFrag);
    m_axesProg = makeProgram(kAxesVert, kAxesFrag);
    m_skyProg = makeProgram(kSkyVert, kSkyFrag);

    m_vao = new QOpenGLVertexArrayObject;
    m_vao->create();
    m_posVbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer); m_posVbo->create();
    m_nrmVbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer); m_nrmVbo->create();
    m_uvVbo  = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer); m_uvVbo->create();
    m_ibo    = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);  m_ibo->create();

    // 单位坐标轴线段：位置 + 颜色
    const float axes[] = {
        0, 0, 0,  1, 0, 0,   1, 0, 0,  1, 0, 0, // X 红
        0, 0, 0,  0, 1, 0,   0, 1, 0,  0, 1, 0, // Y 绿
        0, 0, 0,  0, 0, 1,   0, 0, 1,  0, 0, 1, // Z 蓝
    };
    m_axesVao = new QOpenGLVertexArrayObject;
    m_axesVao->create();
    m_axesVao->bind();
    m_axesVbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    m_axesVbo->create();
    m_axesVbo->bind();
    m_axesVbo->allocate(axes, sizeof(axes));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    m_axesVao->release();

    // 天空盒立方体
    m_skyVao = new QOpenGLVertexArrayObject;
    m_skyVao->create();
    m_skyVao->bind();
    m_skyVbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
    m_skyVbo->create();
    m_skyVbo->bind();
    m_skyVbo->allocate(kSkyCube, sizeof(kSkyCube));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    m_skyVao->release();
}

void GLViewport::resizeGL(int, int)
{
    // QOpenGLWidget 在每次 paintGL 前会按设备像素自动设置视口，这里无需处理
}

void GLViewport::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (m_meshDirty)
        uploadMesh();
    if (m_textureDirty)
        rebuildTexture();
    if (m_envDirty)
        rebuildEnvironment();

    const bool hasMesh = m_mesh && m_indexCount > 0;
    const bool drawSky = m_envEnabled && m_envTexture && m_skyboxVisible;
    if (!hasMesh && !drawSky)
        return;

    const float radius = hasMesh ? m_mesh->boundingRadius() : 1.0f;
    QMatrix4x4 proj;
    const float aspect = height() > 0 ? float(width()) / float(height()) : 1.0f;
    proj.perspective(45.0f, aspect, m_distance / 100.0f, m_distance * 100.0f + radius * 4.0f);

    QMatrix4x4 view;
    view.lookAt(eyePosition(), m_target, QVector3D(0, 1, 0));
    const QMatrix4x4 projView = proj * view;

    if (hasMesh) {
        drawMesh(projView);
        drawAxes(projView);
    }
    // 天空盒最后画：深度恒为 1，只填充模型没有覆盖的像素，节省片元计算
    if (drawSky)
        drawSkybox(proj, view);
}

void GLViewport::uploadMesh()
{
    m_meshDirty = false;
    if (!m_mesh) {
        m_indexCount = 0;
        return;
    }

    m_vao->bind();

    m_posVbo->bind();
    m_posVbo->allocate(m_mesh->positions().data(),
                       static_cast<int>(m_mesh->positions().size() * sizeof(float)));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    m_nrmVbo->bind();
    m_nrmVbo->allocate(m_mesh->normals().data(),
                       static_cast<int>(m_mesh->normals().size() * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    m_uvVbo->bind();
    m_uvVbo->allocate(m_mesh->texCoords().data(),
                      static_cast<int>(m_mesh->texCoords().size() * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    m_ibo->bind();
    m_ibo->allocate(m_mesh->indices().data(),
                    static_cast<int>(m_mesh->indices().size() * sizeof(unsigned int)));
    m_indexCount = static_cast<int>(m_mesh->indices().size());

    m_vao->release();
}

void GLViewport::rebuildTexture()
{
    m_textureDirty = false;
    if (m_texture) {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
    if (!m_textureEnabled || m_pendingTexture.isNull())
        return;
    // 手动构建并上传整条 mipmap 链，然后套用当前选择的过滤方式
    m_texture = TextureUtils::createTexture2D(this, m_pendingTexture, m_mipTint);
    if (m_texture) {
        glBindTexture(GL_TEXTURE_2D, m_texture);
        TextureUtils::applyFilter(this, GL_TEXTURE_2D, m_filter);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void GLViewport::rebuildEnvironment()
{
    m_envDirty = false;
    if (m_envTexture) {
        glDeleteTextures(1, &m_envTexture);
        m_envTexture = 0;
    }
    if (m_envEnabled && !m_pendingEnv.isNull())
        m_envTexture = TextureUtils::createCubeMapFromPanorama(this, m_pendingEnv, 1024);
    m_pendingEnv = QImage(); // 已上传到显存，释放内存中的大图
}

void GLViewport::drawSkybox(const QMatrix4x4& proj, const QMatrix4x4& view)
{
    QMatrix4x4 viewRot = view;
    viewRot.setColumn(3, QVector4D(0, 0, 0, 1)); // 去掉平移，只保留旋转

    glDepthFunc(GL_LEQUAL); // 深度恒为 1.0，需要 <= 才能通过深度测试
    m_skyProg->bind();
    m_skyProg->setUniformValue("uProj", proj);
    m_skyProg->setUniformValue("uViewRot", viewRot);
    m_skyProg->setUniformValue("uEnv", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envTexture);
    m_skyVao->bind();
    glDrawArrays(GL_TRIANGLES, 0, 36);
    m_skyVao->release();
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    m_skyProg->release();
    glDepthFunc(GL_LESS);
}

void GLViewport::drawMesh(const QMatrix4x4& projView)
{
    if (m_renderMode == Wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    const bool useTex = m_textureEnabled && m_texture;
    const bool useEnv = m_envEnabled && m_envTexture;

    m_meshProg->bind();
    m_meshProg->setUniformValue("uMvp", projView);
    const QVector3D eye = eyePosition();
    m_meshProg->setUniformValue("uLightPos", eye);
    m_meshProg->setUniformValue("uEyePos", eye);
    m_meshProg->setUniformValue("uBaseColor", QVector3D(1.0f, 1.0f, 1.0f));
    m_meshProg->setUniformValue("uAmbient", m_ambient);
    m_meshProg->setUniformValue("uDiffuse", m_diffuse);
    m_meshProg->setUniformValue("uSpecular", m_specular);
    m_meshProg->setUniformValue("uLighting", m_lighting);
    m_meshProg->setUniformValue("uTiling", m_tiling);
    m_meshProg->setUniformValue("uUseTexture", useTex);
    m_meshProg->setUniformValue("uUseEnv", useEnv);
    m_meshProg->setUniformValue("uReflectivity", m_reflectivity);
    // 纹理单元 0：2D 纹理；纹理单元 1：环境立方体贴图
    m_meshProg->setUniformValue("uTex", 0);
    m_meshProg->setUniformValue("uEnv", 1);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, useTex ? m_texture : 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, useEnv ? m_envTexture : 0);

    m_vao->bind();
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    m_vao->release();

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_meshProg->release();

    if (m_renderMode == Wireframe)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void GLViewport::drawAxes(const QMatrix4x4& projView)
{
    QMatrix4x4 model;
    model.translate(m_mesh->center());
    model.scale(m_mesh->boundingRadius());

    m_axesProg->bind();
    m_axesProg->setUniformValue("uMvp", projView * model);
    m_axesVao->bind();
    glDrawArrays(GL_LINES, 0, 6);
    m_axesVao->release();
    m_axesProg->release();
}

QVector3D GLViewport::eyePosition() const
{
    const float yaw = qDegreesToRadians(m_yaw);
    const float pitch = qDegreesToRadians(m_pitch);
    const QVector3D dir(qCos(pitch) * qSin(yaw), qSin(pitch), qCos(pitch) * qCos(yaw));
    return m_target + dir * m_distance;
}

void GLViewport::mousePressEvent(QMouseEvent* event)
{
    m_lastPos = event->pos();
    QOpenGLWidget::mousePressEvent(event);
}

void GLViewport::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint delta = event->pos() - m_lastPos;
    m_lastPos = event->pos();

    if (event->buttons() & Qt::LeftButton) {
        m_yaw += delta.x() * 0.5f;
        m_pitch = qBound(-89.0f, m_pitch + delta.y() * 0.5f, 89.0f);
        update();
    } else if (event->buttons() & (Qt::RightButton | Qt::MiddleButton)) {
        const QVector3D viewDir = (m_target - eyePosition()).normalized();
        const QVector3D right = QVector3D::crossProduct(viewDir, QVector3D(0, 1, 0)).normalized();
        const QVector3D up = QVector3D::crossProduct(right, viewDir);
        const float scale = m_distance * 0.0015f;
        m_target += (-delta.x() * right + delta.y() * up) * scale;
        update();
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void GLViewport::wheelEvent(QWheelEvent* event)
{
    const float steps = event->angleDelta().y() / 120.0f;
    m_distance *= qPow(0.9, steps);
    m_distance = qBound(0.01f, m_distance, 1e6f);
    update();
    event->accept();
}
