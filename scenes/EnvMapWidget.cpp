#include "EnvMapWidget.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QMatrix4x4>
#include <QMenu>
#include <QMouseEvent>
#include <QOpenGLShaderProgram>
#include <QWheelEvent>
#include <QtMath>

#include "Primitives.h"
#include "TextureUtils.h"

namespace {

// ---------------- 物体着色器：环境映射 ----------------
const char* kObjVert = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uViewProj;
uniform mat3 uNormalMat;
out vec3 vWorldPos;
out vec3 vNormal;
void main()
{
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal = uNormalMat * aNormal;   // 法线变换到世界坐标
    gl_Position = uViewProj * world;
}
)GLSL";

const char* kObjFrag = R"GLSL(
#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
uniform samplerCube uEnv;
uniform vec3 uEye;
uniform int uMode;        // 0 反射  1 折射  2 菲涅耳（反射+折射）
uniform float uEta;       // 折射率之比 n1/n2（空气→玻璃 = 1/1.52）
uniform vec3 uTint;
out vec4 fragColor;
void main()
{
    vec3 I = normalize(vWorldPos - uEye);   // 入射方向：从眼睛指向表面点
    vec3 N = normalize(vNormal);
    vec3 reflColor = texture(uEnv, reflect(I, N)).rgb;          // R = I - 2(N·I)N
    vec3 refrColor = texture(uEnv, refract(I, N, uEta)).rgb;    // 斯涅尔定律
    vec3 color;
    if (uMode == 0) {
        color = reflColor;
    } else if (uMode == 1) {
        color = refrColor;
    } else {
        // Schlick 近似：掠射角处反射更强
        float F0 = 0.04;
        float F = F0 + (1.0 - F0) * pow(1.0 - max(dot(-I, N), 0.0), 5.0);
        color = mix(refrColor, reflColor, F);
    }
    fragColor = vec4(color * uTint, 1.0);
}
)GLSL";

// ---------------- 天空盒着色器 ----------------
const char* kSkyVert = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
uniform mat4 uProj;
uniform mat4 uViewRot;   // 去掉平移的视图矩阵：天空盒永远包围观察者
out vec3 vDir;
void main()
{
    vDir = aPos;
    vec4 p = uProj * uViewRot * vec4(aPos, 1.0);
    gl_Position = p.xyww;   // z = w，透视除法后深度恒为 1（最远）
}
)GLSL";

const char* kSkyFrag = R"GLSL(
#version 330 core
in vec3 vDir;
uniform samplerCube uEnv;
out vec4 fragColor;
void main() { fragColor = vec4(texture(uEnv, vDir).rgb, 1.0); }
)GLSL";

QOpenGLShaderProgram* makeProgram(const char* vs, const char* fs)
{
    auto* p = new QOpenGLShaderProgram;
    p->addShaderFromSourceCode(QOpenGLShader::Vertex, vs);
    p->addShaderFromSourceCode(QOpenGLShader::Fragment, fs);
    p->link();
    return p;
}

constexpr float kMinDist = 2.2f;
constexpr float kMaxDist = 40.0f;

} // namespace

EnvMapWidget::EnvMapWidget(QWidget* parent) : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(640, 400);
    setWindowTitle(QStringLiteral("任务2：环境映射 —— 天空盒 + 反射球（右键菜单）"));

    // 默认全景图：优先 rooitou_park，否则取目录中的第一张
    const QStringList panos = TextureUtils::listImages(envDir());
    for (const QString& p : panos)
        if (QFileInfo(p).completeBaseName() == QLatin1String("rooitou_park"))
            m_panoPath = p;
    if (m_panoPath.isEmpty() && !panos.isEmpty())
        m_panoPath = panos.first();

    m_hud = new QLabel(this);
    m_hud->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_hud->setStyleSheet(QStringLiteral(
        "QLabel{background:rgba(0,0,0,150);color:#f0f0f0;padding:8px;border-radius:6px;"
        "font-family:'Microsoft YaHei';font-size:13px;}"));
    m_hud->move(10, 10);

    // 立方体自转动画
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        const float dt = qMin(0.1f, m_clock.restart() / 1000.0f);
        if (m_spinCube && m_showCube) {
            m_cubeAngle += 40.0f * dt;
            update();
        }
    });
    m_timer.start();
    m_clock.start();

    updateHud();
}

EnvMapWidget::~EnvMapWidget()
{
    makeCurrent();
    if (m_envTex) glDeleteTextures(1, &m_envTex);
    destroy(m_sphere);
    destroy(m_cube);
    destroy(m_skyCube);
    delete m_objProg;
    delete m_skyProg;
    doneCurrent();
}

QString EnvMapWidget::materialName(Material m)
{
    switch (m) {
    case Reflect: return QStringLiteral("镜面反射");
    case Refract: return QStringLiteral("玻璃折射");
    case Fresnel: return QStringLiteral("菲涅耳（反射+折射）");
    default:      return {};
    }
}

QString EnvMapWidget::envDir()
{
    return TextureUtils::dataDir() + QStringLiteral("/env");
}

// ============================================================
// 对外接口
// ============================================================

void EnvMapWidget::setPanorama(const QString& path)
{
    m_panoPath = path;
    m_envDirty = true;
    updateHud();
    update();
}

void EnvMapWidget::setCubeVisible(bool on)
{
    m_showCube = on;
    updateHud();
    update();
}

void EnvMapWidget::setCubeSpinning(bool on)
{
    m_spinCube = on;
    updateHud();
}

void EnvMapWidget::setMaterial(Material m)
{
    m_material = m;
    updateHud();
    update();
}

void EnvMapWidget::setDistance(float d)
{
    m_distance = qBound(kMinDist, d, kMaxDist);
    updateHud();
    update();
}

void EnvMapWidget::resetView()
{
    m_yaw = 20.0f;
    m_pitch = 10.0f;
    setDistance(6.0f);
}

// ============================================================
// OpenGL
// ============================================================

template <typename MeshT>
EnvMapWidget::GpuMesh EnvMapWidget::upload(const MeshT& m)
{
    GpuMesh g;
    glGenVertexArrays(1, &g.vao);
    glBindVertexArray(g.vao);
    glGenBuffers(1, &g.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(m.vertices.size() * sizeof(float)), m.vertices.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &g.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(m.indices.size() * sizeof(unsigned)), m.indices.data(),
                 GL_STATIC_DRAW);
    const GLsizei stride = Primitives::MeshData::kStride * sizeof(float);
    glEnableVertexAttribArray(0); // 位置
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(1); // 法线
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glBindVertexArray(0);
    g.count = GLsizei(m.indices.size());
    return g;
}

void EnvMapWidget::destroy(GpuMesh& g)
{
    if (g.ibo) glDeleteBuffers(1, &g.ibo);
    if (g.vbo) glDeleteBuffers(1, &g.vbo);
    if (g.vao) glDeleteVertexArrays(1, &g.vao);
    g = GpuMesh();
}

void EnvMapWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    m_objProg = makeProgram(kObjVert, kObjFrag);
    m_skyProg = makeProgram(kSkyVert, kSkyFrag);

    m_sphere = upload(Primitives::makeSphere(1.0f, 96, 48));
    m_cube = upload(Primitives::makeCube(0.75f));
    m_skyCube = upload(Primitives::makeCube(1.0f)); // 天空盒只用位置属性
}

void EnvMapWidget::paintGL()
{
    if (m_envDirty) {
        // 全景图 -> 立方体贴图（CPU 重采样 6 个面，再上传 GPU）
        m_envDirty = false;
        if (m_envTex) {
            glDeleteTextures(1, &m_envTex);
            m_envTex = 0;
        }
        const QImage pano(m_panoPath);
        if (!pano.isNull())
            m_envTex = TextureUtils::createCubeMapFromPanorama(this, pano, 1024);
    }

    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_envTex)
        return;

    QMatrix4x4 proj;
    proj.perspective(60.0f, height() > 0 ? float(width()) / float(height()) : 1.0f, 0.1f, 200.0f);
    const QVector3D eye = eyePosition();
    QMatrix4x4 view;
    view.lookAt(eye, QVector3D(0, 0, 0), QVector3D(0, 1, 0));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_envTex);

    // ① 物体：显示立方体时球在左、立方体在右；否则球居中
    QMatrix4x4 sphereModel;
    if (m_showCube)
        sphereModel.translate(-1.4f, 0.0f, 0.0f);
    const QVector3D tint = m_material == Reflect ? QVector3D(0.95f, 0.95f, 0.95f)
                                                 : QVector3D(0.92f, 0.97f, 1.0f);
    drawObject(m_sphere, sphereModel, view, proj, eye, tint);

    if (m_showCube) {
        QMatrix4x4 cubeModel;
        cubeModel.translate(1.5f, 0.0f, 0.0f);
        cubeModel.rotate(m_cubeAngle, 0.3f, 1.0f, 0.2f);
        drawObject(m_cube, cubeModel, view, proj, eye, tint);
    }

    // ② 天空盒最后画（深度恒为 1，用 LEQUAL 只填补物体以外的像素）
    QMatrix4x4 viewRot = view;
    viewRot.setColumn(3, QVector4D(0, 0, 0, 1));
    glDepthFunc(GL_LEQUAL);
    m_skyProg->bind();
    m_skyProg->setUniformValue("uProj", proj);
    m_skyProg->setUniformValue("uViewRot", viewRot);
    m_skyProg->setUniformValue("uEnv", 0);
    glBindVertexArray(m_skyCube.vao);
    glDrawElements(GL_TRIANGLES, m_skyCube.count, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    m_skyProg->release();
    glDepthFunc(GL_LESS);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void EnvMapWidget::drawObject(const GpuMesh& g, const QMatrix4x4& model, const QMatrix4x4& view,
                              const QMatrix4x4& proj, const QVector3D& eye, const QVector3D& tint)
{
    m_objProg->bind();
    m_objProg->setUniformValue("uModel", model);
    m_objProg->setUniformValue("uViewProj", proj * view);
    m_objProg->setUniformValue("uNormalMat", model.normalMatrix());
    m_objProg->setUniformValue("uEye", eye);
    m_objProg->setUniformValue("uMode", int(m_material));
    m_objProg->setUniformValue("uEta", 1.0f / 1.52f);
    m_objProg->setUniformValue("uTint", tint);
    m_objProg->setUniformValue("uEnv", 0);
    glBindVertexArray(g.vao);
    glDrawElements(GL_TRIANGLES, g.count, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    m_objProg->release();
}

QVector3D EnvMapWidget::eyePosition() const
{
    const float y = qDegreesToRadians(m_yaw);
    const float p = qDegreesToRadians(m_pitch);
    return QVector3D(qCos(p) * qSin(y), qSin(p), qCos(p) * qCos(y)) * m_distance;
}

// ============================================================
// 交互
// ============================================================

void EnvMapWidget::keyPressEvent(QKeyEvent* e)
{
    const QStringList panos = TextureUtils::listImages(envDir());
    const int k = e->key();
    if (k == Qt::Key_Plus || k == Qt::Key_Equal || k == Qt::Key_W) {
        setDistance(m_distance * 0.9f);              // 物体拉近
    } else if (k == Qt::Key_Minus || k == Qt::Key_S) {
        setDistance(m_distance / 0.9f);              // 物体推远
    } else if (k == Qt::Key_Left) {
        m_yaw -= 5; update();
    } else if (k == Qt::Key_Right) {
        m_yaw += 5; update();
    } else if (k == Qt::Key_Up) {
        m_pitch = qMin(85.0f, m_pitch + 5); update();
    } else if (k == Qt::Key_Down) {
        m_pitch = qMax(-85.0f, m_pitch - 5); update();
    } else if (k == Qt::Key_C) {
        setCubeVisible(!m_showCube);
    } else if (k == Qt::Key_R) {
        setCubeSpinning(!m_spinCube);
    } else if (k == Qt::Key_M) {
        setMaterial(Material((m_material + 1) % MaterialCount));
    } else if (k >= Qt::Key_1 && k <= Qt::Key_9 && k - Qt::Key_1 < panos.size()) {
        setPanorama(panos[k - Qt::Key_1]);
    } else if (k == Qt::Key_Space) {
        resetView();
    } else if (k == Qt::Key_Escape && isWindow()) {
        close();
    }
    QOpenGLWidget::keyPressEvent(e);
}

void EnvMapWidget::mousePressEvent(QMouseEvent* e)
{
    m_lastMouse = e->pos();
    m_rightDragged = false;
    setFocus();
}

void EnvMapWidget::mouseReleaseEvent(QMouseEvent* e)
{
    QOpenGLWidget::mouseReleaseEvent(e);
}

void EnvMapWidget::mouseMoveEvent(QMouseEvent* e)
{
    const QPoint d = e->pos() - m_lastMouse;
    m_lastMouse = e->pos();
    if (e->buttons() & Qt::LeftButton) {
        // 左键拖动：绕物体环视（天空盒随之旋转，反射内容同步变化）
        m_yaw -= d.x() * 0.4f;
        m_pitch = qBound(-85.0f, m_pitch + d.y() * 0.4f, 85.0f);
        update();
    } else if (e->buttons() & Qt::RightButton) {
        // 右键上下拖动：物体远近
        if (qAbs(d.y()) > 0)
            m_rightDragged = true;
        setDistance(m_distance * qPow(1.01, d.y()));
    }
}

void EnvMapWidget::wheelEvent(QWheelEvent* e)
{
    // 滚轮：物体远近（向前滚拉近）
    setDistance(m_distance * qPow(0.9, e->angleDelta().y() / 120.0));
    e->accept();
}

void EnvMapWidget::contextMenuEvent(QContextMenuEvent* e)
{
    if (m_rightDragged) { // 刚刚是右键拖动调整远近，不弹菜单
        m_rightDragged = false;
        return;
    }
    QMenu menu(this);

    QAction* cube = menu.addAction(QStringLiteral("显示立方体\tC"));
    cube->setCheckable(true);
    cube->setChecked(m_showCube);
    connect(cube, &QAction::toggled, this, &EnvMapWidget::setCubeVisible);

    QAction* spin = menu.addAction(QStringLiteral("立方体自转\tR"));
    spin->setCheckable(true);
    spin->setChecked(m_spinCube);
    connect(spin, &QAction::toggled, this, &EnvMapWidget::setCubeSpinning);

    QMenu* matMenu = menu.addMenu(QStringLiteral("表面材质"));
    auto* matGroup = new QActionGroup(matMenu);
    for (int i = 0; i < MaterialCount; ++i) {
        QAction* a = matMenu->addAction(materialName(Material(i)));
        a->setCheckable(true);
        a->setChecked(i == m_material);
        matGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, i] { setMaterial(Material(i)); });
    }

    QMenu* panoMenu = menu.addMenu(QStringLiteral("环境全景图"));
    auto* panoGroup = new QActionGroup(panoMenu);
    const QStringList panos = TextureUtils::listImages(envDir());
    for (int i = 0; i < panos.size(); ++i) {
        const QString path = panos[i];
        QAction* a = panoMenu->addAction(QStringLiteral("%1\t%2").arg(QFileInfo(path).completeBaseName()).arg(i + 1));
        a->setCheckable(true);
        a->setChecked(path == m_panoPath);
        panoGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, path] { setPanorama(path); });
    }

    menu.addSeparator();
    connect(menu.addAction(QStringLiteral("物体拉近\t+")), &QAction::triggered, this,
            [this] { setDistance(m_distance * 0.8f); });
    connect(menu.addAction(QStringLiteral("物体推远\t-")), &QAction::triggered, this,
            [this] { setDistance(m_distance / 0.8f); });
    connect(menu.addAction(QStringLiteral("复位视角\tSpace")), &QAction::triggered, this, &EnvMapWidget::resetView);

    menu.exec(e->globalPos());
}

void EnvMapWidget::updateHud()
{
    m_hud->setText(QStringLiteral(
        "<b style='color:#80deea;font-size:15px'>环境映射：%1</b><br>"
        "全景图：%2<br>立方体：%3　自转：%4　物体距离：%5<br>"
        "<span style='color:#aaa'>右键：菜单　滚轮 / 右键上下拖 / +-：物体远近<br>"
        "左键拖动 / 方向键：环视　C：立方体　M：材质　R：自转　1~9：全景图　空格：复位</span>")
        .arg(materialName(m_material),
             m_panoPath.isEmpty() ? QStringLiteral("（data/env 中没有全景图）")
                                  : QFileInfo(m_panoPath).completeBaseName(),
             m_showCube ? QStringLiteral("显示") : QStringLiteral("隐藏"),
             m_spinCube ? QStringLiteral("开") : QStringLiteral("关"))
        .arg(m_distance, 0, 'f', 1));
    m_hud->adjustSize();
}
