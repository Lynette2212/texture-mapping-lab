#pragma once

#include <QElapsedTimer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLWidget>
#include <QString>
#include <QTimer>

class QLabel;
class QOpenGLShaderProgram;

// 任务2：环境映射演示 —— 天空盒 + 反射球（可选反射立方体）
// 全景图（polyhaven 等距柱状投影）转换为立方体贴图，既画成背景天空盒，又作为物体表面的反射来源。
// 右键菜单：显示/隐藏立方体、材质、全景图；鼠标滚轮/右键拖动调整物体远近，左键拖动环绕观察。
class EnvMapWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
    Q_OBJECT

public:
    enum Material { Reflect = 0, Refract = 1, Fresnel = 2, MaterialCount = 3 };

    explicit EnvMapWidget(QWidget* parent = nullptr);
    ~EnvMapWidget() override;

    void setPanorama(const QString& path);
    QString panorama() const { return m_panoPath; }

    void setCubeVisible(bool on);
    bool cubeVisible() const { return m_showCube; }
    void setCubeSpinning(bool on);
    void setMaterial(Material m);
    Material material() const { return m_material; }
    void setDistance(float d);    // 物体与观察者的距离
    void resetView();

    static QString materialName(Material m);
    static QString envDir();      // data/env

protected:
    void initializeGL() override;
    void paintGL() override;

    void keyPressEvent(QKeyEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void wheelEvent(QWheelEvent* e) override;
    void contextMenuEvent(QContextMenuEvent* e) override;

private:
    struct GpuMesh { GLuint vao = 0, vbo = 0, ibo = 0; GLsizei count = 0; };
    template <typename MeshT> GpuMesh upload(const MeshT& m);
    void destroy(GpuMesh& g);
    void drawObject(const GpuMesh& g, const QMatrix4x4& model, const QMatrix4x4& view,
                    const QMatrix4x4& proj, const QVector3D& eye, const QVector3D& tint);
    void updateHud();
    QVector3D eyePosition() const;

    QOpenGLShaderProgram* m_objProg = nullptr;
    QOpenGLShaderProgram* m_skyProg = nullptr;
    GpuMesh m_sphere, m_cube, m_skyCube;

    GLuint m_envTex = 0;
    QString m_panoPath;
    bool m_envDirty = true;

    bool m_showCube = true;
    bool m_spinCube = true;
    float m_cubeAngle = 0.0f;
    Material m_material = Reflect;

    // 环绕相机：绕物体中心旋转，m_distance 即物体离观察者的远近
    float m_yaw = 20.0f;
    float m_pitch = 10.0f;
    float m_distance = 6.0f;

    QPoint m_lastMouse;
    bool m_rightDragged = false;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QLabel* m_hud = nullptr;
};
