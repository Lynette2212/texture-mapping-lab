#include "EnvMapPlugin.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include "EnvMapWidget.h"
#include "GLViewport.h"
#include "TextureUtils.h"
#include "ViewerContext.h"

QWidget* EnvMapPlugin::createUi()
{
    auto* box = new QGroupBox(QStringLiteral("环境映射"));
    auto* layout = new QVBoxLayout(box);
    layout->setSpacing(6);

    m_enableCheck = new QCheckBox(QStringLiteral("启用环境映射"));
    layout->addWidget(m_enableCheck);

    // 全景图列表：新增素材只需把 2:1 的 jpg/png 放进 data/env
    m_panoCombo = new QComboBox;
    m_panoPaths = TextureUtils::listImages(ViewerContext::dataPath(QStringLiteral("env")));
    for (const QString& p : m_panoPaths)
        m_panoCombo->addItem(QFileInfo(p).completeBaseName());
    layout->addWidget(m_panoCombo);

    m_skyCheck = new QCheckBox(QStringLiteral("显示天空盒背景"));
    m_skyCheck->setChecked(m_ctx->viewport->skyboxVisible());
    layout->addWidget(m_skyCheck);

    auto* reflRow = new QHBoxLayout;
    reflRow->addWidget(new QLabel(QStringLiteral("反射率")));
    m_reflSlider = new QSlider(Qt::Horizontal);
    m_reflSlider->setRange(0, 100);
    m_reflSlider->setValue(qRound(m_ctx->viewport->reflectivity() * 100));
    m_reflLabel = new QLabel;
    m_reflLabel->setMinimumWidth(36);
    reflRow->addWidget(m_reflSlider, 1);
    reflRow->addWidget(m_reflLabel);
    layout->addLayout(reflRow);

    auto* sphereBtn = new QPushButton(QStringLiteral("加载演示球体"));
    auto* demoBtn = new QPushButton(QStringLiteral("打开环境映射演示窗口（任务2）"));
    layout->addWidget(sphereBtn);
    layout->addWidget(demoBtn);

    const bool hasPano = !m_panoPaths.isEmpty();
    m_enableCheck->setEnabled(hasPano);
    if (!hasPano)
        m_enableCheck->setText(QStringLiteral("启用环境映射（data/env 中无全景图）"));

    // ---------- 信号连接 ----------
    connect(m_enableCheck, &QCheckBox::toggled, this, &EnvMapPlugin::applyEnvironment);
    connect(m_panoCombo, &QComboBox::currentIndexChanged, this, [this] {
        if (m_enableCheck->isChecked())
            applyEnvironment();
        if (m_demo && m_panoCombo->currentIndex() >= 0)
            m_demo->setPanorama(m_panoPaths.value(m_panoCombo->currentIndex()));
    });
    connect(m_skyCheck, &QCheckBox::toggled, this, [this](bool on) {
        m_ctx->viewport->setSkyboxVisible(on);
    });
    connect(m_reflSlider, &QSlider::valueChanged, this, [this](int v) {
        m_reflLabel->setText(QStringLiteral("%1%").arg(v));
        m_ctx->viewport->setReflectivity(v / 100.0f);
    });
    connect(sphereBtn, &QPushButton::clicked, this, &EnvMapPlugin::loadDemoSphere);
    connect(demoBtn, &QPushButton::clicked, this, &EnvMapPlugin::openEnvDemo);

    m_reflLabel->setText(QStringLiteral("%1%").arg(m_reflSlider->value()));
    return box;
}

void EnvMapPlugin::applyEnvironment()
{
    if (!m_enableCheck->isChecked()) {
        m_ctx->viewport->setEnvironmentMap(QImage());   // 空图 = 关闭
        m_ctx->appendInfo(QStringLiteral("[envmap] 已关闭环境映射"));
        return;
    }
    const QString path = m_panoPaths.value(m_panoCombo->currentIndex());
    QElapsedTimer t;
    t.start();
    QApplication::setOverrideCursor(Qt::WaitCursor);   // 4K 全景解码需要几百毫秒
    const QImage pano(path);
    QApplication::restoreOverrideCursor();
    if (pano.isNull()) {
        m_ctx->appendInfo(QStringLiteral("[envmap] 全景图读取失败：%1").arg(path));
        return;
    }
    m_ctx->viewport->setEnvironmentMap(pano);   // 下一帧在 GL 线程中转成立方体贴图
    m_ctx->appendInfo(QStringLiteral("[envmap] 环境贴图 %1（%2×%3，解码 %4 ms）")
                          .arg(QFileInfo(path).fileName())
                          .arg(pano.width()).arg(pano.height()).arg(t.elapsed()));
}

void EnvMapPlugin::loadDemoSphere()
{
    const QString path = ViewerContext::dataPath(QStringLiteral("obj/demo/smooth_sphere.obj"));
    if (!QFileInfo::exists(path) || !m_ctx->loadModel(path)) {
        m_ctx->appendInfo(QStringLiteral("[envmap] 找不到演示球体：%1").arg(path));
        return;
    }
    m_ctx->viewport->clearTexture();            // 纯反射更容易看清环境
    if (!m_enableCheck->isChecked() && m_enableCheck->isEnabled())
        m_enableCheck->setChecked(true);        // 触发 applyEnvironment
    m_reflSlider->setValue(90);
}

void EnvMapPlugin::openEnvDemo()
{
    if (!m_demo) {
        m_demo = new EnvMapWidget;
        m_demo->setAttribute(Qt::WA_DeleteOnClose);
        m_demo->setWindowTitle(QStringLiteral("任务2 · 天空盒与环境映射（由 environment_mapping 插件打开）"));
        m_demo->resize(1100, 700);
        if (m_panoCombo->currentIndex() >= 0)
            m_demo->setPanorama(m_panoPaths.value(m_panoCombo->currentIndex()));
    }
    m_demo->show();
    m_demo->raise();
    m_demo->activateWindow();
}

void EnvMapPlugin::onDestroy()
{
    if (m_demo)
        m_demo->close();
}
