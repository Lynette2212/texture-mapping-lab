#include "TexturePlugin.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QIcon>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "GLViewport.h"
#include "ViewerContext.h"

namespace {
constexpr int kColumnCount = 4;
}

TexturePlugin::TexturePlugin()
{
    // 单次触发防抖定时器：窗口缩放时延迟重排缩略图
    m_reloadTimer = new QTimer(this);
    m_reloadTimer->setSingleShot(true);
    m_reloadTimer->setInterval(150);
    connect(m_reloadTimer, &QTimer::timeout, this, &TexturePlugin::loadTextureImages);
}

TexturePlugin::~TexturePlugin() = default;

QWidget* TexturePlugin::createUi()
{
    auto* panel = new QWidget;
    panel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding); // 占满右侧剩余高度
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);

    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* container = new QWidget;
    m_grid = new QGridLayout(container);
    m_grid->setSpacing(6);
    m_grid->setContentsMargins(15, 15, 15, 15);
    for (int c = 0; c < kColumnCount; ++c)
        m_grid->setColumnStretch(c, 1);
    m_scroll->setWidget(container);
    layout->addWidget(m_scroll);

    m_textureDir = QCoreApplication::applicationDirPath() + QStringLiteral("/data/texture");

    // 等待窗口布局完成再加载图片
    QTimer::singleShot(200, this, &TexturePlugin::loadTextureImages);
    return panel;
}

void TexturePlugin::onWindowResize()
{
    m_reloadTimer->start();
}

void TexturePlugin::loadTextureImages()
{
    if (!m_grid)
        return;

    // 清空旧控件
    m_selectedBtn = nullptr;
    while (QLayoutItem* item = m_grid->takeAt(0)) {
        if (QWidget* w = item->widget())
            w->deleteLater();
        delete item;
    }

    int viewWidth = m_scroll->viewport()->width();
    if (viewWidth <= 20)
        viewWidth = 300;
    const QMargins margins = m_grid->contentsMargins();
    const int avail = viewWidth - margins.left() - margins.right()
                      - (kColumnCount - 1) * m_grid->spacing();
    const int thumbSize = std::max(64, avail / kColumnCount);

    const QDir dir(m_textureDir);
    const QStringList images = dir.entryList(
        {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg")},
        QDir::Files, QDir::Name);

    for (int slot = 0; slot < images.size(); ++slot) {
        const QString path = dir.absoluteFilePath(images[slot]);
        const QPixmap pix = QPixmap(path).scaled(thumbSize, thumbSize,
                                                 Qt::KeepAspectRatio, Qt::SmoothTransformation);
        auto* btn = new QPushButton;
        btn->setFixedSize(thumbSize, thumbSize);
        btn->setIcon(QIcon(pix));
        btn->setIconSize(QSize(thumbSize - 8, thumbSize - 8));
        connect(btn, &QPushButton::clicked, this, [this, btn, path] { onTextureClicked(btn, path); });
        m_grid->addWidget(btn, slot / kColumnCount, slot % kColumnCount);
    }
}

void TexturePlugin::onTextureClicked(QPushButton* btn, const QString& path)
{
    if (m_selectedBtn)
        m_selectedBtn->setStyleSheet(QString());
    btn->setStyleSheet(QStringLiteral("border:3px solid #ff4400;"));
    m_selectedBtn = btn;
    m_selectedPath = path;

    m_ctx->appendInfo(QStringLiteral("选中纹理：%1").arg(path));
    if (m_ctx->currentMesh) {
        m_ctx->viewport->setTextureImage(QImage(path));
        m_ctx->appendInfo(QStringLiteral("纹理已绑定模型：%1")
                              .arg(QFileInfo(m_ctx->currentModelPath).fileName()));
    }
}
