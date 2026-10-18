// 最小插件模板：复制整个目录到 plugins/<你的插件名>/ 后改名即可
#pragma once

#include <QObject>

#include "PluginInterface.h"

class HelloPlugin : public QObject, public PluginInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginInterface_iid)   // 让 QPluginLoader 识别
    Q_INTERFACES(PluginInterface)                // 让 qobject_cast<PluginInterface*> 成功

public:
    // pluginId 必须唯一，并写进 plugin_config.json 的 active_plugins 才会被加载
    QString pluginId() const override { return QStringLiteral("hello"); }
    QString pluginName() const override { return QStringLiteral("Hello 示例"); }
    QString pluginDesc() const override { return QStringLiteral("最小插件示例"); }

    void initialize(ViewerContext* ctx) override { m_ctx = ctx; }
    QWidget* createUi() override;
    void onModelLoaded() override;

private:
    ViewerContext* m_ctx = nullptr;
};
