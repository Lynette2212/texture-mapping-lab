#include "LightPlugin.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include "GLViewport.h"
#include "ViewerContext.h"

QWidget* LightPlugin::createUi()
{
    auto* panel = new QFrame;
    panel->setFrameShape(QFrame::StyledPanel);
    auto* layout = new QVBoxLayout(panel);
    layout->setSpacing(6);
    layout->setContentsMargins(10, 10, 10, 10);

    m_chkGlobal = new QCheckBox(QStringLiteral("启用光照"));
    m_chkGlobal->setChecked(true);
    layout->addWidget(m_chkGlobal);

    auto* subLayout = new QHBoxLayout;
    m_chkAmbient = new QCheckBox(QStringLiteral("环境光"));
    m_chkAmbient->setChecked(true);
    m_chkDiffuse = new QCheckBox(QStringLiteral("漫反射"));
    m_chkDiffuse->setChecked(true);
    m_chkSpecular = new QCheckBox(QStringLiteral("镜面高光"));
    m_chkSpecular->setChecked(true);
    subLayout->addWidget(m_chkAmbient);
    subLayout->addWidget(m_chkDiffuse);
    subLayout->addWidget(m_chkSpecular);
    layout->addLayout(subLayout);

    connect(m_chkGlobal, &QCheckBox::toggled, this, [this](bool on) {
        setSubEnabled(on);
        updateMaterial();
    });
    connect(m_chkAmbient, &QCheckBox::toggled, this, [this] { updateMaterial(); });
    connect(m_chkDiffuse, &QCheckBox::toggled, this, [this] { updateMaterial(); });
    connect(m_chkSpecular, &QCheckBox::toggled, this, [this] { updateMaterial(); });

    updateMaterial();
    return panel;
}

void LightPlugin::setSubEnabled(bool enabled)
{
    m_chkAmbient->setEnabled(enabled);
    m_chkDiffuse->setEnabled(enabled);
    m_chkSpecular->setEnabled(enabled);
}

void LightPlugin::updateMaterial()
{
    if (!m_ctx->currentMesh)
        return;
    if (!m_chkGlobal->isChecked()) {
        m_ctx->viewport->setLightingEnabled(false);
        return;
    }
    m_ctx->viewport->setLightingEnabled(true);
    m_ctx->viewport->setLightingCoeffs(m_chkAmbient->isChecked() ? 0.2f : 0.0f,
                                       m_chkDiffuse->isChecked() ? 0.8f : 0.0f,
                                       m_chkSpecular->isChecked() ? 0.4f : 0.0f);
}
