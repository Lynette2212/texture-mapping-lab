#include "CorridorWidget.h"

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

namespace {

// 走廊着色器：只做纹理采样 + 随距离轻微变暗（让走廊尽头自然过渡到黑色）。
// 不加光照，避免光照变化干扰对纹理过滤效果的观察。
const char* kVert = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aUv;
uniform mat4 uMvp;
uniform vec3 uEye;
out vec2 vUv;
out float vDist;
void main()
{
    vUv = aUv;
    vDist = distance(aPos, uEye);
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)GLSL";

const char* kFrag = R"GLSL(
#version 330 core
in vec2 vUv;
in float vDist;
uniform sampler2D uTex;
uniform bool uUseTex;
uniform vec3 uColor;
uniform float uFadeDist;
out vec4 fragColor;
void main()
{
    vec3 c = uUseTex ? texture(uTex, vUv).rgb : uColor;
    float fade = 1.0 - 0.55 * clamp(vDist / uFadeDist, 0.0, 1.0);
    fragColor = vec4(c * fade, 1.0);
}
)GLSL";

// 走廊由 6 个矩形组成：左墙、右墙、地面、天花板、远端黑墙、背后黑墙
enum QuadId { LeftWall, RightWall, FloorQuad, CeilingQuad, FarEnd, BackEnd, QuadCount };

QString pickDefault(const QString& dir, const QString& preferred)
{
    const QStringList files = TextureUtils::listImages(dir);
    for (const QString& f : files)
        if (QFileInfo(f).fileName().compare(preferred, Qt::CaseInsensitive) == 0)
            return f;
    return files.isEmpty() ? QString() : files.first();
}

} // namespace

CorridorWidget::CorridorWidget(QWidget* parent) : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(640, 400);
    setWindowTitle(QStringLiteral("任务1：Mipmap 纹理过滤 —— 走廊（右键切换过滤方式）"));

    // 默认贴图：红砖墙、沙滩沙土地面、云彩天花板
    m_texPath[Wall] = pickDefault(surfaceDir(Wall), QStringLiteral("red_brick_03.jpg"));
    m_texPath[Floor] = pickDefault(surfaceDir(Floor), QStringLiteral("aerial_beach_01.jpg"));
    m_texPath[Ceiling] = pickDefault(surfaceDir(Ceiling), QStringLiteral("clouds_ceiling.png"));

    // 左上角半透明信息面板（子控件叠加在 OpenGL 画面上）
    m_hud = new QLabel(this);
    m_hud->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_hud->setStyleSheet(QStringLiteral(
        "QLabel{background:rgba(0,0,0,150);color:#f0f0f0;padding:8px;border-radius:6px;"
        "font-family:'Microsoft YaHei';font-size:13px;}"));
    m_hud->move(10, 10);

    // 约 60 帧/秒的定时器：处理按住不放的连续移动和自动漫游
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &CorridorWidget::tick);
    m_timer.start();
    m_clock.start();

    resetView();
    updateHud();
}

CorridorWidget::~CorridorWidget()
{
    makeCurrent();
    for (GLuint& t : m_tex)
        if (t) glDeleteTextures(1, &t);
    if (m_ibo) glDeleteBuffers(1, &m_ibo);
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    delete m_prog;
    doneCurrent();
}

QString CorridorWidget::surfaceName(Surface s)
{
    switch (s) {
    case Wall:    return QStringLiteral("墙面");
    case Floor:   return QStringLiteral("地面");
    case Ceiling: return QStringLiteral("天花板");
    default:      return {};
    }
}

QString CorridorWidget::surfaceDir(Surface s)
{
    static const char* kSub[] = {"wall", "floor", "ceiling"};
    return TextureUtils::dataDir() + QStringLiteral("/corridor/") + QLatin1String(kSub[s]);
}

// ============================================================
// 对外接口
// ============================================================

void CorridorWidget::setFilter(TextureUtils::Filter f)
{
    if (m_filter == f)
        return;
    m_filter = f;
    // 过滤方式只是纹理对象的参数，修改后无需重新上传图像
    if (isValid()) {
        makeCurrent();
        for (GLuint t : m_tex) {
            if (!t) continue;
            glBindTexture(GL_TEXTURE_2D, t);
            TextureUtils::applyFilter(this, GL_TEXTURE_2D, m_filter);
        }
        glBindTexture(GL_TEXTURE_2D, 0);
        doneCurrent();
    }
    updateHud();
    update();
    emit filterChanged(f);
}

void CorridorWidget::setSurfaceTexture(Surface s, const QString& path)
{
    m_texPath[s] = path;
    m_texDirty[s] = true;
    updateHud();
    update();
}

void CorridorWidget::setMipmapTint(bool on)
{
    if (m_mipTint == on)
        return;
    m_mipTint = on;
    // 着色版 mipmap 链与原图不同，需要重建全部纹理
    for (bool& d : m_texDirty)
        d = true;
    updateHud();
    update();
}

void CorridorWidget::setAutoWalk(bool on)
{
    m_autoWalk = on;
    updateHud();
}

void CorridorWidget::resetView()
{
    m_pos = QVector3D(0.0f, kEyeHeight, -1.0f);
    m_yaw = 0.0f;
    m_pitch = 0.0f;
    update();
}

// ============================================================
// OpenGL
// ============================================================

void CorridorWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); // 黑色背景
    glEnable(GL_DEPTH_TEST);

    m_prog = new QOpenGLShaderProgram;
    m_prog->addShaderFromSourceCode(QOpenGLShader::Vertex, kVert);
    m_prog->addShaderFromSourceCode(QOpenGLShader::Fragment, kFrag);
    m_prog->link();

    buildGeometry();
}

void CorridorWidget::buildGeometry()
{
    const float W = kHalfWidth, H = kHeight, L = kLength;
    const float alongRepeat = L / kTile;        // 沿走廊方向纹理重复次数（60 次）
    const float upRepeat = H / kTile;           // 墙面竖直方向重复次数
    const float acrossRepeat = 2 * W / kTile;   // 地面/天花板横向重复次数

    struct QuadDef { float o[3], u[3], v[3]; float ru, rv; };
    const QuadDef quads[QuadCount] = {
        {{-W, 0, 0}, {0, 0, -L}, {0, H, 0}, alongRepeat, upRepeat},    // 左墙（法线 +X 朝内）
        {{ W, 0, -L}, {0, 0, L}, {0, H, 0}, alongRepeat, upRepeat},    // 右墙（法线 -X 朝内）
        {{-W, 0, 0}, {2 * W, 0, 0}, {0, 0, -L}, acrossRepeat, alongRepeat}, // 地面
        {{-W, H, -L}, {2 * W, 0, 0}, {0, 0, L}, acrossRepeat, alongRepeat}, // 天花板
        {{-W, 0, -L}, {2 * W, 0, 0}, {0, H, 0}, 1, 1},                // 远端（黑色）
        {{ W, 0, 0}, {-2 * W, 0, 0}, {0, H, 0}, 1, 1},                // 入口背后（黑色）
    };

    // 把 6 个矩形合并到一个顶点缓冲里，按顺序每 6 个索引对应一个矩形
    std::vector<float> verts;
    std::vector<unsigned int> idx;
    for (const QuadDef& q : quads) {
        const Primitives::MeshData m = Primitives::makeQuad(q.o, q.u, q.v, q.ru, q.rv);
        const unsigned base = unsigned(verts.size() / Primitives::MeshData::kStride);
        verts.insert(verts.end(), m.vertices.begin(), m.vertices.end());
        for (unsigned i : m.indices)
            idx.push_back(base + i);
    }

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, GLsizeiptr(verts.size() * sizeof(float)), verts.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &m_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, GLsizeiptr(idx.size() * sizeof(unsigned)), idx.data(), GL_STATIC_DRAW);

    const GLsizei stride = Primitives::MeshData::kStride * sizeof(float);
    glEnableVertexAttribArray(0); // 位置
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    glEnableVertexAttribArray(2); // 纹理坐标
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
    glBindVertexArray(0);
}

void CorridorWidget::rebuildTexture(Surface s)
{
    m_texDirty[s] = false;
    if (m_tex[s]) {
        glDeleteTextures(1, &m_tex[s]);
        m_tex[s] = 0;
    }
    QImage img;
    if (!m_texPath[s].isEmpty())
        img = QImage(m_texPath[s]);
    if (img.isNull())
        img = TextureUtils::makeCheckerboard(512, 32); // 路径为空或读取失败时用棋盘格

    // 手动生成整条 mipmap 链并上传，再设置当前选择的过滤方式
    m_tex[s] = TextureUtils::createTexture2D(this, img, m_mipTint);
    glBindTexture(GL_TEXTURE_2D, m_tex[s]);
    TextureUtils::applyFilter(this, GL_TEXTURE_2D, m_filter);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void CorridorWidget::paintGL()
{
    for (int s = 0; s < SurfaceCount; ++s)
        if (m_texDirty[s])
            rebuildTexture(Surface(s));

    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMatrix4x4 proj;
    proj.perspective(60.0f, height() > 0 ? float(width()) / float(height()) : 1.0f, 0.05f, 500.0f);
    QMatrix4x4 view;
    view.lookAt(m_pos, m_pos + viewDir(), QVector3D(0, 1, 0));

    m_prog->bind();
    m_prog->setUniformValue("uMvp", proj * view);
    m_prog->setUniformValue("uEye", m_pos);
    m_prog->setUniformValue("uFadeDist", kLength);
    m_prog->setUniformValue("uTex", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(m_vao);

    // 每个矩形绑定各自的纹理后绘制
    const int surfaceOf[QuadCount] = {Wall, Wall, Floor, Ceiling, -1, -1};
    for (int q = 0; q < QuadCount; ++q) {
        const int s = surfaceOf[q];
        m_prog->setUniformValue("uUseTex", s >= 0);
        m_prog->setUniformValue("uColor", QVector3D(0, 0, 0));
        glBindTexture(GL_TEXTURE_2D, s >= 0 ? m_tex[s] : 0);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT,
                       reinterpret_cast<void*>(q * 6 * sizeof(unsigned int)));
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_prog->release();
}

// ============================================================
// 相机与交互
// ============================================================

QVector3D CorridorWidget::viewDir() const
{
    const float y = qDegreesToRadians(m_yaw);
    const float p = qDegreesToRadians(m_pitch);
    return QVector3D(qSin(y) * qCos(p), qSin(p), -qCos(y) * qCos(p));
}

void CorridorWidget::moveBy(float forward, float strafe)
{
    // 只在水平面内移动：前方向 f 与右方向 r
    const float y = qDegreesToRadians(m_yaw);
    const QVector3D f(qSin(y), 0, -qCos(y));
    const QVector3D r(qCos(y), 0, qSin(y));
    m_pos += f * forward + r * strafe;
    // 限制在走廊内部
    m_pos.setX(qBound(-kHalfWidth + 0.3f, m_pos.x(), kHalfWidth - 0.3f));
    m_pos.setZ(qBound(-kLength + 1.0f, m_pos.z(), -0.3f));
    update();
}

void CorridorWidget::tick()
{
    const float dt = qMin(0.1f, m_clock.restart() / 1000.0f);
    const float speed = 10.0f * dt;     // 移动速度：10 单位/秒
    const float turn = 90.0f * dt;      // 转向速度：90 度/秒

    float forward = 0, strafe = 0;
    if (m_keys.contains(Qt::Key_Up) || m_keys.contains(Qt::Key_W)) forward += speed;
    if (m_keys.contains(Qt::Key_Down) || m_keys.contains(Qt::Key_S)) forward -= speed;
    if (m_keys.contains(Qt::Key_A)) strafe -= speed;
    if (m_keys.contains(Qt::Key_D)) strafe += speed;
    if (m_keys.contains(Qt::Key_Left)) { m_yaw -= turn; update(); }
    if (m_keys.contains(Qt::Key_Right)) { m_yaw += turn; update(); }
    if (m_keys.contains(Qt::Key_PageUp)) { m_pitch = qMin(80.0f, m_pitch + turn); update(); }
    if (m_keys.contains(Qt::Key_PageDown)) { m_pitch = qMax(-80.0f, m_pitch - turn); update(); }
    if (forward != 0 || strafe != 0)
        moveBy(forward, strafe);

    if (m_autoWalk) {
        // 自动漫游：沿走廊来回匀速移动，运动中更容易看出远处纹理的闪烁
        m_pos.setZ(m_pos.z() + m_autoDir * 6.0f * dt);
        if (m_pos.z() < -kLength * 0.6f) m_autoDir = 1.0f;
        if (m_pos.z() > -1.0f) m_autoDir = -1.0f;
        update();
    }
}

void CorridorWidget::keyPressEvent(QKeyEvent* e)
{
    const QList<TextureUtils::Filter> filters = TextureUtils::allFilters();
    const int k = e->key();
    if (k >= Qt::Key_1 && k <= Qt::Key_6) {
        setFilter(filters[k - Qt::Key_1]);   // 数字键 1~6 直接切换过滤方式
    } else if (k == Qt::Key_T) {
        setMipmapTint(!m_mipTint);
    } else if (k == Qt::Key_G) {
        // 一键切换"全部棋盘格"：棋盘格最容易看出走样与摩尔纹
        const bool allChecker = m_texPath[Wall].isEmpty() && m_texPath[Floor].isEmpty()
                                && m_texPath[Ceiling].isEmpty();
        for (int s = 0; s < SurfaceCount; ++s) {
            if (allChecker) {
                setSurfaceTexture(Surface(s), m_savedPath[s]);
            } else {
                m_savedPath[s] = m_texPath[s];
                setSurfaceTexture(Surface(s), QString());
            }
        }
    } else if (k == Qt::Key_M) {
        setAutoWalk(!m_autoWalk);
    } else if (k == Qt::Key_Space || k == Qt::Key_Home) {
        resetView();
    } else if (k == Qt::Key_Escape && isWindow()) {
        close();
    } else if (!e->isAutoRepeat()) {
        m_keys.insert(k);
    }
    QOpenGLWidget::keyPressEvent(e);
}

void CorridorWidget::keyReleaseEvent(QKeyEvent* e)
{
    if (!e->isAutoRepeat())
        m_keys.remove(e->key());
    QOpenGLWidget::keyReleaseEvent(e);
}

void CorridorWidget::focusOutEvent(QFocusEvent* e)
{
    m_keys.clear(); // 失去焦点时清空按键，避免"卡键"一直移动
    QOpenGLWidget::focusOutEvent(e);
}

void CorridorWidget::mousePressEvent(QMouseEvent* e)
{
    m_lastMouse = e->pos();
    setFocus();
}

void CorridorWidget::mouseMoveEvent(QMouseEvent* e)
{
    if (e->buttons() & Qt::LeftButton) {
        // 左键拖动：转动视线
        const QPoint d = e->pos() - m_lastMouse;
        m_yaw += d.x() * 0.25f;
        m_pitch = qBound(-80.0f, m_pitch - d.y() * 0.25f, 80.0f);
        update();
    }
    m_lastMouse = e->pos();
}

void CorridorWidget::wheelEvent(QWheelEvent* e)
{
    // 滚轮：前进 / 后退
    moveBy(e->angleDelta().y() / 120.0f * 2.0f, 0);
    e->accept();
}

void CorridorWidget::contextMenuEvent(QContextMenuEvent* e)
{
    QMenu menu(this);

    // ① 6 种纹理过滤方式（互斥单选）
    menu.addSection(QStringLiteral("纹理过滤方式"));
    auto* group = new QActionGroup(&menu);
    group->setExclusive(true);
    const QList<TextureUtils::Filter> filters = TextureUtils::allFilters();
    for (int i = 0; i < filters.size(); ++i) {
        const TextureUtils::Filter f = filters[i];
        QAction* a = menu.addAction(QStringLiteral("%1\t%2").arg(TextureUtils::filterName(f)).arg(i + 1));
        a->setCheckable(true);
        a->setChecked(f == m_filter);
        a->setToolTip(TextureUtils::filterDescription(f));
        group->addAction(a);
        connect(a, &QAction::triggered, this, [this, f] { setFilter(f); });
    }

    // ② 每个面的贴图选择
    menu.addSection(QStringLiteral("贴图"));
    for (int s = 0; s < SurfaceCount; ++s) {
        const Surface surf = Surface(s);
        QMenu* sub = menu.addMenu(surfaceName(surf) + QStringLiteral("贴图"));
        auto* texGroup = new QActionGroup(sub);
        QStringList choices = TextureUtils::listImages(surfaceDir(surf));
        choices.append(QString()); // 空路径 = 程序棋盘格
        for (const QString& path : choices) {
            const QString label = path.isEmpty() ? QStringLiteral("程序生成棋盘格")
                                                 : QFileInfo(path).completeBaseName();
            QAction* a = sub->addAction(label);
            a->setCheckable(true);
            a->setChecked(path == m_texPath[s]);
            texGroup->addAction(a);
            connect(a, &QAction::triggered, this, [this, surf, path] { setSurfaceTexture(surf, path); });
        }
    }

    // ③ 其他选项
    menu.addSection(QStringLiteral("观察辅助"));
    QAction* tint = menu.addAction(QStringLiteral("Mipmap 层级着色\tT"));
    tint->setCheckable(true);
    tint->setChecked(m_mipTint);
    connect(tint, &QAction::toggled, this, &CorridorWidget::setMipmapTint);

    QAction* walk = menu.addAction(QStringLiteral("自动漫游\tM"));
    walk->setCheckable(true);
    walk->setChecked(m_autoWalk);
    connect(walk, &QAction::toggled, this, &CorridorWidget::setAutoWalk);

    QAction* reset = menu.addAction(QStringLiteral("复位视角\tSpace"));
    connect(reset, &QAction::triggered, this, &CorridorWidget::resetView);

    menu.exec(e->globalPos());
}

void CorridorWidget::updateHud()
{
    auto texName = [this](Surface s) {
        return m_texPath[s].isEmpty() ? QStringLiteral("棋盘格") : QFileInfo(m_texPath[s]).completeBaseName();
    };
    m_hud->setText(QStringLiteral(
        "<b style='color:#ffd54f;font-size:15px'>%1</b><br>%2<br>"
        "墙面：%3　地面：%4　天花板：%5<br>"
        "Mipmap 层级着色：%6　自动漫游：%7<br>"
        "<span style='color:#aaa'>右键：菜单　1~6：过滤方式　↑↓/W S：前后　←→：转向　A D：平移<br>"
        "左键拖动：环视　滚轮：前后　T：层级着色　G：棋盘格　M：漫游　空格：复位</span>")
        .arg(TextureUtils::filterName(m_filter), TextureUtils::filterDescription(m_filter),
             texName(Wall), texName(Floor), texName(Ceiling),
             m_mipTint ? QStringLiteral("开") : QStringLiteral("关"),
             m_autoWalk ? QStringLiteral("开") : QStringLiteral("关")));
    m_hud->adjustSize();
}
