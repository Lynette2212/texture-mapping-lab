#pragma once

#include <QList>
#include <QString>

#include "viewer_core_global.h"

class QMainWindow;
class QPlainTextEdit;
class QVBoxLayout;
class GLViewport;
class Mesh;
class PluginInterface;

// 全局上下文：插件与内核共享状态（对应 Python 版 ViewerContext）
struct VIEWER_CORE_EXPORT ViewerContext
{
    QMainWindow*     mainWindow = nullptr;
    GLViewport*      viewport = nullptr;        // OpenGL 渲染后端
    QPlainTextEdit*  infoDisplay = nullptr;
    QVBoxLayout*     rightPanelLayout = nullptr; // 插件面板挂载容器

    QString currentModelPath;
    Mesh*   currentMesh = nullptr;               // 由主窗口持有，插件只读

    QList<PluginInterface*> plugins;             // 已加载插件实例

    void appendInfo(const QString& text);        // 向底部信息栏追加一行
    bool loadModel(const QString& path);         // 请求内核加载 OBJ（会触发所有插件的 onModelLoaded）
    static QString dataPath(const QString& relative); // <exe目录>/data/<relative>
};
