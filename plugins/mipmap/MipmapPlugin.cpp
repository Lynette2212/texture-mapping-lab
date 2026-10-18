#include "MipmapPlugin.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include "CorridorWidget.h"
#include "GLViewport.h"
#include "ViewerContext.h"

using TextureUtils::Filter;

QWidget* MipmapPlugin::createUi()
{
    auto* box = new QGroupBox(QStringLiteral("Mipmap 纹理过滤"));
    auto* layout = new QVBoxLayout(box);
    layout->setSpacing(6);

    // 1) 过滤方式：combo 的 userData 存 Filter 枚举值
    m_filterCombo = new QComboBox;
    for (Filter f : TextureUtils::allFilters())
        m_filterCombo->addItem(TextureUtils::filterName(f), static_cast<int>(f));
    m_filterCombo->setCurrentIndex(m_filterCombo->findData(static_cast<int>(m_ctx->viewport->textureFilter())));
    layout->addWidget(m_filterCombo);

    m_descLabel = new QLabel;
    m_descLabel->setWordWrap(true);
    m_descLabel->setStyleSheet(QStringLiteral("color:#555;"));
    layout->addWidget(m_descLabel);

    // 2) 平铺次数：纹理坐标放大倍数，越大远处纹素越密，越能看出 mipmap 作用
    auto* tilingRow = new QHBoxLayout;
    tilingRow->addWidget(new QLabel(QStringLiteral("平铺")));
    m_tilingSlider = new QSlider(Qt::Horizontal);
    m_tilingSlider->setRange(1, 32);
    m_tilingSlider->setValue(qRound(m_ctx->viewport->textureTiling()));
    m_tilingLabel = new QLabel;
    m_tilingLabel->setMinimumWidth(32);
    tilingRow->addWidget(m_tilingSlider, 1);
    tilingRow->addWidget(m_tilingLabel);
    layout->addLayout(tilingRow);

    // 3) 层级着色：每层 mipmap 染成不同颜色，直观看到 GPU 在哪里切换层级
    m_tintCheck = new QCheckBox(QStringLiteral("按 mipmap 层级着色（调试）"));
    layout->addWidget(m_tintCheck);

    // 4) 快捷操作
    auto* btnRow = new QHBoxLayout;
    auto* checkerBtn = new QPushButton(QStringLiteral("棋盘格纹理"));
    auto* groundBtn = new QPushButton(QStringLiteral("加载演示地面"));
    btnRow->addWidget(checkerBtn);
    btnRow->addWidget(groundBtn);
    layout->addLayout(btnRow);
    auto* corridorBtn = new QPushButton(QStringLiteral("打开走廊演示窗口（任务1）"));
    layout->addWidget(corridorBtn);

    // ---------- 信号连接 ----------
    connect(m_filterCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        applyFilter(static_cast<Filter>(m_filterCombo->itemData(idx).toInt()));
    });
    connect(m_tilingSlider, &QSlider::valueChanged, this, [this](int v) {
        m_tilingLabel->setText(QStringLiteral("×%1").arg(v));
        m_ctx->viewport->setTextureTiling(float(v));
    });
    connect(m_tintCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_ctx->viewport->setMipmapLevelTint(on);
        m_ctx->appendInfo(on ? QStringLiteral("[mipmap] 已开启层级着色：L0 原色 / L1 红 / L2 绿 / L3 蓝 ...")
                             : QStringLiteral("[mipmap] 已关闭层级着色"));
    });
    connect(checkerBtn, &QPushButton::clicked, this, [this] {
        m_ctx->viewport->setTextureImage(TextureUtils::makeCheckerboard());
        m_ctx->appendInfo(QStringLiteral("[mipmap] 已应用程序生成的 512×512 黑白棋盘格纹理"));
    });
    connect(groundBtn, &QPushButton::clicked, this, &MipmapPlugin::loadDemoGround);
    connect(corridorBtn, &QPushButton::clicked, this, &MipmapPlugin::openCorridor);

    m_tilingLabel->setText(QStringLiteral("×%1").arg(m_tilingSlider->value()));
    m_descLabel->setText(TextureUtils::filterDescription(m_ctx->viewport->textureFilter()));
    return box;
}

void MipmapPlugin::applyFilter(Filter f)
{
    m_ctx->viewport->setTextureFilter(f);
    m_descLabel->setText(TextureUtils::filterDescription(f));
    if (m_corridor && m_corridor->filter() != f)
        m_corridor->setFilter(f);
    m_ctx->appendInfo(QStringLiteral("[mipmap] 过滤方式 -> %1").arg(TextureUtils::filterName(f)));
}

void MipmapPlugin::onModelLoaded()
{
    // 视口的过滤/平铺状态跨模型保留；这里仅提示当前状态
    m_ctx->appendInfo(QStringLiteral("[mipmap] 当前过滤 %1，平铺 ×%2")
                          .arg(TextureUtils::filterName(m_ctx->viewport->textureFilter()))
                          .arg(m_ctx->viewport->textureTiling()));
}

void MipmapPlugin::loadDemoGround()
{
    const QString path = ViewerContext::dataPath(QStringLiteral("obj/demo/ground_plane.obj"));
    if (!QFileInfo::exists(path) || !m_ctx->loadModel(path)) {
        m_ctx->appendInfo(QStringLiteral("[mipmap] 找不到演示地面：%1").arg(path));
        return;
    }
    m_ctx->viewport->setTextureImage(TextureUtils::makeCheckerboard());
    m_tilingSlider->setValue(24);   // 触发 valueChanged -> setTextureTiling
    // 低机位掠射视角：近处纹素被放大、远处被大幅缩小，最能体现各过滤方式差异
    m_ctx->viewport->setCameraOrbit(0.0f, 7.0f, 25.0f, QVector3D(0.0f, 0.0f, 70.0f));
    m_ctx->appendInfo(QStringLiteral("[mipmap] 演示地面已加载：拖动视角放低到掠射角，再切换过滤方式对比远处效果"));
}

void MipmapPlugin::openCorridor()
{
    if (!m_corridor) {
        m_corridor = new CorridorWidget;   // 顶层窗口，关闭即销毁
        m_corridor->setAttribute(Qt::WA_DeleteOnClose);
        m_corridor->setWindowTitle(QStringLiteral("任务1 · Mipmap 走廊（由 mipmap_filter 插件打开）"));
        m_corridor->resize(1100, 700);
        m_corridor->setFilter(m_ctx->viewport->textureFilter());
        // 走廊里用右键菜单/数字键切换时，同步回面板（阻断信号防止回环）
        connect(m_corridor, &CorridorWidget::filterChanged, this, [this](Filter f) {
            const QSignalBlocker block(m_filterCombo);
            m_filterCombo->setCurrentIndex(m_filterCombo->findData(static_cast<int>(f)));
            m_ctx->viewport->setTextureFilter(f);
            m_descLabel->setText(TextureUtils::filterDescription(f));
        });
    }
    m_corridor->show();
    m_corridor->raise();
    m_corridor->activateWindow();
}

void MipmapPlugin::onDestroy()
{
    if (m_corridor)
        m_corridor->close();
}
