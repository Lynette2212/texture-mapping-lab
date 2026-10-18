#pragma once

#include <memory>

#include <QMainWindow>

#include "ViewerContext.h"
#include "viewer_core_global.h"

class QTreeView;
class QFileSystemModel;
class QModelIndex;
class PluginInterface;
class Mesh;

// 框架内核主窗口（对应 Python 版 VTKViewerCore）。
// 只保留基础能力：文件树、OBJ 加载、OpenGL 视口、底部信息栏，
// 所有扩展功能经 registerPlugin 挂载，事件统一分发到插件钩子。
class VIEWER_CORE_EXPORT MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    ViewerContext* context() { return &m_ctx; }

    // 插件注册入口：initialize -> createUi -> 挂载右侧面板
    void registerPlugin(PluginInterface* plugin);

    // 加载 OBJ 模型（也可由命令行参数调用）
    bool loadModel(const QString& path);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private:
    void buildLayout();
    void createMenus();
    void openFolder();
    void onTreeClicked(const QModelIndex& index);

    ViewerContext m_ctx;
    QTreeView* m_treeView = nullptr;
    QFileSystemModel* m_fileModel = nullptr;
    std::unique_ptr<Mesh> m_mesh;
};
