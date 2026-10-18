#include "ToolbarPlugin.h"

#include <QAction>
#include <QActionGroup>
#include <QIcon>
#include <QMainWindow>
#include <QSize>
#include <QToolBar>

#include "GLViewport.h"
#include "ViewerContext.h"

QWidget* ToolbarPlugin::createUi()
{
    m_toolbar = new QToolBar(QStringLiteral("Model View Toolbar"), m_ctx->mainWindow);
    m_toolbar->setIconSize(QSize(32, 32));
    m_toolbar->setMovable(false);
    // 明确只显示文字：不同平台/Qt 版本对“图标缺失时”的默认显示不一致
    // （Windows 显示文字，Linux + Qt 6.2 会显示空白按钮）
    m_toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);

    // 第一组：线框/面片互斥按钮
    auto* modeGroup = new QActionGroup(m_toolbar);
    modeGroup->setExclusive(true);

    QAction* actSurface = m_toolbar->addAction(QIcon(QStringLiteral(":/icons/surface.png")),
                                               QStringLiteral("面片显示"));
    actSurface->setCheckable(true);
    actSurface->setChecked(true);
    QAction* actWire = m_toolbar->addAction(QIcon(QStringLiteral(":/icons/wire.png")),
                                            QStringLiteral("线框显示"));
    actWire->setCheckable(true);
    modeGroup->addAction(actSurface);
    modeGroup->addAction(actWire);
    connect(actSurface, &QAction::triggered, this, [this] { setDisplayMode(0); });
    connect(actWire, &QAction::triggered, this, [this] { setDisplayMode(1); });

    m_toolbar->addSeparator();

    // 第二组：退出按钮
    QAction* actExit = m_toolbar->addAction(QIcon(QStringLiteral(":/icons/exit.png")),
                                            QStringLiteral("退出程序"));
    connect(actExit, &QAction::triggered, m_ctx->mainWindow, &QWidget::close);

    m_ctx->mainWindow->addToolBar(Qt::TopToolBarArea, m_toolbar);
    return nullptr; // 工具栏插件无右侧面板
}

void ToolbarPlugin::setDisplayMode(int mode)
{
    m_displayMode = mode;
    if (!m_ctx->currentMesh) {
        m_ctx->appendInfo(QStringLiteral("提示：未加载任何模型，无法切换显示模式"));
        return;
    }
    m_ctx->viewport->setRenderMode(mode == 0 ? GLViewport::Surface : GLViewport::Wireframe);
    m_ctx->appendInfo(mode == 0 ? QStringLiteral("切换模型显示：面片模式")
                                : QStringLiteral("切换模型显示：线框模式"));
}

void ToolbarPlugin::onDestroy()
{
    delete m_toolbar;
    m_toolbar = nullptr;
}
