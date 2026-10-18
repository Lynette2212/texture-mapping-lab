#include "MainWindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QSplitter>
#include <QTreeView>
#include <QVBoxLayout>

#include "GLViewport.h"
#include "Mesh.h"
#include "PluginInterface.h"

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("3D Model Viewer Framework"));
    resize(1600, 800);

    m_ctx.mainWindow = this;

    buildLayout();
    createMenus();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildLayout()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(4);

    auto* splitter = new QSplitter(Qt::Horizontal);
    mainLayout->addWidget(splitter, 8);

    auto* info = new QPlainTextEdit;
    info->setReadOnly(true);
    info->setMaximumHeight(160);
    mainLayout->addWidget(info, 1);
    m_ctx.infoDisplay = info;

    // 左侧文件树：仅显示 OBJ
    m_treeView = new QTreeView;
    m_fileModel = new QFileSystemModel(this);
    m_fileModel->setFilter(QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Files);
    m_fileModel->setNameFilters({QStringLiteral("*.obj")});
    m_fileModel->setNameFilterDisables(false);
    m_treeView->setModel(m_fileModel);
    for (int col = 1; col < m_fileModel->columnCount(); ++col)
        m_treeView->hideColumn(col);
    // 默认打开程序目录下的 data/obj（构建时自动拷贝的示例模型），不存在时显示整个文件系统
    const QString objDir = QCoreApplication::applicationDirPath() + QStringLiteral("/data/obj");
    m_treeView->setRootIndex(m_fileModel->setRootPath(QDir(objDir).exists() ? objDir : QString()));
    connect(m_treeView, &QTreeView::clicked, this, &MainWindow::onTreeClicked);
    splitter->addWidget(m_treeView);

    // 中间 OpenGL 视口
    auto* viewport = new GLViewport;
    m_ctx.viewport = viewport;
    splitter->addWidget(viewport);

    // 右侧：插件统一挂载容器
    auto* rightContainer = new QWidget;
    auto* rightLayout = new QVBoxLayout(rightContainer);
    rightLayout->setSpacing(8);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addStretch(1); // 面板自上而下堆叠
    m_ctx.rightPanelLayout = rightLayout;
    splitter->addWidget(rightContainer);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 6);
    splitter->setStretchFactor(2, 2);
    splitter->setSizes({480, 960, 320}); // 初始比例 3:6:2，防止空容器塌缩
}

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("File"));
    QAction* openFolder = fileMenu->addAction(QStringLiteral("Open Folder"));
    connect(openFolder, &QAction::triggered, this, &MainWindow::openFolder);
    QAction* exitAction = fileMenu->addAction(QStringLiteral("Exit"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::registerPlugin(PluginInterface* plugin)
{
    plugin->initialize(&m_ctx);
    m_ctx.plugins.append(plugin);
    if (QWidget* panel = plugin->createUi()) {
        // 面板声明纵向 Expanding（如缩略图列表）时分到剩余高度，否则按自身高度紧凑堆叠
        const bool expand = panel->sizePolicy().verticalPolicy() & QSizePolicy::ExpandFlag;
        m_ctx.rightPanelLayout->insertWidget(m_ctx.rightPanelLayout->count() - 1, panel, expand ? 100 : 0);
    }
}

void MainWindow::openFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, QStringLiteral("Open Folder"));
    if (folder.isEmpty())
        return;
    m_treeView->setRootIndex(m_fileModel->setRootPath(folder));
}

void MainWindow::onTreeClicked(const QModelIndex& index)
{
    const QString path = m_fileModel->filePath(index);
    if (QFileInfo(path).isFile() && path.endsWith(QLatin1String(".obj"), Qt::CaseInsensitive))
        loadModel(path);
}

bool MainWindow::loadModel(const QString& path)
{
    auto mesh = std::make_unique<Mesh>();
    QString error;
    if (!mesh->loadFromObj(path, &error)) {
        m_ctx.appendInfo(QStringLiteral("模型加载失败: %1\n%2").arg(path, error));
        return false;
    }

    m_mesh = std::move(mesh);
    m_ctx.currentMesh = m_mesh.get();
    m_ctx.currentModelPath = path;
    m_ctx.viewport->setMesh(m_ctx.currentMesh);

    m_ctx.infoDisplay->setPlainText(QStringLiteral("OBJ File: %1\nVertices: %2\nFaces: %3")
                                        .arg(path)
                                        .arg(m_mesh->vertexCount())
                                        .arg(m_mesh->triangleCount()));

    // 分发插件钩子：模型加载完成
    for (PluginInterface* plugin : m_ctx.plugins)
        plugin->onModelLoaded();
    return true;
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    for (PluginInterface* plugin : m_ctx.plugins)
        plugin->onWindowResize();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    for (PluginInterface* plugin : m_ctx.plugins)
        plugin->onDestroy();
    event->accept();
}
