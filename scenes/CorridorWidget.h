#pragma once

#include <QElapsedTimer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector3D>

#include "TextureUtils.h"

class QLabel;
class QOpenGLShaderProgram;

// 任务1：Mipmap 纹理过滤演示 —— 长走廊场景
// 左右砖墙、云彩天花板、沙土地面、远端黑色；右键弹出菜单切换 6 种纹理过滤方式。
// 既可作为独立程序窗口（demos/task1_mipmap），也可被 mipmap 插件弹出。
class CorridorWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    enum Surface { Wall = 0, Floor = 1, Ceiling = 2, SurfaceCount = 3 };

    explicit CorridorWidget(QWidget* parent = nullptr);
    ~CorridorWidget() override;

    void setFilter(TextureUtils::Filter f);
    TextureUtils::Filter filter() const { return m_filter; }

    // path 为空表示使用程序生成的棋盘格
    void setSurfaceTexture(Surface s, const QString& path);
    QString surfaceTexture(Surface s) const { return m_texPath[s]; }

    void setMipmapTint(bool on);
    bool mipmapTint() const { return m_mipTint; }

    void setAutoWalk(bool on);
    void resetView();

    static QString surfaceName(Surface s);
    static QString surfaceDir(Surface s); // data/corridor/<wall|floor|ceiling>

signals:
    void filterChanged(TextureUtils::Filter f);

protected:
    void initializeGL() override;
    void paintGL() override;

    void keyPressEvent(QKeyEvent* e) override;
    void keyReleaseEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;
    void focusOutEvent(QFocusEvent* e) override;

private:
    void buildGeometry();
    void rebuildTexture(Surface s);
    void tick();
    void moveBy(float forward, float strafe);
    void updateHud();
    QVector3D viewDir() const;

    // ---------------- 场景尺寸 ----------------
    static constexpr float kHalfWidth = 2.5f;  // 走廊半宽
    static constexpr float kHeight = 5.0f;     // 走廊高度
    static constexpr float kLength = 150.0f;   // 走廊长度（沿 -Z）
    static constexpr float kTile = 2.5f;       // 一张纹理覆盖的世界尺寸
    static constexpr float kEyeHeight = 2.5f;  // 视点高度

    QOpenGLShaderProgram* m_prog = nullptr;
    GLuint m_vao = 0, m_vbo = 0, m_ibo = 0;

    // 每个面的纹理对象与路径（墙面左右共用一张）
    GLuint m_tex[SurfaceCount] = {0, 0, 0};
    QString m_texPath[SurfaceCount];
    QString m_savedPath[SurfaceCount]; // G 键切换棋盘格前保存的原贴图
    bool m_texDirty[SurfaceCount] = {true, true, true};

    TextureUtils::Filter m_filter = TextureUtils::Filter::LinearMipmapLinear;
    bool m_mipTint = false;

    // 第一人称相机
    QVector3D m_pos;
    float m_yaw = 0.0f;   // 0 表示看向 -Z（走廊深处），单位：度
    float m_pitch = 0.0f;

    // 交互
    QSet<int> m_keys;     // 当前按住的键，用于平滑连续移动
    QTimer m_timer;
    QElapsedTimer m_clock;
    QPoint m_lastMouse;
    bool m_autoWalk = false;
    float m_autoDir = -1.0f;

    QLabel* m_hud = nullptr;
};
