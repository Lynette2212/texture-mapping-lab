#pragma once

#include <QObject>

#include "PluginInterface.h"

class QCheckBox;

// 光照材质调节插件（对应 Python 版 plugin_light）
class LightPlugin : public QObject, public PluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginInterface_iid)
    Q_INTERFACES(PluginInterface)

public:
    QString pluginId() const override { return QStringLiteral("light_control"); }
    QString pluginName() const override { return QStringLiteral("light_control"); }
    QString pluginDesc() const override { return QStringLiteral("光照材质调节插件"); }

    void initialize(ViewerContext* ctx) override { m_ctx = ctx; }
    QWidget* createUi() override;
    void onModelLoaded() override { updateMaterial(); }

private:
    void updateMaterial();
    void setSubEnabled(bool enabled);

    ViewerContext* m_ctx = nullptr;
    QCheckBox* m_chkGlobal = nullptr;
    QCheckBox* m_chkAmbient = nullptr;
    QCheckBox* m_chkDiffuse = nullptr;
    QCheckBox* m_chkSpecular = nullptr;
};
