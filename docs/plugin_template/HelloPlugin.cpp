#include "HelloPlugin.h"

#include <QFileInfo>
#include <QGroupBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "GLViewport.h"
#include "ViewerContext.h"

QWidget* HelloPlugin::createUi()
{
    auto* box = new QGroupBox(pluginName());
    auto* layout = new QVBoxLayout(box);

    auto* btn = new QPushButton(QStringLiteral("线框 / 面片切换"));
    layout->addWidget(btn);
    connect(btn, &QPushButton::clicked, this, [this] {
        GLViewport* vp = m_ctx->viewport;
        vp->setRenderMode(vp->renderMode() == GLViewport::Surface ? GLViewport::Wireframe
                                                                  : GLViewport::Surface);
    });
    return box;   // 返回 nullptr 表示该插件没有面板
}

void HelloPlugin::onModelLoaded()
{
    m_ctx->appendInfo(QStringLiteral("[hello] 已加载 %1").arg(QFileInfo(m_ctx->currentModelPath).fileName()));
}
